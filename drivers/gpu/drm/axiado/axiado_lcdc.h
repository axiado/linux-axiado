/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2022-2025 Axiado Corporation.
 */

#ifndef __AXIADO_LCDC_H__
#define __AXIADO_LCDC_H__

#include<linux/debugfs.h>

#define DB9000_FB_MAX_WIDTH 4095
#define DB9000_FB_MAX_HEIGHT 4095
#define RZN1_REGS ((void *)1)

#define DB9000_MAX_LAYER 1
#define NB_PF 8
#define CLUT_SIZE 256
#define NB_CRTC 1
#define CRTC_MASK GENMASK(NB_CRTC - 1, 0)

#define CLR_REG_PTRN 0x00000000 /**< Default Register Pattern */

/* LCD Controller Control Register 1 */
#define DB9000_CR1 0x000
/* Horizontal Timing Register */
#define DB9000_HTR 0x008
/* Vertical Timing Register 1 */
#define DB9000_VTR1 0x00C
/* Vertical Timing Register 2 */
#define DB9000_VTR2 0x010
/* Pixel Clock Timing Register */
#define DB9000_PCTR 0x014
/* Interrupt Status Register */
#define DB9000_ISR 0x018
/* Interrupt Mask Register */
#define DB9000_IMR 0x01C
/* Interrupt Vector Register */
#define DB9000_IVR 0x020
/* Interrupt Scan Compare Register */
#define DB9000_ISCR 0x024
/* DMA Base Address Register */
#define DB9000_DBAR 0x028
/* DMA Current Address Register */
#define DB9000_DCAR 0x02C
/* DMA End Address Register */
#define DB9000_DEAR 0x030
/* DMA Horizontal and Vertical Timing Extension Register */
#define DB9000_HVTER 0x044
/* Horizontal Pixels-Per-Line Override Control */
#define DB9000_HPPLOR 0x048
/* Horizontal Pixels-Per-Line Override Enable */
#define DB9000_HPOE BIT(31)
/* GPIO Register */
#define DB9000_GPIOR 0x1F8
/* Core Identification Register */
#define DB9000_CIR 0x1FC
/* Palette Data Words */
#define DB9000_PALT 0x200

/* Control Register 1, Offset 0x000 */
/* LCD Controller Enable */
#define DB9000_CR1_LCE BIT(0)
/* LCD Power Enable */
#define DB9000_CR1_LPE BIT(1)
/* LCD Bits per Pixel */
#define DB9000_CR1_BPP(x) (((x) & 0x7) << 2)
/* RGB or BGR Format */
#define DB9000_CR1_RGB BIT(5)
/* Enable Vblank Event */
#define DB9000_CR1_VBLANK BIT(6)
/* Data Enable Polarity */
#define DB9000_CR1_DEP BIT(8)
/* Pixel Clock Polarity */
#define DB9000_CR1_PCP BIT(9)
/* Horizontal Sync Polarity */
#define DB9000_CR1_HSP BIT(10)
/* Vertical Sync Polarity */
#define DB9000_CR1_VSP BIT(11)
/* Output Pixel Select */
#define DB9000_CR1_OPS(x) (((x) & 0x7) << 12)
/* FIFO DMA Request Words */
#define DB9000_CR1_FDW(x) ((x) << 16)
/* LCD 1 or Port Select */
#define DB9000_CR1_LPS BIT(18)
/* Frame Buffer 24bpp Packed Word */
#define DB9000_CR1_FBP BIT(19)
#define DB9000_CR1_HSS(x) ((x) << 24)

#define CR1_TEST_SET_A_1280                                  \
	(CLR_REG_PTRN | DB9000_CR1_HSS(1) | DB9000_CR1_FBP | \
	 DB9000_CR1_FDW(1) | DB9000_CR1_DEP | DB9000_CR1_BPP(6))

#define CR1_TEST_SET_A                                         \
	(CLR_REG_PTRN | DB9000_CR1_HSS(1) | DB9000_CR1_FBP |   \
	 DB9000_CR1_FDW(1) | DB9000_CR1_VSP | DB9000_CR1_HSP | \
	 DB9000_CR1_DEP | DB9000_CR1_BPP(6))

#define CR1_TEST_SET_B                                          \
	(CLR_REG_PTRN | DB9000_CR1_HSS(1) | DB9000_CR1_FDW(1) | \
	 DB9000_CR1_DEP | DB9000_CR1_RGB | DB9000_CR1_BPP(6) |  \
	 DB9000_CR1_LPE | DB9000_CR1_LCE)

