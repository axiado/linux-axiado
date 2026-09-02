// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2026 Axiado Corporation (or its affiliates). All rights reserved.
 *
 * Axiado Host FIFO Platform Driver
 */

#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/if_vlan.h>
#include <linux/netdevice.h>
#include <linux/phy.h>

#include "hfifo.h"
#include "shim_common.h"
#include "shim_platform.h"

#define NETDEV_NAME "eth0"
#define HFIFO_NAPI_WEIGHT 64

/**
 * @brief hfifo_adjust_link - phylib link-state callback
 *
 * @param ndev - network interface device structure
 *
 * Registered with phy_connect(). phylib calls this on every link-state
 * change, driven by the MAC/PHY link interrupt
 */
static void hfifo_adjust_link(struct net_device *ndev)
{
	struct hfifo_data *hdata = netdev_priv(ndev);
	struct phy_device *phydev = ndev->phydev;

	if (!phydev) {
		netdev_warn(ndev, "No phydev attached\n");
		return;
	}

	/* Only act on actual link-state transitions. */
	if (phydev->link == hdata->link)
		return;

	hdata->link = phydev->link;

	if (phydev->link) {
		netif_start_queue(ndev);
	} else {
		netif_stop_queue(ndev);
		/* Drop any stale data buffered in the SHIM FIFO. */
		shim_fifo_reset(hdata->mac_idx);
	}

	phy_print_status(phydev);
}

/**
 * @brief hfifo_open - Called when a network interface is made active
 *
 * @param ndev - network interface device structure
 * @return int - 0 on success and negative value on failure
 *
 */
static int hfifo_open(struct net_device *ndev)
{
	struct hfifo_data *hdata = netdev_priv(ndev);
	struct mac_phy *mac_cfg = hcp_get_mac_cfg(hdata->hcp);
	struct phy_device *phydev = mac_cfg[hdata->mac_idx].phydev;
	int err;

	hfifo_reset_rx(hdata->mac_idx);
	napi_enable(&hdata->napi_hfifo_rx);
	/* rx and link irq enable */
	hfifo_irq_enable(hdata->mac_idx);
	netif_start_queue(ndev);

	if (phydev) {
		hdata->link = false;
		netif_carrier_off(ndev);
		if (ndev->phydev && ndev->phydev->attached_dev) {
			phydev = ndev->phydev;
		} else {
			/* Connect to the external PHY; the carrier and TX queue are
			 * driven by hfifo_adjust_link() from here on.
			 */
			phydev = phy_connect(ndev, phydev_name(phydev),
					     hfifo_adjust_link,
					     mac_cfg[hdata->mac_idx].phy_mode);
			if (IS_ERR(phydev)) {
				err = PTR_ERR(phydev);
				netdev_err(ndev, "Could not attach to PHY: %d\n", err);
				goto err_disable;
			}
		}

		phy_start(phydev);
	} else {
		/* No external PHY (e.g. DC-SCI port): assume a fixed
		 * 1Gbps/Full link and force the carrier up.
		 */
		hdata->link = true;
		netif_carrier_on(ndev);
		netdev_info(ndev, "Link is Up - 1Gbps/Full - flow control off\n");
	}

	return 0;

err_disable:
	netif_stop_queue(ndev);
	hfifo_irq_disable(hdata->mac_idx);
	napi_disable(&hdata->napi_hfifo_rx);
	return err;
}

/**
 * @brief hfifo_close Disables the network interface
 *
 * @param ndev - network interface device structure
 * @return int - always returns 0
 *
 */
static int hfifo_close(struct net_device *ndev)
{
	struct hfifo_data *hdata = netdev_priv(ndev);

	netif_stop_queue(ndev);
	netif_carrier_off(ndev);
	hfifo_irq_disable(hdata->mac_idx);
	napi_disable(&hdata->napi_hfifo_rx);
	if (ndev->phydev) {
		phy_stop(ndev->phydev);
		phy_disconnect(ndev->phydev);
	} else {
		hdata->link = false;
		netdev_info(ndev, "Link is Down\n");
	}

	return 0;
}

/**
 * @brief hfifo_xmit_frame - This will be called by stack for sending packet
 *
 * @param skb - socket buffer structure
 * @param ndev - network interface device structure
 * @return int - 0 on success
 */
