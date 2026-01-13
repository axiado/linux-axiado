/* eip97_global_event.h
 *
 * EIP-97 Global Control Driver Library API:
 * Event Management use case
 *
 * Refer to the EIP-97 Driver Library User Guide for information about
 * re-entrance and usage from concurrent execution contexts of this API
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

#ifndef EIP97_GLOBAL_EVENT_H_
#define EIP97_GLOBAL_EVENT_H_


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Driver Framework Basic Definitions API
#include "basic_defs.h"         // u8, u16, bool

// EIP-97 Global Control Driver Library Types API
#include "eip97_global_types.h" // EIP97_* types


/*----------------------------------------------------------------------------
 * Definitions and macros
 */

// EIP-97 Debug statistics counters.
typedef struct
{
    // Input packets for each interface.
    u64 ifc_packets_in[16];
    // Output packets for each interface.
    u64 ifc_packets_out[16];
    // Total processed packets for each processing pipe.
    u64 pipe_total_packets[16];
    // Total amount of data processed in each pipe.
    u64 pipe_data_count[16];
    // Current number of packets in each pipe.
    u8 pipe_current_packets[16];
    // Maximum number of packets in each pipe.
    u8 pipe_max_packets[16];
} eip97_global_debug_statistics_t;


// EIP-97 data Fetch Engine (DFE) thread status,
// 1 DFE thread corresponds to 1 Processing Engine (PE)
typedef struct
{
    // number of 32-bit words currently available from the assigned CD fifo for
    // this thread
    u16 cd_fifo_word32_count;


    // size of the current DMA operation for this thread, only valid
    // if f_at_dma_busy or f_data_dma_busy are true
    u16 dma_size;

    // When true the thread is currently busy DMA-ing Additional Token data
    // to the Processing Engine
    bool f_at_dma_busy;

    // When true the thread is currently busy DMA-ing packet data to
    // the Processing Engine
    bool f_data_dma_busy;

    // FATAL ERROR when true requiring Global SW or HW Reset.
    // True when a DMA error was detected, the thread is stopped as
    // a result of this. A DFE thread error interrupt for thread n occurs
    // simultaneously with the assertion of this bit.
    // Further information may be extracted from the other thread status
    // registers.
    // Note: A DMA error is a Host bus Master interface read/write error and is
    // qualified as a fatal error. The result is unknown and in the worst case
    // it can cause a system hang-up. The only correct way to recover is
    // to issue a complete system reset (HW Reset or Global SW Reset).
    bool f_dma_error;
} eip97_global_dfe_status_t;

// EIP-97 data Store Engine (DSE) thread status,
// 1 DSE thread corresponds to 1 Processing Engine (PE)
typedef struct
{
    // number of 32-bit words currently available from the assigned RD fifo for
    // this thread
    u16 rd_fifo_word32_count;

    // size of the current DMA operation for this thread, only valid
    // if f_data_dma_busy is true
    u16 dma_size;

    // When true the thread is currently busy flushing any remaining packet data
    // from the Processing Engine either because it did not fit into
    // the reserved packet buffer or because a destination not allowed
    // interrupt (DSE thread n Irq) was fired
    bool f_data_flush_busy;

    // When true the thread is currently busy DMA-ing packet data from the
    // Processing Engine to Host memory.
    bool f_data_dma_busy;

    // FATAL ERROR when true requiring Global SW or HW Reset.
    // True when a DMA error was detected, the thread is stopped as
    // a result of this. A DSE thread error interrupt for thread n occurs
    // simultaneously with the assertion of this bit.
    // Further information may be extracted from the other thread status
    // registers.
    // Note: A DMA error is a Host bus Master interface read/write error and is
    // qualified as a fatal error. The result is unknown and in the worst case
    // it can cause a system hang-up. The only correct way to recover is
    // to issue a complete system reset (HW Reset or Global SW Reset).
    bool f_dma_error;
} eip97_global_dse_status_t;