struct fps_info {
	unsigned int counter;
	ktime_t last_timestamp;
};

struct db9000 {
	struct device *dev;
	void __iomem *regs;
	void __iomem *data_reg;
	spinlock_t lock;
	struct clk *lcd_eclk;
	struct fps_info plane_fpsi[DB9000_MAX_LAYER];
	struct drm_atomic_state *suspend_state;
	struct dentry *debugfs;
	u8 bpp;
	u32 paddr_disp;
	u32 irqstatus;
	int bus_width;
	unsigned int frame_size;
};

enum DB9000_CR1_BPP {
	/* 1 bit per pixel */
	DB9000_CR1_BPP_1bpp,
	/* 2 bits per pixel */
	DB9000_CR1_BPP_2bpp,
	/* 4 bits per pixel */
	DB9000_CR1_BPP_4bpp,
	/* 8 bits per pixel */
	DB9000_CR1_BPP_8bpp,
	/* 16 bits per pixel */
	DB9000_CR1_BPP_16bpp,
	/* 18 bits per pixel */
	DB9000_CR1_BPP_18bpp,
	/* 24 bits per pixel */
	DB9000_CR1_BPP_24bpp
} DB9000_CR1_BPP_VAL;

#define DB9000_REG_OFFSET 0x04
#define DB9000_DEF_VAL 0x00000000
/* Horizontal Timing Register, Offset 0x008 */
/* Horizontal Front Porch */
#define DB9000_HTR_HFP(x) ((x) << 0)
/* Pixels per Line */
#define DB9000_HTR_PPL(x) ((x) << 8)
/* Horizontal Back Porch */
#define DB9000_HTR_HBP(x) ((x) << 16)
/* Horizontal Sync Width */
#define DB9000_HTR_HSW(x) ((x) << 24)

#define SET_HTR_PARM                                              \
	(CLR_REG_PTRN | DB9000_HTR_HSW(95) | DB9000_HTR_HBP(48) | \
	 DB9000_HTR_PPL(40) | DB9000_HTR_HFP(16))
/* < Value for Horizontal Timing Register */

#define SET_HTR_PARM_1280                                          \
	(CLR_REG_PTRN | DB9000_HTR_HSW(39) | DB9000_HTR_HBP(220) | \
	 DB9000_HTR_PPL(80) |                                      \
	 DB9000_HTR_HFP(110)) /**< Value for Horizontal Timing Register */

/* Vertical Timing Register 1, Offset 0x00C */
/* Vertical Sync Width */
#define DB9000_VTR1_VSW(x) ((x) << 0)
/* Vertical Front Porch */
#define DB9000_VTR1_VFP(x) ((x) << 8)
/* Vertical Back Porch */
#define DB9000_VTR1_VBP(x) ((x) << 16)

#define SET_VTR1_PARM                                               \
	(CLR_REG_PTRN | DB9000_VTR1_VBP(33) | DB9000_VTR1_VFP(10) | \
	 DB9000_VTR1_VSW(2))
/**< Value for Vertical Timing Register 1 */

#define SET_VTR1_PARM_1280                                         \
	(CLR_REG_PTRN | DB9000_VTR1_VBP(20) | DB9000_VTR1_VFP(5) | \
	 DB9000_VTR1_VSW(5)) /**< Value for Vertical Timing Register 1 */

/* Vertical Timing Register 2, Offset 0x010 */
/* Lines Per Panel */
#define DB9000_VTR2_LPP(x) ((x) << 0)

#define SET_VTR2_PARM_1280 \
	(CLR_REG_PTRN |    \
	 DB9000_VTR2_LPP(720)) /**< Value for Vertical Timing Register 2 */

/**< Value for Vertical Timing Register 2 */
#define SET_VTR2_PARM (CLR_REG_PTRN | DB9000_VTR2_LPP(480))

/* Vertical and Horizontal Timing Extension Register, Offset 0x044 */
/* Horizontal Front Porch Extension */
#define DB9000_HVTER_HFPE(x) ((((x) >> 8) & 0x3) << 0)
/* Horizontal Back Porch Extension */
#define DB9000_HVTER_HBPE(x) ((((x) >> 8) & 0x3) << 4)
/* Vertical Front Porch Extension */
#define DB9000_HVTER_VFPE(x) ((((x) >> 8) & 0x3) << 8)
/* Vertical Back Porch Extension */
#define DB9000_HVTER_VBPE(x) ((((x) >> 8) & 0x3) << 12)

