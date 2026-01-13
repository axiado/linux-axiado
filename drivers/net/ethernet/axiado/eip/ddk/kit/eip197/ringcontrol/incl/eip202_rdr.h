/* eip202_rdr.h
 *
 * EIP-202 Driver Library API:
 * Result Descriptor Ring (RDR)
 *
 * All the RDR API functions can be used concurrently with the CDR API functions
 * for any ring interface ID unless the API function description states
 * otherwise.
 *
 * Refer to the EIP-202 Driver Library User Guide for more information about
 * usage of this API
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

#ifndef EIP202_RDR_H_
#define EIP202_RDR_H_


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Default configuration
#include "c_eip202_ring.h"

// Driver Framework Basic Definitions API
#include "basic_defs.h"             // u32, bool

// Driver Framework device API
#include "device_types.h"           // device_handle_t

// EIP-202 Ring Control Driver Library Common Types API
#include "eip202_ring_types.h"       // EIP202_* common types


/*----------------------------------------------------------------------------
 * Definitions and macros
 */

// Processed result descriptor Control data size (in 32-bit words)

#ifdef EIP202_64BIT_DEVICE
#define EIP202_RD_CTRL_DATA_MAX_WORD_COUNT  4
#else
#define EIP202_RD_CTRL_DATA_MAX_WORD_COUNT  2
#endif // !EIP202_64BIT_DEVICE

// RDR status information
typedef struct
{
    // Fatal errors (interrupts), set to true when pending
    // RDR must be re-initialized in case of a fatal error
    bool        f_dma_error;
    bool        f_error;
    bool        f_ou_flow_error;

    // Result Descriptor Buffer Overflow Interrupt (not fatal),
    // set to true when pending
    bool        f_rd_buf_overflow_int;

    // Result Descriptor Overflow Interrupt (not fatal),
    // set to true when pending
    bool        f_rd_overflow_int;

   /* The Buffer/Descriptor Overflow interrupts are not fatal errors and do
    * not require the reset. These events signal that packet or result
    * descriptor data  is lost. In this situation the packet processing result
    * must be discarded by the Host. These events when occur are also signaled
    * via the result descriptor bits.
    */

    // threshold Interrupt (not fatal), set to true when pending
    bool        f_treshold_int;

    // timeout Interrupt (not fatal), set to true when pending
    bool        f_timeout_int;

    // number of 32-bit words that are currently free
    // in the Result Descriptor fifo
    u16    rdfifo_word_count;

    // number of 32-bit words that are prepared in the RDR
    u32    rd_prep_word_count;

    // number of 32-bit words that are processed and stored in the RDR
    u32    rd_proc_word_count;

    // number of full packets (i.e. the amount of
    // descriptors marked Last_Seg written) written to the RDR
    // (i.e. ‘processed’ / ‘updated’) and not yet acknowledged (processed)
    // by the host.
    u8     rd_proc_pkt_word_count;
} eip202_rdr_status_t;

// RDR settings
typedef struct
{
    // Other ARM settings
    eip202_arm_ring_settings_t   params;

    // Make settings specific for continuous scatter mode.
    bool f_continuous_scatter;
} eip202_rdr_settings_t;

// Control word parameters for the Logical Prepared Descriptor
typedef struct
{
    // Set to true for the first descriptor in the descriptor chain
    bool        f_first_segment;

    // Set to true for the last descriptor in the descriptor chain
    bool        f_last_segment;

    // Prepared segment size in bytes
    u32    prep_segment_byte_count;

    // Expected result token data size in 32-bit words, optional
    u32    expected_result_word_count;

} eip202_rdr_prepared_control_t;

// Logical Prepared Descriptor
typedef struct
{
    // Control word for the prepared descriptor
    // eip202_rdr_write_prepared_control_word() helper function
    // can be used for obtaining this word
    u32                prep_control_word;

    // Destination packet data buffer, has to be provided by the caller:
    // Physical address that can be used by device DMA
    eip202_device_address_t   dst_packet_addr;

} eip202_arm_prepared_descriptor_t;

