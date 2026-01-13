/* sa_builder_params_basic.h
 *
 * Basic crypto and hash specific extension to the sa_builder_params_t type.
 */

/*****************************************************************************
* Copyright (c) 2011-2025 by Rambus, Inc. and/or its subsidiaries.
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


#ifndef SA_BUILDER_PARAMS_BASIC_H_
#define SA_BUILDER_PARAMS_BASIC_H_


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

#include "sa_builder_params.h"

// Driver Framework Basic Definitions API
#include "basic_defs.h"


/*----------------------------------------------------------------------------
 * Definitions and macros
 */


/* Flag bits for the basic_flags field. Combine any values using a
   bitwise or.
 */
#define SAB_BASIC_FLAG_EXTRACT_ICV   BIT_0 /* Extract and verify ICV from packet*/
#define SAB_BASIC_FLAG_ENCRYPT_AFTER_HASH BIT_1
/* Encrypt the hashed data (default is hashing the encrypted data). */
#define SAB_BASIC_FLAG_HMAC_PRECOMPUTE BIT_2
/* Special operation to precompute HMAC keys */
#define SAB_BASIC_FLAG_XFRM_API BIT_3
/* Specify bypass operation for XFRM API */
#define SAB_BASIC_FLAG_HASH_UNALIGNED BIT_4

/* Extension record for SAParams_t. Protocol_Extension_p must point
   to this structure when the Basic crypto/hash protocol is used.

   SABuilder_Iinit_Basic() will fill all fields in this structure  with
   sensible defaults.
 */

typedef struct
{
    u32 basic_flags;
    u32 digest_block_count;
    u32 icv_byte_count; /* Length of ICV in bytes. */
    u32  fresh;      /* 32-bit 'fresh' value for wireless authentication
                             algorithms. */
    u8 bearer;       /* 5-bit 'bearer' value for wireless algorithms. */
    u8 direction;   /* 1-bit 'direction' value for wireless algorithms. */
    u32 context_ref; /* Reference to application context */
} sa_builder_params_basic_t;


#endif /* SA_BUILDER_PARAMS_BASIC_H_ */


/* end of file sa_builder_params_basic.h */
