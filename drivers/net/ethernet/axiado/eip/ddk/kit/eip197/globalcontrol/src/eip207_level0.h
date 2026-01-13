/* eip207_level0.h
 *
 * EIP-207 Level0 internal interface
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

#ifndef EIP207_LEVEL0_H_
#define EIP207_LEVEL0_H_


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Default configuration
#include "c_eip207_global.h"

// Driver Framework Basic Definitions API
#include "basic_defs.h"         // BIT definitions, bool, u32

// EIP-207 HW interface
#include "eip207_hw_interface.h"

// Driver Framework device API
#include "device_types.h"       // device_handle_t
#include "device_rw.h"          // Read32, Write32
#include "device_swap.h"        // device_swap_endian32


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

/*----------------------------------------------------------------------------
 * eip207_read32
 *
 * This routine writes to a Register location in the EIP-207.
 */
static inline u32
eip207_read32(
        device_handle_t device,
        const unsigned int offset)
{
    return device_read32(device, offset);
}


/*----------------------------------------------------------------------------
 * eip207_write32
 *
 * This routine writes to a Register location in the EIP-207.
 */
static inline void
eip207_write32(
        device_handle_t device,
        const unsigned int offset,
        const u32 value)
{
    device_write32(device, offset, value);

#ifdef EIP207_CLUSTERED_WRITES_DISABLE
    // Prevent clustered write operations, break them with a read operation
    // Note: Reading the EIP207_CS_REG_VERSION register has no side effects!
    eip207_read32(device, EIP207_CS_REG_VERSION);
#endif
}


static inline bool
EIP207_ICE_SIGNATURE_MATCH(
        const u32 rev)
{
    return (((u16)rev) == EIP207_ICE_SIGNATURE);
}


static inline bool
EIP207_CS_SIGNATURE_MATCH(
        const u32 rev)
{
    return (((u16)rev) == EIP207_CS_SIGNATURE);
}


static inline void
EIP207_CS_VERSION_RD(
        device_handle_t device,
        u8 * const eip_number,
        u8 * const complmt_eip_number,
        u8 * const hw_patch_level,
        u8 * const min_hw_revision,
        u8 * const maj_hw_revision)
{
    u32 rev_reg_val;

    rev_reg_val = eip207_read32(device, EIP207_CS_REG_VERSION);

    *maj_hw_revision     = (u8)((rev_reg_val >> 24) & MASK_4_BITS);
    *min_hw_revision     = (u8)((rev_reg_val >> 20) & MASK_4_BITS);
    *hw_patch_level      = (u8)((rev_reg_val >> 16) & MASK_4_BITS);
    *complmt_eip_number  = (u8)((rev_reg_val >> 8)  & MASK_8_BITS);
    *eip_number         = (u8)((rev_reg_val)       & MASK_8_BITS);
}


static inline void
EIP207_CS_OPTIONS_RD(
        device_handle_t device,
        u8 * const nof_lookup_tables,
        bool * const f_lookup_cached,
        u8 * const nof_lookup_clients,
        bool * const f_arc4_trc_combined,
        bool * const f_arc4_frc_combined,
        bool * const f_arc4_present,
        u8 * const nof_arc4_clients,
        bool * const f_trc_frc_combined,
        u8 * const nof_trc_clients,
        u8 * const nof_frc_clients,
        u8 * const nof_cache_sets)
{
    u32 rev_reg_val;

    rev_reg_val = eip207_read32(device, EIP207_CS_REG_OPTIONS);

    *nof_lookup_tables  = (u8)((rev_reg_val >> 28) & MASK_3_BITS);
    *f_lookup_cached    = ((rev_reg_val & BIT_27) != 0);
    *nof_lookup_clients = (u8)((rev_reg_val >> 22) & MASK_5_BITS);
    *f_arc4_trc_combined = ((rev_reg_val & BIT_21) != 0);
    *f_arc4_frc_combined = ((rev_reg_val & BIT_20) != 0);
    *f_arc4_present     = ((rev_reg_val & BIT_19) != 0);
    *nof_arc4_clients   = (u8)((rev_reg_val >> 14) & MASK_5_BITS);
    *f_trc_frc_combined  = ((rev_reg_val & BIT_13) != 0);
    *nof_trc_clients    = (u8)((rev_reg_val >> 8)  & MASK_5_BITS);
    *nof_frc_clients    = (u8)((rev_reg_val >> 3)  & MASK_5_BITS);
    *nof_cache_sets     = (u8)((rev_reg_val >> 1)  & MASK_2_BITS);
}

