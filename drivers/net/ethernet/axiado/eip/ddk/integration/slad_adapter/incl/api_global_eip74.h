/* api_global_eip74.h
 *
 * Deterministic Random Bit Generator (EIP-74) Global Control Initialization
 * API. The EIP-74 is used to generate pseudo-random IVs for outbound
 * operations in CBC mode.
 *
 * This API is not re-entrant.
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

#ifndef API_GLOBAL_EIP74_H_
#define API_GLOBAL_EIP74_H_


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Driver Framework Basic Definitions API
#include "basic_defs.h"         // bool, u8


/*----------------------------------------------------------------------------
 * Definitions and macros
 */

// GlobalControl74 error Codes
typedef enum
{
    GLOBAL_CONTROL_EIP74_NO_ERROR = 0,
    GLOBAL_CONTROL_EIP74_ERROR_BAD_PARAMETER,
    GLOBAL_CONTROL_EIP74_ERROR_BAD_USE_ORDER,
    GLOBAL_CONTROL_EIP74_ERROR_INTERNAL,
    GLOBAL_CONTROL_EIP74_ERROR_NOT_IMPLEMENTED
} global_control74_error_t;


#define GLOBAL_CONTROL_EIP74_MAXLEN_TEXT           128

// zero-terminated descriptive text of the available services.
typedef struct
{
    char sz_text_description[GLOBAL_CONTROL_EIP74_MAXLEN_TEXT];
} global_control74_capabilities_t;


// configuration parameters for EIP-74.
typedef struct
{
    // number of IVs generated for each Generate operation.
    // value can be set to zero to request default value.
    unsigned int generate_block_size;

    // number of Generate operations after which it is an error to continue
    // without reseed
    // value can be set to zero to request default value.
    unsigned int reseed_thr;

    // number of Generate operations after which to notify that reseed is
    // required
    // value can be set to zero to request default value.
    unsigned int reseed_thr_early;

    // Detect stuck-out faults
    bool f_stuck_out;
} global_control74_configuration_t;


// status information of the EIP-74.
typedef struct
{
    // number of generate operations since last initialize or reseed
    unsigned int generate_block_count;

    // Stuck-out fault detected
    bool f_stuck_out;

    // Not-initialized error detected
    bool f_not_initialized;

    // Reseed error  detected, generate_block_count passed threshold
    bool f_reseed_error;

    // Reseed warning detected, generate_block_count passed early  threshold
    bool f_reseed_warning;

    // DRBG was instantiated successfully
    bool f_instantiated;

    // number of IVs available
    unsigned int available_count;
} global_control74_status_t;


/*----------------------------------------------------------------------------
 * global_control74_notify_function_t
 *
 * This type specifies the callback function prototype for the function
 * global_control74_notify_request. The notification will occur only once.
 *
 * NOTE: The exact context in which the callback function is invoked and the
 *       allowed actions in that callback are implementation specific. The
 *       intention is that all API functions can be used, except
 *       global_control74_uninit.
 */
typedef void (* global_control74_notify_function_t)(void);


/*----------------------------------------------------------------------------
 * global_control74_capabilities_get
 *
 * This functions retrieves info about the capabilities of the
 * implementation.
 *
 * capabilities_p
 *     Pointer to the capabilities structure to fill in.
 *
 * Return value
 *     None
 */
void
global_control74_capabilities_get(
        global_control74_capabilities_t * const capabilities_p);


/*----------------------------------------------------------------------------
 * global_control74_init
 *
 * This function performs the initialization of the EIP-74 Deterministic
 * Random Bit Generator.
 *
 * configuration_p (input)
 *     configuration parameters of the DRBG.
 *
 * entropy_p (input)
 *     Pointer to a string of exactly 48 bytes that serves as the entropy.
 *     to initialize the DRBG.
 *
 * Return value
 *     GLOBAL_CONTROL_EIP74_NO_ERROR : initialization performed successfully
 *     GLOBAL_CONTROL_EIP74_ERROR_BAD_PARAMETER : Invalid parameters supplied
 *     GLOBAL_CONTROL_EIP74_ERROR_INTERNAL : initialization failed
 *     GLOBAL_CONTROL_EIP74_ERROR_BAD_USE_ORDER : initialization is already
 *                                                done
 */
