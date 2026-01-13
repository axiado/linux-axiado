/* eip207_flow_dtl.h
 *
 * EIP-207 Flow Control API, flue DTL extensions:
 *      Large Transform record support,
 *      Transform records addition,
 *      Flow and Transform records removal,
 *      Direct Transform Record Lookup
 *
 * This API should be used for the In-line IPsec and Fast-Path IPsec
 * use cases only!
 *
 * This API is not re-entrant.
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

#ifndef EIP207_FLOW_DTL_H_
#define EIP207_FLOW_DTL_H_


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Driver Framework Basic Definitions API
#include "basic_defs.h"           // u8, u32, bool

// Driver Framework device API
#include "device_types.h"         // device_handle_t

// Driver Framework DMA Resource API
#include "dmares_types.h"         // dma_resource_handle_t

// EIP-207 Driver Library Flow Control Generic API
#include "eip207_flow_generic.h"  // EIP207_FLOW_*


/*----------------------------------------------------------------------------
 * Definitions and macros
 */


/*----------------------------------------------------------------------------
 * eip207_flow_dtl_fr_remove
 *
 * This function removes the flow record from
 * the HT and the classification engine so that the engine can not look it up.
 *
 * Note: only one flow record can be removed at a time for one instance
 *       of the classification engine.
 *
 * io_area_p (input)
 *     Pointer to the IO Area of the required Classification device.
 *
 * hash_table_id (input)
 *     index of the flow hash table where the flow record must be removed from
 *
 * fr_dscr_p (input)
 *     Pointer to the flow descriptor data structure that describes
 *     the flow record to be removed. The flow descriptor buffer can
 *     be discarded after the flow record is removed.
 *     The flow descriptor pointer cannot be 0.
 *
 * Return value
 *     EIP207_FLOW_NO_ERROR
 *     EIP207_FLOW_RECORD_REMOVE_BUSY
 *     EIP207_FLOW_UNSUPPORTED_FEATURE_ERROR
 *     EIP207_FLOW_ARGUMENT_ERROR
 *     EIP207_FLOW_ILLEGAL_IN_STATE
 */
eip207_flow_error_t
eip207_flow_dtl_fr_remove(
        eip207_flow_io_area_t * const io_area_p,
        const unsigned int hash_table_id,
        eip207_flow_fr_dscr_t * const fr_dscr_p);


/*----------------------------------------------------------------------------
 * eip207_flow_dtl_tr_large_word_count_get
 *
 * This function returns the required size of one Large Transform Record
 * (in 32-bit words).
 */
unsigned int
eip207_flow_dtl_tr_large_word_count_get(void);


/*----------------------------------------------------------------------------
 * eip207_flow_dtl_tr_add
 *
 * This function adds the provided transform record to the Hash Table
 * so that the classification device can look it up (Direct Transform Lookup).
 *
 * This function returns the EIP207_FLOW_UNSUPPORTED_FEATURE_ERROR error code
 * when it detects a mismatch in the Flow Control Driver Library configuration
 * and the used EIP-207 HW capabilities.
 *
 * Note: this function must be called only for those transform records
 *       which can be directly looked up by the Classification device.
 *
 * io_area_p (input)
 *     Pointer to the IO Area of the required Classification device.
 *
 * hash_table_id (input)
 *     index of the flow hash table where the flow record must be added.
 *
 * tr_dscr_p (input)
 *     Pointer to the data structure that describes the transform record.
 *     This descriptor must exist along with the record it describes until
 *     that record is removed by the eip207_flow_dtl_tr_remove() function.
 *     The transform descriptor pointer cannot be 0.
 *
 * xform_in_data_p (input)
 *     Pointer to the data structure that contains the input data that will
 *     be used for adding the transform record. The buffer holding this data
 *     structure can be freed after this function returns.
 *
 * Return value
 *     EIP207_FLOW_NO_ERROR
 *     EIP207_FLOW_UNSUPPORTED_FEATURE_ERROR
 *     EIP207_FLOW_ARGUMENT_ERROR
 *     EIP207_FLOW_ILLEGAL_IN_STATE
 */
eip207_flow_error_t
eip207_flow_dtl_tr_add(
        eip207_flow_io_area_t * const io_area_p,
        const unsigned int hash_table_id,
        eip207_flow_tr_dscr_t * const tr_dscr_p,
        const eip207_flow_tr_input_data_t * const xform_in_data_p);


/*----------------------------------------------------------------------------
 * eip207_flow_dtl_tr_large_read
 *
 * This function reads output data from the provided large transform record.
 *
 * io_area_p (input)
 *     Pointer to the IO Area of the required Classification device.
 *
 * hash_table_id (input)
 *     index of the flow hash table where the flow record must be removed from.
 *
 * tr_dscr_p (input)
 *     Pointer to the transform record descriptor. The transform record
 *     descriptor is only used during this function execution.
 *     It can be discarded after this function returns.
 *
 * xform_data_p (output)
 *     Pointer to the data structure where the read transform data will
 *     be stored.
 *
 * Return value
 *     EIP207_FLOW_NO_ERROR
 *     EIP207_FLOW_UNSUPPORTED_FEATURE_ERROR
 *     EIP207_FLOW_ARGUMENT_ERROR
 *     EIP207_FLOW_ILLEGAL_IN_STATE
 */
eip207_flow_error_t
eip207_flow_dtl_tr_large_read(
        eip207_flow_io_area_t * const io_area_p,
        const unsigned int hash_table_id,
        const eip207_flow_tr_dscr_t * const tr_dscr_p,
        eip207_flow_tr_output_data_t * const xform_data_p);


/*----------------------------------------------------------------------------
 * eip207_flow_dtl_tr_remove
 *
 * This function removes the requested transform record from the classification
 * engine.
 *
 * Note: this function must be called only for those transform records
 *       which can be directly looked up by the Classification device.
 *
 * io_area_p (input)
 *     Pointer to the IO Area of the required Classification device.
 *
 * hash_table_id (input)
 *     index of the flow hash table where the flow record must be removed from.
 *
 * tr_dscr_p (input)
 *     Pointer to the transform record descriptor.
 *
 * Return value
 *     EIP207_FLOW_NO_ERROR
 *     EIP207_FLOW_UNSUPPORTED_FEATURE_ERROR
 *     EIP207_FLOW_ARGUMENT_ERROR
 *     EIP207_FLOW_ILLEGAL_IN_STATE
 */
eip207_flow_error_t
eip207_flow_dtl_tr_remove(
        eip207_flow_io_area_t * const io_area_p,
        const unsigned int hash_table_id,
        eip207_flow_tr_dscr_t * const tr_dscr_p);


#endif /* EIP207_FLOW_DTL_H_ */


/* end of file eip207_flow_dtl.h */
