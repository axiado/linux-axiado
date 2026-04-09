// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * mac_config.c - MAC initialization and configuration routines for the Axiado SHIM driver.
 * Handles 10G/1G MAC setup, RXAUI/SGMII PHY initialization, and FIFO resets.
 *
 * Copyright (C) 2022-2026 Axiado Corporation
 */

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/types.h>

#include "mac_config.h"
#include "shim_eip_common.h"
#include "shim_mac.h"
#include "shim_platform.h"

/* RXAUI Timeout Configuration */
#define RXAUI_POLL_TIMEOUT_US 600000 /* 600ms total timeout */
#define RXAUI_POLL_INTERVAL_US 2000 /* 2ms sleep interval */

/* RXAUI register offsets */
#define RXAUI_REG_EEE_CTRL 0x0e4c
#define RXAUI_REG_MODE_CTRL 0x0e00
#define RXAUI_REG_STATUS 0x0fd8

/* RXAUI configuration values */
#define RXAUI_EEE_CTRL_VALUE 0x000000f8
#define RXAUI_MODE_DUNE_VALUE 0x00000000
#define RXAUI_MODE_DUNE_EXPECTED 0x00000003

/**
 * mac_is_enabled - Check if a specific MAC is enabled and verify hardware revision.
 * @mac_idx: MAC index to check (0-4).
 *
 * Return: true if enabled and hardware revision matches, false otherwise.
 */
bool mac_is_enabled(int mac_idx)
{
	u32 reg_rev, exp_rev;
	u32 mac_base;
	struct mac_phy *mac_cfg;
	struct device *dev = &shim_admin.pdev->dev;

	if (!dev || mac_idx < 0 || mac_idx >= MAX_MAC_CNT)
		return false;

	mac_cfg = &shim_admin.mac_cfg[mac_idx];
	if (!mac_cfg->enabled)
		return false;

	/* Determine expected revision based on MAC type */
	if (mac_idx == 0)
		exp_rev = MAC_XG_REVISION_VAL; /* XGMII MAC */
	else
		exp_rev = MAC_1G_REVISION_VAL; /* GMII MACs */

	mac_base = MAC_BASE_OFFSET + (mac_idx * MAC_CONFIG_BYTE_CNT);
	reg_rev = shim_read_word(mac_base + R_REVISION);

	if (reg_rev != exp_rev) {
		dev_warn(dev,
			 "MAC-%d: Revision mismatch (Read: 0x%x, Exp: 0x%x)\n",
			 mac_idx, reg_rev, exp_rev);
		return false;
	}

	return true;
}

/**
 * rxaui_init_10g - Initialize RXAUI for the 10G PHY.
 * @dev: Device structure for logging.
 *
 * Configures EEE control and sets the PHY to Dune mode.
 * Waits for the mode to be established.
 */
void rxaui_init_10g(struct device *dev)
{
	int ret;
	u32 val;

	dev_dbg(dev, "Initializing RXAUI for 10G\n");

	/* EEE Support (LPIDLE) Enable, RX FIFO auto clear enable, active HIGH */
	rxaui_write_phy_word(RXAUI_REG_EEE_CTRL, RXAUI_EEE_CTRL_VALUE);

	/* Set Dune Mode */
	dev_dbg(dev, "Setting DUNE mode\n");
	udelay(PHY_INIT_DELAY_US);
	rxaui_write_phy_word(RXAUI_REG_MODE_CTRL, RXAUI_MODE_DUNE_VALUE);

	/* Poll for expected status.
	 * Using readx_poll_timeout allows to sleep between checks
	 * instead of the original code's udelay() busy-wait loop.
	 */
	ret = readx_poll_timeout(rxaui_read_phy_word, RXAUI_REG_STATUS, val,
				 (val == RXAUI_MODE_DUNE_EXPECTED),
				 RXAUI_POLL_INTERVAL_US, RXAUI_POLL_TIMEOUT_US);

	if (ret) {
		dev_err(dev, "Failed to set DUNE Mode (Status: 0x%x)\n", val);
		return;
	}

	dev_info(dev, "RXAUI 10G Configured (DUNE Mode Set)\n");
	msleep(PLATFORM_INIT_DELAY_MS / 10); /* Allow stabilization */
}

