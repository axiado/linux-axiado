/* eip202_global_hw_interface.h
 *
 * EIP-202 HIA Global Control HW internal interface
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

#ifndef EIP202_GLOBAL_HW_INTERFACE_H_
#define EIP202_GLOBAL_HW_INTERFACE_H_


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Default configuration
#include "c_eip202_global.h"

// Driver Framework Basic Definitions API
#include "basic_defs.h"         // u16


/*----------------------------------------------------------------------------
 * Definitions and macros
 */

#define EIP202_DFE_TRD_REG_STAT_IDLE    0xF

// Read/Write register constants

/*****************************************************************************
 * byte offsets of the EIP-202 HIA registers
 *****************************************************************************/
// EIP-202 HIA EIP number (0xCA) and complement (0x35)
#define EIP202_SIGNATURE                ((u16)0x35CA)

#define EIP202_REG_OFFS                 4

#define EIP202_FE_REG_MAP_SIZE          128

// HIA Look-aside fifo base offset
#define EIP202_LASIDE_BASE              0x9FF00
#define EIP202_REG_LASIDE_MAP_SIZE      8

// HIA Inline fifo base offset
#define EIP202_INLINE_BASE              0x9FF80
#define EIP202_REG_INLINE_MAP_SIZE      4

// HIA Ring Arbiter map size
#define EIP202_REG_RA_MAP_SIZE          8


// HIA Ring Arbiter
#define EIP202_RA_REG_PRIO_0     ((EIP202_RA_BASE)+(0x00 * EIP202_REG_OFFS))
#define EIP202_RA_REG_PRIO_1     ((EIP202_RA_BASE)+(0x01 * EIP202_REG_OFFS))
#define EIP202_RA_REG_PRIO_2     ((EIP202_RA_BASE)+(0x02 * EIP202_REG_OFFS))
#define EIP202_RA_REG_PRIO_3     ((EIP202_RA_BASE)+(0x03 * EIP202_REG_OFFS))

// HIA Ring Arbiter control for PE n (n - number of the PE)
#define EIP202_RA_PE_REG_CTRL(n)   ((EIP202_REG_RA_MAP_SIZE * n) + \
                                      ((EIP202_RA_BASE) + \
                                       (0x04 * EIP202_REG_OFFS)))

// HIA DFE all threads
#define EIP202_DFE_REG_CFG(n)        ((EIP202_FE_REG_MAP_SIZE * n) + \
                                      EIP202_DFE_BASE + \
                                       (0x00 * EIP202_REG_OFFS))

// HIA DFE thread n (n - number of the DFE thread)
#define EIP202_DFE_TRD_REG_CTRL(n)   ((EIP202_FE_REG_MAP_SIZE * n) + \
                                      ((EIP202_DFE_TRD_BASE) + \
                                       (0x00 * EIP202_REG_OFFS)))
#define EIP202_DFE_TRD_REG_STAT(n)   ((EIP202_FE_REG_MAP_SIZE * n) + \
                                      ((EIP202_DFE_TRD_BASE) + \
                                       (0x01 * EIP202_REG_OFFS)))

// HIA DSE all threads
#define EIP202_DSE_REG_CFG(n)        ((EIP202_FE_REG_MAP_SIZE * n) + \
                                      (EIP202_DSE_BASE) + \
                                       (0x00 * EIP202_REG_OFFS))

// HIA DSE thread n (n - number of the DSE thread)
#define EIP202_DSE_TRD_REG_CTRL(n)   ((EIP202_FE_REG_MAP_SIZE * n) + \
                                      ((EIP202_DSE_TRD_BASE) + \
                                       (0x00 * EIP202_REG_OFFS)))
#define EIP202_DSE_TRD_REG_STAT(n)   ((EIP202_FE_REG_MAP_SIZE * n) + \
                                      ((EIP202_DSE_TRD_BASE) + \
                                       (0x01 * EIP202_REG_OFFS)))


