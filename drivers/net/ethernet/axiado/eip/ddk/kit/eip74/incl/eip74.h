/* eip74.h
 *
 * EIP-74 Driver Library API:
 *
 */

/*****************************************************************************
* Copyright (c) 2017-2020 by Rambus, Inc. and/or its subsidiaries.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are
* met:
*
* 1. Redistributions of source code must retain the above copyright
* notice, this list of conditions and the following disclaimer.
*
* 2. Redistributions in binary form must reproduce the above copyright
* notice, this list of conditions and the following disclaimer in the
* documentation and/or other materials provided with the distribution.
*
* 3. Neither the name of the copyright holder nor the names of its
* contributors may be used to endorse or promote products derived from
* this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
* A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
* HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
* THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*****************************************************************************/

#ifndef EIP74_H_
#define EIP74_H_


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Driver Framework Basic Definitions API
#include "basic_defs.h"         // u8, u32, bool

// Driver Framework device API
#include "device_types.h"       // device_handle_t

/*----------------------------------------------------------------------------
 * Example API Use Order:
 *
 * 1 Call eip74_init to initialize the io_area of the Driver Library.
 *
 * 2 Call eip74_configure to set the configuration parameters of the device:
 *   the generate block size and the reseed thresholds. This function can be
 *   called anywhere between step 1 and step 5. Typically these configuration
 *   parameters are set only once.
 *
 * 3 Call eip74_reset to reset the device. It it returns EIP74_NO_ERROR
 *   go to step 5, if it returns EIP74_BUSY_RETRY_LATER, go to step 4.
 *
 * 4 Call EIP74_Reset_Isdone until the result is EIP74_NO_ERROR.
 *
 * 5 Call eip74_instantiate. When this function returns, the device is
 *   operational.
 *
 * Steps 3 through 5 can be repeated to re-instantiate the DRBG, but typically
 * these steps are performed only once,
 *
 * 6 Periodically, or upon receiving a device interrupt, check the
 *   status of the device and take necessary actions.
 *
 * 6.1 call eip74_status_get to find out what events occurred.
 *
 * 6.2 If a stuck-output fault occurred, call eip74_clear to clear the fault
 *     condition. Stuck-output faults are very rare but not impossible during
 *     normal operation.
 *
 * 6.3 If a reseed warning occurred, call eip74_reseed
 *
 * 7 Optionally call eip74_reset to stop operation of the device.
 */

/*----------------------------------------------------------------------------
 * Definitions and macros
 */

#define EIP74_IOAREA_REQUIRED_SIZE       (2 * sizeof(void*))

/*----------------------------------------------------------------------------
 * eip74_error_t
 *
 * status (error) code type returned by these API functions
 * See each function "Return value" for details.
 *
 * EIP74_NO_ERROR : successful completion of the call.
 * EIP74_UNSUPPORTED_FEATURE_ERROR : not supported by the device.
 * EIP74_ARGUMENT_ERROR :  invalid argument for a function parameter.
 * EIP74_BUSY_RETRY_LATER : device is busy.
 * EIP74_ILLEGAL_IN_STATE : illegal state transition
 */
typedef enum
{
    EIP74_NO_ERROR = 0,
    EIP74_UNSUPPORTED_FEATURE_ERROR,
    EIP74_ARGUMENT_ERROR,
    EIP74_BUSY_RETRY_LATER,
    EIP74_ILLEGAL_IN_STATE
} eip74_error_t;

// Generic EIP HW version
typedef struct
{
    // The basic EIP number.
    u8 eip_number;

    // The complement of the basic EIP number.
    u8 complmt_eip_number;

    // Hardware Patch Level.
    u8 hw_patch_level;

    // minor Hardware revision.
    u8 min_hw_revision;

    // major Hardware revision.
    u8 maj_hw_revision;
} eip_version_t;

// EIP-74 HW options
typedef struct
{
    // number of client interfaces
    u8 client_count;

    // number of AES cores
    u8 aes_core_count;

    // Speed grade of AES core
    u8 aes_speed;

    // Depth of output fifo
    u8 fifo_depth;
} eip74_options_t;


// EIP-74 Hardware capabilities.
typedef struct
{
    eip_version_t hw_revision;
    eip74_options_t hw_options;
} eip74_capabilities_t;

// configuration parameters for EIP-74.
typedef struct
{
    // number of IVs generated for each Generate operation.
    unsigned int generate_block_size;

    // number of Generate operations after which it is an error to continue
    // without reseed
    unsigned int reseed_thr;

    // number of Generate operations after which to notify that reseed is
    // required
    unsigned int reseed_thr_early;
} eip74_configuration_t;


