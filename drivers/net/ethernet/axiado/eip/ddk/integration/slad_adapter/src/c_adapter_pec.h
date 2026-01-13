/* c_adapter_pec.h
 *
 * Default Adapter PEC configuration
 */

/*****************************************************************************
* Copyright (c) 2012-2021 by Rambus, Inc. and/or its subsidiaries.
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

#ifndef INCLUDE_GUARD_C_ADAPTER_PEC_H
#define INCLUDE_GUARD_C_ADAPTER_PEC_H

/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Top-level Adapter configuration
#include "cs_adapter.h"


/****************************************************************************
 * Adapter general configuration parameters
 */

#ifndef ADAPTER_PEC_BANK_PACKET
#define ADAPTER_PEC_BANK_PACKET         0
#endif

#ifndef ADAPTER_PEC_BANK_TOKEN
#define ADAPTER_PEC_BANK_TOKEN          0
#endif

#ifndef ADAPTER_PEC_BANK_SA
#define ADAPTER_PEC_BANK_SA             0
#endif

//#define ADAPTER_PEC_STRICT_ARGS

#ifndef ADAPTER_PEC_DEVICE_COUNT
#define ADAPTER_PEC_DEVICE_COUNT        1
#endif

#ifndef ADAPTER_PEC_MAX_PACKETS
#define ADAPTER_PEC_MAX_PACKETS         32
#endif

#ifndef ADAPTER_PEC_MAX_LOGICDESCR
#define ADAPTER_PEC_MAX_LOGICDESCR      32
#endif

//#define ADAPTER_PEC_SEPARATE_RINGS
//#define ADAPTER_PEC_ENABLE_SCATTERGATHER
//#define ADAPTER_PEC_INTERRUPTS_ENABLE
//#define ADAPTER_PEC_ARMRING_ENABLE_SWAP

// Remove bounce buffers support
//#define ADAPTER_PEC_REMOVE_BOUNCEBUFFERS

// EIP-202 Ring manager device ID, keep undefined if RPM for EIP-202 not used
//#define ADAPTER_PEC_RPM_EIP202_DEVICE0_ID  0


#endif /* Include Guard */


/* end of file c_adapter_pec.h */
