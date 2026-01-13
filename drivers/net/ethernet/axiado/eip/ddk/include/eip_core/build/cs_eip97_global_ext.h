/* cs_eip97_global_ext.h
 *
 * Top-level configuration parameters extensions
 * for the EIP-97 Global Control Driver Library
 *
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

#ifndef CS_EIP97_GLOBAL_EXT_H_
#define CS_EIP97_GLOBAL_EXT_H_

#include "cs_ddk197.h"
#include "cs_adapter.h"

/*----------------------------------------------------------------------------
 * Definitions and macros
 */

// Define this parameter in order to configure the DFE and DSE ring priorities
//#define EIP97_GLOBAL_DFE_DSE_PRIO_CONFIGURE

// EIP-207s Classification Support, DMA Control base address
//#define EIP97_RC_BASE      0x37000
#define EIP97_RC_BASE      0xF7000

// EIP-207s Classification Support, DMA Control base address
//#define EIP97_BASE         0x3FFF4
#define EIP97_BASE         0xFFFF4

//Define if the hardware ECN registers are included
#define EIP97_GLOBAL_HAVE_ECN_FIXUP

#ifdef EIP97_GLOBAL_HAVE_ECN_FIXUP
#ifdef ADAPTER_CS_GLOBAL_ECN_CONTROL
#define EIP97_GLOBAL_EIP96_ECN_CONTROL ADAPTER_CS_GLOBAL_ECN_CONTROL
#endif
#endif

// Numbers of the look-aside fifo queues
// EIP-197 configuration:
//           b - 1, c - 3, d - 5, e - 1
// Leave this undefined for devices without LA fifo queues (Mobile).
#define EIP97_GLOBAL_MAX_NOF_LAFIFO_TO_USE          1


// Numbers of the Inline fifo queues
// Leave this undefined for devices without Inline fifo queues (Mobile).
#define EIP97_GLOBAL_MAX_NOF_INFIFO_TO_USE          1

// Enables DSE single write mode, Set to 1 for optimal performance.
// Applicable only for configurations with
// - Non-overlapping rings and
// - 64-bit DMA address descriptor format
#if defined(DRIVER_PE_ARM_SEPARATE) && defined(DRIVER_64BIT_DEVICE)
#define EIP97_GLOBAL_DSE_ENABLE_SINGLE_WR_FLAG      1
#endif

// data write bufferability control,
// for the Look-Aside fifo streaming interface only
// Note: when the LA fifo is not used then set this to 3 for optimal performance
#define EIP97_GLOBAL_DSE_BUFFER_CTRL                3

// Determines the maximum burst size that will be used on the receive side
// of the AXI interface or
// secondary requesting and priority for the PLB interface
#if defined(EIP197_BUS_VERSION_AXI3)
// For AXI v3
#define EIP97_GLOBAL_RX_BUS_BURST_SIZE       3
#elif defined(EIP197_BUS_VERSION_AXI4)
// For AXI v4
#define EIP97_GLOBAL_RX_BUS_BURST_SIZE       4
#elif defined(EIP197_BUS_VERSION_PLB)
// For PLB
#define EIP97_GLOBAL_RX_BUS_BURST_SIZE       5
#else
#error "error: EIP97_BUS_VERSION_[PLB|AXI3|AXI4] not configured"
#endif


// Burst size of inline interface
#define EIP202_INLINE_BURST_SIZE  2

// Force in-order on inline interface.
#define EIP202_INLINE_FORCE_INORDER 1

// EIP96 context size in words
#ifdef DDK_EIP197_EIP96_SEQMASK_1024
#define EIP97_EIP96_CTX_SIZE 0x52
#endif

#endif /* CS_EIP97_GLOBAL_EXT_H_ */


/* end of file cs_eip97_global_ext.h */
