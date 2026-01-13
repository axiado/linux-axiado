/* eip207_flow_internal.c
 *
 *  EIP-207 Flow Control internal interface implementation
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

// EIP-207 Flow Control Driver Library internal interfaces
#include "eip207_flow_internal.h"
#include "eip207_flow_generic.h"


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

#include "eip207_flow_level0.h"         // EIP-207 Level 0 macros

// Driver Framework Basic Definitions API
#include "basic_defs.h"         // BIT definitions, bool, u32

// Driver Framework DMA Resource API
#include "dmares_types.h"       // dma_resource_handle_t
#include "dmares_rw.h"          // dma_resource_write32()/_Read32()/_PreDMA()


/*----------------------------------------------------------------------------
 * Definitions and macros
 */


/*----------------------------------------------------------------------------
 * Local variables
 */


/*----------------------------------------------------------------------------
 * eip207_flow_internal_read64
 */
eip207_flow_error_t
eip207_flow_internal_read64(
        const dma_resource_handle_t handle,
        const unsigned int value64_word_offset_lo,
        const unsigned int value64_word_offset_hi,
        u32 * const value64_lo,
        u32 * const value64_hi)
{
    u32 value32;
    unsigned int i;

    for (i = 0; i < EIP207_FLOW_VALUE_64BIT_MAX_NOF_READ_ATTEMPTS; i++)
    {
        value32     = dma_resource_read32(handle, value64_word_offset_hi);
        *value64_lo = dma_resource_read32(handle, value64_word_offset_lo);

        // Prepare the flow record for reading
        dma_resource_post_dma(handle, 0, 0);

        *value64_hi = dma_resource_read32(handle, value64_word_offset_hi);

        if (value32 == (*value64_hi))
            return EIP207_FLOW_NO_ERROR;
    }

    return EIP207_FLOW_INTERNAL_ERROR;
}


/* end of file eip207_flow_hte_dscr_dtl.c */
