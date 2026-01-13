/* sa_builder_basic.h
 *
 * Basic Crypto/hash specific functions of the SA Builder.
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


#ifndef  SA_BUILDER_BASIC_H_
#define SA_BUILDER_BASIC_H_


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

#include "sa_builder_params.h"
#include "sa_builder.h"
#include "sa_builder_params_basic.h"

// Driver Framework Basic Definitions API
#include "basic_defs.h"


/*----------------------------------------------------------------------------
 * sa_builder_init_basic
 *
 * This function initializes the sa_builder_params_t data structure and
 * its sa_builder_params_basic_t extension with sensible defaults for
 * basic crypto and hash processing..
 *
 * sa_params_p (output)
 *   Pointer to SA parameter structure to be filled in.
 * sa_params_basic_p (output)
 *   Pointer to Basic parameter extension to be filled in
 * direction (input)
 *   Must be one of SAB_DIRECTION_INBOUND or SAB_DIRECTION_OUTBOUND.
 *
 * Both the crypto and the authentication algorithm are initialized to
 * NULL. Either the cipher algorithm or the authentication algorithm
 * or both can be set to one of the supported algorithms for
 * basic crypto or basic hash or HMAC. If they are both left at NULL. the
 * SA will be a bypass SA. The crypto mode and iv source
 * can be specified as well.  Any required keys have to be specified
 * as well.
 *
 * Both the sa_params_p and sa_params_basic_p input parameters must point
 * to valid storage where variables of the appropriate type can be
 * stored. This function initializes the link from sa_params_p to
 * sa_params_basic_p.
 *
 * Return:
 * SAB_STATUS_OK on success
 * SAB_INVALID_PARAMETER when one of the pointer parameters is NULL
 *   or the remaining parameters have illegal values.
 */
sa_builder_status_t
sa_builder_init_basic(
    sa_builder_params_t * const sa_params_p,
    sa_builder_params_basic_t * const sa_params_basic_p,
    const sa_builder_direction_t direction);


#endif /* SA_BUILDER_BASIC_H_ */


/* end of file sa_builder_basic.h */
