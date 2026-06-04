// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * shim_platform.c - SHIM subsystem initialization for HCP
 * (Header and Crypto Processing Engine).
 *
 * Copyright (c) 2022-2026 Axiado Corporation
 */

#include <linux/device.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_mdio.h>
#include <linux/of_net.h>
#include <linux/of_platform.h>
#include <linux/phy.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <asm/unaligned.h>

#include "mac_config.h"
#include "phy_interrupt.h"
#include "shim_common.h"
#include "shim_mac.h"
#include "shim_platform.h"
#include "hcp.h"

/* Definitions and macros */

/* rxaui init for 10g uses this offset (in PHY_CSR region) */
#define RXAUI_BASE 0x40000

/* Global struct-variable for shim - must be non-static for built-in visibility */
struct shim_mem_admin shim_admin;

/**
 * shim_read_shim_stats - function to read the shim stats
 * @shim_stat: (out) array to store the stat data
 * @mac_idx:  mac index
 */
void shim_read_shim_stats(u32 shim_stat[], int mac_idx)
{
	shim_stat[0] = shim_read_word(SHIM_RX_PKTS + (mac_idx * 4));
	shim_stat[1] = shim_read_word(SHIM_RX_GOOD_PKTS + (mac_idx * 4));
	shim_stat[2] = shim_read_word(SHIM_RX_BAD_PKTS + (mac_idx * 4));
}
EXPORT_SYMBOL_GPL(shim_read_shim_stats);

/**
 * shim_read_mac_rx_stats - to read the rx mac stats
 * @shim_mac_rx_stat: (out) array to store the rx mac stats data
 * @mac_idx: mac index
 */
void shim_read_mac_rx_stats(u32 shim_mac_rx_stat[], int mac_idx)
{
	u32 mac_base = MAC_BASE_OFFSET + (mac_idx * MAC_CONFIG_BYTE_CNT);

	shim_mac_rx_stat[0] = shim_read_word(mac_base + SHIM_MAC_RX_GOOD);
	shim_mac_rx_stat[1] = shim_read_word(mac_base + SHIM_MAC_RX_DROP);
	shim_mac_rx_stat[2] = shim_read_word(mac_base + SHIM_MAC_RX_UNDER_SZ_ERR);
	shim_mac_rx_stat[3] = shim_read_word(mac_base + SHIM_MAC_RX_TOTAL);
	shim_mac_rx_stat[4] = shim_read_word(mac_base + SHIM_MAC_RX_CRC_ERR);
	shim_mac_rx_stat[5] = shim_read_word(mac_base + SHIM_MAC_RX_IF_IN_ERR);
	shim_mac_rx_stat[6] = shim_read_word(mac_base + SHIM_MAC_RX_OVR_SZ_ERR);
	shim_mac_rx_stat[7] = shim_read_word(mac_base + SHIM_MAC_RX_JABBER_ERR);
	shim_mac_rx_stat[8] = shim_read_word(mac_base + SHIM_MAC_RX_FRAG_ERR);
}
EXPORT_SYMBOL_GPL(shim_read_mac_rx_stats);

/**
 * shim_read_mac_tx_stats - to read the tx mac stats
 * @shim_mac_tx_stat: (out) array to store the tx mac stats data
 * @mac_idx: mac index
 */
void shim_read_mac_tx_stats(u32 shim_mac_tx_stat[], int mac_idx)
{
	u32 mac_base = MAC_BASE_OFFSET + (mac_idx * MAC_CONFIG_BYTE_CNT);

	shim_mac_tx_stat[0] = shim_read_word(mac_base + SHIM_MAC_TX_TOTAL);
	shim_mac_tx_stat[1] = shim_read_word(mac_base + SHIM_MAC_TX_GOOD);
	shim_mac_tx_stat[2] = shim_read_word(mac_base + SHIM_MAC_TX_DROP);
	shim_mac_tx_stat[3] = shim_read_word(mac_base + SHIM_MAC_TX_CRC_ERR);
	shim_mac_tx_stat[4] = shim_read_word(mac_base + SHIM_MAC_TX_IF_OUT_ERR);
}
EXPORT_SYMBOL_GPL(shim_read_mac_tx_stats);