/**
 * mac_init_10g - Configure 10G MAC registers.
 * @dev: Device structure.
 * @mac_idx: MAC index (Always 0 for 10G).
 *
 * Return: SHIM_STATUS_SUCCESS on success.
 */
static enum AX_SHIM_STATUS mac_init_10g(struct device *dev, int mac_idx)
{
	u32 cmd_cfg;
	u32 mac_base = MAC_BASE_OFFSET + (mac_idx * MAC_CONFIG_BYTE_CNT);

	dev_dbg(dev, "Configuring 10G MAC at base 0x%x\n", mac_base);

	/* Configure FIFO Sections for Cut-through */
	shim_write_word(mac_base + R_RX_FIFO_SECTIONS, XG_MAC_R_RX_FIFO_VAL);
	shim_write_word(mac_base + R_TX_FIFO_SECTIONS, XG_MAC_R_TX_FIFO_VAL);

	/* Configure MAC Command Config Enable TX, RX, Padding */
	shim_write_word(mac_base + R_COMMAND_CONFIG,
			MAC_CC_NULL | MAC_CC_TX_ENA | MAC_CC_RX_ENA |
				MAC_CC_TX_PAD_EN);

	/* Set Max Frame Length */
	shim_write_word(mac_base + R_FRM_LENGTH, XG_MAC_R_FRM_LENGTH_VAL);

	/* Set Interface Mode (XGMII) */
	shim_write_word(mac_base + R_IF_MODE, XG_MAC_R_IF_MODE_VAL);

	/* Verify Configuration */
	cmd_cfg = shim_read_word(mac_base + R_COMMAND_CONFIG);
	dev_info(dev, "10G MAC Configured (Cmd: 0x%08x)\n", cmd_cfg);

	return SHIM_STATUS_SUCCESS;
}

/**
 * sgmii_fast_sim - Hardware specific fixups for 1G MACs.
 * @dev: Device structure.
 * @mac_idx: MAC index (1-4).
 *
 * Applies undocumented register writes required for SGMII signal integrity
 * and stabilization on specific hardware revisions.
 */
static void sgmii_fast_sim(struct device *dev, int mac_idx)
{
	u32 sgmii_base = SGMII_BASE + ((mac_idx - 1) * SGMII_CONFIG_RANGE);

	if (mac_idx < 1 || mac_idx >= MAX_MAC_CNT)
		return;

	/* SGMII PHY tuning sequence required for link stability on this platform. */
	shim_write_phy_word(sgmii_base + 0x0dd4, 0x00);
	shim_write_phy_word(sgmii_base + 0x070c, 0x00800000);
	shim_write_phy_word(sgmii_base + 0x0710, 0x00a00090);
	shim_write_phy_word(sgmii_base + 0x470c, 0x00800000);
	shim_write_phy_word(sgmii_base + 0x4710, 0x00a00090);
	shim_write_phy_word(sgmii_base + 0x0b24, 0x00000061);
	shim_write_phy_word(sgmii_base + 0x0b28, 0xffffff00);
	shim_write_phy_word(sgmii_base + 0x4b24, 0x00000061);
	shim_write_phy_word(sgmii_base + 0x4b28, 0xffffff00);
	shim_write_phy_word(sgmii_base + 0x0b88, 0x00000100);
	shim_write_phy_word(sgmii_base + 0x4b88, 0x00000100);
	shim_write_phy_word(sgmii_base + 0x0b8c, 0x00000000);
	shim_write_phy_word(sgmii_base + 0x4b8c, 0x00000000);
	shim_write_phy_word(sgmii_base + 0x0b6c, 0x00000000);
	shim_write_phy_word(sgmii_base + 0x0b70, 0x00000001);
	shim_write_phy_word(sgmii_base + 0x0b74, 0x40000102);
	shim_write_phy_word(sgmii_base + 0x0664, 0xb8000380);
	shim_write_phy_word(sgmii_base + 0x4b6c, 0x00000000);
	shim_write_phy_word(sgmii_base + 0x4b70, 0x00000001);
	shim_write_phy_word(sgmii_base + 0x4b74, 0x40000102);
	shim_write_phy_word(sgmii_base + 0x4664, 0xb8000380);
	shim_write_phy_word(sgmii_base + 0x1540, 0xfffefffe);
	shim_write_phy_word(sgmii_base + 0x1544, 0xfffefffe);
	shim_write_phy_word(sgmii_base + 0x1548, 0xfffefffe);
	shim_write_phy_word(sgmii_base + 0x154c, 0xfffefffe);
	shim_write_phy_word(sgmii_base + 0x1550, 0xfffefffe);
	shim_write_phy_word(sgmii_base + 0x1554, 0xfffefffe);
	shim_write_phy_word(sgmii_base + 0x1558, 0xfffefffe);
	shim_write_phy_word(sgmii_base + 0x155c, 0xfffefffe);
	shim_write_phy_word(sgmii_base + 0x1560, 0xfffefffe);
	shim_write_phy_word(sgmii_base + 0x1564, 0xfffefffe);
	shim_write_phy_word(sgmii_base + 0x1568, 0xfffefffe);
	shim_write_phy_word(sgmii_base + 0x156c, 0xfffefffe);
	shim_write_phy_word(sgmii_base + 0x1570, 0xfffefffe);
	shim_write_phy_word(sgmii_base + 0x1574, 0x0000fffe);

	udelay(1); /* Wait for reset propagation */

	shim_write_phy_word(sgmii_base + 0x1954, 0x00);
	shim_write_phy_word(sgmii_base + 0x1958, 0x00);
	shim_write_phy_word(sgmii_base + 0x195c, 0x7d);
	shim_write_phy_word(sgmii_base + 0x1800, 0x000d);
}

