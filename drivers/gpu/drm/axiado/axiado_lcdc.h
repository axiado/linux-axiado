/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2022-2026 Axiado Corporation.
 */

#ifndef __AXIADO_LCDC_H__
#define __AXIADO_LCDC_H__

#include <linux/debugfs.h>

#define DB9000_FB_MAX_WIDTH 4095
#define DB9000_FB_MAX_HEIGHT 4095

#define DB9000_MAX_LAYER 1
#define NB_PF 8
#define CLUT_SIZE 256
#define NB_CRTC 1
#define CRTC_MASK GENMASK(NB_CRTC - 1, 0)

#define CLR_REG_PTRN 0x00000000 /* Default Register Pattern */

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

/*
 * Core Identification Register 1 (TRM 8.66, CIR_REV ≥ 1.16).
 * Overlaps the older "GPIOR" address. Validate by checking the
 * model-number field equals 0x90 before trusting the bus-width field.
 */
#define DB9000_CIR1                0x1F8
#define DB9000_CIR1_MN_SHIFT       16
#define DB9000_CIR1_MN_MASK        (0xffu << DB9000_CIR1_MN_SHIFT)
#define DB9000_CIR1_MN_DB9000      0x90
#define DB9000_CIR1_BW_SHIFT       12
#define DB9000_CIR1_BW_MASK        (0xfu << DB9000_CIR1_BW_SHIFT)
#define DB9000_CIR1_BW_32          0x3
#define DB9000_CIR1_BW_64          0x4
#define DB9000_CIR1_BW_128         0x5
#define DB9000_CIR1_BW_256         0x6

/*
 * Multiple Memory Reads Request Register (TRM 8.68).
 * MRR[1:0]: 00=1, 01=2, 10=4 outstanding AXI read requests.
 * DEAR_MRR[31:3]: lookahead end-address — DMAC stops generating new
 * bursts once it reaches this address so the (MRR-1) reads still in
 * flight have room to complete up to DEAR without overrunning.
 */
#define DB9000_MRR                 0xFFC
#define DB9000_MRR_MRR_MASK        0x3
#define DB9000_MRR_1_READ          0x0
#define DB9000_MRR_2_READS         0x1
#define DB9000_MRR_4_READS         0x2
#define DB9000_MRR_DEAR_MASK       0xfffffff8u

/* Control Register 1, Offset 0x000 */
#define DB9000_CR1_LCE BIT(0)
#define DB9000_CR1_LPE BIT(1)
#define DB9000_CR1_BPP(x) (((x)&0x7) << 2)
#define DB9000_CR1_RGB BIT(5)
#define DB9000_CR1_VBLANK BIT(6)
#define DB9000_CR1_DEP BIT(8)
#define DB9000_CR1_PCP BIT(9)
#define DB9000_CR1_HSP BIT(10)
#define DB9000_CR1_VSP BIT(11)
#define DB9000_CR1_OPS(x) (((x)&0x7) << 12)
#define DB9000_CR1_FDW(x) (((x) & 0x3) << 16)
#define DB9000_CR1_LPS BIT(18)
#define DB9000_CR1_FBP BIT(19)
#define DB9000_CR1_HSS(x) (((x) & 0x3) << 24)

struct db9000 {
	struct device *dev;
	void __iomem *regs;
	spinlock_t lock;
	struct clk *lcd_eclk;
	struct drm_crtc crtc;
	struct drm_plane plane;
	struct dentry *debugfs;
	u8 bpp;
	int bus_width;
	unsigned int frame_size;

	/*
	 * Bytes per FIFO refill burst — (master AXI width / 8) * FDW beats.
	 * Used to derive DEAR_MRR for the MRR (multiple outstanding reads)
	 * lookahead. Set from CIR1 at bind time.
	 */
	u32 dma_burst_bytes;
};

enum db9000_cr1_bpp {
	DB9000_CR1_BPP_1,
	DB9000_CR1_BPP_2,
	DB9000_CR1_BPP_4,
	DB9000_CR1_BPP_8,
	DB9000_CR1_BPP_16,
	DB9000_CR1_BPP_18,
	DB9000_CR1_BPP_24,
};

#define DB9000_REG_OFFSET 0x04
#define DB9000_DEF_VAL 0x00000000