/**
 * shim_setup_resources - Setup memory resources from HCP device
 * @hcp: HCP device structure (already has mapped addresses)
 * @shim: Shim admin structure
 *
 * Uses memory regions already mapped by hcp_main.c probe function.
 * No longer does ioremap - just copies pointers from hcp structure.
 *
 * Return: 0 on success, negative error code on failure
 */
static int shim_setup_resources(struct hcp_device *hcp,
				struct shim_mem_admin *shim)
{
	struct device *dev = hcp->dev;

	dev_dbg(dev, "Setting up SHIM resources from HCP device\n");

	/* Use SHIM registers already mapped by HCP */
	if (!hcp->shim_base) {
		dev_err(dev, "SHIM base address not mapped by HCP\n");
		return -EINVAL;
	}
	shim->virt_base_addr = hcp->shim_base;
	shim->res_byte_cnt = hcp->shim_size;

	/* Use PHY CSR already mapped by HCP */
	if (!hcp->phy_csr_base) {
		dev_err(dev, "PHY CSR base address not mapped by HCP\n");
		return -EINVAL;
	}
	shim->phy_csr_base = hcp->phy_csr_base;

	return 0;
}

/**
 * shim_parse_mac_node - Parse a single MAC child node from Device Tree
 * @dev: Device pointer for logging
 * @child: Device tree node for this MAC
 * @mac_cfg: MAC configuration structure to populate
 * @mac_idx: MAC index (0-4)
 * @phy_mask: Bitmask of MACs with external PHYs
 *
 * Parses MAC properties:
 * - use-ncsi: bool from "use-ncsi"
 * - mdi-swap: bool from "swap-abcd"
 * - phy-mode: string property "phy-mode", handles "rxaui" and "rmii"
 * - phy-handle: presence recorded to drive MDIO phy_mask
 *
 * Return: 0 on success, negative on error
 */
static int shim_parse_mac_node(struct device *dev, struct device_node *child,
			       struct mac_phy *mac_cfg, u32 mac_idx,
			       u32 *phy_mask)
{
	const char *property;
	bool has_phy = false;

	/* Check if MAC is enabled in DT */
	if (!of_device_is_available(child)) {
		mac_cfg->enabled = false;
		dev_dbg(dev, "MAC-%d: disabled in DT\n", mac_idx);
		return 0;
	}

	mac_cfg->enabled = true;

	mac_cfg->app_id = mac_idx ? mac_idx : MAC_10G_APPID;

	/* Parse use-ncsi (optional) */
	mac_cfg->use_ncsi = of_property_read_bool(child, "use-ncsi");

	/* Parse mdi-swap */
	mac_cfg->mdi_swap = of_property_read_bool(child, "swap-abcd");

	/* Parse phy-mode */
	property = of_get_property(child, "phy-mode", NULL);
	mac_cfg->phy_mode = PHY_INTERFACE_MODE_SGMII; /* Default */

	if (property) {
		if (!strncmp(property, "rxaui", strlen("rxaui")))
			mac_cfg->phy_mode = PHY_INTERFACE_MODE_RXAUI;
		else if (!strncmp(property, "rmii", strlen("rmii")))
			mac_cfg->phy_mode = PHY_INTERFACE_MODE_RMII;
	}

	/* New code logic for phy-handle check (sets phy_mask later, but here logs) */
	property = of_get_property(child, "phy-handle", NULL);
	has_phy = !!property;
	if (has_phy && phy_mask)
		*phy_mask |= (1 << mac_idx);

	pr_warn("MAC-%u: use_ncsi: %u, swap-abcd: %u, app_id: %u, phy_mode: %u, phy: %s\n",
		mac_idx, mac_cfg->use_ncsi, mac_cfg->mdi_swap, mac_cfg->app_id,
		mac_cfg->phy_mode, has_phy ? "yes" : "no");

	return 0;
}

