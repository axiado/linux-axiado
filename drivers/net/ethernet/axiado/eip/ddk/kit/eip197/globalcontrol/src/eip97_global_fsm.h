/* eip97_global_fsm.h
 *
 * EIP-97 Global Control Driver Library API state Machine internal Interface
 *
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

#ifndef EIP97_GLOBAL_FSM_H_
#define EIP97_GLOBAL_FSM_H_

/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

#include "eip97_global_types.h"            // eip97_global_error_t

/*----------------------------------------------------------------------------
 * Definitions and macros
 */

// EIP-97 Global Control Driver Library API States
typedef enum
{
    EIP97_GLOBAL_STATE_UNKNOWN = 1,
    EIP97_GLOBAL_STATE_SW_RESET_START,
    EIP97_GLOBAL_STATE_SW_RESET_DONE,
    EIP97_GLOBAL_STATE_HW_RESET_DONE,
    EIP97_GLOBAL_STATE_INITIALIZED,
    EIP97_GLOBAL_STATE_ENABLED,
    EIP97_GLOBAL_STATE_FATAL_ERROR
} eip97_global_state_t;


/*----------------------------------------------------------------------------
 * eip97_global_state_set
 *
 * This function check whether the transition from the "current_state" to the
 * "new_state" is allowed and if yes changes the former to the latter.
 *
  * Return value
 *     EIP97_GLOBAL_NO_ERROR : operation is completed
 *     EIP97_GLOBAL_ILLEGAL_IN_STATE : state transition is not allowed
 */
eip97_global_error_t
eip97_global_state_set(
        volatile eip97_global_state_t * const current_state,
        const eip97_global_state_t new_state);


#endif /* EIP97_GLOBAL_FSM_H_ */


/* end of file eip97_global_fsm.h */
