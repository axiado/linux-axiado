/* eip97_global_prng.c
 *
 * EIP-97 GLobal Control Driver Library
 * PRNG Module
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

#include "eip97_global_prng.h"


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Driver Framework device API
#include "device_types.h"              // device_handle_t

// EIP-97 Global Control Driver Library internal interfaces
#include "eip97_global_internal.h"
#include "eip96_level0.h"              // EIP-96 Level 0 macros
#include "eip97_global_fsm.h"          // state machine


/*----------------------------------------------------------------------------
 * Definitions and macros
 */


/*----------------------------------------------------------------------------
 * Local variables
 */


/*----------------------------------------------------------------------------
 * eip97_global_prng_reseed
 */
eip97_global_error_t
eip97_global_prng_reseed(
        eip97_global_io_area_t * const io_area_p,
        const unsigned int pe_number,
        const eip96_prng_reseed_t * const reseed_data_p)
{
    device_handle_t device;
    volatile eip97_true_io_area_t * const true_io_area_p = ioarea(io_area_p);

    EIP97_GLOBAL_CHECK_POINTER(io_area_p);

    if(pe_number >= EIP97_GLOBAL_MAX_NOF_PE_TO_USE)
        return EIP97_GLOBAL_ARGUMENT_ERROR;

    device = true_io_area_p->device;

#ifdef EIP97_GLOBAL_DEBUG_FSM
    {
        eip97_global_error_t rv;

        // Remain in the current state
        rv = eip97_global_state_set((volatile eip97_global_state_t*)&true_io_area_p->state,
                                    (eip97_global_state_t)true_io_area_p->state);
        if(rv != EIP97_GLOBAL_NO_ERROR)
            return EIP97_GLOBAL_ILLEGAL_IN_STATE;
    }
#endif // EIP97_GLOBAL_DEBUG_FSM

    EIP96_PRNG_CTRL_WR(device,
                       pe_number, // EIP-96 PE number
                       false,     // Disable PRNG
                       false);    // Set PRNG Manual mode

    // Write new seed data
    EIP96_PRNG_SEED_L_WR(device, pe_number, reseed_data_p->seed_lo);
    EIP96_PRNG_SEED_H_WR(device, pe_number, reseed_data_p->seed_hi);

    // Write new key data
    EIP96_PRNG_KEY_0_L_WR(device, pe_number, reseed_data_p->key0_lo);
    EIP96_PRNG_KEY_0_H_WR(device, pe_number, reseed_data_p->key0_hi);
    EIP96_PRNG_KEY_1_L_WR(device, pe_number, reseed_data_p->key1_lo);
    EIP96_PRNG_KEY_1_H_WR(device, pe_number, reseed_data_p->key1_hi);

    // Write new LFSR data
    EIP96_PRNG_LFSR_L_WR(device, pe_number, reseed_data_p->lfsr_lo);
    EIP96_PRNG_LFSR_H_WR(device, pe_number, reseed_data_p->lfsr_hi);

    EIP96_PRNG_CTRL_WR(device,
                       pe_number, // EIP-96 PE number
                       true,      // Enable PRNG
                       true);     // Set PRNG Auto mode

    return EIP97_GLOBAL_NO_ERROR;
}


/* end of file eip97_global_prng.c */
