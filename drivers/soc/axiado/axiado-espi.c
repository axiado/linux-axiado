// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Axiado eSPI Slave Controller Driver
 *
 * Copyright (c) 2025 Axiado Corporation
 */

#include <linux/delay.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

#include "axiado-espi.h"

#define DRIVER_NAME "axiado-espi"

static const struct of_device_id ax_espi_of_match[];

/* Configuration defaults - can be overridden via device tree */
#define ESPI_DEFAULT_FIFO_FLUSH_TIMEOUT_MS	100
#define ESPI_DEFAULT_POLL_DELAY_MIN_US		10
#define ESPI_DEFAULT_POLL_DELAY_MAX_US		20
#define ESPI_DEFAULT_MAX_RETRIES		3

/* Register offset validation */
#define ESPI_CORE_REG_MAX			0x1000
#define ESPI_VALID_REG_OFFSET(offset)		((offset) <= ESPI_CORE_REG_MAX)

/* Error recovery levels */
enum espi_recovery_level {
	ESPI_RECOVERY_NONE,
	ESPI_RECOVERY_FIFO_FLUSH,
	ESPI_RECOVERY_CHANNEL_RESET,
	ESPI_RECOVERY_CONTROLLER_RESET
};

/* Hardware variant configurations */
static const struct espi_hw_config espi_mmio_config = {
	.ext_atu_base = 0x2000,
	.hw_type = ESPI_HW_TYPE_MMIO,
};

static const struct espi_hw_config espi_mfd_config = {
	.ext_atu_base = 0xE000,
	.hw_type = ESPI_HW_TYPE_MFD,
};

/*
 * Virtual Wire Response Table
 *
 * This table contains hardcoded virtual wire responses for common host
 * requests. The format follows eSPI virtual wire protocol where each
 * entry contains:
 * - cmd: Virtual wire command/index received from host
 * - response: Array of response data to send back
 * - count: Number of response entries
 *
 *  Virtual Wire Packet Format (16-bit):
 *  Bits [15:8] - Data
 *  Bits [7:0]  - Index
 *
 * Example: 0x1102
 *   - Index: 0x02 (SLP_S3# virtual wire)
 *   - Data:  0x11 (wire state)
 *
 * Note: Hardware doesn't handle virtual wire protocol automatically,
 * so we implement common responses in software for basic virtual wire functionality.
 *
 * TODO: Replace hard-coded virtual wire response table with a dynamic
 * registration mechanism or firmware-provided configuration.
 * JIRA Ticket: KWS-7745
 */
static const struct vw_response {
	u32 cmd;
	u32 response[6];
	int count;
} vw_response_table[] = {
	{ 0x1102, { 0x1181 }, 1 }, /* Ind. 0x02, Dat 0x11 -> Resp. Ind 0x81, Dat. 0x11 */
	{ 0x1107, { 0x0C04, 0x0905, 0x8F06, 0x0140, 0x0045, 0x0046 }, 6 },
	{ 0x1007, { 0x0C04, 0x0905, 0x8706, 0x0140, 0x0045, 0x0046 }, 6 },
	{ 0x7607, { 0x9905 }, 1 },
	{ 0x3303, { 0x9905 }, 1 }, /* Maverick case, we need to send boot status */
};

/* External Logic Register offsets (relative to ATU base) */
#define ESPI_EXT_ISR_OFFSET		0x168	/* Status register */
#define ESPI_EXT_ISR_MASK_OFFSET	0x16C	/* Mask register */
#define ESPI_INTERNAL_ISR_OFFSET	0x15C	/* Internal status register */
#define ESPI_INTERNAL_ISR_MASK_OFFSET	0x160	/* Internal mask register */

/* External IRQ bit definitions */
#define ESPI_EXT_IRQ_KCS0		0	/* KCS0 interrupt (bit 0) */
#define ESPI_EXT_IRQ_KCS1		1	/* KCS1 interrupt (bit 1) */
#define ESPI_EXT_IRQ_KCS2		2	/* KCS2 interrupt (bit 2) */
#define ESPI_EXT_IRQ_KCS3		3	/* KCS3 interrupt (bit 3) */
#define ESPI_EXT_IRQ_UART0_ESPI		4	/* UART0 eSPI side interrupt */
#define ESPI_EXT_IRQ_UART0_CPU		5	/* UART0 CPU side interrupt */
#define ESPI_EXT_IRQ_UART1_ESPI		6	/* UART1 eSPI side interrupt */
#define ESPI_EXT_IRQ_UART1_CPU		7	/* UART1 CPU side interrupt */
#define ESPI_EXT_IRQ_UART2_ESPI		8	/* UART2 eSPI side interrupt */
#define ESPI_EXT_IRQ_UART2_CPU		9	/* UART2 CPU side interrupt */
#define ESPI_EXT_IRQ_UART3_ESPI		10	/* UART3 eSPI side interrupt */
#define ESPI_EXT_IRQ_UART3_CPU		11	/* UART3 CPU side interrupt */
#define ESPI_EXT_IRQ_PC_FIFO_EMPTY	12	/* PostCode FIFO empty */
#define ESPI_EXT_IRQ_PC_FIFO_NOT_EMPTY	13	/* PostCode FIFO not empty */
#define ESPI_EXT_IRQ_PC_FIFO_FULL	14	/* PostCode FIFO full */