// EIP-96 Token status
typedef struct
{
    // number of tokens located in the EIP-96, result token not included
    // (maximum is two)
    u8 active_token_count;

    // If true then a new token can be read by the EIP-96
    bool f_token_location_available;

    // If true then a (partial) result token is available in the EIP-96
    bool f_result_token_available;

    // If true then a token is currently read by the EIP-96
    bool f_token_read_active;

    // If true then the context cache contains a new context
    bool f_context_cache_active;

    // If true then the context cache is currently filled
    bool f_context_fetch;

    // If true then the context cache contains result context data that
    // needs to be updated
    bool f_result_context;

    // If true then no (part of) tokens are in the EIP-96 and no context
    // update is required
    bool f_processing_held;

    // If true then packet engine is busy (a context is active)
    bool f_busy;
} eip96_token_status_t;

// EIP-96 Context status
typedef struct
{
    /*
     * Packet processing error bit mask:
        error_0  Packet length error
        error_1  Token error, unknown token command/instruction.
        error_2  Token contains to much bypass data.
        error_3  Cryptographic block size error.
        error_4  Hash block size error (basic hash only).
        error_5  Invalid command/algorithm/mode/combination.
        error_6  Prohibited algorithm.
        error_7  Hash input overflow (basic hash only).
        error_8  ttl / HOP-limit underflow.
        error_9  Authentication failed.
        error_10 Sequence number check failed / roll-over detected.
        error_11 spi check failed.
        error_12 Checksum incorrect.
        error_13 Pad verification failed.
        error_14 internal Packet Engine time-out:
                 FATAL ERROR when set requiring Global SW or HW Reset.
        error_15 reserved error bit, will never be set to 1b.
     */
    u16 error;

    // number of available tokens is the sum of new, active and result tokens
    // that are available
    u8 available_token_count;

    // True indicates that a context is active
    bool f_active_context;

    // True indicates that a new context is (currently) loaded
    bool f_next_context;

    // True indicates that a result context data needs to be stored.
    // Result context and next context cannot be both active.
    bool f_result_context;

    // True indicates that an existing error condition has not yet been properly
    // handled to completion. Note that the next packet context and data fetch
    // can be started. In addition, error bits may still be active due
    // to the previous packet.
    bool f_error_recovery;
} eip96_context_status_t;

// EIP-96 Interrupt status
typedef struct
{
    // FATAL ERROR when true requiring Global SW or HW Reset.
    // True when the input fetch engine does not properly receive all packet
    // data.
    bool f_input_dma_error;

    // True when the output store engine does not properly store all packet data
    bool f_output_dma_error;

    // A logic OR of the error_0 up to and including error_7 of the error field
    // in the EIP-96 Context status
    bool f_packet_processing_error;

    // FATAL ERROR when true requiring Global SW or HW Reset.
    // True when the internal Packet Engine time-out, copy of error_14 from
    // the error field in the EIP-96 Context status
    // Fatal error that requires the engine reset (via HW Reset or
    // Global SW Reset)
    bool f_packet_timeout;

    // FATAL ERROR when true requiring Global SW or HW Reset.
    // True when Fatal internal error within EIP-96 Packet Engine is detected,
    // reset of engine required via HW Reset or Global SW Reset).
    bool f_fatal_error;

    // If true then there is at least one pending EIP-96 interrupt
    bool f_pe_interrupt_out;

    // If true then the input_dma_error interrupt is enabled
    bool f_input_dma_error_enabled;

    // If true then the output_dma_error interrupt is enabled
    bool f_output_dma_error_enabled;

    // If true then the packet_processin interrupt is enabled
    bool f_packet_processing_enabled;

    // If true then the packet_timeout interrupt is enabled
    bool f_packet_timeout_enabled;

    // If true then the packet_timeout interrupt is enabled
    bool f_fatal_error_enabled;

    // If true then the EIP-96 interrupt output is enabled.
    // If false then the EIP-96 interrupts will never become active.
    bool f_pe_interrupt_out_enabled;
} eip96_interrupt_status_t;

