/* eip206_level0.h
 *
 * EIP-206 Processing Engine Level0 internal interface
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

#ifndef EIP206_LEVEL0_H_
#define EIP206_LEVEL0_H_


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Driver Framework Basic Definitions API
#include "basic_defs.h"         // BIT definitions, bool, u32

// EIP-206 HW interface
#include "eip206_hw_interface.h"

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
 * eip206_read32
 *
 * This routine writes to a Register location in the EIP-206.
 */
static inline u32
eip206_read32(
        device_handle_t device,
        const unsigned int offset)
{
    return device_read32(device, offset);
}


/*----------------------------------------------------------------------------
 * eip206_write32
 *
 * This routine writes to a Register location in the EIP-206.
 */
static inline void
eip206_write32(
        device_handle_t device,
        const unsigned int offset,
        const u32 value)
{
    device_write32(device, offset, value);
}


static inline bool
EIP206_REV_SIGNATURE_MATCH(
        const u32 rev)
{
    return (((u16)rev) == EIP206_SIGNATURE);
}


static inline void
EIP206_EIP_REV_RD(
        device_handle_t device,
        const unsigned int p_enr,
        u8 * const eip_number,
        u8 * const complmt_eip_number,
        u8 * const hw_patch_level,
        u8 * const min_hw_revision,
        u8 * const maj_hw_revision)
{
    u32 rev_reg_val;

    rev_reg_val = eip206_read32(device, EIP206_REG_VERSION(p_enr));

    *maj_hw_revision     = (u8)((rev_reg_val >> 24) & MASK_4_BITS);
    *min_hw_revision     = (u8)((rev_reg_val >> 20) & MASK_4_BITS);
    *hw_patch_level      = (u8)((rev_reg_val >> 16) & MASK_4_BITS);
    *complmt_eip_number  = (u8)((rev_reg_val >> 8)  & MASK_8_BITS);
    *eip_number         = (u8)((rev_reg_val)       & MASK_8_BITS);
}


static inline void
EIP206_OPTIONS_RD(
        device_handle_t device,
        const unsigned int p_enr,
        u8 * const pe_type,
        u8 * const in_classifier,
        u8 * const out_classifier,
        u8 * const nof_mac_channels,
        u8 * const in_dbuf_size_kb,
        u8 * const in_tbuf_size_kb,
        u8 * const out_dbuf_size_kb,
        u8 * const out_tbuf_size_kb)
{
    u32 rev_reg_val;

    rev_reg_val = eip206_read32(device, EIP206_REG_OPTIONS(p_enr));

    *out_tbuf_size_kb   = (u8)((rev_reg_val >> 28) & MASK_4_BITS);
    *out_dbuf_size_kb   = (u8)((rev_reg_val >> 24) & MASK_4_BITS);
    *in_tbuf_size_kb    = (u8)((rev_reg_val >> 20) & MASK_4_BITS);
    *in_dbuf_size_kb    = (u8)((rev_reg_val >> 16) & MASK_4_BITS);
    *nof_mac_channels = (u8)((rev_reg_val >> 12) & MASK_4_BITS);
    *out_classifier   = (u8)((rev_reg_val >> 10) & MASK_2_BITS);
    *in_classifier    = (u8)((rev_reg_val >> 8)  & MASK_2_BITS);
    *pe_type         = (u8)((rev_reg_val)       & MASK_8_BITS);
}


static inline void
EIP206_IN_DEBUG_WR(
        device_handle_t device,
        const unsigned int p_enr,
        const bool f_bypass,
        const bool f_in_flight_clear,
        const bool f_meta_enb)
{
    u32 reg_val = 0;

    if (f_bypass)
        reg_val |= BIT_0;

    if (f_in_flight_clear)
        reg_val |= BIT_16;

    if (f_meta_enb)
        reg_val |= BIT_31;
    eip206_write32(device,EIP206_REG_DEBUG(p_enr),reg_val);
}


static inline void
EIP206_IN_DBUF_THRESH_DEFAULT_WR(
        device_handle_t device,
        const unsigned int p_enr)
{
    eip206_write32(device,
                   EIP206_IN_REG_DBUF_TRESH(p_enr),
                   EIP206_IN_REG_DBUF_TRESH_DEFAULT);
}


