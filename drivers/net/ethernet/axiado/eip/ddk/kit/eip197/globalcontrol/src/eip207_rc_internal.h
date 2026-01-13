/* eip207_rc_internal.h
 *
 * EIP-207s Record Cache (RC) internal interface
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

#ifndef EIP207_RC_INTERNAL_H_
#define EIP207_RC_INTERNAL_H_


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// EIP-207 Global Control Init API
#include "eip207_global_init.h"


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Driver Framework Basic Definitions API
#include "basic_defs.h"                 // bool, u32

// Driver Framework device API
#include "device_types.h"               // device_handle_t


/*----------------------------------------------------------------------------
 * Definitions and macros
 */

// Record cache combination type
typedef enum
{
    EIP207_RC_INTERNAL_NOT_COMBINED,        // Not combined
    EIP207_RC_INTERNAL_FRC_TRC_COMBINED,    // trc combined with frc
    EIP207_RC_INTERNAL_FRC_ARC4_COMBINED,   // arc4_rc combined with frc
    EIP207_RC_INTERNAL_TRC_ARC4_COMBINED    // arc4_rc combined with trc
} eip207_rc_internal_combination_type_t;


/*----------------------------------------------------------------------------
 * eip207_rc_internal_init
 *
 * Initialize the EIP-207 Record Cache.
 *
 * device (input)
 *      device handle that identifies the Record Cache hardware
 *
 * combination_type (input)
 *      Specifies the Record Cache combination type with another one,
 *      see eip207_rc_internal_combination_type_t
 *
 * cache_base (input)
 *      Base address of the Record Cache, must be one of the following:
 *      EIP207_FRC_REG_BASE    - for the Flow Record Cache,
 *      EIP207_TRC_REG_BASE    - for the Transform Record Cache,
 *      EIP207_ARC4RC_REG_BASE - for the arc4 Record Cache
 *
 * rc_params_p (input, output)
 *      Pointer to the memory location containing the Record Cache
 *      initialization parameters.
 *
 * record_word_count (input)
 *      Record Cache Record size in 32-bit words.
 *
 * Return code
 *     EIP97_GLOBAL_NO_ERROR : operation is completed
 *     EIP97_GLOBAL_ARGUMENT_ERROR : Passed wrong argument
 */
eip207_global_error_t
eip207_rc_internal_init(
        const device_handle_t device,
        const eip207_rc_internal_combination_type_t combination_type,
        const u32 cache_base,
        eip207_global_cache_params_t * rc_params_p,
        const unsigned int record_word_count);


/*----------------------------------------------------------------------------
 * eip207_rc_internal_status_get
 *
 * Get the EIP-207 Record Cache status.
 *
 * device (input)
 *      device handle that identifies the Record Cache hardware
 *
 * cache_set_id (input)
 *      Cache set identifier.
 *
 * frc_status_p (output)
 *      Pointer to the memory location to store the Flow Record Cache
 *      status information.
 *
 * trc_status_p (output)
 *      Pointer to the memory location to store the Transform Record Cache
 *      status information.
 *
 * arc4_rc_status_p (output)
 *      Pointer to the memory location to store the arc4 Record Cache
 *      status information.
 *
 * Return code
 *     None
 */
void
eip207_rc_internal_status_get(
        const device_handle_t device,
        const unsigned int cache_set_id,
        eip207_global_cache_status_t * const frc_status_p,
        eip207_global_cache_status_t * const trc_status_p,
        eip207_global_cache_status_t * const arc4_rc_status_p);


/*----------------------------------------------------------------------------
 * eip207_rc_internal_debug_statistics_get
 *
 * Get the EIP-207 Record Cache debug statistics.
 *
 * device (input)
 *      device handle that identifies the Record Cache hardware
 *
 * cache_set_id (input)
 *      Cache set identifier.
 *
 * frc_stats_p (output)
 *      Pointer to the memory location to store the Flow Record Cache
 *      status information.
 *
 * trc_stats_p (output)
 *      Pointer to the memory location to store the Transform Record Cache
 *      statistics information.
 *
 * Return code
 *     None
 */
void
eip207_rc_internal_debug_statistics_get(
        const device_handle_t device,
        const unsigned int cache_set_id,
        eip207_global_cache_debug_statistics_t * const frc_stats_p,
        eip207_global_cache_debug_statistics_t * const trc_stats_p);

/* end of file eip207_rc_internal.h */


#endif /* EIP207_RC_INTERNAL_H_ */
