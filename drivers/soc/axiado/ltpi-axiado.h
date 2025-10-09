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

#define GET_BANK(a) ((a) / 32)
#define GET_POS(a) ({ \
	typeof(a) _a = (a); \
	_a - (32 * GET_BANK(_a)); \
})
#define IS_BIT_SET(reg, bit_position) (((reg) & (1 << (bit_position))) != 0)
#define POS_TO_ID(pos, bank) ((pos) + (32 * (bank)))
#define BANK_SZ 32

#define LTPI_NODE 5
#define PINS_PER_GPIO_BANK 32

#define MAX_GPIO_PINS 128
#define MAX_OFFSET_REG (MAX_GPIO_PINS / 32)
#define MAX_BANKS 16

struct ax3000_ltpi {
	struct mutex lock; /* Protects concurrent access to GPIO operations */
	struct irq_chip irq;
	int lines;
	struct gpio_chip chip;
	struct irq_domain *irq_domain;
	int num_banks;
	u32 *inputs_cache;
	u32 *outputs_cache;
	u32 *mask; /* 1 - Mask & 0 - Unmask */
	struct work_struct ltpi_work;
	void __iomem *membase; /* Fallback for direct memory access */
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
