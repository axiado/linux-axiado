/* cs_driver_ext2.h
 *
 * Top-level Product EIP-197 hardware specific configuration Settings.
 */

/*****************************************************************************
* Copyright (c) 2012-2022 by Rambus, Inc. and/or its subsidiaries.
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

#ifndef INCLUDE_GUARD_CS_DRIVER_EXT2_H
#define INCLUDE_GUARD_CS_DRIVER_EXT2_H

// Disable usage of the EIP-202 Ring Arbiter
//#define DRIVER_EIP202_RA_DISABLE

// number of Ring interfaces to use
//#define DRIVER_MAX_NOF_RING_TO_USE                     14
//#define DRIVER_MAX_NOF_RING_TO_USE                     4

// Maximum number of Flow Look-Up Engines (flue in EIP-207s) to use
#define DRIVER_PCL_MAX_FLUE_DEVICES                 DRIVER_CS_MAX_NOF_CE_TO_USE

// Maximum number of Flow Hash Tables to use in the Classification (PCL) Driver
#define DRIVER_CS_MAX_NOF_FLOW_HASH_TABLES_TO_USE      1

// number of overflow buckets associated with hash table
#define DRIVER_PCL_FLOW_HASH_OVERFLOW_COUNT  DRIVER_PCL_FLOW_HASH_ENTRIES_COUNT

#ifdef DRIVER_DMARESOURCE_BANKS_ENABLE

// Maximum number of flow records the driver can allocate
#define DRIVER_FLOW_RECORD_COUNT            \
                        (DRIVER_PCL_FLOW_HASH_ENTRIES_COUNT + \
                                    DRIVER_PCL_FLOW_HASH_OVERFLOW_COUNT)

// Flow record size is determined by the EIP-207 Firmware API
#define DRIVER_FLOW_RECORD_BYTE_COUNT                  64

// Maximum number of SA or transform records the driver can allocate
#define DRIVER_TRANSFORM_RECORD_COUNT                  DRIVER_PEC_MAX_SAS

// Maximum Transform record size for the static DMA bank
// For the Look-Aside Basic Crypto flow the transform record size is determined
// by the maximum used SA size.
// For all the other flows the transform record size is determined
// by the EIP-207 Firmware API.
// The static DMA bank is used only for the Flow and Transform records
// that can be looked up, e.g. NOT for the Look-Aside Basic Crypto flow.
#define DRIVER_TRANSFORM_RECORD_BYTE_COUNT             512

// Each static bank for DMA resources must have 2 lists plus
// each classification engine requires 1 list
#define DRIVER_LIST_PCL_MAX_NOF_INSTANCES           DRIVER_PCL_MAX_FLUE_DEVICES
#define DRIVER_LIST_MAX_NOF_INSTANCES              \
                                (DRIVER_LIST_HWPAL_MAX_NOF_INSTANCES +  \
                                        DRIVER_LIST_PCL_MAX_NOF_INSTANCES)
#define DRIVER_LIST_PCL_OFFSET             DRIVER_LIST_HWPAL_MAX_NOF_INSTANCES

// Determine maximum DMA bank element size
#define DRIVER_DMA_BANK_ELEMENT_BYTE_COUNT      \
        (DRIVER_FLOW_RECORD_BYTE_COUNT > DRIVER_TRANSFORM_RECORD_BYTE_COUNT ? \
           DRIVER_FLOW_RECORD_BYTE_COUNT : DRIVER_TRANSFORM_RECORD_BYTE_COUNT)

// Determine maximum number of elements in DMA bank
#define DRIVER_DMA_BANK_ELEMENT_COUNT           \
                (DRIVER_FLOW_RECORD_COUNT + DRIVER_TRANSFORM_RECORD_COUNT)

#endif // DRIVER_DMARESOURCE_BANKS_ENABLE

// Driver uses record invalidate commands (with non-extended descriptor format).
//#define DRIVER_USE_INVALIDATE_COMMANDS


#endif /* Include Guard */


/* end of file cs_driver_ext2.h */
