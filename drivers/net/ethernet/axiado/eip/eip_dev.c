// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * eip_dev.c - Netdev and eip interfacing.
 *
 * Copyright (C) 2022-2026 Axiado Corporation.
 */

#include <linux/etherdevice.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/ip.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/phy.h>
#include <linux/platform_device.h>
#include <linux/tcp.h>
#include <linux/types.h>
#include <linux/udp.h>

#include "adapter.h"
#include "api_driver197_init.h"
#include "da_pec_internal.h"
#include "device_mgmt.h"
#include "device_rw.h"
#include "driver_197_api.h"
#include "dtl.h"
#include "eip.h"
#include "hcp.h"
#include "log.h"
#include "pec_api.h"
#include "shim_eip_common.h"
#include "shim_platform.h"

#define PHY_RESET_MAX 20
#define PHY_RESET_DELAY 200
#define MII_MARVELL_COPPER_PAGE 0x00
#define MII_MARVELL_FIBER_PAGE 0x01
#define MARVELL_PHY_ID_88E1510 0x01410dd0

static struct eip_public *eip_pub_glb;

/* Axiado MAC OUI */
u8 ax_mac_oui[3] = { 0x00, 0xFB, 0xF9 };

#define DTL_DEV_NAME "ethdtl"
#define DFL_DEV_NAME "ethdfl"

/* return eip public structure needed by different functions */
struct eip_public *get_eip_public(void)
{
	return eip_pub_glb;
}

/* return user ring 2 for 10G, otherwise 1 */
u8 get_tx_user_ring_id(struct eip_public *pub, u8 app_id)
{
	if (app_id == NDEV_APP_ID_MAC0)
		return 2;

	return 1;
}

/* return user ring in round robin fashion */
u8 get_rx_user_ring_id(struct eip_public *pub, u8 app_id)
{
	static u8 rid = 2;

	if (rid == 2)
		rid = 1;
	else
		rid = 2;

	return rid;
}

/* Extract app_id from skb received on DTL netdev */
u8 get_app_id_eip_hdr(struct sk_buff *skb)
{
	struct eiphdr *eiph = eip_hdr(skb);

	return eiph->app_id > NDEV_APP_ID_MAC0 ? 0 : eiph->app_id;
}

/**
 * @brief eip_xmit_frame - This will be called by stack whenever packet is available to sent
 *
 * @param skb - socket buffer structure
 * @param dev - network interface device structure
 * @return int - 0 on success
 */
static int eip_xmit_frame(struct sk_buff *skb, struct net_device *ndev)
{
	struct eip_priv *priv = netdev_priv(ndev);
	struct eip_public *eip_pub = priv->eip_pub;
	u8 app_id = priv->app_id;
	bool ret = true;
	u8 rid = 0;

	LOG_DEBG("Xmit from %s - appid: %d", netdev_name(ndev), app_id);
	if (unlikely(app_id > NDEV_APP_ID_MAC0)) {
		if (app_id == NDEV_APP_ID_DTL) {
			app_id = get_app_id_eip_hdr(skb);
			if (app_id) {
				rid = get_rx_user_ring_id(eip_pub, app_id);
				add_dtl_src_rule(eip_pub, skb, rid);
			}
		}
	} else {
		if (eip_pub && eip_pub->sa_handle) {
			rid = get_tx_user_ring_id(eip_pub, app_id);
			/* For user ring, send all pkt to CDR */
			LOG_DEBG("TX on user ring-%d\n", rid);
			ret = eip_tx_send(priv, skb, eip_pub->sa_handle, rid);

		} else {
			LOG_CRIT("SA handle not created yet\n");
			goto err_no_sa_handle;
		}
	}

	if (!ret)
		return NETDEV_TX_BUSY;

	dev_kfree_skb(skb);
	return NETDEV_TX_OK;

err_no_sa_handle:
	// TODO: This freeing skb will change once we enable interrupt for proc_cd
	dev_kfree_skb(skb);
	return NETDEV_TX_OK;
}

/**
 * @brief eip_get_stats64 - Get System Network Statistics
 *
 * @param dev -  network interface device structure
 * @netdev: network interface device structure
 * @stats: rtnl_link_stats64 pointer
 */
static void eip_get_stats64(struct net_device *dev,
			    struct rtnl_link_stats64 *stats)
{
	struct eip_priv *priv = netdev_priv(dev);
	struct rtnl_link_stats64 *net_stats = &priv->stats64;

	memcpy(stats, net_stats, sizeof(*stats));
}

/**
 * @brief eip_set_mac - Change the Ethernet address of the NIC
 *
 * @param dev -  network interface device structure
 * @param address - mac address pointer
 * @return int - 0 on success, negative on failure
 */
static int eip_set_mac(struct net_device *dev, void *address)
{
	int err;
	struct sockaddr *addr = address;
	struct eip_priv *priv;
	u32 mac_0 = 0, mac_1 = 0;

	/* Don't allow mac-change if interface is up */
	err = eth_prepare_mac_addr_change(dev, addr);
	if (err < 0)
		return err;

	eth_hw_addr_set(dev, addr->sa_data);

	priv = netdev_priv(dev);
	if (priv->app_id && priv->app_id <= 5) {
		mac_0 = (u32)addr->sa_data[0] | (u32)addr->sa_data[1] << 8 |
			(u32)addr->sa_data[2] << 16 |
			(u32)addr->sa_data[3] << 24;

		mac_1 = (u32)addr->sa_data[4] | (u32)addr->sa_data[5] << 8;

		mac_addr_app_id_wr(priv->app_id, mac_0, mac_1);
	}

	return 0;
}

/**
 * @brief Set the initial mac address for netdev interface while opening
 * the interface. This also sets mac address for eipdtl interface.
 *
 * @param ndev Pointer to netdev struct.
 * @param mac_0 4B MAC read from HW reg.
 * @param mac_1 2B MAC read from HW reg.
 */
