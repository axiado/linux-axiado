/* c_eip207_flow.h
 *
 * Default configuration parameters
 * for the EIP-207 Flow Control Driver Library
 *
 */

/* -------------------------------------------------------------------------- */
/*                                                                            */
/*   Module        : ddk197                                                   */
/*   Version       : 5.7.2                                                    */
/*   configuration : DDK-197-BSD                                              */
/*                                                                            */
/*   Date          : 2025-Nov-13                                              */
/*                                                                            */
/* Copyright (c) 2008-2025 by Rambus, Inc. and/or its subsidiaries.           */
/*                                                                            */
/* Redistribution and use in source and binary forms, with or without         */
/* modification, are permitted provided that the following conditions are     */
/* met:                                                                       */
/*                                                                            */
/* 1. Redistributions of source code must retain the above copyright          */
/* notice, this list of conditions and the following disclaimer.              */
/*                                                                            */
/* 2. Redistributions in binary form must reproduce the above copyright       */
/* notice, this list of conditions and the following disclaimer in the        */
/* documentation and/or other materials provided with the distribution.       */
/*                                                                            */
/* 3. Neither the name of the copyright holder nor the names of its           */
/* contributors may be used to endorse or promote products derived from       */
/* this software without specific prior written permission.                   */
/*                                                                            */
/* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS        */
/* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT          */
/* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR      */
/* A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT       */
/* HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,     */
/* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT           */
/* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,      */
/* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY      */
/* THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT        */
/* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE      */
/* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.       */
/* -------------------------------------------------------------------------- */

#ifndef C_EIP207_FLOW_H_
#define C_EIP207_FLOW_H_


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Top-level configuration
#include "cs_eip207_flow.h"


/*----------------------------------------------------------------------------
 * Definitions and macros
 */

// Maximum number of EIP-207c Classification Engines that can be used
// Should not exceed the number of engines physically available
#ifndef EIP207_FLOW_MAX_NOF_CE_TO_USE
#define EIP207_FLOW_MAX_NOF_CE_TO_USE                   1
#endif

// Maximum supported number of flow hash tables
#ifndef EIP207_FLOW_MAX_NOF_FLOW_HASH_TABLES_TO_USE
#define EIP207_FLOW_MAX_NOF_FLOW_HASH_TABLES_TO_USE     1
#endif

// Define this parameter for additional consistency checks in the flow
// control functionality
//#define EIP207_FLOW_CONSISTENCY_CHECK

// Strict argument checking for the input parameters
// If required then define this parameter in the top-level configuration
//#define EIP207_FLOW_STRICT_ARGS

// Disable clustered write operations, e.g. every write operation to
// an EIP-207 register will be followed by one read operation to
// a pre-defined EIP-207 register
//#define EIP207_FLOW_CLUSTERED_WRITES_DISABLE

// EIP-207s Classification Support,
// Flow Look-Up Engine (flue) Record Cache type:
// If defined then use the  Hihg-Performance (HP) Record Cache,
// otherwise use the standard Record Cache
//#define EIP207_FLUE_RC_HP

// EIP-207s Classification Support,
// Flow Look-Up Engine (flue) register bank size
#ifndef EIP207_FLUE_FHT_REG_MAP_SIZE
#define EIP207_FLUE_FHT_REG_MAP_SIZE                     8192
#endif

// EIP-207s Classification Support,
// Flow Look-Up Engine (flue) CACHEBASE registers
#ifndef EIP207_FLUE_FHT1_REG_BASE
#define EIP207_FLUE_FHT1_REG_BASE                        0x00000
#endif // EIP207_FLUE_FHT1_REG_BASE

// EIP-207s Classification Support,
// Flow Look-Up Engine (flue) HASHBASE registers
#ifndef EIP207_FLUE_FHT2_REG_BASE
#define EIP207_FLUE_FHT2_REG_BASE                        0x00000
#endif // EIP207_FLUE_FHT2_REG_BASE

// EIP-207s Classification Support,
// Flow Look-Up Engine (flue) SIZE register
#ifndef EIP207_FLUE_FHT3_REG_BASE
#define EIP207_FLUE_FHT3_REG_BASE                        0x00000
#endif // EIP207_FLUE_FHT3_REG_BASE

// EIP-207s Classification Support, options and version registers
#ifndef EIP207_FLUE_OPTVER_REG_BASE
#define EIP207_FLUE_OPTVER_REG_BASE                      0x00000
#endif // EIP207_FLUE_OPTVER_REG_BASE

// EIP-207s Classification Support, Flow Hash Engine (FHASH) registers
#ifndef EIP207_FLUE_HASH_REG_BASE
#define EIP207_FLUE_HASH_REG_BASE                        0x00000
#endif // EIP207_FLUE_HASH_REG_BASE

// Finite state Machine that can be used for verifying that the Driver Library
// API is called in a correct order
//#define EIP207_FLOW_DEBUG_FSM

// Enable 64-bit DMA address support in the device
//#define EIP207_64BIT_DEVICE

// Define to disable the large transform record support
//#define EIP207_FLOW_REMOVE_TR_LARGE_SUPPORT

// Define to disable type bits in pointers inside flow record.
//#define EIP207_FLOW_NO_TYPE_BITS_IN_FLOW_RECORD


// Flow or transform record (FLUE_STD), Hash bucket (FLUE_DTL)
// remove wait loop count
#ifndef EIP207_FLOW_RECORD_REMOVE_WAIT_COUNT
#define EIP207_FLOW_RECORD_REMOVE_WAIT_COUNT            1000000
#endif


#endif /* C_EIP207_FLOW_H_ */


/* end of file c_eip207_flow.h */
