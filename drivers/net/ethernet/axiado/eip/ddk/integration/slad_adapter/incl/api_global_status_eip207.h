/* api_global_status_eip207.h
 *
 * Classification (EIP-207) Global Control status API
 *
 */

/*****************************************************************************
* Copyright (c) 2011-2022 by Rambus, Inc. and/or its subsidiaries.
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

#ifndef API_GLOBAL_STATUS_EIP207_H_
#define API_GLOBAL_STATUS_EIP207_H_


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Driver Framework Basic Definitions API
#include "basic_defs.h"         // u32, bool

// EIP-207 Driver Library Global Classification Control API
#include "eip207_global_init.h"

/*----------------------------------------------------------------------------
 * Definitions and macros
 */

// GlobalControl207 error Codes
typedef enum
{
    EIP207_GLOBAL_CONTROL_NO_ERROR = 0,
    EIP207_GLOBAL_CONTROL_ERROR_BAD_PARAMETER,
    EIP207_GLOBAL_CONTROL_ERROR_BAD_USE_ORDER,
    EIP207_GLOBAL_CONTROL_ERROR_INTERNAL,
    EIP207_GLOBAL_CONTROL_ERROR_NOT_IMPLEMENTED
} global_control207_error_t;

// EIP-207s frc/trc/arc4_rc status information
typedef struct
{
    // True when a Host bus master read access from this cache
    // resulted in an error.
    bool f_dma_read_error;

    // True when a Host bus master write access from this cache
    // resulted in an error.
    bool f_dma_write_error;

} global_control207_cache_status_t;

// Classification Engine status information
typedef eip207_global_status_t global_control207_status_t;

// The Classification hardware global statistics
typedef eip207_global_global_stats_t global_control207_global_stats_t;

// The Classification Engine clock count
typedef eip207_global_clock_t global_control207_clock_t;

// The Classification Engine firmware configuration.
typedef eip207_firmware_config_t global_control_firmware_config_t;


/*----------------------------------------------------------------------------
 * global_control207_status_get
 *
 * This function retrieves the global status information
 * from the EIP-207 Classification Engine hardware.
 *
 * This function can detect the EIP-207 Fatal error condition requiring
 * the Global SW or HW Reset!
 *
 * ce_number (input)
 *     number of the EIP-207 Classification Engine for which the status
 *     information must be retrieved
 *
 * status_p (output)
 *     Pointer to the data structure where the engine status will be stored
 *
 * This function is re-entrant.
 *
 * Return value
 *     EIP207_GLOBAL_CONTROL_NO_ERROR : status retrieved successfully
 *     EIP207_GLOBAL_CONTROL_ERROR_BAD_PARAMETER : invalid parameter
 */
global_control207_error_t
global_control207_status_get(
        const unsigned int ce_number,
        global_control207_status_t * const status_p);


/*----------------------------------------------------------------------------
 * global_control207_global_stats_get
 *
 * This function obtains global statistics for the Classification Engine.
 *
 * ce_number (input)
 *     number of the EIP-207 Classification Engine for which the global
 *     statistics must be retrieved
 *
 * global_stats_p (output)
 *     Pointer to the data structure where the global statistics will be stored.
 *
 * Return value
 *     EIP207_GLOBAL_CONTROL_NO_ERROR : statistics retrieved successfully
 *     EIP207_GLOBAL_CONTROL_ERROR_BAD_PARAMETER : invalid parameter
 */
global_control207_error_t
global_control207_global_stats_get(
        const unsigned int ce_number,
        global_control207_global_stats_t * const global_stats_p);


/*--------------------- -------------------------------------------------------
 * global_control207_clock_count_get
 *
 * Retrieve the current clock count as used by the Classification Engine.
 *
 * ce_number (input)
 *     number of the EIP-207 Classification Engine for which the clock
 *     count must be retrieved
 *
 * clock_p (output)
 *     Pointer to the data structure where the current clock count used by
 *     the Classification Engine will be stored.
 *
 * Return value
 *     EIP207_GLOBAL_CONTROL_NO_ERROR : status retrieved successfully
 *     EIP207_GLOBAL_CONTROL_ERROR_BAD_PARAMETER : invalid parameter
 */
global_control207_error_t
global_control207_clock_count_get(
        const unsigned int ce_number,
        global_control207_clock_t * const clock_p);


/*--------------------- -------------------------------------------------------
 * global_control207_firmware_configure
 *
 * This function configures firmware settings.
 *
 * fw_config_p (input)
 *     configuration parameters for the firmware.
 *
 * Return value
 *     EIP207_GLOBAL_CONTROL_NO_ERROR : status retrieved successfully
 *     EIP207_GLOBAL_CONTROL_ERROR_BAD_PARAMETER : invalid parameter
 */
global_control207_error_t
global_control207_firmware_configure(
        global_control_firmware_config_t * const fw_config_p);


#endif /* API_GLOBAL_STATUS_EIP207_H_ */


/* end of file api_global_status_eip207.h */