void set_initial_mac(struct net_device *ndev, u32 mac_0, u32 mac_1)
{
	struct eip_priv *priv;
	u8 random_mac[3] = { 0 }, mac_addr[6] = { 0 };

	priv = netdev_priv(ndev);

	if (!mac_0 || !mac_1) {
		/* Get 3 random bytes using kernel API */
		get_random_bytes(random_mac, 3);

		/* Copy Axiado-OUI into first 3 bytes (0th, 1st and 2nd) of MAC-address. */
		memcpy(mac_addr, ax_mac_oui, 3);
		/* Copy random 3 bytes into last 3 bytes (3rd, 4th and 5th) of MAC-address. */
		memcpy(mac_addr + 3, random_mac, 3);
	} else {
		memcpy(mac_addr, &mac_0, 4);
		memcpy(mac_addr + 4, &mac_1, 2);
	}
	/* Copy 6 bytes MAC addr into netdev-mac-address array */
	eth_hw_addr_set(ndev, mac_addr);
}

/**
 * @brief reset the phy if mac status failed with rx loc fault
 */
static void phy_reset_retry(struct work_struct *work)
{
	struct eip_public *eip_pub =
		container_of(work, struct eip_public, rxaui_phy_reset);
	struct net_device *ndev = eip_pub->ndev_list[0]; // MAC-0

	phy_stop(ndev->phydev);
	msleep(PHY_RESET_DELAY);
	phy_start(ndev->phydev);
}

/**
 * @brief  - config/tweak mac on link up/down, netdev-priv is link up/downed
 * @param netdev - network interface device structuer
 * @param updown  - true if link to be made up, false otherwise.
 * @return
 *
 */
static void eip_eth_link_updown(struct net_device *netdev, bool updown)
{
	struct eip_priv *priv = NULL;
	struct eip_public *eip_pub = NULL;
	struct phy_device *phydev = NULL;
	int mac_idx;
	static u8 phy_reset_retry[MAX_MAC_CNT] = { 0 };
	bool phy_retry = false;

	priv = netdev_priv(netdev);
	if (updown == priv->link)
		return;

	phydev = netdev->phydev;
	mac_idx = priv->app_id == NDEV_APP_ID_MAC0 ? 0 : priv->app_id;
	priv->link = updown;
	if (priv->link) {
		if (!phydev) {
			netif_carrier_on(netdev);
			/*Assume non-phy netdev always 1Gbps/FD with flow control off*/
			netdev_info(netdev,
				    "%s: Link is Up - 1Gbps/Full - flow control off\n",
				netdev_name(netdev));
		} else {
			if (mac_idx) {
				/* check for autonegcomplete for Marvell COPPER/FIBER.
				 * Marvell Phy is requiring autonegcomplete on both
				 * Copper & Fiber mode for Tx/Rx to work */
				if (phy_id_compare(MARVELL_PHY_ID_88E1510, phydev->phy_id,
							phydev->drv->phy_id_mask)) {
					phy_retry =
						!(phy_read_paged(phydev,
									MII_MARVELL_COPPER_PAGE,
									MII_BMSR) &
								BMSR_ANEGCOMPLETE) ||
						!(phy_read_paged(phydev,
									MII_MARVELL_FIBER_PAGE,
									MII_BMSR) &
								BMSR_ANEGCOMPLETE);
				}
			} else {
				phy_retry = is_rx_local_fault(mac_idx);
			}
			if (phy_retry) {
				LOG_CRIT(
					"%s: PHY-Quirks - phy-reset retry: %u\n",
					netdev_name(netdev),
					phy_reset_retry[mac_idx] + 1);
				if (phy_reset_retry[mac_idx] == PHY_RESET_MAX) {
					LOG_CRIT(
						"Error max reset retry reached %d\n",
						PHY_RESET_MAX);
					phy_reset_retry[mac_idx] = 0;
				} else {
					if (!mac_idx) {
						eip_pub = priv->eip_pub;
						/* schedule phy-reset worker thread */
						schedule_work(&eip_pub->rxaui_phy_reset);
					} else {
						/* soft reset the Port */
						shim_mac_soft_reset(mac_idx);
						/* soft reset phy */
						if (genphy_soft_reset(
							    phydev) <
						    0) {
							pr_crit("Phy soft reset failed\n");
						}
					}
					phy_reset_retry[mac_idx]++;
					netif_carrier_off(netdev);
					goto end;
				}
			} else {
				phy_reset_retry[mac_idx] = 0;
			}
		}
		netif_start_queue(netdev);
	} else {
		netif_stop_queue(netdev);
		if (!phydev) {
			netif_carrier_off(netdev);
			netdev_info(netdev, "%s: Link is Down\n",
				    netdev_name(netdev));
		}
		if (mac_idx < MAX_MAC_CNT) {
			LOG_INFO("%s:reset shim-fifo on phy[%d] link-down\n",
				 __func__, mac_idx);
			shim_fifo_reset(mac_idx);
		}
	}
end:
	return;
}

/**
 * @brief  - callback registered in phy_connect
 *
 * @param dev - network interface device structure
 * @return
 *
 */
static void eip_eth_adjust_link(struct net_device *netdev)
{
	if (netdev && netdev->phydev) {
		phy_print_status(netdev->phydev);
		eip_eth_link_updown(netdev, netdev->phydev->link);
		return;
	}
	LOG_WARN("%s: netdev isn't connected to phydev\n", __func__);
}

/**
 * @brief  - setup/initialize/connect/start phy and if reqd bind to netdev
 *
 * @param netdev - network interface device structure
 * @return
 *
 */