// Control word parameters for the Logical Result Descriptor
typedef struct
{
    // Set to true for the first descriptor in the descriptor chain
    bool        f_first_segment;

    // Set to true for the last descriptor in the descriptor chain
    bool        f_last_segment;

    // Set to true when the output data does not fit in the last output segment
    // assigned to this packet
    bool        f_buffer_overflow;

    // Set to true when the result data does not fit the result descriptor
    bool        f_dscr_overflow;

    // Actual processed segment size in bytes
    u32    proc_segment_byte_count;

    // Actual processed result token data size in 32-bit words,
    // for the last segment only
    u32    proc_result_word_count;

} eip202_rdr_result_control_t;

// Result Token data data structure embedded into processed result descriptors
typedef struct
{
    // PE specific error code
    u32    error_code;

    // Result packet size,
    // sum of all actual segment sizes that belong to this packet
    u32    packet_byte_count;

    u8     next_header;
    u8     hash_byte_count;
    u8     bypass_word_count;  // in 32-bit words
    u8     pad_byte_count;
    bool        f_e15;             // true when E15 occurred
    bool        f_hash;            // true when hash "hash_byte_count" bytes are
                                  // appended at the end of packet data
    u8     bcnl;             // bcnl mask in bits 0 (B) - 3 (L)

    // This parameter is copied through from command to result descriptor
    // unless EIP202_RING_ANTI_DMA_RACE_CONDITION_CDS configuration parameter
    // is defined in c_eip202_ring.h
    u32    application_id;

    // Bypass token words received from the PE
    // Optional, up to 3 32-bit words, actual length is in "bypass_word_count"
    u32*   bypass_data_p;

} eip202_rdr_result_token_t;

// Bypass data for successful packet processing
typedef struct
{
    // type of Service / Traffic class
    u8 TOS_TC;

    // Don't fragment flag
    bool f_df;

    // Next Header field offset within IPv6 packet header header
    u16 next_header_offset;

    // Application-specific reference to the Header Processing Context
    u32 hdr_proc_ctx_ref;

} eip202_rdr_bypass_data_pass_t;

// Bypass data for failed packet processing
typedef struct
{
    // See EIP202_RDR_BYPASS_FLAG_*
    u8 error_flags;

} eip202_rdr_bypass_data_fail_t;

// Bypass data
typedef union
{
    // Use when bypass_word_count = 2 in Result Token
    eip202_rdr_bypass_data_pass_t pass;

    // Use when bypass_word_count = 1 in Result Token
    eip202_rdr_bypass_data_fail_t fail;

} eip202_rdr_bypass_data_t;

// Logical Result Descriptor
typedef struct
{
    // control fields for the command descriptor
    // eip202_rdr_read_processed_control_word() helper function
    // can be used for obtaining this word
    u32 proc_control_word;

    // Destination packet (segment) data, has to be provided by the caller:
    // Physical address that can be used by device DMA
    eip202_device_address_t dst_packet_addr;

    // control fields for the command descriptor
    // eip202_rdr_read_processed_control_word() helper function
    // can be used for obtaining this word

    // Output Token buffer with fixed size
    u32 * token_p;

} eip202_arm_result_descriptor_t;


/*----------------------------------------------------------------------------
 * RDR Initialization API functions
 ----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * eip202_rdr_init
 *
 * This function performs the initialization of the EIP-202 RDR
 * interface and transits the API to the Initialized state.
 *
 * This function returns the EIP202_RING_UNSUPPORTED_FEATURE_ERROR error code
 * when it detects a mismatch in the Ring Control Driver Library configuration.
 *
 * Note: This function must be called either after the EIP-202 HW Reset or
 *       after the Ring SW Reset, see the eip202_rdr_reset() function.
 *       This function as well as optionally eip202_rdr_reset() function must be
 *       executed before any other of the EIP202_RDR_*() functions can be called.
 *
 * This function cannot be called concurrently with
 * the eip202_rdr_reset() function for the same device.
 *
 * io_area_p (output)
 *     Pointer to the place holder in memory for the IO Area
 *     for the RDR instance.
 *
 * device (input)
 *     handle for the Ring Control device instance returned by eip_device_find()
 *     for this RDR instance.
 *
 * ring_settings_p (input)
 *     Pointer to the data structure that contains RDR configuration parameters
 *
 * This function is NOT re-entrant for the same device.
 * This function is re-entrant for different Devices.
 *
 * Return value
 *     EIP202_RING_NO_ERROR : operation is completed
 *     EIP202_RING_UNSUPPORTED_FEATURE_ERROR : not supported by the device.
 *     EIP202_RING_ARGUMENT_ERROR : Passed wrong argument
 *     EIP202_RING_ILLEGAL_IN_STATE : invalid API state transition
 */
