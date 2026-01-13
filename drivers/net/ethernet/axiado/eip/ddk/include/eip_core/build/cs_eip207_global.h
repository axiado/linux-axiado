/* cs_eip207_global.h
 *
 * Top-level configuration parameters
 * for the EIP-207 Global Control
 *
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

#ifndef CS_EIP207_GLOBAL_H_
#define CS_EIP207_GLOBAL_H_


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Top-level configuration
#include "cs_adapter.h"


/*----------------------------------------------------------------------------
 * Definitions and macros
 */

#define EIP207_MAX_NOF_PE_TO_USE DRIVER_MAX_NOF_PE_TO_USE

// Maximum number of EIP-207c Classification Engines that can be used
#ifdef ADAPTER_CS_GLOBAL_MAX_NOF_CE_TO_USE
#define EIP207_GLOBAL_MAX_NOF_CE_TO_USE  ADAPTER_CS_GLOBAL_MAX_NOF_CE_TO_USE
#else
#define EIP207_GLOBAL_MAX_NOF_CE_TO_USE            1
#endif

// Maximum supported number of flow hash tables
#ifdef ADAPTER_CS_MAX_NOF_FLOW_HASH_TABLES_TO_USE
#define EIP207_MAX_NOF_FLOW_HASH_TABLES_TO_USE \
                           ADAPTER_CS_MAX_NOF_FLOW_HASH_TABLES_TO_USE
#else
#define EIP207_MAX_NOF_FLOW_HASH_TABLES_TO_USE     1
#endif

// Maximum supported number of frc/trc/arc4 cache sets
#define EIP207_GLOBAL_MAX_NOF_CACHE_SETS_TO_USE    1

// Enable firmware version check after the download
#define EIP207_GLOBAL_FIRMWARE_DOWNLOAD_VERSION_CHECK

// ice Scratchpad RAM size in 128-byte blocks (512 bytes)
#define EIP207_ICE_SCRATCH_RAM_128B_BLOCK_COUNT         4

// oce Scratchpad RAM size in 128-byte blocks (512 bytes)
#define EIP207_OCE_SCRATCH_RAM_128B_BLOCK_COUNT         4

// Disable clustered write operations, e.g. every write operation to
// an EIP-207 register will be followed by one read operation to
// a pre-defined EIP-207 register
#define EIP207_CLUSTERED_WRITES_DISABLE

// Strict argument checking for the input parameters
// If required then define this parameter in the top-level configuration
#define EIP207_GLOBAL_STRICT_ARGS

// Finite state Machine that can be used for verifying that the Driver Library
// API is called in a correct order
#define EIP207_GLOBAL_DEBUG_FSM


// configuration parameter extensions
#include "cs_eip207_global_ext.h"

// configuration parameter extensions
#include "cs_eip207_global_ext2.h"


#endif /* CS_EIP207_GLOBAL_H_ */


/* end of file cs_eip207_global.h */