// EIP-96 Output Transfer status
typedef struct
{
    // number of (32-bit) words that are available in the 2K Bytes data
    // output buffer, shows value 255 when more than 255 words are available.
    u8 available_word32_count;

    // Minimum number of words that is transferred per beat (fixed)
    u8 min_transfer_word_count;

    // Maximum number of words that can be transferred per beat (fixed)
    u8 max_transfer_word_count;

    // Masks the number of available word entries to obtain an optimal
    // transfer size (fixed)
    u8 transfer_size_mask;
} eip96_output_transfer_status_t;

// EIP-96 PRNG status
typedef struct
{
    // True when the PRNG is busy generating a Pseudo-Random number
    bool f_busy;

    // True when a valid Pseudo-Random number is available
    bool f_result_ready;
} eip96_prng_status_t;


/*----------------------------------------------------------------------------
 * eip97_global_debug_statistics_get
 *
 * This function returns debug statistics information in
 * the eip97_global_debug_statistics_t data structure.
 *
 * io_area_p (input)
 *     Pointer to the place holder in memory for the IO Area.
 *
 * debug_statistics_p (output)
 *     Pointer to the data structure where the debug statistics
 *     will be stored.
 *
 * This function is re-entrant.
 *
 * Return value
 *     EIP97_GLOBAL_NO_ERROR : operation is completed
 *     EIP97_GLOBAL_ARGUMENT_ERROR : Passed wrong argument
 *     EIP97_GLOBAL_ILLEGAL_IN_STATE : invalid API state transition
 */
eip97_global_error_t
eip97_global_debug_statistics_get(
        eip97_global_io_area_t * const io_area_p,
        eip97_global_debug_statistics_t * const debug_statistics_p);


/*----------------------------------------------------------------------------
 * eip97_global_dfe_status_get
 *
 * This function returns hardware status information in
 * the eip97_global_dfe_status_t data structure.
 *
 * io_area_p (input)
 *     Pointer to the place holder in memory for the IO Area.
 *
 * pe_number (input)
 *     number of the PE for which the DFE status must be obtained.
 *
 * dfe_status_p (output)
 *     Pointer to the data structure where the DFE status
 *     will be stored.
 *
 * This function is re-entrant.
 *
 * Return value
 *     EIP97_GLOBAL_NO_ERROR : operation is completed
 *     EIP97_GLOBAL_ARGUMENT_ERROR : Passed wrong argument
 *     EIP97_GLOBAL_ILLEGAL_IN_STATE : invalid API state transition
 */
eip97_global_error_t
eip97_global_dfe_status_get(
        eip97_global_io_area_t * const io_area_p,
        const unsigned int pe_number,
        eip97_global_dfe_status_t * const dfe_status_p);


/*----------------------------------------------------------------------------
 * eip97_global_dse_status_get
 *
 * This function returns hardware status information in
 * the eip97_global_dse_status_t data structure.
 *
 * io_area_p (input)
 *     Pointer to the place holder in memory for the IO Area.
 *
 * pe_number (input)
 *     number of the PE for which the DSE status must be obtained.
 *
 * dse_status_p (output)
 *     Pointer to the data structure where the DSE status
 *     will be stored.
 *
 * This function is re-entrant.
 *
 * Return value
 *     EIP97_GLOBAL_NO_ERROR : operation is completed
 *     EIP97_GLOBAL_ARGUMENT_ERROR : Passed wrong argument
 *     EIP97_GLOBAL_ILLEGAL_IN_STATE : invalid API state transition
 */
eip97_global_error_t
eip97_global_dse_status_get(
        eip97_global_io_area_t * const io_area_p,
        const unsigned int pe_number,
        eip97_global_dse_status_t * const dse_status_p);


