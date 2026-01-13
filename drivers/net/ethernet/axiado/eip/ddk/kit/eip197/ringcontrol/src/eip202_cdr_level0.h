/* eip202_cdr_level0.h
 *
 * EIP-202 internal interface
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

#ifndef EIP202_CDR_LEVEL0_H_
#define EIP202_CDR_LEVEL0_H_


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
#include "device_swap.h"        // device_swap_endian32


/*----------------------------------------------------------------------------
 * Definitions and macros
 */

// Read/Write register constants

/*****************************************************************************
 * byte offsets of the EIP-202 HIA CDR registers
 *****************************************************************************/

#define EIP202_CDR_OFFS           4

// 2 KB MMIO space for one CDR instance
#define EIP202_CDR_BASE                   0x00000000
#define EIP202_CDR_RING_BASE_ADDR_LO      ((EIP202_CDR_BASE) + \
                                            (0x00 * EIP202_CDR_OFFS))
#define EIP202_CDR_RING_BASE_ADDR_HI      ((EIP202_CDR_BASE) + \
                                            (0x01 * EIP202_CDR_OFFS))
#define EIP202_CDR_DATA_BASE_ADDR_LO      ((EIP202_CDR_BASE) + \
                                            (0x02 * EIP202_CDR_OFFS))
#define EIP202_CDR_DATA_BASE_ADDR_HI      ((EIP202_CDR_BASE) + \
                                            (0x03 * EIP202_CDR_OFFS))
#define EIP202_CDR_ATOK_BASE_ADDR_LO      ((EIP202_CDR_BASE) + \
                                            (0x04 * EIP202_CDR_OFFS))
#define EIP202_CDR_ATOK_BASE_ADDR_HI      ((EIP202_CDR_BASE) + \
                                            (0x05 * EIP202_CDR_OFFS))
#define EIP202_CDR_RING_SIZE              ((EIP202_CDR_BASE) + \
                                            (0x06 * EIP202_CDR_OFFS))
#define EIP202_CDR_DESC_SIZE              ((EIP202_CDR_BASE) + \
                                            (0x07 * EIP202_CDR_OFFS))
#define EIP202_CDR_CFG                    ((EIP202_CDR_BASE) + \
                                            (0x08 * EIP202_CDR_OFFS))
#define EIP202_CDR_DMA_CFG                ((EIP202_CDR_BASE) + \
                                            (0x09 * EIP202_CDR_OFFS))
#define EIP202_CDR_THRESH                 ((EIP202_CDR_BASE) + \
                                            (0x0A * EIP202_CDR_OFFS))
#define EIP202_CDR_COUNT                  ((EIP202_CDR_BASE) + \
                                            (0x0B * EIP202_CDR_OFFS))
#define EIP202_CDR_PROC_COUNT             ((EIP202_CDR_BASE) + \
                                            (0x0C * EIP202_CDR_OFFS))
#define EIP202_CDR_POINTER                ((EIP202_CDR_BASE) + \
                                            (0x0D * EIP202_CDR_OFFS))
#define EIP202_CDR_STAT                   ((EIP202_CDR_BASE) + \
                                            (0x0F * EIP202_CDR_OFFS))
#define EIP202_CDR_OPTIONS                ((EIP202_CDR_BASE) + \
                                            (0x1FE* EIP202_CDR_OFFS))
#define EIP202_CDR_VERSION                ((EIP202_CDR_BASE) + \
                                            (0x1FF * EIP202_CDR_OFFS))

// EIP-202 HIA EIP number (0xCA) and complement (0x35)
#define EIP202_CDR_SIGNATURE              ((u16)0x35CA)


// Default EIP202_CDR_x register values
#define EIP202_CDR_RING_BASE_ADDR_LO_DEFAULT       0x00000000
#define EIP202_CDR_RING_BASE_ADDR_HI_DEFAULT       0x00000000
#define EIP202_CDR_DATA_BASE_ADDR_LO_DEFAULT       0x00000000
#define EIP202_CDR_DATA_BASE_ADDR_HI_DEFAULT       0x00000000
#define EIP202_CDR_ATOK_BASE_ADDR_LO_DEFAULT       0x00000000
#define EIP202_CDR_ATOK_BASE_ADDR_HI_DEFAULT       0x00000000
#define EIP202_CDR_RING_SIZE_DEFAULT               0x00000000
#define EIP202_CDR_DESC_SIZE_DEFAULT               0x00000000
#define EIP202_CDR_CFG_DEFAULT                     0x00000000
#define EIP202_CDR_DMA_CFG_DEFAULT                 0x01000000
#define EIP202_CDR_THRESH_DEFAULT                  0x00000000
#define EIP202_CDR_COUNT_DEFAULT                   0x00000000
#define EIP202_CDR_PROC_COUNT_DEFAULT              0x00000000
#define EIP202_CDR_POINTER_DEFAULT                 0x00000000
// Ignore the CD fifo size after reset for this register default value for now
#define EIP202_CDR_STAT_DEFAULT                    0x00000000


