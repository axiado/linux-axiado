// SPDX-License-Identifier: GPL-2.0-only
/*
 * Axiado eSPI Snoop Driver
 * Copyright (c) 2025-2026 Axiado Corporation
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kfifo.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/regmap.h>

#include "axiado-espi.h"

#define DRIVER_NAME "axiado-espi-snoop"
#define SNOOP_DEVICE_NAME "axiado-espi-snoop"

#define AXIADO_ESPI_SNOOP_FIFO_SIZE	256
#define MAX_READ_LIMIT			256

/* Section B register offsets for PostCode FIFO data access */
#define PC_FIFO_DATA_REG		0x0C0
#define PC_CMD_FIFO_DATA_REG		0x0D8

/* eSPI ATU register definitions for PostCode FIFO - Section C offsets */
#define ATU_RX_FIFO_START_LSB		0x040
#define ATU_RX_FIFO_START_MSB		0x044
#define ATU_RX_FIFO_END_LSB		0x0C8
#define ATU_RX_FIFO_END_MSB		0x0CC

/* ATU Control Registers - Section C */
#define ATU_ENABLE			0x150
#define ATU_ENABLE_RX_FIFO		BIT(8)

/* Interrupt Registers - Section C */
#define ATU_ISR				0x168
#define ATU_ISR_MASK			0x16C

/* PostCode FIFO interrupt bits in ISR - Must match regmap IRQ definitions */
#define ISR_PC_RX_FIFO_EMPTY		BIT(12)
#define ISR_PC_RX_FIFO_NOT_EMPTY	BIT(13)
#define ISR_PC_RX_FIFO_FULL		BIT(14)

/* FIFO Status Registers - Section C */
#define ATU_DATA_AVAIL			0x1A0
#define ATU_PTR				0x1A8
#define ATU_SOFT_RST			0x1B8

/* CMD FIFO Registers - Section C */
#define ATU_DATA_AVAIL_CMD		0x1BC
#define ATU_PTR_CMD			0x1C0
#define ATU_CMD_FIFO			0x1C8

/* PostCode FIFO data available mask */
#define PC_RX_FIFO_DATA_MASK		0x1FF

/* PostCode FIFO pointer masks */
#define PC_RX_FIFO_WR_PTR_MASK		0xFF
#define PC_RX_FIFO_RD_PTR_MASK		0xFF00
#define PC_RX_FIFO_RD_PTR_SHIFT		8

/* PostCode FIFO reset bits */
#define SOFT_RST_PC_FIFO_WPTR		BIT(0)
#define SOFT_RST_PC_FIFO_RPTR		BIT(1)

/**
 * struct axiado_espi_snoop - Snoop driver data
 * @dev: Platform device
 * @hw_cfg: Peripheral and ATU regmaps from the parent eSPI controller
 * (struct ax_espi_hw_cfg, shared across eSPI child drivers)
 * @fifo: Standard Linux kfifo for POST codes
 * @wq: Wait queue for poll/read operations
 * @miscdev: Misc device
 * @fifo_lock: Spinlock for kfifo protection (no regmap calls while held)
 * @irq: Virtual IRQ number from the parent eSPI controller's IRQ domain
 * @use_cmd_fifo: Flag to use CMD FIFO for additional metadata
 * @io_addr: I/O port address from host perspective
 * @addr_size: Size of address range
 */
struct axiado_espi_snoop {
	struct device *dev;
	struct ax_espi_hw_cfg hw_cfg;

	DECLARE_KFIFO(fifo, u32, AXIADO_ESPI_SNOOP_FIFO_SIZE);
	wait_queue_head_t wq;
	spinlock_t fifo_lock;  /* Protects kfifo only - no regmap calls */

	struct miscdevice miscdev;
	int irq;
	bool use_cmd_fifo;
	u32 io_addr;
	u32 addr_size;
};

/* Register access helpers - peripheral (Section B) registers */
static inline int snoop_reg_read(struct axiado_espi_snoop *snoop, u32 offset, u32 *val)
{
	int ret = regmap_read(snoop->hw_cfg.perif_regmap, offset, val);

	if (ret)
		dev_err(snoop->dev, "Failed to read register 0x%x: %d\n",
			offset, ret);
	return ret;
}

