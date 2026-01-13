/* eip207_hw_interface.h
 *
 * EIP-207 HW interface
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

#ifndef EIP207_HW_INTERFACE_H_
#define EIP207_HW_INTERFACE_H_


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Default configuration
#include "c_eip207_global.h"

// Driver Framework Basic Definitions API
#include "basic_defs.h"         // u16


/*----------------------------------------------------------------------------
 * Definitions and macros
 */

// Read/Write register constants

/*****************************************************************************
 * byte offsets of the EIP-207 HIA registers
 *****************************************************************************/
// EIP-207c ice EIP number (0xCF) and complement (0x30)
#define EIP207_ICE_SIGNATURE            ((u16)0x30CF)

#define EIP207_REG_OFFS                 4
#define EIP207_REG_MAP_SIZE             8192

// EIP-207c Classification Engine n (n - number of the CE)
// Input Side

#define EIP207_ICE_REG_SCRATCH_RAM(n)  ((EIP207_REG_MAP_SIZE * n) + \
                                        ((EIP207_ICE_REG_SCRATCH_BASE) + \
                                         (0x00 * EIP207_REG_OFFS)))

#define EIP207_ICE_REG_ADAPT_CTRL(n)   ((EIP207_REG_MAP_SIZE * n) + \
                                        ((EIP207_ICE_REG_ADAPT_CTRL_BASE) + \
                                         (0x00 * EIP207_REG_OFFS)))

#define EIP207_ICE_REG_PUE_CTRL(n)     ((EIP207_REG_MAP_SIZE * n) + \
                                        ((EIP207_ICE_REG_PUE_CTRL_BASE) + \
                                         (0x00 * EIP207_REG_OFFS)))
#define EIP207_ICE_REG_PUE_DEBUG(n)    ((EIP207_REG_MAP_SIZE * n) + \
                                        ((EIP207_ICE_REG_PUE_CTRL_BASE) + \
                                         (0x01 * EIP207_REG_OFFS)))

#define EIP207_ICE_REG_PUTF_CTRL(n)    ((EIP207_REG_MAP_SIZE * n) + \
                                        ((EIP207_ICE_REG_PUTF_CTRL_BASE) + \
                                         (0x00 * EIP207_REG_OFFS)))
#define EIP207_ICE_REG_SCRATCH_CTRL(n) ((EIP207_REG_MAP_SIZE * n) + \
                                        ((EIP207_ICE_REG_PUTF_CTRL_BASE) + \
                                         (0x01 * EIP207_REG_OFFS)))
#define EIP207_ICE_REG_TIMER_LO(n)     ((EIP207_REG_MAP_SIZE * n) + \
                                        ((EIP207_ICE_REG_PUTF_CTRL_BASE) + \
                                         (0x02 * EIP207_REG_OFFS)))
#define EIP207_ICE_REG_TIMER_HI(n)     ((EIP207_REG_MAP_SIZE * n) + \
                                        ((EIP207_ICE_REG_PUTF_CTRL_BASE) + \
                                         (0x03 * EIP207_REG_OFFS)))
#define EIP207_ICE_REG_UENG_STAT(n)    ((EIP207_REG_MAP_SIZE * n) + \
                                        ((EIP207_ICE_REG_PUTF_CTRL_BASE) + \
                                         (0x04 * EIP207_REG_OFFS)))

#define EIP207_ICE_REG_FPP_CTRL(n)     ((EIP207_REG_MAP_SIZE * n) + \
                                        ((EIP207_ICE_REG_FPP_CTRL_BASE) + \
                                         (0x00 * EIP207_REG_OFFS)))
#define EIP207_ICE_REG_FPP_DEBUG(n)    ((EIP207_REG_MAP_SIZE * n) + \
                                        ((EIP207_ICE_REG_FPP_CTRL_BASE) + \
                                         (0x01 * EIP207_REG_OFFS)))

#define EIP207_ICE_REG_PPTF_CTRL(n)    ((EIP207_REG_MAP_SIZE * n) + \
                                        ((EIP207_ICE_REG_PPTF_CTRL_BASE) + \
                                         (0x00 * EIP207_REG_OFFS)))

#define EIP207_ICE_REG_RAM_CTRL(n)      ((EIP207_REG_MAP_SIZE * n) + \
                                         ((EIP207_ICE_REG_RAM_CTRL_BASE) + \
                                          (0x00 * EIP207_REG_OFFS)))
#define EIP207_ICE_REG_RAM_CTRL_RSV1(n) ((EIP207_REG_MAP_SIZE * n) + \
                                         ((EIP207_ICE_REG_RAM_CTRL_BASE) + \
                                          (0x01 * EIP207_REG_OFFS)))
#define EIP207_ICE_REG_OPTIONS(n)       ((EIP207_REG_MAP_SIZE * n) + \
                                         ((EIP207_ICE_REG_RAM_CTRL_BASE) + \
                                          (0x02 * EIP207_REG_OFFS)))
