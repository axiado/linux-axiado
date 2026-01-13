/* c_adapter_eip74.h
 *
 * Default Adapter Global configuration for EIP-74
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

#ifndef INCLUDE_GUARD_C_ADAPTER_EIP74_H
#define INCLUDE_GUARD_C_ADAPTER_EIP74_H

/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Top-level Adapter configuration
#include "cs_adapter.h"


#ifndef ADAPTER_EIP74_DEVICE_NAME
#define ADAPTER_EIP74_DEVICE_NAME            "EIP74"
#endif // ADAPTER_EIP74_DEVICE_NAME

#ifndef ADAPTER_EIP74_RESET_MAX_RETRIES
#define ADAPTER_EIP74_RESET_MAX_RETRIES      1000
#endif // ADAPTER_EIP74_RESET_MAX_RETRIES


// EIP-74 generate block size
#ifndef ADAPTER_EIP74_GEN_BLK_SIZE
#define ADAPTER_EIP74_GEN_BLK_SIZE           4095
#endif

// EIP-74 reseed threshold.
#ifndef ADAPTER_EIP74_RESEED_THR
#define ADAPTER_EIP74_RESEED_THR             0xffffffff
#endif

// EIP-74 reaseed early wrning threshold.
#ifndef ADAPTER_EIP74_RESEED_THR_EARLY
#define ADAPTER_EIP74_RESEED_THR_EARLY       0xff000000
#endif

// Define if EIP-74 uses interrupts.
//ADAPTER_EIP74_INTERRUPTS_ENABLE

#ifdef ADAPTER_EIP74_INTERRUPTS_ENABLE
#ifndef ADAPTER_EIP74_ERR_IRQ
#define ADAPTER_EIP74_ERR_IRQ           0
#endif
#ifndef ADAPTER_EIP74_RES_IRQ
#define ADAPTER_EIP74_RES_IRQ           0
#endif
#endif

// EIP-74 DRBG device ID, keep undefined if RPM for EIP-74 not used
//#define ADAPTER_PEC_RPM_EIP74_DEVICE0_ID  0


#endif /* Include Guard */


/* end of file c_adapter_eip74.h */
