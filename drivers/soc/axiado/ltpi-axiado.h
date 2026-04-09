// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2021-25 Axiado Corporation.
 */

#ifndef _LTPI_AXIADO_H_
#define _LTPI_AXIADO_H_

/* ax3000 ltpi register offset */
#define REG_LTPI_DI_ADRS_OFFSET 0x0BC /* Input data register */
#define REG_LTPI_DO_ADRS_OFFSET 0x07C /* Output data register */
#define REG_LTPI_MASK_ADRS_OFFSET 0x108 /* Interrupt mask register */
#define REG_LTPI_STATUS_ADRS_OFFSET 0x104 /* Interrupt status register */

#define REG_LTPI_LOCAL_CAPA_LOW_OFFSET 0x014 /* Advertising local capabilities*/
#define REG_LTPI_LOCAL_CAPA_HIGH_OFFSET 0x018 /* Advertising local capabilities*/
#define REG_LTPI_REMOTE_CAPA_LOW_OFFSET    0x01C /* Advertising remote capabilities*/
#define REG_LTPI_REMOTE_CAPA_HIGH_OFFSET 0x020 /* Advertising remote capabilities*/
/* Configuration Local capabilities Low and High */
#define REG_LTPI_CONFIGURED_LOCAL_CAPA_LOW_OFFSET 0x024
#define REG_LTPI_CONFIGURED_LOCAL_CAPA_HIGH_OFFSET 0x028
#define REG_LTPI_CONTROL_STATUS_REG1_OFFSET 0x064

#define LINK_STATUS_READY_BIT BIT(18)

#define RESET_BIT BIT(0)

#define REG_LTPI_LINK_STATUS_OFFSET 0x00 /* LTPI Link status */

#define LOCAL_STATE_SHIFT 16 /* TCU side */
#define REMOTE_STATE_SHIFT 12 /* HPM side */

#define GET_BANK(a) ((a) / 32)
#define GET_POS(a) ({ \
	typeof(a) _a = (a); \
	_a - (32 * GET_BANK(_a)); \
})
#define POS_TO_ID(pos, bank) ((pos) + (32 * (bank)))
#define BANK_SZ 32

#define LTPI_NODE 5
#define PINS_PER_GPIO_BANK 32

#define MAX_GPIO_PINS 128
#define MAX_OFFSET_REG (MAX_GPIO_PINS / 32)
#define MAX_BANKS 16

/* LTPI Bus state on Local and Remote side */
#define LINK_DETECT 0x0
#define LINK_SPEED 0x1
#define ADVERTISE 0x2
#define CONFIGURATION 0x3
#define OPERATIONAL 0x4

#define LTPI_CAP_GPIO_BIT BIT(0) /* LTPI cap GPIO bit */
#define LTPI_CAP_I2C_BIT BIT(1) /* LTPI cap I2C bit */
#define LTPI_CAP_UART_BIT BIT(2) /* LTPI cap UART bit */
#define LTPI_CAP_DATA_BIT BIT(3) /* LTPI cap DATA bit */
#define LTPI_CAP_OEM_BIT BIT(4) /* LTPI cap OEM bit */
#define LTPI_CAP_I2C_CH0_BIT BIT(24) /* LTPI I2C Ch_0 bit */
#define LTPI_CAP_I2C_CH1_BIT BIT(25) /* LTPI I2C Ch_1 bit */
#define LTPI_CAP_I2C_CH2_BIT BIT(26) /* LTPI I2C Ch_2 bit */
#define LTPI_CAP_I2C_CH3_BIT BIT(27) /* LTPI I2C Ch_3 bit */
#define LTPI_CAP_I2C_CH4_BIT BIT(28) /* LTPI I2C Ch_4 bit */
#define LTPI_CAP_I2C_CH5_BIT BIT(29) /* LTPI I2C Ch_5 bit */
#define LTPI_CAP_I2C_ECHO_BIT BIT(30) /* LTPI cap I2C echo bit */
#define LTPI_CAP_UART_FLOW_CONTROL_BIT BIT(12) /* LTPI UART flow control bit */
#define LTPI_CAP_UART0_EN_BIT BIT(13) /* LTPI UART0 EN bit */
#define LTPI_CAP_UART1_EN_BIT BIT(14) /* LTPI UART1 EN bit */
#define NL_MAX_GPIO_PINS 128

#define LTPI_LINK_MAX_RETRIES 50
#define LTPI_LINK_RETRY_INTERVAL_MS 5000
#define LTPI_CAP_NL_GPIO_SHIFT 8
#define LTPI_CAP_I2C_SHIFT 24

#define LTPI_LINK_STATUS_SUCCESS 0
#define LTPI_LINK_STATUS_FAILED 1

struct ax3000_ltpi {
	struct mutex lock; /* Protects concurrent access to GPIO operations */
	struct irq_chip irq;
	int lines;
	struct gpio_chip chip;
	struct irq_domain *irq_domain;
	int num_banks;
	u32 cap_l;
	u32 cap_h;
	u32 *inputs_cache;
	u32 *outputs_cache;
	u32 *mask; /* 1 - Mask & 0 - Unmask */
	struct work_struct ltpi_work;
	struct delayed_work ltpi_link_work;
	void __iomem *membase; /* Fallback for direct memory access */
	struct device *dev;
	int link_retry;
#ifdef CONFIG_AXIADO_LTPI_REGMAP
	/* Regmap support */
	struct regmap *regmap;
	u32 regmap_base_offset;
#endif

#ifdef CONFIG_AXIADO_LTPI_DEBUGFS
	struct dentry *debugfs_root; /* for test */
	bool is_simulating_irq;
	int simulating_irq;
#endif
};

/**
 * Register access functions
 */

/**
 * ltpi_reg_read - Read a register value from LTPI device
 * @ltpi: LTPI device structure
 * @offset: Register offset to read
 * Return: Register value
 */
static inline u32 ltpi_reg_read(struct ax3000_ltpi *ltpi, u32 offset)
{
	u32 val = 0;

#ifdef CONFIG_AXIADO_LTPI_REGMAP
	if (ltpi->regmap) {
		regmap_read(ltpi->regmap, ltpi->regmap_base_offset + offset,
			    &val);
		return val;
	}
#endif
	if (ltpi->membase)
		val = ioread32(ltpi->membase + offset);
	return val;
}

/**
 * ltpi_reg_write - Write a value to LTPI device register
 * @ltpi: LTPI device structure
 * @offset: Register offset to write
 * @val: Value to write
 */
static inline void ltpi_reg_write(struct ax3000_ltpi *ltpi, u32 offset, u32 val)
{
#ifdef CONFIG_AXIADO_LTPI_REGMAP
	if (ltpi->regmap) {
		regmap_write(ltpi->regmap, ltpi->regmap_base_offset + offset,
			     val);
		return;
	}
#endif
	if (ltpi->membase)
		iowrite32(val, ltpi->membase + offset);
}

#endif /* _LTPI_AXIADO_H_ */
