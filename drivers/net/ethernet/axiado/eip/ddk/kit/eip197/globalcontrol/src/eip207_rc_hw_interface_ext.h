/* eip207_rc_hw_interface_ext.h
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

#ifndef EIP207_RC_HW_INTERFACE_EXT_H_
#define EIP207_RC_HW_INTERFACE_EXT_H_


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */


/*----------------------------------------------------------------------------
 * Definitions and macros
 */

// Invalid (dummy) address
#define EIP207_RC_RECORD_DUMMY_ADDRESS    0


// Read/Write register constants

// EIP-207s Classification Support module (p - number of the cache set)
// Record Cache (frc/trc/arc4_rc) register interface
#define EIP207_RC_REG_REGINDEX(base,p)  ((EIP207_REG_MAP_SIZE * p) + \
                                         (base + \
                                          (0x00 * EIP207_REG_OFFS)))
#define EIP207_RC_REG_PARAMS(base,p)    ((EIP207_REG_MAP_SIZE * p) + \
                                         ((base + EIP207_RC_REG_PARAMS_BASE) + \
                                          (0x00 * EIP207_REG_OFFS)))
#define EIP207_RC_REG_FREECHAIN(base,p) ((EIP207_REG_MAP_SIZE * p) + \
                                         ((base + EIP207_RC_REG_PARAMS_BASE) + \
                                          (0x01 * EIP207_REG_OFFS)))
#define EIP207_RC_REG_PARAMS2(base,p)   ((EIP207_REG_MAP_SIZE * p) + \
                                         ((base + EIP207_RC_REG_PARAMS_BASE) + \
                                          (0x02 * EIP207_REG_OFFS)))
#define EIP207_RC_REG_ECCCTRL(base,p)   ((EIP207_REG_MAP_SIZE * p) + \
                                         ((base + EIP207_RC_REG_PARAMS_BASE) + \
                                          (0x04 * EIP207_REG_OFFS)))

// Debug counter registers (40 bit each, 2 consecutive words)
#define EIP207_RC_REG_PREFEXEC(base,p) ((EIP207_REG_MAP_SIZE * p) + base + 0x80)
#define EIP207_RC_REG_PREFBLCK(base,p) ((EIP207_REG_MAP_SIZE * p) + base + 0x88)
#define EIP207_RC_REG_PREFDMA(base,p) ((EIP207_REG_MAP_SIZE * p) + base + 0x90)
#define EIP207_RC_REG_SELOPS(base,p)  ((EIP207_REG_MAP_SIZE * p) + base + 0x98)
#define EIP207_RC_REG_SELDMA(base,p)  ((EIP207_REG_MAP_SIZE * p) + base + 0xA0)
#define EIP207_RC_REG_IDMAWR(base,p)  ((EIP207_REG_MAP_SIZE * p) + base + 0xA8)
#define EIP207_RC_REG_XDMAWR(base,p)  ((EIP207_REG_MAP_SIZE * p) + base + 0xB0)
#define EIP207_RC_REG_INVCMD(base,p)  ((EIP207_REG_MAP_SIZE * p) + base + 0xB8)
// Debug register, DMA read error status
#define EIP207_RC_REG_RDMAERRFLGS_0(base,p) \
    ((EIP207_REG_MAP_SIZE * p) + base + 0xD0)
// Debug counter registers, (24 bits each)
#define EIP207_RC_REG_RDMAERR(base,p) ((EIP207_REG_MAP_SIZE * p) + base + 0xE0)
#define EIP207_RC_REG_WDMAERR(base,p) ((EIP207_REG_MAP_SIZE * p) + base + 0xE4)
#define EIP207_RC_REG_INVECC(base,p)  ((EIP207_REG_MAP_SIZE * p) + base + 0xF0)
#define EIP207_RC_REG_DATECC_CORR(base,p) ((EIP207_REG_MAP_SIZE * p) + base + 0xF4)
#define EIP207_RC_REG_ADMECC_CORR(base,p) ((EIP207_REG_MAP_SIZE * p) + base + 0xE8)



// Register default value
#define EIP207_RC_REG_PARAMS_DEFAULT        0x00200401
#define EIP207_RC_REG_PARAMS2_DEFAULT       0x00000000
#define EIP207_RC_REG_REGINDEX_DEFAULT      0x00000000
#define EIP207_RC_REG_ECCCRTL_DEFAULT       0x00000000




#endif /* EIP207_RC_HW_INTERFACE_EXT_H_ */


/* end of file eip207_rc_hw_interface_ext.h */
