/* eip202_rd_format.c
 *
 * EIP-202 Ring Control Driver Library
 * Result Descriptor internal interface
 *
 * This module contains the EIP-202 Result Descriptor specific functionality
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

#include "eip202_rd_format.h"

// Descriptor I/O Driver Library API implementation
#include "eip202_rdr.h"                 // eip202_arm_command_descriptor_t


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
 * eip202_rd_make_control_word
 */
u32
eip202_rd_make_control_word(
        const u8 expected_result_word_count,
        const u32 prep_segment_byte_count,
        const bool f_first_segment,
        const bool f_last_segment)
{
    u32 value = 0;

    if(f_first_segment)
        value |= BIT_23;

    if(f_last_segment)
        value |= BIT_22;

    value |= ((((u32)expected_result_word_count) & MASK_8_BITS) << 24);
    value |= ((((u32)prep_segment_byte_count)    & MASK_20_BITS));

    return value;
}


/*----------------------------------------------------------------------------
 * eip202_prepared_write
 */
void
eip202_prepared_write(
        dma_resource_handle_t handle,
        const unsigned int word_offset,
        const eip202_arm_prepared_descriptor_t * const descr_p)
{
#ifdef EIP202_64BIT_DEVICE
    // Write Control Word
    dma_resource_write32(handle, word_offset, descr_p->prep_control_word);

    // Do not support lengths greater than 20 bit.
    dma_resource_write32(handle, word_offset + 1, 0);

    // Write Destination Packet data address
    dma_resource_write32(handle, word_offset + 2, descr_p->dst_packet_addr.addr);
    dma_resource_write32(handle, word_offset + 3, descr_p->dst_packet_addr.upper_addr);

#else
    // Write Control Word
    dma_resource_write32(handle, word_offset, descr_p->prep_control_word);

    // Write Destination Packet data address
    dma_resource_write32(handle, word_offset + 1, descr_p->dst_packet_addr.addr);
#endif

    return;
}


/*----------------------------------------------------------------------------
 * eip202_read_descriptor
 */
void
eip202_read_descriptor(
        eip202_arm_result_descriptor_t * const descr_p,
        const dma_resource_handle_t handle,
        const unsigned int word_offset,
        const unsigned int dscr_offs_word_count,
        const unsigned int token_offs_word_count,
        bool * const f_last_segment,
        bool * const f_first_segment)
{
    unsigned int out_token_word_offset;

#ifdef EIP202_64BIT_DEVICE
    // Word 0 - Control Word
    descr_p->proc_control_word = dma_resource_read32(handle, word_offset);

    // Word 1 - extended length, not read.

    // Word 2 & 3 - Destination Packet data Buffer address
    descr_p->dst_packet_addr.addr = dma_resource_read32(handle, word_offset + 2);
    descr_p->dst_packet_addr.upper_addr = dma_resource_read32(handle, word_offset + 3);

    out_token_word_offset = word_offset + token_offs_word_count;
#else // EIP202_64BIT_DEVICE
    // Word 0 - Control Word
    descr_p->proc_control_word = dma_resource_read32(handle, word_offset);

    // Word 1 - Destination Packet data Buffer address
    descr_p->dst_packet_addr.addr = dma_resource_read32(handle, word_offset + 1);

    out_token_word_offset = word_offset + 2;
#endif // !EIP202_64BIT_DEVICE

    if (descr_p->token_p == NULL)
    {
        *f_last_segment   = false;
        *f_first_segment  = false;
        return; // Fatal error
    }

    // Read token data
    {
        unsigned int i;

#ifdef EIP202_RING_ANTI_DMA_RACE_CONDITION_CDS
        for (i = 0; i < io_token_out_word_count_get(); i++)
            if (i != (unsigned int)io_token_out_mark_offset_get())
                descr_p->token_p[i] = dma_resource_read32(
                                          handle,
                                          out_token_word_offset + i);
#else
        for (i = 0; i < io_token_out_word_count_get(); i++)
            descr_p->token_p[i] = dma_resource_read32(
                                    handle,
                                    out_token_word_offset + i);
#endif
    }

    // Check if this descriptor is for the last segment
    if((descr_p->proc_control_word & BIT_22) != 0)
        *f_last_segment = true; // Processed packet
    else
        *f_last_segment = false;

    // Check if this descriptor is for the first segment
    if((descr_p->proc_control_word & BIT_23) != 0)
        *f_first_segment = true; // New packet descriptor chain detected
    else
        *f_first_segment = false;

    IDENTIFIER_NOT_USED(dscr_offs_word_count);

    return;
}