static inline void
EIP207_ICE_ADAPT_CTRL_WR(
        device_handle_t device,
        const unsigned int c_enr,
        u32 max_packet_size)
{
    u32 reg_val = 0xc0de0000;
    reg_val |= (max_packet_size & 0xfffc);
    eip207_write32(device, EIP207_ICE_REG_ADAPT_CTRL(c_enr), reg_val);
}

static inline void
EIP207_ICE_VERSION_RD(
        device_handle_t device,
        const unsigned int c_enr,
        u8 * const eip_number,
        u8 * const complmt_eip_number,
        u8 * const hw_patch_level,
        u8 * const min_hw_revision,
        u8 * const maj_hw_revision)
{
    u32 rev_reg_val;

    rev_reg_val = eip207_read32(device, EIP207_ICE_REG_VERSION(c_enr));

    *maj_hw_revision     = (u8)((rev_reg_val >> 24) & MASK_4_BITS);
    *min_hw_revision     = (u8)((rev_reg_val >> 20) & MASK_4_BITS);
    *hw_patch_level      = (u8)((rev_reg_val >> 16) & MASK_4_BITS);
    *complmt_eip_number  = (u8)((rev_reg_val >> 8)  & MASK_8_BITS);
    *eip_number         = (u8)((rev_reg_val)       & MASK_8_BITS);
}


static inline void
EIP207_CS_RAM_CTRL_WR(
        const device_handle_t device,
        const bool f_frc0_enable,
        const bool f_frc1_enable,
        const bool f_frc2_enable,
        const bool f_trc0_enable,
        const bool f_trc1_enable,
        const bool f_trc2_enable,
        const bool f_arc4rc0_enable,
        const bool f_arc4rc1_enable,
        const bool f_arc4rc2_enable,
        const bool f_fluec_enable)
{
    u32 reg_val = EIP207_CS_REG_RAM_CTRL_DEFAULT;

    if(f_frc0_enable)
        reg_val |= BIT_0;

    if(f_frc1_enable)
        reg_val |= BIT_1;

    if(f_frc2_enable)
        reg_val |= BIT_2;

    if(f_trc0_enable)
        reg_val |= BIT_4;

    if(f_trc1_enable)
        reg_val |= BIT_5;

    if(f_trc2_enable)
        reg_val |= BIT_6;

    if(f_arc4rc0_enable)
        reg_val |= BIT_8;

    if(f_arc4rc1_enable)
        reg_val |= BIT_9;

    if(f_arc4rc2_enable)
        reg_val |= BIT_10;

    if(f_fluec_enable)
        reg_val |= BIT_12;

    eip207_write32(device, EIP207_CS_REG_RAM_CTRL, reg_val);
}


static inline void
EIP207_CS_RAM_CTRL_DEFAULT_WR(
        const device_handle_t device)
{
    eip207_write32(device,
                   EIP207_CS_REG_RAM_CTRL,
                   EIP207_CS_REG_RAM_CTRL_DEFAULT);
}


static inline void
EIP207_RC_CTRL_WR(
        const device_handle_t device,
        const u32 cache_base,
        const unsigned int cache_nr,
        const u8 command)
{
    u32 reg_val = EIP207_RC_REG_CTRL_DEFAULT;

    reg_val &= (~MASK_4_BITS);
    reg_val |= (u32)((((u32)command)   & MASK_4_BITS));

    eip207_write32(device, EIP207_RC_REG_CTRL(cache_base, cache_nr), reg_val);
}


