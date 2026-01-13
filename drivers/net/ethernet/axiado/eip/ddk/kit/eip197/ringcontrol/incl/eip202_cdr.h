/* eip202_cdr.h
 *
 * EIP-202 Driver Library API:
 * command Descriptor Ring (CDR)
 *
 * All the CDR API functions can be used concurrently with the RDR API functions
 * for any ring interface ID unless the API function description states
 * otherwise.
 *
 * Refer to the EIP-202 Driver Library User Guide for more information about
 * usage of this API.
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

#ifndef EIP202_CDR_H_
#define EIP202_CDR_H_


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

// command descriptor Control data size (in 32-bit words)
// Note: This is valid for ATP mode only!
#ifdef EIP202_64BIT_DEVICE
#define EIP202_CD_CTRL_DATA_MAX_WORD_COUNT  6
#else
#define EIP202_CD_CTRL_DATA_MAX_WORD_COUNT  3
#endif // !EIP202_64BIT_DEVICE

// CDR status information
typedef struct
{
    // Fatal errors (interrupts), set to true when pending
    // CDR must be re-initialized in case of a fatal error
    bool f_dma_error;
    bool f_error;
    bool f_ou_flow_error;

    // threshold Interrupt, set to true when pending
    bool f_treshold_int;

    // timeout Interrupt, set to true when pending
    bool f_timeout_int;

    // number of 32-bit words that are currently free
    // in the command Descriptor fifo
    u16 cdfifo_word_count;

    // number of valid 32-bit Prepared command Descriptor words that
    // are prepared in the CDR
    u32 cd_prep_word_count;

    // number of valid 32-bit Processed command Descriptor words that
    // are processed in the CDR
    u32 cd_proc_word_count;

    // number of full packets (i.e. the number of descriptors marked Last)
    // that are fully processed by the DFE and not yet acknowledged by Host.
    // If more than 127 packets are processed this field returns the value 127.
    u8 cd_proc_pkt_word_count;
} eip202_cdr_status_t;

// CDR settings
typedef struct
{
    // Additional Token Pointer Descriptor Mode
    // When true the token data can be stored in a separate from the descriptor
    // DMA buffer
    bool                        f_atp;

    // When true then the tokens consisting out of 1 or 2 32-bit words
    // can be passed to the PE directly via the command descriptor
    bool                        f_at_pto_token;

    // Other ARM settings
    eip202_arm_ring_settings_t  params;

} eip202_cdr_settings_t;

// Control word parameters for the Logical command Descriptor
typedef struct
{
    // Set to true for the first descriptor in the descriptor chain
    bool        f_first_segment;

    // Set to true for the last descriptor in the descriptor chain
    bool        f_last_segment;

    // Segment size in bytes, can be 0 for an empty segment
    u32    segment_byte_count;

    // Token size in 32-bit words,
    // important for the first segment only but can be 0 too,
    // must be always 0 for the non-first segments
    u8     token_word_count;

    // Force the command to be processed on a specific engine.
    bool        f_force_engine;

    // Engine ID to process the command on.
    u8     engine_id;

} eip202_cdr_control_t;

// Logical command Descriptor
typedef struct
{
    // control fields for the command descriptor
    // eip202_cdr_write_control_word() helper function
    // can be used for obtaining this word
    u32 control_word;

    // Token header word
    u32 token_header;

    // This parameter is copied through from command to result descriptor
    // unless EIP202_RING_ANTI_DMA_RACE_CONDITION_CDS configuration parameter
    // is defined in c_eip202_ring.h
    u32 application_id;

    // source packet data length, in bytes
    unsigned int src_packet_byte_count;

    // source packet data, has to be provided by the caller:
    // Physical address that can be used by device DMA
    eip202_device_address_t src_packet_addr;

    // Context data DMA buffer, has to be allocated and filled in by the caller
    // Physical address that can be used by device DMA
    eip202_device_address_t token_data_addr;

    // Context data DMA buffer, has to be allocated and filled in by the caller
    // Physical address that can be used by device DMA
    eip202_device_address_t context_data_addr;

    // Input Token buffer with fixed size
    u32 * token_p;

} eip202_arm_command_descriptor_t;


/*----------------------------------------------------------------------------
 * CDR Initialization API functions
 ----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * eip202_cdr_init
 *
 * This function performs the initialization of the EIP-202 CDR
 * interface and transits the API to the Initialized state.
 *
 * This function returns the EIP202_RING_UNSUPPORTED_FEATURE_ERROR error code
 * when it detects a mismatch in the Ring Control Driver Library configuration.
 *
 * Note: This function should be called either after the EIP-202 HW Reset or
 *       after the Ring SW Reset, see the eip202_cdr_reset() function.
 *       This function as well as optionally eip202_cdr_reset() function must be
 *       executed before any other of the EIP202_CDR_*() functions can be called.
 *
 * This function cannot be called concurrently with
 * the eip202_cdr_reset() function for the same device.
 *
 * io_area_p (output)
 *     Pointer to the place holder in memory for the IO Area
 *     for the CDR instance identified by the device parameter.
 *
 * device (input)
 *     handle for the Ring Control device instance returned by eip_device_find
 *     for this CDR instance.
 *
 * cdr_settings_p (input)
 *     Pointer to the data structure that contains CDR configuration parameters
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
eip202_cdr_init(
        eip202_ring_io_area_t * io_area_p,
        const device_handle_t device,
        const eip202_cdr_settings_t * const cdr_settings_p);


/*----------------------------------------------------------------------------
 * eip202_cdr_reset
 *
 * This function performs the CDR SW Reset and transits the API
 * to the Uninitialized state. This function must be called before using
 * the other CDR API functions if the state of the ring is Unknown.
 *
 * This function can be used to recover the CDR from a fatal error.
 *
 * Note: This function must be called before calling the eip202_cdr_init()
 *       function only if the EIP-202 HW Reset was not done. Otherwise it still
 *       is can be called but it is not necessary.
 *
 * This function cannot be called concurrently with
 * the eip202_cdr_init() function for the same device.
 *
 * io_area_p (output)
 *     Pointer to the place holder in memory for the IO Area
 *     for the CDR instance identified by the device parameter.
 *
 * device (input)
 *     handle for the Ring Control device instance returned by eip_device_find
 *     for this CDR instance.
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
eip202_cdr_reset(
        eip202_ring_io_area_t * const io_area_p,
        const device_handle_t device);

/*----------------------------------------------------------------------------
 * eip202_cdr_options_get
 *
 * This function reads the local options register of the CDR device and
 * returns the information in a data structure of type eip202_ring_options_t.
 *
 * Note: the options register (and therefore this function) is not available
 *       in all versions of the EIP202.
 *
 * This function can be called in any state, even when the device is not
 * yet initialized.
 *
 * device (input)
 *     handle for the Ring Control device instance returned by eip_device_find
 *     for this CDR instance.
 *
 * options_p (output)
 *     Pointer to the options data structure to be filled in by this
 *     function.
 *
 * This function is re-entrant for any device.
 *
 * Return value
 *     EIP202_RING_NO_ERROR : operation is completed
 *     EIP202_RING_UNSUPPORTED_FEATURE_ERROR : not supported by the device.
 *     EIP202_RING_ARGUMENT_ERROR : Passed wrong argument
 */