/**
 * shim_parse_mac_config - Parse all MAC child nodes from Device Tree
 * @hcp: HCP device structure
 * @shim: Shim admin structure
 * @phy_mask: Bitmask of MACs with external PHYs
 *
 * Iterates through MAC child nodes and populates shim->mac_cfg array
 *
 * Return: 0 on success, negative on error
 */
static int shim_parse_mac_config(struct hcp_device *hcp,
				 struct shim_mem_admin *shim, u32 *phy_mask)
{
	struct platform_device *pdev = hcp->pdev;
	struct device *dev = &pdev->dev;
	struct device_node *child;
	u32 mac_idx;
	int ret;

	dev_dbg(dev, "Parsing MAC configuration from Device Tree\n");

	for_each_child_of_node(dev->of_node, child) {
		/* Skip the MDIO node */
		if (of_node_name_eq(child, "mdio") ||
		    of_node_name_eq(child, "hfifo"))
			continue;

		/* Get MAC index from 'reg' property */
		ret = of_property_read_u32(child, "reg", &mac_idx);
		if (ret) {
			dev_warn(dev,
				 "MAC node missing 'reg' property, skipping\n");
			continue;
		}

		if (mac_idx >= MAX_MAC_CNT) {
			dev_warn(dev,
				 "Invalid MAC index %u (max %d), skipping\n",
				 mac_idx, MAX_MAC_CNT - 1);
			continue;
		}

		/* Parse this MAC's configuration */
		ret = shim_parse_mac_node(dev, child, &shim->mac_cfg[mac_idx],
					  mac_idx, phy_mask);
		if (ret) {
			dev_warn(dev, "Failed to parse MAC-%d config: %d\n",
				 mac_idx, ret);
			/* Continue parsing other MACs */
		}

	}

	return 0;
}

/**
 * shim_register_mac_interrupts - Register interrupt handlers for enabled MACs
 * @hcp: HCP device structure
 * @shim: Shim admin structure
 * @phy_mask: Bitmask of MACs that have a phy-handle in Device Tree
 *
 * Registers custom interrupt handlers for MACs that have IRQs configured
 * and an external PHY (phy-handle in DT). MACs without phy-handle (e.g.
 * DC-SCI interfaces) have no PHY to generate link-change interrupts, so
 * registering a handler would only cause spurious IRQ storms.
 *
 * Return: 0 on success, negative on error (non-fatal for individual MACs)
 */
static int shim_register_mac_interrupts(struct hcp_device *hcp,
					struct shim_mem_admin *shim,
					u32 phy_mask)
{
	struct device *dev = hcp->dev;
	struct hfifo_priv *hpriv = hcp->hfifo_priv;
	u32 mac_idx = hpriv->mac_idx;
	int ret;

	/* Format IRQ name */
	if (mac_idx == 0)
		snprintf(shim->mii_irq_name[mac_idx],
				SHIM_MAC_IRQ_NAME_LEN, "xgmii");
	else
		snprintf(shim->mii_irq_name[mac_idx],
				SHIM_MAC_IRQ_NAME_LEN, "gmii%d", mac_idx - 1);

	ret = devm_request_irq(dev, shim->mac_cfg[mac_idx].mac_irq,
			hfifo_phy_interrupt_handler, IRQF_SHARED,
			shim->mii_irq_name[mac_idx],
			hcp->hfifo_priv);
	if (ret) {
		dev_err(dev,
				"Failed to request IRQ %d for MAC-%d: %d\n",
				shim->mac_cfg[mac_idx].mac_irq, mac_idx, ret);
		return ret;
	}


	dev_info(dev, "Registered IRQ %d (%s) for MAC-%u\n",
			shim->mac_cfg[mac_idx].mac_irq,
			shim->mii_irq_name[mac_idx], mac_idx);

	return 0;
}

/**
 * shim_subsystem_init - Initialize SHIM subsystem
 * @hcp: HCP device structure
 *
 * Initializes the SHIM subsystem including:
 * 1. Setup memory regions (already mapped by HCP)
 * 2. Parse MAC configuration from DT
 * 3. Initialize hardware
 * 4. Register MDIO bus
 * 5. Set up MAC interrupts
 *
 * Return: 0 on success, negative error code on failure
 */