/*----------------------------------------------------------------------------
 * Local variables
 */


/*----------------------------------------------------------------------------
 * eip202_cdr_read32
 *
 * This routine writes to a Register location in the EIP-202 CDR.
 */
static inline u32
eip202_cdr_read32(
        device_handle_t device,
        const unsigned int offset)
{
    return device_read32(device, offset);
}


/*----------------------------------------------------------------------------
 * eip202_cdr_write32
 *
 * This routine writes to a Register location in the EIP-202 CDR.
 */
static inline void
eip202_cdr_write32(
        device_handle_t device,
        const unsigned int offset,
        const u32 value)
{
    device_write32(device, offset, value);
}


static inline void
EIP202_CDR_STAT_RD(
        device_handle_t device,
        bool * const f_dma_error_irq,
        bool * const f_tresh_irq,
        bool * const f_error_irq,
        bool * const f_ouflow_irq,
        bool * const f_timeout_irq,
        u16 * const cdfifo_word_count)
{
    u32 reg_val;

    reg_val = eip202_cdr_read32(device, EIP202_CDR_STAT);

    *f_dma_error_irq      = ((reg_val & BIT_0) != 0);
    *f_tresh_irq         = ((reg_val & BIT_1) != 0);
    *f_error_irq         = ((reg_val & BIT_2) != 0);
    *f_ouflow_irq        = ((reg_val & BIT_3) != 0);
    *f_timeout_irq       = ((reg_val & BIT_5) != 0);
    *cdfifo_word_count   = (u16)((reg_val >> 16) & MASK_12_BITS);
}


static inline void
EIP202_CDR_STAT_FIFO_SIZE_RD(
        device_handle_t device,
        u16 * const cdfifo_word_count)
{
    u32 reg_val;

    reg_val = eip202_cdr_read32(device, EIP202_CDR_STAT);

    *cdfifo_word_count   = (u16)((reg_val >> 16) & MASK_12_BITS);
}


static inline void
EIP202_CDR_STAT_CLEAR_ALL_IRQ_WR(
        device_handle_t device)
{
    eip202_cdr_write32(device,
                       EIP202_CDR_STAT,
                       (u32)(EIP202_CDR_STAT_DEFAULT | MASK_5_BITS));
}


static inline void
EIP202_CDR_RING_BASE_ADDR_LO_DEFAULT_WR(
        device_handle_t device)
{
    eip202_cdr_write32(device,
                       EIP202_CDR_RING_BASE_ADDR_LO,
                       EIP202_CDR_RING_BASE_ADDR_LO_DEFAULT);
}


static inline void
EIP202_CDR_RING_BASE_ADDR_LO_WR(
        device_handle_t device,
        const u32 low_addr)
{
    eip202_cdr_write32(device, EIP202_CDR_RING_BASE_ADDR_LO, low_addr);
}


static inline void
EIP202_CDR_RING_BASE_ADDR_HI_DEFAULT_WR(
        device_handle_t device)
{
    eip202_cdr_write32(device,
                       EIP202_CDR_RING_BASE_ADDR_HI,
                       EIP202_CDR_RING_BASE_ADDR_HI_DEFAULT);
}


static inline void
EIP202_CDR_RING_BASE_ADDR_HI_WR(
        device_handle_t device,
        const u32 hi_addr)
{
    eip202_cdr_write32(device, EIP202_CDR_RING_BASE_ADDR_HI, hi_addr);
}


static inline void
EIP202_CDR_RING_SIZE_DEFAULT_WR(
        device_handle_t device)
{
    eip202_cdr_write32(device,
                       EIP202_CDR_RING_SIZE,
                       EIP202_CDR_RING_SIZE_DEFAULT);
}


static inline void
EIP202_CDR_RING_SIZE_WR(
        device_handle_t device,
        const u32 cdr_word_count)
{
    u32 reg_val = EIP202_CDR_RING_SIZE_DEFAULT;

    reg_val |= ((((u32)cdr_word_count) & MASK_22_BITS) << 2);

    eip202_cdr_write32(device, EIP202_CDR_RING_SIZE, reg_val);
}