global_control74_error_t
global_control74_init(
        const global_control74_configuration_t * const configuration_p,
        const u8 * const entropy_p);


/*----------------------------------------------------------------------------
 * global_control74_uninit
 *
 * This function performs the un-initialization of the EIP-74 Deterministic
 * Random Bit Generator.
 *
 * Return value
 *     GLOBAL_CONTROL_EIP74_NO_ERROR : un-initialization performed successfully
 *     GLOBAL_CONTROL_EIP74_ERROR_INTERNAL : un-initialization failed
 *     GLOBAL_CONTROL_EIP74_ERROR_BAD_USE_ORDER : un-initialization is already
 *                                                done
 */
global_control74_error_t
global_control74_uninit(void);


/*----------------------------------------------------------------------------
 * global_control74_reseed
 *
 * This function performs a reseed of the EIP-74 Deterministic
 * Random Bit Generator.
 *
 * entropy_p (input)
 *     Pointer to a string of exactly 48 bytes that serves as the entropy.
 *     to reseed the DRBG.
 *
 * Return value
 *     GLOBAL_CONTROL_EIP74_NO_ERROR : operation performed successfully
 *     GLOBAL_CONTROL_EIP74_ERROR_BAD_PARAMETER : Invalid parameter supplied
 *     GLOBAL_CONTROL_EIP74_ERROR_INTERNAL : operation failed
 *     GLOBAL_CONTROL_EIP74_ERROR_BAD_USE_ORDER : device not initialized
 */
global_control74_error_t
global_control74_reseed(
        const u8 * const entropy_p);


/*----------------------------------------------------------------------------
 * global_control74_status_get
 *
 * This function reads the status of the EIP-74 Deterministic Random
 * Bit Generator.
 *
 * status_p (output)
 *     status information obtained from the EIP-74.
 *
 * Return value
 *     GLOBAL_CONTROL_EIP74_NO_ERROR : operation performed successfully
 *     GLOBAL_CONTROL_EIP74_ERROR_BAD_PARAMETER : Invalid parameter supplied
 *     GLOBAL_CONTROL_EIP74_ERROR_INTERNAL : operation failed
 *     GLOBAL_CONTROL_EIP74_ERROR_BAD_USE_ORDER : device not initialized
 */
global_control74_error_t
global_control74_status_get(
        global_control74_status_t * const status_p);


/*----------------------------------------------------------------------------
 * global_control74_clear
 *
 * This function clears any stuck-out condition of the EIP-74 Deterministic
 * Random Bit Generator.
 *
 * Return value
 *     GLOBAL_CONTROL_EIP74_NO_ERROR : operation performed successfully
 *     GLOBAL_CONTROL_EIP74_ERROR_INTERNAL : operation failed
 *     GLOBAL_CONTROL_EIP74_ERROR_BAD_USE_ORDER : device not initialized
 */
global_control74_error_t
global_control74_clear(void);


/*----------------------------------------------------------------------------
 * global_control74_notify_request
 *
 * This routine can be used to request a one-time notification of
 * EIP-74 related events. When any error, fault or warning condition
 * occurs, the implementation will invoke the callback once to notify
 * the user. The callback function can then call
 * global_control74_status_get to find out what event occurred and it
 * can take any actions to rectify the situation, to log the event or
 * to stop processing. The callback can call this function again to
 * request future notifications of future events.
 *
 * cb_func_p (input)
 *     address of the callback function.
 *
 * Return value
 *     GLOBAL_CONTROL_EIP74_NO_ERROR : operation performed successfully
 *     GLOBAL_CONTROL_EIP74_ERROR_BAD_PARAMETER : Invalid parameter supplied
 *     GLOBAL_CONTROL_EIP74_ERROR_INTERNAL : operation failed
 *     GLOBAL_CONTROL_EIP74_ERROR_BAD_USE_ORDER : device not initialized
 */
global_control74_error_t
global_control74_notify_request(
        global_control74_notify_function_t cb_func_p);


#endif /* API_GLOBAL_EIP74_H_ */


/* end of file api_global_eip74.h */
