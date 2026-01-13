/* eip207_rc_level0_ext.h
 *
 * EIP-207 Record Cache Level0 internal interface High-Performance (HP)
 * extensions
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

#ifndef EIP207_RC_LEVEL0_EXT_H_
#define EIP207_RC_LEVEL0_EXT_H_


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Driver Framework Basic Definitions API
#include "basic_defs.h"             // BIT definitions, bool, u32

// EIP-207 HW interface
#include "eip207_hw_interface.h"    // EIP207_RC_REG_*

// Driver Framework device API
#include "device_types.h"           // device_handle_t


/*----------------------------------------------------------------------------
 * Definitions and macros
 */


/*----------------------------------------------------------------------------
 * Local variables
 */


/*----------------------------------------------------------------------------
 * EIP207 Global Register Write/Read Functions
 *
 */

static inline void
EIP207_RC_PARAMS_WR(
        const device_handle_t device,
        const u32 cache_base,
        const unsigned int cache_nr,
        const bool f_sw_reset,
        const bool f_block,
        const bool f_data_access,
        const u8 hash_table_mask,
        const u8 block_clock_count,
        const bool f_max_rec_size,
        const u16 record_word_count)
{
    u32 reg_val = EIP207_RC_REG_PARAMS_DEFAULT;

    IDENTIFIER_NOT_USED(f_max_rec_size);

    if (!f_sw_reset)
        reg_val &= (~BIT_0);

    if (f_block)
        reg_val |= BIT_1;

    if (f_data_access)
        reg_val |= BIT_2;

    reg_val &= (~((u32)(MASK_9_BITS << 18)));
    reg_val |= (u32)((((u32)record_word_count) & MASK_9_BITS) << 18);

    reg_val &= (~((u32)(MASK_3_BITS << 10)));
    reg_val |= (u32)((((u32)block_clock_count) & MASK_3_BITS) << 10);

    reg_val |= (u32)((((u32)hash_table_mask)   & MASK_3_BITS) << 4);

    eip207_write32(device, EIP207_RC_REG_PARAMS(cache_base, cache_nr), reg_val);
}


static inline void
EIP207_RC_PARAMS_RD(
        const device_handle_t device,
        const u32 cache_base,
        const unsigned int cache_nr,
        bool * const f_dma_read_error_p,
        bool * const f_dma_write_error_p)
{
    u32 reg_val;

    reg_val = eip207_read32(device, EIP207_RC_REG_PARAMS(cache_base, cache_nr));

    *f_dma_read_error_p  = ((reg_val & BIT_30) != 0);
    *f_dma_write_error_p = ((reg_val & BIT_31) != 0);
}


static inline void
EIP207_RC_FREECHAIN_WR(
        const device_handle_t device,
        const u32 cache_base,
        const unsigned int cache_nr,
        const u16 head_offset,
        const u16 tail_offset)
{
    u32 reg_val = EIP207_RC_REG_PARAMS2_DEFAULT;

    reg_val |= (u32)((((u32)tail_offset) & MASK_10_BITS) << 16);
    reg_val |= (u32)((((u32)head_offset) & MASK_10_BITS));

    eip207_write32(device, EIP207_RC_REG_FREECHAIN(cache_base,cache_nr), reg_val);
}


static inline void
EIP207_RC_PARAMS2_WR(
        const device_handle_t device,
        const u32 cache_base,
        const unsigned int cache_nr,
        const u16 hash_table_start,
        const u16 record2_word_count,
        const u8 dma_wr_comb_dly)
{
    u32 reg_val = EIP207_RC_REG_PARAMS2_DEFAULT;

    reg_val |= (u32)((((u32)record2_word_count) & MASK_9_BITS) << 18);
    reg_val |= (u32)((((u32)dma_wr_comb_dly)     & MASK_8_BITS) << 10);
    reg_val |= (u32)((((u32)hash_table_start)   & MASK_10_BITS));

    eip207_write32(device, EIP207_RC_REG_PARAMS2(cache_base, cache_nr), reg_val);
}


