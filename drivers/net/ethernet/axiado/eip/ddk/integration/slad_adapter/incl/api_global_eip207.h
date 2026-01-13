/* api_global_eip207.h
 *
 * Classification (EIP-207) Global Control Initialization API
 *
 * This API is not re-entrant.
 */

/*****************************************************************************
* Copyright (c) 2011-2020 by Rambus, Inc. and/or its subsidiaries.
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

#ifndef API_GLOBAL_EIP207_H_
#define API_GLOBAL_EIP207_H_


// The status part of the API
#include "api_global_status_eip207.h"


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Driver Framework Basic Definitions API
#include "basic_defs.h"         // u32, bool


/*----------------------------------------------------------------------------
 * Definitions and macros
 */

#define EIP207_GLOBAL_CONTROL_MAXLEN_TEXT           128

// iv to use for the Flow Hash ID calculations by the Classification hardware
typedef struct
{
    u32 iv[4]; // must be written with a true random value
} global_control207_iv_t;

// zero-terminated descriptive text of the available services.
typedef struct
{
    char sz_text_description[EIP207_GLOBAL_CONTROL_MAXLEN_TEXT];
} global_control207_capabilities_t;


/*----------------------------------------------------------------------------
 * global_control207_capabilities_get
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
global_control207_capabilities_get(
        global_control207_capabilities_t * const capabilities_p);


/*----------------------------------------------------------------------------
 * global_control207_init
 *
 * This function performs the initialization of the EIP-207 Classification
 * Engine.
 *
 * f_load_firmware (input)
 *     Flag indicates whether the EIP-207 Classification Engine firmware
 *     must be loaded
 *
 * iv_p (input)
 *     Pointer to the Initialization Vector data.
 *
 * Return value
 *     EIP207_GLOBAL_CONTROL_NO_ERROR : initialization performed successfully
 *     EIP207_GLOBAL_CONTROL_ERROR_INTERNAL : initialization failed
 *     EIP207_GLOBAL_CONTROL_ERROR_BAD_USE_ORDER : initialization is already
 *                                                 done
 */
global_control207_error_t
global_control207_init(
        const bool f_load_firmware,
        const global_control207_iv_t * const iv_p);


/*----------------------------------------------------------------------------
 * global_control207_uninit
 *
 * This function performs the un-initialization of the EIP-207 Classification
 * Engine.
 *
 * Return value
 *     EIP207_GLOBAL_CONTROL_NO_ERROR : un-initialization performed successfully
 *     EIP207_GLOBAL_CONTROL_ERROR_INTERNAL : un-initialization failed
 *     EIP207_GLOBAL_CONTROL_ERROR_BAD_USE_ORDER : un-initialization is already
 *                                                 done
 */
global_control207_error_t
global_control207_uninit(void);


#endif /* API_GLOBAL_EIP207_H_ */


/* end of file api_global_eip207.h */