/* regmap IRQ definitions for external logic */
static const struct regmap_irq espi_ext_irqs[] = {
	REGMAP_IRQ_REG(ESPI_EXT_IRQ_KCS0, 0, BIT(ESPI_EXT_IRQ_KCS0)),
	REGMAP_IRQ_REG(ESPI_EXT_IRQ_KCS1, 0, BIT(ESPI_EXT_IRQ_KCS1)),
	REGMAP_IRQ_REG(ESPI_EXT_IRQ_KCS2, 0, BIT(ESPI_EXT_IRQ_KCS2)),
	REGMAP_IRQ_REG(ESPI_EXT_IRQ_KCS3, 0, BIT(ESPI_EXT_IRQ_KCS3)),
	REGMAP_IRQ_REG(ESPI_EXT_IRQ_UART0_ESPI, 0, BIT(ESPI_EXT_IRQ_UART0_ESPI)),
	REGMAP_IRQ_REG(ESPI_EXT_IRQ_UART0_CPU, 0, BIT(ESPI_EXT_IRQ_UART0_CPU)),
	REGMAP_IRQ_REG(ESPI_EXT_IRQ_UART1_ESPI, 0, BIT(ESPI_EXT_IRQ_UART1_ESPI)),
	REGMAP_IRQ_REG(ESPI_EXT_IRQ_UART1_CPU, 0, BIT(ESPI_EXT_IRQ_UART1_CPU)),
	REGMAP_IRQ_REG(ESPI_EXT_IRQ_UART2_ESPI, 0, BIT(ESPI_EXT_IRQ_UART2_ESPI)),
	REGMAP_IRQ_REG(ESPI_EXT_IRQ_UART2_CPU, 0, BIT(ESPI_EXT_IRQ_UART2_CPU)),
	REGMAP_IRQ_REG(ESPI_EXT_IRQ_UART3_ESPI, 0, BIT(ESPI_EXT_IRQ_UART3_ESPI)),
	REGMAP_IRQ_REG(ESPI_EXT_IRQ_UART3_CPU, 0, BIT(ESPI_EXT_IRQ_UART3_CPU)),
	REGMAP_IRQ_REG(ESPI_EXT_IRQ_PC_FIFO_NOT_EMPTY, 0, BIT(ESPI_EXT_IRQ_PC_FIFO_NOT_EMPTY)),
	REGMAP_IRQ_REG(ESPI_EXT_IRQ_PC_FIFO_FULL, 0, BIT(ESPI_EXT_IRQ_PC_FIFO_FULL)),
};

/* regmap IRQ chip configuration for Ext Logic ISR register */
static const struct regmap_irq_chip espi_ext_irq_chip = {
	.name = "espi-ext-irq",
	.irqs = espi_ext_irqs,
	.num_irqs = ARRAY_SIZE(espi_ext_irqs),
	.num_regs = 1,
	.use_ack = false,
	/* Note: status_base and mask_base will be set dynamically based on hardware type */
};

/* Get regmap from parent MFD device */
static int espi_get_mfd_regmap(struct ax_espi *espi)
{
	espi->regmap = dev_get_regmap(espi->dev->parent, NULL);
	if (!espi->regmap) {
		dev_err(espi->dev, "Failed to get regmap from parent MFD\n");
		return -ENODEV;
	}
	return 0;
}

/* TODO: Placeholder for direct MMIO access, to be implemented later */
static int espi_get_mmio_regmap(struct ax_espi *espi)
{
	dev_err(espi->dev, "MMIO regmap not implemented yet\n");
	return -ENOTSUPP;
}

/**
 * espi_get_hw_config() - Get hardware configuration from device tree
 * @pdev: Platform device
 *
 * Return: Hardware config structure or NULL on error
 */
static const struct espi_hw_config *espi_get_hw_config(struct platform_device *pdev)
{
	const struct of_device_id *match;

	match = of_match_device(ax_espi_of_match, &pdev->dev);
	if (!match || !match->data) {
		dev_err(&pdev->dev, "No matching hardware configuration found\n");
		return NULL;
	}

	return (const struct espi_hw_config *)match->data;
}

/**
 * espi_init_device_config() - Parse configuration from device tree
 * @espi: eSPI controller instance
 *
 * Parse timing and configuration parameters from device tree.
 * Falls back to defaults if properties are not specified.
 */
