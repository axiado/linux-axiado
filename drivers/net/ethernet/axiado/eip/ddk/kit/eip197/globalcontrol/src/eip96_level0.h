/* eip96_level0.h
 *
 * EIP-96 Packet Engine Level0 internal interface
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

#ifndef EIP96_LEVEL0_H_
#define EIP96_LEVEL0_H_


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Driver Framework Basic Definitions API
#include "basic_defs.h"         // BIT definitions, bool, u32

// EIP-96 Packet Engine HW interface
#include "eip96_hw_interface.h"

// Driver Framework device API
#include "device_types.h"       // device_handle_t
#include "device_rw.h"          // Read32, Write32


/*----------------------------------------------------------------------------
 * Definitions and macros
 */


/*----------------------------------------------------------------------------
 * Local variables
 */


/*----------------------------------------------------------------------------
 * eip96_read32
 *
 * This routine writes to a Register location in the EIP-96.
 */
static inline u32
eip96_read32(
        device_handle_t device,
        const unsigned int offset)
{
    return device_read32(device, offset);
}


/*----------------------------------------------------------------------------
 * eip96_write32
 *
 * This routine writes to a Register location in the EIP-96.
 */
static inline void
eip96_write32(
        device_handle_t device,
        const unsigned int offset,
        const u32 value)
{
    device_write32(device, offset, value);
}


static inline void
EIP96_EIP_REV_RD(
        device_handle_t device,
        const unsigned int p_enr,
        u8 * const eip_number,
        u8 * const complmt_eip_number,
        u8 * const hw_patch_level,
        u8 * const min_hw_revision,
        u8 * const maj_hw_revision)
{
    u32 rev_reg_val;

    rev_reg_val = eip96_read32(device, EIP96_REG_VERSION(p_enr));

    *maj_hw_revision     = (u8)((rev_reg_val >> 24) & MASK_4_BITS);
    *min_hw_revision     = (u8)((rev_reg_val >> 20) & MASK_4_BITS);
    *hw_patch_level      = (u8)((rev_reg_val >> 16) & MASK_4_BITS);
    *complmt_eip_number  = (u8)((rev_reg_val >> 8)  & MASK_8_BITS);;
    *eip_number         = (u8)((rev_reg_val)       & MASK_8_BITS);
}


static inline void
EIP96_OPTIONS_RD(
        device_handle_t device,
        const unsigned int p_enr,
        bool * const f_aes,
        bool * const f_ae_sfb,
        bool * const f_ae_sspeed,
        bool * const f_des,
        bool * const f_de_sfb,
        bool * const f_de_sspeed,
        u8 * const arc4,
        bool * const f_aes_xts,
        bool * const f_wireless,
        bool * const f_md5,
        bool * const f_sha1,
        bool * const f_sha1speed,
        bool * const f_sha224_256,
        bool * const f_sha384_512,
        bool * const f_xcbc_mac,
        bool * const f_cbc_ma_cspeed,
        bool * const f_cbc_ma_ckeylens,
        bool * const f_ghash)
{
    u32 reg_val;

    reg_val = eip96_read32(device, EIP96_REG_OPTIONS(p_enr));

    *f_ghash           = ((reg_val & BIT_30) != 0);
    *f_cbc_ma_ckeylens  = ((reg_val & BIT_29) != 0);
    *f_cbc_ma_cspeed    = ((reg_val & BIT_28) != 0);
    *f_xcbc_mac        = ((reg_val & BIT_27) != 0);
    *f_sha384_512      = ((reg_val & BIT_26) != 0);
    *f_sha224_256      = ((reg_val & BIT_25) != 0);
    *f_sha1speed       = ((reg_val & BIT_24) != 0);
    *f_sha1            = ((reg_val & BIT_23) != 0);
    *f_md5             = ((reg_val & BIT_22) != 0);
    *f_wireless        = ((reg_val & BIT_21) != 0);
    *f_aes_xts         = ((reg_val & BIT_20) != 0);
    *arc4             = (u8)((reg_val >> 18) & MASK_2_BITS);
    *f_de_sspeed        = ((reg_val & BIT_17) != 0);
    *f_de_sfb           = ((reg_val & BIT_16) != 0);
    *f_des             = ((reg_val & BIT_15) != 0);
    *f_ae_sspeed        = ((reg_val & BIT_14) != 0);
    *f_ae_sfb           = ((reg_val & BIT_13) != 0);
    *f_aes             = ((reg_val & BIT_12) != 0);
}


