// SPDX-License-Identifier: GPL-2.0-only
/*
 * MFD driver for Axiado SPI bridge.
 *
 * Copyright (c) 2021-25 Axiado Corporation (or its affiliates). All rights reserved.
 *
 * This driver provides a regmap interface to the Axiado SPI bridge,
 * enabling child devices to communicate with the hardware through
 * a standardized register access interface.
 */

#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/mfd/core.h>
#include <linux/regmap.h>
#include <linux/of_platform.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/mfd/syscon.h>
#include <linux/types.h>
#include <asm/unaligned.h>
#include <linux/mfd/axiado-mfd.h>
#if IS_ENABLED(CONFIG_MFD_AXIADO_SPI_DEBUG_FS)
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#endif

/* Axiado MFD firmware version */
#define AXIADO_MFD_FIRMWARE_VERSION 0xA814
/* Axiado MFD Board Type */
#define AXIADO_MFD_BOARD_TYPE 0xA818
/* Axiado MFD Board Type String Length */
#define AXIADO_MFD_BOARD_TYPE_LEN 9
/* Axiado MFD_mTahoe Type */
#define AXIADO_MFD_BOARD_TYPE_MT 0x20204d54
/* Axiado MFD_Snowbird Type */
#define AXIADO_MFD_BOARD_TYPE_SB 0x20205342

/* Address Phase Encoding Bits from Axiado MFD Spec */
#define AXIADO_MFD_ADDR_RW_BIT BIT(30)

/* Total length of the 3-phase SPI transaction in bytes */
#define TRANSACTION_LEN 12

/* Register offsets within the common_cfg block, from the design spec */
#define COMMON_CFG_INTR_STATUS_REG  0xa830

/*
 * Private data structure for our MFD driver.
 */
struct axiado_mfd_data {
	struct device       *dev;
	struct regmap       *regmap; /* The main regmap for the whole device */
	struct regmap_irq_chip_data *irq_chip_data;
#if IS_ENABLED(CONFIG_MFD_AXIADO_SPI_DEBUG_FS)
	struct dentry       *debugfs_dir; /* Debugfs directory */
#endif
};

/*
 * This array describes each individual interrupt within the shared status register.
 * The regmap irqchip framework uses this to know which bit to check.
 * Since all our IRQs are in a single register, reg_offset is always 0.
 */
static const struct regmap_irq axiado_mfd_irqs[] = {
	/* MAPPING FOR I2C 0-22 */
	/* IRQ index, Reg Offset, Mask      */
	REGMAP_IRQ_REG(I2C0_IRQ,  0, BIT(I2C0_IRQ)),
	REGMAP_IRQ_REG(I2C1_IRQ,  0, BIT(I2C1_IRQ)),
	REGMAP_IRQ_REG(I2C2_IRQ,  0, BIT(I2C2_IRQ)),
	REGMAP_IRQ_REG(I2C3_IRQ,  0, BIT(I2C3_IRQ)),
	REGMAP_IRQ_REG(I2C4_IRQ,  0, BIT(I2C4_IRQ)),
	REGMAP_IRQ_REG(I2C5_IRQ,  0, BIT(I2C5_IRQ)),
	REGMAP_IRQ_REG(I2C6_IRQ,  0, BIT(I2C6_IRQ)),
	REGMAP_IRQ_REG(I2C7_IRQ,  0, BIT(I2C7_IRQ)),
	REGMAP_IRQ_REG(I2C8_IRQ,  0, BIT(I2C8_IRQ)),
	REGMAP_IRQ_REG(I2C9_IRQ,  0, BIT(I2C9_IRQ)),
	REGMAP_IRQ_REG(I2C10_IRQ, 0, BIT(I2C10_IRQ)),
	REGMAP_IRQ_REG(I2C11_IRQ, 0, BIT(I2C11_IRQ)),
	REGMAP_IRQ_REG(I2C12_IRQ, 0, BIT(I2C12_IRQ)),
	REGMAP_IRQ_REG(I2C13_IRQ, 0, BIT(I2C13_IRQ)),
	REGMAP_IRQ_REG(I2C14_IRQ, 0, BIT(I2C14_IRQ)),
	REGMAP_IRQ_REG(I2C15_IRQ, 0, BIT(I2C15_IRQ)),
	REGMAP_IRQ_REG(I2C16_IRQ, 0, BIT(I2C16_IRQ)),
	REGMAP_IRQ_REG(I2C17_IRQ, 0, BIT(I2C17_IRQ)),
	REGMAP_IRQ_REG(I2C18_IRQ, 0, BIT(I2C18_IRQ)),
	REGMAP_IRQ_REG(I2C19_IRQ, 0, BIT(I2C19_IRQ)),
	REGMAP_IRQ_REG(I2C20_IRQ, 0, BIT(I2C20_IRQ)),
	REGMAP_IRQ_REG(I2C21_IRQ, 0, BIT(I2C21_IRQ)),
	REGMAP_IRQ_REG(I2C22_IRQ, 0, BIT(I2C22_IRQ)),
	/* MAPPING FOR SGPIO, LTPI, ESPI */
	[SGPIO_IRQ] = { .reg_offset = 0, .mask = BIT(SGPIO_IRQ) },
	[LTPI_IRQ] = { .reg_offset = 0, .mask = BIT(LTPI_IRQ) },
	[ESPI_IRQ] = { .reg_offset = 0, .mask = BIT(ESPI_IRQ) },
};

