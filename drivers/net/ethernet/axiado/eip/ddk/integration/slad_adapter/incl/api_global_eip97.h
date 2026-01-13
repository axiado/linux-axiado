/* api_global_eip97.h
 *
 * Security-IP-97 Global Control API
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

#ifndef API_GLOBAL_EIP97_H_
#define API_GLOBAL_EIP97_H_

// The status part of the API
#include "api_global_status_eip97.h"


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Driver Framework Basic Definitions API
#include "basic_defs.h"         // u8, u32, bool

// EIP-97 Driver Library Global API
#include "eip97_global_init.h"  // Initialization
#include "eip97_global_prng.h"  // PRNG


/*----------------------------------------------------------------------------
 * Definitions and macros
 */

#define GLOBAL_CONTROL_MAXLEN_TEXT      128

// Ring PE assignment map
typedef eip97_global_ring_pe_map_t      global_control97_ring_pe_map_t;

// zero-terminated descriptive text of the available services.
typedef struct
{
    char sz_text_description[GLOBAL_CONTROL_MAXLEN_TEXT];
} global_control97_capabilities_t;


/*----------------------------------------------------------------------------
 * global_control97_capabilities_get
 *
 * This routine returns a structure that describes the capabilities of the
 * implementation. See description of global_control97_capabilities_t
 * for details.
 *
 * capabilities_p
 *     Pointer to the capabilities structure to fill in.
 *
 * This function is re-entrant.
 */
global_control97_error_t
global_control97_capabilities_get(
        global_control97_capabilities_t * const capabilities_p);


/*----------------------------------------------------------------------------
 * global_control97_init
 *
 * This function performs the initialization of the EIP-97 Global Control
 * functionality.
 *
 * f_hw_reset_done (input)
 *     Flag indicates whether the HW Reset operation was performed
 *     for the Security-IP-97 hardware before calling this function
 *
 * This function is not re-entrant.
 *
 * Return value
 *     GLOBAL_CONTROL_NO_ERROR : initialization performed successfully
 *     GLOBAL_CONTROL_ERROR_INTERNAL : initialization failed
 *     GLOBAL_CONTROL_ERROR_BAD_USE_ORDER : initialization is already done
 */
global_control97_error_t
global_control97_init(
        const bool f_hw_reset_done);


/*----------------------------------------------------------------------------
 * global_control97_uninit
 *
 * This function performs the initialization of the EIP-97 Global Control
 * functionality.
 *
 * This function is not re-entrant.
 *
 * Return value
 *     GLOBAL_CONTROL_NO_ERROR : un-initialization performed successfully
 *     GLOBAL_CONTROL_ERROR_INTERNAL : un-initialization failed
 *     GLOBAL_CONTROL_ERROR_BAD_USE_ORDER : un-initialization is already done
 */
global_control97_error_t
global_control97_uninit(void);


/*----------------------------------------------------------------------------
 * global_control97_configure
 *
 * This function performs the Ring to PE assignment and configures
 * the Ring priority. The EIP-97 device supports multiple Ring interfaces
 * as well as multiple PE's. One ring can be assigned to the same or different
 * PE's. Multiple rings can be assigned to the same PE.
 *
 * pe_number (input)
 *     number of the EIP-97 Processing Engine for which the status information
 *     must be retrieved
 *
 * ring_pe_map_p (input)
 *     Pointer to the data structure that contains the Ring PE assignment map.
 *
 * This function is not re-entrant.
 *
 * Return value
 *     GLOBAL_CONTROL_NO_ERROR : PE configured successfully
 *     GLOBAL_CONTROL_ERROR_INTERNAL : operation failed
 *     GLOBAL_CONTROL_ERROR_BAD_PARAMETER : invalid parameter
 */
global_control97_error_t
global_control97_configure(
        const unsigned int pe_number,
        const global_control97_ring_pe_map_t * const ring_pe_map_p);


#endif /* API_GLOBAL_EIP97_H_ */


/* end of file api_global_eip97.h */