int shim_subsystem_init(struct hcp_device *hcp)
{
	struct shim_mem_admin *shim = &shim_admin;
	struct device *dev = hcp->dev;
	struct hfifo_priv *hpriv;
	u32 phy_mask = 0;
	int ret;

	if (!dev->of_node) {
		dev_err(dev, "Missing DT node\n");
		return -EINVAL;
	}

	/* Setup memory resources from HCP (already mapped) */
	ret = shim_setup_resources(hcp, shim);
	if (ret)
		return ret;

	/* Parse MAC configuration from Device Tree */
	ret = shim_parse_mac_config(hcp, shim, &phy_mask);
	if (ret) {
		dev_err(dev, "Failed to parse MAC configuration: %d\n", ret);
		return ret;
	}

	/* Store platform device reference */
	shim->pdev = hcp->pdev;

	/* Mark initialization as done before hardware access.
	 * Required by shim_mac_init() and other functions.
	 */
	shim->init_done = true;

	/* Initialize SHIM subsystem and MAC hardware */
	dev_info(dev, "Initializing SHIM subsystem\n");
	ret = shim_mac_init(dev);
	if (ret != SHIM_STATUS_SUCCESS) {
		dev_err(dev, "Hardware initialization failed\n");
		ret = -EIO;
		goto init_failed;
	}

	hpriv = hcp->hfifo_priv;
	if (!shim->mac_cfg[hpriv->mac_idx].enabled) {
		dev_err(dev, "HOST FIFO MAC-%d not enabled\n", hpriv->mac_idx);
		ret = -EINVAL;
		goto init_failed;
	}
	port_set_hfifo_mode(dev, hpriv->mac_idx);
	shim->mac_cfg[hpriv->mac_idx].mac_irq =
		hcp->mac_irqs[hpriv->mac_idx];

	/* Register MAC interrupt handlers only for MACs with phy-handle */
	ret = shim_register_mac_interrupts(hcp, shim, phy_mask);
	if (ret) {
		dev_warn(dev, "Some MAC interrupts failed to register\n");
		/* Non-fatal - MACs can fall back to polling */
	}

	/* Save references in HCP device */
	hcp->shim_priv = shim;

	dev_info(dev, "SHIM subsystem initialized successfully\n");
	return 0;

init_failed:
	/* All devm_ resources are automatically freed on failure.
	 * Just reset our initialization flags.
	 */
	shim->init_done = false;
	return ret;
}
EXPORT_SYMBOL_GPL(shim_subsystem_init);

/**
 * shim_subsystem_exit - Cleanup SHIM subsystem
 * @hcp: HCP device structure
 */
void shim_subsystem_exit(struct hcp_device *hcp)
{
	struct shim_mem_admin *shim = &shim_admin;

	if (!shim->pdev)
		return;

	//TODO call free_irq

	/* Disable all MAC interrupts at the HW level.
	 * synchronize_irq() is called inside shim_mac_disable_all_irq()
	 * to ensure no handlers are running.
	 */
	shim_mac_disable_all_irq();

	if (shim->mii)
		mdiobus_unregister(shim->mii);

	shim->init_done = false;

	/* All devm_ioremap resources are freed automatically by HCP driver.
	 * We just need to clear our global struct.
	 */
	memset(shim, 0, sizeof(*shim));
}
EXPORT_SYMBOL_GPL(shim_subsystem_exit);

/* Global Accessors */

/**
 * shim_is_init_done - get shim-platform driver initialisation status.
 *
 * Return: true if shim-driver is successfully initialised, else false.
 */
bool shim_is_init_done(void)
{
	return shim_admin.init_done;
}

/**
 * shim_get_virt_base_addr - Get virtual base address for shim memory
 *
 * Return: void __iomem * Virtual address(Base) of shim memory space.
 */
void __iomem *shim_get_virt_base_addr(void)
{
	return shim_admin.virt_base_addr;
}

