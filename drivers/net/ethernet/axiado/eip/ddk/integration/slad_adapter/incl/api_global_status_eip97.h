/* api_global_status_eip97.h
 *
 * Security-IP-97 Global Control Get status API
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

#ifndef API_GLOBAL_STATUS_EIP97_H_
#define API_GLOBAL_STATUS_EIP97_H_


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Driver Framework Basic Definitions API
#include "basic_defs.h"         // u8, u32, bool

// EIP-97 Driver Library API
#include "eip97_global_event.h" // Event Control
#include "eip97_global_prng.h"  // PRNG


/*----------------------------------------------------------------------------
 * Definitions and macros
 */

// GlobalControl97 error Codes
typedef enum
{
    GLOBAL_CONTROL_NO_ERROR = 0,
    GLOBAL_CONTROL_ERROR_BAD_PARAMETER,
    GLOBAL_CONTROL_ERROR_BAD_USE_ORDER,
    GLOBAL_CONTROL_ERROR_INTERNAL,
    GLOBAL_CONTROL_ERROR_NOT_IMPLEMENTED
} global_control97_error_t;

// EIP-97 data Fetch Engine (DFE) thread status,
// 1 DFE thread corresponds to 1 Processing Engine (PE)
typedef eip97_global_dfe_status_t global_control97_dfe_status_t;

// EIP-97 data Store Engine (DSE) thread status,
// 1 DSE thread corresponds to 1 Processing Engine (PE)
typedef eip97_global_dse_status_t global_control97_dse_status_t;

// EIP-96 Token status
typedef eip96_token_status_t global_control97_token_status_t;

// EIP-96 Context status
typedef eip96_context_status_t global_control97_context_status_t;

// EIP-96 Interrupt status
typedef eip96_interrupt_status_t global_control97_interrupt_status_t;

// EIP-96 Output Transfer status
typedef eip96_output_transfer_status_t global_control97_output_transfer_status_t;

// EIP-96 PRNG status
typedef eip96_prng_status_t global_control97_prng_status_t;

// EIP-96 PRNG Re-seed data
typedef eip96_prng_reseed_t             global_control97_prng_reseed_t;

// EIP-197 Debug statistics
typedef eip97_global_debug_statistics_t global_control97_debug_statistics_t;


/*----------------------------------------------------------------------------
 * global_control97_dfe_status_get
 *
 * This function retrieves the global status information
 * from the EIP-97 HIA DFE hardware.
 *
 * This function can detect the EIP-97 Fatal error condition requiring
 * the EIP-97 Global SW or HW Reset!
 *
 * pe_number (input)
 *     number of the EIP-97 Processing Engine for which the status information
 *     must be retrieved
 *
 * dfe_status_p (output)
 *     Pointer to the data structure where the DFE status will be stored
 *
 * This function is re-entrant.
 *
 * Return value
 *     GLOBAL_CONTROL_NO_ERROR : status retrieved successfully
 *     GLOBAL_CONTROL_ERROR_BAD_PARAMETER : invalid parameter
 */
global_control97_error_t
global_control97_dfe_status_get(
        const unsigned int pe_number,
        global_control97_dfe_status_t * const dfe_status_p);


/*----------------------------------------------------------------------------
 * global_control97_dse_status_get
 *
 * This function can detect the EIP-97 Fatal error condition requiring
 * the EIP-97 Global SW or HW Reset!
 *
 * This function can detect the EIP-97 Fatal error condition requiring
 * the EIP-97 Global SW or HW Reset!
 *
 * pe_number (input)
 *     number of the EIP-97 Processing Engine for which the status information
 *     must be retrieved
 *
 * dse_status_p (output)
 *     Pointer to the data structure where the DSE status will be stored
 *
 * This function is re-entrant.
 *
 * Return value
 *     GLOBAL_CONTROL_NO_ERROR : status retrieved successfully
 *     GLOBAL_CONTROL_ERROR_BAD_PARAMETER : invalid parameter
 */
global_control97_error_t
global_control97_dse_status_get(
        const unsigned int pe_number,
        global_control97_dse_status_t * const dse_status_p);


/*----------------------------------------------------------------------------
 * global_control97_token_status_get
 *
 * This function retrieves the EIP-96 Token status information
 * from the EIP-97 hardware (includes HIA DFE, HIA DSE and EIP-96 PE)
 *
 * pe_number (input)
 *     number of the EIP-97 Processing Engine for which the status information
 *     must be retrieved
 *
 * token_status_p (output)
 *     Pointer to the data structure where the EIP-96 Token status
 *     will be stored
 *
 * This function is re-entrant.
 *
 * Return value
 *     GLOBAL_CONTROL_NO_ERROR : status retrieved successfully
 *     GLOBAL_CONTROL_ERROR_BAD_PARAMETER : invalid parameter
 */
global_control97_error_t
global_control97_token_status_get(
        const unsigned int pe_number,
        global_control97_token_status_t * const token_status_p);


/*----------------------------------------------------------------------------
 * global_control97_context_status_get
 *
 * This function retrieves the EIP-96 Context status information
 * from the EIP-97 hardware (includes HIA DFE, HIA DSE and EIP-96 PE)
 *
 * This function can detect the EIP-97 Fatal error condition requiring
 * the EIP-97 Global SW or HW Reset!
 *
 * pe_number (input)
 *     number of the EIP-97 Processing Engine for which the status information
 *     must be retrieved
 *
 * context_status_p (output)
 *     Pointer to the data structure where the EIP-96 Context status
 *     will be stored
 *
 * This function is re-entrant.
 *
 * Return value
 *     GLOBAL_CONTROL_NO_ERROR : status retrieved successfully
 *     GLOBAL_CONTROL_ERROR_BAD_PARAMETER : invalid parameter
 */