void eip_start_phy(struct net_device *netdev)
{
	struct phy_device *phydev = NULL;
	struct mii_bus *mdiobus = NULL;
	struct eip_priv *priv;
	struct mac_phy *mac_cfg = NULL;
	phy_interface_t phy_iface;
	u8 addr = -1;
	int ret = 0;

	if (!netdev) {
		WARN_ON(!netdev);
		return;
	}

	priv = netdev_priv(netdev);
	if (priv->app_id == NDEV_APP_ID_DFL)
		return;

	addr = priv->app_id;
	if (priv->app_id == NDEV_APP_ID_MAC0)
		addr = 0;

	/* Get MAC config structure from HCP device */
	if (!priv->eip_pub || !priv->eip_pub->hcp) {
		netdev_err(netdev, "%s:HCP device not available\n",
			   netdev_name(netdev));
		return;
	}
	if (!priv->eip_pub->hcp->shim_priv) {
		netdev_err(netdev, "%s:Shim error - can't get mac_phy\n",
			   netdev_name(netdev));
		return;
	}
	mac_cfg = hcp_get_mac_cfg(priv->eip_pub->hcp);

	/* Initialize PHY interface mode from config */
	phy_iface = mac_cfg[addr].phy_mode;

	/* Check if already attached/connected */
	if (netdev->phydev && netdev->phydev->attached_dev) {
		phydev = netdev->phydev;
		pr_info("%s(%d):PHY[%d]-attached, starting phy-sm.\n", __func__,
			__LINE__, addr);
		goto eip_phy_fixup_and_start;
	}

	/* Get MDIO bus from HCP device */
	mdiobus = priv->eip_pub->hcp->mdio_bus;
	if (!mdiobus) {
		netdev_info(netdev, "%s:MDIO bus not available\n",
			    netdev_name(netdev));
		return;
	}

	/* Get the PHY device from the MDIO bus.
	 * Prefer the cached pointer populated by shim_dev_probe.
	 */
	phydev = mac_cfg[addr].phydev;
	if (!phydev) {
		pr_info("%s(%d):MAC-%02d: No PHY found on MDIO bus at address %d\n",
			__func__, __LINE__, addr, addr);
		return;
	}

	netdev_info(netdev, "PHY[%d]: device %s, driver %s\n",
		    phydev->mdio.addr, phydev_name(phydev),
		    phydev->drv ? phydev->drv->name : "unknown");

	/* Connect the PHY to the network device.
	 * This attaches the generic PHY driver and registers the link state callback.
	 */
	phydev = phy_connect(netdev, phydev_name(phydev), eip_eth_adjust_link,
			     phy_iface);
	if (IS_ERR(phydev)) {
		netdev_err(netdev, "%s: Could not attach to PHY\n",
			   netdev_name(netdev));
		/* Clean up the global pointer on failure */
		mac_cfg[addr].phydev = NULL;
		return;
	}

	/* Now that phydev is connected, enable the hardware interrupts.
	 * The interrupt handler (shim_phy_interrupt_handler) can now
	 * safely access mac_cfg[addr].phydev.
	 */
	if (mac_cfg[addr].mac_irq > 0)
		shim_irq_enable(addr);

	phy_attached_info(phydev);

eip_phy_fixup_and_start:
	if (phy_iface == PHY_INTERFACE_MODE_RXAUI) {
		if (mac_cfg[0].mdi_swap) {
			/* Swap abcd to dcba. */
			pr_info("%s(%d): swap abcd to dcba", __func__,
				__LINE__);
			ret = phy_modify_mmd_changed(phydev, SWAP_DEV_ADDR,
						     SWAP_REG, SWAP_MASK,
						     SWAP_SET);
			if (ret < 0)
				pr_crit("phy_modify_mmd_changed error devad = %x\n",
					SWAP_DEV_ADDR);
		}
		/* set X2 Disparity*/
		ret = phy_modify_mmd_changed(phydev, _10GBASE_X4_ADDR,
					     _10GBASE_X4_REG, _10GBASE_X4_MASK,
					     _10GBASE_X4_SET);
		if (ret < 0)
			pr_crit("phy_modify_mmd_changed error devad = %x\n",
				_10GBASE_X4_ADDR);
	}

	phy_start(phydev);
}

/**
 * @brief  - stop phy connected to netdev
 *
 * @param dev - network interface device structure
 * @return
 */
void eip_stop_phy(struct net_device *netdev)
{
	struct eip_priv *priv;

	if (!netdev || !netdev->phydev)
		return;
	priv = netdev_priv(netdev);
	if (priv->app_id == MAC_10G_APPID)
		phy_disconnect(netdev->phydev);
	else
		phy_stop(netdev->phydev);
}

/**
 * @brief eip_open - Called when a network interface is made active
 *
 * @param ndev - network interface device structure
 * @return int - 0 on success and negative value on failure
 *
 * The open entry point is called when a network interface is made
 * active by the system (IFF_UP).  At this point all resources needed
 * for transmit and receive operations are allocated,  and the stack is
 * notified that the interface is ready.
 */
static int eip_open(struct net_device *ndev)
{
	struct eip_public *eip_pub;
	struct eip_priv *priv;
	struct eip_ring_interface *eip_ring;
	int i;

	priv = netdev_priv(ndev);
	eip_pub = priv->eip_pub;

	LOG_DEBG("%s called: %s\n", __func__, netdev_name(ndev));
	if (priv->app_id == NDEV_APP_ID_DTL) {
		netif_start_queue(ndev);
		LOG_DEBG("started dtl interface\n");
		return 0;
	}

	if (!eip_pub->rdr_napi_cnt) {
		for (i = 0; i < RING_INTERFACE_CNT; i++) {
			eip_ring = &eip_pub->eip_ring[i];
			//TODO: hndle IPSEC_LOOKASIDE_INTERFACE
			if (i != REJECTED_FLOW_RULE_RING_ID)
				napi_enable(&eip_ring->rdr_handler.napi_ring);
			enable_rdr_irq(eip_ring->rid);
		}
	}

	eip_pub->rdr_napi_cnt++;
	eip_start_phy(ndev);
	/*netdev up, directly if no phy*/
	if (!ndev->phydev)
		eip_eth_link_updown(ndev, true);

	LOG_INFO("Initialized/Started netdev\n");

	return 0;
}

/**
 * @brief eip_close Disables the network interface
 *
 * @param ndev - network interface device structure
 * @return int - always returns 0
 *
 * The close entry point is called when an interface is de-activated
 * by the OS, and all transmit and receive resources are freed
 */
