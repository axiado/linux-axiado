/* eip206_hw_interface.h
 *
 * EIP-206 Processing Engine HW interface
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

#ifndef EIP206_HW_INTERFACE_H_
#define EIP206_HW_INTERFACE_H_


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Default configuration
#include "c_eip206.h"

// Driver Framework Basic Definitions API
#include "basic_defs.h"         // u16


/*----------------------------------------------------------------------------
 * Definitions and macros
 */

// Read/Write register constants

/*****************************************************************************
 * byte offsets of the EIP-206 Processing Engine registers
 *****************************************************************************/
// Processing Engine EIP number (0xCE) and complement (0x31)
#define EIP206_SIGNATURE              ((u16)0x31CE)

#define EIP206_REG_OFFS               4
#define EIP206_REG_MAP_SIZE           8192

// Processing Packet Engine n (n - number of the PE)
// Input Side
#define EIP206_IN_REG_DBUF_TRESH(n)   ((EIP206_REG_MAP_SIZE * n) + \
                                       ((EIP206_IN_DBUF_BASE) + \
                                        (0x00 * EIP206_REG_OFFS)))

#define EIP206_IN_REG_TBUF_TRESH(n)   ((EIP206_REG_MAP_SIZE * n) + \
                                       ((EIP206_IN_TBUF_BASE) + \
                                        (0x00 * EIP206_REG_OFFS)))

// Output Side
#define EIP206_OUT_REG_DBUF_TRESH(n)  ((EIP206_REG_MAP_SIZE * n) + \
                                       ((EIP206_OUT_DBUF_BASE) + \
                                        (0x00 * EIP206_REG_OFFS)))

#define EIP206_OUT_REG_TBUF_TRESH(n)  ((EIP206_REG_MAP_SIZE * n) + \
                                       ((EIP206_OUT_TBUF_BASE) + \
                                        (0x00 * EIP206_REG_OFFS)))

// PE options and Version
#define EIP206_REG_DEBUG(n)           ((EIP206_REG_MAP_SIZE * n) + \
                                       ((EIP206_VER_BASE) - \
                                        (0x01 * EIP206_REG_OFFS)))
#define EIP206_REG_OPTIONS(n)         ((EIP206_REG_MAP_SIZE * n) + \
                                       ((EIP206_VER_BASE) + \
                                        (0x00 * EIP206_REG_OFFS)))
#define EIP206_REG_VERSION(n)         ((EIP206_REG_MAP_SIZE * n) + \
                                       ((EIP206_VER_BASE) + \
                                        (0x01 * EIP206_REG_OFFS)))

// Default EIP206_IN_REG_DBUF_TRESH register value
#define EIP206_IN_REG_DBUF_TRESH_DEFAULT        0x00000000

// Default EIP206_IN_REG_TBUF_TRESH register value
#define EIP206_IN_REG_TBUF_TRESH_DEFAULT        0x00000000

// Default EIP206_OUT_REG_DBUF_TRESH register value
#define EIP206_OUT_REG_DBUF_TRESH_DEFAULT       0x00000000

// Default EIP206_OUT_REG_TBUF_TRESH register value
#define EIP206_OUT_REG_TBUF_TRESH_DEFAULT       0x00000000


// arc4 size Small/Large
#define EIP206_ARC4_REG_SIZE(n)       ((EIP206_REG_MAP_SIZE * n) + \
                                       ((EIP206_ARC4_BASE) + \
                                        (0x00 * EIP206_REG_OFFS)))


// Default EIP206_ARC4_REG_SIZE_DEFAULT register value
#define EIP206_ARC4_REG_SIZE_DEFAULT            0x00000000

#endif /* EIP206_HW_INTERFACE_H_ */


/* end of file eip206_hw_interface.h */