/*
 * This structure defines the irqchip itself.
 *
 * When the parent IRQ fires, the regmap IRQ framework will:
 * 1. Read the status register (status_base) to see which IRQs are pending.
 * 2. For each pending bit, it will trigger the corresponding virtual IRQ
 * for the child driver.
 *
 */
static const struct regmap_irq_chip axiado_mfd_irq_chip = {
	.name           = "axiado-mfd-irq",
	.status_base    = COMMON_CFG_INTR_STATUS_REG,
	.num_regs       = 1, /* All status bits are in a single register */
	.irqs           = axiado_mfd_irqs,
	.num_irqs       = AXIADO_NUM_IRQS,
};

/*
 * The hardware protocol uses a 3-phase SPI transaction.
 * Phase 1 (Bytes 0-3): Address word
 * Phase 2 (Bytes 4-7): Dummy
 * Phase 3 (Bytes 8-11): Data
 */
static int axiado_mfd_regmap_write(void *context, unsigned int reg,
				   unsigned int val)
{
	struct spi_device *spi = context;
	u32 addr_phase;
	u8 tx_buf[TRANSACTION_LEN];
	struct spi_transfer xfer = {
		.tx_buf = tx_buf,
		.len = TRANSACTION_LEN,
	};
	struct spi_message msg;

	/* The 'reg' from regmap is the 16-bit address.
	 * The hardware protocol expects the 16-bit offset in the lower bits.
	 */
	addr_phase = reg | AXIADO_MFD_ADDR_RW_BIT;

	/* Phase 1: Address */
	put_unaligned_le32(addr_phase, tx_buf + 0);
	/* Phase 2: Dummy */
	put_unaligned_le32(0, tx_buf + 4);
	/* Phase 3: Data */
	put_unaligned_le32(val, tx_buf + 8);

	spi_message_init(&msg);
	spi_message_add_tail(&xfer, &msg);

	return spi_sync(spi, &msg);
}

