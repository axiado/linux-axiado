// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2026 Axiado Corporation (or its affiliates). All rights reserved.
 *
 * Axiado Host FIFO Platform Driver
 */

#include <linux/etherdevice.h>
#include <linux/netdevice.h>

#include "hfifo.h"
#include "shim_eip_common.h"
#include "shim_platform.h"

#define NETDEV_NAME "eth0"
#define HFIFO_NAPI_WEIGHT 64

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

	hfifo_reset_rx(hdata->mac_idx);
	napi_enable(&hdata->napi_hfifo_rx);
	hfifo_irq_enable(hdata->mac_idx);

	netif_start_queue(ndev);
	netif_carrier_on(ndev);
	netdev_info(ndev, "Link is Up - 1Gbps/Full - flow control off\n");

	return 0;
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
	netdev_info(ndev, "Link is Down\n");

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
	int pkt_len;

	while (work_done < budget) {
		pkt_len = hfifo_rx_pkt_len(hdata->mac_idx);
		if (pkt_len > 0) {
			skb = napi_alloc_skb(napi, pkt_len + NET_IP_ALIGN);
			if (!skb) {
				net_stats->rx_dropped++;
				break;
			}

			skb_reserve(skb, NET_IP_ALIGN);
			hfifo_packet_rx(skb_put(skb, pkt_len), pkt_len,
					hdata->mac_idx);
			skb->protocol = eth_type_trans(skb, ndev);
			skb->ip_summed = CHECKSUM_NONE;
			napi_gro_receive(napi, skb);

			net_stats->rx_packets++;
			net_stats->rx_bytes += pkt_len;
			work_done++;
		} else {
			if (pkt_len < 0) {
				hfifo_reset_rx(hdata->mac_idx);
				net_stats->rx_errors++;
			}
			break;
		}
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

	if (!hcp->hfifo_mode)
		return;

	unregister_netdev(hdata->ndev);
	netif_napi_del(&hdata->napi_hfifo_rx);
	free_netdev(hdata->ndev);
}
