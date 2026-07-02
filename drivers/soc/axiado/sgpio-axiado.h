/* SPDX-License-Identifier: GPL-2.0-or-later */

/* Copyright (c) 2021-25 Axiado Corporation (or its affiliates). All rights reserved.
 * Use, modification and redistribution of this file is subject to your possession
 * of a valid End User License Agreement (EULA) for the Axiado Product of which
 * these sources are part of and your compliance with all applicable terms and
 * conditions of such licence agreement.
 */

/* ax3000 sgpio B0_512bits_FPGA register offset */
#define REG_SGPIO_SLICE_MUX_CFG0_ADRS_OFFSET_512	0x000
#define REG_SGPIO_PRESET0_ADRS_OFFSET_512		0x1dc
#define REG_SGPIO_COUNT0_ADRS_OFFSET_512		0x1f0
#define REG_SGPIO_POS0_ADRS_OFFSET_512			0x204

#define REG_SGPIO_SLICE_MUX_CFG1_ADRS_OFFSET_512	0x004
#define REG_SGPIO_REG_LD_ADRS_OFFSET_512		0x014
#define REG_SGPIO_REG_SS_LD_W_ADRS_OFFSET_512		0x0d8
#define REG_SGPIO_PRESET1_ADRS_OFFSET_512		0x1e0
#define REG_SGPIO_COUNT1_ADRS_OFFSET_512		0x1f4
#define REG_SGPIO_POS1_ADRS_OFFSET_512			0x208

#define REG_SGPIO_SLICE_MUX_CFG2_ADRS_OFFSET_512	0x008
#define REG_SGPIO_REG_DO_ADRS_OFFSET_512		0x054
#define REG_SGPIO_REG_SS_DO_ADRS_OFFSET_512		0x158
#define REG_SGPIO_PRESET2_ADRS_OFFSET_512		0x1e4
#define REG_SGPIO_COUNT2_ADRS_OFFSET_512		0x1f8
#define REG_SGPIO_POS2_ADRS_OFFSET_512			0x20c

#define REG_SGPIO_SLICE_MUX_CFG3_ADRS_OFFSET_512	0x00c
#define REG_SGPIO_PRESET3_ADRS_OFFSET_512		0x1e8
#define REG_SGPIO_COUNT3_ADRS_OFFSET_512		0x1fc
#define REG_SGPIO_POS3_ADRS_OFFSET_512			0x210

#define REG_SGPIO_SLICE_MUX_CFG4_ADRS_OFFSET_512	0x010
#define REG_SGPIO_REG_OE_ADRS_OFFSET_512		0x0d4
#define REG_SGPIO_REG_SS_OE_ADRS_OFFSET_512		0x1d8
#define REG_SGPIO_PRESET4_ADRS_OFFSET_512		0x1ec
#define REG_SGPIO_COUNT4_ADRS_OFFSET_512		0x200
#define REG_SGPIO_POS4_ADRS_OFFSET_512			0x214

#define REG_SGPIO_MASK_ADRS_OFFSET_512			0x224
#define REG_SGPIO_CTRL_ENABLE_ADRS_OFFSET_512		0x218
#define REG_SGPIO_CTRL_ENABLE_POS_ADRS_OFFSET_512	0x21c

#define REG_SGPIO_REG_SS_DI_ADRS_OFFSET_512		0x198

#define REG_SGPIO_STATUS_ADRS_OFFSET_512		0x228

/* ax3000 sgpio B0 128 bits_FPGA register offset */
#define REG_SGPIO_SLICE_MUX_CFG0_ADRS_OFFSET_128            0x000
#define REG_SGPIO_PRESET0_ADRS_OFFSET_128                   0x08c
#define REG_SGPIO_COUNT0_ADRS_OFFSET_128                    0x0a0
#define REG_SGPIO_POS0_ADRS_OFFSET_128                      0x0b4

#define REG_SGPIO_SLICE_MUX_CFG1_ADRS_OFFSET_128            0x004
#define REG_SGPIO_REG_LD_ADRS_OFFSET_128                    0x014
#define REG_SGPIO_REG_SS_LD_W_ADRS_OFFSET_128               0x048
#define REG_SGPIO_PRESET1_ADRS_OFFSET_128                   0x090
#define REG_SGPIO_COUNT1_ADRS_OFFSET_128                    0x0a4
#define REG_SGPIO_POS1_ADRS_OFFSET_128                      0x0b8