static int axiado_mfd_regmap_read(void *context, unsigned int reg,
				  unsigned int *val)
{
	struct spi_device *spi = context;
	u32 addr_phase;
	u8 tx_buf[TRANSACTION_LEN] = { 0 };
	u8 rx_buf[TRANSACTION_LEN];
	struct spi_transfer xfer = {
		.tx_buf = tx_buf,
		.rx_buf = rx_buf,
		.len = TRANSACTION_LEN,
	};
	struct spi_message msg;
	int ret;

	/* Address phase for read (RW bit is 0) */
	addr_phase = reg;

	/* Phase 1: Address */
	put_unaligned_le32(addr_phase, tx_buf + 0);
	/* Phase 2: Dummy (keep as 0 for read) */
	put_unaligned_le32(0, tx_buf + 4);
	/* Phase 3: Dummy (keep as 0 for read) */
	put_unaligned_le32(0, tx_buf + 8);

	spi_message_init(&msg);
	spi_message_add_tail(&xfer, &msg);

	ret = spi_sync(spi, &msg);
	if (ret < 0)
		return ret;

	/* Data is in the 3rd 4-byte cycle of the receive buffer */
	*val = get_unaligned_le32(rx_buf + 8);
	return 0;
}

static const struct regmap_bus axiado_mfd_regmap_bus = {
	.reg_write = axiado_mfd_regmap_write,
	.reg_read = axiado_mfd_regmap_read,
	.reg_format_endian_default = REGMAP_ENDIAN_LITTLE,
	.val_format_endian_default = REGMAP_ENDIAN_LITTLE,
};

static const struct regmap_config axiado_mfd_regmap_config = {
	.reg_bits = 16, /* Children see a 16-bit address space */
	.val_bits = 32,
	.max_register = 0xFFFF, /* Highest address from Axiado MFD spec */
	.name = "axiado-spi-bridge",
	.cache_type = REGCACHE_NONE,
};

static void axiado_mfd_print_version(struct device *dev, struct regmap *regmap)
{
	u32 bd_type, firmware_version;
	int ret;
	u8 ver[4];
	char board_type[AXIADO_MFD_BOARD_TYPE_LEN];

	ret = regmap_read(regmap, AXIADO_MFD_BOARD_TYPE, &bd_type);
	if (ret) {
		dev_err(dev, "Failed to read board type: %d\n", ret);
		return;
	}

	ret = regmap_read(regmap, AXIADO_MFD_FIRMWARE_VERSION,
			  &firmware_version);
	if (ret) {
		dev_err(dev, "Failed to read firmware version: %d\n", ret);
		return;
	}

	memset(board_type, 0, AXIADO_MFD_BOARD_TYPE_LEN);
	/*
	 * The board type string is fixed currently on the Axiado MFD image
	 * MT - 0x20204d54
	 * SB - 0x20205342
	 */
	switch (bd_type) {
	case AXIADO_MFD_BOARD_TYPE_MT:
		strscpy(board_type, "mTahoe", sizeof(board_type));
		break;
	case AXIADO_MFD_BOARD_TYPE_SB:
		strscpy(board_type, "Snowbird", sizeof(board_type));
		break;
	default:
		strscpy(board_type, "Unknown", sizeof(board_type));
		break;
	}

	put_unaligned_le32(firmware_version, ver);

	dev_info(dev, "Axiado MFD board type: %s\n", board_type);
	dev_info(dev, "Axiado MFD Version: %x.%x.%x\n", ver[3], ver[2],
		 (ver[1] << 8) | ver[0]);
}

#if IS_ENABLED(CONFIG_MFD_AXIADO_SPI_DEBUG_FS)
/*
 * Debugfs structure to hold register address and MFD data
 */
struct axiado_mfd_debugfs_data {
	struct axiado_mfd_data *mfd;
	unsigned int reg_addr;
};

/*
 * Debugfs read function for register value
 */
static int axiado_mfd_debugfs_value_read(void *data, u64 *val)
{
	struct axiado_mfd_debugfs_data *debugfs_data = data;
	struct axiado_mfd_data *mfd = debugfs_data->mfd;
	unsigned int reg = debugfs_data->reg_addr;
	unsigned int reg_val;
	int ret;

	ret = regmap_read(mfd->regmap, reg, &reg_val);
	if (ret < 0) {
		dev_err(mfd->dev, "Failed to read register 0x%04x: %d\n",
			reg, ret);
		return ret;
	}

	*val = reg_val; /* Only return 32-bit value */
	return 0;
}

