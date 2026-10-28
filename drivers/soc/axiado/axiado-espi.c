// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Axiado eSPI Slave Controller Driver
 *
 * Copyright (c) 2025-2026 Axiado Corporation
 */

#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/regmap.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/wait.h>

#include "axiado-espi.h"

#define DRIVER_NAME	"axiado-espi"

/* Configuration defaults - can be overridden via device tree */
#define ESPI_DEFAULT_FIFO_FLUSH_TIMEOUT_MS	100
#define ESPI_DEFAULT_POLL_DELAY_MIN_US		10
#define ESPI_DEFAULT_POLL_DELAY_MAX_US		20

/* Maximum FIFO flush timeout sanity limit */
#define ESPI_MAX_FIFO_FLUSH_TIMEOUT_MS		1000

/* Register offset validation */
#define ESPI_CORE_REG_MAX			0x1000

static inline bool espi_valid_reg_offset(u32 offset)
{
	return offset <= ESPI_CORE_REG_MAX;
}

/* Error recovery levels */
enum espi_recovery_level {
	ESPI_RECOVERY_NONE,
	ESPI_RECOVERY_FIFO_FLUSH,
	ESPI_RECOVERY_CHANNEL_RESET,
	ESPI_RECOVERY_CONTROLLER_RESET,
};

/* Hardware variant configuration (MMIO) */
static const struct espi_hw_config espi_mmio_config = {
	.ext_atu_base	= 0x2000,
};

/* External Logic Register offsets (relative to ATU base) */
#define ESPI_EXT_ISR_OFFSET		0x168   /* Status register */
#define ESPI_EXT_ISR_MASK_OFFSET	0x16C   /* Mask register */
#define ESPI_INTERNAL_ISR_OFFSET	0x15C   /* Internal status register */
#define ESPI_INTERNAL_ISR_MASK_OFFSET	0x160   /* Internal mask register */
#define ESPI_UART_CLKDIV_OFFSET		0x164   /* UART over eSPI clock divider */
#define ESPI_UART_CLKDIV_RATIO		GENMASK(6, 0)	/* Divider ratio, reset 0x02 */
#define ESPI_UART_CLKDIV_ENABLE		BIT(8)		/* Enable the clock divider */

/* External IRQ bit definitions */
#define ESPI_EXT_IRQ_KCS0		0       /* KCS0 interrupt */
#define ESPI_EXT_IRQ_KCS1		1       /* KCS1 interrupt */
#define ESPI_EXT_IRQ_KCS2		2       /* KCS2 interrupt */
#define ESPI_EXT_IRQ_KCS3		3       /* KCS3 interrupt */
#define ESPI_EXT_IRQ_UART0_ESPI		4       /* UART0 eSPI side interrupt */
#define ESPI_EXT_IRQ_UART0_CPU		5       /* UART0 CPU side interrupt */
#define ESPI_EXT_IRQ_UART1_ESPI		6       /* UART1 eSPI side interrupt */
#define ESPI_EXT_IRQ_UART1_CPU		7       /* UART1 CPU side interrupt */
#define ESPI_EXT_IRQ_UART2_ESPI		8       /* UART2 eSPI side interrupt */
#define ESPI_EXT_IRQ_UART2_CPU		9       /* UART2 CPU side interrupt */
#define ESPI_EXT_IRQ_UART3_ESPI		10      /* UART3 eSPI side interrupt */
#define ESPI_EXT_IRQ_UART3_CPU		11      /* UART3 CPU side interrupt */
#define ESPI_EXT_IRQ_PC_FIFO_EMPTY	12      /* PostCode FIFO empty */
#define ESPI_EXT_IRQ_PC_FIFO_NOT_EMPTY	13      /* PostCode FIFO not empty */
#define ESPI_EXT_IRQ_PC_FIFO_FULL	14      /* PostCode FIFO full */