// status information of the EIP-74.
typedef struct
{
    // number of generate operations since last initialize or reseed
    unsigned int generate_block_count;

    // Stuck-output fault detected
    bool f_stuck_out;

    // Not-initialized error detected
    bool f_not_initialized;

    // Reseed error  detected, generate_block_count passed threshold
    bool f_reseed_error;

    // Reseed warning detected, generate_block_count passed early threshold
    bool f_reseed_warning;

    // DRBG was instantiated successfully
    bool f_instantiated;

    // number of IVs available
    unsigned int available_count;
} eip74_status_t;


// place holder for device specific internal data
typedef struct
{
    u32 placeholder[EIP74_IOAREA_REQUIRED_SIZE];
} eip74_io_area_t;


/*----------------------------------------------------------------------------
 * eip74_init
 *
 * This function performs the initialization of the EIP-74 HW
 * interface and transits the API to the Initialized state.
 *
 * This function returns the EIP74_UNSUPPORTED_FEATURE_ERROR error code
 * when it detects a mismatch in the Driver Library configuration
 * and the EIP-74 HW revision or configuration.
 *
 * Note: This function should be called first.
 *
 * io_area_p (output)
 *     Pointer to the place holder in memory for the IO Area.
 *
 * device (input)
 *     handle for the Global Control device instance returned by eip_device_find.
 *
 * Return value
 *     EIP74_NO_ERROR : operation is completed
 *     EIP74_UNSUPPORTED_FEATURE_ERROR : not supported by the device.
 *     EIP74_ARGUMENT_ERROR : Passed wrong argument
 *     EIP74_ILLEGAL_IN_STATE : invalid API state transition
 */
eip74_error_t
eip74_init(
        eip74_io_area_t * const io_area_p,
        const device_handle_t device);


/*----------------------------------------------------------------------------
 * eip74_reset
 *
 * This function starts the EIP-74 Reset operation. If the reset operation
 * can be done immediately this function returns EIP74_NO_ERROR.
 * Otherwise it will return EIP74_BUSY_RETRY_LATER indicating
 * that the reset operation has been started and is ongoing.
 * The eip74_reset_is_done() function can be used to poll the device
 * for the completion of the reset operation.
 *
 * Note: This function must be called and the operation must be complete
 *       before calling the eip74_instantiate() function
 * Note: This function can be used to terminate operation of the DRBG. This
 *       will also clear the internal state of the hardware.
 *
 * io_area_p (output)
 *     Pointer to the place holder in memory for the IO Area.
 *
 * This function can be called once the driver library is initialized.
 *
 * This function is NOT re-entrant. It can be called concurrently with
 * eip74_hw_revision_get, eip74_configure, EIP174_Status_Get and eip74_clear.
 *
 * Return value
 *     EIP74_NO_ERROR : Global SW Reset is done
 *     EIP74_UNSUPPORTED_FEATURE_ERROR : not supported by the device.
 *     EIP74_BUSY_RETRY_LATER: Global SW Reset is started but
 *                                    not completed yet
 *     EIP74_ARGUMENT_ERROR : Passed wrong argument
 *     EIP74_ILLEGAL_IN_STATE : invalid API state transition
 */
eip74_error_t
eip74_reset(
        eip74_io_area_t * const io_area_p);


/*----------------------------------------------------------------------------
 * eip74_reset_is_done
 *
 * This function checks the status of the started by the eip74_reset()
 * function for the EIP-74 device.
 *
 * io_area_p (input)
 *     Pointer to the place holder in memory for the IO Area.
 *
 * This function can be called after a call to eip74_reset and before any
 * subsequent call to eip74_instantiate
 *
 * This function is NOT re-entrant. It can be called concurrently with
 * eip74_hw_revision_get, eip74_configure, EIP174_Status_Get and eip74_clear.
 *
 * Return value
 *     EIP74_NO_ERROR : Global SW Reset operation is done
 *     EIP74_BUSY_RETRY_LATER: Global SW Reset is started but
 *                                    not completed yet
 *     EIP74_ARGUMENT_ERROR : Passed wrong argument
 *     EIP74_ILLEGAL_IN_STATE : invalid API state transition
 */
eip74_error_t
eip74_reset_is_done(
        eip74_io_area_t * const io_area_p);


/*----------------------------------------------------------------------------
 * eip74_hw_revision_get
 *
 * This function returns EIP-74 hardware revision
 * information in the capabilities_p data structure.
 *
 * device (input)
 *     handle of the device to be used.
 *
 * capabilities_p (output)
 *     Pointer to the place holder in memory where the device capability
 *     information will be stored.
 *
 * This function can be called at all times, even when the device is
 * not initialized.
 *
 * This function is re-entrant and can be called
 * concurrently with other driver library functions..
 *
 * Return value
 *     EIP74_NO_ERROR : operation is completed
 *     EIP74_ARGUMENT_ERROR : Passed wrong argument
 */
eip74_error_t
eip74_hw_revision_get(
        const device_handle_t device,
        eip74_capabilities_t * const capabilities_p);


