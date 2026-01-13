// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Phy_interrupt.c - Interrupt handling routines for the Axiado SHIM driver.
 * Manages MAC/PHY interrupts, enabling/disabling IRQs, and acknowledging status.
 *
 * Copyright (c) 2022-2026 Axiado Corporation
 */

#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/phy.h>

#include "mac_config.h"
#include "phy_interrupt.h"
#include "shim_eip_common.h"
#include "shim_mac.h"
#include "shim_platform.h"

static struct device *shim_get_dev(void)
{
	return shim_admin.pdev ? &shim_admin.pdev->dev : NULL;
}

static bool shim_mac_idx_valid(int mac_idx)
{
	return mac_idx >= 0 && mac_idx < MAX_MAC_CNT;
}

/**
 * shim_irq_set_csr_val - Common function to enable/disable MAC interrupts
 * @mac_idx: MAC index
 * @enable: true to enable, false to disable
 */
static void shim_irq_set_csr_val(int mac_idx, bool enable)
{
	u32 csr_addr;
	u32 val;

	if (!shim_get_dev() || !shim_mac_idx_valid(mac_idx))
		return;

	csr_addr = SHIM_MAC_IRQ_CSR + (mac_idx * SHIM_MAC_REG_ADDR_SIZE);
	val = shim_read_word(csr_addr);

	if (enable)
		val |= (CSR_REG_BIT_MACPHY_IRQ_EN | CSR_REG_BIT_MAC_IRQ_EN);
	else
		val &= ~(CSR_REG_BIT_MACPHY_IRQ_EN | CSR_REG_BIT_MAC_IRQ_EN);

	shim_write_word(csr_addr, val);
}

/**
 * shim_irq_enable - Enables IRQ for the given mac
 * @mac_idx: MAC index
 *
 * This function enables the interrupt in the hardware CSR.
 * It also clears any stale W1C (Write-1-to-Clear) status bits
 * in the Monitor and CSR registers before enabling to prevent
 * spurious interrupts immediately upon enable.
 */
void shim_irq_enable(int mac_idx)
{
	struct device *dev = shim_get_dev();
	u32 mon_addr, csr_addr, val;

	if (!dev) {
		pr_err("shim_irq_enable: SHIM device not initialized\n");
		return;
	}

	if (!shim_mac_idx_valid(mac_idx)) {
		dev_err(dev, "shim_irq_enable: invalid mac_idx %d\n", mac_idx);
		return;
	}

	mon_addr = SHIM_MAC_MON + (mac_idx * SHIM_MAC_REG_ADDR_SIZE);
	csr_addr = SHIM_MAC_IRQ_CSR + (mac_idx * SHIM_MAC_REG_ADDR_SIZE);

	/* 1: Clear W1C status bits in the Monitor Register (SHIM_MAC_MON).
	 * Writing '1' clears the flag. Only write the bits we want to clear.
	 */
	shim_write_word(mon_addr, MON_REG_BIT_AUTO_NEG_MON |
				  MON_REG_BIT_SYNC_STATUS_MON);

	/* 2: Clear W1C status bits in the CSR Register AND enable interrupts.
	 */
	val = shim_read_word(csr_addr);

	/* Clear status bits (W1C) */
	val |= (CSR_REG_BIT_MACPHY_MON | CSR_REG_BIT_MAC_MON);

	/* Set enable bits */
	val |= (CSR_REG_BIT_MACPHY_IRQ_EN | CSR_REG_BIT_MAC_IRQ_EN);

	shim_write_word(csr_addr, val);

	dev_dbg(dev, "MAC-%d IRQ enabled and status cleared\n", mac_idx);
}
EXPORT_SYMBOL_GPL(shim_irq_enable);

/**
 * shim_irq_disable - Disables IRQ for the given mac
 * @mac_idx: MAC index
 */
void shim_irq_disable(int mac_idx)
{
	struct device *dev = shim_get_dev();

	if (!dev) {
		pr_err("shim_irq_disable: SHIM device not initialized\n");
		return;
	}

	if (!shim_mac_idx_valid(mac_idx)) {
		dev_err(dev, "shim_irq_disable: invalid mac_idx %d\n", mac_idx);
		return;
	}

	shim_irq_set_csr_val(mac_idx, false);
	dev_dbg(dev, "MAC-%d IRQ disabled\n", mac_idx);
}
EXPORT_SYMBOL_GPL(shim_irq_disable);

/**
 * shim_mac_disable_all_irq - Disables all registered mac's IRQs
 *
 * This is typically called during driver unload or suspend.
 * It uses synchronize_irq() to ensure no handlers are running.
 */