/* Regmap IRQ definitions for eSPI extices */
static const struct regmap_irq espi_ext_irqs[] = {
	REGMAP_IRQ_REG(ESPI_EXT_IRQ_KCS0,		0, BIT(ESPI_EXT_IRQ_KCS0)),
	REGMAP_IRQ_REG(ESPI_EXT_IRQ_KCS1,		0, BIT(ESPI_EXT_IRQ_KCS1)),
	REGMAP_IRQ_REG(ESPI_EXT_IRQ_KCS2,		0, BIT(ESPI_EXT_IRQ_KCS2)),
	REGMAP_IRQ_REG(ESPI_EXT_IRQ_KCS3,		0, BIT(ESPI_EXT_IRQ_KCS3)),
	REGMAP_IRQ_REG(ESPI_EXT_IRQ_UART0_ESPI,		0, BIT(ESPI_EXT_IRQ_UART0_ESPI)),
	REGMAP_IRQ_REG(ESPI_EXT_IRQ_UART0_CPU,		0, BIT(ESPI_EXT_IRQ_UART0_CPU)),
	REGMAP_IRQ_REG(ESPI_EXT_IRQ_UART1_ESPI,		0, BIT(ESPI_EXT_IRQ_UART1_ESPI)),
	REGMAP_IRQ_REG(ESPI_EXT_IRQ_UART1_CPU,		0, BIT(ESPI_EXT_IRQ_UART1_CPU)),
	REGMAP_IRQ_REG(ESPI_EXT_IRQ_UART2_ESPI,		0, BIT(ESPI_EXT_IRQ_UART2_ESPI)),
	REGMAP_IRQ_REG(ESPI_EXT_IRQ_UART2_CPU,		0, BIT(ESPI_EXT_IRQ_UART2_CPU)),
	REGMAP_IRQ_REG(ESPI_EXT_IRQ_UART3_ESPI,		0, BIT(ESPI_EXT_IRQ_UART3_ESPI)),
	REGMAP_IRQ_REG(ESPI_EXT_IRQ_UART3_CPU,		0, BIT(ESPI_EXT_IRQ_UART3_CPU)),
	REGMAP_IRQ_REG(ESPI_EXT_IRQ_PC_FIFO_NOT_EMPTY,	0, BIT(ESPI_EXT_IRQ_PC_FIFO_NOT_EMPTY)),
	REGMAP_IRQ_REG(ESPI_EXT_IRQ_PC_FIFO_FULL,	0, BIT(ESPI_EXT_IRQ_PC_FIFO_FULL)),
};

/* Core register regmap configuration (Section A) */
static const struct regmap_config espi_core_regmap_config = {
	.reg_bits	= 32,
	.val_bits	= 32,
	.reg_stride	= 4,
	.max_register	= ESPI_CORE_REG_MAX,
	.cache_type	= REGCACHE_NONE,
};

/* Peripheral regmap configuration for Section B */
static const struct regmap_config espi_perif_regmap_config = {
	.reg_bits	= 32,
	.val_bits	= 32,
	.reg_stride	= 4,
	.max_register	= 0xFFF,
	.cache_type	= REGCACHE_NONE,
};

/* CSR regmap configuration for Section C */
static const struct regmap_config espi_csr_regmap_config = {
	.reg_bits	= 32,
	.val_bits	= 32,
	.reg_stride	= 4,
	.max_register	= 0xFFF,
	.cache_type	= REGCACHE_NONE,
};

/*
 * Regmap IRQ chip configuration for eSPI atu interrupt registers.
 * status_base and mask_base are fixed offsets from the ATU base.
 */
static const struct regmap_irq_chip espi_ext_irq_chip = {
	.name		= "espi-ext-irq",
	.irqs		= espi_ext_irqs,
	.num_irqs	= ARRAY_SIZE(espi_ext_irqs),
	.num_regs	= 1,
	.use_ack	= false,
	.status_base	= ESPI_EXT_ISR_OFFSET,
	.mask_base	= ESPI_EXT_ISR_MASK_OFFSET,
};

/**
 * espi_get_mmio_regmap() - Map MMIO register regions and create regmaps
 * @espi: eSPI controller instance
 *
 * Maps three memory resources from the platform device:
 *   0 - eSPI core registers  (Section A) -> core_regmap
 *   1 - Peripheral registers (Section B) -> perif_regmap
 *   2 - CSR / ATU registers  (Section C) -> atu_regmap
 *
 * Return: 0 on success, negative error code on failure.
 */
static int espi_get_mmio_regmap(struct ax_espi *espi)
{
	struct platform_device *pdev = to_platform_device(espi->dev);
	struct device *dev = espi->dev;
	void __iomem *base;

	base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(base))
		return dev_err_probe(dev, PTR_ERR(base),
				     "Failed to map eSPI core registers\n");

	espi->core_regmap = devm_regmap_init_mmio(dev, base,
						  &espi_core_regmap_config);
	if (IS_ERR(espi->core_regmap))
		return dev_err_probe(dev, PTR_ERR(espi->core_regmap),
				     "Failed to create core regmap\n");

	base = devm_platform_ioremap_resource(pdev, 1);
	if (IS_ERR(base))
		return dev_err_probe(dev, PTR_ERR(base),
				     "Failed to map peripheral registers\n");

	espi->perif_regmap = devm_regmap_init_mmio(dev, base,
						   &espi_perif_regmap_config);
	if (IS_ERR(espi->perif_regmap))
		return dev_err_probe(dev, PTR_ERR(espi->perif_regmap),
				     "Failed to create peripheral regmap\n");

	base = devm_platform_ioremap_resource(pdev, 2);
	if (IS_ERR(base))
		return dev_err_probe(dev, PTR_ERR(base),
				     "Failed to map CSR registers\n");

	espi->atu_regmap = devm_regmap_init_mmio(dev, base,
						 &espi_csr_regmap_config);
	if (IS_ERR(espi->atu_regmap))
		return dev_err_probe(dev, PTR_ERR(espi->atu_regmap),
				     "Failed to create CSR regmap\n");

	return 0;
}

