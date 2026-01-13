/* eip97_global_level0.h
 *
 * EIP-97 Global Control Level0 internal interface
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

#ifndef EIP97_GLOBAL_LEVEL0_H_
#define EIP97_GLOBAL_LEVEL0_H_


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Default configuration
#include "c_eip97_global.h"     // EIP97_RC_BASE, EIP97_BASE

// Driver Framework Basic Definitions API
#include "basic_defs.h"         // BIT definitions, bool, u32

// Driver Framework device API
#include "device_types.h"       // device_handle_t
#include "device_rw.h"          // Read32, Write32

#define EIP97_REG_MST_CTRL_DEFAULT      0x00000000

/*----------------------------------------------------------------------------
 * Definitions and macros
 */

// Read/Write register constants

// byte offsets of the EIP-97 Engine registers

// EIP-97 Engine EIP number (0x61) and complement (0x9E)
#define EIP97_SIGNATURE    ((u16)0x9E61)

#define EIP97_REG_OFFS     4

#define EIP97_REG_CS_DMA_CTRL2  ((EIP97_RC_BASE)+(0x01 * EIP97_REG_OFFS))

#define EIP97_REG_FORCE_CLOCK_ON2    0xfffd8
#define EIP97_REG_FORCE_CLOCK_OFF2   0xfffdc
#define EIP97_REG_FORCE_CLOCK_STATE2 0xfffe0
#define EIP97_REG_FORCE_CLOCK_STATE  0xfffe4
#define EIP97_REG_FORCE_CLOCK_ON     0xfffe8
#define EIP97_REG_FORCE_CLOCK_OFF    0xfffec
#define EIP97_REG_MST_CTRL ((EIP97_BASE)+(0x00 * EIP97_REG_OFFS))
#define EIP97_REG_OPTIONS  ((EIP97_BASE)+(0x01 * EIP97_REG_OFFS))
#define EIP97_REG_VERSION  ((EIP97_BASE)+(0x02 * EIP97_REG_OFFS))

#define EIP97_REG_CS_DMA_CTRL2_DEFAULT  0x00000000


/*----------------------------------------------------------------------------
 * Local variables
 */


/*----------------------------------------------------------------------------
 * eip97_read32
 *
 * This routine writes to a Register location in the EIP-97.
 */
static inline u32
eip97_read32(
        device_handle_t device,
        const unsigned int offset)
{
    return device_read32(device, offset);
}


/*----------------------------------------------------------------------------
 * eip97_write32
 *
 * This routine writes to a Register location in the EIP97.
 */
static inline void
eip97_write32(
        device_handle_t device,
        const unsigned int offset,
        const u32 value)
{
    device_write32(device, offset, value);
}


static inline bool
EIP97_REV_SIGNATURE_MATCH(
        const u32 rev)
{
    return (((u16)rev) == EIP97_SIGNATURE);
}


static inline void
EIP97_EIP_REV_RD(
        device_handle_t device,
        u8 * const eip_number,
        u8 * const complmt_eip_number,
        u8 * const hw_patch_level,
        u8 * const min_hw_revision,
        u8 * const maj_hw_revision)
{
    u32 rev_reg_val;

    rev_reg_val = eip97_read32(device, EIP97_REG_VERSION);

    *maj_hw_revision     = (u8)((rev_reg_val >> 24) & MASK_4_BITS);
    *min_hw_revision     = (u8)((rev_reg_val >> 20) & MASK_4_BITS);
    *hw_patch_level      = (u8)((rev_reg_val >> 16) & MASK_4_BITS);
    *complmt_eip_number  = (u8)((rev_reg_val >> 8)  & MASK_8_BITS);
    *eip_number         = (u8)((rev_reg_val)       & MASK_8_BITS);
}


static inline void
EIP97_MST_CTRL_WR(
        const device_handle_t device,
        const unsigned int pe_nr,
        const u8 rd_cache,
        const u8 wr_cache,
        const u8 swap_method,
        const u8 protection,
        const u8 ctx_cache_align,
        const bool f_write_cache_no_buf)
{
    u32 reg_val = EIP97_REG_MST_CTRL_DEFAULT;

    IDENTIFIER_NOT_USED(pe_nr);

    reg_val |= (u32)((((u32)f_write_cache_no_buf) & MASK_1_BIT) << 18);
    reg_val |= (u32)((((u32)ctx_cache_align) & MASK_2_BITS) << 16);
    reg_val |= (u32)((((u32)protection) & MASK_4_BITS) << 12);
    reg_val |= (u32)((((u32)swap_method) & MASK_4_BITS) << 8);
    reg_val |= (u32)((((u32)wr_cache)    & MASK_4_BITS) << 4);
    reg_val |= (u32)((((u32)rd_cache)    & MASK_4_BITS));

    eip97_write32(device, EIP97_REG_MST_CTRL, reg_val);
}


static inline void
EIP97_CS_DMA_CTRL2_WR(
        const device_handle_t device,
        const u8 flue_swap_mask,
        const u8 frc_swap_mask,
        const u8 trc_swap_mask,
        const u8 arc4_swap_mask)
{
    u32 reg_val = EIP97_REG_CS_DMA_CTRL2_DEFAULT;

    reg_val |= (u32)((((u32)flue_swap_mask) & MASK_4_BITS) << 24);
    reg_val |= (u32)((((u32)arc4_swap_mask) & MASK_4_BITS) << 16);
    reg_val |= (u32)((((u32)trc_swap_mask)  & MASK_4_BITS) << 8);
    reg_val |= (u32)((((u32)frc_swap_mask)  & MASK_4_BITS));

    eip97_write32(device, EIP97_REG_CS_DMA_CTRL2, reg_val);
}