eip202_ring_error_t
eip202_cdr_options_get(
        const device_handle_t device,
        eip202_ring_options_t * const options_p);


/*----------------------------------------------------------------------------
 * CDR Descriptor I/O API functions
 ----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * eip202_cdr_fill_level_get
 *
 * This function outputs the fill level of the ring requested by means of
 * the device parameter in the I/O Area. The fill level is obtained as a number
 * of command descriptors that have been submitted by the Host to the CDR but
 * not processed by the device yet.
 *
 * When the CDR is in the "Ring Full" state this function outputs the fill
 * level equal to the CDR size. When the CDR is in the "Ring Empty" state this
 * functions outputs the zero fill level. the CDR is in the "Ring Free" state
 * when this function returns the fill level that is greater than zero and less
 * than the CDR size (in command descriptors).
 *
 * This function cannot be called concurrently with
 * the eip202_cdr_descriptor_put() function for the same device.
 *
 * io_area_p (input)
 *     Pointer to the place holder in memory for the IO Area
 *     for the CDR instance which contains the device handle.
 *
 * fill_level_dscr_count_p (output)
 *     Pointer to the memory location where the number of command
 *     descriptors pending in the CDR will be stored
 *     CDR Empty: fill_level = 0
 *     CDR Free:  0 < fill_level < RingSize (in descriptors)
 *     CDR Full:  fill_level = RingSize (in descriptors)
 *
 * This function is re-entrant for any device.
 *
 * Return value
 *     EIP202_RING_NO_ERROR : operation is completed
 *     EIP202_RING_ARGUMENT_ERROR : Passed wrong argument
 *     EIP202_RING_ILLEGAL_IN_STATE : invalid API state transition
 */