static inline int snoop_reg_write(struct axiado_espi_snoop *snoop, u32 offset, u32 val)
{
	int ret = regmap_write(snoop->hw_cfg.perif_regmap, offset, val);

	if (ret)
		dev_err(snoop->dev, "Failed to write register 0x%x: %d\n",
			offset, ret);
	return ret;
}

static inline int snoop_reg_update_bits(struct axiado_espi_snoop *snoop,
					u32 offset, u32 mask, u32 val)
{
	int ret = regmap_update_bits(snoop->hw_cfg.perif_regmap, offset,
				      mask, val);

	if (ret)
		dev_err(snoop->dev, "Failed to update register 0x%x: %d\n",
			offset, ret);
	return ret;
}

/* Register access helpers - ATU/CSR (Section C) registers */
static inline int snoop_atu_read(struct axiado_espi_snoop *snoop, u32 offset, u32 *val)
{
	int ret = regmap_read(snoop->hw_cfg.atu_regmap, offset, val);

	if (ret)
		dev_err(snoop->dev, "Failed to read ATU register 0x%x: %d\n",
			offset, ret);
	return ret;
}

static inline int snoop_atu_write(struct axiado_espi_snoop *snoop, u32 offset, u32 val)
{
	int ret = regmap_write(snoop->hw_cfg.atu_regmap, offset, val);

	if (ret)
		dev_err(snoop->dev, "Failed to write ATU register 0x%x: %d\n",
			offset, ret);
	return ret;
}

static inline int snoop_atu_update_bits(struct axiado_espi_snoop *snoop,
					u32 offset, u32 mask, u32 val)
{
	int ret = regmap_update_bits(snoop->hw_cfg.atu_regmap, offset,
				      mask, val);

	if (ret)
		dev_err(snoop->dev, "Failed to update ATU register 0x%x: %d\n",
			offset, ret);
	return ret;
}

/**
 * axiado_snoop_get_resources() - Fetch regmaps/bases from the parent eSPI
 * controller
 * @snoop: Pointer to Snoop driver private data structure
 * @pdev: Platform device
 * The Snoop device operates via the parent eSPI controller's own MMIO
 * regmaps, so all register access can go through the snoop_reg_*()/
 * snoop_atu_*() helpers unconditionally.
 * Return: 0 on success, negative error code on failure
 */
static int axiado_snoop_get_resources(struct axiado_espi_snoop *snoop,
				      struct platform_device *pdev)
{
	struct ax_espi *espi;

	espi = dev_get_drvdata(pdev->dev.parent);
	if (!espi)
		return dev_err_probe(snoop->dev, -ENODEV,
				      "Failed to get parent eSPI controller data\n");

	snoop->hw_cfg.perif_regmap = espi->perif_regmap;
	if (!snoop->hw_cfg.perif_regmap)
		return dev_err_probe(snoop->dev, -ENODEV,
				      "Parent eSPI controller missing peripheral regmap\n");

	snoop->hw_cfg.atu_regmap = espi->atu_regmap;
	if (!snoop->hw_cfg.atu_regmap)
		return dev_err_probe(snoop->dev, -ENODEV,
				      "Parent eSPI controller missing ATU/CSR regmap\n");

	return 0;
}

static struct axiado_espi_snoop *snoop_file_to_snoop_axiado(struct file *file)
{
	return container_of(file->private_data, struct axiado_espi_snoop,
			     miscdev);
}

static void put_fifo_with_discard(struct axiado_espi_snoop *snoop_axiado, u32 val)
{
	unsigned long flags;

	spin_lock_irqsave(&snoop_axiado->fifo_lock, flags);
	if (kfifo_is_full(&snoop_axiado->fifo))
		kfifo_skip(&snoop_axiado->fifo);
	if (kfifo_in(&snoop_axiado->fifo, &val, sizeof(val)) != sizeof(val)) {
		spin_unlock_irqrestore(&snoop_axiado->fifo_lock, flags);
		return;
	}
	spin_unlock_irqrestore(&snoop_axiado->fifo_lock, flags);
	wake_up_interruptible(&snoop_axiado->wq);
}