static inline void
EIP96_TOKEN_CTRL_STAT_DEFAULT_WR(
        device_handle_t device,
        const unsigned int p_enr)
{
    eip96_write32(device,
                  EIP96_REG_TOKEN_CTRL_STAT(p_enr),
                  EIP96_REG_TOKEN_CTRL_STAT_DEFAULT);
}


static inline void
EIP96_TOKEN_CTRL_STAT_RD(
        device_handle_t device,
        const unsigned int p_enr,
        u8 * const active_token_count,
        bool * const f_token_location_available,
        bool * const f_result_token_available,
        bool * const f_token_read_active,
        bool * const f_context_cache_active,
        bool * const f_context_fetch,
        bool * const f_result_context,
        bool * const f_processing_held,
        bool * const f_busy)
{
    u32 reg_val;

    reg_val = eip96_read32(device, EIP96_REG_TOKEN_CTRL_STAT(p_enr));

    *f_busy                   = ((reg_val & BIT_15) != 0);
    *f_processing_held         = ((reg_val & BIT_14) != 0);
    *f_result_context          = ((reg_val & BIT_7) != 0);
    *f_context_fetch           = ((reg_val & BIT_6) != 0);
    *f_context_cache_active     = ((reg_val & BIT_5) != 0);
    *f_token_read_active        = ((reg_val & BIT_4) != 0);
    *f_result_token_available   = ((reg_val & BIT_3) != 0);
    *f_token_location_available = ((reg_val & BIT_2) != 0);
    *active_token_count        = (u8)((reg_val) & MASK_2_BITS);
}


static inline void
EIP96_TOKEN_CTRL_STAT_WR(
        device_handle_t device,
        const unsigned int p_enr,
        const bool f_optimal_context_update,
        const bool f_ct_no_token_wait,
        const bool f_absolute_arc4,
        const bool f_allow_reuse_cached,
        const bool f_allow_postponed_reuse,
        const bool f_zero_length_result,
        const bool f_timeout_counter_enable,
        const bool f_extended_error_enable)
{
    u32 reg_val = EIP96_REG_TOKEN_CTRL_STAT_DEFAULT;

    if(f_optimal_context_update)
        reg_val |= BIT_16;

    if(f_ct_no_token_wait)
        reg_val |= BIT_17;

    if(f_absolute_arc4)
        reg_val |= BIT_18;

    if(f_allow_reuse_cached)
        reg_val |= BIT_19;

    if(f_allow_postponed_reuse)
        reg_val |= BIT_20;

    if(f_zero_length_result)
        reg_val |= BIT_21;

    if(f_timeout_counter_enable)
        reg_val |= BIT_22;

    if(f_extended_error_enable)
        reg_val |= BIT_30;

    eip96_write32(device, EIP96_REG_TOKEN_CTRL_STAT(p_enr), reg_val);
}


static inline void
EIP96_TOKEN_CTRL2_WR(
        device_handle_t device,
        const unsigned int p_enr,
        const bool f_adv_fifo_mode,
        const bool f_prep_ovs_chk_dis,
        const bool f_fetch_ovs_chk_dis,
        const bool f_done_pulses)
{
    u32 reg_val = 0;

    if (f_adv_fifo_mode)
        reg_val |= BIT_0;

    if (f_prep_ovs_chk_dis)
        reg_val |= BIT_1;

    if (f_fetch_ovs_chk_dis)
        reg_val |= BIT_2;

    if (f_done_pulses)
        reg_val |= BIT_3;

    eip96_write32(device, EIP96_REG_TOKEN_CTRL2(p_enr), reg_val);
}


