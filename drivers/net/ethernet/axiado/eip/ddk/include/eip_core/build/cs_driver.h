/* cs_driver.h
 *
 * Top-level Product configuration Settings.
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

#ifndef INCLUDE_GUARD_CS_DRIVER_H
#define INCLUDE_GUARD_CS_DRIVER_H

#include "cs_systemtestconfig.h"      // defines SYSTEMTEST_CONFIG_Cnn

// Host hardware platform specific extensions
#include "cs_driver_ext.h"


// Driver name used for reporting
#define DRIVER_NAME     "Security_IP-197"

// activate in case of endianness difference between CPU and EIP
// to ask driver to swap byte order of all control words
// we assume that if ARCH is PowerPC, then CPU is big endian
#ifdef ARCH_POWERPC
#define DRIVER_SWAPENDIAN
#endif //ARCH_POWERPC

// Maximum number of Processing Engines (EIP-206) to use
#if defined(DRIVER_EIP197_HW_CONF_B)
#define DRIVER_MAX_NOF_PE_TO_USE                       1
#elif defined(DRIVER_EIP197_HW_CONF_C)
#define DRIVER_MAX_NOF_PE_TO_USE                       2
#elif defined(DRIVER_EIP197_HW_CONF_D)
#define DRIVER_MAX_NOF_PE_TO_USE                       4
#elif defined(DRIVER_EIP197_HW_CONF_E)
#define DRIVER_MAX_NOF_PE_TO_USE                       8
#elif defined(DRIVER_EIP197_HW_CONF_F)
#define DRIVER_MAX_NOF_PE_TO_USE                       12
#else
#error "Unsupported EIP-197 HW configuration"
#endif

// Maximum number of Classification Engines (EIP-207c) to use
// Note: one Processing Engine includes one Classification Engine
#define DRIVER_CS_MAX_NOF_CE_TO_USE                    DRIVER_MAX_NOF_PE_TO_USE

// size (in entries) of the Flow Hash Table (FHT)
// used by the EIP-207 Classification Engine
#define DRIVER_PCL_FLOW_HASH_ENTRIES_COUNT             512

// Some applications allocate network packets at unaligned addresses.
// To avoid bouncing these buffers, you can allow unaligned buffers if
// caching is properly handled in hardware.
//#define DRIVER_ALLOW_UNALIGNED_DMA


// C0 = ARM, Separate, Interrupt coalescing, BB=Y, Perf=N, sg=N
#ifdef SYSTEMTEST_CONFIGURATION_C0
//#define DRIVER_PE_TCM
#define DRIVER_PE_ARM_SEPARATE
#define DRIVER_INTERRUPTS
#define DRIVER_INTERRUPT_COALESCING
#define DRIVER_BOUNCEBUFFERS
//#define DRIVER_PERFORMANCE
//#define DRIVER_SCATTERGATHER
#define DRIVER_PEC_MAX_SAS         64
#define DRIVER_PEC_MAX_PACKETS     32
#define DRIVER_MAX_PECLOGICDESCR   32
#endif

// C1 = ARM, Separate, Polling, BB=N, Perf=N, sg=N
#ifdef SYSTEMTEST_CONFIGURATION_C1
//#define DRIVER_PE_TCM
#define DRIVER_PE_ARM_SEPARATE
//#define DRIVER_INTERRUPTS
//#define DRIVER_INTERRUPT_COALESCING
//#define DRIVER_BOUNCEBUFFERS
//#define DRIVER_PERFORMANCE
//#define DRIVER_SCATTERGATHER
#endif

// C2 = ARM, Separate, Interrupts, BB=N, Perf=N, sg=N
#ifdef SYSTEMTEST_CONFIGURATION_C2
//#define DRIVER_PE_TCM
#define DRIVER_PE_ARM_SEPARATE
#define DRIVER_INTERRUPTS
//#define DRIVER_INTERRUPT_COALESCING
//#define DRIVER_BOUNCEBUFFERS
//#define DRIVER_PERFORMANCE
//#define DRIVER_SCATTERGATHER
#undef DRIVER_PCL_FLOW_HASH_ENTRIES_COUNT
#define DRIVER_PCL_FLOW_HASH_ENTRIES_COUNT             64
#define DRIVER_ALLOW_UNALIGNED_DMA
#endif


// C4 = ARM, Separate, Polling, BB=Yes, Perf=N, sg=N
#ifdef SYSTEMTEST_CONFIGURATION_C4
//#define DRIVER_PE_TCM
#define DRIVER_PE_ARM_SEPARATE
//#define DRIVER_INTERRUPTS
//#define DRIVER_INTERRUPT_COALESCING
#define DRIVER_BOUNCEBUFFERS
//#define DRIVER_PERFORMANCE
//#define DRIVER_SCATTERGATHER
#endif

// C5 = ARM, Separate, Interrupts, BB=Yes, Perf=N, sg=N
#ifdef SYSTEMTEST_CONFIGURATION_C5
//#define DRIVER_PE_TCM
#define DRIVER_PE_ARM_SEPARATE
#define DRIVER_INTERRUPTS
//#define DRIVER_INTERRUPT_COALESCING
#define DRIVER_BOUNCEBUFFERS
//#define DRIVER_PERFORMANCE
//#define DRIVER_SCATTERGATHER
#endif

// C8 = ARM, Polling, Separate, BB=N, Perf=Yes, sg=N
#ifdef SYSTEMTEST_CONFIGURATION_C8
//#define DRIVER_PE_TCM
#define DRIVER_PE_ARM_SEPARATE
//#define DRIVER_INTERRUPTS
//#define DRIVER_INTERRUPT_COALESCING
//#define DRIVER_BOUNCEBUFFERS
#define DRIVER_PERFORMANCE
//#define DRIVER_SCATTERGATHER
#define DRIVER_PEC_MAX_SAS         64
#define DRIVER_PEC_MAX_PACKETS     32
#define DRIVER_MAX_PECLOGICDESCR   32
#endif

// C9 = ARM, Interrupts, Separate, BB=N, Perf=Yes, sg=N
#ifdef SYSTEMTEST_CONFIGURATION_C9
//#define DRIVER_PE_TCM
#define DRIVER_PE_ARM_SEPARATE
#define DRIVER_INTERRUPTS
//#define DRIVER_INTERRUPT_COALESCING
//#define DRIVER_BOUNCEBUFFERS
#define DRIVER_PERFORMANCE
//#define DRIVER_SCATTERGATHER
#define DRIVER_PEC_MAX_SAS         64
#define DRIVER_PEC_MAX_PACKETS     32
#define DRIVER_MAX_PECLOGICDESCR   32
#endif


// C11 = ARM, Polling, Separate, BB=Yes, Perf=Yes, sg=N
#ifdef SYSTEMTEST_CONFIGURATION_C11
//#define DRIVER_PE_TCM
#define DRIVER_PE_ARM_SEPARATE
//#define DRIVER_INTERRUPTS
//#define DRIVER_INTERRUPT_COALESCING
#define DRIVER_BOUNCEBUFFERS
#define DRIVER_PERFORMANCE
//#define DRIVER_SCATTERGATHER
#define DRIVER_PEC_MAX_SAS         64
#define DRIVER_PEC_MAX_PACKETS     32
#define DRIVER_MAX_PECLOGICDESCR   32
#endif

// C14 = ARM, Polling, Separate, BB=N, Perf=N, sg=Yes
#ifdef SYSTEMTEST_CONFIGURATION_C14
//#define DRIVER_PE_TCM
#define DRIVER_PE_ARM_SEPARATE
//#define DRIVER_INTERRUPTS
//#define DRIVER_INTERRUPT_COALESCING
//#define DRIVER_BOUNCEBUFFERS
//#define DRIVER_PERFORMANCE
#define DRIVER_SCATTERGATHER
#define DRIVER_PEC_MAX_PACKETS     64
#define DRIVER_MAX_PECLOGICDESCR   64
#endif

// C15 = ARM, Interrupts, Separate, BB=N, Perf=N, sg=Yes
#ifdef SYSTEMTEST_CONFIGURATION_C15
//#define DRIVER_PE_TCM
#define DRIVER_PE_ARM_SEPARATE
#define DRIVER_INTERRUPTS
//#define DRIVER_INTERRUPT_COALESCING
//#define DRIVER_BOUNCEBUFFERS
//#define DRIVER_PERFORMANCE
#define DRIVER_SCATTERGATHER
#define DRIVER_PEC_MAX_PACKETS     64
#define DRIVER_MAX_PECLOGICDESCR   64
#endif

// C17 = ARM, Interrupt + Coalescing, Separate, BB=N, Perf=Yes, sg=N
#ifdef SYSTEMTEST_CONFIGURATION_C17
//#define DRIVER_PE_TCM
#define DRIVER_PE_ARM_SEPARATE
#define DRIVER_INTERRUPTS
#define DRIVER_INTERRUPT_COALESCING
//#define DRIVER_BOUNCEBUFFERS
#define DRIVER_PERFORMANCE
//#define DRIVER_SCATTERGATHER
#define DRIVER_PEC_MAX_SAS           64
#define DRIVER_PEC_MAX_PACKETS       32
#define DRIVER_MAX_PECLOGICDESCR     32
#endif

//TODO: add another config for netdev instead of using existing C18
// C18 = ARM, Polling, Separate, BB=N, Perf=No, sg=N Byteswap on
#ifdef SYSTEMTEST_CONFIGURATION_C18
//#define DRIVER_PE_TCM
#define DRIVER_PE_ARM_SEPARATE
#ifndef DRIVER_MAX_PECLOGICDESCR
#define DRIVER_MAX_PECLOGICDESCR     20
#endif
#ifndef DRIVER_PEC_MAX_PACKETS
#define DRIVER_PEC_MAX_PACKETS       20
#endif
// As of now BuildSKB is not enabled in Alpha-1 release(July-2023).
// Hence the below one is disabled.
#define HWPAL_DMARESOURCE_FORCE_NON_COHERENT
#ifndef DRIVER_KERNEL_BUILTIN
#define DRIVER_KERNEL_BUILTIN
#endif
#ifndef DRIVER_64BIT_HOST
#define DRIVER_64BIT_HOST
#endif

//#define DRIVER_INTERRUPTS
//#define DRIVER_INTERRUPT_COALESCING
//#define DRIVER_BOUNCEBUFFERS
#ifndef DRIVER_PERFORMANCE
#define DRIVER_PERFORMANCE
#endif
//#define DRIVER_SCATTERGATHER
#ifdef SOME_STUPID_BOARD
// PLB FPGA does not allow for endianness conversion
// in the Board Control device slave interface required on x86,
// so just disable the test
#ifndef EIP197_BUS_VERSION_PLB
#define DRIVER_SWAPENDIAN         // Host and device have different endianness
#ifdef ARCH_POWERPC
#undef  DRIVER_SWAPENDIAN         // Switch off byte swap by the host processor
#endif //ARCH_POWERPC
// Enable byte swap by the Engine slave interface
#define DRIVER_ENABLE_SWAP_SLAVE
// Enable byte swap by the Engine master interface
#define DRIVER_ENABLE_SWAP_MASTER
#endif // not EIP197_BUS_VERSION_PLB
#endif // SOME_STUPID_BOARD
#endif // SYSTEMTEST_CONFIGURATION_C18

#ifndef DRIVER_PEC_MAX_SAS
#define DRIVER_PEC_MAX_SAS                      60
#endif


// EIP-197 hardware specific extensions
#include "cs_driver_ext2.h"


#endif /* Include Guard */


/* end of file cs_driver.h */