static bool axiado_snoop_read_fifo_data(struct axiado_espi_snoop *snoop_axiado, u32 *data)
{
	u32 data_avail, fifo_data;
	u32 fifo_data_reg;
	int ret;

	ret = snoop_atu_read(snoop_axiado, ATU_DATA_AVAIL, &data_avail);
	if (ret)
		return false;

	if ((data_avail & PC_RX_FIFO_DATA_MASK) == 0) {
		dev_dbg(snoop_axiado->dev, "No data available in PC FIFO\n");
		return false;
	}

	fifo_data_reg = snoop_axiado->use_cmd_fifo ? PC_CMD_FIFO_DATA_REG
						    : PC_FIFO_DATA_REG;

	ret = snoop_reg_read(snoop_axiado, fifo_data_reg, &fifo_data);
	if (ret)
		return false;

	*data = (u32)(fifo_data & 0xFFFFFFFF);
	dev_dbg(snoop_axiado->dev,
		"Read POST code: 0x%02X from PC FIFO (Section B offset 0x%x)\n",
		*data, fifo_data_reg);
	return true;
}

static void axiado_snoop_reset_fifo(struct axiado_espi_snoop *snoop_axiado)
{
	snoop_atu_update_bits(snoop_axiado, ATU_SOFT_RST,
			      SOFT_RST_PC_FIFO_WPTR | SOFT_RST_PC_FIFO_RPTR,
			      SOFT_RST_PC_FIFO_WPTR | SOFT_RST_PC_FIFO_RPTR);

	snoop_atu_update_bits(snoop_axiado, ATU_SOFT_RST,
			      SOFT_RST_PC_FIFO_WPTR | SOFT_RST_PC_FIFO_RPTR, 0);

	dev_dbg(snoop_axiado->dev, "PostCode FIFO reset completed\n");
}

static int axiado_snoop_configure_atu(struct axiado_espi_snoop *snoop_axiado)
{
	u64 start_addr = snoop_axiado->io_addr;
	u64 end_addr = snoop_axiado->io_addr + snoop_axiado->addr_size - 1;
	int ret;

	ret = snoop_atu_write(snoop_axiado, ATU_RX_FIFO_START_LSB,
			      (u32)(start_addr & 0xFFFFFFFF));
	if (ret)
		return ret;

	ret = snoop_atu_write(snoop_axiado, ATU_RX_FIFO_START_MSB,
			      (u32)(start_addr >> 32));
	if (ret)
		return ret;

	ret = snoop_atu_write(snoop_axiado, ATU_RX_FIFO_END_LSB,
			      (u32)(end_addr & 0xFFFFFFFF));
	if (ret)
		return ret;

	ret = snoop_atu_write(snoop_axiado, ATU_RX_FIFO_END_MSB,
			      (u32)(end_addr >> 32));
	if (ret)
		return ret;

	ret = snoop_atu_update_bits(snoop_axiado, ATU_ENABLE,
				    ATU_ENABLE_RX_FIFO, ATU_ENABLE_RX_FIFO);
	if (ret)
		return ret;

	dev_dbg(snoop_axiado->dev,
		"Configured ATU for Snoop: range 0x%llx-0x%llx\n",
		start_addr, end_addr);
	return 0;
}

static void axiado_snoop_disable_atu(struct axiado_espi_snoop *snoop_axiado)
{
	snoop_atu_update_bits(snoop_axiado, ATU_ENABLE, ATU_ENABLE_RX_FIFO, 0);
	dev_dbg(snoop_axiado->dev, "Disabled ATU for PostCode\n");
}

static irqreturn_t axiado_snoop_irq_handler(int irq, void *data)
{
	struct axiado_espi_snoop *snoop_axiado = data;
	u32 post_code, p_data;
	int count = 0;

	dev_dbg(snoop_axiado->dev, "PostCode virtual interrupt: IRQ %d\n", irq);

	/*
	 * Read all available POST codes from FIFO with bounds checking.
	 * NOTE: axiado_snoop_read_fifo_data() makes regmap calls that can
	 * sleep, so we CANNOT hold spinlocks during this loop.
	 */
	while (count < MAX_READ_LIMIT &&
	       axiado_snoop_read_fifo_data(snoop_axiado, &post_code)) {
		/*
		 * Convert hardware big-endian POST code to CPU endianness.
		 * The Snoop hardware provides data in big-endian format.
		 */
		p_data = be32_to_cpu(post_code);
		put_fifo_with_discard(snoop_axiado, p_data);
		dev_dbg(snoop_axiado->dev, "Received Snoop Code: %08X (count:%d)\n",
			p_data, count);
		count++;
	}

	if (count == MAX_READ_LIMIT)
		dev_warn(snoop_axiado->dev,
			 "Hit maximum read limit (%d) in IRQ handler\n",
			 MAX_READ_LIMIT);

	return IRQ_HANDLED;
}