static inline void
EIP96_CONTEXT_STAT_RD(
        device_handle_t device,
        const unsigned int p_enr,
        u16 * const error,
        u8 * const available_token_count,
        bool * const f_active_context,
        bool * const f_next_context,
        bool * const f_result_context,
        bool * const f_error_recovery)
{
    u32 reg_val;

    reg_val = eip96_read32(device, EIP96_REG_CONTEXT_STAT(p_enr));

    *f_error_recovery          = ((reg_val & BIT_21) != 0);
    *f_result_context          = ((reg_val & BIT_20) != 0);
    *f_next_context            = ((reg_val & BIT_19) != 0);
    *f_active_context          = ((reg_val & BIT_18) != 0);
    *available_token_count     = (u16)((reg_val >> 16) & MASK_2_BITS);
    *error                   = (u16)((reg_val)       & MASK_16_BITS);
}


static inline void
EIP96_CONTEXT_CTRL_WR(
        device_handle_t device,
        const unsigned int p_enr,
        const u8 context_size,
        bool f_address_mode,
        bool f_control_mode)
{
    u32 reg_val = 0;

    if (f_address_mode)
        reg_val |= BIT_8;

    if (f_control_mode)
        reg_val |= BIT_9;

    reg_val |= context_size & MASK_8_BITS;

    device_write32(device,EIP96_REG_CONTEXT_CTRL(p_enr), reg_val);
}


static inline void
EIP96_OUT_TRANS_CTRL_STAT_RD(
        device_handle_t device,
        const unsigned int p_enr,
        u8 * const available_word32_count,
        u8 * const min_transfer_word_count,
        u8 * const max_transfer_word_count,
        u8 * const transfer_size_mask)
{
    u32 reg_val;

    reg_val = eip96_read32(device, EIP96_REG_OUT_TRANS_CTRL_STAT(p_enr));

    *transfer_size_mask      = (u8)((reg_val >> 24) & MASK_8_BITS);
    *max_transfer_word_count  = (u8)((reg_val >> 16) & MASK_8_BITS);
    *min_transfer_word_count  = (u8)((reg_val >> 8)  & MASK_8_BITS);
    *available_word32_count  = (u8)((reg_val)       & MASK_8_BITS);
}


static inline void
EIP96_OUT_BUF_CTRL_WR(
        device_handle_t device,
        const unsigned int p_enr,
        const u8 hold_output_data,
        const bool f_block_update_append,
        const bool f_len_delta_enable)
{
    u32 reg_val = EIP96_REG_OUT_BUF_CTRL_DEFAULT;

    if (f_block_update_append)
        reg_val |= BIT_31;

    if (f_len_delta_enable)
        reg_val |= BIT_30;

    reg_val |= (u32)((((u32)hold_output_data) & MASK_5_BITS) << 3);

    eip96_write32(device, EIP96_REG_OUT_BUF_CTRL(p_enr), reg_val);
}

static inline void
EIP96_CTX_NUM32_THR_WR(
        device_handle_t device,
        const unsigned int p_enr,
        const u32 threshold)
{
    eip96_write32(device,
                  EIP96_REG_CTX_NUM32_THR(p_enr),
                  threshold);
}

static inline void
EIP96_CTX_NUM64_THR_L_WR(
        device_handle_t device,
        const unsigned int p_enr,
        const u32 threshold_l)
{
    eip96_write32(device,
                  EIP96_REG_CTX_NUM64_THR_L(p_enr),
                  threshold_l);
}

static inline void
EIP96_CTX_NUM64_THR_H_WR(
        device_handle_t device,
        const unsigned int p_enr,
        const u32 threshold_h)
{
    eip96_write32(device,
                  EIP96_REG_CTX_NUM64_THR_H(p_enr),
                  threshold_h);
}