static int eip_close(struct net_device *ndev)
{
	struct eip_priv *priv = netdev_priv(ndev);
	struct eip_public *eip_pub = priv->eip_pub;
	struct eip_ring_interface *eip_ring;
	int i;

	netif_stop_queue(ndev);
	if (priv->app_id == NDEV_APP_ID_DTL)
		return 0;

	if (!eip_pub->rdr_napi_cnt) {
		LOG_CRIT("ERROR - THIS SHOULD NEVER HAPPEN\n");
		goto out_link_down;
	}
	eip_pub->rdr_napi_cnt--;
	if (!eip_pub->rdr_napi_cnt) {
		for (i = 0; i < RING_INTERFACE_CNT; i++) {
			eip_ring = &eip_pub->eip_ring[i];
			disable_rdr_irq(i);
			//TODO: handle IPSEC_LOOKASIDE_INTERFACE
			if (i == REJECTED_FLOW_RULE_RING_ID) {
				tasklet_kill(&eip_ring->rdr_handler.tasklet_ring);
			} else {
				napi_disable(&eip_ring->rdr_handler.napi_ring);
				netif_napi_del(&eip_ring->rdr_handler.napi_ring);
			}
		}
	}

out_link_down:
	if (ndev->phydev)
		eip_stop_phy(ndev);
	else
		eip_eth_link_updown(ndev, false);

	return 0;
}

/* ethtool support */
/**
 * @brief enable/disable tx/rx pause.
 */
static void eip_get_pauseparam(struct net_device *netdev,
			       struct ethtool_pauseparam *pause)
{
	struct phy_device *phydev = NULL;

	if (!netdev || !netdev->phydev) {
		LOG_CRIT("netdev or phydev is NULL\n");
		return;
	}
	phydev = netdev->phydev;

	phy_get_pause(phydev, (bool *)&pause->tx_pause,
		      (bool *)&pause->rx_pause);
}

static int eip_set_pauseparam(struct net_device *netdev,
			      struct ethtool_pauseparam *pause)
{
	struct phy_device *phydev = NULL;
	bool sync_pause;

	if (!netdev || !netdev->phydev) {
		LOG_CRIT("netdev or phydev is NULL\n");
		return -ENODEV;
	}
	phydev = netdev->phydev;

	if (!phy_validate_pause(phydev, pause)) {
		LOG_CRIT("pause frame settings not supported by PHY\n");
		return -EINVAL;
	}

	sync_pause = !(phydev->asym_pause);
	if (sync_pause) {
		phy_set_sym_pause(phydev, pause->tx_pause, pause->rx_pause,
				  pause->autoneg);
	} else {
		if (pause->tx_pause)
			phy_set_asym_pause(phydev, 1, 0); // Set TX Pause only
		if (pause->rx_pause)
			phy_set_asym_pause(phydev, 0, 1); // Set RX Pause only
	}

	return 0;
}

/**
 *@brief driver info query, currently dummy
 */
static void eip_get_drvinfo(struct net_device *netdev,
			    struct ethtool_drvinfo *info)
{
	if (!netdev) {
		LOG_CRIT("%s: invalid arguments(netdev)\n", __func__);
		return;
	}
	if (!info) {
		LOG_CRIT("%s: invalid arguments(info)\n", __func__);
		return;
	}

	strscpy(info->driver, "AX3000-driver", sizeof(info->driver));
	strscpy(info->version, "1.0", sizeof(info->version));
	if (netdev->dev.parent) {
		strscpy(info->bus_info, dev_name(netdev->dev.parent),
			sizeof(info->bus_info));
	} else {
		LOG_CRIT("%s: netdev->dev.parent is NULL\n", __func__);
		strscpy(info->bus_info, "unknown", sizeof(info->bus_info));
	}
}

/**
 *@brief function to retrieve link settings
 */
static int eip_get_link_ksettings(struct net_device *netdev,
				  struct ethtool_link_ksettings *cmd)
{
	struct eip_priv *priv;
	int status = 0;

	if (!netdev) {
		LOG_CRIT("%s: invalid arguments(netdev)\n", __func__);
		return -EINVAL;
	}

	priv = netdev_priv(netdev);

	if (!cmd) {
		LOG_CRIT("%s: invalid arguments(cmd)\n", __func__);
		return -EINVAL;
	}

	if (netdev->phydev) {
		status = phy_read_status(netdev->phydev);
		if (status) {
			LOG_CRIT("%s:Failed to read PHY status\n", __func__);
			return -EIO;
		}
	} else {
		/* Log useful info: Interface Name (ethX), Interface ID (Ring), and App ID (MAC) */
		pr_warn("%s: netdev %s (app_id:%d) has no phydev\n", __func__,
			netdev->name, priv->app_id);
		return -ENODEV;
	}

	phy_ethtool_ksettings_get(netdev->phydev, cmd);
	__set_bit(ETHTOOL_LINK_MODE_TP_BIT, cmd->link_modes.supported);
	__set_bit(ETHTOOL_LINK_MODE_FIBRE_BIT, cmd->link_modes.supported);
	__set_bit(ETHTOOL_LINK_MODE_MII_BIT, cmd->link_modes.supported);
	return 0;
}

/**
 *@brief function to set link settings
 */
static int eip_set_link_ksettings(struct net_device *netdev,
				  const struct ethtool_link_ksettings *cmd)
{
	int ret;

	if (!netdev) {
		LOG_CRIT("%s: invalid arguments(netdev)\n", __func__);
		return -EINVAL;
	}
	if (!cmd) {
		LOG_CRIT("%s: invalid arguments(cmd)\n", __func__);
		return -EINVAL;
	}
	if (!netdev->phydev)
		return -EINVAL;

	ret = phy_ethtool_ksettings_set(netdev->phydev, cmd);
	return ret;
}

static void eip_get_wol(struct net_device *dev, struct ethtool_wolinfo *wol)
{
	wol->supported = 0;
	wol->wolopts = 0;
	if (dev && dev->phydev)
		phy_ethtool_get_wol(dev->phydev, wol);
}

static int eip_set_wol(struct net_device *dev, struct ethtool_wolinfo *wol)
{
	int err;

	if (dev && !dev->phydev)
		return -ENODEV;

	err = phy_ethtool_set_wol(dev->phydev, wol);
	return err;
}

static void eip_get_regs(struct net_device *netdev, struct ethtool_regs *regs,
			 void *data)
{
	struct eip_priv *priv = NULL;

	if (!netdev) {
		LOG_CRIT("%s: invalid arguments(netdev)\n", __func__);
		return;
	}
	priv = netdev_priv(netdev);
	if (!priv) {
		LOG_CRIT("%s: netdev-priv err\n", __func__);
		return;
	}
	regs->version = 1; // Register version
	//memcpy(data, priv->registers, eip_get_regs_len(netdev));
}