eip202_ring_error_t
eip202_rdr_init(
        eip202_ring_io_area_t * io_area_p,
        const device_handle_t device,
        const eip202_rdr_settings_t * const ring_settings_p);


/*----------------------------------------------------------------------------
 * eip202_rdr_reset
 *
 * This function performs the RDR SW Reset and transits the API
 * to the Uninitialized state. This function must be called before using
 * the other RDR API functions if the state of the ring is Unknown.
 *
 * This function can be used to recover the RDR from a fatal error.
 *
 * Note: This function must be called before calling the eip202_rdr_init()
 *       function only if the EIP-202 HW Reset was not done. Otherwise it still
 *       is can be called but it is not necessary.
 *
 * This function cannot be called concurrently with
 * the eip202_rdr_init() function for the same device.
 *
 * io_area_p (output)
 *     Pointer to the place holder in memory for the IO Area
 *     for the RDR instance identified by the device parameter.
 *
 * device (input)
 *     handle for the Ring Control device instance returned by eip_device_find
 *     for this RDR instance.
 *
 * This function is NOT re-entrant for the same device.
 * This function is re-entrant for different Devices.
 *
 * Return value
 *     EIP202_RING_NO_ERROR : operation is completed
 *     EIP202_RING_ARGUMENT_ERROR : Passed wrong argument
 *     EIP202_RING_ILLEGAL_IN_STATE : invalid API state transition
 */
eip202_ring_error_t
eip202_rdr_reset(
        eip202_ring_io_area_t * const io_area_p,
        const device_handle_t device);


/*----------------------------------------------------------------------------
 * RDR Descriptor I/O API functions
 ----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * eip202_rdr_fill_level_get
 *
 * This function outputs the fill level of the RDR for all the descriptors
 * requested by means of the RingID parameter. The fill level is obtained as
 * a number of prepared  (via eip202_rdr_descriptor_prepare) and processed result
 * descriptors that are NOT obtained (via eip202_rdr_descriptor_get)
 * by the Host yet.
 *
 * The RDR states such as "Ring Full", "Ring Empty" and "Ring Free" are not
 * changed by this function, e.g. it does not perform any state transition.
 *
 * This function cannot be called concurrently with
 * the eip202_rdr_descriptor_prepare() function for the same device.
 *
 * io_area_p (input)
 *     Pointer to the place holder in memory for the IO Area
 *     which contains the device handle for the RDR instance.
 *
 * fill_level_dscr_count_p (output)
 *     Pointer to the memory location where the number of prepared
 *     descriptors pending in the RDR will be stored
 *
 * This function is re-entrant for any device.
 *
 * Return value
 *     EIP202_RING_NO_ERROR : operation is completed
 *     EIP202_RING_ARGUMENT_ERROR : Passed wrong argument
 *     EIP202_RING_ILLEGAL_IN_STATE : invalid API state transition
 */
eip202_ring_error_t
eip202_rdr_fill_level_get(
        eip202_ring_io_area_t * const io_area_p,
        unsigned int * fill_level_dscr_count_p);


/*----------------------------------------------------------------------------
 * eip202_rdr_prepared_fill_level_get
 *
 * This function outputs the fill level of the RDR for the Prepared Descriptors
 * requested by means of the RingID parameter. The fill level is obtained as
 * a number of prepared descriptors that have been submitted to the RDR but
 * not processed by the device yet.
 *
 * The RDR states such as "Ring Full", "Ring Empty" and "Ring Free" are not
 * changed by this function, e.g. it does not perform any state transition.
 *
 * This function cannot be called concurrently with
 * the eip202_rdr_descriptor_prepare() function for the same device.
 *
 * io_area_p (input)
 *     Pointer to the place holder in memory for the IO Area
 *     which contains the device handle for the RDR instance.
 *
 * fill_level_dscr_count_p (output)
 *     Pointer to the memory location where the number of prepared
 *     descriptors pending in the RDR will be stored
 *
 * This function is re-entrant for any device.
 *
 * Return value
 *     EIP202_RING_NO_ERROR : operation is completed
 *     EIP202_RING_ARGUMENT_ERROR : Passed wrong argument
 *     EIP202_RING_ILLEGAL_IN_STATE : invalid API state transition
 */