eip202_ring_error_t
eip202_cdr_fill_level_get(
        eip202_ring_io_area_t * const io_area_p,
        unsigned int * fill_level_dscr_count_p);


/*----------------------------------------------------------------------------
 * eip202_cdr_write_control_word
 *
 * This helper function returns the control word that can be written to
 * the logical command descriptor.
 *
 * This function is re-entrant.
 *
 */
u32
eip202_cdr_write_control_word(
        const eip202_cdr_control_t * const  command_ctrl_p);


/*----------------------------------------------------------------------------
 * eip202_cdr_descriptor_put
 *
 * This function puts a requested number of command descriptors to the CDR.
 * The function returns EIP202_RING_NO_ERROR and zero descriptors done count
 * (*dscr_done_count_p set to 0) when no descriptors can be added to the CDR,
 * e.g. "Ring Full" state.
 *
 * This function can be called after the eip202_cdr_fill_level_get()
 * function when the latter checks how many command descriptors can be added
 * to the CDR.
 *
 * The execution context calling this API function must
 *
 * 1) Provide input data via the CDR using the eip202_cdr_descriptor_put()
 * function before it or another context can obtain output data from the RDR.
 * This requirement is relevant for the look-aside use case only.
 *
 * 2) Ensure that the Token data, Packet data and Context data DMA buffers are
 * not re-used or freed by it or another context until the processed result
 * descriptor(s) referring to the packet associated with these buffers is(are)
 * fully processed by the device. This is required not only when in-place
 * packet transform is done using the same Packet data DMA buffer as input and
 * output buffer but also when different DMA buffers are used for the packet
 * processing input and output data.
 *
 * 3) Keep the packet descriptor chain state consistent. All descriptors that
 * belong to the same packet must be submitted atomically into the CDR without
 * being intermixed with descriptors that belong to other packets.
 *
 * 4) Submit the descriptors that belong to the same packet descriptor chain in
 * the right order, e.g. first descriptor for the first segment followed by
 * the middle descriptors followed by the last descriptor.
 *
 * 5) Ensure it does not call this function concurrently with another context
 * for the same device.
 *
 * 6) Ensure it does not call this function concurrently with another context
 * calling the eip202_cdr_fill_level_get() function for the same device.
 *
 * io_area_p (input)
 *     Pointer to the place holder in memory for the IO Area
 *     for the CDR instance which contains the device handle.
 *
 * command_dscr_p (input)
 *     Pointer to 1st in the the array of command descriptors.
 *
 * dscr_requested_count (input)
 *     number of descriptors stored back-to-back in the array
 *     pointed to by command_dscr_p.
 *
 * dscr_done_count_p (output)
 *     Pointer to the memory location where the number of descriptors
 *     actually added to the CDR will be stored.
 *
 * fill_level_dscr_count_p (output)
 *     Pointer to the memory location where the number of command
 *     descriptors pending in the CDR will be stored
 *     CDR Empty: fill_level = 0
 *     CDR Free:  0 < fill_level < RingSize (in descriptors)
 *     CDR Full:  fill_level = RingSize (in descriptors)
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
eip202_cdr_descriptor_put(
        eip202_ring_io_area_t * const io_area_p,
        const eip202_arm_command_descriptor_t * command_dscr_p,
        const unsigned int dscr_requested_count,
        unsigned int * const dscr_done_count_p,
        unsigned int * fill_level_dscr_count_p);


/*----------------------------------------------------------------------------
 * CDR Event Management API functions
 ----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * eip202_cdr_status_get
 *
 * This function retrieves the CDR status information. It can be called
 * periodically to monitor the CDR status and occurrence of fatal errors.
 *
 * In case of a fatal error the eip202_cdr_reset() function can be called to
 * recover the CDR and bring it to the sane and safe state.
 * The RDR SW Reset by means of the eip202_rdr_reset() function as well
 * as the Global SW Reset by means of the eip202_global_reset() function
 * or the HW Reset must be performed too.
 *
 * io_area_p (input)
 *     Pointer to the place holder in memory for the IO Area
 *     for this CDR instance which contains the device handle.
 *
 * status_p (output)
 *     Pointer to the memory location where the CDR status information
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
eip202_cdr_status_get(
        eip202_ring_io_area_t * const io_area_p,
        eip202_cdr_status_t * const status_p);


/*----------------------------------------------------------------------------
 * eip202_cdr_fill_level_low_int_enable
 *
 * This function enables the command Descriptor threshold and timeout
 * interrupts. This function does not change the current API state. It must
 * be called every time after the CDR threshold or timeout interrupt occurs
 * in order to re-enable these one-shot interrupts.
 *
 * The CDR Manager interrupts are routed to the EIP-201 Advanced Interrupt
 * Controller (aic) which in its turn can be connected to a System Interrupt
 * Controller (SIC).
 *
 * io_area_p (input)
 *     Pointer to the place holder in memory for the IO Area
 *     for the CDR instance which contains the device handle.
 *
 * threshold_dscr_count (input)
 *     When the CDR descriptor fill level reaches the value below the number
 *     specified by this parameter the CDR threshold interrupt (cdr_thresh_irq)
 *     will be generated.
 *
 * timeout (input)
 *     When the CDR descriptor fill level is non-zero and not decremented
 *     for the amount of time specified by this parameter the CDR timeout
 *     interrupt (cdr_timeout_irq) will be generated.
 *     The timeout value must be specified in 256 clock cycles
 *     of the EIP-202 HIA clock.
 *
 * This function is re-entrant for any device.
 *
 * Return value
 *     EIP202_RING_NO_ERROR : operation is completed
 *     EIP202_RING_ARGUMENT_ERROR : Passed wrong argument
 *     EIP202_RING_ILLEGAL_IN_STATE : invalid API state transition
 */