/*
 * Debugfs write function for register value
 */
static int axiado_mfd_debugfs_value_write(void *data, u64 val)
{
	struct axiado_mfd_debugfs_data *debugfs_data = data;
	struct axiado_mfd_data *mfd = debugfs_data->mfd;
	unsigned int reg = debugfs_data->reg_addr;
	int ret;

	ret = regmap_write(mfd->regmap, reg, (unsigned int)val);
	if (ret < 0) {
		dev_err(mfd->dev, "Failed to write register 0x%04x: %d\n",
			reg, ret);
		return ret;
	}

	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(axiado_mfd_value_fops, axiado_mfd_debugfs_value_read,
			 axiado_mfd_debugfs_value_write, "0x%08llx\n");

/*
 * Debugfs read function for register address
 */
static int axiado_mfd_debugfs_addr_read(void *data, u64 *val)
{
	struct axiado_mfd_debugfs_data *debugfs_data = data;
	*val = debugfs_data->reg_addr;
	return 0;
}

/*
 * Debugfs write function for register address
 */
static int axiado_mfd_debugfs_addr_write(void *data, u64 val)
{
	struct axiado_mfd_debugfs_data *debugfs_data = data;

	/* Validate register address is within 16-bit range */
	if (val > 0xFFFF) {
		dev_err(debugfs_data->mfd->dev,
			"Invalid register address 0x%llx (max 0xFFFF)\n", val);
		return -EINVAL;
	}

	debugfs_data->reg_addr = (unsigned int)val;
	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(axiado_mfd_addr_fops, axiado_mfd_debugfs_addr_read,
			 axiado_mfd_debugfs_addr_write, "0x%04llx\n");

/*
 * Debugfs structure for specific registers
 */
struct axiado_mfd_specific_reg_data {
	struct axiado_mfd_data *mfd;
	unsigned int reg_addr;
};

/*
 * Debugfs read function for specific register (firmware_version, board_type, etc.)
 */
static int axiado_mfd_debugfs_specific_reg_read(void *data, u64 *val)
{
	struct axiado_mfd_specific_reg_data *reg_data = data;
	struct axiado_mfd_data *mfd = reg_data->mfd;
	unsigned int reg = reg_data->reg_addr;
	unsigned int reg_val;
	int ret;

	ret = regmap_read(mfd->regmap, reg, &reg_val);
	if (ret < 0) {
		dev_err(mfd->dev, "Failed to read register 0x%04x: %d\n",
			reg, ret);
		return ret;
	}

	*val = reg_val; /* Only return 32-bit value */
	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(axiado_mfd_specific_reg_fops,
			 axiado_mfd_debugfs_specific_reg_read,
			 NULL, "0x%08llx\n");

/*
 * Initialize debugfs interface for the MFD driver
 *
 * This creates a debugfs interface at /sys/kernel/debug/axiado-mfd/ with:
 * - addr: Set register address (0x0000-0xFFFF)
 * - value: Read/write register value at the address set in 'addr'
 * - firmware_version: Read firmware version register
 * - board_type: Read board type register
 * - intr_status: Read interrupt status register
 *
 * Usage examples:
 *   # Read register 0x0020
 *   echo 0x20 > /sys/kernel/debug/axiado-mfd/addr
 *   cat /sys/kernel/debug/axiado-mfd/value
 *
 *   # Write value 0x12345678 to register 0x0020
 *   echo 0x20 > /sys/kernel/debug/axiado-mfd/addr
 *   echo 0x12345678 > /sys/kernel/debug/axiado-mfd/value
 *
 *   # Read specific registers
 *   cat /sys/kernel/debug/axiado-mfd/firmware_version
 *   cat /sys/kernel/debug/axiado-mfd/board_type
 */
static void axiado_mfd_debugfs_init(struct axiado_mfd_data *mfd)
{
	struct dentry *reg_file;
	struct axiado_mfd_debugfs_data *debugfs_data;
	int i;

	/* Create debugfs directory */
	mfd->debugfs_dir = debugfs_create_dir("axiado-mfd", NULL);
	if (!mfd->debugfs_dir) {
		dev_warn(mfd->dev, "Failed to create debugfs directory\n");
		return;
	}

	/* Allocate debugfs data structure */
	debugfs_data = devm_kzalloc(mfd->dev, sizeof(*debugfs_data),
				    GFP_KERNEL);
	if (!debugfs_data)
		return;
	debugfs_data->mfd = mfd;
	debugfs_data->reg_addr = 0; /* Default to register 0 */

	/* Create addr file for setting register address */
	reg_file = debugfs_create_file("addr", 0644, mfd->debugfs_dir,
				       debugfs_data, &axiado_mfd_addr_fops);
	if (!reg_file)
		dev_warn(mfd->dev, "Failed to create addr debugfs file\n");

	/* Create value file for reading/writing register value */
	reg_file = debugfs_create_file("value", 0644, mfd->debugfs_dir,
				       debugfs_data, &axiado_mfd_value_fops);
	if (!reg_file)
		dev_warn(mfd->dev, "Failed to create value debugfs file\n");

	/* Create individual register files for common registers */
	struct {
		unsigned int addr;
		const char *name;
	} common_regs[] = {
		{ AXIADO_MFD_FIRMWARE_VERSION, "firmware_version" },
		{ AXIADO_MFD_BOARD_TYPE, "board_type" },
		{ COMMON_CFG_INTR_STATUS_REG, "intr_status" },
	};

	for (i = 0; i < ARRAY_SIZE(common_regs); i++) {
		struct axiado_mfd_specific_reg_data *reg_data;

		/* Allocate memory for each specific register */
		reg_data = devm_kzalloc(mfd->dev, sizeof(*reg_data),
					GFP_KERNEL);
		if (!reg_data) {
			dev_warn(mfd->dev, "Could not set up register %s, skipping\n",
				common_regs[i].name);
			continue;
		}

		reg_data->mfd = mfd;
		reg_data->reg_addr = common_regs[i].addr;

		reg_file = debugfs_create_file(common_regs[i].name, 0444,
					       mfd->debugfs_dir, reg_data,
					       &axiado_mfd_specific_reg_fops);
		if (!reg_file) {
			dev_warn(mfd->dev, "Failed to create %s debugfs file\n",
				 common_regs[i].name);
		}
	}

	dev_info(mfd->dev, "Debugfs interface created at /sys/kernel/debug/axiado-mfd/\n");
}
#endif

/*
 * This function initializes the IRQ chip for the MFD driver.
 * It sets up the syscon regmap and registers the IRQ chip with the kernel.
 */
static int axiado_mfd_irq_init(struct device *dev, struct axiado_mfd_data *mfd)
{
	struct device_node *np = dev->of_node;
	int parent_irq;
	int ret;

	/*
	 * Get the physical, shared interrupt line from the Device Tree.
	 * We assume it is named "mfd-shared" in the parent MFD node's DT entry.
	 */
	parent_irq = of_irq_get_byname(np, "mfd-shared");
	if (parent_irq < 0) {
		/*
		 * Fallback to the first available interrupt if not named.
		 * Recommended practice is to name interrupts in the device tree.
		 */
		parent_irq = of_irq_get(np, 0);
		if (parent_irq < 0) {
			dev_err(dev, "Failed to get parent IRQ: %d\n",
				parent_irq);
			return parent_irq;
		}
	}

	/*
	 * This is the key function. It registers our irqchip with the kernel.
	 * It automatically:
	 * 1. Allocates a block of virtual IRQs.
	 * 2. Creates an irq_domain to map our hardware IRQs to virtual IRQs.
	 * 3. Requests the parent_irq and attaches the regmap framework's handler.
	 *
	 * When the parent_irq fires, the framework's handler will read our
	 * status register, check the bits, and fire the correct virtual IRQ.
	 */
	ret = devm_regmap_add_irq_chip(dev, mfd->regmap, parent_irq,
				       IRQF_ONESHOT | IRQF_SHARED, 0,
				       &axiado_mfd_irq_chip,
				       &mfd->irq_chip_data);
	if (ret < 0) {
		dev_err(dev, "Failed to add MFD IRQ chip: %d\n", ret);
		return ret;
	}

	return 0;
}

static int axiado_mfd_spi_probe(struct spi_device *spi)
{
	struct device *dev = &spi->dev;
	struct axiado_mfd_data *mfd;
	struct regmap *regmap;
	int ret;

	dev_info(dev, "Probing Axiado MFD SPI driver\n");

	/* Allocate MFD private data */
	mfd = devm_kzalloc(dev, sizeof(*mfd), GFP_KERNEL);
	if (!mfd)
		return -ENOMEM;

	mfd->dev = dev;

	regmap = devm_regmap_init(dev, &axiado_mfd_regmap_bus, spi,
				  &axiado_mfd_regmap_config);
	if (IS_ERR(regmap)) {
		dev_err(dev, "Failed to initialize regmap: %ld\n",
			PTR_ERR(regmap));
		return PTR_ERR(regmap);
	}

	mfd->regmap = regmap;

	axiado_mfd_print_version(dev, regmap);

	/* Initialize IRQ chip */
	ret = axiado_mfd_irq_init(dev, mfd);
	if (ret < 0) {
		dev_warn(dev, "Failed to initialize IRQ chip: %d\n", ret);
		/* Continue without IRQ support - not all configurations may need it */
	}
#if IS_ENABLED(CONFIG_MFD_AXIADO_SPI_DEBUG_FS)
	/* Initialize debugfs interface */
	axiado_mfd_debugfs_init(mfd);
#endif

	/* Store regmap data for child drivers to access */
	spi_set_drvdata(spi, mfd);

	/* Populate child devices from Device Tree */
	ret = of_platform_populate(dev->of_node, NULL, NULL, dev);
	if (ret) {
		dev_err(dev, "Failed to populate child devices: %d\n", ret);
		return ret;
	}

	return 0;
}

static void axiado_mfd_spi_remove(struct spi_device *spi)
{
	struct axiado_mfd_data *mfd = spi_get_drvdata(spi);

	dev_dbg(&spi->dev, "Removing Axiado MFD SPI driver\n");

	if (mfd)
		of_platform_depopulate(&spi->dev);

#if IS_ENABLED(CONFIG_MFD_AXIADO_SPI_DEBUG_FS)
	/* Remove debugfs interface */
	if (mfd && mfd->debugfs_dir) {
		debugfs_remove_recursive(mfd->debugfs_dir);
		mfd->debugfs_dir = NULL;
	}
#endif
}

static const struct spi_device_id axiado_spi_id_table[] = {
	{ "ax3000-spi-bridge", 0 },
	{ "spi-bridge", 0 },
	{},
};
MODULE_DEVICE_TABLE(spi, axiado_spi_id_table);

static const struct of_device_id axiado_mfd_spi_of_match[] = {
	{ .compatible = "axiado,ax3000-spi-bridge" },
	{ .compatible = "axiado,spi-bridge" },
	{}
};
MODULE_DEVICE_TABLE(of, axiado_mfd_spi_of_match);

static struct spi_driver axiado_mfd_spi_driver = {
	.driver = {
		.name = "axiado-mfd-spi",
		.of_match_table = of_match_ptr(axiado_mfd_spi_of_match),
	},
	.probe = axiado_mfd_spi_probe,
	.remove = axiado_mfd_spi_remove,
	.id_table = axiado_spi_id_table,
};

module_spi_driver(axiado_mfd_spi_driver);

MODULE_AUTHOR("AXIADO CORPORATION");
MODULE_DESCRIPTION("Axiado MFD SPI driver");
MODULE_LICENSE("GPL");