static inline void
EIP202_CDR_DESC_SIZE_DEFAULT_WR(
        device_handle_t device)
{
    eip202_cdr_write32(device,
                       EIP202_CDR_DESC_SIZE,
                       EIP202_CDR_DESC_SIZE_DEFAULT);
}


static inline void
EIP202_CDR_DESC_SIZE_WR(
        device_handle_t device,
        const u8 dscr_word_count,
        const u8 dscr_offs_word_count,
        const bool f_atp_to_token,
        const bool f_atp,
        const bool f64bit)
{
    u32 reg_val = EIP202_CDR_DESC_SIZE_DEFAULT;

    if(f64bit)
        reg_val |= BIT_31;

    if(f_atp)
        reg_val |= BIT_30;

    if(f_atp_to_token)
        reg_val |= BIT_29;

    reg_val |= ((((u32)dscr_offs_word_count) & MASK_8_BITS) << 16);
    reg_val |= ((((u32)dscr_word_count)     & MASK_8_BITS)      );

    eip202_cdr_write32(device, EIP202_CDR_DESC_SIZE, reg_val);
}


static inline void
EIP202_CDR_CFG_DEFAULT_WR(
        device_handle_t device)
{
    eip202_cdr_write32(device,
                       EIP202_CDR_CFG,
                       EIP202_CDR_CFG_DEFAULT);
}


static inline void
EIP202_CDR_CFG_WR(
        device_handle_t device,
        const u16 cd_fetch_word_count,
        const u16 cd_fetch_thresh_word_count)
{
    u32 reg_val = EIP202_CDR_CFG_DEFAULT;

    reg_val |= ((((u32)cd_fetch_thresh_word_count) & MASK_10_BITS) << 16);
    reg_val |= ((((u32)cd_fetch_word_count)       & MASK_16_BITS)      );

    eip202_cdr_write32(device, EIP202_CDR_CFG, reg_val);
}


static inline void
EIP202_CDR_DMA_CFG_DEFAULT_WR(
        device_handle_t device)
{
    eip202_cdr_write32(device,
                       EIP202_CDR_DMA_CFG,
                       EIP202_CDR_DMA_CFG_DEFAULT);
}


static inline void
EIP202_CDR_DMA_CFG_WR(
        device_handle_t device,
        const u8 cd_swap,
        const u8 data_swap,
        const u8 token_swap,
        const bool f_buf,
        const u8 wr_cache,
        const u8 rd_cache,
        const u8 cd_protection,
        const u8 data_protection,
        const u8 acd_protection)
{
    u32 reg_val = EIP202_CDR_DMA_CFG_DEFAULT;

    reg_val &= (~BIT_24);
    if(f_buf)
        reg_val |= BIT_24;

    reg_val |= ((((u32)rd_cache)   & MASK_3_BITS) << 29);
    reg_val |= ((((u32)wr_cache)   & MASK_3_BITS) << 25);
    reg_val |= ((((u32)token_swap) & MASK_4_BITS) << 16);
    reg_val |= ((((u32)data_swap)  & MASK_4_BITS) << 8);
    reg_val |= ((((u32)cd_swap)    & MASK_4_BITS));

    reg_val |= ((cd_protection & MASK_4_BITS) << 4);
    reg_val |= ((data_protection & MASK_4_BITS) << 12);
    reg_val |= ((acd_protection  & MASK_4_BITS) << 20);

    eip202_cdr_write32(device, EIP202_CDR_DMA_CFG, reg_val);
}


static inline void
EIP202_CDR_THRESH_DEFAULT_WR(
        device_handle_t device)
{
    eip202_cdr_write32(device,
                       EIP202_CDR_THRESH,
                       EIP202_CDR_THRESH_DEFAULT);
}


static inline void
EIP202_CDR_THRESH_WR(
        device_handle_t device,
        const u32 cd_thresh_word_count,
        const u8 cd_timeout)
{
    u32 reg_val = EIP202_CDR_THRESH_DEFAULT;

    reg_val |= ((((u32)cd_timeout) & MASK_8_BITS) << 24);
    reg_val |= (cd_thresh_word_count            & MASK_22_BITS);

    eip202_cdr_write32(device, EIP202_CDR_THRESH, reg_val);
}


static inline void
EIP202_CDR_COUNT_DEFAULT_WR(
        device_handle_t device)
{
    eip202_cdr_write32(device,
                       EIP202_CDR_COUNT,
                       EIP202_CDR_COUNT_DEFAULT);
}