global_control97_error_t
global_control97_context_status_get(
        const unsigned int pe_number,
        global_control97_context_status_t * const context_status_p);


/*----------------------------------------------------------------------------
 * global_control97_interrupt_status_get
 *
 * This function retrieves the EIP-96 Interrupt status information
 * from the EIP-97 hardware (includes HIA DFE, HIA DSE and EIP-96 PE)
 *
 * This function can detect the EIP-97 Fatal error condition requiring
 * the EIP-97 Global SW or HW Reset!
 *
 * pe_number (input)
 *     number of the EIP-97 Processing Engine for which the status information
 *     must be retrieved
 *
 * interrupt_status_p (output)
 *     Pointer to the data structure where the EIP-96 Interrupt status
 *     will be stored
 *
 * This function is re-entrant.
 *
 * Return value
 *     GLOBAL_CONTROL_NO_ERROR : status retrieved successfully
 *     GLOBAL_CONTROL_ERROR_BAD_PARAMETER : invalid parameter
 */
global_control97_error_t
global_control97_interrupt_status_get(
        const unsigned int pe_number,
        global_control97_interrupt_status_t * const interrupt_status_p);


/*----------------------------------------------------------------------------
 * global_control97_out_xfer_status_get
 *
 * This function retrieves the EIP-96 Output Transfer status information
 * from the EIP-97 hardware (includes HIA DFE, HIA DSE and EIP-96 PE)
 *
 * pe_number (input)
 *     number of the EIP-97 Processing Engine for which the status information
 *     must be retrieved
 *
 * out_xfer_status_p (output)
 *     Pointer to the data structure where the EIP-96 Output Transfer status
 *     will be stored
 *
 * This function is re-entrant.
 *
 * Return value
 *     GLOBAL_CONTROL_NO_ERROR : status retrieved successfully
 *     GLOBAL_CONTROL_ERROR_BAD_PARAMETER : invalid parameter
 */
global_control97_error_t
global_control97_out_xfer_status_get(
        const unsigned int pe_number,
        global_control97_output_transfer_status_t * const out_xfer_status_p);


/*----------------------------------------------------------------------------
 * global_control97_prng_status_get
 *
 * This function retrieves the EIP-96 PRNG status information
 * from the EIP-97 hardware (includes HIA DFE, HIA DSE and EIP-96 PE)
 *
 * pe_number (input)
 *     number of the EIP-97 Processing Engine for which the status information
 *     must be retrieved
 *
 * prng_status_p (output)
 *     Pointer to the data structure where the EIP-96 PRNG status
 *     will be stored
 *
 * This function is re-entrant.
 *
 * Return value
 *     GLOBAL_CONTROL_NO_ERROR : status retrieved successfully
 *     GLOBAL_CONTROL_ERROR_BAD_PARAMETER : invalid parameter
 */
global_control97_error_t
global_control97_prng_status_get(
        const unsigned int pe_number,
        global_control97_prng_status_t * const prng_status_p);

/*----------------------------------------------------------------------------
 * global_control97_prng_reseed
 *
 * This function performs the Packet Engine re-seed operation
 *
 * io_area_p (input)
 *     Pointer to the place holder in memory for the IO Area.
 *
 * pe_number (input)
 *     number of the PE that must be re-seed.
 *
 * reseed_data_p (input)
 *     Pointer to the PRNG seed and key data.
 *
 * This function is not re-entrant.
 *
 * Return value
 *     GLOBAL_CONTROL_NO_ERROR : re-seed done successfully
 *     GLOBAL_CONTROL_ERROR_INTERNAL : operation failed
 *     GLOBAL_CONTROL_ERROR_BAD_PARAMETER : invalid parameter
 */
global_control97_error_t
global_control97_prng_reseed(
        const unsigned int pe_number,
        const global_control97_prng_reseed_t * const reseed_data_p);


/*----------------------------------------------------------------------------
 * global_control97_debug_statistics_get
 *
 * This function retrieves the debug statistics
 * from the EIP-197
 *
 * debug_statistics_p (output)
 *     Pointer to the data structure where the debug statistics will be stored
 *
 * This function is re-entrant.
 *
 * Return value
 *     GLOBAL_CONTROL_NO_ERROR : status retrieved successfully
 *     GLOBAL_CONTROL_ERROR_BAD_PARAMETER : invalid parameter
 */
global_control97_error_t
global_control97_debug_statistics_get(
        global_control97_debug_statistics_t * const debug_statistics_p);


/*----------------------------------------------------------------------------
 * global_control97_interfaces_get
 *
 * Read the number of available packet interfaces.
 *
 * nof_p_es_p (output)
 *    number of processing engines.
 * nof_rings_p (output)
 *    number of available ring pairs.
 * nof_la_interfaces_p (output)
 *    number of available Look-Aside fifo interfaces.
 * nof_inline_interfaces_p (output)
 *    number of available Inline interfaces.
 *
 * If all returned values are zero, the device has not been initialized.
 * This is considered an error.
 */
void
global_control97_interfaces_get(
    unsigned int * const nof_p_es_p,
    unsigned int * const nof_rings_p,
    unsigned int * const nof_la_interfaces_p,
    unsigned int * const nof_inline_interfaces_p);


#endif /* API_GLOBAL_STATUS_EIP97_H_ */


/* end of file api_global_status_eip97.h */
