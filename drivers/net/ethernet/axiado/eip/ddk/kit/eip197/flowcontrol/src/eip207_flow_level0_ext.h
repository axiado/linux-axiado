/* eip207_flow_level0_ext.h
 *
 * EIP-207 Flow Level0 internal interface extensions
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

#ifndef EIP207_FLOW_LEVEL0_EXT_H_
#define EIP207_FLOW_LEVEL0_EXT_H_


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */


/*----------------------------------------------------------------------------
 * Definitions and macros
 */


/*----------------------------------------------------------------------------
 * Local variables
 */


/*----------------------------------------------------------------------------
 * EIP207 Flow Functions
 *
 */

static inline void
EIP207_FLUE_CACHEBASE_LO_WR(
        const device_handle_t device,
        const unsigned int hash_table_nr,
        const u32 addr32_lo)
{
    u32 reg_val = EIP207_FLUE_REG_CACHEBASE_LO_DEFAULT;

    reg_val |= (u32)((((u32)addr32_lo) & (~MASK_2_BITS)));

    eip207_flow_write32(device,
                        EIP207_FLUE_FHT_REG_CACHEBASE_LO(hash_table_nr),
                        reg_val);
}


static inline void
EIP207_FLUE_CACHEBASE_HI_WR(
        const device_handle_t device,
        const unsigned int hash_table_nr,
        const u32 addr32_hi)
{
    eip207_flow_write32(device,
                        EIP207_FLUE_FHT_REG_CACHEBASE_HI(hash_table_nr),
                        addr32_hi);
}


static inline void
EIP207_FLUE_SIZE_UPDATE(
        const device_handle_t device,
        const unsigned int hash_table_nr,
        const u8 table_size)
{
    u32 reg_val =
                eip207_flow_read32(device,
                                   EIP207_FLUE_FHT_REG_SIZE(hash_table_nr));

    reg_val &= (~(MASK_4_BITS << 4));
    reg_val |=
          (u32)((((u32)table_size) & MASK_4_BITS) << 4);

    eip207_flow_write32(device,
                        EIP207_FLUE_FHT_REG_SIZE(hash_table_nr),
                        reg_val);
}


#endif /* EIP207_FLOW_LEVEL0_EXT_H_ */


/* end of file eip207_flow_level0_ext.h */
