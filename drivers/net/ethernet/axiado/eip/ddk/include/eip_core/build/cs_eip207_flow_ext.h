/* cs_eip207_flow_ext.h
 *
 * Top-level configuration parameters extensions
 * for the EIP-207 Flow Control Driver Library
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

#ifndef CS_EIP207_FLOW_EXT_H_
#define CS_EIP207_FLOW_EXT_H_


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */


/*----------------------------------------------------------------------------
 * Definitions and macros
 */


// Read/Write register constants


/*****************************************************************************
 * byte offsets of the EIP-207 HIA registers
 *****************************************************************************/


// Direct Transform Lookup (DTL) hardware is present
#define EIP207_FLOW_HAVE_DTL

// General configuration and version, in-flight counters
#define EIP207_REG_IN_FLIGHT_BASE           0xA1FF0

// Flow Hash Engine (FHASH)
#define EIP207_FHASH_REG_BASE               0xF68C0

// Flow Look-Up Engine (flue) register bank size
#define EIP207_FLUE_FHT_REG_MAP_SIZE        8192

// EIP-207s Classification Support, flue CACHEBASE registers base offset
#define EIP207_FLUE_FHT1_REG_BASE           0x00000

// EIP-207s Classification Support,
// Flow Hash Engine (FHASH) registers base offset
#define EIP207_FLUE_HASH_REG_BASE           0x00100

// EIP-207s Classification Support, flue HASHBASE registers base offset
#define EIP207_FLUE_FHT2_REG_BASE           0x00008

// EIP-207s Classification Support, flue SIZE register base offset
#define EIP207_FLUE_FHT3_REG_BASE           0x00010

// EIP-207s Classification Support, options and version registers base offset
#define EIP207_FLUE_OPTVER_REG_BASE         0x01FF8

// EIP-207s Classification Support, flue Record Cache type - High-Performance
#define EIP207_FLUE_RC_HP


#endif /* CS_EIP207_FLOW_EXT_H_ */


/* end of file cs_eip207_flow_ext.h */