#define REG_SGPIO_SLICE_MUX_CFG2_ADRS_OFFSET_128            0x008
#define REG_SGPIO_REG_DO_ADRS_OFFSET_128                    0x024
#define REG_SGPIO_REG_SS_DO_ADRS_OFFSET_128                 0x068
#define REG_SGPIO_PRESET2_ADRS_OFFSET_128                   0x094
#define REG_SGPIO_COUNT2_ADRS_OFFSET_128                    0x0a8
#define REG_SGPIO_POS2_ADRS_OFFSET_128                      0x0bc

#define REG_SGPIO_SLICE_MUX_CFG3_ADRS_OFFSET_128            0x00c
#define REG_SGPIO_PRESET3_ADRS_OFFSET_128                   0x098
#define REG_SGPIO_COUNT3_ADRS_OFFSET_128                    0x0ac
#define REG_SGPIO_POS3_ADRS_OFFSET_128                      0x0c0

#define REG_SGPIO_SLICE_MUX_CFG4_ADRS_OFFSET_128            0x010
#define REG_SGPIO_REG_OE_ADRS_OFFSET_128                    0x044
#define REG_SGPIO_REG_SS_OE_ADRS_OFFSET_128                 0x088
#define REG_SGPIO_PRESET4_ADRS_OFFSET_128                   0x09c
#define REG_SGPIO_COUNT4_ADRS_OFFSET_128                    0x0b0
#define REG_SGPIO_POS4_ADRS_OFFSET_128                      0x0c4

#define REG_SGPIO_MASK_ADRS_OFFSET_128                      0x0d4
#define REG_SGPIO_CTRL_ENABLE_ADRS_OFFSET_128               0x0c8
#define REG_SGPIO_CTRL_ENABLE_POS_ADRS_OFFSET_128           0x0cc

#define REG_SGPIO_REG_SS_DI_ADRS_OFFSET_128                 0x078

#define REG_SGPIO_STATUS_ADRS_OFFSET_128                    0x0d8

#define MASK_CLR			0x0
#define MASK_EN				0xDFFF
#define STATUS_SHIFT		16
#define OFFSET(bank, pos)	((pos) + ((bank) * 32))
#define GET_BANK(a)			((a) / 32)
#define GET_POS(a)			((a) % 32)
#define SET_TILL_POS(reg, pos)		((reg) |= ((1 << (pos)) - 1))
#define CLEAR_BITS_UP_TO(reg, pos) \
	((reg) &= ~((1U << (pos)) - 1))
#define IS_BIT_SET(reg, bit_position) \
	(((reg)  & (1 << (bit_position))) != 0)

/* Below values are the maximum for the current design */
#define MAX_SGPIO_PINS		512
#define MAX_OFFSET_REG		16
#define MAX_SLICE_COUNT		4

#define NUM_WORDS(bits)   (((bits) + 31) / 32)
#define START_OFFSET(bits)   (16 - NUM_WORDS(bits))

struct ax3000_sgpio {
	u32 preset_value;
	u32 count_value;
	int bus_frequency;
	int apb_frequency;
	u32 pos;
	u32 pos_reset;
	u32 pos_reg;
	struct irq_chip irq;
	int ngpios;
	int max_sgpio_pins;
	int max_offset_regs;
	int fpga_flag;
	struct class *class;
	struct cdev cdev;
	dev_t dev;
	struct gpio_chip chip;
	u32 slice_enable;
	u32 mask_status[MAX_SGPIO_PINS]; /* 1 - Mask & 0 - Unmask */
	u32 irq_number[MAX_SGPIO_PINS]; /* To store the irq number of each bit */
	int parent_irq; /* Parent IRQ for the GPIO chip */
	void __iomem *membase;
#ifdef CONFIG_AXIADO_SGPIO_REGMAP
	struct regmap *regmap;
	u32 regmap_base_offset;
#endif
	struct sgpio_reg_offsets *regs;
};

/* Register access functions */
static inline u32 sgpio_reg_read(struct ax3000_sgpio *sgpio, u32 offset)
{
	u32 val = 0;

#ifdef CONFIG_AXIADO_SGPIO_REGMAP
	if (sgpio->regmap) {
		regmap_read(sgpio->regmap, sgpio->regmap_base_offset + offset,
			    &val);
		return val;
	}
#endif
	if (sgpio->membase)
		val = ioread32(sgpio->membase + offset);

	return val;
}

