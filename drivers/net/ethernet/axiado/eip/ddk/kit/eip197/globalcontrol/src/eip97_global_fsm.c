/* eip97_global_fsm.c
 *
 * EIP-97 Global Control Driver Library API state Machine internal Interface
 * implementation
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

#include "eip97_global_fsm.h"

/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// EIP-97 Driver Library Types API
#include "eip97_global_types.h"        // EIP97_Global_* types


/*----------------------------------------------------------------------------
 * eip97_global_state_set
 *
 */
eip97_global_error_t
eip97_global_state_set(
        volatile eip97_global_state_t * const current_state,
        const eip97_global_state_t new_state)
{
    switch(*current_state)
    {
        case EIP97_GLOBAL_STATE_UNKNOWN:
            switch(new_state)
            {
                case EIP97_GLOBAL_STATE_SW_RESET_START:
                    *current_state = new_state;
                    break;
                case EIP97_GLOBAL_STATE_SW_RESET_DONE:
                    *current_state = new_state;
                    break;
                default:
                    return EIP97_GLOBAL_ILLEGAL_IN_STATE;
            }
            break;

         case EIP97_GLOBAL_STATE_HW_RESET_DONE:
            switch(new_state)
            {
                case EIP97_GLOBAL_STATE_INITIALIZED:
                   *current_state = new_state;
                   break;
                default:
                    return EIP97_GLOBAL_ILLEGAL_IN_STATE;
            }
            break;

        case EIP97_GLOBAL_STATE_SW_RESET_START:
            switch(new_state)
            {
                case EIP97_GLOBAL_STATE_SW_RESET_DONE:
                    *current_state = new_state;
                    break;
                case EIP97_GLOBAL_STATE_SW_RESET_START:
                    *current_state = new_state;
                    break;
                default:
                    return EIP97_GLOBAL_ILLEGAL_IN_STATE;
            }
            break;

        case EIP97_GLOBAL_STATE_SW_RESET_DONE:
            switch(new_state)
            {
                case EIP97_GLOBAL_STATE_SW_RESET_DONE:
                case EIP97_GLOBAL_STATE_INITIALIZED:
                    *current_state = new_state;
                    break;
                default:
                    return EIP97_GLOBAL_ILLEGAL_IN_STATE;
            }
            break;

        case EIP97_GLOBAL_STATE_INITIALIZED:
            switch(new_state)
            {
                case EIP97_GLOBAL_STATE_ENABLED:
                    *current_state = new_state;
                    break;
                case EIP97_GLOBAL_STATE_SW_RESET_DONE:
                    *current_state = new_state;
                    break;
                case EIP97_GLOBAL_STATE_INITIALIZED:
                    *current_state = new_state;
                    break;
                default:
                    return EIP97_GLOBAL_ILLEGAL_IN_STATE;
            }
            break;

        case EIP97_GLOBAL_STATE_ENABLED:
            switch(new_state)
            {
                case EIP97_GLOBAL_STATE_SW_RESET_START:
                    *current_state = new_state;
                    break;
                case EIP97_GLOBAL_STATE_SW_RESET_DONE:
                    *current_state = new_state;
                    break;
                case EIP97_GLOBAL_STATE_FATAL_ERROR:
                    break;
                case EIP97_GLOBAL_STATE_ENABLED:
                    break;
                default:
                    return EIP97_GLOBAL_ILLEGAL_IN_STATE;
            }
            break;

        case EIP97_GLOBAL_STATE_FATAL_ERROR:
            switch(new_state)
            {
                case EIP97_GLOBAL_STATE_SW_RESET_START:
                    *current_state = new_state;
                    break;
                case EIP97_GLOBAL_STATE_SW_RESET_DONE:
                    *current_state = new_state;
                    break;
                default:
                    return EIP97_GLOBAL_ILLEGAL_IN_STATE;
            }
            break;

        default:
            return EIP97_GLOBAL_ILLEGAL_IN_STATE;
    }

    return EIP97_GLOBAL_NO_ERROR;
}


/* end of file eip97_global_fsm.c */
