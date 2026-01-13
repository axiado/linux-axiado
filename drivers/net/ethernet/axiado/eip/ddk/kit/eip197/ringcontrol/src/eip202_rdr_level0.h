/* eip202_rdr_level0.h
 *
 * EIP-202 internal interface: RDR Manager
 */

/* -------------------------------------------------------------------------- */
/*                                                                            */
/*   Module        : ddk197                                                   */
/*   Version       : 5.7.2                                                    */
/*   configuration : DDK-197-BSD                                              */
/*                                                                            */
/*   Date          : 2025-Nov-13                                              */
/*                                                                            */
/* Copyright (c) 2008-2025 by Rambus, Inc. and/or its subsidiaries.           */
/*                                                                            */
/* Redistribution and use in source and binary forms, with or without         */
/* modification, are permitted provided that the following conditions are     */
/* met:                                                                       */
/*                                                                            */
/* 1. Redistributions of source code must retain the above copyright          */
/* notice, this list of conditions and the following disclaimer.              */
/*                                                                            */
/* 2. Redistributions in binary form must reproduce the above copyright       */
/* notice, this list of conditions and the following disclaimer in the        */
/* documentation and/or other materials provided with the distribution.       */
/*                                                                            */
/* 3. Neither the name of the copyright holder nor the names of its           */
/* contributors may be used to endorse or promote products derived from       */
/* this software without specific prior written permission.                   */
/*                                                                            */
/* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS        */
/* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT          */
/* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR      */
/* A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT       */
/* HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,     */
/* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT           */
/* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,      */
/* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY      */
/* THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT        */
/* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE      */
/* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.       */
/* -------------------------------------------------------------------------- */

#ifndef EIP202_RDR_LEVEL0_H_
#define EIP202_RDR_LEVEL0_H_


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Default configuration
#include "c_eip202_ring.h"

// Driver Framework Basic Definitions API
#include "basic_defs.h"         // u8, u32

// Driver Framework device API
#include "device_types.h"       // device_handle_t
#include "device_rw.h"          // Read32, Write32


/*----------------------------------------------------------------------------
 * Definitions and macros
 */

// Read/Write register constants

/*****************************************************************************
 * byte offsets of the EIP-202 HIA RDR registers
 *****************************************************************************/

#define EIP202_RDR_OFFS           4

// 2 KB MMIO space for one RDR instance
#define EIP202_RDR_BASE                   0x00000800
#define EIP202_RDR_RING_BASE_ADDR_LO      ((EIP202_RDR_BASE) + \
                                            (0x00 * EIP202_RDR_OFFS))
#define EIP202_RDR_RING_BASE_ADDR_HI      ((EIP202_RDR_BASE) + \
                                            (0x01 * EIP202_RDR_OFFS))
#define EIP202_RDR_DATA_BASE_ADDR_LO      ((EIP202_RDR_BASE) + \
                                            (0x02 * EIP202_RDR_OFFS))
#define EIP202_RDR_DATA_BASE_ADDR_HI      ((EIP202_RDR_BASE) + \
                                            (0x03 * EIP202_RDR_OFFS))
#define EIP202_RDR_RING_SIZE              ((EIP202_RDR_BASE) + \
                                            (0x06 * EIP202_RDR_OFFS))
#define EIP202_RDR_DESC_SIZE              ((EIP202_RDR_BASE) + \
                                            (0x07 * EIP202_RDR_OFFS))
#define EIP202_RDR_CFG                    ((EIP202_RDR_BASE) + \
                                            (0x08 * EIP202_RDR_OFFS))
#define EIP202_RDR_DMA_CFG                ((EIP202_RDR_BASE) + \
                                            (0x09 * EIP202_RDR_OFFS))
#define EIP202_RDR_THRESH                 ((EIP202_RDR_BASE) + \
                                            (0x0A * EIP202_RDR_OFFS))
#define EIP202_RDR_PREP_COUNT             ((EIP202_RDR_BASE) + \
                                            (0x0B * EIP202_RDR_OFFS))
#define EIP202_RDR_PROC_COUNT             ((EIP202_RDR_BASE) + \
                                            (0x0C * EIP202_RDR_OFFS))
#define EIP202_RDR_PREP_PNTR              ((EIP202_RDR_BASE) + \
                                            (0x0D * EIP202_RDR_OFFS))
#define EIP202_RDR_PROC_PNTR              ((EIP202_RDR_BASE) + \
                                            (0x0E * EIP202_RDR_OFFS))
#define EIP202_RDR_STAT                   ((EIP202_RDR_BASE) + \
                                            (0x0F * EIP202_RDR_OFFS))