/* Horizontal Timing Register, Offset 0x008 */
#define DB9000_HTR_HSW(x) (((x)&0xFF) << 24)
#define DB9000_HTR_HBP(x) (((x)&0xFF) << 16)
#define DB9000_HTR_PPL(x) (((x)&0xFF) << 8)
#define DB9000_HTR_HFP(x) (((x)&0xFF) << 0)

/* Vertical Timing Register 1, Offset 0x00C */
#define DB9000_VTR1_VSW(x) (((x)&0xFF) << 0)
#define DB9000_VTR1_VFP(x) (((x)&0xFF) << 8)
#define DB9000_VTR1_VBP(x) (((x)&0xFF) << 16)

/* Vertical Timing Register 2, Offset 0x010 */
#define DB9000_VTR2_LPP(x) (((x)&0xFFF) << 0)

/* Vertical and Horizontal Timing Extension Register, Offset 0x044 */
#define DB9000_HVTER_HFPE(x) ((((x) >> 8) & 0x3) << 0)
#define DB9000_HVTER_HBPE(x) ((((x) >> 8) & 0x3) << 4)
#define DB9000_HVTER_VFPE(x) ((((x) >> 8) & 0x3) << 8)
#define DB9000_HVTER_VBPE(x) ((((x) >> 8) & 0x3) << 12)

/* clock reset select */
#define DB9000_PCTR_PCR BIT(10)
#define DB9000_PCTR_PCI BIT(9)

/* Interrupt Status Register, Offset 0x018 */
#define DB9000_ISR_OFU BIT(0)
#define DB9000_ISR_OFO BIT(1)
#define DB9000_ISR_IFU BIT(2)
#define DB9000_ISR_IFO BIT(3)
#define DB9000_ISR_FER BIT(4)
#define DB9000_ISR_MBE BIT(5)
#define DB9000_ISR_VCT BIT(6)
#define DB9000_ISR_BAU BIT(7)
#define DB9000_ISR_LDD BIT(8)

/* Interrupt Mask Register, Offset 0x01C */
#define DB9000_IMR_OFUM BIT(0)
#define DB9000_IMR_OFOM BIT(1)
#define DB9000_IMR_IFUM BIT(2)
#define DB9000_IMR_IFOM BIT(3)
#define DB9000_IMR_FERM BIT(4)
#define DB9000_IMR_MBEM BIT(5)
#define DB9000_IMR_VCTM BIT(6)
#define DB9000_IMR_BAUM BIT(7)
#define DB9000_IMR_LDDM BIT(8)

/* Interrupt Scan Compare Register, offset 0x024 */
#define DB9000_ISCR_OFU BIT(0)
#define DB9000_ISCR_OFO BIT(1)
#define DB9000_ISCR_IFU BIT(2)
#define DB9000_ISCR_IFO BIT(3)
#define DB9000_ISCR_FER BIT(4)
#define DB9000_ISCR_MBE BIT(5)
#define DB9000_ISCR_VCT BIT(6)
#define DB9000_ISCR_BAU BIT(7)
#define DB9000_ISCR_LDD BIT(8)

#define DB9000_PWMFR_0 0x034
#define DB9000_PWMFR_RZN1 0x04C
#define DB9000_PWMDCR_0 0x038
#define DB9000_PWMDCR_RZN1 0x050
#define DB9000_PWMFR_PWMFCD(x) (((x)&0x3fffff) << 0)
#define DB9000_PWMFR_PWMFCE BIT(22)

/*
 * All IRQ mask bits set: every ISR source is gated off the GIC line.
 * This is the "quiet" state — vblank is unmasked on demand by
 * .enable_vblank, all the error/status bits stay masked.
 *
 * Without this, FIFO underrun (OFU), FIFO error (FER) and base-addr-
 * unhandled (BAU) re-assert continuously when the LCDC is driven at
 * less than its design pixel clock (currently 74.25 MHz against a
 * 148.5 MHz mode). They keep the IRQ line stuck high, so the GIC
 * never sees a fresh edge for the real vblank and atomic modeset
 * waits time out.
 */
#define DB9000_IMR_ALL_MASKED 0x000001ff
#define DB9000_IMR_VAL2 DB9000_IMR_ALL_MASKED
#define DB9000_ISCR_VAL 0x00000006
#define DB9000_ISR_VAL 0x0000ffff
#define DB9000_PCTR_VAL 0x00000600
#define DB9000_BPP_16 16
#define DB9000_BPP_24 24

#endif /* __AXIADO_LCDC_H__ */
