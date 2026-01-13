/* eip207_hw_interface_ext.h
 *
 * EIP-207 HW interface extensions
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

#ifndef EIP207_HW_INTERFACE_EXT_H_
#define EIP207_HW_INTERFACE_EXT_H_


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */


/*----------------------------------------------------------------------------
 * Definitions and macros
 */

// VM-specific register sets in 32-byte blocks
#define EIP207_FLUE_VM_REG_MAP_SIZE       32


// Read/Write register constants

// EIP-207s Classification Support, Flow Look-Up Engine (flue), VM-specific
// (f - number of VM)
#define EIP207_FLUE_REG_CONFIG(f)       ((EIP207_FLUE_VM_REG_MAP_SIZE * f) + \
                                         ((EIP207_FLUE_CONFIG_REG_BASE) + \
                                          (0x00 * EIP207_REG_OFFS)))

// EIP-207s Classification Support, Flow Look-Up Engine (flue)
#define EIP207_FLUE_REG_OFFSETS         (EIP207_FLUE_REG_BASE + \
                                          (0x00 * EIP207_REG_OFFS))
#define EIP207_FLUE_REG_ARC4_OFFSET     (EIP207_FLUE_REG_BASE + \
                                          (0x01 * EIP207_REG_OFFS))

#define EIP207_FLUE_REG_ENABLED_LO      (EIP207_FLUE_ENABLED_REG_BASE + \
                                          (0x02 * EIP207_REG_OFFS))
#define EIP207_FLUE_REG_ENABLED_HI      (EIP207_FLUE_ENABLED_REG_BASE + \
                                          (0x03 * EIP207_REG_OFFS))
#define EIP207_FLUE_REG_ERROR_LO        (EIP207_FLUE_ENABLED_REG_BASE + \
                                          (0x04 * EIP207_REG_OFFS))
#define EIP207_FLUE_REG_ERROR_HI        (EIP207_FLUE_ENABLED_REG_BASE + \
                                          (0x05 * EIP207_REG_OFFS))

#define EIP207_FLUE_REG_IFC_LUT(f)       (EIP207_FLUE_IFC_LUT_REG_BASE + \
                                          ((f) * EIP207_REG_OFFS))

// EIP-207c Classification Engine n (n - number of the CE)
// Output Side

#define EIP207_OCE_REG_SCRATCH_RAM(n)  ((EIP207_REG_MAP_SIZE * n) + \
                                        ((EIP207_OCE_REG_SCRATCH_BASE) + \
                                         (0x00 * EIP207_REG_OFFS)))

#define EIP207_OCE_REG_ADAPT_CTRL(n)   ((EIP207_REG_MAP_SIZE * n) + \
                                        ((EIP207_OCE_REG_ADAPT_CTRL_BASE) + \
                                         (0x00 * EIP207_REG_OFFS)))

#define EIP207_OCE_REG_PUE_CTRL(n)     ((EIP207_REG_MAP_SIZE * n) + \
                                        ((EIP207_OCE_REG_PUE_CTRL_BASE) + \
                                         (0x00 * EIP207_REG_OFFS)))
#define EIP207_OCE_REG_PUE_DEBUG(n)    ((EIP207_REG_MAP_SIZE * n) + \
                                        ((EIP207_OCE_REG_PUE_CTRL_BASE) + \
                                         (0x01 * EIP207_REG_OFFS)))

#define EIP207_OCE_REG_PUTF_CTRL(n)    ((EIP207_REG_MAP_SIZE * n) + \
                                        ((EIP207_OCE_REG_PUTF_CTRL_BASE) + \
                                         (0x00 * EIP207_REG_OFFS)))
#define EIP207_OCE_REG_SCRATCH_CTRL(n) ((EIP207_REG_MAP_SIZE * n) + \
                                        ((EIP207_OCE_REG_PUTF_CTRL_BASE) + \
                                         (0x01 * EIP207_REG_OFFS)))
#define EIP207_OCE_REG_TIMER_LO(n)     ((EIP207_REG_MAP_SIZE * n) + \
                                        ((EIP207_OCE_REG_PUTF_CTRL_BASE) + \
                                         (0x02 * EIP207_REG_OFFS)))
#define EIP207_OCE_REG_TIMER_HI(n)     ((EIP207_REG_MAP_SIZE * n) + \
                                        ((EIP207_OCE_REG_PUTF_CTRL_BASE) + \
                                         (0x03 * EIP207_REG_OFFS)))
#define EIP207_OCE_REG_UENG_STAT(n)    ((EIP207_REG_MAP_SIZE * n) + \
                                        ((EIP207_OCE_REG_PUTF_CTRL_BASE) + \
                                         (0x04 * EIP207_REG_OFFS)))

#define EIP207_OCE_REG_FPP_CTRL(n)     ((EIP207_REG_MAP_SIZE * n) + \
                                        ((EIP207_OCE_REG_FPP_CTRL_BASE) + \
                                         (0x00 * EIP207_REG_OFFS)))
#define EIP207_OCE_REG_FPP_DEBUG(n)    ((EIP207_REG_MAP_SIZE * n) + \
                                        ((EIP207_OCE_REG_FPP_CTRL_BASE) + \
                                         (0x01 * EIP207_REG_OFFS)))

#define EIP207_OCE_REG_PPTF_CTRL(n)    ((EIP207_REG_MAP_SIZE * n) + \
                                        ((EIP207_OCE_REG_PPTF_CTRL_BASE) + \
                                         (0x00 * EIP207_REG_OFFS)))

#define EIP207_OCE_REG_RAM_CTRL(n)      ((EIP207_REG_MAP_SIZE * n) + \
                                         ((EIP207_OCE_REG_RAM_CTRL_BASE) + \
                                          (0x00 * EIP207_REG_OFFS)))
#define EIP207_OCE_REG_RAM_CTRL_RSV1(n) ((EIP207_REG_MAP_SIZE * n) + \
                                         ((EIP207_OCE_REG_RAM_CTRL_BASE) + \
                                          (0x01 * EIP207_REG_OFFS)))
#define EIP207_OCE_REG_OPTIONS(n)       ((EIP207_REG_MAP_SIZE * n) + \
                                         ((EIP207_OCE_REG_RAM_CTRL_BASE) + \
                                          (0x02 * EIP207_REG_OFFS)))
#define EIP207_OCE_REG_VERSION(n)       ((EIP207_REG_MAP_SIZE * n) + \
                                         ((EIP207_OCE_REG_RAM_CTRL_BASE) + \
                                          (0x03 * EIP207_REG_OFFS)))

// Output Side: reserved

// Register default value
#define EIP207_FLUE_REG_CONFIG_DEFAULT      0x00000000

#define EIP207_FLUE_REG_IFC_LUT_DEFAULT     0x00000000

#define EIP207_OCE_REG_RAM_CTRL_DEFAULT     0x00000000
#define EIP207_OCE_REG_PUE_CTRL_DEFAULT     0x00000001
#define EIP207_OCE_REG_FPP_CTRL_DEFAULT     0x00000001
#define EIP207_OCE_REG_SCRATCH_CTRL_DEFAULT 0x001F0200


#endif /* EIP207_HW_INTERFACE_EXT_H_ */


/* end of file eip207_hw_interface_ext.h */