eip202_ring_error_t
eip202_cdr_fill_level_low_int_enable(
        eip202_ring_io_area_t * const io_area_p,
        const unsigned int threshold_dscr_count,
        const unsigned int timeout);


/*----------------------------------------------------------------------------
 * eip202_cdr_fill_level_low_int_clear_and_disable
 *
 * This function clears and disables the command Descriptor
 * threshold (cdr_thresh_irq) and timeout (cdr_timeout_irq) interrupts.
 * This function does not change the current API state.
 *
 * This function must be called as soon as the command Descriptor threshold or
 * timeout interrupt occurs. The occurrence of these interrupts can be detected
 * by means of the eip202_cdr_status_get() function, for example from an
 * Interrupt Service Routine.
 *
 * io_area_p (input)
 *     Pointer to the place holder in memory for the IO Area
 *     for the CDR instance which contains the device handle.
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
eip202_cdr_fill_level_low_int_clear_and_disable(
        eip202_ring_io_area_t * const io_area_p);


/*----------------------------------------------------------------------------
 * eip202_cdr_dump
 *
 */
void
eip202_cdr_dump(
        eip202_ring_io_area_t * const io_area_p,
        eip202_ring_admin_t * const ring_admin_p);


/* end of file eip202_cdr.h */


#endif /* EIP202_CDR_H_ */