static ssize_t axiado_snoop_read(struct file *filp, char __user *buf,
				 size_t count, loff_t *ppos)
{
	struct axiado_espi_snoop *snoop_axiado = snoop_file_to_snoop_axiado(filp);
	unsigned int copied;
	int ret = 0;

	if (count == 0)
		return 0;

	if (kfifo_is_empty(&snoop_axiado->fifo)) {
		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;
		ret = wait_event_interruptible(snoop_axiado->wq,
					       !kfifo_is_empty(&snoop_axiado->fifo));
		if (ret == -ERESTARTSYS)
			return -EINTR;
	}

	ret = kfifo_to_user(&snoop_axiado->fifo, buf, count, &copied);
	if (ret)
		return -EFAULT;

	dev_dbg(snoop_axiado->dev, "Read %u bytes\n", copied);
	return copied;
}

static __poll_t axiado_snoop_poll(struct file *filp,
				  struct poll_table_struct *wait)
{
	struct axiado_espi_snoop *snoop_axiado = snoop_file_to_snoop_axiado(filp);

	poll_wait(filp, &snoop_axiado->wq, wait);
	return !kfifo_is_empty(&snoop_axiado->fifo) ? EPOLLIN : 0;
}

static int axiado_snoop_open(struct inode *inode, struct file *filp)
{
	struct axiado_espi_snoop *snoop_axiado = snoop_file_to_snoop_axiado(filp);

	dev_dbg(snoop_axiado->dev, "PostCode device opened\n");
	return 0;
}

static int axiado_snoop_release(struct inode *inode, struct file *filp)
{
	struct axiado_espi_snoop *snoop_axiado = snoop_file_to_snoop_axiado(filp);

	dev_dbg(snoop_axiado->dev, "eSPI Snoop device closed\n");
	return 0;
}

static const struct file_operations axiado_snoop_fops = {
	.owner = THIS_MODULE,
	.open = axiado_snoop_open,
	.release = axiado_snoop_release,
	.read = axiado_snoop_read,
	.poll = axiado_snoop_poll,
	.llseek = noop_llseek,
};

static int axiado_snoop_setup_device(struct axiado_espi_snoop *snoop_axiado)
{
	int ret;

	snoop_axiado->miscdev.minor = MISC_DYNAMIC_MINOR;
	snoop_axiado->miscdev.name = SNOOP_DEVICE_NAME;
	snoop_axiado->miscdev.fops = &axiado_snoop_fops;
	snoop_axiado->miscdev.parent = snoop_axiado->dev;

	ret = misc_register(&snoop_axiado->miscdev);
	if (ret)
		return dev_err_probe(snoop_axiado->dev, ret,
				      "Failed to register misc device\n");

	return 0;
}

static void axiado_snoop_cleanup_device(struct axiado_espi_snoop *snoop_axiado)
{
	misc_deregister(&snoop_axiado->miscdev);
}

