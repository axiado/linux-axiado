/* eip207_flow_level0.h
 *
 * EIP-207 Flow Level0 internal interface
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

#ifndef EIP207_FLOW_LEVEL0_H_
#define EIP207_FLOW_LEVEL0_H_


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Default configuration
#include "c_eip207_flow.h"

// Driver Framework Basic Definitions API
#include "basic_defs.h"         // BIT definitions, bool, u32

// EIP-207 Flow HW interface
#include "eip207_flow_hw_interface.h"

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
 * EIP207 Global Register Write/Read Functions
 *
 */

/*----------------------------------------------------------------------------
 * eip207_flow_read32
 *
 * This routine writes to a Register location in the EIP-207.
 */
static inline u32
eip207_flow_read32(
        device_handle_t device,
        const unsigned int offset)
{
    return device_read32(device, offset);
}


/*----------------------------------------------------------------------------
 * eip207_flow_write32
 *
 * This routine writes to a Register location in the EIP-207.
 */
static inline void
eip207_flow_write32(
        device_handle_t device,
        const unsigned int offset,
        const u32 value)
{
    device_write32(device, offset, value);

#ifdef EIP207_FLOW_CLUSTERED_WRITES_DISABLE
    // Prevent clustered write operations, break them with a read operation
    // Note: Reading the EIP207_CS_REG_VERSION register has no side effects!
    eip207_flow_read32(device, EIP207_CS_REG_VERSION);
#endif
}


static inline bool
EIP207_FLUE_SIGNATURE_MATCH(
        const u32 rev)
{
    return (((u16)rev) == EIP207_FLUE_SIGNATURE);
}


static inline void
EIP207_FLUE_HASHBASE_LO_WR(
        const device_handle_t device,
        const unsigned int hash_table_nr,
        const u32 addr32_lo)
{
    u32 reg_val = EIP207_FLUE_REG_HASHBASE_LO_DEFAULT;

    reg_val |= (u32)((((u32)addr32_lo) & (~MASK_2_BITS)));

    eip207_flow_write32(device,
                        EIP207_FLUE_FHT_REG_HASHBASE_LO(hash_table_nr),
                        reg_val);
}


static inline void
EIP207_FLUE_HASHBASE_HI_WR(
        const device_handle_t device,
        const unsigned int hash_table_nr,
        const u32 addr32_hi)
{
    eip207_flow_write32(device,
                        EIP207_FLUE_FHT_REG_HASHBASE_HI(hash_table_nr),
                        addr32_hi);
}


static inline void
EIP207_FHASH_IV_RD(
        const device_handle_t device,
        const unsigned int hash_table_nr,
        u32 * const iv_0,
        u32 * const iv_1,
        u32 * const iv_2,
        u32 * const iv_3)
{
    *iv_0 = eip207_flow_read32(device, EIP207_FHASH_REG_IV_0(hash_table_nr));
    *iv_1 = eip207_flow_read32(device, EIP207_FHASH_REG_IV_1(hash_table_nr));
    *iv_2 = eip207_flow_read32(device, EIP207_FHASH_REG_IV_2(hash_table_nr));
    *iv_3 = eip207_flow_read32(device, EIP207_FHASH_REG_IV_3(hash_table_nr));
}


// EIP-207 Flow Level0 interface extensions
#include "eip207_flow_level0_ext.h"


#endif /* EIP207_FLOW_LEVEL0_H_ */


/* end of file eip207_flow_level0.h */