static inline void
EIP96_ECN_TABLE_WR(
        device_handle_t device,
        const unsigned int p_enr,
        const unsigned int index,
        const unsigned int ecn0,
        const unsigned int cle0,
        const unsigned int ecn1,
        const unsigned int cle1,
        const unsigned int ecn2,
        const unsigned int cle2,
        const unsigned int ecn3,
        const unsigned int cle3)
{
    u32 reg_val = 0;

    reg_val |= (ecn0 & MASK_2_BITS);
    reg_val |= (cle0 & MASK_5_BITS) << 2;
    reg_val |= (ecn1 & MASK_2_BITS) << 8;
    reg_val |= (cle1 & MASK_5_BITS) << 10;
    reg_val |= (ecn2 & MASK_2_BITS) << 16;
    reg_val |= (cle2 & MASK_5_BITS) << 18;
    reg_val |= (ecn3 & MASK_2_BITS) << 24;
    reg_val |= (cle3 & MASK_5_BITS) << 26;

    device_write32(device,EIP96_REG_ECN_TABLE(p_enr, index), reg_val);
}

static inline void
EIP96_PRNG_CTRL_DEFAULT_WR(
        device_handle_t device,
        const unsigned int p_enr)
{
    eip96_write32(device,
                  EIP96_REG_PRNG_CTRL(p_enr),
                  EIP96_REG_PRNG_CTRL_DEFAULT);
}


static inline void
EIP96_PRNG_CTRL_WR(
        device_handle_t device,
        const unsigned int p_enr,
        const bool f_enable,
        const bool f_auto_mode)
{
    u32 reg_val = EIP96_REG_PRNG_CTRL_DEFAULT;

    if(f_enable)
        reg_val |= BIT_0;

    if(f_auto_mode)
        reg_val |= BIT_1;

    eip96_write32(device, EIP96_REG_PRNG_CTRL(p_enr), reg_val);
}


static inline void
EIP96_PRNG_STAT_RD(
        device_handle_t device,
        const unsigned int p_enr,
        bool * const f_busy,
        bool * const f_result_ready)
{
    u32 reg_val;

    reg_val = eip96_read32(device, EIP96_REG_PRNG_STAT(p_enr));

    *f_result_ready  = ((reg_val & BIT_1) != 0);
    *f_busy         = ((reg_val & BIT_0) != 0);
}


static inline void
EIP96_PRNG_SEED_L_WR(
        device_handle_t device,
        const unsigned int p_enr,
        const u32 seed_lo)
{
    eip96_write32(device, EIP96_REG_PRNG_SEED_L(p_enr), seed_lo);
}


static inline void
EIP96_PRNG_SEED_H_WR(
        device_handle_t device,
        const unsigned int p_enr,
        const u32 seed_hi)
{
    eip96_write32(device, EIP96_REG_PRNG_SEED_H(p_enr), seed_hi);
}


static inline void
EIP96_PRNG_KEY_0_L_WR(
        device_handle_t device,
        const unsigned int p_enr,
        const u32 key0_lo)
{
    eip96_write32(device, EIP96_REG_PRNG_KEY_0_L(p_enr), key0_lo);
}


static inline void
EIP96_PRNG_KEY_0_H_WR(
        device_handle_t device,
        const unsigned int p_enr,
        const u32 key0_hi)
{
    eip96_write32(device, EIP96_REG_PRNG_KEY_0_H(p_enr), key0_hi);
}


static inline void
EIP96_PRNG_KEY_1_L_WR(
        device_handle_t device,
        const unsigned int p_enr,
        const u32 key1_lo)
{
    eip96_write32(device, EIP96_REG_PRNG_KEY_1_L(p_enr), key1_lo);
}


static inline void
EIP96_PRNG_KEY_1_H_WR(
        device_handle_t device,
        const unsigned int p_enr,
        const u32 key1_hi)
{
    eip96_write32(device, EIP96_REG_PRNG_KEY_1_H(p_enr), key1_hi);
}


static inline void
EIP96_PRNG_LFSR_L_WR(
        device_handle_t device,
        const unsigned int p_enr,
        const u32 lfsr_lo)
{
    eip96_write32(device, EIP96_REG_PRNG_LFSR_L(p_enr), lfsr_lo);
}


static inline void
EIP96_PRNG_LFSR_H_WR(
        device_handle_t device,
        const unsigned int p_enr,
        const u32 lfsr_hi)
{
    eip96_write32(device, EIP96_REG_PRNG_LFSR_H(p_enr), lfsr_hi);
}


#endif /* EIP96_LEVEL0_H_ */


/* end of file eip96_level0.h */
