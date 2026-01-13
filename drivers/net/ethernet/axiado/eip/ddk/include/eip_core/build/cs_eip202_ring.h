/* cs_eip202_ring.h
 *
 * Top-level configuration parameters
 * for the EIP-202 Ring Control Driver Library
 */

/*****************************************************************************
* Copyright (c) 2011-2021 by Rambus, Inc. and/or its subsidiaries.
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

#ifndef CS_EIP202_RING_H_
#define CS_EIP202_RING_H_

/*----------------------------------------------------------------------------
 * This module implements (provides) the following interface(s):
 */

#include "cs_driver.h"


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */


/*----------------------------------------------------------------------------
 * Definitions and macros
 */

// Is device 64-bit?
#ifdef DRIVER_64BIT_DEVICE
#define EIP202_64BIT_DEVICE
#endif // DRIVER_64BIT_DEVICE

// Disable clustered write operations, e.g. every write operation to
// an EIP-202 RD register will be followed by one read operation to
// a pre-defined EIP-202 register
#define EIP202_CLUSTERED_WRITES_DISABLE

#ifdef DRIVER_PEC_CD_PROT_VALUE
#define EIP202_RING_CD_PROT_VALUE               DRIVER_PEC_CD_PROT_VALUE
#endif

#ifdef DRIVER_PEC_RD_PROT_VALUE
#define EIP202_RING_RD_PROT_VALUE               DRIVER_PEC_RD_PROT_VALUE
#endif

#ifdef DRIVER_PEC_DATA_PROT_VALUE
#define EIP202_RING_DATA_PROT_VALUE             DRIVER_PEC_DATA_PROT_VALUE
#endif

#ifdef DRIVER_PEC_ACD_PROT_VALUE
#define EIP202_RING_ACD_PROT_VALUE              DRIVER_PEC_ACD_PROT_VALUE
#endif

#ifdef EIP197_BUS_VERSION_AXI3
// For AXI v3
#define EIP202_RING_CD_RD_CACHE_CTRL            2
#define EIP202_RING_CD_WR_CACHE_CTRL            2
#define EIP202_RING_RD_RD_CACHE_CTRL            2
#define EIP202_RING_RD_WR_CACHE_CTRL            2
#endif

#ifdef EIP197_BUS_VERSION_AXI4
// For AXI v4
#define EIP202_RING_CD_RD_CACHE_CTRL            5
#define EIP202_RING_CD_WR_CACHE_CTRL            3
#define EIP202_RING_RD_RD_CACHE_CTRL            5
#define EIP202_RING_RD_WR_CACHE_CTRL            3
#endif

// Strict argument checking for the input parameters
// If required then define this parameter in the top-level configuration
#ifndef DRIVER_PERFORMANCE
#define EIP202_RING_STRICT_ARGS
#endif

// Finite state Machine that can be used for verifying that the Driver Library
// API is called in a correct order
#ifndef DRIVER_PERFORMANCE
#define EIP202_RING_DEBUG_FSM
#endif

// Performance optimization: minimize CD and RD words writes and reads
#ifdef DRIVER_PERFORMANCE
#define EIP202_CDR_OPT1
#endif

#if defined(DRIVER_PE_ARM_SEPARATE)
#ifndef DRIVER_SCATTERGATHER
#if defined(DRIVER_64BIT_DEVICE) && !defined(DRIVER_PERFORMANCE)
// Enables padding the result descriptor to its full programmed offset,
// 0 - disabled, 1 - enabled
// Note: when enabled EIP97_GLOBAL_DSE_ENABLE_SINGLE_WR_FLAG must be defined too
#define EIP202_RDR_PAD_TO_OFFSET                1
#endif

// Enable RDR ownership word mechanism
#define EIP202_RDR_OWNERSHIP_WORD_ENABLE

#endif // !DRIVER_SCATTERGATHER
#else // DRIVER_PE_ARM_SEPARATE
#ifndef DRIVER_SCATTERGATHER
// Application ID field in the result token cannot be used when scatter/gather
// is enabled because this ID field is only written in the last descriptor
// in the chain!
#define EIP202_RING_ANTI_DMA_RACE_CONDITION_CDS
#endif // !DRIVER_SCATTERGATHER
#endif // !DRIVER_PE_ARM_SEPARATE


// bufferability control for result token DMA writes:
// 0b = do not buffer, 1b = allow buffering
#define EIP202_RING_RD_RES_BUF                  0

// bufferability control for descriptor control word DMA writes:
// 0b = do not buffer, 1b = allow buffering.
#define EIP202_RING_RD_CTRL_BUF                 0

#include "cs_eip202_ring_ext.h"

#endif /* CS_EIP202_RING_H_ */


/* end of file cs_eip202_ring.h */