static int hfifo_xmit_frame(struct sk_buff *skb, struct net_device *ndev)
{
	struct hfifo_data *hdata = netdev_priv(ndev);
	struct rtnl_link_stats64 *net_stats = &hdata->stats64;

	if (hfifo_packet_tx(skb->data, skb->len)) {
		net_stats->tx_errors++;
	} else {
		net_stats->tx_packets++;
		net_stats->tx_bytes += skb->len;
	}

	dev_kfree_skb(skb);
	return NETDEV_TX_OK;
}

/**
 * @brief hfifo_set_rx_mode Set RX mode on netdev
 * @param ndev - network interface device structure
 */
static void hfifo_set_rx_mode(struct net_device *ndev)
{
	struct hfifo_data *hdata = netdev_priv(ndev);

	mac_update_promisc(hdata->mac_idx, !!(ndev->flags & IFF_PROMISC));
}

/**
 * @brief hfifo_set_mac - Change the Ethernet address of the NIC
 *
 * @param ndev -  network interface device structure
 * @param address - mac address pointer
 * @return int - 0 on success, negative on failure
 */
static int hfifo_set_mac(struct net_device *ndev, void *address)
{
	struct sockaddr *addr = address;
	struct hfifo_data *hdata;
	u32 mac_0, mac_1;
	int err;

	err = eth_prepare_mac_addr_change(ndev, addr);
	if (err < 0)
		return err;

	mac_0 = (u32)addr->sa_data[0] | (u32)addr->sa_data[1] << 8 |
		(u32)addr->sa_data[2] << 16 | (u32)addr->sa_data[3] << 24;
	mac_1 = (u32)addr->sa_data[4] | (u32)addr->sa_data[5] << 8;

	hdata = netdev_priv(ndev);
	mac_addr_mac_idx_wr(hdata->mac_idx, mac_0, mac_1);
	eth_commit_mac_addr_change(ndev, addr);

	return 0;
}

/**
 * @brief hfifo_set_mac_with_idx - Set MAC addr on port
 * @param ndev -  network interface device structure
 * Param mac_idx - Port MAC index for setting mac
 */
static void hfifo_set_mac_with_idx(struct net_device *ndev, u8 mac_idx)
{
	const u8 ax_mac_oui[3] = { 0x00, 0xFB, 0xF9 };
	u8 mac_addr[ETH_ALEN] = {};
	u32 mac_0 = 0, mac_1 = 0;

	mac_addr_mac_idx_rd(mac_idx, &mac_0, &mac_1);
	if (!mac_0 || !mac_1) {
		netdev_info(ndev, "Programming random MAC\n");
		memcpy(mac_addr, ax_mac_oui, 3);
		get_random_bytes(mac_addr + 3, 3);
		memcpy(&mac_0, mac_addr, 4);
		memcpy(&mac_1, mac_addr + 4, 2);
		mac_addr_mac_idx_wr(mac_idx, mac_0, mac_1);
	} else {
		memcpy(mac_addr, &mac_0, 4);
		memcpy(mac_addr + 4, &mac_1, 2);
	}
	eth_hw_addr_set(ndev, mac_addr);
	netdev_info(ndev, "MAC: %02X:%02X:%02X:%02X:%02X:%02X\n", mac_addr[0],
		    mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4],
		    mac_addr[5]);
}

/**
 * @brief hfifo_get_stats64 - Get System Network Statistics
 *
 * @param ndev -  network interface device structure
 * @stats: rtnl_link_stats64 pointer
 */
static void hfifo_get_stats64(struct net_device *ndev,
			      struct rtnl_link_stats64 *stats)
{
	struct hfifo_data *hdata = netdev_priv(ndev);

	memcpy(stats, &hdata->stats64, sizeof(*stats));
}

static const struct net_device_ops hfifo_ndo = {
	.ndo_open = hfifo_open,
	.ndo_stop = hfifo_close,
	.ndo_start_xmit = hfifo_xmit_frame,
	.ndo_set_rx_mode = hfifo_set_rx_mode,
	.ndo_validate_addr = eth_validate_addr,
	.ndo_set_mac_address = hfifo_set_mac,
	.ndo_get_stats64 = hfifo_get_stats64,
};