static inline void
EIP207_RC_REGINDEX_WR(
        const device_handle_t device,
        const u32 cache_base,
        const unsigned int cache_nr,
        const u8 regindex)
{
    u32 reg_val = EIP207_RC_REG_REGINDEX_DEFAULT;

    reg_val |= (u32)((((u32)regindex) & MASK_6_BITS));

    eip207_write32(device, EIP207_RC_REG_REGINDEX(cache_base, cache_nr), reg_val);
}


static inline void
EIP207_RC_ECCCTRL_WR(
        const device_handle_t device,
        const u32 cache_base,
        const unsigned int cache_nr,
        const bool f_data_ecc_oflo,
        const bool f_data_ecc_err,
        const bool f_admin_ecc_err)
{
    u32 reg_val = EIP207_RC_REG_ECCCRTL_DEFAULT;

    if (f_data_ecc_oflo)
        reg_val |= BIT_29;

    if (f_data_ecc_err)
        reg_val |= BIT_30;

    if (f_admin_ecc_err)
        reg_val |= BIT_31;

    eip207_write32(device, EIP207_RC_REG_ECCCTRL(cache_base, cache_nr), reg_val);
}


static inline void
EIP207_RC_ECCCTRL_RD_CLEAR(
        const device_handle_t device,
        const u32 cache_base,
        const unsigned int cache_nr,
        bool * const f_data_ecc_oflo_p,
        bool * const f_data_ecc_err_p,
        bool * const f_admin_ecc_err_p)
{
    u32 reg_val;

    reg_val = eip207_read32(device, EIP207_RC_REG_ECCCTRL(cache_base, cache_nr));

    *f_data_ecc_oflo_p  = ((reg_val & BIT_29) != 0);
    *f_data_ecc_err_p   = ((reg_val & BIT_30) != 0);
    *f_admin_ecc_err_p  = ((reg_val & BIT_31) != 0);

    // Clear non-correctable (but non-fatal) RC data RAM ECC error if detected
    if (*f_data_ecc_err_p)
    {
        // *f_data_ecc_oflo_p:
        // Do not clear fatal RC data RAM non-correctable error here

        // *f_admin_ecc_err_p:
        // Do not clear fatal RC Administration RAM non-correctable error here

        // Do not clear anything in admin_corr_mask
        reg_val &= (~MASK_12_BITS);

        if(*f_data_ecc_err_p)
            reg_val &= (~BIT_30); // Write 0 to clear the error '1' status bit

        eip207_write32(device,
                       EIP207_RC_REG_ECCCTRL(cache_base, cache_nr),
                       reg_val);
    }
}

static inline void
EIP207_RC_LONGCTR_RD(
        const device_handle_t device,
        unsigned int reg_offset,
        u64 *counter)
{
    u32 val_lo, val_hi;
    val_lo = eip207_read32(device, reg_offset);
    val_hi = eip207_read32(device, reg_offset + 4) & MASK_8_BITS;

    *counter = (((u64) val_hi) << 32) | val_lo;
}

static inline void
EIP207_RC_SHORTCTR_RD(
        const device_handle_t device,
        unsigned int reg_offset,
        u32 *counter)
{
    *counter = eip207_read32(device, reg_offset) & MASK_24_BITS;
}

static inline void
EIP207_RC_RDMAERRFLGS_RD(
        const device_handle_t device,
        const u32 cache_base,
        const unsigned int cache_nr,
        u32 *err_flags)
{
    *err_flags = eip207_read32(device,
                              EIP207_RC_REG_RDMAERRFLGS_0(cache_base,cache_nr)) &
                             MASK_8_BITS;
}

#endif /* EIP207_RC_LEVEL0_EXT_H_ */

/* end of file eip207_rc_level0_ext.h */
