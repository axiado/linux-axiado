/* c_adapter_pcl.h
 *
 * Default PCL Adapter configuration.
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

#ifndef INCLUDE_GUARD_C_ADAPTER_PCL_H
#define INCLUDE_GUARD_C_ADAPTER_PCL_H

/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Top-level Adapter configuration
#include "cs_adapter.h"

#ifndef ADAPTER_PCL_DEVICE_NAME
#define ADAPTER_PCL_DEVICE_NAME                         "PCL_Device"
#endif

#ifndef ADAPTER_PCL_BANK_FLOWTABLE
#define ADAPTER_PCL_BANK_FLOWTABLE                      0
#endif

#ifndef ADAPTER_PCL_BANK_TRANSFORM
#define ADAPTER_PCL_BANK_TRANSFORM                      0
#endif

#ifndef ADAPTER_PCL_BANK_FLOW
#define ADAPTER_PCL_BANK_FLOW                           0
#endif

//#define ADAPTER_PCL_ENABLE

// This parameters enables use of the DMA banks
//#define ADAPTER_PCL_DMARESOURCE_BANKS_ENABLE

#ifndef ADAPTER_PCL_ENABLE_FLUE_CACHE
#define ADAPTER_PCL_ENABLE_FLUE_CACHE false
#endif

#ifndef ADAPTER_CS_MAX_NOF_FLOW_HASH_TABLES_TO_USE
#define ADAPTER_CS_MAX_NOF_FLOW_HASH_TABLES_TO_USE      1
#endif

#ifndef ADAPTER_PCL_FLOW_HASH_ENTRIES_COUNT
#define ADAPTER_PCL_FLOW_HASH_ENTRIES_COUNT             1024
#endif

#if defined(ADAPTER_PCL_DMARESOURCE_BANKS_ENABLE) && \
    !defined(ADAPTER_PCL_FLOW_RECORD_COUNT)
#define ADAPTER_PCL_FLOW_RECORD_COUNT     ADAPTER_PCL_FLOW_HASH_ENTRIES_COUNT
#endif

#if defined(ADAPTER_PCL_DMARESOURCE_BANKS_ENABLE) && \
    !defined(ADAPTER_PCL_FLOW_RECORD_BYTE_COUNT)
#error "ADAPTER_PCL_FLOW_RECORD_BYTE_COUNT is not defined"
#endif

#if defined(ADAPTER_PCL_DMARESOURCE_BANKS_ENABLE) && \
    !defined(ADAPTER_TRANSFORM_RECORD_COUNT)
#error "ADAPTER_TRANSFORM_RECORD_COUNT is not defined"
#endif

#if defined(ADAPTER_PCL_DMARESOURCE_BANKS_ENABLE) && \
    !defined(ADAPTER_TRANSFORM_RECORD_BYTE_COUNT)
#error "ADAPTER_TRANSFORM_RECORD_BYTE_COUNT is not defined"
#endif

// number of tries to wait until a flow can be ssafely removed.
#ifndef ADAPTER_PCL_FLOW_REMOVE_MAX_TRIES
#define ADAPTER_PCL_FLOW_REMOVE_MAX_TRIES               100
#endif

// Delay between tries to check that a flow can be safely removed.
// Delay is in milliseconds. If it is specified as zero, no delay is inserted.
#ifndef ADAPTER_PCL_FLOW_REMOVE_DELAY
#define ADAPTER_PCL_FLOW_REMOVE_DELAY                   10
#endif

// number of overflow buckets associated with hash table
// (defaults here to: number of hash table entries / 16)
#ifndef ADAPTER_PCL_FLOW_HASH_OVERFLOW_COUNT
#define ADAPTER_PCL_FLOW_HASH_OVERFLOW_COUNT \
    (ADAPTER_PCL_FLOW_HASH_ENTRIES_COUNT / 16)
#endif

// Default flue device name
#ifndef ADAPTER_PCL_FLUE_DEFAULT_DEVICE_NAME
#define ADAPTER_PCL_FLUE_DEFAULT_DEVICE_NAME            "EIP207_FLUE0"
#endif

// Maximum number of flue device instances to be supported
#ifndef ADAPTER_PCL_MAX_FLUE_DEVICES
#define ADAPTER_PCL_MAX_FLUE_DEVICES                    1
#endif

// device numbering: max number of final digits in device instance name
// eg. value of 1 allows 0-9, value of 2 allows 0-99
#ifndef ADAPTER_PCL_MAX_DEVICE_DIGITS
#define ADAPTER_PCL_MAX_DEVICE_DIGITS                   1
#endif

#ifndef ADAPTER_PCL_LIST_ID_OFFSET
#error "ADAPTER_PCL_LIST_ID_OFFSET is not defined"
#endif

// Default DMA buffer allocation alignment is 4 bytes
#ifndef ADAPTER_PCL_DMA_ALIGNMENT_BYTE_COUNT
#define ADAPTER_PCL_DMA_ALIGNMENT_BYTE_COUNT            4
#endif

// EIP-207 flow control device ID, keep undefined if RPM for EIP-207 not used
//#define ADAPTER_PCL_RPM_EIP207_DEVICE_ID                0


#endif /* Include Guard */


/* end of file c_adapter_pcl.h */