/* Per-interface statistics exposed via `ethtool -S`. The order MUST match the
 * fill order in hfifo_get_ethtool_stats(): 8 netdev counters, the MAC RX and
 * MAC TX hardware counters.
 */
static const char hfifo_gstrings_stats[][ETH_GSTRING_LEN] = {
	"rx_packets", "tx_packets", "rx_bytes", "tx_bytes",
	"rx_errors", "tx_errors", "rx_dropped", "tx_dropped",
	/* shim_read_mac_rx_stats() */
	"mac_rx_good", "mac_rx_drop", "mac_rx_undersize_err", "mac_rx_total",
	"mac_rx_crc_err", "mac_rx_if_in_err", "mac_rx_oversize_err",
	"mac_rx_jabber_err", "mac_rx_frag_err",
	/* shim_read_mac_tx_stats() */
	"mac_tx_total", "mac_tx_good", "mac_tx_drop", "mac_tx_crc_err",
	"mac_tx_if_out_err",
};

#define HFIFO_STATS_COUNT ARRAY_SIZE(hfifo_gstrings_stats)

/**
 * @brief hfifo_get_drvinfo - report driver identification
 */
static void hfifo_get_drvinfo(struct net_device *ndev,
			      struct ethtool_drvinfo *info)
{
	strscpy(info->driver, "axiado-hfifo", sizeof(info->driver));
	strscpy(info->version, "1.0", sizeof(info->version));
	strscpy(info->fw_version, "N/A", sizeof(info->fw_version));
	if (ndev->dev.parent)
		strscpy(info->bus_info, dev_name(ndev->dev.parent),
			sizeof(info->bus_info));
	else
		strscpy(info->bus_info, "unknown", sizeof(info->bus_info));
}

/**
 * @brief hfifo_get_link_ksettings - report link settings from the PHY
 *
 * When no external PHY is present (forced-carrier fallback) report the
 * assumed fixed 1Gbps/Full link.
 */
static int hfifo_get_link_ksettings(struct net_device *ndev,
				    struct ethtool_link_ksettings *cmd)
{
	struct hfifo_data *hdata = netdev_priv(ndev);

	if (ndev->phydev)
		return phy_ethtool_get_link_ksettings(ndev, cmd);

	cmd->base.speed = SPEED_1000;
	cmd->base.duplex = DUPLEX_FULL;
	cmd->base.autoneg = AUTONEG_DISABLE;
	cmd->base.port = PORT_MII;
	cmd->base.phy_address = hdata->mac_idx;

	ethtool_link_ksettings_zero_link_mode(cmd, supported);
	ethtool_link_ksettings_add_link_mode(cmd, supported, MII);
	ethtool_link_ksettings_add_link_mode(cmd, supported, 1000baseT_Full);

	return 0;
}

/**
 * @brief hfifo_set_link_ksettings - apply link settings to the PHY
 */
static int hfifo_set_link_ksettings(struct net_device *ndev,
				    const struct ethtool_link_ksettings *cmd)
{
	if (!ndev->phydev)
		return -EOPNOTSUPP;

	return phy_ethtool_set_link_ksettings(ndev, cmd);
}

/**
 * @brief hfifo_get_pauseparam - report PHY pause-frame settings
 */
static void hfifo_get_pauseparam(struct net_device *ndev,
				 struct ethtool_pauseparam *pause)
{
	bool tx_pause = false, rx_pause = false;

	if (!ndev->phydev)
		return;

	phy_get_pause(ndev->phydev, &tx_pause, &rx_pause);
	pause->autoneg = ndev->phydev->autoneg;
	pause->tx_pause = tx_pause;
	pause->rx_pause = rx_pause;
}

/**
 * @brief hfifo_set_pauseparam - configure PHY pause-frame settings
 */
static int hfifo_set_pauseparam(struct net_device *ndev,
				struct ethtool_pauseparam *pause)
{
	struct phy_device *phydev = ndev->phydev;

	if (!phydev)
		return -ENODEV;

	if (!phy_validate_pause(phydev, pause))
		return -EINVAL;

	phy_set_sym_pause(phydev, pause->rx_pause, pause->tx_pause,
			  pause->autoneg);

	return 0;
}

/**
 * @brief hfifo_get_wol - report Wake-on-LAN capabilities from the PHY
 */