/**
 * shim_get_mem_byte_cnt - get byte count of shim memory.
 *
 * Return: u32 size of shim memory in bytes
 */
u32 shim_get_mem_byte_cnt(void)
{
	return shim_admin.res_byte_cnt;
}

/**
 * is_shim_offset_valid - checks that the parameters are valid to make the access
 * of shim regs.
 * @byte_offset: shim register offset to be checked.
 *
 * Return: true if offset is valid, otherwise false.
 */
bool is_shim_offset_valid(const u32 byte_offset)
{
	if (!shim_is_init_done()) {
		pr_err_ratelimited("SHIM not initialized\n");
		return false;
	}
	if (byte_offset > shim_get_mem_byte_cnt()) {
		pr_err_ratelimited("Invalid SHIM offset: 0x%08x\n",
				   byte_offset);
		return false;
	}
	return true;
}

/**
 * shim_read_word() - Read from main SHIM register space
 * @offset: Byte offset from virt_base_addr
 * Return: 32-bit value read
 */
u32 shim_read_word(const u32 offset)
{
	if (WARN_ON_ONCE(!is_shim_offset_valid(offset)))
		return 0;
	return ioread32(shim_admin.virt_base_addr + offset);
}

/**
 * shim_write_word() - Write to main SHIM register space
 * @offset: Byte offset from virt_base_addr
 * @value_in: 32-bit value to write
 */
void shim_write_word(const u32 offset, const u32 value_in)
{
	if (WARN_ON_ONCE(!is_shim_offset_valid(offset)))
		return;
	iowrite32(value_in, shim_admin.virt_base_addr + offset);
}

/**
 * shim_read_phy_word() - Read from ETH_PHY CSR register space
 * @offset: Byte offset from phy_csr_base
 * Return: 32-bit value read
 */
u32 shim_read_phy_word(u32 offset)
{
	if (WARN_ON_ONCE(!shim_admin.phy_csr_base))
		return 0;
	return ioread32(shim_admin.phy_csr_base + offset);
}

/**
 * shim_write_phy_word() - Write to ETH_PHY CSR register space
 * @offset: Byte offset from phy_csr_base
 * @value_in: 32-bit value to write
 */
void shim_write_phy_word(u32 offset, u32 value_in)
{
	if (WARN_ON_ONCE(!shim_admin.phy_csr_base))
		return;
	iowrite32(value_in, shim_admin.phy_csr_base + offset);
}

/**
 * rxaui_write_phy_word - Write 32 bits at the offset in RXAUI configuration.
 * @offset: 32 bit byte offset in RXAUI config space
 * @value_in: value to be written at the offset.
 */
void rxaui_write_phy_word(u32 offset, u32 value_in)
{
	shim_write_phy_word(RXAUI_BASE + offset, value_in);
}

/**
 * rxaui_read_phy_word - Read 32 bits(4 Bytes) of RXAUI memory(regs).
 * @offset: Phy register offset to be read.
 *
 * Return: u32 value at the RXAUI config reg(offset).
 */
u32 rxaui_read_phy_word(u32 offset)
{
	return shim_read_phy_word(RXAUI_BASE + offset);
}

/**
 * hfifo_packet_tx - Xmit the buffer using Host FIFO
 * @buff: data buffer to transmit
 * @len: length of buffer
 * Return: 0 on success and < 0 on failure
 */