// HIA Global
#define EIP202_G_REG_OPTIONS2     ((EIP202_G_BASE)+(0x00 * EIP202_REG_OFFS))
#define EIP202_G_REG_MST_CTRL     ((EIP202_G_BASE)+(0x01 * EIP202_REG_OFFS))
#define EIP202_G_REG_OPTIONS      ((EIP202_G_BASE)+(0x02 * EIP202_REG_OFFS))
#define EIP202_G_REG_VERSION      ((EIP202_G_BASE)+(0x03 * EIP202_REG_OFFS))

// HIA Look-aside (LA) fifo, k - LA fifo number, must be from 1 to 5
#define EIP202_REG_LASIDE_BASE_ADDR_LO      ((EIP202_LASIDE_BASE) + \
                                                (0x00 * EIP202_REG_OFFS))
#define EIP202_REG_LASIDE_BASE_ADDR_HI      ((EIP202_LASIDE_BASE) + \
                                                (0x01 * EIP202_REG_OFFS))
#define EIP202_REG_LASIDE_SLAVE_CTRL(k)  ((EIP202_REG_LASIDE_MAP_SIZE * k) + \
                                           ((EIP202_LASIDE_BASE) + \
                                            (0x00 * EIP202_REG_OFFS)))
#define EIP202_REG_LASIDE_MASTER_CTRL(k) ((EIP202_REG_LASIDE_MAP_SIZE * k) + \
                                           ((EIP202_LASIDE_BASE) + \
                                            (0x01 * EIP202_REG_OFFS)))

// HIA Inline (IN) fifo base offset, l - IN fifo number
#define EIP202_REG_INLINE_CTRL(k)         ((EIP202_REG_INLINE_MAP_SIZE * k) + \
                                           ((EIP202_INLINE_BASE) + \
                                            (0x00 * EIP202_REG_OFFS)))

// Default EIP202_DFE_REG_CFG register value
#define EIP202_DFE_REG_CFG_DEFAULT          0x00000000

// Default EIP202_DFE_TRD_REG_CTRL register value
#define EIP202_DFE_TRD_REG_CTRL_DEFAULT     0x00000000

// Default EIP202_DSE_REG_CFG register value
#define EIP202_DSE_REG_CFG_DEFAULT          0x80008000

// Default EIP202_DSE_TRD_REG_CTRL register value
#define EIP202_DSE_TRD_REG_CTRL_DEFAULT     0x00000000

// Default EIP202_RA_REG_PRIO_x register values
#define EIP202_RA_REG_PRIO_0_DEFAULT        0x00000000
#define EIP202_RA_REG_PRIO_1_DEFAULT        0x00000000
#define EIP202_RA_REG_PRIO_2_DEFAULT        0x00000000
#define EIP202_RA_REG_PRIO_3_DEFAULT        0x00000000
#define EIP202_RA_PE_REG_CTRL_DEFAULT       0x00000000

// Default HIA Look-aside (LA) fifo registers values
#define EIP202_REG_LASIDE_BASE_ADDR_LO_DEFAULT     0x00000000
#define EIP202_REG_LASIDE_BASE_ADDR_HI_DEFAULT     0x00000000
#define EIP202_REG_LASIDE_MASTER_CTRL_DEFAULT      0x00000000
#define EIP202_REG_LASIDE_SLAVE_CTRL_DEFAULT       0x00000000

// Default HIA Inline (IN) fifo registers values
#define EIP202_REG_INLINE_CTRL_DEFAULT             0x00000000

// Default value of the buffer_ctrl field in the EIP202_DSE_REG_CFG register
#define EIP202_DSE_BUFFER_CTRL            ((EIP202_DSE_REG_CFG_DEFAULT >> 14) \
                                           & MASK_2_BITS)


#endif /* EIP202_GLOBAL_HW_INTERFACE_H_ */


/* end of file eip202_global_hw_interface.h */