/**
 * mac_init_1g - Configure 1G MAC registers.
 * @dev: Device structure.
 * @mac_idx: MAC index (1-4).
 *
 * Return: SHIM_STATUS_SUCCESS on success.
 */
static enum AX_SHIM_STATUS mac_init_1g(struct device *dev, int mac_idx)
{
	u32 mac_base = MAC_BASE_OFFSET + (mac_idx * MAC_CONFIG_BYTE_CNT);
	u32 cmd_cfg;

	dev_dbg(dev, "Configuring 1G MAC-%d at base 0x%x\n", mac_idx, mac_base);

	/* Configure FIFO Sections */
	shim_write_word(mac_base + R_RX_FIFO_SECTIONS, MAC_1G_R_RX_FIFO_VAL);
	shim_write_word(mac_base + R_TX_FIFO_SECTIONS, MAC_1G_R_TX_FIFO_VAL);

	/* Configure MAC Command Config,Enable TX, RX, Padding */
	shim_write_word(mac_base + R_COMMAND_CONFIG,
			MAC_CC_NULL | MAC_CC_TX_ENA | MAC_CC_RX_ENA |
				MAC_CC_TX_PAD_EN);

	/* Set Max Frame Length */
	shim_write_word(mac_base + R_FRM_LENGTH, MAC_1G_R_FRM_LENGTH_VAL);

	/* Set Interface Mode (GMII) */
	shim_write_word(mac_base + R_IF_MODE, MAC_1G_R_IF_MODE_VAL);

	/* Apply hardware fixups */
	sgmii_fast_sim(dev, mac_idx);

	/* Verify Configuration */
	cmd_cfg = shim_read_word(mac_base + R_COMMAND_CONFIG);
	dev_info(dev, "1G MAC-%d Configured (Cmd: 0x%08x)\n", mac_idx, cmd_cfg);

	return SHIM_STATUS_SUCCESS;
}

/**
 * mac_basic_init - Initialize all enabled MACs.
 * @dev: Device structure.
 *
 * Calls initialization routines for 10G and 1G MACs based on configuration.
 *
 * Return: SHIM_STATUS_SUCCESS on success, error code otherwise.
 */
enum AX_SHIM_STATUS mac_basic_init(struct device *dev)
{
	enum AX_SHIM_STATUS ret;
	int mac_idx;