static const char * const eip_stats[] = {
	"rx_packets",
	"tx_packets",
	"rx_bytes",
	"tx_bytes",
	"rx_errors",
	"tx_errors",
	"rx_dropped",
	"tx_dropped",
	"Associated RING_ID wth iface",
	"Default Ring TX",
	"Default Ring RX",
	"Default Ring Debug counter",
	"Tx Ring counter",
	"Rx Ring counter",
	"Ring Debug counter",
	"SHIM_RX_Total",
	"SHIM_RX_Good",
	"SHIM_RX_Bad",
	"RX_MAC_Good",
	"RX_MAC_Drop",
	"RX MAC CRC Err",
	"RX_MAC_Total",
	"RX MAC If In Err",
	"RX MAC Undersize Err",
	"RX MAC Oversize Err",
	"RX MAC Jabber Err",
	"RX MAC Frag Err",
	"Tx MAC Total",
	"Tx MAC Good",
	"Tx MAC Drop",
	"Tx MAC CRC Err",
	"Tx MAC If Out Err",
};

static void eip_get_strings(struct net_device *netdev, u32 stringset, u8 *data)
{
	int i = 0;

	if (!netdev || !data) {
		LOG_CRIT("%s: invalid arguments(netdev)\n", __func__);
		return;
	}
	for (i = 0; i < EIP_STATS_COUNT; i++) {
		strscpy(data + i * ETH_GSTRING_LEN, eip_stats[i],
			ETH_GSTRING_LEN);
	}
}

static int eip_get_sset_count(struct net_device *netdev, int sset)
{
	return EIP_STATS_COUNT;
}

static void eip_get_stats(struct net_device *netdev,
			  struct ethtool_stats *stats, u64 *data)
{
	struct eip_priv *priv = NULL;
	struct rtnl_link_stats64 stats64;
	u32 shim_rx_stat[3] = { 0 };
	u32 shim_mac_rx_stat[9] = { 0 };
	u32 shim_mac_tx_stat[5] = { 0 };
	u16 i = 0;
	u16 j = 0;
	u8 addr = 0;
	u8 iface = 0;
	device_handle_t dev;

	if (!netdev || !data) {
		LOG_CRIT("%s: invalid arguments(netdev)\n", __func__);
		return;
	}

	priv = netdev_priv(netdev);
	if (!priv) {
		LOG_CRIT("%s: netdev-priv err\n", __func__);
		return;
	}

	eip_get_stats64(netdev, &stats64);
	data[j++] = stats64.rx_packets;
	data[j++] = stats64.tx_packets;
	data[j++] = stats64.rx_bytes;
	data[j++] = stats64.tx_bytes;
	data[j++] = stats64.rx_errors;
	data[j++] = stats64.tx_errors;
	data[j++] = stats64.rx_dropped;
	data[j++] = stats64.tx_dropped;

	/* eip-ring-stats */
	addr = priv->app_id;
	if (addr == NDEV_APP_ID_MAC0) {
		addr = 0;
		iface = 2; /* 10G port connected to mac-0 & netdev interface eth2 */
	} else {
		iface = priv->app_id;
	}

	data[j++] = iface;
	dev = eip_get_dev_handle(priv->eip_pub);

	if (dev) {
		data[j++] = device_read32(dev,
					  EIP_RING_IN_BASE +
					  EIP_DEFAULT_RING *
					  STAT_RING_OFFSET_MULTIPLIER);
		data[j++] = device_read32(dev,
					  EIP_RING_OUT_BASE +
					  EIP_DEFAULT_RING *
					  STAT_RING_OFFSET_MULTIPLIER);
		data[j++] = device_read32(dev,
					  EIP_RING_DBG_BASE +
					  EIP_DEFAULT_RING *
					  STAT_RING_OFFSET_MULTIPLIER);
		data[j++] = device_read32(dev,
					  EIP_RING_IN_BASE +
					  iface *
					  STAT_RING_OFFSET_MULTIPLIER);
		data[j++] = device_read32(dev,
					  EIP_RING_OUT_BASE +
					  iface *
					  STAT_RING_OFFSET_MULTIPLIER);
		data[j++] = device_read32(dev,
					  EIP_RING_DBG_BASE +
					  iface *
					  STAT_RING_OFFSET_MULTIPLIER);
	} else {
		for (i = 0; i < 6; i++)
			data[j++] = 0;
	}

	shim_read_shim_stats(shim_rx_stat, addr);

	for (i = 0; i < 3; i++)
		data[j++] = shim_rx_stat[i];

	shim_read_mac_rx_stats(shim_mac_rx_stat, addr);

	for (i = 0; i < 9; i++)
		data[j++] = shim_mac_rx_stat[i];

	shim_read_mac_tx_stats(shim_mac_tx_stat, addr);

	for (i = 0; i < 5; i++)
		data[j++] = shim_mac_tx_stat[i];
}

/**
 * @brief ethtool_ops support
 */
static const struct ethtool_ops eip_ethtool_ops = {
	.get_drvinfo = eip_get_drvinfo, // Provide driver info
	.get_link = ethtool_op_get_link, // Return link status
	.get_link_ksettings = eip_get_link_ksettings,
	.set_link_ksettings = eip_set_link_ksettings,
	.get_strings = eip_get_strings, // Get supported statistics strings
	.get_ethtool_stats = eip_get_stats, // Retrieve driver statistics
	.get_sset_count = eip_get_sset_count, // count of stats supported
	.get_regs = eip_get_regs, // Dump register contents
	.get_wol = eip_get_wol,
	.set_wol = eip_set_wol,
	.get_pauseparam = eip_get_pauseparam,
	.set_pauseparam = eip_set_pauseparam,

};

/* end ethtool */

static void eip_set_rx_mode(struct net_device *netdev)
{
	struct eip_priv *priv = netdev_priv(netdev);

	if (priv->app_id && priv->app_id <= NDEV_APP_ID_MAC0) {
		mac_update_promisc(priv->app_id,
				   !!(netdev->flags & IFF_PROMISC));
	}
}

/* @brief Called when a network interface is unloaded
 * @param ndev - network interface device structure
 */