static inline void
EIP206_IN_DBUF_THRESH_WR(
        device_handle_t device,
        const unsigned int p_enr,
        const u8 pkt_tresh,
        const u8 min_tresh,
        const u8 max_tresh)
{
    u32 reg_val = EIP206_IN_REG_DBUF_TRESH_DEFAULT;

    reg_val |= (u32)((((u32)pkt_tresh) & MASK_8_BITS));
    reg_val |= (u32)((((u32)max_tresh) & MASK_4_BITS) << 12);
    reg_val |= (u32)((((u32)min_tresh) & MASK_4_BITS) << 8);

    eip206_write32(device, EIP206_IN_REG_DBUF_TRESH(p_enr), reg_val);
}


static inline void
EIP206_IN_TBUF_THRESH_DEFAULT_WR(
        device_handle_t device,
        const unsigned int p_enr)
{
    eip206_write32(device,
                   EIP206_IN_REG_TBUF_TRESH(p_enr),
                   EIP206_IN_REG_TBUF_TRESH_DEFAULT);
}


static inline void
EIP206_IN_TBUF_THRESH_WR(
        device_handle_t device,
        const unsigned int p_enr,
        const u8 pkt_tresh,
        const u8 min_tresh,
        const u8 max_tresh)
{
    u32 reg_val = EIP206_IN_REG_TBUF_TRESH_DEFAULT;

    reg_val |= (u32)((((u32)pkt_tresh) & MASK_8_BITS));
    reg_val |= (u32)((((u32)max_tresh) & MASK_4_BITS) << 12);
    reg_val |= (u32)((((u32)min_tresh) & MASK_4_BITS) << 8);

    eip206_write32(device, EIP206_IN_REG_TBUF_TRESH(p_enr), reg_val);
}


static inline void
EIP206_OUT_DBUF_THRESH_DEFAULT_WR(
        device_handle_t device,
        const unsigned int p_enr)
{
    eip206_write32(device,
                   EIP206_OUT_REG_DBUF_TRESH(p_enr),
                   EIP206_OUT_REG_DBUF_TRESH_DEFAULT);
}


static inline void
EIP206_OUT_DBUF_THRESH_WR(
        device_handle_t device,
        const unsigned int p_enr,
        const u8 min_tresh,
        const u8 max_tresh)
{
    u32 reg_val = EIP206_OUT_REG_DBUF_TRESH_DEFAULT;

    reg_val |= (u32)((((u32)max_tresh) & MASK_4_BITS) << 4);
    reg_val |= (u32)((((u32)min_tresh) & MASK_4_BITS));

    eip206_write32(device, EIP206_OUT_REG_DBUF_TRESH(p_enr), reg_val);
}


static inline void
EIP206_OUT_TBUF_THRESH_DEFAULT_WR(
        device_handle_t device,
        const unsigned int p_enr)
{
    eip206_write32(device,
                   EIP206_OUT_REG_TBUF_TRESH(p_enr),
                   EIP206_OUT_REG_TBUF_TRESH_DEFAULT);
}


static inline void
EIP206_OUT_TBUF_THRESH_WR(
        device_handle_t device,
        const unsigned int p_enr,
        const u8 min_tresh,
        const u8 max_tresh)
{
    u32 reg_val = EIP206_OUT_REG_TBUF_TRESH_DEFAULT;

    reg_val |= (u32)((((u32)max_tresh) & MASK_4_BITS) << 4);
    reg_val |= (u32)((((u32)min_tresh) & MASK_4_BITS));

    eip206_write32(device, EIP206_OUT_REG_TBUF_TRESH(p_enr), reg_val);
}

static inline void
EIP206_ARC4_SIZE_WR(
        device_handle_t device,
        const unsigned int p_enr,
        const bool f_large)
{
    u32 reg_val = EIP206_ARC4_REG_SIZE_DEFAULT;

    if(f_large)
        reg_val |= BIT_0;

    eip206_write32(device, EIP206_ARC4_REG_SIZE(p_enr), reg_val);
}



#endif /* EIP206_LEVEL0_H_ */


/* end of file eip206_level0.h */