	/* Configuration for MACs (Iterate 0 to 4) */
	for (mac_idx = 0; mac_idx < MAX_MAC_CNT; mac_idx++) {
		/* Check if the mac is enabled */
		if (mac_is_enabled(mac_idx)) {
			if (!mac_idx) {
				/* 10G MAC (Index 0) */
				rxaui_init_10g(dev);
				ret = mac_init_10g(dev, mac_idx);
			} else {
				/* 1G MACs (Indices 1-4) */
				ret = mac_init_1g(dev, mac_idx);
			}

			if (ret != SHIM_STATUS_SUCCESS) {
				dev_err(dev,
					"mac_init for mac_idx(%d) failed. ret_status: %d\n",
					mac_idx, ret);
				return ret;
			}
		}
	}

	return SHIM_STATUS_SUCCESS;
}

/**
 * mac_flow_config - Clear MAC flow control registers to enable normal operation.
 *
 * The hardware default for these registers is 0x10. Writing 0x0 is required
 * to disable loopback and enable normal packet flow.
 *
 * Return: SHIM_STATUS_SUCCESS.
 */
enum AX_SHIM_STATUS mac_flow_config(void)
{
	static const u32 flow_vals[MAX_MAC_CNT] = {
		MAC_XG_FLOW_VAL,
		MAC_1G_0_FLOW_VAL,
		MAC_1G_1_FLOW_VAL,
		MAC_1G_2_FLOW_VAL,
		MAC_1G_3_FLOW_VAL,
	};
	int i;

	for (i = 0; i < MAX_MAC_CNT; i++) {
		if (mac_is_enabled(i))
			shim_write_word(i * MAC_FLOW_CONFIG_BYTE_CNT, flow_vals[i]);
	}

	return SHIM_STATUS_SUCCESS;
}

/**
 * get_mac_base - Get base address for a MAC.
 * @app_id: Application ID (MAC_10G_APPID for 10G, 1-4 for 1G MACs).
 *
 * Maps application ID to hardware MAC index and returns the register base.
 * 10G MAC (app_id=5) maps to index 0, 1G MACs use app_id directly.
 *
 * Return: MAC register base offset.
 */
static u32 get_mac_base(int app_id)
{
	int idx = (app_id == MAC_10G_APPID) ? 0 : app_id;

	return MAC_BASE_OFFSET + (idx * MAC_CONFIG_BYTE_CNT);
}

/**
 * mac_phy_addr_rd - Read the MAC address from hardware registers.
 * @app_id: Application ID (Logic mapping).
 * @mac_0: Pointer for lower 32 bits of MAC address.
 * @mac_1: Pointer for upper 16 bits of MAC address.
 *
 * Return: true.
 */
bool mac_phy_addr_rd(int app_id, u32 *mac_0, u32 *mac_1)
{
	u32 mac_base = get_mac_base(app_id);

	*mac_0 = shim_read_word(mac_base + R_MAC_0);
	*mac_1 = shim_read_word(mac_base + R_MAC_1);

	return true;
}
EXPORT_SYMBOL_GPL(mac_phy_addr_rd);

/**
 * mac_phy_addr_wr - Configure the MAC address in hardware registers.
 * @app_id: Application ID (Logic mapping).
 * @mac_0: Lower 32 bits of MAC address.
 * @mac_1: Upper 16 bits of MAC address.
 *
 * Return: true.
 */
bool mac_phy_addr_wr(int app_id, u32 mac_0, u32 mac_1)
{
	u32 mac_base = get_mac_base(app_id);

	shim_write_word(mac_base + R_MAC_0, mac_0);
	shim_write_word(mac_base + R_MAC_1, mac_1);

	return true;
}
EXPORT_SYMBOL_GPL(mac_phy_addr_wr);

/**
 * mac_update_promisc - Toggle promiscuous mode.
 * @app_id: Application ID.
 * @promisc: true to enable, false to disable.
 */
