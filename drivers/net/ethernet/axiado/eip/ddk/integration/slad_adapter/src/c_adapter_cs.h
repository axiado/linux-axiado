/* c_adapter_cs.h
 *
 * Default Global Classification Control Adapter configuration
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

#ifndef INCLUDE_GUARD_C_ADAPTER_H
#define INCLUDE_GUARD_C_ADAPTER_H

/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Top-level Global Classification Control Adapter configuration
#include "cs_adapter.h"


/****************************************************************************
 * Global Classification Control Adapter general configuration parameters
 */

#ifndef ADAPTER_CS_IV
// Four words to provide IVs
#define ADAPTER_CS_IV         {0x28e1d20f, 0x30507ca5, 0x084484f1, 0xb8a5aeab}
#endif

#ifndef ADAPTER_CS_TIMER_PRESCALER
#define ADAPTER_CS_TIMER_PRESCALER           32
#endif // ADAPTER_CS_TIMER_PRESCALER

#ifndef ADAPTER_CS_RC_BLOCK_CLOCK_COUNT
#define ADAPTER_CS_RC_BLOCK_CLOCK_COUNT      1
#endif // ADAPTER_CS_RC_BLOCK_CLOCK_COUNT

#ifndef ADAPTER_CS_FLUE_CACHE_CHAIN
#define ADAPTER_CS_FLUE_CACHE_CHAIN          0
#endif // ADAPTER_CS_FLUE_CACHE_CHAIN

#ifndef ADAPTER_CS_FLUE_MEMXS_DELAY
#define ADAPTER_CS_FLUE_MEMXS_DELAY          0
#endif // ADAPTER_CS_FLUE_MEMXS_DELAY

// Set to 0 to disable the Flow Record Cache.
// Any other value will enable the frc
#ifndef ADAPTER_CS_FRC_ENABLED
// Disabling frc as this cache is not available in HW as of now.
// Please refer to Bug-Id#1005 for more details.
//Keeping this as is, its mostly due to 4M vs 1MB map (and indeed
//h/w doesn't have this cache
#define ADAPTER_CS_FRC_ENABLED               0
#endif // ADAPTER_CS_FRC_ENABLED

// Set to 0 to disable the Transform Record Cache.
// Any other value will enable the trc
#ifndef ADAPTER_CS_TRC_ENABLED
#define ADAPTER_CS_TRC_ENABLED               1
#endif // ADAPTER_CS_TRC_ENABLED

// Set to 0 to disable the arc4 state Record Cache.
// Any other value will enable the arc4_rc
#ifndef ADAPTER_CS_ARC4RC_ENABLED
#define ADAPTER_CS_ARC4RC_ENABLED            1
#endif // ADAPTER_CS_ARC4RC_ENABLED

// Set to 0 to disable the flue Cache. Any other value will enable the FLUEC
#ifndef ADAPTER_CS_FLUE_LOOKUP_CACHED
#define ADAPTER_CS_FLUE_LOOKUP_CACHED        1
#endif // ADAPTER_CS_FLUE_LOOKUP_CACHED

#ifndef ADAPTER_CS_FLUE_PREFETCH_XFORM
#define ADAPTER_CS_FLUE_PREFETCH_XFORM       1
#endif // ADAPTER_CS_FLUE_PREFETCH_XFORM

#ifndef ADAPTER_CS_FLUE_PREFETCH_ARC4
#define ADAPTER_CS_FLUE_PREFETCH_ARC4        0
#endif // ADAPTER_CS_FLUE_PREFETCH_ARC4

// This parameter enables the EIP-207 Record Cache support when defined
//#define ADAPTER_CS_RC_SUPPORT

// This parameter defines the maximum supported number of flow hash tables
#ifndef ADAPTER_CS_MAX_NOF_FLOW_HASH_TABLES_TO_USE
#define ADAPTER_CS_MAX_NOF_FLOW_HASH_TABLES_TO_USE    1
#endif

#ifndef ADAPTER_CS_GLOBAL_MAX_NOF_CE_TO_USE
#define ADAPTER_CS_GLOBAL_MAX_NOF_CE_TO_USE  1
#endif

// EIP-207 global control device ID, keep undefined if RPM for EIP-207 not used
//#define ADAPTER_GLOBAL_RPM_EIP207_DEVICE_ID     0

// Enable use of meta-data in input and output tokens,
// EIP-207 firmware must support this if enabled
//#define ADAPTER_CS_GLOBAL_IOTOKEN_METADATA_ENABLE

// Enable CFH headers.
//#define ADAPTER_CS_GLOBAL_CFH_ENABLE

// Configure the adapter for ice oce configuration
//#define ADAPTER_CS_GLOBAL_SRV_ICEOCE

// Configure that adapter for ice configuration
//#define ADAPTER_CS_GLOBAL_SRV_ICEO

// Enable pkt_id incrementing on egress tunnel headers.
//#define ADAPTER_CS_GLOBAL_INCREMENT_PKTID

// ECN control for ingress packets.
#ifndef ADAPTER_CS_GLOBAL_ECN_CONTROL
#define ADAPTER_CS_GLOBAL_ECN_CONTROL 0
#endif

// Record header alignment for DTLS records headers in decrypted packets.
#ifndef ADAPTER_CS_GLOBAL_DTLS_HDR_ALIGN
#define ADAPTER_CS_GLOBAL_DTLS_HDR_ALIGN 0
#endif

#ifndef ADAPTER_CS_GLOBAL_TRANSFORM_REDIRECT_ENABLE
#define ADAPTER_CS_GLOBAL_TRANSFORM_REDIRECT_ENABLE 0
#endif

// Defer DTLS packets type CCS to slowpath
//#define ADAPTER_CS_GLOBAL_DTLS_DEFER_CCS
// Defer DTLS packets type Alert to slowpath
//#define ADAPTER_CS_GLOBAL_DTLS_DEFER_ALERT
// Defer DTLS packets type Handshake to slowpath
//#define ADAPTER_CS_GLOBAL_DTLS_DEFER_HANDSHAKE
// Defer DTLS packets type AppData to slowpath
//#define ADAPTER_CS_GLOBAL_DTLS_DEFER_APPDATA
// Defer DTLS packets type CAPWAP to slowpath
//#define ADAPTER_CS_GLOBAL_DTLS_DEFER_CAPWAP


#include "c_adapter_cs_ext.h"      // chip-specific extensions


#endif /* Include Guard */


/* end of file c_adapter_cs.h */