/*----------------------------------------------------------------------------
 * eip74_configure
 *
 * This function configures the block size and the reseed thresholds of the
 * EIP-74 hardware. If called, it should be called before the first
 * call to eip74_reset or between eip74_reset and eip74_instantiate.
 *
 * io_area_p (input)
 *     Pointer to the place holder in memory for the IO Area.
 *
 * configuration_p (input)
 *     Pointer to the data structure that contains the EIP74 configuration
 *
 * This function can be called once the driver library is initialized.
 *
 * This function is NOT re-entrant. It can be called concurrently
 * with other driver library functions.
 *
 * Return value
 *     EIP74_NO_ERROR : operation is completed
 *     EIP74_ARGUMENT_ERROR : Passed wrong argument
 *     EIP74_ILLEGAL_IN_STATE : invalid API state transition
 */
eip74_error_t
eip74_configure(
        eip74_io_area_t * const io_area_p,
        const eip74_configuration_t * const configuration_p);


/*----------------------------------------------------------------------------
 * eip74_instantiate
 *
 * This function instantiates the AES-256-CTR SP800-90 DRBG on the EIP-74
 * hardware. It can be called after a reset operation that is completed.
 * After instantiation, the EIP-74 will generate pseudo-random data blocks
 * of the configured size autonomously.
 *
 * io_area_p (input)
 *     Pointer to the place holder in memory for the IO Area.
 *
 * entropy_p (input)
 *     Pointer to a string of exactly 12 32-bit worfs that serves as the
 *     entropy to instantiate the DRBG.
 *
 * f_stuck_out (input)
 *     Enable stuck-output detection (next output is the same as previous)
 *     during iv generation.
 *
 * This function can be called after eip74_reset or eip74_reset_is_done when
 * such function returns EIP74_NO_ERROR.
 *
 * This function is NOT re-entrant. It can be called concurrently with
 * eip74_hw_revision_get, eip74_configure, EIP174_Status_Get and eip74_clear.
 *
 * Return value
 *     EIP74_NO_ERROR : operation is completed
 *     EIP74_ARGUMENT_ERROR : Passed wrong argument
 *     EIP74_ILLEGAL_IN_STATE : invalid API state transition
 */
eip74_error_t
eip74_instantiate(
        eip74_io_area_t * const io_area_p,
        const u32 * const entropy_p,
        bool f_stuck_out);


/*----------------------------------------------------------------------------
 * eip74_reseed
 *
 * This function initiates a reseed operation of the DRBG. The reseed
 * will take effect when the next Generate operation is started.
 *
 * io_area_p (input)
 *     Pointer to the place holder in memory for the IO Area.
 *
 * entropy_p (input)
 *     Pointer to a string of exactly 12 32-bit worfs that serves as the
 *     entropy to reseed the DRBG.
 *
 * This function can be called after EIP7_Instantiate and before any
 * subsequent call to eip74_reset.
 *
 * This function is NOT re-entrant. It can be called concurrently with
 * eip74_hw_revision_get, eip74_configure, EIP174_Status_Get and eip74_clear.
 *
 * Return value
 *     EIP74_NO_ERROR : operation is completed
 *     EIP74_ARGUMENT_ERROR : Passed wrong argument
 *     EIP74_ILLEGAL_IN_STATE : invalid API state transition
 */
eip74_error_t
eip74_reseed(
        eip74_io_area_t * const io_area_p,
        const u32 * const entropy_p);


/*----------------------------------------------------------------------------
 * eip74_status_get
 *
 * This function reads the status of the EIP-74.
 *
 * io_area_p (input)
 *     Pointer to the place holder in memory for the IO Area.
 *
 * status_p (output)
 *     status information obtained from the EIP-74.
 *
 * This function can be called once the driver library is initialized.
 *
 * This function is re-entrant and can be called
 * concurrently with other driver library functions.
 *
 * Return value
 *     EIP74_NO_ERROR : operation is completed
 *     EIP74_ARGUMENT_ERROR : Passed wrong argument
 *     EIP74_ILLEGAL_IN_STATE : invalid API state transition
 */
eip74_error_t
eip74_status_get(
        eip74_io_area_t * const io_area_p,
        eip74_status_t * const status_p);


/*----------------------------------------------------------------------------
 * eip74_clear
 *
 * This function clears any stuck-output fault condition that may have
 * been detected by the hardware.
 *
 * io_area_p (input)
 *     Pointer to the place holder in memory for the IO Area.
 *
 * This function can be called once the driver library is initialized.
 *
 * This function is re-entrant and can be called
 * concurrently with other driver library functions..
 *
 * Return value
 *     EIP74_NO_ERROR : operation is completed
 *     EIP74_ARGUMENT_ERROR : Passed wrong argument
 *     EIP74_ILLEGAL_IN_STATE : invalid API state transition
 */
eip74_error_t
eip74_clear(
        eip74_io_area_t * const io_area_p);


#endif /* EIP74_H_ */


/* end of file eip74.h */
