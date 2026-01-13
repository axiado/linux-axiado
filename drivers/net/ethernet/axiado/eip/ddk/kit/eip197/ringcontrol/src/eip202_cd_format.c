/* eip202_cd_format.c
 *
 * EIP-202 Ring Control Driver Library
 * command Descriptor internal interface
 *
 * This module contains the EIP-202 command Descriptor specific functionality
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

/*----------------------------------------------------------------------------
 * This module implements (provides) the following interface(s):
 */

#include "eip202_cd_format.h"

// Descriptor I/O Driver Library API implementation
#include "eip202_cdr.h"                 // eip202_arm_command_descriptor_t


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Default configuration
#include "c_eip202_ring.h"

// EIP-202 Ring Control Driver Library internal interfaces
#include "eip202_ring_internal.h"

// Driver Framework Basic Definitions API
#include "basic_defs.h"                // bool, u32, u8

// Driver Framework DMA Resource API
#include "dmares_types.h"              // dma_resource_handle_t
#include "dmares_rw.h"                 // DMAResource_Write/Read

// Standard IOToken API
#include "iotoken.h"


/*----------------------------------------------------------------------------
 * Definitions and macros
 */


/*----------------------------------------------------------------------------
 * Local variables
 */


/*----------------------------------------------------------------------------
 * eip202_cd_make_control_word
 */
u32
eip202_cd_make_control_word(
        const u8 token_word_count,
        const u32 segment_byte_count,
        const bool f_first_segment,
        const bool f_last_segment,
        const bool f_force_engine,
        const u8 engine_id)
{
    u32 value = 0;

    if(f_first_segment)
        value |= BIT_23;

    if(f_last_segment)
        value |= BIT_22;

    value |= ((((u32)token_word_count) & MASK_8_BITS) << 24);
    value |= ((((u32)segment_byte_count) & MASK_16_BITS));
    if (f_force_engine)
        value |= BIT_21 | (((u32)engine_id & MASK_5_BITS) << 16);

    return value;
}


/*----------------------------------------------------------------------------
 * eip202_cd_write
 */
void
eip202_cd_write(
        dma_resource_handle_t handle,
        const unsigned int word_offset,
        const eip202_arm_command_descriptor_t * const descr_p,
        const bool f_atp)

{
    unsigned int in_token_word_offset;

#ifdef EIP202_RING_ANTI_DMA_RACE_CONDITION_CDS
    io_token_mark_set(descr_p->token_p);
#endif

#ifdef EIP202_64BIT_DEVICE
    // Write Control Word
    dma_resource_write32(handle, word_offset, descr_p->control_word);

    // Lengths greater than 20 bits not supported yet.
#ifndef EIP202_CDR_OPT1
    dma_resource_write32(handle, word_offset + 1, 0);
#endif

    // Write source Packet data address
    dma_resource_write32(handle, word_offset + 2, descr_p->src_packet_addr.addr);
    dma_resource_write32(handle, word_offset + 3, descr_p->src_packet_addr.upper_addr);

    if (f_atp)
    {
#ifndef EIP202_CDR_OPT2
        // Write Token data address
        dma_resource_write32(handle,
                            word_offset + 4,
                            descr_p->token_data_addr.addr);
        dma_resource_write32(handle,
                            word_offset + 5,
                            descr_p->token_data_addr.upper_addr);
#endif
        in_token_word_offset = word_offset + 6;
    }
    else
        in_token_word_offset = word_offset + 4;
#else // EIP202_64BIT_DEVICE
    // Write Control Word
    dma_resource_write32(handle, word_offset, descr_p->control_word);

    // Write source Packet data address
    dma_resource_write32(handle, word_offset + 1, descr_p->src_packet_addr.addr);

    if (f_atp)
    {
#ifndef EIP202_CDR_OPT2
        // Write Token data address
        dma_resource_write32(handle, word_offset + 2, descr_p->token_data_addr.addr);
#endif
        in_token_word_offset = word_offset + 3;
    }
    else
        in_token_word_offset = word_offset + 2;
#endif // !EIP202_64BIT_DEVICE

    // Write Input Token (only for the first segment and if token is available)
    if (descr_p->control_word & BIT_23 && descr_p->token_p)
    {
        unsigned int i, offset = in_token_word_offset;

        // Write Application ID
#ifdef EIP202_RING_ANTI_DMA_RACE_CONDITION_CDS
        io_token_mark_set(descr_p->token_p);
#endif

        for (i = 0; i < io_token_in_word_count_get(); i++)
            dma_resource_write32(handle, offset + i, descr_p->token_p[i]);
    }

    return;
}


/* end of file eip202_cd_format.c */