static int axiado_snoop_probe(struct platform_device *pdev)
{
	struct axiado_espi_snoop *snoop_axiado;
	int ret;
	u32 io_addr, addr_size;

	snoop_axiado = devm_kzalloc(&pdev->dev, sizeof(*snoop_axiado), GFP_KERNEL);
	if (!snoop_axiado)
		return -ENOMEM;

	snoop_axiado->dev = &pdev->dev;

	spin_lock_init(&snoop_axiado->fifo_lock);
	init_waitqueue_head(&snoop_axiado->wq);

	ret = of_property_read_u32(pdev->dev.of_node, "io-addr", &io_addr);
	if (ret)
		return dev_err_probe(&pdev->dev, -EINVAL,
				      "Missing io-addr property\n");
	snoop_axiado->io_addr = io_addr;

	ret = of_property_read_u32(pdev->dev.of_node, "addr-size", &addr_size);
	if (ret) {
		addr_size = 4;
		dev_dbg(&pdev->dev, "Using default addr-size: %u\n", addr_size);
	}
	snoop_axiado->addr_size = addr_size;

	snoop_axiado->use_cmd_fifo = of_property_read_bool(pdev->dev.of_node,
							   "use-cmd-fifo");

	INIT_KFIFO(snoop_axiado->fifo);

	ret = axiado_snoop_get_resources(snoop_axiado, pdev);
	if (ret)
		return ret;

	snoop_axiado->irq = platform_get_irq(pdev, 0);
	if (snoop_axiado->irq < 0)
		return dev_err_probe(&pdev->dev, snoop_axiado->irq,
				      "Failed to get virtual IRQ from device tree\n");

	dev_dbg(&pdev->dev,
		"Using virtual IRQ %d from perif regmap IRQ domain\n",
		snoop_axiado->irq);

	ret = axiado_snoop_configure_atu(snoop_axiado);
	if (ret)
		dev_err(&pdev->dev, "Failed to configure ATU for PostCode: %d\n", ret);

	axiado_snoop_reset_fifo(snoop_axiado);

	ret = devm_request_threaded_irq(&pdev->dev, snoop_axiado->irq, NULL,
					axiado_snoop_irq_handler,
					IRQF_ONESHOT | IRQF_TRIGGER_HIGH,
					"axiado-snoop-axiado", snoop_axiado);
	if (ret) {
		dev_err_probe(&pdev->dev, ret, "Failed to request virtual IRQ\n");
		goto err_disable_atu;
	}

	snoop_atu_update_bits(snoop_axiado, ATU_ISR_MASK,
			      ISR_PC_RX_FIFO_NOT_EMPTY, ISR_PC_RX_FIFO_NOT_EMPTY);

	ret = axiado_snoop_setup_device(snoop_axiado);
	if (ret)
		goto err_disable_atu;

	/*
	 * UNMASK PostCode interrupts to enable them.
	 * Writing 0 to mask bit = UNMASK = ENABLE interrupt.
	 * This is the key step that enables PostCode interrupt delivery.
	 */
	snoop_atu_update_bits(snoop_axiado, ATU_ISR_MASK,
			      ISR_PC_RX_FIFO_NOT_EMPTY, 0);

	platform_set_drvdata(pdev, snoop_axiado);

	dev_info(&pdev->dev,
		 "eSPI Snoop%s (IRQ: %d, io_addr: 0x%04x, range: %u bytes) ->/dev/%s\n",
		 snoop_axiado->use_cmd_fifo ? " with CMD FIFO" : "",
		 snoop_axiado->irq, io_addr, addr_size, SNOOP_DEVICE_NAME);

	return 0;

err_disable_atu:
	axiado_snoop_disable_atu(snoop_axiado);
	return ret;
}

static void axiado_snoop_remove(struct platform_device *pdev)
{
	struct axiado_espi_snoop *snoop_axiado = platform_get_drvdata(pdev);

	/* MASK PostCode interrupts - Writing 1 to mask bit = MASK = DISABLE interrupt */
	snoop_atu_update_bits(snoop_axiado, ATU_ISR_MASK,
			      ISR_PC_RX_FIFO_NOT_EMPTY, ISR_PC_RX_FIFO_NOT_EMPTY);

	axiado_snoop_disable_atu(snoop_axiado);
	axiado_snoop_cleanup_device(snoop_axiado);
}

static const struct of_device_id axiado_snoop_of_match[] = {
	{ .compatible = "axiado,ax3000-espi-snoop" },
	{},
};
MODULE_DEVICE_TABLE(of, axiado_snoop_of_match);

static struct platform_driver axiado_snoop_driver = {
	.probe = axiado_snoop_probe,
	.remove = axiado_snoop_remove,
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = axiado_snoop_of_match,
	},
};

module_platform_driver(axiado_snoop_driver);

MODULE_AUTHOR("AXIADO CORPORATION");
MODULE_DESCRIPTION("Axiado device interface to the eSPI Snoop device");
MODULE_LICENSE("GPL");