eip202_ring_error_t
eip202_rdr_prepared_fill_level_get(
        eip202_ring_io_area_t * const io_area_p,
        unsigned int * fill_level_dscr_count_p);


/*----------------------------------------------------------------------------
 * eip202_rdr_write_prepared_control_word
 *
 * This helper function returns the control word that can be written to
 * the logical prepared descriptor.
 *
 * This function is re-entrant.
 *
 */
u32
eip202_rdr_write_prepared_control_word(
        const eip202_rdr_prepared_control_t * const  prepared_ctrl_p);


/*----------------------------------------------------------------------------
 * eip202_rdr_descriptor_prepare
 *
 * This function prepares a requested number of descriptors to the RDR.
 * The function returns EIP202_RING_NO_ERROR and zero descriptors done count
 * (*dscr_done_count_p set to 0) when no descriptors can be added to the RDR.
 * Until prepared descriptors are submitted to the RDR no result descriptors
 * can be retrieved from it. The prepared descriptors must refer to the Packet
 * data DMA buffers that are large enough to receive the processed by the PE
 * data submitted via the CDR (see the EIP-202 Ring Control CDR API).
 *
 * The RDR states such as "Ring Full", "Ring Empty" and "Ring Free" are not
 * changed by this function, e.g. it does not perform any state transition.
 *
 * This function can be called after
 * the eip202_rdr_fill_level_get() function when the latter checks
 * how many descriptors can be added to the RDR.
 *
 * The execution context calling this API must
 *
 * 1) Provide input data via the RDR using the eip202_rdr_descriptor_prepare()
 * function before it or another context can obtain output data from the RDR.
 * This requirement is relevant for the look-aside and in-line use case only.
 *
 * 2) Ensure that Packet data DMA buffers referred to by the prepared
 * descriptors are not re-used or freed by it or another context until
 * the processed result descriptor(s) referring to the packet associated with
 * these buffers is(are) fully processed by the device. This is required not
 * only when in-place packet transform is done using the same Packet data DMA
 * buffer as input and output buffer but also when different DMA buffers are
 * used for the packet processing input and output data.
 *
 * 3) Keep the packet descriptor chain state consistent. All descriptors that
 * belong to the same packet must be submitted atomically into the RDR without
 * being intermixed with descriptors that belong to other packets.
 *
 * 4) Submit the descriptors that belong to the same packet descriptor chain in
 * the right order, e.g. first descriptor for the first segment followed by
 * the middle descriptors followed by the last descriptor.
 *
 * 5) Ensure it does not call this function concurrently with another context
 * for the same RingID.
 *
 * 6) Ensure it does not call this function concurrently with another context
 * calling the eip202_rdr_prepared_fill_level_get() and
 * eip202_rdr_fill_level_get() functions for the same device.
 *
 * io_area_p (input)
 *     Pointer to the place holder in memory for the IO Area
 *     which contains the device handle for the RDR instance.
 *
 * prepared_dscr_p (input)
 *     Pointer to 1st in the the array of prepared descriptors.
 *
 * dscr_requested_count (input)
 *     number of prepared descriptors stored back-to-back in the array
 *     pointed to by prepared_dscr_p.
 *
 * dscr_prepared_count_p (output)
 *     Pointer to the memory location where the number of prepared descriptors
 *     actually added to the RDR will be stored.
 *
 * fill_level_dscr_count_p (output)
 *     Pointer to the memory location where the number of prepared
 *     descriptors pending in the RDR will be stored
 *
 * This function is NOT re-entrant for the same device.
 * This function is re-entrant for different Devices.
 *
 * Return value
 *     EIP202_RING_NO_ERROR : operation is completed
 *     EIP202_RING_UNSUPPORTED_FEATURE_ERROR : feature is not supported
 *     EIP202_RING_ARGUMENT_ERROR : passed wrong argument
 *     EIP202_RING_ILLEGAL_IN_STATE : invalid API state transition
 */