/*----------------------------------------------------------------------------
 * eip97_global_eip96_token_status_get
 *
 * This function returns hardware status information in
 * the eip96_token_status_t data structure.
 *
 * io_area_p (input)
 *     Pointer to the place holder in memory for the IO Area.
 *
 * pe_number (input)
 *     number of the PE for which the status must be obtained.
 *
 * token_status_p (output)
 *     Pointer to the data structure where the EIP-96 Token status
 *     will be stored.
 *
 * This function is re-entrant.
 *
 * Return value
 *     EIP97_GLOBAL_NO_ERROR : operation is completed
 *     EIP97_GLOBAL_ARGUMENT_ERROR : Passed wrong argument
 *     EIP97_GLOBAL_ILLEGAL_IN_STATE : invalid API state transition
 */
eip97_global_error_t
eip97_global_eip96_token_status_get(
        eip97_global_io_area_t * const io_area_p,
        const unsigned int pe_number,
        eip96_token_status_t * const token_status_p);


/*----------------------------------------------------------------------------
 * eip97_global_eip96_context_status_get
 *
 * This function returns hardware status information in
 * the eip96_context_status_t data structure.
 *
 * io_area_p (input)
 *     Pointer to the place holder in memory for the IO Area.
 *
 * pe_number (input)
 *     number of the PE for which the status must be obtained.
 *
 * context_status_p (output)
 *     Pointer to the data structure where the EIP-96 Context status
 *     will be stored.
 *
 * This function is re-entrant.
 *
 * Return value
 *     EIP97_GLOBAL_NO_ERROR : operation is completed
 *     EIP97_GLOBAL_ARGUMENT_ERROR : Passed wrong argument
 *     EIP97_GLOBAL_ILLEGAL_IN_STATE : invalid API state transition
 */
eip97_global_error_t
eip97_global_eip96_context_status_get(
        eip97_global_io_area_t * const io_area_p,
        const unsigned int pe_number,
        eip96_context_status_t * const context_status_p);


/*----------------------------------------------------------------------------
 * eip97_global_eip96_out_xfer_status_get
 *
 * This function returns hardware status information in
 * the eip96_output_transfer_status_t data structure.
 *
 * io_area_p (input)
 *     Pointer to the place holder in memory for the IO Area.
 *
 * pe_number (input)
 *     number of the PE for which the status must be obtained.
 *
 * out_xfer_status_p (output)
 *     Pointer to the data structure where the EIP-96 Output Transfer status
 *     will be stored.
 *
 * This function is re-entrant.
 *
 * Return value
 *     EIP97_GLOBAL_NO_ERROR : operation is completed
 *     EIP97_GLOBAL_ARGUMENT_ERROR : Passed wrong argument
 *     EIP97_GLOBAL_ILLEGAL_IN_STATE : invalid API state transition
 */
eip97_global_error_t
eip97_global_eip96_out_xfer_status_get(
        eip97_global_io_area_t * const io_area_p,
        const unsigned int pe_number,
        eip96_output_transfer_status_t * const out_xfer_status_p);


/*----------------------------------------------------------------------------
 * eip97_global_eip96_prng_status_get
 *
 * This function returns hardware status information in
 * the eip96_prng_status_t data structure.
 *
 * io_area_p (input)
 *     Pointer to the place holder in memory for the IO Area.
 *
 * pe_number (input)
 *     number of the PE for which the status must be obtained.
 *
 * out_xfer_status_p (output)
 *     Pointer to the data structure where the EIP-96 PRNG status
 *     will be stored.
 *
 * This function is re-entrant.
 *
 * Return value
 *     EIP97_GLOBAL_NO_ERROR : operation is completed
 *     EIP97_GLOBAL_ARGUMENT_ERROR : Passed wrong argument
 *     EIP97_GLOBAL_ILLEGAL_IN_STATE : invalid API state transition
 */
eip97_global_error_t
eip97_global_eip96_prng_status_get(
        eip97_global_io_area_t * const io_area_p,
        const unsigned int pe_number,
        eip96_prng_status_t * const prng_status_p);


#endif /* EIP97_GLOBAL_EVENT_H_ */


/* end of file eip97_global_event.h */
