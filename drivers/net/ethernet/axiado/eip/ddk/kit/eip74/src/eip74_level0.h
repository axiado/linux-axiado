/* eip74_level0.h
 *
 * This file contains all the macros and functions that allow
 * access to the EIP74 registers and to build the values
 * read or written to the registers.
 *
 */

/*****************************************************************************
* Copyright (c) 2017-2020 by Rambus, Inc. and/or its subsidiaries.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are
* met:
*
* 1. Redistributions of source code must retain the above copyright
* notice, this list of conditions and the following disclaimer.
*
* 2. Redistributions in binary form must reproduce the above copyright
* notice, this list of conditions and the following disclaimer in the
* documentation and/or other materials provided with the distribution.
*
* 3. Neither the name of the copyright holder nor the names of its
* contributors may be used to endorse or promote products derived from
* this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
* A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
* HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
* THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*****************************************************************************/

#ifndef EIP74_LEVEL0_H_
#define EIP74_LEVEL0_H_

/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Default configuration
#include "c_eip74.h"

// Register addresses
#include "eip74_hw_interface.h"

// Driver Framework Basic Definitions API
#include "basic_defs.h"         // BIT definitions, bool, u32

// Driver Framework device API
#include "device_types.h"       // device_handle_t
#include "device_rw.h"          // Read32, Write32


/*----------------------------------------------------------------------------
 * Definitions and macros
 */


/*----------------------------------------------------------------------------
 * eip74_read32
 *
 * This routine writes to a Register location in the EIP74.
 */
static inline u32
eip74_read32(
        device_handle_t device,
        const unsigned int offset)
{
    return device_read32(device, offset);
}


/*----------------------------------------------------------------------------
 * eip74_write32
 *
 * This routine writes to a Register location in the EIP74.
 */
static inline void
eip74_write32(
        device_handle_t device,
        const unsigned int offset,
        const u32 value)
{
    device_write32(device, offset, value);
}


static inline void
EIP74_INPUT_WR(
        device_handle_t device,
        const u32 * const input_p)
{
    unsigned i;
    for (i=0; i<4; i++)
    {
        eip74_write32(device,
                      EIP74_REG_INPUT_0 + i*sizeof(u32),
                      input_p[i]);
    }
}

static inline void
EIP74_OUTPUT_RD(
        device_handle_t device,
        u32 * const output_p)
{
    unsigned i;
    for (i=0; i<4; i++)
    {
        output_p[i] =
            eip74_read32(device, EIP74_REG_OUTPUT_0 + i*sizeof(u32));
    }
}


static inline void
EIP74_CONTROL_WR(
        device_handle_t device,
        const bool f_ready_mask,
        const bool f_stuck_out_mask,
        const bool f_test_mode,
        const bool f_host_mode,
        const bool f_enable_drbg,
        const bool f_force_stuck_out,
        const bool f_request_data,
        const u16 data_blocks)
{
    u32 reg_val = EIP74_REG_CONTROL_DEFAULT;

    if (f_request_data)
    {   // RequestData bit leaves all other control settings unchanged.
        reg_val |= BIT_16;
        reg_val |= (data_blocks & MASK_12_BITS) << 20;
    }
    else
    {
        if (f_ready_mask)
            reg_val |= BIT_0;

        if (f_stuck_out_mask)
            reg_val |= BIT_2;

        if (f_test_mode)
            reg_val |= BIT_8;

        if (f_host_mode)
            reg_val |= BIT_9;

        if (f_enable_drbg)
            reg_val |= BIT_10;

        if (f_force_stuck_out)
            reg_val |= BIT_11;
    }


    eip74_write32(device, EIP74_REG_CONTROL, reg_val);
}


static inline void
EIP74_CONTROL_RD(
        device_handle_t device,
        bool * const f_reseed)
{
    u32 reg_val = eip74_read32(device, EIP74_REG_CONTROL);

    *f_reseed = (reg_val & BIT_15) != 0;
}


static inline void
EIP74_STATUS_RD(
        device_handle_t device,
        bool * const f_ready,
        bool * const f_psai_write_ok,
        bool * const f_stuck_out,
        bool * const f_early_reseed,
        bool * const f_test_ready,
        bool * const f_gen_pkt_error,
        bool * const f_instantiated,
        bool * const f_test_stuck_out,
        u8 * const blocks_available,
        bool * const f_need_clock)
{
    u32 reg_val =  eip74_read32(device, EIP74_REG_STATUS);

    *f_ready          = (reg_val & BIT_0) != 0;
    *f_psai_write_ok    = (reg_val & BIT_1) != 0;
    *f_stuck_out       = (reg_val & BIT_2) != 0;
    *f_early_reseed    = (reg_val & BIT_7) != 0;
    *f_test_ready      = (reg_val & BIT_8) != 0;
    *f_gen_pkt_error    = (reg_val & BIT_9) != 0;
    *f_instantiated   = (reg_val & BIT_10) != 0;
    *f_test_stuck_out   = (reg_val & BIT_15) != 0;

    *blocks_available = (reg_val >> 16) & MASK_8_BITS;

    *f_need_clock      = (reg_val & BIT_31) != 0;
}


