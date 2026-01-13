/* eip207_flow_hte_dscr_dtl.h
 *
 * Helper functions for operations with the HTE Descriptor DTL,
 * interface
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

#ifndef EIP207_FLOW_HTE_DSCR_DTL_H_
#define EIP207_FLOW_HTE_DSCR_DTL_H_

/*----------------------------------------------------------------------------
 * This module implements (provides) the following interface(s):
 */


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */


// Driver Framework Basic Definitions API
#include "basic_defs.h"                 // u32


/*----------------------------------------------------------------------------
 * Definitions and macros
 */

// Record offset mask values
#define EIP207_FLOW_HTE_REC_OFFSET_1        0x01
#define EIP207_FLOW_HTE_REC_OFFSET_2        0x02
#define EIP207_FLOW_HTE_REC_OFFSET_3        0x04
#define EIP207_FLOW_HTE_REC_OFFSET_ALL      MASK_3_BITS

// type of records for lookup by flue
typedef enum
{
    EIP207_FLOW_REC_INVALID = 0,
    EIP207_FLOW_REC_FLOW,
    EIP207_FLOW_REC_TRANSFORM_SMALL,
    EIP207_FLOW_REC_TRANSFORM_LARGE
} eip207_flow_record_type_t;

// Record states
typedef enum
{
    EIP207_FLOW_REC_STATE_INSTALLED = 1,
    EIP207_FLOW_REC_STATE_REMOVED
} eip207_flow_rec_state_t;

// HTE descriptor list
typedef struct
{
    // Pointer to the next descriptor in the chain
    void * next_dscr_p;

    // Pointer to the previous descriptor in the chain
    void * prev_dscr_p;

} eip207_flow_hte_dscr_list_t;

// Hash Table Entry (HTE) descriptor
typedef struct
{
    // Hash Bucket byte offset from the Hash Table base address
    u32 bucket_byte_offset;

    // Record reference count,
    // 0 when Hash Bucket refers to no records for lookup
    unsigned int record_count;

    // True when descriptor is for an overflow Hash Bucket
    bool f_overflow_bucket;

    // mask for used record offsets fields in the Hash Bucket,
    // see EIP207_FLOW_HTE_REC_OFFSET_*
    u32 used_rec_offs_mask;

    // list of HTE descriptors, either the free list of unused descriptors or
    // the hash bucket overflow chain
    eip207_flow_hte_dscr_list_t list;

} eip207_flow_hte_dscr_t;

// Record data
typedef struct
{
    // Record type
    eip207_flow_record_type_t type;

    // Host address (pointer) for the HTE descriptor associated with the record
    eip207_flow_hte_dscr_t * hte_dscr_p;

    // slot number in the Hash Bucket descriptor used by this record
    unsigned int slot;

} eip207_flow_hte_dscr_rec_data_t;


/*----------------------------------------------------------------------------
 * eip207_flow_hte_dscr_list_prev_get
 *
 * Gets the previous HTE descriptor for the provided one
 * in the descriptor list
 */
static inline eip207_flow_hte_dscr_t *
eip207_flow_hte_dscr_list_prev_get(
        eip207_flow_hte_dscr_t * const hte_dscr_p)
{
    if (hte_dscr_p != NULL)
    {
        eip207_flow_hte_dscr_list_t * list_p;

        list_p = &hte_dscr_p->list;

        return list_p->prev_dscr_p;
    }
    else
        return NULL;
}


/*----------------------------------------------------------------------------
 * eip207_flow_hte_dscr_list_prev_set
 *
 * Sets the previous HTE descriptor for the provided one
 * in the descriptor list
 */
static inline void
eip207_flow_hte_dscr_list_prev_set(
        eip207_flow_hte_dscr_t * const hte_dscr_p,
        eip207_flow_hte_dscr_t * const prev_hte_dscr_p)
{
    if (hte_dscr_p != NULL)
    {
        eip207_flow_hte_dscr_list_t * list_p;

        list_p = &hte_dscr_p->list;

        list_p->prev_dscr_p = prev_hte_dscr_p;
    }
}