/**
 * espi_get_hw_config() - Look up hardware configuration from device tree
 * @pdev: Platform device
 *
 * Return: Pointer to the matched hardware config, or NULL on failure.
 */
static const struct espi_hw_config *
espi_get_hw_config(struct platform_device *pdev)
{
	const struct espi_hw_config *cfg;

	cfg = of_device_get_match_data(&pdev->dev);
	if (!cfg)
		dev_err(&pdev->dev,
			"No matching hardware configuration found\n");

	return cfg;
}

/**
 * espi_init_device_config() - Parse configuration parameters from device tree
 * @espi: eSPI controller instance
 */
static void espi_init_device_config(struct ax_espi *espi)
{
	struct device_node *np = espi->dev->of_node;
	struct espi_config *cfg = &espi->config;

	cfg->fifo_flush_timeout_ms	= ESPI_DEFAULT_FIFO_FLUSH_TIMEOUT_MS;
	cfg->poll_delay_min_us		= ESPI_DEFAULT_POLL_DELAY_MIN_US;
	cfg->poll_delay_max_us		= ESPI_DEFAULT_POLL_DELAY_MAX_US;

	if (!np)
		return;

	of_property_read_u32(np, "axiado,fifo-flush-timeout-ms",
			     &cfg->fifo_flush_timeout_ms);
	of_property_read_u32(np, "axiado,poll-delay-min-us",
			     &cfg->poll_delay_min_us);
	of_property_read_u32(np, "axiado,poll-delay-max-us",
			     &cfg->poll_delay_max_us);

	if (cfg->fifo_flush_timeout_ms > ESPI_MAX_FIFO_FLUSH_TIMEOUT_MS) {
		dev_warn(espi->dev, "fifo-flush-timeout-ms out of range, using default\n");
		cfg->fifo_flush_timeout_ms = ESPI_DEFAULT_FIFO_FLUSH_TIMEOUT_MS;
	}

	dev_dbg(espi->dev, "Config: timeout=%ums, poll=%u-%uus\n",
		cfg->fifo_flush_timeout_ms, cfg->poll_delay_min_us, cfg->poll_delay_max_us);
}

/**
 * espi_reg_read() - Read a core register
 * @espi:   eSPI controller instance
 * @offset: Register offset
 *
 * Return: Register value, or 0 on error.
 */
static u32 espi_reg_read(struct ax_espi *espi, u32 offset)
{
	u32 val;
	int ret;

	if (WARN_ON(!espi_valid_reg_offset(offset))) {
		dev_err(espi->dev, "Invalid register offset: 0x%x\n", offset);
		return 0;
	}

	ret = regmap_read(espi->core_regmap, offset, &val);
	if (ret) {
		dev_err(espi->dev, "Failed to read register 0x%x: %d\n",
			offset, ret);
		return 0;
	}

	return val;
}

/* espi_reg_write() - Write a core register */
static void espi_reg_write(struct ax_espi *espi, u32 offset, u32 val)
{
	int ret;

	if (WARN_ON(!espi_valid_reg_offset(offset))) {
		dev_err(espi->dev, "Invalid register offset: 0x%x\n", offset);
		return;
	}

	ret = regmap_write(espi->core_regmap, offset, val);
	if (ret)
		dev_err(espi->dev, "Failed to write register 0x%x: %d\n",
			offset, ret);
}

/* espi_reg_update_bits() - Read-modify-write a core register */
static __maybe_unused void espi_reg_update_bits(struct ax_espi *espi, u32 offset,
						u32 mask, u32 val)
{
	int ret;

	if (WARN_ON(!espi_valid_reg_offset(offset))) {
		dev_err(espi->dev, "Invalid register offset: 0x%x\n", offset);
		return;
	}

	ret = regmap_update_bits(espi->core_regmap, offset, mask, val);
	if (ret)
		dev_err(espi->dev, "Failed to update register 0x%x: %d\n",
			offset, ret);
}

/* espi_wait_for_bit() - Poll a register bit until it reaches the expected value */
static int espi_wait_for_bit(struct ax_espi *espi, u32 reg, u32 mask,
			     u32 val, unsigned long timeout)
{
	unsigned long end = jiffies + timeout;

	do {
		if ((espi_reg_read(espi, reg) & mask) == val)
			return 0;
		usleep_range(espi->config.poll_delay_min_us, espi->config.poll_delay_max_us);
	} while (time_before(jiffies, end));

	return -ETIMEDOUT;
}

static int espi_flush_all_fifos(struct ax_espi *espi)
{
	int ret;

	espi_reg_write(espi, FIFO_FLUSH, ESPI_FIFO_ALL_FLUSH);
	ret = espi_wait_for_bit(espi, FIFO_FLUSH, ESPI_FIFO_ALL_FLUSH, 0,
				msecs_to_jiffies(espi->config.fifo_flush_timeout_ms));
	if (ret)
		dev_err(espi->dev, "Timeout waiting for FIFO flush\n");

	return ret;
}

