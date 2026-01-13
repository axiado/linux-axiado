/* eip202_ring_internal.h
 *
 * EIP-202 Ring Control Driver Library internal interface
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

#ifndef EIP202_RING_INTERNAL_H_
#define EIP202_RING_INTERNAL_H_

/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Default configuration
#include "c_eip202_ring.h"       // EIP202_RING_STRICT_ARGS

// EIP-202 Ring Driver Library Types API
#include "eip202_ring_types.h"   // EIP202_* types

// Driver Framework Basic Definitions API
#include "basic_defs.h"         // u32

// Driver Framework device API
#include "device_types.h"       // device_handle_t

// Driver Framework DMA Resource API
#include "dmares_types.h"       // types of the DMA resource API

// Ring Helper API
#include "ringhelper.h"


/*----------------------------------------------------------------------------
 * Definitions and macros
 */

// RDR ownership word pattern
#define EIP202_RDR_OWNERSHIP_WORD_PATTERN       0xAAAAAAAAU

// CDR I/O Area, used internally
typedef struct
{
    device_handle_t device;
    unsigned int state;
    ring_helper_t ring_helper;
    ring_helper_callback_interface_t ring_helper_callbacks;
    dma_resource_handle_t ring_handle;
    u32 desc_offs_word_count;
    u32 ring_size_word_count;
    u32 count_rd;
    bool f_valid_count_rd;
    bool f_atp;
} eip202_cdr_true_io_area_t;

// RDR I/O Area, used internally
typedef struct
{
    device_handle_t device;
    unsigned int state;
    ring_helper_t ring_helper;
    ring_helper_callback_interface_t ring_helper_callbacks;
    dma_resource_handle_t ring_handle;
    u32 desc_offs_word_count;
    u32 ring_size_word_count;
    u32 token_offset_word_count;
    unsigned int acknowledged_rd_count;
    unsigned int acknowledged_pkt_count;
    unsigned int rd_to_get_count;
    unsigned int pkt_to_get_count;
    bool packet_found;
} eip202_rdr_true_io_area_t;

#define cdrioarea(_p) ((volatile eip202_cdr_true_io_area_t *)_p)
#define rdrioarea(_p) ((volatile eip202_rdr_true_io_area_t *)_p)

#ifdef EIP202_RING_STRICT_ARGS
#define EIP202_RING_CHECK_POINTER(_p) \
    if (NULL == (_p)) \
        return EIP202_RING_ARGUMENT_ERROR;
#define EIP202_RING_CHECK_INT_INRANGE(_i, _min, _max) \
    if ((_i) < (_min) || (_i) > (_max)) \
        return EIP202_RING_ARGUMENT_ERROR;
#define EIP202_RING_CHECK_INT_ATLEAST(_i, _min) \
    if ((_i) < (_min)) \
        return EIP202_RING_ARGUMENT_ERROR;
#define EIP202_RING_CHECK_INT_ATMOST(_i, _max) \
    if ((_i) > (_max)) \
        return EIP202_RING_ARGUMENT_ERROR;
#else
/* EIP202_RING_STRICT_ARGS undefined */
#define EIP202_RING_CHECK_POINTER(_p)
#define EIP202_RING_CHECK_INT_INRANGE(_i, _min, _max)
#define EIP202_RING_CHECK_INT_ATLEAST(_i, _min)
#define EIP202_RING_CHECK_INT_ATMOST(_i, _max)
#endif /*end of EIP202_RING_STRICT_ARGS */

#define TEST_SIZEOF(type, size) \
    extern int size##_must_bigger[1 - 2*((int)(sizeof(type) > size))]

// validate the size of the fake and real io_area structures
TEST_SIZEOF(eip202_cdr_true_io_area_t, EIP202_IOAREA_REQUIRED_SIZE);
TEST_SIZEOF(eip202_rdr_true_io_area_t, EIP202_IOAREA_REQUIRED_SIZE);


#endif /* EIP202_RING_INTERNAL_H_ */


/* end of file eip202_ring_internal.h */
