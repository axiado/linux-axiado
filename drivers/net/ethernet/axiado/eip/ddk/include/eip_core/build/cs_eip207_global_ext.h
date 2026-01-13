/* cs_eip207_global_ext.h
 *
 * Top-level configuration parameters extensions
 * for the EIP-207 Global Control
 *
 */

/*****************************************************************************
* Copyright (c) 2012-2020 by Rambus, Inc. and/or its subsidiaries.
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

#ifndef CS_EIP207_GLOBAL_EXT_H_
#define CS_EIP207_GLOBAL_EXT_H_


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */


/*----------------------------------------------------------------------------
 * Definitions and macros
 */

#define EIP207_FLUE_HAVE_VIRTUALIZATION

// number of interfaces for virtualization.
#define EIP207_FLUE_MAX_NOF_INTERFACES_TO_USE 15

// Read/Write register constants

/*****************************************************************************
 * byte offsets of the EIP-207 registers
 *****************************************************************************/


// EIP-207c Classification Engine n (n - number of the CE)
// Input Side

// EIP-207c Classification Engine n (n - number of the CE)
// Input Side

#define EIP207_ICE_REG_SCRATCH_BASE    0xA0800
#define EIP207_ICE_REG_ADAPT_CTRL_BASE 0xA0C00
#define EIP207_ICE_REG_PUE_CTRL_BASE   0xA0C80
#define EIP207_ICE_REG_PUTF_CTRL_BASE  0xA0D00
#define EIP207_ICE_REG_FPP_CTRL_BASE   0xA0D80
#define EIP207_ICE_REG_PPTF_CTRL_BASE  0xA0E00
#define EIP207_ICE_REG_RAM_CTRL_BASE   0xA0FF0

// EIP-207c Classification Engine n (n - number of the CE)
// Output Side

#define EIP207_OCE_REG_SCRATCH_BASE    0xA1400
#define EIP207_OCE_REG_ADAPT_CTRL_BASE 0xA1860
#define EIP207_OCE_REG_PUE_CTRL_BASE   0xA1880
#define EIP207_OCE_REG_PUTF_CTRL_BASE  0xA1900
#define EIP207_OCE_REG_FPP_CTRL_BASE   0xA1980
#define EIP207_OCE_REG_PPTF_CTRL_BASE  0xA1A00
#define EIP207_OCE_REG_RAM_CTRL_BASE   0xA1BF0

// Output Side: reserved

// Access space for Classification RAM, base offset
#define EIP207_CS_RAM_XS_SPACE_BASE    0xE0000

// EIP-207s Classification Support module (p - number of the cache set)

// General Record Cache (frc/trc/arc4_rc) register interface
#define EIP207_FRC_REG_BASE            0xF0000
#define EIP207_TRC_REG_BASE            0xF0800
#define EIP207_ARC4RC_REG_BASE         0xF1000

// EIP-207s Classification Support, Flow Hash Engine (FHASH)
#define EIP207_FHASH_REG_BASE          0xF68C0

// EIP-207s Classification Support, Flow Look-Up Engine (flue), VM-specific
#define EIP207_FLUE_CONFIG_REG_BASE     0xF6010

// EIP-207s Classification Support, Flow Look-Up Engine (flue)
#define EIP207_FLUE_REG_BASE            0xF6808

#ifdef EIP207_FLUE_HAVE_VIRTUALIZATION
#define EIP207_FLUE_IFC_LUT_REG_BASE    0xF6820
#endif

#define EIP207_FLUE_ENABLED_REG_BASE    0xF6840

// EIP-207s Classification Support, Flow Look-Up Engine Cache (FLUEC) Control
#define EIP207_FLUEC_REG_BASE           0xF6880

// Flow Look-Up Engine Cache (FLUEC) Control
#define EIP207_FLUEC_REG_INV_BASE       0xF688C

// EIP-207s Classification Support, DMA Control
#define EIP207_CS_DMA_REG_BASE          0xF7000

// EIP-207s Classification Support, options and versions
#define EIP207_CS_REG_BASE              0xF7FF0

// size of the Flow Record Cache (administration RAM) in 32-bit words
#define EIP207_FRC_ADMIN_RAM_WORD_COUNT             320

// size of the Flow Record Cache (data RAM) in 32-bit words
#define EIP207_FRC_RAM_WORD_COUNT                   768


// size of the Transform Record Cache (administration RAM) in 32-bit words
// 80 * 4 = 320 32-bit words for EIP-197 configurations b and c
// 320 * 4 = 1280 32-bit words for EIP-197 configurations d and e
//
// size of the Transform Record Cache (data RAM) in 32-bit words
// 480 * 8 = 3840 32-bit words for EIP-197 configurations b and c
// 1920 * 4 = 15360 32-bit words for EIP-197 configurations d and e
#if EIP207_MAX_NOF_PE_TO_USE > 1
#define EIP207_TRC_ADMIN_RAM_WORD_COUNT             16384
#define EIP207_TRC_RAM_WORD_COUNT                   131072
#else
#define EIP207_TRC_ADMIN_RAM_WORD_COUNT             3840
#define EIP207_TRC_RAM_WORD_COUNT                   15360
#endif

// size of the arc4 Record Cache (administration RAM) in 32-bit words
#define EIP207_ARC4RC_ADMIN_RAM_WORD_COUNT          0

// size of the arc4 Record Cache (data RAM) in 32-bit words
#define EIP207_ARC4RC_RAM_WORD_COUNT                0

// Input Pull-Up Engine Program RAM size (in 32-bit words), 16KB
#define EIP207_IPUE_PROG_RAM_WORD_COUNT             4096

// Input Flow Post-Processor Engine Program RAM size (in 32-bit words), 16KB
#define EIP207_IFPP_PROG_RAM_WORD_COUNT             4096

// Output Pull-Up Engine Program RAM size (in 32-bit words), 16KB
#define EIP207_OPUE_PROG_RAM_WORD_COUNT             4096

// Output Flow Post-Processor Engine Program RAM size (in 32-bit words), 16KB
#define EIP207_OFPP_PROG_RAM_WORD_COUNT             4096


#endif /* CS_EIP207_GLOBAL_EXT_H_ */


/* end of file cs_eip207_global_ext.h */