// Default EIP202_RDR_x register values
#define EIP202_RDR_RING_BASE_ADDR_LO_DEFAULT       0x00000000
#define EIP202_RDR_RING_BASE_ADDR_HI_DEFAULT       0x00000000
#define EIP202_RDR_DATA_BASE_ADDR_LO_DEFAULT       0x00000000
#define EIP202_RDR_DATA_BASE_ADDR_HI_DEFAULT       0x00000000
#define EIP202_RDR_RING_SIZE_DEFAULT               0x00000000
#define EIP202_RDR_DESC_SIZE_DEFAULT               0x00000000
#define EIP202_RDR_CFG_DEFAULT                     0x00000000
#define EIP202_RDR_DMA_CFG_DEFAULT                 0x01800000
#define EIP202_RDR_THRESH_DEFAULT                  0x00000000
#define EIP202_RDR_PREP_COUNT_DEFAULT              0x00000000
#define EIP202_RDR_PROC_COUNT_DEFAULT              0x00000000
#define EIP202_RDR_PREP_PNTR_DEFAULT               0x00000000
#define EIP202_RDR_PROC_PNTR_DEFAULT               0x00000000
// Ignore the RD fifo size after reset for this register default value for now
#define EIP202_RDR_STAT_DEFAULT                    0x00000000


/*----------------------------------------------------------------------------
 * Local variables
 */


/*----------------------------------------------------------------------------
 * eip202_rdr_read32
 *
 * This routine writes to a Register location in the EIP-202 RDR.
 */
static inline u32
eip202_rdr_read32(
        device_handle_t device,
        const unsigned int offset)
{
    return device_read32(device, offset);
}


/*----------------------------------------------------------------------------
 * eip202_rdr_write32
 *
 * This routine writes to a Register location in the EIP-202 RDR.
 */
static inline void
eip202_rdr_write32(
        device_handle_t device,
        const unsigned int offset,
        const u32 value)
{
    device_write32(device, offset, value);
}


static inline void
EIP202_RDR_RING_BASE_ADDR_LO_DEFAULT_WR(
        device_handle_t device)
{
    eip202_rdr_write32(device,
                       EIP202_RDR_RING_BASE_ADDR_LO,
                       EIP202_RDR_RING_BASE_ADDR_LO_DEFAULT);
}


static inline void
EIP202_RDR_RING_BASE_ADDR_LO_WR(
        device_handle_t device,
        const u32 low_addr)
{
    eip202_rdr_write32(device, EIP202_RDR_RING_BASE_ADDR_LO, low_addr);
}


static inline void
EIP202_RDR_RING_BASE_ADDR_HI_DEFAULT_WR(
        device_handle_t device)
{
    eip202_rdr_write32(device,
                       EIP202_RDR_RING_BASE_ADDR_HI,
                       EIP202_RDR_RING_BASE_ADDR_HI_DEFAULT);
}


static inline void
EIP202_RDR_RING_BASE_ADDR_HI_WR(
        device_handle_t device,
        const u32 hi_addr)
{
    eip202_rdr_write32(device, EIP202_RDR_RING_BASE_ADDR_HI, hi_addr);
}


static inline void
EIP202_RDR_RING_SIZE_DEFAULT_WR(
        device_handle_t device)
{
    eip202_rdr_write32(device,
                       EIP202_RDR_RING_SIZE,
                       EIP202_RDR_RING_SIZE_DEFAULT);
}


static inline void
EIP202_RDR_RING_SIZE_WR(
        device_handle_t device,
        const u32 rdr_word_count)
{
    u32 reg_val = EIP202_RDR_RING_SIZE_DEFAULT;

    reg_val |= ((((u32)rdr_word_count) & MASK_22_BITS) << 2);

    eip202_rdr_write32(device, EIP202_RDR_RING_SIZE, reg_val);
}


static inline void
EIP202_RDR_DESC_SIZE_DEFAULT_WR(
        device_handle_t device)
{
    eip202_rdr_write32(device,
                       EIP202_RDR_DESC_SIZE,
                       EIP202_RDR_DESC_SIZE_DEFAULT);
}


static inline void
EIP202_RDR_DESC_SIZE_WR(
        device_handle_t device,
        const u8 dscr_word_count,
        const u8 dscr_offs_word_count,
        const bool f64bit)
{
    u32 reg_val = EIP202_RDR_DESC_SIZE_DEFAULT;

    if(f64bit)
        reg_val |= BIT_31;

    reg_val |= ((((u32)dscr_offs_word_count) & MASK_8_BITS) << 16);
    reg_val |= ((((u32)dscr_word_count)     & MASK_8_BITS)      );

    eip202_rdr_write32(device, EIP202_RDR_DESC_SIZE, reg_val);
}