static void hfifo_get_wol(struct net_device *ndev, struct ethtool_wolinfo *wol)
{
	wol->supported = 0;
	wol->wolopts = 0;

	if (ndev->phydev)
		phy_ethtool_get_wol(ndev->phydev, wol);
}

/**
 * @brief hfifo_set_wol - configure Wake-on-LAN on the PHY
 */
static int hfifo_set_wol(struct net_device *ndev, struct ethtool_wolinfo *wol)
{
	if (!ndev->phydev)
		return -EOPNOTSUPP;

	return phy_ethtool_set_wol(ndev->phydev, wol);
}

/**
 * @brief hfifo_get_strings - report statistics names
 */
static void hfifo_get_strings(struct net_device *ndev, u32 sset, u8 *data)
{
	if (sset != ETH_SS_STATS)
		return;

	memcpy(data, hfifo_gstrings_stats, sizeof(hfifo_gstrings_stats));
}

/**
 * @brief hfifo_get_sset_count - report number of statistics
 */
static int hfifo_get_sset_count(struct net_device *ndev, int sset)
{
	if (sset != ETH_SS_STATS)
		return -EOPNOTSUPP;

	return HFIFO_STATS_COUNT;
}

/**
 * @brief hfifo_get_ethtool_stats - collect netdev and SHIM/MAC HW counters
 */
static void hfifo_get_ethtool_stats(struct net_device *ndev,
				    struct ethtool_stats *stats, u64 *data)
{
	struct hfifo_data *hdata = netdev_priv(ndev);
	struct rtnl_link_stats64 *s = &hdata->stats64;
	u32 mac_rx[9] = { 0 };
	u32 mac_tx[5] = { 0 };
	int i, j = 0;

	data[j++] = s->rx_packets;
	data[j++] = s->tx_packets;
	data[j++] = s->rx_bytes;
	data[j++] = s->tx_bytes;
	data[j++] = s->rx_errors;
	data[j++] = s->tx_errors;
	data[j++] = s->rx_dropped;
	data[j++] = s->tx_dropped;

	shim_read_mac_rx_stats(mac_rx, hdata->mac_idx);
	for (i = 0; i < 9; i++)
		data[j++] = mac_rx[i];

	shim_read_mac_tx_stats(mac_tx, hdata->mac_idx);
	for (i = 0; i < 5; i++)
		data[j++] = mac_tx[i];
}

/* phylib-backed ethtool operations (link settings come from the PHY) */
static const struct ethtool_ops hfifo_ethtool_ops = {
	.get_drvinfo = hfifo_get_drvinfo,
	.get_link = ethtool_op_get_link,
	.nway_reset = phy_ethtool_nway_reset,
	.get_link_ksettings = hfifo_get_link_ksettings,
	.set_link_ksettings = hfifo_set_link_ksettings,
	.get_pauseparam = hfifo_get_pauseparam,
	.set_pauseparam = hfifo_set_pauseparam,
	.get_wol = hfifo_get_wol,
	.set_wol = hfifo_set_wol,
	.get_strings = hfifo_get_strings,
	.get_sset_count = hfifo_get_sset_count,
	.get_ethtool_stats = hfifo_get_ethtool_stats,
};

/**
 * @brief hfifo_rx_poll - NAPI poll function
 *
 * @param napi -  napi structure
 * @param budget - max packets to pull
 */