static void eip_uninit(struct net_device *ndev)
{
	if (ndev->phydev)
		phy_disconnect(ndev->phydev);
}

static const struct net_device_ops eip_ndo = {
	.ndo_uninit = eip_uninit,
	.ndo_open = eip_open,
	.ndo_stop = eip_close,
	.ndo_start_xmit = eip_xmit_frame,
	.ndo_set_rx_mode = eip_set_rx_mode,
	.ndo_set_mac_address = eip_set_mac,
	.ndo_get_stats64 = eip_get_stats64,
};

/**
 * @brief eip_setup register the netdevops
 *
 * @param dev network device
 */
static void eip_setup(struct net_device *dev)
{
	/* setting the ethernet feilds for netdev structure */
	ether_setup(dev);
	dev->netdev_ops = &eip_ndo;
	dev->ethtool_ops = &eip_ethtool_ops;
#ifdef CONFIG_EIP_IPSEC_OFFLOAD
	eip_ipsec_init(dev);
#endif
}

int device_init(struct eip_public *eip_pub, struct device *dev)
{
	struct net_device *ndev;
	struct eip_priv *priv;
	struct eip_ring_interface *eip_ring;
	int i, err = 0;
	u32 mac_0, mac_1;

	for (i = 0; i < RING_INTERFACE_CNT; i++) {
		eip_ring = &eip_pub->eip_ring[i];
		eip_ring->rid = i;
		eip_ring->eip_pub = eip_pub;

		err = eip_create_rx_buffers(eip_ring);
		if (err)
			goto err;
		err = eip_create_tx_buffers(eip_ring);
		if (err)
			goto err;
		err = eip_pec_init(i);
		if (err)
			goto err;
		err = eip_prepare_rds(eip_ring, MAX_RD_COUNT);
		if (err)
			goto err;

		err = interrupt_init(eip_ring,
				     eip_pub->ndev_list[NDEV_APP_ID_DFL - 1], dev);
		if (err)
			goto err;
	}

	err = eip_pcl_dtl_init(eip_pub, DEFAULT_INTERFACE_ID, LOOKUP_TABLE_CNT);
	if (err)
		goto err;

	eip_pub->sa_handle = kmalloc(sizeof(*eip_pub->sa_handle), GFP_KERNEL);
	if (!eip_pub->sa_handle ||
	    !eip_create_tx_sa(true, eip_pub->sa_handle)) {
		err = -ENOMEM;
		goto err;
	}

	for (i = 0; i < MAX_NETDEV_CNT; i++) {
		if (!eip_pub->ndev_list[i])
			continue;
		ndev = eip_pub->ndev_list[i];
		priv = netdev_priv(ndev);

		/* init phy_reset_retry worker thread for MAC-0 */
		if (i == 0) {
			LOG_DEBG("Registering worker thread for phy-reset-retry\n");
			INIT_WORK(&eip_pub->rxaui_phy_reset, phy_reset_retry);
		}

		mac_0 = 0;
		mac_1 = 0;
		if (i < MAX_MAC_CNT)
			mac_addr_app_id_rd(priv->app_id, &mac_0, &mac_1);

		if (ndev->dev_addr)
			set_initial_mac(ndev, mac_0, mac_1);

		/* set MAC address in HW-MAC-Port */
		if (i < MAX_MAC_CNT && (!mac_0 || !mac_1)) {
			mac_0 = (u32)ndev->dev_addr[0] |
				(u32)ndev->dev_addr[1] << 8 |
				(u32)ndev->dev_addr[2] << 16 |
				(u32)ndev->dev_addr[3] << 24;

			mac_1 = (u32)ndev->dev_addr[4] | (u32)ndev->dev_addr[5]
								 << 8;
			LOG_DEBG("setting MAC for: %d - %s\n", i,
				 netdev_name(ndev));
			mac_addr_app_id_wr(priv->app_id, mac_0, mac_1);
		}
	}

	return 0;

err:
	eip_pcl_dtl_uninit(eip_pub, DEFAULT_INTERFACE_ID, LOOKUP_TABLE_CNT);

	for (i = 0; i < RING_INTERFACE_CNT; i++) {
		eip_ring = &eip_pub->eip_ring[i];
		eip_free_irq(eip_ring);
		eip_pec_uninit(i);
		eip_delete_tx_buffers(eip_ring);
		eip_delete_rx_buffers(eip_ring);
	}
	kfree(eip_pub->sa_handle);
	eip_pub->sa_handle = NULL;

	return err;
}

/* get_netdev_interfaces - Get network device interfaces with priority ordering
 * @hcp: HCP device structure
 * @app_id: Output array to store app_id for each enabled MAC
 *
 * Returns the number of enabled MACs and fills app_id array with their IDs.
 * Network devices are created in priority order:
 *   1. 1G MACs with external PHY
 *   2. 10G MAC with external PHY
 *   3. 1G MACs with SGMII phy-mode (no external PHY)
 *   4. Remaining MACs (RMII, etc.)
 *
 * Return: Number of enabled network interfaces
 */
u8 get_netdev_interfaces(struct hcp_device *hcp, u8 *app_id)
{
	struct mac_phy *mac_cfg = NULL;
	bool macs[MAX_MAC_CNT] = { false };
	u8 i, ifc_cnt = 0;

	if (!hcp || !hcp->shim_priv)
		return 0;

	mac_cfg = hcp_get_mac_cfg(hcp);

	/* Get all the enabled MACs */
	for (i = 0; i < MAX_MAC_CNT; i++)
		macs[i] = mac_cfg[i].enabled;

	/* Priority 1: 1G MACs (1-4) with external PHY */
	for (i = 1; i < MAX_MAC_CNT; i++) {
		if (macs[i] && mac_cfg[i].phydev) {
			app_id[ifc_cnt] = mac_cfg[i].app_id;
			ifc_cnt++;
			macs[i] = false;
		}
	}

	/* Priority 2: 10G MAC (MAC-0 RXAUI) with external PHY */
	if (macs[0] && mac_cfg[0].phydev) {
		app_id[ifc_cnt] = mac_cfg[0].app_id;
		ifc_cnt++;
		macs[0] = false;
	}

	/* Priority 3: 1G MACs with SGMII phy-mode (no external PHY) */
	for (i = 1; i < MAX_MAC_CNT; i++) {
		if (macs[i] &&
		    mac_cfg[i].phy_mode == PHY_INTERFACE_MODE_SGMII) {
			app_id[ifc_cnt] = mac_cfg[i].app_id;
			ifc_cnt++;
			macs[i] = false;
		}
	}

	/* Priority 4: Remaining MACs (RMII, etc.) */
	for (i = 1; i < MAX_MAC_CNT; i++) {
		if (macs[i]) {
			app_id[ifc_cnt] = mac_cfg[i].app_id;
			ifc_cnt++;
		}
	}

	return ifc_cnt;
}

