/* eip202_rdr_fsm.c
 *
 * EIP-202 Ring Control Driver Library API
 * state Machine internal Interface implementation for RDR
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

#include "eip202_rdr_fsm.h"

/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Default configuration
#include "c_eip202_ring.h"

// Driver Framework Basic Definitions API
#include "basic_defs.h"              // IDENTIFIER_NOT_USED

// EIP-202 Ring Control Driver Library Types API
#include "eip202_ring_types.h"        // EIP202_Ring_* types


/*----------------------------------------------------------------------------
 * eip202_rdr_state_set
 *
 */
eip202_ring_error_t
eip202_rdr_state_set(
        volatile eip202_rdr_state_t * const current_state,
        const eip202_rdr_state_t new_state)
{
#ifdef EIP202_RING_DEBUG_FSM
    switch(*current_state)
    {
        case EIP202_RDR_STATE_UNKNOWN:
            switch(new_state)
            {
                case EIP202_RDR_STATE_UNINITIALIZED:
                    *current_state = new_state;
                    break;
                default:
                    return EIP202_RING_ILLEGAL_IN_STATE;
            }
            break;

         case EIP202_RDR_STATE_UNINITIALIZED:
            switch(new_state)
            {
                case EIP202_RDR_STATE_INITIALIZED:
                   *current_state = new_state;
                   break;
                default:
                    return EIP202_RING_ILLEGAL_IN_STATE;
            }
            break;

        case EIP202_RDR_STATE_INITIALIZED:
            switch(new_state)
            {
                case EIP202_RDR_STATE_UNINITIALIZED:
                    *current_state = new_state;
                    break;
                case EIP202_RDR_STATE_FREE:
                    *current_state = new_state;
                    break;
                case EIP202_RDR_STATE_FULL:
                    *current_state = new_state;
                    break;
                case EIP202_RDR_STATE_INITIALIZED:
                    *current_state = new_state;
                    break;
                default:
                    return EIP202_RING_ILLEGAL_IN_STATE;
            }
            break;

        case EIP202_RDR_STATE_FREE:
            switch(new_state)
            {
                case EIP202_RDR_STATE_UNINITIALIZED:
                    *current_state = new_state;
                    break;
                case EIP202_RDR_STATE_INITIALIZED:
                    *current_state = new_state;
                    break;
                case EIP202_RDR_STATE_FULL:
                    *current_state = new_state;
                    break;
                case EIP202_RDR_STATE_FATAL_ERROR:
                    *current_state = new_state;
                    break;
                case EIP202_RDR_STATE_FREE:
                    *current_state = new_state;
                    break;
                default:
                    return EIP202_RING_ILLEGAL_IN_STATE;
            }
            break;

        case EIP202_RDR_STATE_FULL:
            switch(new_state)
            {
                case EIP202_RDR_STATE_UNINITIALIZED:
                    *current_state = new_state;
                    break;
                case EIP202_RDR_STATE_INITIALIZED:
                    *current_state = new_state;
                    break;
                case EIP202_RDR_STATE_FREE:
                    *current_state = new_state;
                    break;
                case EIP202_RDR_STATE_FATAL_ERROR:
                    *current_state = new_state;
                    break;
                case EIP202_RDR_STATE_FULL:
                    *current_state = new_state;
                    break;
                default:
                    return EIP202_RING_ILLEGAL_IN_STATE;
            }
            break;

        case EIP202_RDR_STATE_FATAL_ERROR:
            switch(new_state)
            {
                case EIP202_RDR_STATE_UNINITIALIZED:
                    *current_state = new_state;
                    break;
                default:
                    return EIP202_RING_ILLEGAL_IN_STATE;
            }
            break;

        default:
            return EIP202_RING_ILLEGAL_IN_STATE;
    }
#else
    IDENTIFIER_NOT_USED(current_state);
    IDENTIFIER_NOT_USED(new_state);
#endif // EIP202_RING_DEBUG_FSM

    return EIP202_RING_NO_ERROR;
}


/* end of file eip202_rdr_fsm.c */
