/* eip97_global_internal.h
 *
 * EIP-97 Global Control Driver Library internal interface
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

#ifndef EIP97_GLOBAL_INTERNAL_H_
#define EIP97_GLOBAL_INTERNAL_H_

/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Default configuration
#include "c_eip97_global.h"

// Driver Framework Basic Definitions API
#include "basic_defs.h"         // u8, u32

// Driver Framework device API
#include "device_types.h"       // device_handle_t

// EIP-97 Driver Library Types API
#include "eip97_global_types.h" // EIP97_* types

/*----------------------------------------------------------------------------
 * Definitions and macros
 */

#define PE_DEFAULT_NR     0  // Default Processing Engine number

#if (PE_DEFAULT_NR >= EIP97_GLOBAL_MAX_NOF_PE_TO_USE)
#error "error: PE_DEFAULT_NR must be less than EIP97_GLOBAL_MAX_NOF_PE_TO_USE"
#endif

// I/O Area, used internally
typedef struct
{
    device_handle_t device;
    u32 state;
} eip97_true_io_area_t;

#define ioarea(_p) ((volatile eip97_true_io_area_t *)_p)

#ifdef EIP97_GLOBAL_STRICT_ARGS
#define EIP97_GLOBAL_CHECK_POINTER(_p) \
    if (NULL == (_p)) \
        return EIP97_GLOBAL_ARGUMENT_ERROR;
#define EIP97_GLOBAL_CHECK_INT_INRANGE(_i, _min, _max) \
    if ((_i) < (_min) || (_i) > (_max)) \
        return EIP97_GLOBAL_ARGUMENT_ERROR;
#define EIP97_GLOBAL_CHECK_INT_ATLEAST(_i, _min) \
    if ((_i) < (_min)) \
        return EIP97_GLOBAL_ARGUMENT_ERROR;
#define EIP97_GLOBAL_CHECK_INT_ATMOST(_i, _max) \
    if ((_i) > (_max)) \
        return EIP97_GLOBAL_ARGUMENT_ERROR;
#else
/* EIP97_GLOBAL_STRICT_ARGS undefined */
#define EIP97_GLOBAL_CHECK_POINTER(_p)
#define EIP97_GLOBAL_CHECK_INT_INRANGE(_i, _min, _max)
#define EIP97_GLOBAL_CHECK_INT_ATLEAST(_i, _min)
#define EIP97_GLOBAL_CHECK_INT_ATMOST(_i, _max)
#endif /*end of EIP97_GLOBAL_STRICT_ARGS */

#define TEST_SIZEOF(type, size) \
    extern int size##_must_bigger[1 - 2*((int)(sizeof(type) > size))]

// validate the size of the fake and real io_area structures
TEST_SIZEOF(eip97_true_io_area_t, EIP97_GLOBAL_IOAREA_REQUIRED_SIZE);


/*----------------------------------------------------------------------------
 * eip97_interfaces_get
 */
void
eip97_interfaces_get(
    unsigned int * const nof_p_es_p,
    unsigned int * const nof_rings_p,
    unsigned int * const nof_la_p,
    unsigned int * const nof_in_p);

/*----------------------------------------------------------------------------
 * eip97_dfedse_offset_get
 */
unsigned int
eip97_dfedse_offset_get(void);

#endif /* EIP97_GLOBAL_INTERNAL_H_ */


/* end of file eip97_global_internal.h */
