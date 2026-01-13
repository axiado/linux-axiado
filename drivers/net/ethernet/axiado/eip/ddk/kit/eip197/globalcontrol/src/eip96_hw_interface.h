/* eip96_hw_interface.h
 *
 * EIP-96 Packet Engine HW interface
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

#ifndef EIP96_HW_INTERFACE_H_
#define EIP96_HW_INTERFACE_H_


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Default configuration
#include "c_eip96.h"

// Driver Framework Basic Definitions API
#include "basic_defs.h"         // BIT definitions


/*----------------------------------------------------------------------------
 * Definitions and macros
 */

// internal Packet Engine time-out – a fatal error requiring a complete reset.
#define EIP96_TIMEOUT_FATAL_ERROR_MASK     BIT_14

// Read/Write register constants

/*****************************************************************************
 * byte offsets of the EIP-96 Packet Engine registers
 *****************************************************************************/
#define EIP96_REG_OFFS                     4
#define EIP96_REG_MAP_SIZE                 8192

// Processing Packet Engine n (n - number of the DSE thread)
#define EIP96_REG_TOKEN_CTRL_STAT(n)       ((EIP96_REG_MAP_SIZE * n) + \
                                            ((EIP96_CONF_BASE) + \
                                             (0x00 * EIP96_REG_OFFS)))
#define EIP96_REG_FUNCTION_EN(n)           ((EIP96_REG_MAP_SIZE * n) + \
                                            ((EIP96_CONF_BASE) + \
                                             (0x01 * EIP96_REG_OFFS)))
#define EIP96_REG_CONTEXT_CTRL(n)          ((EIP96_REG_MAP_SIZE * n) + \
                                            ((EIP96_CONF_BASE) + \
                                             (0x02 * EIP96_REG_OFFS)))
#define EIP96_REG_CONTEXT_STAT(n)          ((EIP96_REG_MAP_SIZE * n) + \
                                            ((EIP96_CONF_BASE) + \
                                             (0x03 * EIP96_REG_OFFS)))

#define EIP96_REG_OUT_TRANS_CTRL_STAT(n)   ((EIP96_REG_MAP_SIZE * n) + \
                                            ((EIP96_CONF_BASE) + \
                                             (0x06 * EIP96_REG_OFFS)))
#define EIP96_REG_OUT_BUF_CTRL(n)          ((EIP96_REG_MAP_SIZE * n) + \
                                            ((EIP96_CONF_BASE) + \
                                             (0x07 * EIP96_REG_OFFS)))
#define EIP96_REG_CTX_NUM32_THR(n)         ((EIP96_REG_MAP_SIZE * n) + \
                                            ((EIP96_CONF_BASE) + \
                                             (0x08 * EIP96_REG_OFFS)))
#define EIP96_REG_CTX_NUM64_THR_L(n)       ((EIP96_REG_MAP_SIZE * n) + \
                                            ((EIP96_CONF_BASE) + \
                                             (0x09 * EIP96_REG_OFFS)))
#define EIP96_REG_CTX_NUM64_THR_H(n)       ((EIP96_REG_MAP_SIZE * n) + \
                                            ((EIP96_CONF_BASE) + \
                                             (0x0a * EIP96_REG_OFFS)))
#define EIP96_REG_TOKEN_CTRL2(n)           ((EIP96_REG_MAP_SIZE * n) + \
                                            ((EIP96_CONF_BASE) + \
                                             (0x0b * EIP96_REG_OFFS)))

// EIP-96 PRNG
#define EIP96_REG_PRNG_STAT(n)             ((EIP96_REG_MAP_SIZE * n) + \
                                            ((EIP96_PRNG_BASE) + \
                                             (0x00 * EIP96_REG_OFFS)))
#define EIP96_REG_PRNG_CTRL(n)             ((EIP96_REG_MAP_SIZE * n) + \
                                            ((EIP96_PRNG_BASE) + \
                                             (0x01 * EIP96_REG_OFFS)))
#define EIP96_REG_PRNG_SEED_L(n)           ((EIP96_REG_MAP_SIZE * n) + \
                                            ((EIP96_PRNG_BASE) + \
                                             (0x02 * EIP96_REG_OFFS)))
#define EIP96_REG_PRNG_SEED_H(n)           ((EIP96_REG_MAP_SIZE * n) + \
                                            ((EIP96_PRNG_BASE) + \
                                             (0x03 * EIP96_REG_OFFS)))
#define EIP96_REG_PRNG_KEY_0_L(n)          ((EIP96_REG_MAP_SIZE * n) + \
                                            ((EIP96_PRNG_BASE) + \
                                             (0x04 * EIP96_REG_OFFS)))
#define EIP96_REG_PRNG_KEY_0_H(n)          ((EIP96_REG_MAP_SIZE * n) + \
                                            ((EIP96_PRNG_BASE) + \
                                             (0x05 * EIP96_REG_OFFS)))
#define EIP96_REG_PRNG_KEY_1_L(n)          ((EIP96_REG_MAP_SIZE * n) + \
                                             ((EIP96_PRNG_BASE) + \
                                              (0x06 * EIP96_REG_OFFS)))
#define EIP96_REG_PRNG_KEY_1_H(n)          ((EIP96_REG_MAP_SIZE * n) + \
                                            ((EIP96_PRNG_BASE) + \
                                             (0x07 * EIP96_REG_OFFS)))
#define EIP96_REG_PRNG_LFSR_L(n)           ((EIP96_REG_MAP_SIZE * n) + \
                                            ((EIP96_PRNG_BASE) + \
                                             (0x0c * EIP96_REG_OFFS)))
#define EIP96_REG_PRNG_LFSR_H(n)           ((EIP96_REG_MAP_SIZE * n) + \
                                            ((EIP96_PRNG_BASE) + \
                                             (0x0d * EIP96_REG_OFFS)))

#define EIP96_REG_ECN_TABLE(n,k)           ((EIP96_REG_MAP_SIZE * n) + \
                                            ((EIP96_CONF_BASE) + 0x3e0 + 4*(k)))

// EIP-96 options and Version
// New registers to must still be added to the HW,
// do not use these registers yet
#define EIP96_REG_OPTIONS(n)               ((EIP96_REG_MAP_SIZE * n) + \
                                            ((EIP96_VER_BASE) + \
                                             (0x00 * EIP96_REG_OFFS)))
#define EIP96_REG_VERSION(n)               ((EIP96_REG_MAP_SIZE * n) + \
                                            ((EIP96_VER_BASE) + \
                                             (0x01 * EIP96_REG_OFFS)))

// Default EIP96_REG_TOKEN_CTRL_STAT register value
#define EIP96_REG_TOKEN_CTRL_STAT_DEFAULT   0x00004004

// Default EIP96_REG_PRNG_CTRL register value
#define EIP96_REG_PRNG_CTRL_DEFAULT         0x00000000

// Default EIP96_REG_OUT_BUF_CTRL register value
#define EIP96_REG_OUT_BUF_CTRL_DEFAULT      0x00000000


#endif /* EIP96_HW_INTERFACE_H_ */


/* end of file eip96_hw_interface.h */