void mac_update_promisc(int app_id, bool promisc)
{
	u32 cmd_cfg;
	u32 mac_base = get_mac_base(app_id);

	cmd_cfg = shim_read_word(mac_base + R_COMMAND_CONFIG);

	if (!!(cmd_cfg & MAC_CC_PROMIS_EN) != promisc) {
		cmd_cfg ^= MAC_CC_PROMIS_EN;
		shim_write_word(mac_base + R_COMMAND_CONFIG, cmd_cfg);
	}
}
EXPORT_SYMBOL_GPL(mac_update_promisc);

/**
 * is_rx_local_fault - Check if MAC has RX local fault.
 * @mac_idx: MAC index (0-4).
 *
 * Return: true if RX local fault is set, false otherwise.
 */
bool is_rx_local_fault(u8 mac_idx)
{
	u32 mac_base;
	u32 mac_status;

	if (mac_idx >= MAX_MAC_CNT)
		return false;

	mac_base = MAC_BASE_OFFSET + (mac_idx * MAC_CONFIG_BYTE_CNT);
	mac_status = shim_read_word(mac_base + R_STATUS);

	return !!(mac_status & BIT(0));
}
EXPORT_SYMBOL(is_rx_local_fault);

/**
 * shim_fifo_reset - Reset the SHIM FIFO for a specific MAC.
 * @mac_idx: MAC index.
 *
 * Return: true on success, false on timeout.
 */
bool shim_fifo_reset(u8 mac_idx)
{
	u32 val;
	int ret;
	u32 reg_offset = REG_XGE_SHIM_FIFO + (mac_idx * 4);
	struct device *dev = &shim_admin.pdev->dev;

	if (!dev || mac_idx >= MAX_MAC_CNT)
		return false;

	/* Poll condition:
	 * Wait until (TX_EMPTY && RX_EMPTY) AND (No TX Overflow && No RX Overflow).
	 * If condition fails, perform reset.
	 */
	ret = readx_poll_timeout(shim_read_word, reg_offset, val,
				 ((val & BIT(BIT_TX_EMPTY)) &&
				  (val & BIT(BIT_RX_EMPTY)) &&
				  !(val & BIT(SHIM_MAC_TX_OVFLOW_BIT)) &&
				  !(val & BIT(SHIM_MAC_RX_OVFLOW_BIT))),
				 SHIM_RESET_RETRY_DELAY,
				 SHIM_RESET_RETRY_DELAY * SHIM_RESET_RETRY_MAX);

	if (ret) {
		/* Timeout occurred, try forcing a reset */
		val = shim_read_word(reg_offset);
		val |= (BIT(BIT_TX_RST) | BIT(BIT_RX_RST));
		shim_write_word(reg_offset, val);
		udelay(SHIM_RESET_RETRY_DELAY);

		/* Re-read to check if reset cleared the bits (optional check) */
		val = shim_read_word(reg_offset);
		if (!((val & BIT(BIT_TX_EMPTY)) && (val & BIT(BIT_RX_EMPTY)))) {
			dev_warn(dev, "MAC-%d: FIFO reset failed (Val: 0x%x)\n",
				 mac_idx, val);
			return false;
		}
	}

	return true;
}
EXPORT_SYMBOL_GPL(shim_fifo_reset);

/**
 * shim_fifo_reset_all - Reset FIFOs for all enabled MACs.
 */
void shim_fifo_reset_all(void)
{
	int i;

	for (i = 0; i < MAX_MAC_CNT; i++) {
		if (shim_admin.mac_cfg[i].enabled)
			shim_fifo_reset(i);
	}
}
EXPORT_SYMBOL_GPL(shim_fifo_reset_all);

/**
 * shim_mac_disable - Disable TX/RX for a specific MAC.
 * @mac_idx: MAC index (0-4).
 */
void shim_mac_disable(int mac_idx)
{
	u32 mac_base;

	if (mac_idx < 0 || mac_idx >= MAX_MAC_CNT)
		return;

	mac_base = MAC_BASE_OFFSET + (mac_idx * MAC_CONFIG_BYTE_CNT);
	shim_write_word(mac_base + R_COMMAND_CONFIG, MAC_CC_NULL);
	shim_admin.mac_cfg[mac_idx].enabled = false;
}
EXPORT_SYMBOL_GPL(shim_mac_disable);