eip202_ring_error_t
eip202_rdr_descriptor_prepare(
        eip202_ring_io_area_t * const io_area_p,
        const eip202_arm_prepared_descriptor_t * prepared_dscr_p,
        const unsigned int dscr_requested_count,
        unsigned int * const dscr_prepared_count_p,
        unsigned int * fill_level_dscr_count_p);


/*----------------------------------------------------------------------------
 * eip202_rdr_processed_fill_level_get
 *
 * This function outputs the fill level of the ring requested by means of
 * the RingID parameter. The fill level is obtained as a number of result
 * descriptors that have been processed by the device but not processed
 * by the Host yet.
 *
 * When the RDR is in the "Ring Full" state this function outputs the fill
 * level equal to the RDR size. When the RDR is in the "Ring Empty" state this
 * functions outputs the zero fill level. the RDR is in the "Ring Free" state
 * when this function returns the fill level that is greater than zero and less
 * than the RDR size (in result descriptors).
 *
 * This function cannot be called concurrently with
 * the eip202_rdr_descriptor_get() function for the same device.
 *
 * Note: This function returns EIP202_RING_UNSUPPORTED_FEATURE_ERROR
 *       when the EIP202_RING_ANTI_DMA_RACE_CONDITION_CDS parameter is
 *       set.
 *
 * io_area_p (input)
 *     Pointer to the place holder in memory for the IO Area
 *     which contains the device handle for the RDR instance.
 *
 * fill_level_dscr_count_p (output)
 *     Pointer to the memory location where the number of processed result
 *     descriptors pending in the RDR will be stored
 *     RDR Empty: fill_level = 0
 *     RDR Free:  0 < fill_level < RingSize (in descriptors)
 *     RDR Full:  fill_level = RingSize (in descriptors)
 *
 * fill_level_pkt_count_p (output)
 *     Pointer to the memory location where the number of processed result
 *     packets (whose descriptors are pending in the RDR) will be stored.
 *     In case this parameter outputs 127 then there can be more packets
 *     available.
 *
 * This function is re-entrant for any device.
 *
 * Return value
 *     EIP202_RING_NO_ERROR : operation is completed
 *     EIP202_RING_UNSUPPORTED_FEATURE_ERROR : feature is not supported
 *     EIP202_RING_ARGUMENT_ERROR : Passed wrong argument
 *     EIP202_RING_ILLEGAL_IN_STATE : invalid API state transition
 */
eip202_ring_error_t
eip202_rdr_processed_fill_level_get(
        eip202_ring_io_area_t * const io_area_p,
        unsigned int * fill_level_dscr_count_p,
        unsigned int * fill_level_pkt_count_p);


/*----------------------------------------------------------------------------
 * eip202_rdr_read_processed_control_word
 *
 * This helper function outputs the control word that can be read from
 * the logical processed result descriptor.
 *
 * res_dscr_p (input)
 *     Processed result descriptor with the control information and
 *     token result data must be read from.
 *
 * rd_control_p (output)
 *     Pointer to the data structure where the result descriptor control
 *     information will be written.
 *
 * res_token_p (output)
 *     Pointer to the data structure where the result token
 *     information will be written.
 *
 * This function is re-entrant.
 *
 */
void
eip202_rdr_read_processed_control_word(
        eip202_arm_result_descriptor_t * const  res_dscr_p,
        eip202_rdr_result_control_t * const rd_control_p,
        eip202_rdr_result_token_t * const res_token_p);


/*----------------------------------------------------------------------------
 * eip202_rdr_read_processed_bypass_data
 *
 * This helper function outputs the bypass data that can be read from
 * the result token data of the logical processed result descriptor.
 *
 * res_token_p (input)
 *     Pointer to the data structure where the result token
 *     information will be written.
 *
 * bd_p (output)
 *     Pointer to the data structure where the bypass data
 *     information will be written.
 *
 * This function is re-entrant.
 */
