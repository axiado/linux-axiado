/* c_eip207_global.h
 *
 * Default configuration parameters
 * for the EIP-207 Global Control Driver Library
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

#ifndef C_EIP207_GLOBAL_H_
#define C_EIP207_GLOBAL_H_


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Top-level configuration
#include "cs_eip207_global.h"


/*----------------------------------------------------------------------------
 * Definitions and macros
 */

// EIP-207 supported HW version
#define EIP207_GLOBAL_MAJOR_VERSION                 1
#define EIP207_GLOBAL_MINOR_VERSION                 0
#define EIP207_GLOBAL_PATCH_LEVEL                   0

// Maximum number of Classification Engine (EIP-207c) instances
//#define EIP207_GLOBAL_MAX_NOF_CE_TO_USE           1

// number of sets of the frc/trc/ARC4C HW interfaces (p+1)
// Maximum number of cache interfaces that can be used
// Should not exceed the number of cache sets physically available
#ifndef EIP207_GLOBAL_MAX_NOF_CACHE_SETS_TO_USE
#define EIP207_GLOBAL_MAX_NOF_CACHE_SETS_TO_USE     1
#endif // EIP207_GLOBAL_MAX_NOF_CACHE_SETS_TO_USE

// Enable Context data Endianness Conversion by the Processing Engine hardware
// Swap bytes within each 32 bit word
//#define EIP207_GLOBAL_BYTE_SWAP_FLOW_DATA

// Enable Transform data Endianness Conversion by the Processing Engine hardware
// Swap bytes within each 32 bit word
//#define EIP207_GLOBAL_BYTE_SWAP_TRANSFORM_DATA

// Enable arc4 Context data Endianness Conversion by the PE hardware
// Swap bytes within each 32 bit word
//#define EIP207_GLOBAL_BYTE_SWAP_ARC4_DATA

// Define to disable the frc
//#define EIP207_GLOBAL_FRC_DISABLE

// size of the Flow Record Cache (data RAM) in 32-bit words
#ifndef EIP207_FRC_RAM_WORD_COUNT
#define EIP207_FRC_RAM_WORD_COUNT                   512
#endif // EIP207_FRC_RAM_WORD_COUNT

// size of the Flow Record Cache (administration RAM) in 32-bit words
//#define EIP207_FRC_ADMIN_RAM_WORD_COUNT             8192

// size of the Transform Record Cache (data RAM) in 32-bit words
#ifndef EIP207_TRC_RAM_WORD_COUNT
#define EIP207_TRC_RAM_WORD_COUNT                   1024
#endif // EIP207_TRC_RAM_WORD_COUNT

// size of the Transform Record Cache (administration RAM) in 32-bit words
//#define EIP207_TRC_ADMIN_RAM_WORD_COUNT             8192

// size of the arc4 Record Cache (data RAM) in 32-bit words
// Note: arc4_rc is combined with trc
#ifndef EIP207_ARC4RC_RAM_WORD_COUNT
#define EIP207_ARC4RC_RAM_WORD_COUNT                EIP207_TRC_RAM_WORD_COUNT
#endif // EIP207_ARC4RC_RAM_WORD_COUNT

// size of the arc4 Record Cache (administration RAM) in 32-bit words
//#define EIP207_ARC4RC_ADMIN_RAM_WORD_COUNT          8192

//#define EIP207_FLUE_HAVE_VIRTUALIZATION

// number of interfaces for virtualization.
#ifndef EIP207_FLUE_MAX_NOF_INTERFACES_TO_USE
#define EIP207_FLUE_MAX_NOF_INTERFACES_TO_USE 15
#endif

// Main hash table size in entries (one entry is one 32-bit word)
// Table size = 2^(EIP207_FLUEC_TABLE_SIZE+3)
#ifndef EIP207_FLUEC_TABLE_SIZE
#define EIP207_FLUEC_TABLE_SIZE                     0
#endif // EIP207_FLUEC_TABLE_SIZE

// Group size in entries (one entry is one 32-bit word)
// Group size = 2^EIP207_FLUEC_GROUP_SIZE
#ifndef EIP207_FLUEC_GROUP_SIZE
#define EIP207_FLUEC_GROUP_SIZE                     1
#endif // EIP207_FLUEC_GROUP_SIZE

// size of the flue Cache in 32-bit words
// FLUEC RAM = Table size * (Group size * 6 + 1)
#ifndef EIP207_FLUEC_RAM_WORD_COUNT
#define EIP207_FLUEC_RAM_WORD_COUNT                 128
#endif // EIP207_FLUEC_RAM_WORD_COUNT