static int espi_init_controller(struct ax_espi *espi)
{
	int ret;

	espi_reg_write(espi, ESPI_CORE_CONFIG, ESPI_CORE_ENABLE | ESPI_ALERT_ENABLE);
	espi_reg_write(espi, DMA_CONFIG_CTRL,
		       ESPI_DMA_CH0_RX_DMA_ENABLE | ESPI_DMA_CH3_RX_DMA_ENABLE);
	/* Set DMA mode to Data-Only for both channels */
	espi_reg_write(espi, DMA_CONFIG_CTRL_2, ESPI_DMA_RX_MODE_DATA_ONLY);

	ret = espi_flush_all_fifos(espi);
	if (ret) {
		dev_err(espi->dev, "Failed to flush FIFOs during initialization\n");
		return ret;
	}

	/* Enable top-level eSPI interrupt only */
	espi_reg_write(espi, IRQ_ENABLE, ESPI_IRQ_ESPI);
	/* Enable specific eSPI sub-interrupts */
	espi_reg_write(espi, IRQ_ENABLE_ESPI,
		       ESPI_IRQ_PUT_IO_WR_SHORT_DONE |
		       ESPI_IRQ_PUT_IO_RD_SHORT_DONE |
		       ESPI_IRQ_PUT_VW_DONE | ESPI_IRQ_GET_VW_DONE);

	return 0;
}

static void espi_cleanup_controller(struct ax_espi *espi)
{
	espi_reg_write(espi, IRQ_ENABLE, 0);
	espi_reg_write(espi, IRQ_ENABLE_ESPI, 0);
	espi_flush_all_fifos(espi);
	espi_reg_write(espi, ESPI_CORE_CONFIG, 0);
}

static enum espi_recovery_level espi_get_recovery_strategy(u32 err_status)
{
	if (err_status & (ESPI_ERR_INVALID_CMD | ESPI_ERR_UNEXPEC_CS_DEASSERT))
		return ESPI_RECOVERY_CONTROLLER_RESET;

	if (err_status & (ESPI_ERR_MAL_PERI_CHANNEL | ESPI_ERR_MAL_VW_CHANNEL |
			  ESPI_ERR_MAL_OOB_CHANNEL | ESPI_ERR_MAL_FLASH_CHANNEL))
		return ESPI_RECOVERY_CHANNEL_RESET;

	if (err_status & (ESPI_ERR_PUT_WOUT_FREE | ESPI_ERR_GET_WOUT_AVAIL))
		return ESPI_RECOVERY_FIFO_FLUSH;

	return ESPI_RECOVERY_NONE;
}

static int espi_flush_channel_fifos(struct ax_espi *espi, u32 err_status)
{
	u32 channels = 0;

	if (err_status & ESPI_ERR_MAL_PERI_CHANNEL)
		channels |= ESPI_FIFO_PC_RX_FLUSH | ESPI_FIFO_PC_TX_FLUSH |
			    ESPI_FIFO_NP_RX_FLUSH | ESPI_FIFO_NP_TX_FLUSH;
	if (err_status & ESPI_ERR_MAL_VW_CHANNEL)
		channels |= ESPI_FIFO_VW_RX_FLUSH | ESPI_FIFO_VW_TX_FLUSH;
	if (err_status & ESPI_ERR_MAL_OOB_CHANNEL)
		channels |= ESPI_FIFO_OOB_RX_FLUSH | ESPI_FIFO_OOB_TX_FLUSH;
	if (err_status & ESPI_ERR_MAL_FLASH_CHANNEL)
		channels |= ESPI_FIFO_FLASH_NP_RX_FLUSH |
			    ESPI_FIFO_FLASH_C_RX_FLUSH |
			    ESPI_FIFO_FLASH_NP_TX_FLUSH |
			    ESPI_FIFO_FLASH_C_TX_FLUSH;

	if (channels) {
		espi_reg_write(espi, FIFO_FLUSH, channels);
		dev_dbg(espi->dev, "Channel FIFO flush mask: 0x%08x\n",
			channels);
		return 0;
	}

	return espi_flush_all_fifos(espi);
}