#define EIP207_ICE_REG_VERSION(n)       ((EIP207_REG_MAP_SIZE * n) + \
                                         ((EIP207_ICE_REG_RAM_CTRL_BASE) + \
                                          (0x03 * EIP207_REG_OFFS)))

// Access space for Classification RAM (size 64 KB)
#define EIP207_CS_RAM_XS_SPACE_WORD_COUNT  16384

// EIP-207s Classification Support module (p - number of the cache set)

// General Record Cache (frc/trc/arc4_rc) register interface
#define EIP207_RC_REG_DATA_BYTE_OFFSET    0x400
#define EIP207_FRC_REG_DATA_BYTE_OFFSET   EIP207_RC_REG_DATA_BYTE_OFFSET
#define EIP207_TRC_REG_DATA_BYTE_OFFSET   EIP207_RC_REG_DATA_BYTE_OFFSET

#define EIP207_RC_HDR_WORD_3_RELOAD_BIT   BIT_15

#define EIP207_RC_REG_CTRL(base,p)      ((EIP207_REG_MAP_SIZE * p) + \
                                         (base + \
                                          (0x00 * EIP207_REG_OFFS)))
#define EIP207_RC_REG_LASTRES(base,p)   ((EIP207_REG_MAP_SIZE * p) + \
                                         (base + \
                                          (0x01 * EIP207_REG_OFFS)))

#define EIP207_RC_REG_PARAMS_BASE       0x00020

// RC RAM (top 1036 bytes are accessible)
#define EIP207_RC_REG_DATA_WORD_COUNT   259

#define EIP207_RC_REG_DATA_BASE       0x00000
#define EIP207_RC_REG_DATA(base,p)    ((EIP207_REG_MAP_SIZE * p) + \
                                       ((base + EIP207_RC_REG_DATA_BASE) + \
                                        (0x00 * EIP207_REG_OFFS)))

// EIP-207s Classification Support, Flow Hash Engine (FHASH)
#define EIP207_FHASH_REG_IV_0           ((EIP207_FHASH_REG_BASE) + \
                                          (0x00 * EIP207_REG_OFFS))
#define EIP207_FHASH_REG_IV_1           ((EIP207_FHASH_REG_BASE) + \
                                          (0x01 * EIP207_REG_OFFS))
#define EIP207_FHASH_REG_IV_2           ((EIP207_FHASH_REG_BASE) + \
                                          (0x02 * EIP207_REG_OFFS))
#define EIP207_FHASH_REG_IV_3           ((EIP207_FHASH_REG_BASE) + \
                                          (0x03 * EIP207_REG_OFFS))

// EIP-207s Classification Support, DMA Control
#define EIP207_CS_DMA_REG_RAM_CTRL      ((EIP207_CS_DMA_REG_BASE) + \
                                         (0x00 * EIP207_REG_OFFS))
#define EIP207_CS_DMA_REG_RAM_CTRL2     ((EIP207_CS_DMA_REG_BASE) + \
                                         (0x01 * EIP207_REG_OFFS))

// EIP-207s Classification Support, options and versions
#define EIP207_CS_REG_RAM_CTRL          ((EIP207_CS_REG_BASE) + \
                                         (0x00 * EIP207_REG_OFFS))
#define EIP207_CS_REG_RESERVED_1        ((EIP207_CS_REG_BASE) + \
                                         (0x01 * EIP207_REG_OFFS))
#define EIP207_CS_REG_OPTIONS           ((EIP207_CS_REG_BASE) + \
                                         (0x02 * EIP207_REG_OFFS))
#define EIP207_CS_REG_VERSION           ((EIP207_CS_REG_BASE) + \
                                         (0x03 * EIP207_REG_OFFS))

// EIP-207s Classification Support EIP number (0xCF) and complement (0x30)
#define EIP207_CS_SIGNATURE             EIP207_ICE_SIGNATURE

// Register default value
#define EIP207_CS_REG_RAM_CTRL_DEFAULT      0x00000000

#define EIP207_ICE_REG_RAM_CTRL_DEFAULT     0x00000000
#define EIP207_ICE_REG_PUE_CTRL_DEFAULT     0x00000001
#define EIP207_ICE_REG_FPP_CTRL_DEFAULT     0x00000001
#define EIP207_ICE_REG_SCRATCH_CTRL_DEFAULT 0x001F0200

#define EIP207_RC_REG_CTRL_DEFAULT          0x00000007
#define EIP207_RC_REG_FREECHAIN_DEFAULT     0x00000000

#define EIP207_FLUE_REG_OFFSETS_DEFAULT     0x00000000
#define EIP207_FLUE_REG_ARC4_OFFSET_DEFAULT 0x00000000


// EIP-207 HW interface extensions
#include "eip207_hw_interface_ext.h"

// EIP-207 Record Cache HW interface extensions
#include "eip207_rc_hw_interface_ext.h"


#endif /* EIP207_HW_INTERFACE_H_ */


/* end of file eip207_hw_interface.h */