/* Write TRNG_INTACK register (write-only) */
static inline void
EIP74_INTACK_WR(
        device_handle_t device,
        const bool f_ready_ack,
        const bool f_stuck_out_ack,
        const bool f_test_stuck_out)
{
    u32 reg_val = EIP74_REG_INTACK_DEFAULT;

    if (f_ready_ack)
        reg_val |= BIT_0;

    if (f_stuck_out_ack)
        reg_val |= BIT_2;

    if (f_test_stuck_out)
        reg_val |= BIT_15;

    eip74_write32(device, EIP74_REG_INTACK, reg_val);
}


static inline void
EIP74_GENERATE_CNT_RD(
        device_handle_t device,
        u32 * const value_p)
{
    *value_p = eip74_read32(device, EIP74_REG_GENERATE_CNT);
}


static inline void
EIP74_RESEED_THR_WR(
        device_handle_t device,
        const u32 value)
{
    eip74_write32(device, EIP74_REG_RESEED_THR, value);
}


static inline void
EIP74_RESEED_THR_RD(
        device_handle_t device,
        u32 * const value)
{
    *value = eip74_read32(device, EIP74_REG_RESEED_THR);
}


static inline void
EIP74_RESEED_THR_EARLY_WR(
        device_handle_t device,
        const u32 value)
{
    eip74_write32(device, EIP74_REG_RESEED_THR_EARLY, value);
}


static inline void
EIP74_RESEED_THR_EARLY_RD(
        device_handle_t device,
        u32 * const value)
{
    *value = eip74_read32(device, EIP74_REG_RESEED_THR_EARLY);
}


static inline void
EIP74_GEN_BLK_SIZE_WR(
        device_handle_t device,
        const u32 value)
{
    u32 reg_val = EIP74_REG_GEN_BLK_CNT_DEFAULT;

    reg_val |= value & MASK_12_BITS;
    eip74_write32(device, EIP74_REG_GEN_BLK_SIZE, reg_val);
}


static inline bool
EIP74_REV_SIGNATURE_MATCH(
        const u32 rev)
{
    return (((u16)rev) == EIP74_SIGNATURE);
}


static inline void
EIP74_VERSION_RD(
        device_handle_t device,
        u8 * const eip_number,
        u8 * const complmt_eip_number,
        u8 * const hw_patch_level,
        u8 * const min_hw_revision,
        u8 * const maj_hw_revision)
{
    u32 rev_reg_val;

    rev_reg_val = eip74_read32(device, EIP74_REG_VERSION);

    *maj_hw_revision     = (u8)((rev_reg_val >> 24) & 0x0f);
    *min_hw_revision     = (u8)((rev_reg_val >> 20) & 0x0f);
    *hw_patch_level      = (u8)((rev_reg_val >> 16) & 0x0f);
    *complmt_eip_number  = (u8)((rev_reg_val >> 8)  & 0xff);
    *eip_number         = (u8)((rev_reg_val)       & 0xff);
}


static inline void
EIP74_OPTIONS_RD(
        device_handle_t device,
        u8 * const client_count,
        u8 * const aes_core_count,
        u8 * const aes_core_speed,
        u8 * const fifo_depth)
{
    u32 reg_val;

    reg_val = eip74_read32(device, EIP74_REG_OPTIONS);

    *client_count = reg_val & MASK_6_BITS;
    *aes_core_count = (reg_val >> 8) & MASK_2_BITS;
    *aes_core_speed = (reg_val >> 10) & MASK_2_BITS;
    *fifo_depth = (reg_val >> 16) & MASK_8_BITS;
}


/* Write TRNG_TEST register */
static inline void
EIP74_TEST_WR(
        device_handle_t device,
        const bool f_test_aes256,
        const bool f_test_sp80090,
        const bool f_test_irq)
{
    u32 value = EIP74_REG_TEST_DEFAULT;

    if (f_test_aes256)
        value |= BIT_6;

    if (f_test_sp80090)
        value |= BIT_7;

    if (f_test_irq)
        value |= BIT_31;

    eip74_write32(device, EIP74_REG_TEST, value);
}


/* Read TRNG_TEST register */
static inline void
EIP74_TEST_RD(
        device_handle_t device,
        bool * const f_test_aes256,
        bool * const f_test_sp80090,
        bool * const f_test_irq)
{
    u32 value = eip74_read32(device, EIP74_REG_TEST);

    *f_test_aes256 = (value & BIT_6) != 0;
    *f_test_sp80090 = (value & BIT_7) != 0;
    *f_test_irq = (value & BIT_31) != 0;
}


/* Write Key registers */
static inline void
EIP74_KEY_WR(
        device_handle_t device,
        const u32 * data_p,
        const unsigned int word_count)
{
    unsigned int i;

    for(i = 0; i < word_count; i++)
        eip74_write32(device,
                      EIP74_REG_KEY_0 + i * sizeof(u32),
                      data_p[i]);
}


/* Write PS_AI registers */
static inline void
EIP74_PS_AI_WR(
        device_handle_t device,
        const u32 * data_p,
        const unsigned int word_count)
{
    unsigned int i;

    for(i = 0; i < word_count; i++)
        eip74_write32(device,
                      EIP74_REG_PS_AI_0 + i * sizeof(u32),
                      data_p[i]);
}


#endif /* EIP74_LEVEL0_H_ */

/* end of file eip74_level0.h */