// Input Pull-Up Engine Program RAM size (in 32-bit words), 8KB
#ifndef EIP207_IPUE_PROG_RAM_WORD_COUNT
#define EIP207_IPUE_PROG_RAM_WORD_COUNT             2048
#endif // EIP207_IPUE_PROG_RAM_WORD_COUNT

// Input Flow Post-Processor Engine Program RAM size (in 32-bit words), 8KB
#ifndef EIP207_IFPP_PROG_RAM_WORD_COUNT
#define EIP207_IFPP_PROG_RAM_WORD_COUNT             2048
#endif // EIP207_IFPP_PROG_RAM_WORD_COUNT

// ice Scratchpad RAM size in 128-byte blocks (1KB)
#ifndef EIP207_ICE_SCRATCH_RAM_128B_BLOCK_COUNT
#define EIP207_ICE_SCRATCH_RAM_128B_BLOCK_COUNT     8
#endif // EIP207_ICE_SCRATCH_RAM_128B_BLOCK_COUNT

// Output Pull-Up Engine Program RAM size (in 32-bit words), 8KB
#ifndef EIP207_OPUE_PROG_RAM_WORD_COUNT
#define EIP207_OPUE_PROG_RAM_WORD_COUNT             2048
#endif // EIP207_OPUE_PROG_RAM_WORD_COUNT

// Output Flow Post-Processor Engine Program RAM size (in 32-bit words), 8KB
#ifndef EIP207_OFPP_PROG_RAM_WORD_COUNT
#define EIP207_OFPP_PROG_RAM_WORD_COUNT             2048
#endif // EIP207_OFPP_PROG_RAM_WORD_COUNT

// oce Scratchpad RAM size in 128-byte blocks (1KB)
#ifndef EIP207_OCE_SCRATCH_RAM_128B_BLOCK_COUNT
#define EIP207_OCE_SCRATCH_RAM_128B_BLOCK_COUNT     8
#endif // EIP207_OCE_SCRATCH_RAM_128B_BLOCK_COUNT

// Classification engine timer overflow bit
#ifndef EIP207_ICE_SCRATCH_TIMER_OFLO_BIT
#define EIP207_ICE_SCRATCH_TIMER_OFLO_BIT           31
#endif // EIP207_ICE_SCRATCH_TIMER_OFLO_BIT

// Classification engine timer overflow bit
#ifndef EIP207_OCE_SCRATCH_TIMER_OFLO_BIT
#define EIP207_OCE_SCRATCH_TIMER_OFLO_BIT           31
#endif // EIP207_OCE_SCRATCH_TIMER_OFLO_BIT

// Enable firmware version check after the download
//#define EIP207_GLOBAL_FIRMWARE_DOWNLOAD_VERSION_CHECK

#ifdef EIP207_GLOBAL_FIRMWARE_DOWNLOAD_VERSION_CHECK
#ifndef EIP207_FW_VER_CHECK_MAX_NOF_READ_ATTEMPTS
#define EIP207_FW_VER_CHECK_MAX_NOF_READ_ATTEMPTS   100
#endif // EIP207_FW_VER_CHECK_MAX_NOF_READ_ATTEMPTS
#endif // EIP207_GLOBAL_FIRMWARE_DOWNLOAD_VERSION_CHECK

// Disable clustered write operations, e.g. every write operation to
// an EIP-207 register will be followed by one read operation to
// a pre-defined EIP-207 register
//#define EIP207_CLUSTERED_WRITES_DISABLE

// Maximum number of read attempts for a 64-bit counter
#ifndef EIP207_VALUE_64BIT_MAX_NOF_READ_ATTEMPTS
#define EIP207_VALUE_64BIT_MAX_NOF_READ_ATTEMPTS    2
#endif

// Maximum number of hash tables supported by the EIP-207 flue
// if the CS_OPTIONS reads 0 for the number of lookup tables
#ifndef EIP207_GLOBAL_MAX_HW_NOF_FLOW_HASH_TABLES
#define EIP207_GLOBAL_MAX_HW_NOF_FLOW_HASH_TABLES   17
#endif

// Flow LookUp Engine (flue) lookup mode
// 0 - legacy single-entry mode
// 1 - 3-entry bucket-based mode
#ifndef EIP207_GLOBAL_FLUE_LOOKUP_MODE
// Default is legacy lookup mode
#define EIP207_GLOBAL_FLUE_LOOKUP_MODE                     0
#endif

// Strict argument checking for the input parameters
// If required then define this parameter in the top-level configuration
//#define EIP207_GLOBAL_STRICT_ARGS

// Finite state Machine that can be used for verifying that the Driver Library
// API is called in a correct order
//#define EIP207_GLOBAL_DEBUG_FSM


#endif /* C_EIP207_GLOBAL_H_ */


/* end of file c_eip207_global.h */