/**
 * @brief eip_netdev_init - Driver Registration Routine
 *
 * @param hcp HCP device structure
 * @return int
 */
static int eip_netdev_init(struct hcp_device *hcp)
{
	struct eip_public *eip_pub = NULL;
	struct eip_priv *priv = NULL;
	struct net_device *ndev;
	struct device *dev = hcp->dev;
	char *ifname[MAX_NETDEV_CNT] = { "eth0",      "eth1", "eth2",
					 "eth3",      "eth4", DFL_DEV_NAME,
					 DTL_DEV_NAME };
	int err;
	u8 i;
	u8 interface_cnt = 0;
	u8 app_id[MAX_MAC_CNT] = { 0 };

	interface_cnt = get_netdev_interfaces(hcp, app_id);
	if (!interface_cnt) {
		LOG_CRIT("ERROR - No MACs enabled\n");
		return -EINVAL;
	}
	LOG_CRIT("Interface count: %u\n", interface_cnt);

	err = driver197_init();
	if (err < 0) {
		LOG_CRIT("ERROR:%d - EIP-197 init failed\n", err);
		return err;
	}

	/* Alloc eip_public */
	eip_pub = kzalloc(sizeof(*eip_pub), GFP_KERNEL);
	if (!eip_pub) {
		LOG_CRIT("Failed to alloc memory for eip_public");
		err = -ENOMEM;
		goto err;
	}
	eip_pub_glb = eip_pub;

	eip_pub->dev_handle = eip_device_find("EIP197_GLOBAL");
	if (!eip_pub->dev_handle) {
		LOG_CRIT("Failed to find EIP197_GLOBAL device\n");
		err = -ENODEV;
		goto err;
	}

	/* Store HCP device pointer for accessing shared resources */
	eip_pub->hcp = hcp;

	/* Allocate all netdevs first before registering any of them */
	for (i = 0; i < MAX_NETDEV_CNT; i++) {
		if (i == interface_cnt)
			i = MAX_NETDEV_CNT - 2;
		ndev = alloc_netdev(sizeof(struct eip_priv), ifname[i],
				    NET_NAME_UNKNOWN, eip_setup);
		if (!ndev) {
			err = -ENOMEM;
			LOG_CRIT("Failed to alloc eip netdev for %s\n",
				 ifname[i]);
			goto err;
		}

		priv = netdev_priv(ndev);
		priv->eip_pub = eip_pub;
		/* add ndev to ndev_list based on MAC idx */
		if (i < interface_cnt) {
			priv->app_id = app_id[i];
			if (app_id[i] == NDEV_APP_ID_MAC0)
				eip_pub->ndev_list[0] = ndev;
			else
				eip_pub->ndev_list[app_id[i]] = ndev;
		} else {
			if (strcmp(ifname[i], DFL_DEV_NAME) == 0) {
				priv->app_id = NDEV_APP_ID_DFL;
				eip_pub->ndev_list[NDEV_APP_ID_DFL - 1] = ndev;
			} else if (strcmp(ifname[i], DTL_DEV_NAME) == 0) {
				priv->app_id = NDEV_APP_ID_DTL;
				eip_pub->ndev_list[NDEV_APP_ID_DTL - 1] = ndev;
			} else {
				priv->app_id = 0xFF;
				LOG_CRIT("Invalid interface name-%s or index-%u\n",
					 ifname[i], i);
			}
		}
		LOG_CRIT("Netdev: %s, app_id: %u, index: %u\n", ifname[i],
			 priv->app_id, i);
	}

	/* Initialize device structures before registering.
	 * This must happen before register_netdev() to avoid race conditions
	 * where userspace (systemd/NetworkManager) tries to bring up the
	 * interface before priv->eip_pub is initialized.
	 */
	if (device_init(eip_pub, dev)) {
		LOG_CRIT("Failed to initialise device data");
		err = -1;
		goto err;
	}

#ifdef CONFIG_EIP_IPSEC_OFFLOAD
	ptr = kmalloc(sizeof(*eip_pub->eip_ipsec), GFP_KERNEL);
	if (!ptr) {
		err = -ENOMEM;
		goto err;
	}
	eip_pub->eip_ipsec = (struct eip_ipsec *)ptr;

	ptr = kcalloc(MAX_TX_IPSEC_SA, sizeof(struct ipsec_handle), GFP_KERNEL);
	if (!ptr) {
		err = -ENOMEM;
		goto err;
	}
	eip_pub->eip_ipsec->tx_ipsec_handle = ptr;

	ptr = kcalloc(MAX_RX_IPSEC_SA, sizeof(struct ipsec_handle), GFP_KERNEL);
	if (!ptr) {
		err = -ENOMEM;
		goto err;
	}
	priv->eip_pub->eip_ipsec->rx_ipsec_handle = ptr;

	init_waitqueue_head(&eip_pub->hmac_key_offload_wait_q.queue);
#endif

	for (i = 0; i < MAX_NETDEV_CNT; i++) {
		if (eip_pub->ndev_list[i]) {
			ndev = eip_pub->ndev_list[i];
			LOG_DEBG("Registering netdev for: %s\n",
				 netdev_name(ndev));
			if (i < MAX_MAC_CNT)
				SET_NETDEV_DEV(ndev, hcp->dev);
			err = register_netdev(ndev);
			if (err) {
				LOG_CRIT("Failed to register eip netdev for %d %s\n",
					 i, netdev_name(ndev));
				goto err;
			}
		}
	}

	shim_mac_enable_all_irq();

	/* Module-load: add details of configs */
	LOG_DEBG("Max hw descriptors:%d\n", DRIVER_PEC_MAX_PACKETS);
	LOG_DEBG("Max DDK logic-descriptors:%d\n", DRIVER_MAX_PECLOGICDESCR);
	LOG_DEBG("Max EIP-RX logic descriptors:%d\n", MAX_RD_COUNT);
	LOG_DEBG("Max EIP-TX logic descriptors:%d\n", EIP_MAX_CDR);
	LOG_DEBG("Max PE:%d\n", EIP_MAX_PE);

	LOG_INFO("Initialised Netdev: %d", interface_cnt);
	return 0;

err:
	LOG_CRIT("EIP Module INIT Failed, err: %d\n", err);
	if (eip_pub) {
		for (i = 0; i < MAX_NETDEV_CNT; i++) {
			if (eip_pub->ndev_list[i]) {
				if (eip_pub->ndev_list[i]->reg_state == NETREG_REGISTERED)
					unregister_netdev(eip_pub->ndev_list[i]);
				free_netdev(eip_pub->ndev_list[i]);
			}
		}
#ifdef CONFIG_EIP_IPSEC_OFFLOAD
		if (eip_pub->eip_ipsec) {
			kfree(eip_pub->eip_ipsec->tx_ipsec_handle);
			kfree(eip_pub->eip_ipsec->rx_ipsec_handle);
			kfree(eip_pub->eip_ipsec);
		}
#endif
		kfree(eip_pub);
		eip_pub = NULL;
	}

	eip_pub_glb = NULL;
	return err;
}

