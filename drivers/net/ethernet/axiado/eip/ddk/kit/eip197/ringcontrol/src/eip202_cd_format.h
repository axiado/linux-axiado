/* eip202_cd_format.h
 *
 * EIP-202 Ring Control Driver Library command Descriptor internal interface
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

#ifndef EIP202_CD_FORMAT_H_
#define EIP202_CD_FORMAT_H_


/*----------------------------------------------------------------------------
 * This module implements (provides) the following interface(s):
 */

// Descriptor I/O Driver Library API implementation
#include "eip202_cdr.h"                // eip202_arm_command_descriptor_t


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
 * eip202_cd_make_control_word
 *
 * This helper function returns the Control Word that can be written to
 * the EIP-202 command Descriptor.
 *
 * This function is re-entrant.
 *
 */
u32
eip202_cd_make_control_word(
        const u8 token_word_count,
        const u32 segment_byte_count,
        const bool f_first_segment,
        const bool f_last_segment,
        const bool f_force_engine,
        const u8 engine_id);


/*----------------------------------------------------------------------------
 * eip202_cd_write
 *
 * This helper function writes the EIP-202 Logical command Descriptor to the CDR
 *
 * This function is not re-entrant.
 *
 */
void
eip202_cd_write(
        dma_resource_handle_t handle,
        const unsigned int word_offset,
        const eip202_arm_command_descriptor_t * const descr_p,
        const bool f_atp);


#endif /* EIP202_CD_FORMAT_H_ */


/* end of file eip202_cd_format.h */