static int hfifo_rx_poll(struct napi_struct *napi, int budget)
{
	struct hfifo_data *hdata =
		container_of(napi, struct hfifo_data, napi_hfifo_rx);
	struct rtnl_link_stats64 *net_stats = &hdata->stats64;
	struct net_device *ndev = hdata->ndev;
	struct sk_buff *skb;
	int work_done = 0;
	u32 frmlen, pkt_len, buf_len, frmstat, max_frame, mod, fifo_rst;

	max_frame = ndev->mtu + VLAN_ETH_HLEN + ETH_FCS_LEN;
	while (work_done < budget) {
		frmlen = hfifo_rx_pkt_frmlen(hdata->mac_idx, &mod, &fifo_rst);
		if (frmlen) {
			buf_len = frmlen * 4;
			pkt_len = buf_len - ((4 - mod) % 4);
			if (pkt_len < ETH_ZLEN || pkt_len > max_frame) {
				fifo_rst = 1;
			} else {
				skb = napi_alloc_skb(napi, buf_len + NET_IP_ALIGN);
				if (!skb) {
					net_stats->rx_dropped++;
					break;
				}
				skb_reserve(skb, NET_IP_ALIGN);
				frmstat = hfifo_packet_rx(skb_put(skb, buf_len),
						frmlen, hdata->mac_idx);
				skb_trim(skb, pkt_len);
				skb->protocol = eth_type_trans(skb, ndev);
				skb->ip_summed = CHECKSUM_NONE;
				napi_gro_receive(napi, skb);
				net_stats->rx_packets++;
				net_stats->rx_bytes += pkt_len;
				work_done++;
			}
		}

		if (fifo_rst) {
			hfifo_reset_rx(hdata->mac_idx);
			net_stats->rx_errors++;
			break;
		}

		if (!frmlen)
			break;
	}

	if (work_done < budget && napi_complete_done(napi, work_done))
		hfifo_irq_enable(hdata->mac_idx);

	return work_done;
}

/**
 * hfifo_setup - setup Ethernet network device
 * @ndev: network device
 *
 * Fill in the fields of the device structure with Ethernet-generic values.
 */
static void hfifo_setup(struct net_device *ndev)
{
	ether_setup(ndev);
	ndev->netdev_ops = &hfifo_ndo;
	ndev->ethtool_ops = &hfifo_ethtool_ops;

	/* HW MAC address write is atomic; no need to bring the link down to
	 * change it.
	 */
	ndev->priv_flags |= IFF_LIVE_ADDR_CHANGE;

	/* No L3/L4 offloads are advertised */
	ndev->features = 0;
	ndev->hw_features = 0;
}

/**
 * @brief hfifo_subsystem_init - Initializes Host FIFO subsystem
 *
 * hcp: HCP device structure
 * Return: 0 on success, negative error code on failure
 */
int hfifo_subsystem_init(struct hcp_device *hcp)
{
	struct hfifo_priv *hpriv = hcp->hfifo_priv;
	struct net_device *ndev;
	struct hfifo_data *hdata;
	int err;

	ndev = alloc_netdev(sizeof(struct hfifo_data), NETDEV_NAME,
			    NET_NAME_UNKNOWN, hfifo_setup);
	if (!ndev)
		return -ENOMEM;

	SET_NETDEV_DEV(ndev, hcp->dev);
	hdata = netdev_priv(ndev);
	hdata->hcp = hcp;
	hdata->mac_idx = hpriv->mac_idx;
	hdata->ndev = ndev;
	hpriv->data = hdata;

	hdata->rx_irq = hcp->mac_irqs[hpriv->mac_idx];
	if (hdata->rx_irq <= 0) {
		dev_err(hcp->dev, "Invalid RX IRQ for MAC-%u\n",
			hdata->mac_idx);
		err = -EINVAL;
		goto err_free_netdev;
	}

	hfifo_set_mac_with_idx(ndev, hdata->mac_idx);
	netif_carrier_off(ndev);

	netif_napi_add_weight(ndev, &hdata->napi_hfifo_rx, hfifo_rx_poll,
			      HFIFO_NAPI_WEIGHT);

	err = register_netdev(ndev);
	if (err) {
		dev_err(hcp->dev, "Failed to register netdev: %d\n", err);
		goto err_napi_del;
	}

	dev_info(hcp->dev, "HFIFO subsystem initialized\n");
	return 0;

err_napi_del:
	netif_napi_del(&hdata->napi_hfifo_rx);
err_free_netdev:
	free_netdev(ndev);
	dev_err(hcp->dev, "HFIFO subsystem initialization failed: %d\n", err);
	return err;
}

/**
 * brief hfifo_subsystem_exit - cleanup
 *
 * @hcp: HCP device structure
 */
void hfifo_subsystem_exit(struct hcp_device *hcp)
{
	struct hfifo_priv *hpriv = hcp->hfifo_priv;
	struct hfifo_data *hdata = hpriv->data;

	unregister_netdev(hdata->ndev);
	netif_napi_del(&hdata->napi_hfifo_rx);
	free_netdev(hdata->ndev);
}