int hfifo_packet_tx(u8 *buf, u32 len)
{
	void __iomem *base = shim_get_virt_base_addr();
	void __iomem *fifo0 = base + TX_PACKET_FIFO_0;
	void __iomem *fifo1 = base + TX_PACKET_FIFO_1;
	void __iomem *wdata = base + TX_PACKET_FIFO_WDATA;
	u32 nwords = DIV_ROUND_UP(len, 4);
	u32 val, i;
	int ret;

	/* Reset TX FIFO before each packet (HW workaround for stale EOP) */
	iowrite32(ioread32(fifo0) | BIT(TX_FIFO_RST), fifo0);

	/* Wait for FIFO ready after reset */
	ret = readl_poll_timeout(fifo0, val, val & BIT(TX_FIFO_RDY), 1, 100);
	if (ret) {
		pr_err_ratelimited("Timedout for TX_FIFO_RDY\n");
		return ret;
	}

	/* Mark start of packet (SOP/EOP/MOD are the only fields in FIFO_1) */
	iowrite32(BIT(TX_FIFO_SOP), fifo1);

	/* Burst-write packet data to FIFO */
	for (i = 0; i < nwords; i++, buf += 4)
		iowrite32(get_unaligned((u32 *)buf), wdata);

	/* Mark end of packet with mod field (valid bytes in last word) */
	iowrite32(BIT(TX_FIFO_EOP) | ((len & 0x3) << TX_FIFO_MOD), fifo1);

	return 0;
}
EXPORT_SYMBOL_GPL(hfifo_packet_tx);

/**
 * hfifo_reset_rx - Reset the Rx FIFO Regs and MAC-port
 * @mac_idx:  mac index
 */
void hfifo_reset_rx(u8 mac_idx)
{
	void __iomem *fifo0 = shim_get_virt_base_addr() + RX_PACKET_FIFO_0;
	u32 val;
	u8 port;

	if (!mac_idx)
		port = 4;
	else
		port = mac_idx - 1;

	val = ioread32(fifo0);
	/* Clear stale port-select bits (6:4) before programming the new port */
	val &= ~GENMASK(6, 4);
	/* W1C: clear OVF/DAV monitor; W1TRG: trigger FIFO reset */
	val |= BIT(RX_FIFO_OVF_MON) | BIT(RX_FIFO_DAV_MON) | BIT(RX_FIFO_RST);
	val |= port << RX_FIFO_SEL;
	iowrite32(val, fifo0);
}
EXPORT_SYMBOL_GPL(hfifo_reset_rx);

/**
 * hfifo_rx_pkt_len - Check and get the available packet length
 * @mac_idx:  mac index
 * @Return: 0 on no packet, < 0 on buff overflow, > 0 - curr packet len
 */
int hfifo_rx_pkt_len(u8 mac_idx)
{
	void __iomem *base = shim_get_virt_base_addr();
	u32 val, len, mod;
	int ret = -1;

	val = ioread32(base + RX_PACKET_FIFO_0);
	if (likely(val & BIT(RX_FIFO_DAV))) {
		val = ioread32(base + RX_PACKET_FIFO_1);
		if (val) {
			len = (val >> RX_FIFO_FRMLEN) & GENMASK(15, 0);
			mod = (val >> RX_FIFO_MOD) & GENMASK(1, 0);
			ret = len * 4 - ((4 - mod) % 4);
		}
	} else {
		if (val & (BIT(RX_FIFO_OVF) | BIT(RX_FIFO_OVF_MON) |
			   BIT(RX_FIFO_FULL)))
			ret = -1;
		else
			ret = 0;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(hfifo_rx_pkt_len);

/**
 * hfifo_packet_rx - Dequeue a single packet from FIFO
 * @buf: buffer to store the dequeued data
 * @buf_len:  lenght of data buffer
 * @mac_idx:  mac index
 * @Return: buffere length dequeued
 */
int hfifo_packet_rx(u8 *buf, u32 buf_len, u8 mac_idx)
{
	void __iomem *base = shim_get_virt_base_addr();
	void __iomem *rdata = base + RX_PACKET_FIFO_RDATA;
	u32 nwords = DIV_ROUND_UP(buf_len, 4);
	u32 i;

	for (i = 0; i < nwords; i++, buf += 4)
		put_unaligned(ioread32(rdata), (u32 *)buf);

	/* dummy read RX_FIFO_FRMSTAT for MAC RX Frame Status */
	ioread32(base + RX_PACKET_FIFO_2);
	/* advance FIFO to next start-of-packet */
	iowrite32(BIT(RX_FIFO_EOP), base + RX_PACKET_FIFO_1);

	return buf_len;
}
EXPORT_SYMBOL_GPL(hfifo_packet_rx);