/* clock reset select */
#define DB9000_PCTR_PCR BIT(10)
#define DB9000_PCTR_PCI BIT(9)
/* Interrupt Status Register, Offset 0x018 */
#define DB9000_ISR_OFU BIT(0) /* Output FIFO Underrun */
#define DB9000_ISR_OFO BIT(1) /* Output FIFO Overrun */
#define DB9000_ISR_IFU BIT(2) /* Input FIFO Underrun */
#define DB9000_ISR_IFO BIT(3) /* Input FIFO Overrun */
#define DB9000_ISR_FER BIT(4) /* OR of OFU, OFO, IFU, IFO */
#define DB9000_ISR_MBE BIT(5) /* Master Bus Error */
#define DB9000_ISR_VCT BIT(6) /* Vertical Compare Triggered */
#define DB9000_ISR_BAU BIT(7) /* DMA Base Address Register Update to CAR */
#define DB9000_ISR_LDD BIT(8) /* LCD Controller Disable Done */

/* Interrupt Mask Register, Offset 0x01C */
#define DB9000_IMR_OFUM BIT(0) /* Output FIFO Underrun - Mask */
#define DB9000_IMR_OFOM BIT(1) /* Output FIFO Overrun - Mask */
#define DB9000_IMR_IFUM BIT(2) /* Input FIFO Underrun - Mask */
#define DB9000_IMR_IFOM BIT(3) /* Input FIFO Overrun - Mask */
#define DB9000_IMR_FERM BIT(4) /* OR of OFU, OFO, IFU, IFO - Mask */
#define DB9000_IMR_MBEM BIT(5) /* Master Bus Error - Mask */
#define DB9000_IMR_VCTM BIT(6) /* Vertical Compare Triggered - Mask */
/* DMA Base Address Register Update to CAR - Mask */
#define DB9000_IMR_BAUM BIT(7)
#define DB9000_IMR_LDDM BIT(8) /* LCD Controller Disable Done - Mask */

/* Interrupt Scan Computr Register , offset 0x024 */
#define DB9000_ISCR_OFU BIT(0) /* Output FIFO Underrun */
#define DB9000_ISCR_OFO BIT(1) /* Output FIFO Overrun */
#define DB9000_ISCR_IFU BIT(2) /* Input FIFO Underrun */
#define DB9000_ISCR_IFO BIT(3) /* Input FIFO Overrun */
#define DB9000_ISCR_FER BIT(4) /* OR of OFU, OFO, IFU, IFO */
#define DB9000_ISCR_MBE BIT(5) /* Master Bus Error */
#define DB9000_ISCR_VCT BIT(6) /* Vertical Compare Triggered */
#define DB9000_ISCR_BAU BIT(7) /* DMA Base Address Register Update to CAR */
#define DB9000_ISCR_LDD BIT(8) /* LCD Controller Disable Done */

/* PWM Frequency Register */
#define DB9000_PWMFR_0 0x034
#define DB9000_PWMFR_RZN1 0x04C
/* PWM Duty Cycle Register */
#define DB9000_PWMDCR_0 0x038
#define DB9000_PWMDCR_RZN1 0x050
/* PWM Frequency Registers, Offset 0x034 and 0x04c */
#define DB9000_PWMFR_PWMFCD(x) (((x) & 0x3fffff) << 0)
#define DB9000_PWMFR_PWMFCE BIT(22)

#define DB9000_CR1_VAL 0x01090118
#define DB9000_CR1_VAL2 0x0101011B
#define DB9000_CR1_VAL3 0x0101011B
#define DB9000_HTR_VAL 0x27DC506E
#define DB9000_VTR1_VAL 0x00140505
#define DB9000_VTR2_VAL 0x000002D0
#define DB9000_IMR_VAL 0x000000f0
#define DB9000_IMR_VAL2 0x00000000
#define DB9000_ISCR_VAL 0x00000006
#define DB9000_ISCR_VAL2 0x00000000
#define DB9000_ISR_VAL 0x0000ffff
#define DB9000_PCTR_VAL 0x00000600
#define DB9000_BPP_16 16
#define DB9000_BPP_24 24
#endif /* __AXIADO_LCDC_H__ */