void
eip202_rdr_read_processed_bypass_data(
        const eip202_rdr_result_token_t * const  res_token_p,
        eip202_rdr_bypass_data_t * const bd_p);


/*----------------------------------------------------------------------------
 * eip202_rdr_descriptor_get
 *
 * This function gets a requested number of result descriptors from the RDR.
 * The function returns EIP202_RING_NO_ERROR and zero descriptors done count
 * (*dscr_done_count_p set to 0) when no descriptors can be retrieved from
 * the RDR, e.g. "Ring Empty" state.
 *
 * This function can be used together with
 * the eip202_rdr_processed_fill_level_get() function when the latter checks
 * how many command descriptors can be retrieved from the RDR.
 *
 * The execution context calling this API function must
 *
 * 1) Ensure it does not call this function concurrently with another context
 * for the same device;
 *
 * 2) Ensure it does not call this function concurrently with another context
 * calling the eip202_rdr_processed_fill_level_get() function for the same device.
 *
 * io_area_p (input)
 *     Pointer to the place holder in memory for the IO Area
 *     which contains the device handle for the RDR instance.
 *
 * result_dscr_p (input)
 *     Pointer to 1st in the the array of command descriptors.
 *
 * pkt_requested_count (input)
 *     Maximum number of packet descriptor chains which result descriptors
 *     should be obtained by this function. This function will obtain
 *     the result descriptors for the fully processed packet chains only.
 *     If set to 0 then this function will try to obtain no more result
 *     descriptors than specified by the dscr_requested_count parameter.
 *
 * dscr_requested_count (input)
 *     number of descriptors stored back-to-back in the array
 *     pointed to by result_dscr_p. Cannot be zero.
 *
 * dscr_done_count_p (output)
 *     Pointer to the memory location where the number of descriptors
 *     actually added to the RDR will be stored.
 *
 * fill_level_dscr_count_p (output)
 *     Pointer to the memory location where the number of processed result
 *     descriptors pending in the RDR will be stored
 *     RDR Empty: fill_level = 0
 *     RDR Free:  0 < fill_level < RingSize (in descriptors)
 *     RDR Full:  fill_level = RingSize (in descriptors)
 *
 * This function is NOT re-entrant for the same device.
 * This function is re-entrant for different Devices.
 *
 * Return value
 *     EIP202_RING_NO_ERROR : operation is completed
 *     EIP202_RING_UNSUPPORTED_FEATURE_ERROR : feature is not supported
 *     EIP202_RING_ARGUMENT_ERROR : passed wrong argument
 *     EIP202_RING_ILLEGAL_IN_STATE : invalid API state transition
 */
eip202_ring_error_t
eip202_rdr_descriptor_get(
        eip202_ring_io_area_t * const io_area_p,
        eip202_arm_result_descriptor_t * result_dscr_p,
        const unsigned int pkt_requested_count,
        const unsigned int dscr_requested_count,
        unsigned int * const dscr_done_count_p,
        unsigned int * fill_level_dscr_count_p);


/*----------------------------------------------------------------------------
 * RDR Event Management API functions
 ----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * eip202_rdr_status_get
 *
 * This function retrieves the RDR status information. It can be called
 * periodically to monitor the RDR status and occurrence of fatal errors.
 *
 * In case of a fatal error the eip202_rdr_reset() function can be called to
 * recover the RDR and bring it to the sane and safe state.
 * The RDR SW Reset by means of the eip202_rdr_reset() function as well
 * as the Global SW Reset by means of the eip202_global_reset() function
 * or the HW Reset must be performed too.
 *
 * io_area_p (input)
 *     Pointer to the place holder in memory for the IO Area
 *     which contains the device handle for this RDR instance.
 *
 * status_p (output)
 *     Pointer to the memory location where the RDR status information
 *     will be stored
 *
 * This function is re-entrant for any device.
 *
 * Return value
 *     EIP202_RING_NO_ERROR : operation is completed
 *     EIP202_RING_ARGUMENT_ERROR : Passed wrong argument
 *     EIP202_RING_ILLEGAL_IN_STATE : invalid API state transition
 */