static void espi_init_device_config(struct ax_espi *espi)
{
	struct device_node *np = espi->dev->of_node;
	struct espi_config *cfg = &espi->config;

	/* Set defaults first */
	cfg->fifo_flush_timeout_ms = ESPI_DEFAULT_FIFO_FLUSH_TIMEOUT_MS;
	cfg->poll_delay_min_us = ESPI_DEFAULT_POLL_DELAY_MIN_US;
	cfg->poll_delay_max_us = ESPI_DEFAULT_POLL_DELAY_MAX_US;
	cfg->max_retries = ESPI_DEFAULT_MAX_RETRIES;

	if (!np)
		return;

	/* Parse optional timing parameters */
	of_property_read_u32(np, "axiado,fifo-flush-timeout-ms",
			     &cfg->fifo_flush_timeout_ms);
	of_property_read_u32(np, "axiado,poll-delay-min-us",
			     &cfg->poll_delay_min_us);
	of_property_read_u32(np, "axiado,poll-delay-max-us",
			     &cfg->poll_delay_max_us);
	of_property_read_u32(np, "axiado,max-retries",
			     &cfg->max_retries);

	/* Validate ranges */
	if (cfg->fifo_flush_timeout_ms > 1000) {
		dev_warn(espi->dev, "fifo_flush_timeout_ms too large, using default\n");
		cfg->fifo_flush_timeout_ms = ESPI_DEFAULT_FIFO_FLUSH_TIMEOUT_MS;
	}

	if (cfg->max_retries > 10) {
		dev_warn(espi->dev, "max_retries too large, using default\n");
		cfg->max_retries = ESPI_DEFAULT_MAX_RETRIES;
	}

	dev_dbg(espi->dev, "Config: timeout=%ums, poll=%u-%uus, retries=%u\n",
		cfg->fifo_flush_timeout_ms, cfg->poll_delay_min_us,
		cfg->poll_delay_max_us, cfg->max_retries);
}

/**
 * espi_get_resources() - Parse eSPI register resources based on hardware type
 * @espi: eSPI controller instance
 *
 * For MFD child: reg contains regmap offsets within parent's address space
 * For direct MMIO: reg contains physical MMIO addresses
 *
 * The device tree 'reg' property format:
 * mfd: reg = <core_offset core_size perif_offset perif_size atu_offset atu_size>;
 * mmio: reg = <core_addr core_size perif_addr perif_size atu_addr atu_size>;
 *
 * This function reads only the core register base (index 0). Child devices
 * (like KCS BMC) read the peripheral and ATU bases (indices 1 and 2) in the
 * respective drivers.
 *
 * Return: 0 on success, -errno on failure
 */
static int espi_get_resources(struct ax_espi *espi)
{
	struct device *dev = espi->dev;
	struct device_node *np = dev->of_node;
	u64 addr, size;
	int ret;

	if (!np) {
		dev_err(dev, "Device tree node not found\n");
		return -EINVAL;
	}

	/* Read eSPI core region (index 0) */
	ret = of_property_read_reg(np, 0, &addr, &size);
	if (ret) {
		dev_err(dev, "Failed to read core region: %d\n", ret);
		return ret;
	}

	if (espi->hw_config->hw_type == ESPI_HW_TYPE_MMIO) {
		/* For MMIO: addr is physical MMIO address, regmap handles the base */
		espi->core_base = 0;
		dev_dbg(dev, "MMIO mode: MMIO base 0x%llx, size 0x%llx\n", addr, size);
	} else {
		/* For MFD: addr is offset within parent regmap */
		if (addr > 0xFFFF) {
			dev_err(dev, "Invalid regmap offset: 0x%llx\n", addr);
			return -EINVAL;
		}
		espi->core_base = addr;
		dev_dbg(dev, "MFD mode: regmap offset 0x%04x, size 0x%llx\n",
			espi->core_base, size);
	}

	return 0;
}

/* Register access helpers with validation */
static inline u32 espi_reg_read(struct ax_espi *espi, u32 offset)
{
	u32 val;
	int ret;

	if (WARN_ON(!ESPI_VALID_REG_OFFSET(offset))) {
		dev_err(espi->dev, "Invalid register offset: 0x%x\n", offset);
		return 0;
	}

	ret = regmap_read(espi->regmap, espi->core_base + offset, &val);
	if (ret) {
		dev_err(espi->dev, "Failed to read core register 0x%x: %d\n",
			offset, ret);
		return 0;
	}
	return val;
}

static inline void espi_reg_write(struct ax_espi *espi, u32 offset, u32 val)
{
	int ret;

	if (WARN_ON(!ESPI_VALID_REG_OFFSET(offset))) {
		dev_err(espi->dev, "Invalid register offset: 0x%x\n", offset);
		return;
	}

	ret = regmap_write(espi->regmap, espi->core_base + offset, val);
	if (ret) {
		dev_err(espi->dev, "Failed to write core register 0x%x: %d\n",
			offset, ret);
	}
}

static inline void espi_reg_update_bits(struct ax_espi *espi, u32 offset,
					u32 mask, u32 val)
{
	int ret;

	if (WARN_ON(!ESPI_VALID_REG_OFFSET(offset))) {
		dev_err(espi->dev, "Invalid register offset: 0x%x\n", offset);
		return;
	}

	ret = regmap_update_bits(espi->regmap, espi->core_base + offset, mask, val);
	if (ret) {
		dev_err(espi->dev,
			"Failed to update core register 0x%x: %d\n",
			offset, ret);
	}
}

