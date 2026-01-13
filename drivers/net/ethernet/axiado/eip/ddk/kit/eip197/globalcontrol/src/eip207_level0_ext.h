/* eip207_level0_ext.h
 *
 * EIP-207 Level0 internal interface extensions
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

#ifndef EIP207_LEVEL0_EXT_H_
#define EIP207_LEVEL0_EXT_H_


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Driver Framework Basic Definitions API
#include "basic_defs.h"             // BIT definitions, bool, u32

// EIP-207 HW interface
#include "eip207_hw_interface.h"    // EIP207_FLUE_REG_*

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
EIP207_FLUE_CONFIG_WR(
        const device_handle_t device,
        const unsigned int hash_table_id,
        const u8 function,
        const u8 generation,
        const bool f_table_enable,
        const bool f_xs_enable)
{
    u32 reg_val = EIP207_FLUE_REG_CONFIG_DEFAULT;

    if (f_table_enable)
        reg_val |= BIT_30;

    if (f_xs_enable)
        reg_val |= BIT_31;

    reg_val |= (u32)((((u32)generation) & MASK_3_BITS) << 24);
    reg_val |= (u32)((((u32)function)   & MASK_8_BITS) << 16);

    eip207_write32(device, EIP207_FLUE_REG_CONFIG(hash_table_id), reg_val);
}


static inline void
EIP207_FLUE_OFFSET_WR(
        const device_handle_t device,
        const bool f_prefetch_xform,
        const bool f_lookup_cached,
        const u8 xform_record_word_offset)
{
    u32 reg_val = EIP207_FLUE_REG_OFFSETS_DEFAULT;

    if (f_prefetch_xform)
        reg_val |= BIT_2;

    if (f_lookup_cached)
        reg_val |= BIT_3;

    reg_val |=
          (u32)((((u32)xform_record_word_offset) & MASK_8_BITS) << 24);

    eip207_write32(device, EIP207_FLUE_REG_OFFSETS, reg_val);
}


static inline void
EIP207_FLUE_ARC4_OFFSET_WR(
        const device_handle_t device,
        const bool f_prefetch_arc4,
        const bool f3_entry_lookup,
        const u8 arc4_record_word_offset)
{
    u32 reg_val = EIP207_FLUE_REG_ARC4_OFFSET_DEFAULT;

    if (f_prefetch_arc4)
        reg_val |= BIT_2;

    if (f3_entry_lookup)
        reg_val |= BIT_3;

    reg_val |=
          (u32)((((u32)arc4_record_word_offset) & MASK_8_BITS) << 24);

    eip207_write32(device, EIP207_FLUE_REG_ARC4_OFFSET, reg_val);
}


static inline void
EIP207_ICE_PUE_CTRL_WR(
        const device_handle_t device,
        const unsigned int ce_nr,
        const u16 current_pc,
        const bool f_ecc_corr,
        const bool f_ecc_derr,
        const bool f_debug_reset,
        const bool f_sw_reset)
{
    u32 reg_val = EIP207_ICE_REG_PUE_CTRL_DEFAULT;

    if (!f_sw_reset)
        reg_val &= (~BIT_0);

    if(f_debug_reset)
        reg_val |= BIT_3;

    if(f_ecc_derr)
        reg_val |= BIT_14;

    if(f_ecc_corr)
        reg_val |= BIT_15;

    reg_val |= (u32)((((u32)current_pc) & MASK_15_BITS) << 16);

    eip207_write32(device, EIP207_ICE_REG_PUE_CTRL(ce_nr), reg_val);
}


static inline void
EIP207_ICE_PUE_CTRL_RD_CLEAR(
        const device_handle_t device,
        const unsigned int ce_nr,
        bool * const f_ecc_corr_p,
        bool * const f_ecc_derr_p)
{
    u32 reg_val;

    reg_val = eip207_read32(device, EIP207_ICE_REG_PUE_CTRL(ce_nr));

    *f_ecc_corr_p = ((reg_val & BIT_15) != 0);
    *f_ecc_derr_p = ((reg_val & BIT_14) != 0);

    // Clear correctable ECC error is detected
    if (*f_ecc_corr_p )
    {
        if(*f_ecc_derr_p)
            reg_val &= (~BIT_14);

        eip207_write32(device, EIP207_ICE_REG_PUE_CTRL(ce_nr), reg_val);
    }
}


static inline void
EIP207_ICE_FPP_CTRL_WR(
        const device_handle_t device,
        const unsigned int ce_nr,
        const u16 current_pc,
        const bool f_ecc_corr,
        const bool f_ecc_derr,
        const bool f_debug_reset,
        const bool f_sw_reset)
{
    u32 reg_val = EIP207_ICE_REG_FPP_CTRL_DEFAULT;

    if (!f_sw_reset)
        reg_val &= (~BIT_0);

    if (f_debug_reset)
        reg_val |= BIT_3;

    if(f_ecc_derr)
        reg_val |= BIT_14;

    if(f_ecc_corr)
        reg_val |= BIT_15;

    reg_val |= (u32)((((u32)current_pc) & MASK_15_BITS) << 16);

    eip207_write32(device, EIP207_ICE_REG_FPP_CTRL(ce_nr), reg_val);
}


static inline void
EIP207_ICE_FPP_CTRL_RD_CLEAR(
        const device_handle_t device,
        const unsigned int ce_nr,
        bool * const f_ecc_corr_p,
        bool * const f_ecc_derr_p)
{
    u32 reg_val;

    reg_val = eip207_read32(device, EIP207_ICE_REG_FPP_CTRL(ce_nr));

    *f_ecc_corr_p = ((reg_val & BIT_15) != 0);
    *f_ecc_derr_p = ((reg_val & BIT_14) != 0);

    // Clear correctable ECC error is detected
    if (*f_ecc_corr_p )
    {
        if(*f_ecc_derr_p)
            reg_val &= (~BIT_14);

        eip207_write32(device, EIP207_ICE_REG_FPP_CTRL(ce_nr), reg_val);
    }
}


static inline void
EIP207_OCE_SCRATCH_CTRL_WR(
        const device_handle_t device,
        const unsigned int ce_nr,
        const bool f_change_timer,
        const bool f_timer_enable,
        const u16 timer_prescaler,
        const u8 timer_oflo_bit,
        const bool f_change_access,
        const u8 scratch_access)
{
    u32 reg_val = EIP207_OCE_REG_SCRATCH_CTRL_DEFAULT;

    if(f_change_timer)
        reg_val |= BIT_2;

    if(f_timer_enable)
        reg_val |= BIT_3;

    if(f_change_access)
        reg_val |= BIT_24;

    reg_val |= (u32)((((u32)scratch_access)  & MASK_4_BITS)  << 25);

    reg_val &= (~((u32)(MASK_5_BITS << 16)));
    reg_val |= (u32)((((u32)timer_oflo_bit)   & MASK_5_BITS)  << 16);

    reg_val &= (~((u32)(MASK_12_BITS << 4)));
    reg_val |= (u32)((((u32)timer_prescaler) & MASK_12_BITS) << 4);

    eip207_write32(device, EIP207_OCE_REG_SCRATCH_CTRL(ce_nr), reg_val);
}


static inline void
EIP207_OCE_SCRATCH_CTRL_RD(
        const device_handle_t device,
        const unsigned int ce_nr,
        bool * const f_timer_oflo_irq)
{
    u32 reg_val = eip207_read32(device, EIP207_OCE_REG_SCRATCH_CTRL(ce_nr));

    *f_timer_oflo_irq = ((reg_val & BIT_0) != 0);
}


static inline void
EIP207_OCE_PUE_CTRL_WR(
        const device_handle_t device,
        const unsigned int ce_nr,
        const u16 current_pc,
        const bool f_ecc_corr,
        const bool f_ecc_derr,
        const bool f_debug_reset,
        const bool f_sw_reset)
{
    u32 reg_val = EIP207_OCE_REG_PUE_CTRL_DEFAULT;

    if (!f_sw_reset)
        reg_val &= (~BIT_0);

    if(f_debug_reset)
        reg_val |= BIT_3;

    if(f_ecc_derr)
        reg_val |= BIT_14;

    if(f_ecc_corr)
        reg_val |= BIT_15;

    reg_val |= (u32)((((u32)current_pc) & MASK_15_BITS) << 16);

    eip207_write32(device, EIP207_OCE_REG_PUE_CTRL(ce_nr), reg_val);
}


static inline void
EIP207_OCE_PUE_CTRL_RD_CLEAR(
        const device_handle_t device,
        const unsigned int ce_nr,
        bool * const f_ecc_corr_p,
        bool * const f_ecc_derr_p)
{
    u32 reg_val;

    reg_val = eip207_read32(device, EIP207_OCE_REG_PUE_CTRL(ce_nr));

    *f_ecc_corr_p = ((reg_val & BIT_15) != 0);
    *f_ecc_derr_p = ((reg_val & BIT_14) != 0);

    // Clear correctable ECC error is detected
    if (*f_ecc_corr_p )
    {
        if(*f_ecc_derr_p)
            reg_val &= (~BIT_14);

        eip207_write32(device, EIP207_OCE_REG_PUE_CTRL(ce_nr), reg_val);
    }
}


static inline void
EIP207_OCE_FPP_CTRL_WR(
        const device_handle_t device,
        const unsigned int ce_nr,
        const u16 current_pc,
        const bool f_ecc_corr,
        const bool f_ecc_derr,
        const bool f_debug_reset,
        const bool f_sw_reset)
{
    u32 reg_val = EIP207_OCE_REG_FPP_CTRL_DEFAULT;

    if (!f_sw_reset)
        reg_val &= (~BIT_0);

    if (f_debug_reset)
        reg_val |= BIT_3;

    if(f_ecc_derr)
        reg_val |= BIT_14;

    if(f_ecc_corr)
        reg_val |= BIT_15;

    reg_val |= (u32)((((u32)current_pc) & MASK_15_BITS) << 16);

    eip207_write32(device, EIP207_OCE_REG_FPP_CTRL(ce_nr), reg_val);
}


static inline void
EIP207_OCE_FPP_CTRL_RD_CLEAR(
        const device_handle_t device,
        const unsigned int ce_nr,
        bool * const f_ecc_corr_p,
        bool * const f_ecc_derr_p)
{
    u32 reg_val;

    reg_val = eip207_read32(device, EIP207_OCE_REG_FPP_CTRL(ce_nr));

    *f_ecc_corr_p = ((reg_val & BIT_15) != 0);
    *f_ecc_derr_p = ((reg_val & BIT_14) != 0);

    // Clear correctable ECC error is detected
    if (*f_ecc_corr_p )
    {
        if(*f_ecc_derr_p)
            reg_val &= (~BIT_14);

        eip207_write32(device, EIP207_OCE_REG_FPP_CTRL(ce_nr), reg_val);
    }
}


static inline void
EIP207_OCE_RAM_CTRL_WR(
        const device_handle_t device,
        const unsigned int ce_nr,
        const bool f_pue_prog_enable,
        const bool f_fpp_prog_enable)
{
    u32 reg_val = EIP207_OCE_REG_RAM_CTRL_DEFAULT;

    if(f_pue_prog_enable)
        reg_val |= BIT_0;

    if(f_fpp_prog_enable)
        reg_val |= BIT_1;

    eip207_write32(device, EIP207_OCE_REG_RAM_CTRL(ce_nr), reg_val);
}


#ifdef EIP207_FLUE_HAVE_VIRTUALIZATION

static inline void
EIP207_FLUE_IFC_LUT_WR(
        const device_handle_t device,
        const unsigned int reg_nr,
        const unsigned int table_id0,
        const unsigned int table_id1,
        const unsigned int table_id2,
        const unsigned int table_id3)
{
    u32 reg_val = EIP207_FLUE_REG_IFC_LUT_DEFAULT;

    reg_val |= (table_id0 & MASK_4_BITS);
    reg_val |= (table_id1 & MASK_4_BITS) << 8;
    reg_val |= (table_id2 & MASK_4_BITS) << 16;
    reg_val |= (table_id3 & MASK_4_BITS) << 24;

    eip207_write32(device, EIP207_FLUE_REG_IFC_LUT(reg_nr), reg_val);
}

#endif


#endif /* EIP207_LEVEL0_EXT_H_ */


/* end of file eip207_level0_ext.h */