static int espi_full_controller_reset(struct ax_espi *espi)
{
	int ret;

	dev_warn(espi->dev, "Performing full controller reset\n");
	disable_irq(espi->irq);
	espi_reg_write(espi, IRQ_ENABLE, 0);
	espi_reg_write(espi, IRQ_ENABLE_ESPI, 0);
	espi_reg_write(espi, IRQ_ENABLE_FIFO, 0);
	espi_reg_write(espi, IRQ_ENABLE_FIFO_2, 0);

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
	switch (espi_get_recovery_strategy(err_status)) {
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

/**
 * espi_setup_ext_irq() - Register the eSPI extice regmap IRQ chip
 * @espi: eSPI controller instance
 *
 * Return: 0 on success, negative error code on failure.
 */
static int espi_setup_ext_irq(struct ax_espi *espi)
{
	struct platform_device *pdev = to_platform_device(espi->dev);
	struct device *dev = espi->dev;
	u32 irq_base;
	int ret;

	espi->ext_irq = platform_get_irq_byname(pdev, "ext-atu-intr");
	if (espi->ext_irq < 0)
		return dev_err_probe(dev, espi->ext_irq,
				     "Failed to get extices IRQ\n");

	irq_base = espi->hw_config->ext_atu_base;
	ret = devm_regmap_add_irq_chip(dev, espi->atu_regmap, espi->ext_irq,
				       IRQF_ONESHOT | IRQF_SHARED, irq_base,
				       &espi_ext_irq_chip, &espi->ext_irq_data);
	if (ret)
		return dev_err_probe(dev, ret,
				     "Failed to add regmap IRQ chip\n");

	dev_dbg(dev, "Registered eSPI ext-atu IRQ chip (IRQ %d)\n", espi->ext_irq);

	return 0;
}

static void espi_handle_error(struct ax_espi *espi)
{
	u32 err_status;
	int ret;

	err_status = espi_reg_read(espi, ERROR_STATUS);
	if (!err_status)
		return;

	dev_err(espi->dev, "eSPI error status: 0x%08x\n", err_status);

	if (err_status & ESPI_ERR_INVALID_CMD)
		dev_err(espi->dev, "Invalid command\n");
	if (err_status & ESPI_ERR_INVALID_CTYPE)
		dev_err(espi->dev, "Invalid cycle type\n");
	if (err_status & ESPI_ERR_INVALID_CRC)
		dev_err(espi->dev, "Invalid CRC\n");
	if (err_status & ESPI_ERR_UNEXPEC_CS_DEASSERT)
		dev_err(espi->dev, "Unexpected CS deassertion\n");
	if (err_status & ESPI_ERR_PUT_WOUT_FREE)
		dev_err(espi->dev, "PUT without free\n");
	if (err_status & ESPI_ERR_GET_WOUT_AVAIL)
		dev_err(espi->dev, "GET without available\n");
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

	/* Acknowledge errors before attempting recovery */
	espi_reg_write(espi, ERROR_STATUS, err_status);

	ret = espi_recover_error(espi, err_status);
	if (ret)
		dev_err(espi->dev, "Error recovery failed: %d\n", ret);
}

/*
 * PLTRST_N monitoring interface, /dev/espi-pltrstn.
 *
 * The controller has no shadow register holding the host-driven system-event
 * wires, so the last PLTRST_N level is tracked in software as it arrives on the
 * VW channel and handed to userspace as a single ASCII character: '0' while the
 * host asserts platform reset, '1' once it deasserts. A blocking read sleeps
 * until the level changes; a non-blocking read returns the current sample once
 * and then -EAGAIN until the next change. poll() reports POLLIN when a sample
 * is pending.
 */
static void espi_pltrstn_update(struct ax_espi *espi, bool high)
{
	char state = high ? '1' : '0';
	unsigned long flags;

	spin_lock_irqsave(&espi->pltrstn_lock, flags);
	espi->pltrstn = state;
	espi->pltrstn_in_avail = true;
	spin_unlock_irqrestore(&espi->pltrstn_lock, flags);

	wake_up_interruptible(&espi->pltrstn_waitq);

	dev_dbg(espi->dev, "PLTRST_N: %c\n", state);
}

static inline struct ax_espi *to_ax_espi_pltrstn(struct file *filp)
{
	return container_of(filp->private_data, struct ax_espi,
			    pltrstn_miscdev);
}

static int espi_pltrstn_open(struct inode *inode, struct file *filp)
{
	struct ax_espi *espi = to_ax_espi_pltrstn(filp);
	unsigned long flags;

	if ((filp->f_flags & O_ACCMODE) != O_RDONLY)
		return -EACCES;

	/* Let the first non-blocking read after open return the current state */
	spin_lock_irqsave(&espi->pltrstn_lock, flags);
	espi->pltrstn_in_avail = true;
	spin_unlock_irqrestore(&espi->pltrstn_lock, flags);

	return 0;
}

/*
 * Consume the current sample if PLTRST_N has moved away from @old. Used as the
 * wait_event_interruptible() condition below, so it must not sleep - the
 * spinlock is the only thing it takes.
 */
static bool espi_pltrstn_take_change(struct ax_espi *espi, char old, char *data)
{
	unsigned long flags;
	bool changed;

	spin_lock_irqsave(&espi->pltrstn_lock, flags);
	changed = espi->pltrstn != old;
	if (changed) {
		*data = espi->pltrstn;
		espi->pltrstn_in_avail = false;
	}
	spin_unlock_irqrestore(&espi->pltrstn_lock, flags);

	return changed;
}

static ssize_t espi_pltrstn_read(struct file *filp, char __user *buf,
				 size_t count, loff_t *offset)
{
	struct ax_espi *espi = to_ax_espi_pltrstn(filp);
	unsigned long flags;
	char data, old_sample;
	int ret;

	if (!count)
		return 0;

	if (filp->f_flags & O_NONBLOCK) {
		spin_lock_irqsave(&espi->pltrstn_lock, flags);
		if (!espi->pltrstn_in_avail) {
			spin_unlock_irqrestore(&espi->pltrstn_lock, flags);
			return -EAGAIN;
		}
		data = espi->pltrstn;
		espi->pltrstn_in_avail = false;
		spin_unlock_irqrestore(&espi->pltrstn_lock, flags);
	} else {
		spin_lock_irqsave(&espi->pltrstn_lock, flags);
		old_sample = espi->pltrstn;
		spin_unlock_irqrestore(&espi->pltrstn_lock, flags);

		ret = wait_event_interruptible(espi->pltrstn_waitq,
					       espi_pltrstn_take_change(espi, old_sample,
									&data));
		if (ret)
			return ret;
	}

	if (put_user(data, buf))
		return -EFAULT;

	return sizeof(data);
}

static __poll_t espi_pltrstn_poll(struct file *filp, poll_table *wait)
{
	struct ax_espi *espi = to_ax_espi_pltrstn(filp);
	__poll_t mask = 0;
	unsigned long flags;

	poll_wait(filp, &espi->pltrstn_waitq, wait);
	spin_lock_irqsave(&espi->pltrstn_lock, flags);
	if (espi->pltrstn_in_avail)
		mask |= EPOLLIN;
	spin_unlock_irqrestore(&espi->pltrstn_lock, flags);
	return mask;
}

static const struct file_operations espi_pltrstn_fops = {
	.owner	= THIS_MODULE,
	.open	= espi_pltrstn_open,
	.read	= espi_pltrstn_read,
	.poll	= espi_pltrstn_poll,
};

static void espi_pltrstn_deregister(void *data)
{
	misc_deregister((struct miscdevice *)data);
}

static int espi_pltrstn_register(struct ax_espi *espi)
{
	int ret;

	spin_lock_init(&espi->pltrstn_lock);
	init_waitqueue_head(&espi->pltrstn_waitq);

	/*
	 * The host drives PLTRST_N low out of eSPI reset, so start out
	 * reporting it asserted; the first valid system-event VW corrects it.
	 */
	espi->pltrstn = '0';
	espi->pltrstn_in_avail = false;

	espi->pltrstn_miscdev.minor = MISC_DYNAMIC_MINOR;
	espi->pltrstn_miscdev.name = "espi-pltrstn";
	espi->pltrstn_miscdev.fops = &espi_pltrstn_fops;
	espi->pltrstn_miscdev.parent = espi->dev;

	ret = misc_register(&espi->pltrstn_miscdev);
	if (ret)
		return ret;

	return devm_add_action_or_reset(espi->dev, espi_pltrstn_deregister,
					&espi->pltrstn_miscdev);
}

static void espi_handle_reset(struct ax_espi *espi, u32 status)
{
	if (status & ESPI_IRQ_RESET_MASTER_ASSERT) {
		dev_info(espi->dev, "eSPI reset asserted by master\n");
		espi_flush_all_fifos(espi);
		/* Re-arm boot-load-done for the next link bring-up */
		espi->boot_load_done_sent = false;
		/* eSPI reset returns the system-event wires to their defaults */
		espi_pltrstn_update(espi, false);
	}

	if (status & ESPI_IRQ_RESET_MASTER_DEASSERT) {
		dev_info(espi->dev, "eSPI reset deasserted by master\n");
		espi_init_controller(espi);
	}

	if (status & ESPI_IRQ_RESET_SLAVE_DONE)
		dev_dbg(espi->dev, "eSPI slave reset completed\n");
}

static void espi_handle_peripheral_channel(struct ax_espi *espi, u32 status)
{
	if (status & ESPI_IRQ_PUT_PC_DONE)
		dev_dbg(espi->dev, "PUT PC completed\n");

	if (status & ESPI_IRQ_PUT_NP_DONE)
		dev_dbg(espi->dev, "PUT NP completed\n");
	if (status & ESPI_IRQ_PUT_IO_WR_SHORT_DONE)
		dev_dbg(espi->dev, "PUT IO Write Short completed\n");
	if (status & ESPI_IRQ_PUT_IO_RD_SHORT_DONE)
		dev_dbg(espi->dev, "PUT IO Read Short completed\n");
	if (status & ESPI_IRQ_PUT_MEM_WR_SHORT_DONE)
		dev_dbg(espi->dev, "PUT Memory Write Short completed\n");
	if (status & ESPI_IRQ_PUT_MEM_RD_SHORT_DONE)
		dev_dbg(espi->dev, "PUT Memory Read Short completed\n");
	if (status & ESPI_IRQ_GET_PC_DONE)
		dev_dbg(espi->dev, "GET PC completed\n");
	if (status & ESPI_IRQ_GET_NP_DONE)
		dev_dbg(espi->dev, "GET NP completed\n");
}

static void espi_process_virtual_wire_data(struct ax_espi *espi)
{
	u32 fifo_status, vw_data;

	fifo_status = espi_reg_read(espi, FIFO_STATUS);
	while (!(fifo_status & ESPI_FIFO_STS_VW_RX_EMPTY)) {
		u8 vw_index, vw_val;

		vw_data = espi_reg_read(espi, VW_RX_DATA_FIFO);
		dev_dbg(espi->dev, "VW data: 0x%08x\n", vw_data);

		vw_index = vw_data & 0xFF;
		vw_val = (vw_data >> 8) & 0xFF;

		/*
		 * System Event index 0x03 carries PLTRST# on wire1. Sample it
		 * only when the host marks that wire valid, otherwise the level
		 * bit is meaningless.
		 */
		if (vw_index == ESPI_VW_SYSEV3_INDEX &&
		    (vw_val & ESPI_VW_PLTRST_VALID))
			espi_pltrstn_update(espi,
					    !!(vw_val & ESPI_VW_PLTRST_LEVEL));

		/*
		 * When the host asserts HOST_RST_WARN (System Event index 0x07,
		 * valid and level 1), reply with HOST_RST_ACK = 1.
		 */
		if (vw_index == ESPI_VW_SYSEV7_INDEX &&
		    (vw_val & ESPI_VW_HOST_RST_WARN_VALID) &&
		    (vw_val & ESPI_VW_HOST_RST_WARN_LEVEL)) {
			dev_info(espi->dev,
				 "HOST_RST_WARN=1 (0x%08x), sending HOST_RST_ACK\n",
				 vw_data);
			espi_reg_write(espi, VW_TX_DATA_FIFO,
				       ESPI_VW_HOST_RST_ACK);
			espi_reg_write(espi, TX_CHN_AVAIL_REQ,
				       ESPI_VW_TX_AVAIL_REQ);
		}

		fifo_status = espi_reg_read(espi, FIFO_STATUS);
	}
}

static void espi_handle_virtual_wire(struct ax_espi *espi, u32 status)
{
	if (status & ESPI_IRQ_PUT_VW_DONE) {
		dev_dbg(espi->dev, "PUT Virtual Wire completed\n");
		espi_process_virtual_wire_data(espi);
	}

	if (status & ESPI_IRQ_GET_VW_DONE)
		dev_dbg(espi->dev, "GET Virtual Wire completed\n");
}

static void espi_handle_oob(struct ax_espi *espi, u32 status)
{
	if (status & ESPI_IRQ_PUT_OOB_DONE)
		dev_dbg(espi->dev, "PUT OOB completed\n");

	if (status & ESPI_IRQ_GET_OOB_DONE)
		dev_dbg(espi->dev, "GET OOB completed\n");
}

static void espi_handle_flash(struct ax_espi *espi, u32 status)
{
	if (status & ESPI_IRQ_PUT_FLASH_NP_DONE)
		dev_dbg(espi->dev, "PUT Flash NP completed\n");

	if (status & ESPI_IRQ_PUT_FLASH_C_DONE)
		dev_dbg(espi->dev, "PUT Flash C completed\n");
	if (status & ESPI_IRQ_GET_FLASH_NP_DONE)
		dev_dbg(espi->dev, "GET Flash NP completed\n");
	if (status & ESPI_IRQ_GET_FLASH_C_DONE)
		dev_dbg(espi->dev, "GET Flash C completed\n");
}

/*
 * Proactively push the SLAVE_BOOT_LOAD_STATUS/DONE virtual wire, mirroring
 * what the Aspeed slave does: queue it as soon as the host enables the VW
 * channel so the host sees boot-done before it drives the system-event wires
 * (SLP_S#, SUS_STAT#, PLTRST#, ...). Sent once per link bring-up.
 */
static void espi_send_boot_load_done(struct ax_espi *espi)
{
	if (espi->boot_load_done_sent)
		return;

	espi_reg_write(espi, VW_TX_DATA_FIFO, ESPI_VW_BOOT_LOAD_DONE);
	espi_reg_write(espi, TX_CHN_AVAIL_REQ, ESPI_VW_TX_AVAIL_REQ);
	espi->boot_load_done_sent = true;

	dev_info(espi->dev, "Sent SLAVE_BOOT_LOAD_DONE virtual wire (0x%04x)\n",
		 ESPI_VW_BOOT_LOAD_DONE);
}

static void espi_handle_configuration(struct ax_espi *espi, u32 status)
{
	if (status & ESPI_IRQ_SET_CONFIG_DONE) {
		u32 vw_cap;

		dev_dbg(espi->dev, "SET_CONFIGURATION completed\n");

		/*
		 * SET_CONFIG_DONE fires for every channel's configuration; the
		 * VW channel is channel 1. Once the host has both marked it
		 * ready and enabled it, queue boot-load-done.
		 */
		vw_cap = espi_reg_read(espi, ESPI_CH1_CAP_CONFIG);
		if ((vw_cap & (ESPI_CHAN_READY | ESPI_CHAN_ENABLE)) ==
		    (ESPI_CHAN_READY | ESPI_CHAN_ENABLE))
			espi_send_boot_load_done(espi);
	}

	if (status & ESPI_IRQ_INBAND_RESET_DONE)
		dev_dbg(espi->dev, "INBAND_RESET completed\n");
}

static void espi_handle_espi_interrupts(struct ax_espi *espi)
{
	u32 espi_status;

	espi_status = espi_reg_read(espi, IRQ_STATUS_ESPI);
	if (!espi_status)
		return;

	espi_reg_write(espi, IRQ_STATUS_ESPI, espi_status);

	if (espi_status & ESPI_IRQ_ERROR_DETECT)
		espi_handle_error(espi);
	if (espi_status & ESPI_IRQ_PERIF_CHANNEL)
		espi_handle_peripheral_channel(espi, espi_status);
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
}

static irqreturn_t espi_irq_handler(int irq, void *dev_id)
{
	struct ax_espi *espi = dev_id;
	u32 irq_status;

	irq_status = espi_reg_read(espi, IRQ_STATUS);
	if (!irq_status)
		return IRQ_NONE;

	espi_reg_write(espi, IRQ_STATUS, irq_status);

	if (irq_status & ESPI_IRQ_ESPI)
		espi_handle_espi_interrupts(espi);
	if (irq_status & ESPI_IRQ_FIFO)
		dev_dbg(espi->dev, "FIFO IRQ detected\n");

	return IRQ_HANDLED;
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

	dev_dbg(&pdev->dev, "eSPI MMIO controller, ATU base: 0x%x\n",
		espi->hw_config->ext_atu_base);

	espi_init_device_config(espi);

	ret = espi_get_mmio_regmap(espi);
	if (ret)
		return ret;

	/*
	 * Enable the UART over eSPI clock divider, keeping the default ratio.
	 * Shared by all UART channels, so set it once before they probe.
	 */
	ret = regmap_update_bits(espi->atu_regmap, ESPI_UART_CLKDIV_OFFSET,
				 ESPI_UART_CLKDIV_ENABLE,
				 ESPI_UART_CLKDIV_ENABLE);
	if (ret)
		return dev_err_probe(&pdev->dev, ret,
				     "Failed to enable UART clock divider\n");

	ret = espi_setup_ext_irq(espi);
	if (ret)
		return ret;

	espi->irq = platform_get_irq(pdev, 0);
	if (espi->irq < 0)
		return dev_err_probe(&pdev->dev, espi->irq, "Failed to get eSPI core IRQ\n");

	ret = devm_request_threaded_irq(&pdev->dev, espi->irq, NULL,
					espi_irq_handler,
					IRQF_ONESHOT | IRQF_NO_AUTOEN,
					"espi-core-irq", espi);
	if (ret)
		return dev_err_probe(&pdev->dev, ret,
				     "Failed to request IRQ\n");

	ret = espi_init_controller(espi);
	if (ret)
		return dev_err_probe(&pdev->dev, ret,
				     "Failed to initialize eSPI controller\n");

	platform_set_drvdata(pdev, espi);

	ret = devm_of_platform_populate(&pdev->dev);
	if (ret) {
		dev_err(&pdev->dev, "Failed to populate child devices: %d\n",
			ret);
		goto err_cleanup_controller;
	}

	/* Expose PLTRST_N to userspace before interrupts start flowing */
	ret = espi_pltrstn_register(espi);
	if (ret) {
		dev_err(&pdev->dev,
			"Failed to register PLTRST_N device: %d\n", ret);
		goto err_cleanup_controller;
	}

	enable_irq(espi->irq);

	dev_info(&pdev->dev, "eSPI controller ready (core IRQ: %d, eSPI-ATU IRQ: %d)\n",
		 espi->irq, espi->ext_irq);

	return 0;

err_cleanup_controller:
	espi_cleanup_controller(espi);
	return ret;
}

static void ax_espi_remove(struct platform_device *pdev)
{
	struct ax_espi *espi = platform_get_drvdata(pdev);

	if (espi->irq >= 0)
		disable_irq(espi->irq);

	espi_reg_write(espi, IRQ_ENABLE, 0);
	espi_reg_write(espi, IRQ_ENABLE_ESPI, 0);
	espi_reg_write(espi, ESPI_CORE_CONFIG, 0);

	dev_dbg(&pdev->dev, "eSPI controller removed\n");
}

static const struct of_device_id ax_espi_of_match[] = {
	{
		.compatible = "axiado,ax3000-espi",
		.data = &espi_mmio_config,
	},
	{}
};
MODULE_DEVICE_TABLE(of, ax_espi_of_match);

static struct platform_driver ax_espi_driver = {
	.probe  = ax_espi_probe,
	.remove = ax_espi_remove,
	.driver = {
		.name           = DRIVER_NAME,
		.of_match_table = ax_espi_of_match,
	},
};
module_platform_driver(ax_espi_driver);

MODULE_AUTHOR("AXIADO CORPORATION");
MODULE_DESCRIPTION("Axiado eSPI Slave Controller Driver");
MODULE_LICENSE("GPL");