static int espi_flush_all_fifos(struct ax_espi *espi)
{
	unsigned long timeout = jiffies + msecs_to_jiffies(espi->config.fifo_flush_timeout_ms);
	u32 status;
	int retry_count;

	for (retry_count = 0; retry_count < espi->config.max_retries; retry_count++) {
		/* Initiate flush */
		espi_reg_write(espi, FIFO_FLUSH, ESPI_FIFO_ALL_FLUSH);

		/* Poll until flush completes or timeout */
		do {
			status = espi_reg_read(espi, FIFO_FLUSH);
			if (!(status & ESPI_FIFO_ALL_FLUSH)) {
				dev_dbg(espi->dev, "FIFO flush completed successfully\n");
				return 0;
			}

			usleep_range(espi->config.poll_delay_min_us,
				     espi->config.poll_delay_max_us);
		} while (time_before(jiffies, timeout));

		dev_warn(espi->dev, "FIFO flush attempt %d failed - status: 0x%08x\n",
			 retry_count + 1, status);

		/* Reset timeout for next retry */
		timeout = jiffies + msecs_to_jiffies(espi->config.fifo_flush_timeout_ms);
	}

	dev_err(espi->dev, "FIFO flush failed after %d retries - final status: 0x%08x\n",
		espi->config.max_retries, status);
	return -ETIMEDOUT;
}

static int espi_init_controller(struct ax_espi *espi)
{
	u32 val;
	int ret;

	/* Configure ESPI_CORE_CONFIG - Enable eSPI core and Alert */
	val = ESPI_CORE_ENABLE | ESPI_ALERT_ENABLE;
	espi_reg_write(espi, ESPI_CORE_CONFIG, val);

	/* Configure DMA_CONTROL - Enable DMA and set channel configuration */
	val = ESPI_DMA_CH0_RX_DMA_ENABLE | ESPI_DMA_CH3_RX_DMA_ENABLE;
	espi_reg_write(espi, DMA_CONFIG_CTRL, val);

	/* Set DMA mode - both CH0 and CH3 to Data-Only mode */
	espi_reg_write(espi, DMA_CONFIG_CTRL_2, ESPI_DMA_RX_MODE_DATA_ONLY);

	/* Flush all FIFOs to ensure clean state */
	ret = espi_flush_all_fifos(espi);
	if (ret) {
		dev_err(espi->dev,
			"Failed to flush FIFOs during initialization\n");
		return ret;
	}

	/* Enable only eSPI interrupts, not FIFO interrupts */
	espi_reg_write(espi, IRQ_ENABLE, ESPI_IRQ_ESPI);

	/* Enable specific eSPI interrupts */
	espi_reg_write(espi, IRQ_ENABLE_ESPI,
		       ESPI_IRQ_PUT_IO_WR_SHORT_DONE |
		       ESPI_IRQ_PUT_IO_RD_SHORT_DONE |
		       ESPI_IRQ_PUT_VW_DONE |
		       ESPI_IRQ_GET_VW_DONE);

	return 0;
}

static void espi_cleanup_controller(struct ax_espi *espi)
{
	/* Disable interrupts */
	espi_reg_write(espi, IRQ_ENABLE, 0);
	espi_reg_write(espi, IRQ_ENABLE_ESPI, 0);

	/* Flush FIFOs before shutdown */
	espi_flush_all_fifos(espi);

	/* Disable core */
	espi_reg_write(espi, ESPI_CORE_CONFIG, 0);
}

static enum espi_recovery_level espi_get_recovery_strategy(u32 err_status)
{
	/* Critical errors requiring full controller reset */
	if (err_status & (ESPI_ERR_INVALID_CMD | ESPI_ERR_UNEXPEC_CS_DEASSERT))
		return ESPI_RECOVERY_CONTROLLER_RESET;

	/* Channel-specific errors */
	if (err_status & (ESPI_ERR_MAL_PERI_CHANNEL | ESPI_ERR_MAL_VW_CHANNEL |
			  ESPI_ERR_MAL_OOB_CHANNEL | ESPI_ERR_MAL_FLASH_CHANNEL))
		return ESPI_RECOVERY_CHANNEL_RESET;

	/* FIFO-related errors */
	if (err_status & (ESPI_ERR_PUT_WOUT_FREE | ESPI_ERR_GET_WOUT_AVAIL))
		return ESPI_RECOVERY_FIFO_FLUSH;

	/* Minor errors */
	if (err_status & (ESPI_ERR_INVALID_CTYPE | ESPI_ERR_INVALID_CRC |
			  ESPI_ERR_PAGE_ERR))
		return ESPI_RECOVERY_NONE;

	return ESPI_RECOVERY_NONE;
}