static inline void sgpio_reg_write(struct ax3000_sgpio *sgpio, u32 offset,
				   u32 val)
{
#ifdef CONFIG_AXIADO_SGPIO_REGMAP
	if (sgpio->regmap) {
		regmap_write(sgpio->regmap, sgpio->regmap_base_offset + offset,
			     val);
		return;
	}
#endif
	if (sgpio->membase)
		iowrite32(val, sgpio->membase + offset);
}

struct ax3000_slice_info {
	u32 out_mux;
	u32 sgpio_mux;
	u32 slice_mux;
	u32 reg[MAX_OFFSET_REG];
	u32 reg_ss[MAX_OFFSET_REG];
	u32 preset;
	u32 count;
	u32 pos;
};

struct sgpio_reg_offsets {
	u32  slice_mux_0;
	u32  slice_preset_0;
	u32  slice_count_0;
	u32  slice_pos_0;
	u32  slice_mux_1;
	u32  slice_ld;
	u32  slice_ld_ss;
	u32  slice_preset_1;
	u32  slice_count_1;
	u32  slice_pos_1;
	u32  slice_mux_2;
	u32  slice_dout;
	u32  slice_dout_ss;
	u32  slice_preset_2;
	u32  slice_count_2;
	u32  slice_pos_2;
	u32  slice_mux_3;
	u32  slice_preset_3;
	u32  slice_count_3;
	u32  slice_pos_3;
	u32  slice_mux_4;
	u32  slice_oe;
	u32  slice_oe_ss;
	u32  slice_preset_4;
	u32  slice_count_4;
	u32  slice_pos_4;
	u32  slice_mask;
	u32  slice_ctrl_en;
	u32  slice_ctrl_en_pos;
	u32  slice_din_ss;
	u32  slice_status;
};

static const struct sgpio_reg_offsets design_512_bit_regs = {
	.slice_mux_0 = REG_SGPIO_SLICE_MUX_CFG0_ADRS_OFFSET_512,
	.slice_preset_0 = REG_SGPIO_PRESET0_ADRS_OFFSET_512,
	.slice_count_0 = REG_SGPIO_COUNT0_ADRS_OFFSET_512,
	.slice_pos_0 = REG_SGPIO_POS0_ADRS_OFFSET_512,
	.slice_mux_1 = REG_SGPIO_SLICE_MUX_CFG1_ADRS_OFFSET_512,
	.slice_ld = REG_SGPIO_REG_LD_ADRS_OFFSET_512,
	.slice_ld_ss = REG_SGPIO_REG_SS_LD_W_ADRS_OFFSET_512,
	.slice_preset_1 = REG_SGPIO_PRESET1_ADRS_OFFSET_512,
	.slice_count_1 = REG_SGPIO_COUNT1_ADRS_OFFSET_512,
	.slice_pos_1 = REG_SGPIO_POS1_ADRS_OFFSET_512,
	.slice_mux_2 = REG_SGPIO_SLICE_MUX_CFG2_ADRS_OFFSET_512,
	.slice_dout = REG_SGPIO_REG_DO_ADRS_OFFSET_512,
	.slice_dout_ss = REG_SGPIO_REG_SS_DO_ADRS_OFFSET_512,
	.slice_preset_2 = REG_SGPIO_PRESET2_ADRS_OFFSET_512,
	.slice_count_2 = REG_SGPIO_COUNT2_ADRS_OFFSET_512,
	.slice_pos_2 = REG_SGPIO_POS2_ADRS_OFFSET_512,
	.slice_mux_3 = REG_SGPIO_SLICE_MUX_CFG3_ADRS_OFFSET_512,
	.slice_preset_3 = REG_SGPIO_PRESET3_ADRS_OFFSET_512,
	.slice_count_3 = REG_SGPIO_COUNT3_ADRS_OFFSET_512,
	.slice_pos_3 = REG_SGPIO_POS3_ADRS_OFFSET_512,
	.slice_mux_4 = REG_SGPIO_SLICE_MUX_CFG4_ADRS_OFFSET_512,
	.slice_oe = REG_SGPIO_REG_OE_ADRS_OFFSET_512,
	.slice_oe_ss = REG_SGPIO_REG_SS_OE_ADRS_OFFSET_512,
	.slice_preset_4 = REG_SGPIO_PRESET4_ADRS_OFFSET_512,
	.slice_count_4 = REG_SGPIO_COUNT4_ADRS_OFFSET_512,
	.slice_pos_4 = REG_SGPIO_POS4_ADRS_OFFSET_512,
	.slice_mask = REG_SGPIO_MASK_ADRS_OFFSET_512,
	.slice_ctrl_en = REG_SGPIO_CTRL_ENABLE_ADRS_OFFSET_512,
	.slice_ctrl_en_pos = REG_SGPIO_CTRL_ENABLE_POS_ADRS_OFFSET_512,
	.slice_din_ss = REG_SGPIO_REG_SS_DI_ADRS_OFFSET_512,
	.slice_status = REG_SGPIO_STATUS_ADRS_OFFSET_512,
};