static inline void
EIP202_CDR_COUNT_WR(
        device_handle_t device,
        const u16 cd_count,
        const bool f_clear_count)
{
    u32 reg_val = EIP202_CDR_COUNT_DEFAULT;

    if(f_clear_count)
        reg_val |= BIT_31;

    reg_val |= ((((u32)cd_count) & MASK_14_BITS) << 2);

    eip202_cdr_write32(device, EIP202_CDR_COUNT, reg_val);
}


static inline void
EIP202_CDR_COUNT_RD(
        device_handle_t device,
        u32 * const cd_word_count_p)
{
    u32 reg_val;

    reg_val = eip202_cdr_read32(device, EIP202_CDR_COUNT);

    *cd_word_count_p   = (u32)((reg_val >> 2) & MASK_22_BITS);
}


static inline void
EIP202_CDR_PROC_COUNT_DEFAULT_WR(
        device_handle_t device)
{
    eip202_cdr_write32(device,
                       EIP202_CDR_PROC_COUNT,
                       EIP202_CDR_PROC_COUNT_DEFAULT);
}


static inline void
EIP202_CDR_PROC_COUNT_WR(
        device_handle_t device,
        const u32 proc_cd_word_count,
        const u16 proc_pkt_count,
        const bool f_clear_count)
{
    u32 reg_val = EIP202_CDR_PROC_COUNT_DEFAULT;

    if(f_clear_count)
        reg_val |= BIT_31;

    reg_val |= ((((u32)proc_cd_word_count) & MASK_22_BITS) << 2);
    reg_val |= ((((u32)proc_pkt_count)    & MASK_7_BITS)  << 24);

    eip202_cdr_write32(device, EIP202_CDR_PROC_COUNT, reg_val);
}


static inline void
EIP202_CDR_PROC_COUNT_RD(
        device_handle_t device,
        u32 * const proc_cd_word_count_p,
        u8 * const proc_pkt_count_p)
{
    u32 reg_val;

    reg_val = eip202_cdr_read32(device, EIP202_CDR_PROC_COUNT);

    *proc_cd_word_count_p = (u32)((reg_val >> 2)  & MASK_22_BITS);
    *proc_pkt_count_p    = (u8)((reg_val >> 24) & MASK_7_BITS);
}


static inline void
EIP202_CDR_POINTER_DEFAULT_WR(
        device_handle_t device)
{
    eip202_cdr_write32(device,
                       EIP202_CDR_POINTER,
                       EIP202_CDR_POINTER_DEFAULT);
}


static inline void
EIP202_CDR_POINTER_RD(
        device_handle_t device,
        u32 * const cdr_pointer_p)
{
    u32 reg_val;

    reg_val = eip202_cdr_read32(device, EIP202_CDR_POINTER);

    *cdr_pointer_p   = (u32)((reg_val >> 2) & MASK_22_BITS);
}

static inline bool
EIP202_CDR_REV_SIGNATURE_MATCH(
        const u32 rev)
{
    return (((u16)rev) == EIP202_CDR_SIGNATURE);
}


static inline void
EIP202_CDR_OPTIONS_RD(
        device_handle_t device,
        u8 * const nof_rings,
        u8 * const nof_pes,
        bool * const f_exp_plf,
        u8 * const cf_size,
        u8 * const rf_size,
        u8 * const host_ifc,
        u8 * const dma_len,
        u8 * const hdw,
        u8 * const tgt_align,
        bool * const f_addr64)
{
    u32 rev_reg_val;

    rev_reg_val = eip202_cdr_read32(device, EIP202_CDR_OPTIONS);

    *f_addr64   = ((rev_reg_val & BIT_31) != 0);
    *tgt_align  = (u8)((rev_reg_val >> 28) & MASK_3_BITS);
    *hdw       = (u8)((rev_reg_val >> 25) & MASK_3_BITS);
    *dma_len   = (u8)((rev_reg_val >> 20) & MASK_5_BITS);
    *host_ifc   = (u8)((rev_reg_val >> 16) & MASK_4_BITS);
    *f_exp_plf   = ((rev_reg_val & BIT_15) != 0);
    // Make RD fifo size = 2^rf_size in hdw words
    *rf_size   = (u8)((rev_reg_val >> 12) & MASK_3_BITS) + 4;
    // Make CD fifo size = 2^cf_size in hdw words
    *cf_size   = (u8)((rev_reg_val >> 9)  & MASK_3_BITS) + 4;
    *nof_pes    = (u8)((rev_reg_val >> 4)  & MASK_5_BITS);
    *nof_rings  = (u8)((rev_reg_val)       & MASK_4_BITS);
}


#endif /* EIP202_CDR_LEVEL0_H_ */


/* end of file eip202_cdr_level0.h */