static int espi_flush_channel_fifos(struct ax_espi *espi, u32 err_status)
{
	u32 channels_to_reset = 0;

	if (err_status & ESPI_ERR_MAL_PERI_CHANNEL)
		channels_to_reset |= ESPI_FIFO_PC_RX_FLUSH | ESPI_FIFO_PC_TX_FLUSH |
			ESPI_FIFO_NP_RX_FLUSH | ESPI_FIFO_NP_TX_FLUSH;

	if (err_status & ESPI_ERR_MAL_VW_CHANNEL)
		channels_to_reset |= ESPI_FIFO_VW_RX_FLUSH | ESPI_FIFO_VW_TX_FLUSH;

	if (err_status & ESPI_ERR_MAL_OOB_CHANNEL)
		channels_to_reset |= ESPI_FIFO_OOB_RX_FLUSH | ESPI_FIFO_OOB_TX_FLUSH;

	if (err_status & ESPI_ERR_MAL_FLASH_CHANNEL)
		channels_to_reset |= ESPI_FIFO_FLASH_NP_RX_FLUSH | ESPI_FIFO_FLASH_C_RX_FLUSH |
			ESPI_FIFO_FLASH_NP_TX_FLUSH | ESPI_FIFO_FLASH_C_TX_FLUSH;

	if (channels_to_reset) {
		espi_reg_write(espi, FIFO_FLUSH, channels_to_reset);
		dev_info(espi->dev, "Reset channels: 0x%x\n", channels_to_reset);
		return 0;
	}

	return espi_flush_all_fifos(espi);
}

static int espi_full_controller_reset(struct ax_espi *espi)
{
	int ret;

	dev_warn(espi->dev, "Performing full controller reset\n");

	/* IRQ handler might be running, prevent it during reset */
	disable_irq(espi->irq);

	/* Disable interrupts before reinitializing */
	espi_reg_write(espi, IRQ_ENABLE, 0);
	espi_reg_write(espi, IRQ_ENABLE_ESPI, 0);
	espi_reg_write(espi, IRQ_ENABLE_FIFO, 0);
	espi_reg_write(espi, IRQ_ENABLE_FIFO_2, 0);

	/* Attempt to re-initialize controller */
	ret = espi_init_controller(espi);
	if (ret) {
		dev_err(espi->dev, "Controller recovery failed: %d\n", ret);
		return ret;
	}

	enable_irq(espi->irq);
	dev_info(espi->dev, "Controller recovery successful\n");
	return 0;
}

static int espi_recover_error(struct ax_espi *espi, u32 err_status)
{
	enum espi_recovery_level level = espi_get_recovery_strategy(err_status);

	switch (level) {
	case ESPI_RECOVERY_FIFO_FLUSH:
		return espi_flush_all_fifos(espi);
	case ESPI_RECOVERY_CHANNEL_RESET:
		return espi_flush_channel_fifos(espi, err_status);
	case ESPI_RECOVERY_CONTROLLER_RESET:
		return espi_full_controller_reset(espi);
	default:
		return 0;
	}
}