static const struct sgpio_reg_offsets design_128_bit_regs = {
	.slice_mux_0 = REG_SGPIO_SLICE_MUX_CFG0_ADRS_OFFSET_128,
	.slice_preset_0 = REG_SGPIO_PRESET0_ADRS_OFFSET_128,
	.slice_count_0 = REG_SGPIO_COUNT0_ADRS_OFFSET_128,
	.slice_pos_0 = REG_SGPIO_POS0_ADRS_OFFSET_128,
	.slice_mux_1 = REG_SGPIO_SLICE_MUX_CFG1_ADRS_OFFSET_128,
	.slice_ld = REG_SGPIO_REG_LD_ADRS_OFFSET_128,
	.slice_ld_ss = REG_SGPIO_REG_SS_LD_W_ADRS_OFFSET_128,
	.slice_preset_1 = REG_SGPIO_PRESET1_ADRS_OFFSET_128,
	.slice_count_1 = REG_SGPIO_COUNT1_ADRS_OFFSET_128,
	.slice_pos_1 = REG_SGPIO_POS1_ADRS_OFFSET_128,
	.slice_mux_2 = REG_SGPIO_SLICE_MUX_CFG2_ADRS_OFFSET_128,
	.slice_dout = REG_SGPIO_REG_DO_ADRS_OFFSET_128,
	.slice_dout_ss = REG_SGPIO_REG_SS_DO_ADRS_OFFSET_128,
	.slice_preset_2 = REG_SGPIO_PRESET2_ADRS_OFFSET_128,
	.slice_count_2 = REG_SGPIO_COUNT2_ADRS_OFFSET_128,
	.slice_pos_2 = REG_SGPIO_POS2_ADRS_OFFSET_128,
	.slice_mux_3 = REG_SGPIO_SLICE_MUX_CFG3_ADRS_OFFSET_128,
	.slice_preset_3 = REG_SGPIO_PRESET3_ADRS_OFFSET_128,
	.slice_count_3 = REG_SGPIO_COUNT3_ADRS_OFFSET_128,
	.slice_pos_3 = REG_SGPIO_POS3_ADRS_OFFSET_128,
	.slice_mux_4 = REG_SGPIO_SLICE_MUX_CFG4_ADRS_OFFSET_128,
	.slice_oe = REG_SGPIO_REG_OE_ADRS_OFFSET_128,
	.slice_oe_ss = REG_SGPIO_REG_SS_OE_ADRS_OFFSET_128,
	.slice_preset_4 = REG_SGPIO_PRESET4_ADRS_OFFSET_128,
	.slice_count_4 = REG_SGPIO_COUNT4_ADRS_OFFSET_128,
	.slice_pos_4 = REG_SGPIO_POS4_ADRS_OFFSET_128,
	.slice_mask = REG_SGPIO_MASK_ADRS_OFFSET_128,
	.slice_ctrl_en = REG_SGPIO_CTRL_ENABLE_ADRS_OFFSET_128,
	.slice_ctrl_en_pos = REG_SGPIO_CTRL_ENABLE_POS_ADRS_OFFSET_128,
	.slice_din_ss = REG_SGPIO_REG_SS_DI_ADRS_OFFSET_128,
	.slice_status = REG_SGPIO_STATUS_ADRS_OFFSET_128,
};

extern struct ax3000_slice_info slice[MAX_SLICE_COUNT];