static inline void
EIP202_RDR_CFG_DEFAULT_WR(
        device_handle_t device)
{
    eip202_rdr_write32(device,
                       EIP202_RDR_CFG,
                       EIP202_RDR_CFG_DEFAULT);
}


static inline void
EIP202_RDR_CFG_WR(
        device_handle_t device,
        const u16 rd_fetch_word_count,
        const u16 rd_fetch_thresh_word_count,
        const bool f_own_enable)
{
    u32 reg_val = EIP202_RDR_CFG_DEFAULT;

    if(f_own_enable)
        reg_val |= BIT_31;

    reg_val |= ((((u32)rd_fetch_thresh_word_count) & MASK_12_BITS)  << 16);
    reg_val |= ((((u32)rd_fetch_word_count)       & MASK_16_BITS)      );

    eip202_rdr_write32(device, EIP202_RDR_CFG, reg_val);
}


static inline void
EIP202_RDR_DMA_CFG_DEFAULT_WR(
        device_handle_t device)
{
    eip202_rdr_write32(device,
                       EIP202_RDR_DMA_CFG,
                       EIP202_RDR_DMA_CFG_DEFAULT);
}


static inline void
EIP202_RDR_DMA_CFG_WR(
        device_handle_t device,
        const u8 rd_swap,
        const u8 data_swap,
        const bool f_res_buf,
        const bool f_ctrl_buf,
        const bool f_own_buf,
        const u8 wr_cache,
        const u8 rd_cache,
        const u8 rd_protection,
        const u8 data_protection,
        const bool f_pad_to_offset)
{
    u32 reg_val = EIP202_RDR_DMA_CFG_DEFAULT;

    if(f_pad_to_offset)
        reg_val |= BIT_28;

    reg_val &= (~BIT_24);
    if(f_own_buf)
        reg_val |= BIT_24;

    reg_val &= (~BIT_23);
    if(f_ctrl_buf)
        reg_val |= BIT_23;

    if(f_res_buf)
        reg_val |= BIT_22;

    reg_val |= ((((u32)rd_cache)        & MASK_3_BITS) << 29);
    reg_val |= ((((u32)wr_cache)        & MASK_3_BITS) << 25);
    reg_val |= ((((u32)data_protection) & MASK_4_BITS) << 12);
    reg_val |= ((((u32)data_swap)       & MASK_4_BITS) << 8);
    reg_val |= ((((u32)rd_protection)   & MASK_4_BITS) << 4);
    reg_val |= ((((u32)rd_swap)         & MASK_4_BITS));

    eip202_rdr_write32(device, EIP202_RDR_DMA_CFG, reg_val);
}


static inline void
EIP202_RDR_THRESH_DEFAULT_WR(
        device_handle_t device)
{
    eip202_rdr_write32(device,
                       EIP202_RDR_THRESH,
                       EIP202_RDR_THRESH_DEFAULT);
}


static inline void
EIP202_RDR_THRESH_WR(
        device_handle_t device,
        const u32 rd_thresh_word_count,
        const bool f_proc_pkt_mode,
        const u8 rd_timeout)
{
    u32 reg_val = EIP202_RDR_THRESH_DEFAULT;

    if(f_proc_pkt_mode)
        reg_val |= BIT_23;

    reg_val |= (rd_thresh_word_count      & MASK_22_BITS);
    reg_val |= ((((u32)rd_timeout) & MASK_8_BITS) << 24);

    eip202_rdr_write32(device, EIP202_RDR_THRESH, reg_val);
}


static inline void
EIP202_RDR_PREP_COUNT_DEFAULT_WR(
        device_handle_t device)
{
    eip202_rdr_write32(device,
                       EIP202_RDR_PREP_COUNT,
                       EIP202_RDR_PREP_COUNT_DEFAULT);
}


static inline void
EIP202_RDR_PREP_COUNT_WR(
        device_handle_t device,
        const u16 rd_count,
        const bool f_clear_count)
{
    u32 reg_val = EIP202_RDR_PREP_COUNT_DEFAULT;

    if(f_clear_count)
        reg_val |= BIT_31;

    reg_val |= ((((u32)rd_count) & MASK_14_BITS) << 2);

    eip202_rdr_write32(device, EIP202_RDR_PREP_COUNT, reg_val);
}