// EIP-97 Global Control Level0 internal API extensions
#define EIP97_REG_DBG_BASE 0xffb00
#define EIP97_REG_DBG_RING_IN_COUNT_LO(n)  (EIP97_REG_DBG_BASE + 0x00 + 16*(n))
#define EIP97_REG_DBG_RING_IN_COUNT_HI(n)  (EIP97_REG_DBG_BASE + 0x04 + 16*(n))
#define EIP97_REG_DBG_RING_OUT_COUNT_LO(n) (EIP97_REG_DBG_BASE + 0x08 + 16*(n))
#define EIP97_REG_DBG_RING_OUT_COUNT_HI(n) (EIP97_REG_DBG_BASE + 0x0c + 16*(n))
#define EIP97_REG_DBG_PIPE_COUNT(n)  (EIP97_REG_DBG_BASE + 0x100 + 16*(n))
#define EIP97_REG_DBG_PIPE_STATE(n)  (EIP97_REG_DBG_BASE + 0x104 + 16*(n))
#define EIP97_REG_DBG_PIPE_DCOUNT_LO(n)  (EIP97_REG_DBG_BASE + 0x108 + 16*(n))
#define EIP97_REG_DBG_PIPE_DCOUNT_HI(n)  (EIP97_REG_DBG_BASE + 0x10c + 16*(n))

/*----------------------------------------------------------------------------
 * Local variables
 */

static inline void
EIP97_OPTIONS_RD(
        device_handle_t device,
        u8 * const nof_pes,
        u8 * const in_tbuf_size,
        u8 * const in_dbuf_size,
        u8 * const out_tbuf_size,
        u8 * const out_dbuf_size,
        bool * const f_central_prng,
        bool * const f_tg,
        bool * const f_trc)
{
    u32 rev_reg_val;

    rev_reg_val = eip97_read32(device, EIP97_REG_OPTIONS);

    *f_trc         = ((rev_reg_val & BIT_31) != 0);
    *f_tg          = ((rev_reg_val & BIT_27) != 0);
    *f_central_prng = ((rev_reg_val & BIT_26) != 0);
    *out_dbuf_size  = (u8)((rev_reg_val >> 16) & MASK_4_BITS);
    *out_tbuf_size  = (u8)((rev_reg_val >> 13) & MASK_3_BITS) + 3;
    *in_dbuf_size   = (u8)((rev_reg_val >> 9)  & MASK_4_BITS);
    *in_tbuf_size   = (u8)((rev_reg_val >> 6)  & MASK_3_BITS) + 3;
    *nof_pes       = (u8)((rev_reg_val)       & MASK_5_BITS);
}

static inline void
EIP97_DBG_RING_IN_COUNT_RD(
        device_handle_t device,
        unsigned int pe_nr,
        u64 * const counter)
{
    u32 val_lo,val_hi;
    val_lo = eip97_read32(device, EIP97_REG_DBG_RING_IN_COUNT_LO(pe_nr));
    val_hi = eip97_read32(device, EIP97_REG_DBG_RING_IN_COUNT_HI(pe_nr));

    *counter = ((u64)val_hi << 32) | ((u64)val_lo);
}

static inline void
EIP97_DBG_RING_OUT_COUNT_RD(
        device_handle_t device,
        unsigned int pe_nr,
        u64 * const counter)
{
    u32 val_lo,val_hi;
    val_lo = eip97_read32(device, EIP97_REG_DBG_RING_OUT_COUNT_LO(pe_nr));
    val_hi = eip97_read32(device, EIP97_REG_DBG_RING_OUT_COUNT_HI(pe_nr));

    *counter = ((u64)val_hi << 32) | ((u64)val_lo);
}


static inline void
EIP97_DBG_PIPE_COUNT_RD(
        device_handle_t device,
        unsigned int pe_nr,
        u64 * const total_packets,
        u8 * const current_packets,
        u8 * const max_packets)
{
    u32 val_lo,val_hi;
    val_lo = eip97_read32(device, EIP97_REG_DBG_PIPE_COUNT(pe_nr));
    val_hi = eip97_read32(device, EIP97_REG_DBG_PIPE_STATE(pe_nr));

    *total_packets = (((u64)val_hi & MASK_16_BITS )<< 32) | ((u64)val_lo);
    *current_packets = (val_hi >> 16) & MASK_8_BITS;
    *max_packets = (val_hi >> 24) & MASK_8_BITS;
}


static inline void
EIP97_DBG_PIPE_DCOUNT_RD(
        device_handle_t device,
        unsigned int pe_nr,
        u64 * const counter)
{
    u32 val_lo,val_hi;
    val_lo = eip97_read32(device, EIP97_REG_DBG_PIPE_DCOUNT_LO(pe_nr));
    val_hi = eip97_read32(device, EIP97_REG_DBG_PIPE_DCOUNT_HI(pe_nr));

    *counter = ((u64)val_hi << 32) | ((u64)val_lo);
}

#endif /* EIP97_GLOBAL_LEVEL0_H_ */


/* end of file eip97_global_level0.h */
