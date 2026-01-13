/* eip202_rd_format.h
 *
 * EIP-202 Ring Control Driver Library API Result Descriptor internal interface
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

#ifndef EIP202_RD_FORMAT_H_
#define EIP202_RD_FORMAT_H_


/*----------------------------------------------------------------------------
 * This module implements (provides) the following interface(s):
 */

// Descriptor I/O Driver Library API implementation
#include "eip202_rdr.h"                // eip202_arm_command_descriptor_t


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Driver Framework Basic Definitions API
#include "basic_defs.h"                // bool, u32, u8

// Driver Framework DMA Resource API
#include "dmares_types.h"              // dma_resource_handle_t


/*----------------------------------------------------------------------------
 * Definitions and macros
 */


/*----------------------------------------------------------------------------
 * Local variables
 */

/*----------------------------------------------------------------------------
 * eip202_rd_make_control_word
 *
 * This helper function returns the Control Word that can be written to
 * the EIP-202 Prepared Descriptor.
 *
 * This function is re-entrant.
 *
 */
u32
eip202_rd_make_control_word(
        const u8 expected_result_word_count,
        const u32 prep_segment_byte_count,
        const bool f_first_segment,
        const bool f_last_segment);


/*----------------------------------------------------------------------------
 * eip202_prepared_write
 *
 * This helper function writes the EIP-202 Logical Prepared Descriptor to the RDR
 *
 * This function is not re-entrant.
 *
 */
void
eip202_prepared_write(
        dma_resource_handle_t handle,
        const unsigned int word_offset,
        const eip202_arm_prepared_descriptor_t * const descr_p);


/*----------------------------------------------------------------------------
 * eip202_read_descriptor
 *
 * This helper function reads the EIP-202 Result Descriptor from the RDR
 *
 * This function is not re-entrant.
 *
 */
void
eip202_read_descriptor(
        eip202_arm_result_descriptor_t * const descr_p,
        const dma_resource_handle_t handle,
        const unsigned int word_offset,
        const unsigned int desc_offset_word_count,
        const unsigned int token_offs_word_count,
        bool * const f_last_segment,
        bool * const f_first_segment);


/*----------------------------------------------------------------------------
 * eip202_clear_descriptor
 *
 * This helper function clears the EIP-202 Result Descriptor in the RDR
 *
 * This function is not re-entrant.
 *
 */
void
eip202_clear_descriptor(
        eip202_arm_result_descriptor_t * const descr_p,
        const dma_resource_handle_t handle,
        const unsigned int word_offset,
        const unsigned int token_offs_word_count,
        const unsigned int dscr_word_count);


/*----------------------------------------------------------------------------
 * eip202_rd_read_control_word
 *
 * This helper function reads the EIP-202 Result Descriptor Control Word
 * and Result Token data
 *
 * This function is not re-entrant.
 *
 */
void
eip202_rd_read_control_word(
        const u32 control_word,
        u32 * token_data_pp,
        eip202_rdr_result_control_t * const rd_control_p,
        eip202_rdr_result_token_t * const res_token_p);


/*----------------------------------------------------------------------------
 * eip202_rd_read_bypass_data
 */
void
eip202_rd_read_bypass_data(
        const u32 * bypass_data_p,
        const u8 bypass_word_count,
        eip202_rdr_bypass_data_t * const bd_p);


#endif /* EIP202_RD_FORMAT_H_ */


/* end of file eip202_rd_format.h */