/*----------------------------------------------------------------------------
 * eip202_clear_descriptor
 */
void
eip202_clear_descriptor(
        eip202_arm_result_descriptor_t * const descr_p,
        const dma_resource_handle_t handle,
        const unsigned int word_offset,
        const unsigned int token_offs_word_count,
        const unsigned int dscr_word_count)
{
    IDENTIFIER_NOT_USED(descr_p);

#if defined(EIP202_RDR_OWNERSHIP_WORD_ENABLE)

    dma_resource_write32(handle, word_offset + dscr_word_count - 1, 0);
    IDENTIFIER_NOT_USED(token_offs_word_count);

#elif defined(EIP202_RING_ANTI_DMA_RACE_CONDITION_CDS)

    IDENTIFIER_NOT_USED(dscr_word_count);

    dma_resource_write32(handle,
                        word_offset + token_offs_word_count +
                                  io_token_out_mark_offset_get(),
                        0);
#else
    IDENTIFIER_NOT_USED(handle);
    IDENTIFIER_NOT_USED(word_offset);
    IDENTIFIER_NOT_USED(dscr_word_count);
    IDENTIFIER_NOT_USED(token_offs_word_count);
#endif // !EIP202_RDR_OWNERSHIP_WORD_ENABLE
}


/*----------------------------------------------------------------------------
 * eip202_rd_read_control_word
 */
void
eip202_rd_read_control_word(
        const u32 control_word,
        u32 * token_data_p,
        eip202_rdr_result_control_t * const rd_control_p,
        eip202_rdr_result_token_t * const res_token_p)
{
    rd_control_p->proc_segment_byte_count =  (control_word        & MASK_20_BITS);
    rd_control_p->proc_result_word_count  = ((control_word >> 24) & MASK_8_BITS);

    // Fill in eip202_rdr_result_control_t
    if((control_word & BIT_20) != 0)
        rd_control_p->f_dscr_overflow = true;
    else
        rd_control_p->f_dscr_overflow = false;

    if((control_word & BIT_21) != 0)
        rd_control_p->f_buffer_overflow = true;
    else
        rd_control_p->f_buffer_overflow = false;

    if((control_word & BIT_22) != 0)
        rd_control_p->f_last_segment = true;
    else
        rd_control_p->f_last_segment = false;

    if((control_word & BIT_23) != 0)
        rd_control_p->f_first_segment = true;
    else
        rd_control_p->f_first_segment = false;

    IDENTIFIER_NOT_USED(token_data_p);
    IDENTIFIER_NOT_USED(res_token_p);
}


/*----------------------------------------------------------------------------
 * eip202_rd_read_bypass_data
 */
void
eip202_rd_read_bypass_data(
        const u32 * bypass_data_p,
        const u8 bypass_word_count,
        eip202_rdr_bypass_data_t * const bd_p)
{
    if (bypass_word_count == 1)
    {
        bd_p->fail.error_flags = bypass_data_p[0] & MASK_2_BITS;
    }
    else if (bypass_word_count == 2)
    {
        bd_p->pass.TOS_TC           = bypass_data_p[0] & MASK_8_BITS;
        bd_p->pass.f_df              = ((bypass_data_p[0] & BIT_8) != 0);
        bd_p->pass.next_header_offset = (bypass_data_p[0] >> 8) & MASK_16_BITS;
        bd_p->pass.hdr_proc_ctx_ref    = bypass_data_p[1];
    }
    else
    {
        IDENTIFIER_NOT_USED(bypass_data_p);
        IDENTIFIER_NOT_USED(bypass_word_count);
        IDENTIFIER_NOT_USED(bd_p);
    }
}


/* end of file eip202_rd_format.c */
