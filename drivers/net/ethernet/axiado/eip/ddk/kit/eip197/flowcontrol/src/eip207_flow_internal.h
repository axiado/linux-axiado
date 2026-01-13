/* eip207_flow_internal.h
 *
 * EIP-207 Flow Control internal interface
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

#ifndef EIP207_FLOW_INTERNAL_H_
#define EIP207_FLOW_INTERNAL_H_


/*----------------------------------------------------------------------------
 * This module implements (provides) the following interface(s):
 */

// EIP-207 Flow Control Driver Library Generic interface
#include "eip207_flow_generic.h"


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Default configuration
#include "c_eip207_flow.h"

// Driver Framework Basic Definitions API
#include "basic_defs.h"         // BIT definitions, bool, u32

// Driver Framework device API
#include "device_types.h"       // device_handle_t
#include "device_rw.h"          // Read32, Write32

// Driver Framework DMA Resource API
#include "dmares_types.h"       // dma_resource_handle_t


/*----------------------------------------------------------------------------
 * Definitions and macros
 */

#define EIP207_FLOW_DSCR_DUMMY_ADDRESS_BYTE             0

#define EIP207_FLOW_VALUE_64BIT_MAX_NOF_READ_ATTEMPTS   2

#define ioarea(_p) ((volatile eip207_flow_true_io_area_t *)_p)

#ifdef EIP207_FLOW_STRICT_ARGS
#define EIP207_FLOW_CHECK_POINTER(_p) \
    if (NULL == (_p)) \
        return EIP207_FLOW_ARGUMENT_ERROR;
#define EIP207_FLOW_CHECK_INT_INRANGE(_i, _min, _max) \
    if ((_i) < (_min) || (_i) > (_max)) \
        return EIP207_FLOW_ARGUMENT_ERROR;
#define EIP207_FLOW_CHECK_INT_ATLEAST(_i, _min) \
    if ((_i) < (_min)) \
        return EIP207_FLOW_ARGUMENT_ERROR;
#define EIP207_FLOW_CHECK_INT_ATMOST(_i, _max) \
    if ((_i) > (_max)) \
        return EIP207_FLOW_ARGUMENT_ERROR;
#else
/* EIP207_FLOW_STRICT_ARGS undefined */
#define EIP207_FLOW_CHECK_POINTER(_p)
#define EIP207_FLOW_CHECK_INT_INRANGE(_i, _min, _max)
#define EIP207_FLOW_CHECK_INT_ATLEAST(_i, _min)
#define EIP207_FLOW_CHECK_INT_ATMOST(_i, _max)
#endif /*end of EIP207_FLOW_STRICT_ARGS */

#ifdef EIP207_FLOW_DEBUG_FSM
// EIP-207 Flow Control API States
typedef enum
{
    EIP207_FLOW_STATE_INITIALIZED  = 1,
    EIP207_FLOW_STATE_ENABLED,
    EIP207_FLOW_STATE_INSTALLED,
    EIP207_FLOW_STATE_FATAL_ERROR
} eip207_flow_state_t;
#endif // EIP207_FLOW_DEBUG_FSM

// Physical (bus) address that can be used for DMA by the device
typedef struct
{
    // 32-bit physical bus address
    u32 addr;

    // upper 32-bit part of a 64-bit physical address
    // Note: this value has to be provided only for 64-bit addresses,
    // in this case addr field provides the lower 32-bit part
    // of the 64-bit address, for 32-bit addresses this field is ignored,
    // and should be set to 0.
    u32 upper_addr;

} eip207_flow_internal_address_t;

// Hash Table parameters
typedef struct
{
    // DMA Resource handle for the Hash Table (HT)
    dma_resource_handle_t ht_dma_handle;

    // Pointer to the Flow Descriptor Table (DT)
    void * dt_p;

    // DMA (physical) base address for records for lookup
    eip207_flow_internal_address_t base_addr;

    // HT table size (in eip207_flow_hash_table_entry_count_t units)
    unsigned int ht_table_size;

    // Pointer to list head of free HTE descriptors for overflow hash buckets
    void * free_list_head_p;

    // True when FLUEC is enabled
    bool f_lookup_cached;

} eip207_flow_ht_params_t;

// I/O Area, used internally
typedef struct
{
    // Classification engine state
    u32 state;

    // device handle representing the Classification engine
    device_handle_t device;

#ifdef EIP207_FLOW_DEBUG_FSM
    unsigned int rec_installed_counter;
#endif

    // Flow Hash Table parameters
    eip207_flow_ht_params_t  ht_params [EIP207_FLOW_MAX_NOF_FLOW_HASH_TABLES_TO_USE];

} eip207_flow_true_io_area_t;


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
        u32 * const value64_hi);


#ifdef EIP207_FLOW_DEBUG_FSM
/*----------------------------------------------------------------------------
 * EIP207Lib_Flow_State_Set
 *
 */
eip207_flow_error_t
eip207_flow_state_set(
        eip207_flow_state_t * const current_state,
        const eip207_flow_state_t new_state);
#endif // EIP207_FLOW_DEBUG_FSM


#endif /* EIP207_FLOW_INTERNAL_H_ */


/* end of file eip207_flow_internal.h */