/*----------------------------------------------------------------------------
 * eip207_flow_hte_dscr_list_next_get
 *
 * Gets the next HTE descriptor for the provided one
 * in the descriptor list
 */
static inline eip207_flow_hte_dscr_t *
eip207_flow_hte_dscr_list_next_get(
        eip207_flow_hte_dscr_t * const hte_dscr_p)
{
    if (hte_dscr_p != NULL)
    {
        eip207_flow_hte_dscr_list_t * list_p;

        list_p = &hte_dscr_p->list;

        return list_p->next_dscr_p;
    }
    else
        return NULL;
}


/*----------------------------------------------------------------------------
 * eip207_flow_hte_dscr_list_next_set
 *
 * Sets the next HTE descriptor for the provided one
 * in the descriptor list
 */
static inline void
eip207_flow_hte_dscr_list_next_set(
        eip207_flow_hte_dscr_t * const hte_dscr_p,
        eip207_flow_hte_dscr_t * const next_hte_dscr_p)
{
    if (hte_dscr_p)
    {
        eip207_flow_hte_dscr_list_t * list_p;

        list_p = &hte_dscr_p->list;

        list_p->next_dscr_p = next_hte_dscr_p;
    }
}


/*----------------------------------------------------------------------------
 * eip207_flow_hte_dscr_list_remove
 *
 * Remove HTE descriptor from the list it's on
 */
static inline void
eip207_flow_hte_dscr_list_remove(
        eip207_flow_hte_dscr_t * const hte_dscr_p)
{
    eip207_flow_hte_dscr_t * hte_dscr_prev_p;
    eip207_flow_hte_dscr_t * hte_dscr_next_p;

    // Update the list
    hte_dscr_prev_p =
               eip207_flow_hte_dscr_list_prev_get(hte_dscr_p);

    hte_dscr_next_p =
               eip207_flow_hte_dscr_list_next_get(hte_dscr_p);

    eip207_flow_hte_dscr_list_next_set(hte_dscr_prev_p,
                                       hte_dscr_next_p);

    eip207_flow_hte_dscr_list_prev_set(hte_dscr_next_p,
                                       hte_dscr_prev_p);

    // Update the HTE descriptor
    eip207_flow_hte_dscr_list_next_set(hte_dscr_p, NULL);
    eip207_flow_hte_dscr_list_prev_set(hte_dscr_p, NULL);
}


/*----------------------------------------------------------------------------
 * eip207_flow_hte_dscr_list_insert
 *
 * Insert HTE descriptor to the list using hte_dscr_prev_p as the previous
 * descriptor
 */
static inline void
eip207_flow_hte_dscr_list_insert(
        eip207_flow_hte_dscr_t * const hte_dscr_prev_p,
        eip207_flow_hte_dscr_t * const hte_dscr_p)
{
    eip207_flow_hte_dscr_t * hte_dscr_next_p;

    // Update the list, get the next HTE descriptor for hte_dscr_prev_p
    hte_dscr_next_p =
               eip207_flow_hte_dscr_list_next_get(hte_dscr_prev_p);

    // Update the hte_dscr_prev_p descriptor
    eip207_flow_hte_dscr_list_next_set(hte_dscr_prev_p,
                                       hte_dscr_p);

    // Update the hte_dscr_next_p descriptor
    eip207_flow_hte_dscr_list_prev_set(hte_dscr_next_p,
                                       hte_dscr_p);

    // Update the HTE descriptor
    eip207_flow_hte_dscr_list_next_set(hte_dscr_p, hte_dscr_next_p);
    eip207_flow_hte_dscr_list_prev_set(hte_dscr_p, hte_dscr_prev_p);
}


#endif /* EIP207_FLOW_HTE_DSCR_DTL_H_ */


/* end of file eip207_flow_hte_dscr_dtl.h */