/**
 * @brief eip_netdev_exit - Driver Exit Cleanup Routine
 *
 */
static void eip_netdev_exit(void)
{
	struct eip_public *eip_pub = get_eip_public();
	struct eip_ring_interface *eip_ring;
	struct net_device *ndev;
	u8 i;

	if (!eip_pub)
		return;

	LOG_INFO("Unloading eip network module\n");

	/* Disable all macirq first before any netdev operations */
	shim_mac_disable_all_irq();

	/* stop upper-layers */
	for (i = 0; i < MAX_NETDEV_CNT; i++) {
		if (eip_pub->ndev_list[i]) {
			netif_device_detach(eip_pub->ndev_list[i]);
			netif_carrier_off(eip_pub->ndev_list[i]);
			netif_tx_stop_all_queues(eip_pub->ndev_list[i]);
		}
	}

	/* Wait for any pending network operations to complete */
	synchronize_net();

	/* Reset shim fifos before unregistering netdevs */
	shim_fifo_reset_all();

	for (i = 0; i < MAX_NETDEV_CNT; i++) {
		if (!eip_pub->ndev_list[i])
			continue;

		ndev = eip_pub->ndev_list[i];
		LOG_DEBG("removing %s", netdev_name(ndev));
		/* Check if interface was never properly initialized */
		if (ndev->reg_state != NETREG_REGISTERED) {
			free_netdev(ndev);
			continue;
		}
		unregister_netdev(ndev);

		if (ndev->phydev)
			eip_stop_phy(ndev);
		else
			eip_eth_link_updown(ndev, false);
	}

	/* Wait for any ongoing interrupt to complete */

	eip_pcl_dtl_uninit(eip_pub, DEFAULT_INTERFACE_ID, LOOKUP_TABLE_CNT);
	for (i = 0; i < RING_INTERFACE_CNT; i++) {
		eip_ring = &eip_pub->eip_ring[i];
		if (eip_ring->irq) {
			synchronize_irq(eip_ring->irq);
			disable_rdr_irq(i);
			free_irq(eip_ring->irq, &eip_ring->rid);
			//TODO: handle IPSEC_LOOKASIDE_INTERFACE
			if (i == REJECTED_FLOW_RULE_RING_ID) {
				tasklet_kill(&eip_ring->rdr_handler.tasklet_ring);
			} else {
				napi_disable(&eip_ring->rdr_handler.napi_ring);
				netif_napi_del(&eip_ring->rdr_handler.napi_ring);
			}
		}
		eip_pec_uninit(i);
		eip_delete_tx_buffers(eip_ring);
		eip_delete_rx_buffers(eip_ring);
	}
	eip_delete_tx_sa(eip_pub->sa_handle);
	kfree(eip_pub->sa_handle);

#ifdef CONFIG_EIP_IPSEC_OFFLOAD
	if (eip_pub->eip_ipsec) {
		kfree(eip_pub->eip_ipsec->rx_ipsec_handle);
		kfree(eip_pub->eip_ipsec->tx_ipsec_handle);
		kfree(eip_pub->eip_ipsec);
	}
#endif

	/* Free up eip_pub */
	kfree(eip_pub);
	eip_pub_glb = NULL;

	driver197_exit();

	LOG_INFO("EIP network module unloaded successfully\n");
}

/**
 * eip_subsystem_init - Initialize EIP subsystem
 * @hcp: HCP device structure
 *
 * Initializes the EIP-197 hardware, rings, crypto, and network devices.
 * The vendor DDK must already be initialized by hcp_main.c before calling this.
 *
 * Return: 0 on success, negative error code on failure
 */
int eip_subsystem_init(struct hcp_device *hcp)
{
	int err;

	if (eip_pub_glb) {
		dev_err(hcp->dev, "EIP already initialized\n");
		return -EBUSY;
	}

	/* Initialize EIP network devices and hardware */
	err = eip_netdev_init(hcp);
	if (err) {
		dev_err(hcp->dev, "EIP netdev init failed: %d\n", err);
		return err;
	}

	/* Save reference in HCP device */
	hcp->eip_priv = eip_pub_glb;

	dev_info(hcp->dev, "EIP subsystem initialized successfully\n");
	return 0;
}
EXPORT_SYMBOL_GPL(eip_subsystem_init);

/**
 * eip_subsystem_exit - Cleanup EIP subsystem
 * @hcp: HCP device structure
 */
void eip_subsystem_exit(struct hcp_device *hcp)
{
	if (!eip_pub_glb)
		return;

	eip_netdev_exit();
	dev_info(hcp->dev, "EIP subsystem cleaned up\n");
}
EXPORT_SYMBOL_GPL(eip_subsystem_exit);