static inline void
EIP207_RC_DATA_WR(
        const device_handle_t device,
        const u32 cache_base,
        const unsigned int cache_nr,
        const unsigned int byte_offset,
        const u32 value32)
{
    eip207_write32(device,
                   EIP207_RC_REG_DATA(cache_base + byte_offset, cache_nr),
                   value32);
}


static inline void
EIP207_ICE_PUTF_CTRL_WR(
        const device_handle_t device,
        const unsigned int ce_nr,
        const u8 token_limit,
        const bool f_fifo_reset)
{
    u32 reg_val = 0;
    reg_val |= token_limit & MASK_8_BITS;

    if (f_fifo_reset)
        reg_val |= BIT_15;

    eip207_write32(device, EIP207_ICE_REG_PUTF_CTRL(ce_nr), reg_val);
}


static inline void
EIP207_ICE_PPTF_CTRL_WR(
        const device_handle_t device,
        const unsigned int ce_nr,
        const u8 token_limit,
        const bool f_fifo_reset)
{
    u32 reg_val = 0;
    reg_val |= token_limit & MASK_8_BITS;

    if (f_fifo_reset)
        reg_val |= BIT_15;

    eip207_write32(device, EIP207_ICE_REG_PPTF_CTRL(ce_nr), reg_val);
}


static inline void
EIP207_ICE_SCRATCH_CTRL_WR(
        const device_handle_t device,
        const unsigned int ce_nr,
        const bool f_change_timer,
        const bool f_timer_enable,
        const u16 timer_prescaler,
        const u8 timer_oflo_bit,
        const bool f_change_access,
        const u8 scratch_access)
{
    u32 reg_val = EIP207_ICE_REG_SCRATCH_CTRL_DEFAULT;

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

    eip207_write32(device, EIP207_ICE_REG_SCRATCH_CTRL(ce_nr), reg_val);
}


static inline void
EIP207_ICE_SCRATCH_CTRL_IRQ_CLEAR(
        const device_handle_t device,
        const unsigned int ce_nr)
{
    u32 reg_val = eip207_read32(device, EIP207_ICE_REG_SCRATCH_CTRL(ce_nr));

    // Clear Time Overflow IRQ
    reg_val |= BIT_0;

    eip207_write32(device, EIP207_ICE_REG_SCRATCH_CTRL(ce_nr), reg_val);
}


static inline void
EIP207_ICE_SCRATCH_CTRL_RD(
        const device_handle_t device,
        const unsigned int ce_nr,
        bool * const f_timer_oflo_irq)
{
    u32 reg_val = eip207_read32(device, EIP207_ICE_REG_SCRATCH_CTRL(ce_nr));

    *f_timer_oflo_irq = ((reg_val & BIT_0) != 0);
}


static inline void
EIP207_ICE_RAM_CTRL_WR(
        const device_handle_t device,
        const unsigned int ce_nr,
        const bool f_pue_prog_enable,
        const bool f_fpp_prog_enable)
{
    u32 reg_val = EIP207_ICE_REG_RAM_CTRL_DEFAULT;

    if(f_pue_prog_enable)
        reg_val |= BIT_0;

    if(f_fpp_prog_enable)
        reg_val |= BIT_1;

    eip207_write32(device, EIP207_ICE_REG_RAM_CTRL(ce_nr), reg_val);
}


static inline void
EIP207_FHASH_IV_WR(
        const device_handle_t device,
        const u32 iv_0,
        const u32 iv_1,
        const u32 iv_2,
        const u32 iv_3)
{
    eip207_write32(device, EIP207_FHASH_REG_IV_0, iv_0);
    eip207_write32(device, EIP207_FHASH_REG_IV_1, iv_1);
    eip207_write32(device, EIP207_FHASH_REG_IV_2, iv_2);
    eip207_write32(device, EIP207_FHASH_REG_IV_3, iv_3);
}


// EIP-207 Level0 interface extensions
#include "eip207_level0_ext.h"

// EIP-207 Record Cache Level0 interface extensions
#include "eip207_rc_level0_ext.h"


#endif /* EIP207_LEVEL0_H_ */


/* end of file eip207_level0.h */