/* Setup external logic IRQ chip for child devices */
static int espi_setup_external_irq(struct ax_espi *espi)
{
	struct device *dev = espi->dev;
	struct platform_device *pdev = to_platform_device(dev);
	struct regmap_irq_chip *chip;
	u32 ext_atu_base;
	int ret;

	espi->ext_irq = platform_get_irq_byname(pdev, "ext-logic-intr");
	if (espi->ext_irq < 0) {
		dev_err(dev, "Failed to get external logic IRQ: %d\n",
			espi->ext_irq);
		return espi->ext_irq;
	}

	/* allocate and configure IRQ chip */
	chip = devm_kmemdup(dev, &espi_ext_irq_chip,
			    sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	/* Calculate hardware-specific register addresses */
	ext_atu_base = espi->hw_config->ext_atu_base;
	chip->status_base = ext_atu_base + ESPI_EXT_ISR_OFFSET;
	chip->mask_base = ext_atu_base + ESPI_EXT_ISR_MASK_OFFSET;

	ret = devm_regmap_add_irq_chip(dev, espi->regmap, espi->ext_irq,
				       IRQF_ONESHOT | IRQF_SHARED,
				       0, chip, &espi->ext_irq_data);
	if (ret) {
		dev_err(dev, "Failed to add regmap IRQ chip: %d\n", ret);
		return ret;
	}

	dev_dbg(dev, "External logic regmap IRQ initialized (IRQ %d, ATU base 0x%x)\n",
		espi->ext_irq, ext_atu_base);

	return 0;
}

static void espi_handle_error(struct ax_espi *espi)
{
	u32 err_status;
	int ret;

	err_status = espi_reg_read(espi, ERROR_STATUS);
	if (!err_status)
		return;

	dev_err(espi->dev, "eSPI error occurred: 0x%08x\n", err_status);

	/* Handle specific errors */
	if (err_status & ESPI_ERR_INVALID_CMD)
		dev_err(espi->dev, "Invalid command error\n");

	if (err_status & ESPI_ERR_INVALID_CTYPE)
		dev_err(espi->dev, "Invalid cycle type error\n");

	if (err_status & ESPI_ERR_INVALID_CRC)
		dev_err(espi->dev, "Invalid CRC error\n");

	if (err_status & ESPI_ERR_UNEXPEC_CS_DEASSERT)
		dev_err(espi->dev, "Unexpected CS deassertion\n");

	if (err_status & ESPI_ERR_PUT_WOUT_FREE)
		dev_err(espi->dev, "PUT without free error\n");

	if (err_status & ESPI_ERR_GET_WOUT_AVAIL)
		dev_err(espi->dev, "GET without available error\n");

	if (err_status & ESPI_ERR_MAL_PERI_CHANNEL)
		dev_err(espi->dev, "Malformed peripheral channel packet\n");

	if (err_status & ESPI_ERR_MAL_VW_CHANNEL)
		dev_err(espi->dev, "Malformed virtual wire channel packet\n");

	if (err_status & ESPI_ERR_MAL_OOB_CHANNEL)
		dev_err(espi->dev, "Malformed OOB channel packet\n");

	if (err_status & ESPI_ERR_MAL_FLASH_CHANNEL)
		dev_err(espi->dev, "Malformed flash channel packet\n");

	if (err_status & ESPI_ERR_PAGE_ERR)
		dev_err(espi->dev, "Address range boundary exceeded\n");

	espi_reg_write(espi, ERROR_STATUS, err_status);

	/* Attempt graduated recovery based on error severity */
	ret = espi_recover_error(espi, err_status);
	if (ret)
		dev_err(espi->dev, "Error recovery failed: %d\n", ret);
}

static void espi_handle_reset(struct ax_espi *espi, u32 status)
{
	if (status & ESPI_IRQ_RESET_MASTER_ASSERT) {
		dev_info(espi->dev, "eSPI reset asserted by master\n");
		/* Perform reset actions */
		espi_flush_all_fifos(espi);
	}

	if (status & ESPI_IRQ_RESET_MASTER_DEASSERT) {
		dev_info(espi->dev, "eSPI reset deasserted by master\n");
		/* Re-initialize controller after reset */
		espi_init_controller(espi);
	}

	if (status & ESPI_IRQ_RESET_SLAVE_DONE)
		dev_dbg(espi->dev, "eSPI reset from slave completed\n");
}

static void espi_handle_peripheral_channel(struct ax_espi *espi, u32 status)
{
	/* Handle PUT PC completion */
	if (status & ESPI_IRQ_PUT_PC_DONE) {
		dev_dbg(espi->dev, "PUT PC command completed\n");
		/* Data is available in PC_RX_DATA_FIFO for wrapper */
	}

	/* Handle PUT NP completion */
	if (status & ESPI_IRQ_PUT_NP_DONE) {
		dev_dbg(espi->dev, "PUT NP command completed\n");
		/* Data is available in NP_RX_DATA_FIFO for wrapper */
	}

	/* Handle PUT IO Write Short completion */
	if (status & ESPI_IRQ_PUT_IO_WR_SHORT_DONE)
		dev_dbg(espi->dev, "PUT IO Write Short command completed\n");

	/* Handle PUT IO Read Short completion */
	if (status & ESPI_IRQ_PUT_IO_RD_SHORT_DONE)
		dev_dbg(espi->dev, "PUT IO Read Short command completed\n");

	/* Handle PUT Memory Write Short completion */
	if (status & ESPI_IRQ_PUT_MEM_WR_SHORT_DONE)
		dev_dbg(espi->dev, "PUT Memory Write Short command completed\n");

	/* Handle PUT Memory Read Short completion */
	if (status & ESPI_IRQ_PUT_MEM_RD_SHORT_DONE)
		dev_dbg(espi->dev, "PUT Memory Read Short command completed\n");

	/* Handle GET PC completion */
	if (status & ESPI_IRQ_GET_PC_DONE)
		dev_dbg(espi->dev, "GET PC command completed\n");

	/* Handle GET NP completion */
	if (status & ESPI_IRQ_GET_NP_DONE)
		dev_dbg(espi->dev, "GET NP command completed\n");
}

static void espi_process_virtual_wire_data(struct ax_espi *espi)
{
	u32 fifo_status;
	u32 vw_data;
	int i, j;

	fifo_status = espi_reg_read(espi, FIFO_STATUS);

	while (!(fifo_status & ESPI_FIFO_STS_VW_RX_EMPTY)) {
		vw_data = espi_reg_read(espi, VW_RX_DATA_FIFO);
		dev_dbg(espi->dev, "VW Data: 0x%08x\n", vw_data);

		/* Look up response in table */
		for (i = 0; i < ARRAY_SIZE(vw_response_table); i++) {
			if (vw_response_table[i].cmd == vw_data) {
				for (j = 0; j < vw_response_table[i].count; j++)
					espi_reg_write(espi, VW_TX_DATA_FIFO,
						       vw_response_table[i].response[j]);
				espi_reg_write(espi, TX_CHN_AVAIL_REQ,
					       ESPI_VW_TX_AVAIL_REQ);
				break;
			}
		}

		if (i == ARRAY_SIZE(vw_response_table))
			dev_dbg(espi->dev,
				"VW: Unknown virtual wire data: 0x%08x\n",
				vw_data);

		fifo_status = espi_reg_read(espi, FIFO_STATUS);
	}
}

static void espi_handle_virtual_wire(struct ax_espi *espi, u32 status)
{
	/* Handle PUT VW completion */
	if (status & ESPI_IRQ_PUT_VW_DONE) {
		dev_dbg(espi->dev, "PUT Virtual Wire command completed\n");
		/* Virtual wire data is available in VW_RX_DATA_FIFO */
		espi_process_virtual_wire_data(espi);
	}

	/* Handle GET VW completion */
	if (status & ESPI_IRQ_GET_VW_DONE)
		dev_dbg(espi->dev, "GET Virtual Wire command completed\n");
}

static void espi_handle_oob(struct ax_espi *espi, u32 status)
{
	/* Handle PUT OOB completion */
	if (status & ESPI_IRQ_PUT_OOB_DONE)
		dev_dbg(espi->dev, "PUT OOB command completed\n");

	/* Handle GET OOB completion */
	if (status & ESPI_IRQ_GET_OOB_DONE)
		dev_dbg(espi->dev, "GET OOB command completed\n");
}

static void espi_handle_flash(struct ax_espi *espi, u32 status)
{
	/* Handle PUT Flash NP completion */
	if (status & ESPI_IRQ_PUT_FLASH_NP_DONE)
		dev_dbg(espi->dev, "PUT Flash NP command completed\n");

	/* Handle PUT Flash C completion */
	if (status & ESPI_IRQ_PUT_FLASH_C_DONE)
		dev_dbg(espi->dev, "PUT Flash C command completed\n");

	/* Handle GET Flash NP completion */
	if (status & ESPI_IRQ_GET_FLASH_NP_DONE)
		dev_dbg(espi->dev, "GET Flash NP command completed\n");

	/* Handle GET Flash C completion */
	if (status & ESPI_IRQ_GET_FLASH_C_DONE)
		dev_dbg(espi->dev, "GET Flash C command completed\n");
}

static void espi_handle_configuration(struct ax_espi *espi, u32 status)
{
	/* Handle SET_CONFIGURATION completion */
	if (status & ESPI_IRQ_SET_CONFIG_DONE)
		dev_dbg(espi->dev, "SET_CONFIGURATION command completed\n");

	/* Handle INBAND_RESET completion */
	if (status & ESPI_IRQ_INBAND_RESET_DONE)
		dev_dbg(espi->dev, "INBAND_RESET command completed\n");
}

static irqreturn_t espi_handle_espi_interrupts(struct ax_espi *espi)
{
	u32 espi_status;

	espi_status = espi_reg_read(espi, IRQ_STATUS_ESPI);
	if (!espi_status)
		return IRQ_NONE;

	/* Clear the eSPI interrupt status */
	espi_reg_write(espi, IRQ_STATUS_ESPI, espi_status);

	/* Handle error conditions first */
	if (espi_status & ESPI_IRQ_ERROR_DETECT)
		espi_handle_error(espi);

	/* Handle peripheral channel interrupts */
	if (espi_status & ESPI_IRQ_PERIF_CHANNEL)
		espi_handle_peripheral_channel(espi, espi_status);

	/* Handle virtual wire channel interrupts */
	if (espi_status & ESPI_IRQ_VW_CHANNEL)
		espi_handle_virtual_wire(espi, espi_status);

	if (espi_status & ESPI_IRQ_OOB_CHANNEL)
		espi_handle_oob(espi, espi_status);

	if (espi_status & ESPI_IRQ_FLASH_CHANNEL)
		espi_handle_flash(espi, espi_status);

	if (espi_status & ESPI_IRQ_RESET)
		espi_handle_reset(espi, espi_status);

	if (espi_status & ESPI_IRQ_CONFIG)
		espi_handle_configuration(espi, espi_status);

	return IRQ_HANDLED;
}

static irqreturn_t espi_irq_handler(int irq, void *dev_id)
{
	struct ax_espi *espi = dev_id;
	u32 irq_status;
	irqreturn_t ret = IRQ_NONE;

	/* Read the main interrupt status */
	irq_status = espi_reg_read(espi, IRQ_STATUS);
	if (!irq_status)
		return IRQ_NONE;

	/* Clear the main interrupt status */
	espi_reg_write(espi, IRQ_STATUS, irq_status);
	ret = IRQ_HANDLED;

	/* Handle eSPI specific interrupts */
	if (irq_status & ESPI_IRQ_ESPI)
		espi_handle_espi_interrupts(espi);

	if (irq_status & ESPI_IRQ_FIFO)
		dev_dbg(espi->dev, "FIFO IRQ detected\n");

	return ret;
}

static int ax_espi_probe(struct platform_device *pdev)
{
	struct ax_espi *espi;
	int ret;

	espi = devm_kzalloc(&pdev->dev, sizeof(*espi), GFP_KERNEL);
	if (!espi)
		return -ENOMEM;

	espi->dev = &pdev->dev;

	espi->hw_config = espi_get_hw_config(pdev);
	if (!espi->hw_config)
		return -ENODEV;

	dev_info(&pdev->dev, "Hardware type: %s, ATU base: 0x%x\n",
		 espi->hw_config->hw_type == ESPI_HW_TYPE_MMIO ? "MMIO" : "MFD",
		 espi->hw_config->ext_atu_base);

	/* Parse configuration from device tree */
	espi_init_device_config(espi);

	/* Get regmap, different methods for MMIO vs MFD */
	if (espi->hw_config->hw_type == ESPI_HW_TYPE_MMIO) {
		/* MMIO: Create regmap from MMIO */
		ret = espi_get_mmio_regmap(espi);
	} else {
		/* MFD: Get regmap from parent MFD device */
		ret = espi_get_mfd_regmap(espi);
	}
	if (ret) {
		dev_err(&pdev->dev, "Failed to get regmap: %d\n", ret);
		return ret;
	}

	/* Get register base addresses from device tree */
	ret = espi_get_resources(espi);
	if (ret) {
		dev_err(&pdev->dev, "Failed to get register bases: %d\n", ret);
		return ret;
	}

	/* Setup external logic IRQ chip for child devices */
	ret = espi_setup_external_irq(espi);
	if (ret) {
		dev_err(&pdev->dev,
			"Failed to setup external regmap IRQ: %d\n", ret);
		return ret;
	}

	/* Get main eSPI IRQ before initializing controller */
	espi->irq = platform_get_irq(pdev, 0);
	if (espi->irq < 0) {
		dev_err(&pdev->dev,
			"Failed to get eSPI core IRQ: %d\n", espi->irq);
		return espi->irq;
	}

	/* Request main eSPI IRQ first but keep it disabled to prevent race */
	ret = devm_request_threaded_irq(&pdev->dev, espi->irq,
					NULL,
					espi_irq_handler,
					IRQF_ONESHOT | IRQF_NO_AUTOEN,
					"espi-core-irq", espi);
	if (ret) {
		dev_err(&pdev->dev, "Failed to request IRQ: %d\n", ret);
		return ret;
	}

	/* Now initialize the eSPI controller - safe since IRQ is disabled */
	ret = espi_init_controller(espi);
	if (ret) {
		dev_err(&pdev->dev,
			"Failed to initialize eSPI controller: %d\n", ret);
		return ret;
	}

	/* Store ext_irq_data for child devices. needed for regmap_irq_get_virq() */
	platform_set_drvdata(pdev, espi->ext_irq_data);

	/* Populate child devices from DT */
	ret = devm_of_platform_populate(&pdev->dev);
	if (ret) {
		dev_err(&pdev->dev, "Failed to populate child devices: %d\n", ret);
		goto err_cleanup_controller;
	}

	/* Enable IRQ last - everything is ready now */
	enable_irq(espi->irq);

	dev_info(&pdev->dev,
		 "eSPI controller ready (core IRQ: %d, ext IRQ: %d)\n",
		 espi->irq, espi->ext_irq);

	return 0;

err_cleanup_controller:
	espi_cleanup_controller(espi);
	return ret;
}

static int ax_espi_remove(struct platform_device *pdev)
{
	struct regmap_irq_chip_data *ext_irq_data = platform_get_drvdata(pdev);
	struct regmap *regmap;
	int irq;
	u16 core_base;

	if (!ext_irq_data)
		return 0;

	irq = platform_get_irq(pdev, 0);
	regmap = dev_get_regmap(pdev->dev.parent, NULL);

	/* Get core_base from device tree (same as in probe) */
	if (pdev->dev.of_node) {
		u64 addr, size;

		if (!of_property_read_reg(pdev->dev.of_node, 0, &addr, &size))
			core_base = (u16)addr;
		else
			core_base = 0;
	} else {
		core_base = 0;
	}

	/* Disable IRQ first to prevent race conditions */
	if (irq >= 0)
		disable_irq(irq);

	/* Now do proper hardware cleanup if we have regmap and core_base */
	if (regmap && core_base) {
		/* Disable interrupts */
		regmap_write(regmap, core_base + IRQ_ENABLE, 0);
		regmap_write(regmap, core_base + IRQ_ENABLE_ESPI, 0);

		/* Disable core */
		regmap_write(regmap, core_base + ESPI_CORE_CONFIG, 0);

		dev_dbg(&pdev->dev, "eSPI controller cleaned up\n");
	}

	return 0;
}

static const struct of_device_id ax_espi_of_match[] = {
	{
		.compatible = "axiado,ax3000-espi",
		.data = &espi_mmio_config,        /* MMIO mode */
	},
	{
		.compatible = "axiado,ax3000-espi-regmap",
		.data = &espi_mfd_config,         /* MFD mode */
	},
	{},
};
MODULE_DEVICE_TABLE(of, ax_espi_of_match);

static struct platform_driver ax_espi_driver = {
	.probe = ax_espi_probe,
	.remove = ax_espi_remove,
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = of_match_ptr(ax_espi_of_match),
	},
};

module_platform_driver(ax_espi_driver);

MODULE_AUTHOR("AXIADO CORPORATION");
MODULE_DESCRIPTION("Axiado eSPI Slave Controller Driver");
MODULE_LICENSE("GPL v2");