eip202_ring_error_t
eip202_rdr_status_get(
        eip202_ring_io_area_t * const io_area_p,
        eip202_rdr_status_t * const status_p);


/*----------------------------------------------------------------------------
 * eip202_rdr_processed_fill_level_high_int_enable
 *
 * This function enables the command Descriptor threshold and timeout
 * interrupts. This function does not change the current API state. It must
 * be called every time after the RDR threshold or timeout interrupt occurs
 * in order to re-enable these one-shot interrupts.
 *
 * The RDR Manager interrupts are routed to the EIP-201 Advanced Interrupt
 * Controller (aic) which in its turn can be connected to a System Interrupt
 * Controller (SIC).
 *
 * io_area_p (input)
 *     Pointer to the place holder in memory for the IO Area
 *     which contains the device handle for the RDR instance.
 *
 * threshold_dscr_count (input)
 *     When the RDR descriptor fill level reaches the value below the number
 *     specified by this parameter the RDR threshold interrupt (cdr_thresh_irq)
 *     will be generated.
 *
 * timeout (input)
 *     When the RDR descriptor fill level is non-zero and not decremented
 *     for the amount of time specified by this parameter the RDR timeout
 *     interrupt (cdr_timeout_irq) will be generated.
 *     The timeout value must be specified in 256 clock cycles
 *     of the EIP-202 HIA clock.
 *
 * f_int_per_packet (input)
 *     Descriptor-oriented or Packet-oriented Interrupts
 *     (rd_proc_thresh_irq and rd_proc_timeout_irq)
 *     When set to true the interrupts will be generated per packet,
 *     otherwise interrupts are generated per descriptor
 *
 * This function is re-entrant for any device.
 *
 * Return value
 *     EIP202_RING_NO_ERROR : operation is completed
 *     EIP202_RING_ARGUMENT_ERROR : Passed wrong argument
 *     EIP202_RING_ILLEGAL_IN_STATE : invalid API state transition
 */
eip202_ring_error_t
eip202_rdr_processed_fill_level_high_int_enable(
        eip202_ring_io_area_t * const io_area_p,
        const unsigned int threshold_dscr_count,
        const unsigned int timeout,
        const bool f_int_per_packet);


/*----------------------------------------------------------------------------
 * eip202_rdr_processed_fill_level_high_int_clear_and_disable
 *
 * This function clears and disables the Result Descriptor
 * threshold (cdr_thresh_irq) and timeout (cdr_timeout_irq) interrupts as well
 * as the Descriptor (rd_proc_oflo_irq) and Buffer (rd_buf_oflo_irq) Overflow
 * interrupts. This function does not change the current API state.
 *
 * This function must be called as soon as these interrupts occur.
 * The occurrence of these interrupts can be detected
 * by means of the eip202_rdr_status_get() function, for example from an
 * Interrupt Service Routine.
 *
 * io_area_p (input)
 *     Pointer to the place holder in memory for the IO Area
 *     which contains the device handle for the RDR instance.
 *
 * f_ovfl_int_only (input)
 *     When true this function will clear the buffer and descriptor overflow
 *     interrupts only without clearing and disabling the already enabled RDR
 *     threshold and timeout interrupts at the same time.
 *
 * This function is NOT re-entrant for the same device.
 * This function is re-entrant for different Devices.
 *
 * Return value
 *     EIP202_RING_NO_ERROR : operation is completed
 *     EIP202_RING_ARGUMENT_ERROR : Passed wrong argument
 *     EIP202_RING_ILLEGAL_IN_STATE : invalid API state transition
 */
eip202_ring_error_t
eip202_rdr_processed_fill_level_high_int_clear_and_disable(
        eip202_ring_io_area_t * const io_area_p,
        const bool f_ovfl_int_only);


/*----------------------------------------------------------------------------
 * eip202_rdr_dump
 *
 */
void
eip202_rdr_dump(
        eip202_ring_io_area_t * const io_area_p,
        eip202_ring_admin_t * const ring_admin_p);


/* end of file eip202_rdr.h */


#endif /* EIP202_RDR_H_ */