static inline void
EIP202_RDR_PREP_COUNT_RD(
        device_handle_t device,
        u32 * const prep_rd_word_count_p)
{
    u32 reg_val;

    reg_val = eip202_rdr_read32(device, EIP202_RDR_PREP_COUNT);

    *prep_rd_word_count_p   = (u32)((reg_val >> 2) & MASK_22_BITS);
}


static inline void
EIP202_RDR_PROC_COUNT_DEFAULT_WR(
        device_handle_t device)
{
    eip202_rdr_write32(device,
                       EIP202_RDR_PROC_COUNT,
                       EIP202_RDR_PROC_COUNT_DEFAULT);
}


static inline void
EIP202_RDR_PROC_COUNT_WR(
        device_handle_t device,
        const u16 rd_word_count,
        const u8 pkt_count,
        const bool f_clear_count)
{
    u32 reg_val = EIP202_RDR_PROC_COUNT_DEFAULT;

    if(f_clear_count)
        reg_val |= BIT_31;

    reg_val |= ((((u32)rd_word_count)  & MASK_14_BITS) << 2);
    reg_val |= ((((u32)pkt_count)     & MASK_7_BITS)  << 24);

    eip202_rdr_write32(device, EIP202_RDR_PROC_COUNT, reg_val);
}


static inline void
EIP202_RDR_PROC_COUNT_RD(
        device_handle_t device,
        u32 * const proc_rd_word_count_p,
        u8 * const proc_pkt_word_count_p)
{
    u32 reg_val;

    reg_val = eip202_rdr_read32(device, EIP202_RDR_PROC_COUNT);

    *proc_rd_word_count_p    = (u32)((reg_val >> 2)  & MASK_22_BITS);
    *proc_pkt_word_count_p   =  (u8)((reg_val >> 24) & MASK_7_BITS);
}


static inline void
EIP202_RDR_PREP_PNTR_DEFAULT_WR(
        device_handle_t device)
{
    eip202_rdr_write32(device,
                       EIP202_RDR_PREP_PNTR,
                       EIP202_RDR_PREP_PNTR_DEFAULT);
}


static inline void
EIP202_RDR_PROC_PNTR_DEFAULT_WR(
        device_handle_t device)
{
    eip202_rdr_write32(device,
                       EIP202_RDR_PROC_PNTR,
                       EIP202_RDR_PROC_PNTR_DEFAULT);
}


static inline void
EIP202_RDR_STAT_RD(
        device_handle_t device,
        bool * const f_dma_error_irq,
        bool * const f_tresh_irq,
        bool * const f_error_irq,
        bool * const f_ouflow_irq,
        bool * const f_timeout_irq,
        bool * const f_buf_oflo_irq,
        bool * const f_proc_oflo_irq,
        u16 * const rdfifo_word_count)
{
    u32 reg_val;

    reg_val = eip202_rdr_read32(device, EIP202_RDR_STAT);

    *f_dma_error_irq      = ((reg_val & BIT_0) != 0);
    *f_error_irq         = ((reg_val & BIT_2) != 0);
    *f_ouflow_irq        = ((reg_val & BIT_3) != 0);
    *f_tresh_irq         = ((reg_val & BIT_4) != 0);
    *f_timeout_irq       = ((reg_val & BIT_5) != 0);
    *f_buf_oflo_irq       = ((reg_val & BIT_6) != 0);
    *f_proc_oflo_irq      = ((reg_val & BIT_7) != 0);
    *rdfifo_word_count   = (u16)((reg_val >> 16) & MASK_12_BITS);
}


static inline void
EIP202_RDR_STAT_FIFO_SIZE_RD(
        device_handle_t device,
        u16 * const rdfifo_word_count)
{
    u32 reg_val;

    reg_val = eip202_rdr_read32(device, EIP202_RDR_STAT);

    *rdfifo_word_count   = (u16)((reg_val >> 16) & MASK_12_BITS);
}


static inline void
EIP202_RDR_STAT_CLEAR_ALL_IRQ_WR(
        device_handle_t device)
{
    eip202_rdr_write32(device,
                       EIP202_RDR_STAT,
                       (u32)(EIP202_RDR_STAT_DEFAULT | MASK_8_BITS));
}


static inline void
EIP202_RDR_STAT_CLEAR_DSCR_BUF_OFLO_IRQ_WR(
        device_handle_t device)
{
    eip202_rdr_write32(device,
                       EIP202_RDR_STAT,
                       (u32)(EIP202_RDR_STAT_DEFAULT | BIT_7 | BIT_6));
}


#endif /* EIP202_RDR_LEVEL0_H_ */


/* end of file eip202_rdr_level0.h */