void shim_mac_disable_all_irq(void)
{
	struct device *dev = shim_get_dev();
	struct mac_phy *mac_cfg;
	int i;

	if (!dev) {
		pr_err("shim_mac_disable_all_irq: SHIM device not initialized\n");
		return;
	}

	mac_cfg = shim_admin.mac_cfg;

	for (i = 0; i < MAX_MAC_CNT; i++) {
		/* Only disable if an IRQ was actually assigned */
		if (mac_cfg[i].mac_irq > 0 && mac_cfg[i].mac_irq != PHY_POLL) {
			shim_irq_set_csr_val(i, false);
			synchronize_irq(mac_cfg[i].mac_irq);
			dev_dbg(dev, "MAC-%d IRQ disabled and synchronized\n",
				i);
		}
	}
}
EXPORT_SYMBOL_GPL(shim_mac_disable_all_irq);

/**
 * shim_mac_enable_all_irq - Enables all mac's IRQs, where appropriate.
 */
void shim_mac_enable_all_irq(void)
{
	struct device *dev = shim_get_dev();
	struct mac_phy *mac_cfg;
	int i;

	if (!dev) {
		pr_err("shim_mac_enable_all_irq: SHIM device not initialized\n");
		return;
	}

	mac_cfg = shim_admin.mac_cfg;

	for (i = 0; i < MAX_MAC_CNT; i++) {
		if (mac_cfg[i].mac_irq > 0 && mac_cfg[i].mac_irq != PHY_POLL) {
			shim_irq_set_csr_val(i, true);
			dev_dbg(dev, "MAC-%d IRQ enabled\n", i);
		}
	}
}
EXPORT_SYMBOL_GPL(shim_mac_enable_all_irq);

/**
 * shim_irq_ack - Acknowledges interrupt & check for interested bits.
 * @mac_id: MAC index
 *
 * Return: 1 if spurious (no interested bits set), 0 otherwise.
 */
static int shim_irq_ack(int mac_id)
{
	u32 val, csr_addr, mon_addr;

	if (!shim_get_dev() || !shim_mac_idx_valid(mac_id))
		return 1;

	csr_addr = SHIM_MAC_IRQ_CSR + (mac_id * SHIM_MAC_REG_ADDR_SIZE);
	mon_addr = SHIM_MAC_MON + (mac_id * SHIM_MAC_REG_ADDR_SIZE);

	/* Read monitor reg to check status */
	val = shim_read_word(mon_addr);

	if ((val & MON_REG_BIT_SYNC_STATUS_MON) ||
	    (val & MON_REG_BIT_AUTO_NEG_MON)) {
		/* Clear W1C bits in Monitor Reg */
		shim_write_word(mon_addr, MON_REG_BIT_AUTO_NEG_MON |
					  MON_REG_BIT_SYNC_STATUS_MON);

		/* Clear W1C bits in CSR reg */
		shim_write_word(csr_addr, CSR_REG_BIT_MACPHY_MON |
					  CSR_REG_BIT_MAC_MON);
	} else {
		/* Spurious or unrelated interrupt */
		return 1;
	}

	return 0;
}

/**
 * shim_phy_interrupt_handler - IRQ handler for given phy
 * @irq: IRQ number
 * @data: Pointer to mac_phy structure
 *
 * Handle the interrupt by acknowledging the SHIM wrapper and notifying phylib.
 *
 * Return: IRQ_HANDLED or IRQ_NONE
 */
irqreturn_t shim_phy_interrupt_handler(int irq, void *data)
{
	struct mac_phy *mac_cfg = (struct mac_phy *)data;
	int mac_id;
	struct device *dev = shim_get_dev();

	if (!data || !dev)
		return IRQ_NONE;

	mac_id = (mac_cfg->app_id == MAC_10G_APPID) ? 0 : mac_cfg->app_id;

	if (!shim_mac_idx_valid(mac_id)) {
		dev_err_ratelimited(dev, "IRQ %d: Invalid MAC index %d\n",
				    irq, mac_id);
		return IRQ_NONE;
	}

	if (!mac_cfg->phydev || !mac_cfg->enabled) {
		int spurious = shim_irq_ack(mac_id);

		dev_warn_once(dev, "IRQ %d: unexpected on MAC-%d (no PHY), disabling\n",
			      irq, mac_id);
		/* Always disable — this MAC should never fire IRQs.
		 * Return based on whether we saw our status bits (shared IRQ).
		 */
		shim_irq_disable(mac_id);
		return spurious ? IRQ_NONE : IRQ_HANDLED;
	}

	/* Ack the interrupt in the SHIM hardware */
	if (shim_irq_ack(mac_id))
		return IRQ_HANDLED; /* Spurious, but we handled the ACK attempt */

	/* Notify the Generic PHY Library */
	if (mac_cfg->phydev) {
		/* Disable SHIM IRQ briefly to prevent re-entrancy/storm if the
		 * PHY line remains asserted while work is scheduled.
		 */
		shim_irq_disable(mac_id);
		phy_mac_interrupt(mac_cfg->phydev);
		shim_irq_enable(mac_id);
	} else {
		dev_warn_ratelimited(dev,
				     "IRQ %d: No PHY device attached to MAC-%d\n",
				     irq, mac_id);
		return IRQ_NONE;
	}

	return IRQ_HANDLED;
}
