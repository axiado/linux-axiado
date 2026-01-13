/* eip202_ring_types.h
 *
 * EIP-202 Ring Control Driver Library Public Interface:
 * Common type Definitions for CDR and RDR
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

#ifndef INCLUDE_GUARD_EIP202_RING_TYPES_H
#define INCLUDE_GUARD_EIP202_RING_TYPES_H

/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Driver Framework Basic Definitions API
#include "basic_defs.h"             // u32

// Driver Framework DMA Resource API
#include "dmares_types.h"         // types of the DMA resource API


/*----------------------------------------------------------------------------
 * Definitions and macros
 */

// I/O Area size for one CDR or RDR instance
#define EIP202_IOAREA_REQUIRED_SIZE           300


/*----------------------------------------------------------------------------
 * eip202_ring_error_t
 *
 * status (error) code type returned by these API functions
 * See each function "Return value" for details.
 *
 * EIP202_RING_NO_ERROR : successful completion of the call.
 * EIP202_RING_UNSUPPORTED_FEATURE_ERROR : not supported by the device.
 * EIP202_RING_ARGUMENT_ERROR :  invalid argument for a function parameter.
 * EIP202_RING_BUSY_RETRY_LATER : device is busy.
 * EIP202_RING_ILLEGAL_IN_STATE : illegal state transition
 */
typedef enum
{
    EIP202_RING_NO_ERROR = 0,
    EIP202_RING_UNSUPPORTED_FEATURE_ERROR,
    EIP202_RING_ARGUMENT_ERROR,
    EIP202_RING_ILLEGAL_IN_STATE
} eip202_ring_error_t;

// place holder for device specific internal data
typedef struct
{
    u32 placeholder[EIP202_IOAREA_REQUIRED_SIZE];
} eip202_ring_io_area_t;

typedef enum
{
    // Endianness Conversion (EC) in the EIP-202 HW master interface setting
    // for different types of DMA data.
    // Default setting is 0 which means that the EC in the EIP-202 hardware
    // is disabled.
    EIP202_RING_BYTESWAP_TOKEN           = BIT_0, // Enables Token data EC
    EIP202_RING_BYTESWAP_DESCRIPTOR      = BIT_1, // Enables Descriptor data EC
    EIP202_RING_BYTESWAP_PACKET          = BIT_2  // Enables Packet data EC
} eip202_ring_dma_byte_swap_data_type_t;

// Bit-mask for the events defined by eip202_ring_dma_byte_swap_data_type_t
typedef u32 eip202_ring_dma_byte_swap_data_type_mask_t;

// Endianness Conversion method
// One (or several) of the following methods must be configured in case
// the endianness conversion is enabled for one (or several) of
// the DMA data types:
typedef enum
{
    // Swap bytes within each 32 bit word
    EIP202_RING_BYTE_SWAP_METHOD_32    = BIT_0,

    // Swap 32 bit chunks within each 64 bit chunk
    EIP202_RING_BYTE_SWAP_METHOD_64    = BIT_1,

    // Swap 64 bit chunks within each 128 bit chunk
    EIP202_RING_BYTE_SWAP_METHOD_128   = BIT_2,

    // Swap 128 bit chunks within each 256 bit chunk
    EIP202_RING_BYTE_SWAP_METHOD_256   = BIT_3
} eip202_ring_dma_byte_swap_method_t;

// Bit-mask for the events defined by eip202_ring_dma_byte_swap_data_type_t
typedef u32 eip202_ring_dma_byte_swap_method_mask_t;

// Physical (bus) address that can be used for DMA by the device
typedef struct
{
    // 32-bit physical bus address
    u32 addr;

    // upper 32-bit part of a 64-bit physical address
    // Note: this value has to be provided only for 64-bit addresses,
    // in this case addr field provides the lower 32-bit part
    // of the 64-bit address, for 32bit addresses this field is ignored,
    // and should be set to 0.
    u32 upper_addr;

} eip202_device_address_t;

// 64-bit DMA address support
typedef enum
{
    EIP202_RING_64BIT_DMA_DISABLED,  // 32-bit DMA addresses in descriptors only
    EIP202_RING_64BIT_DMA_DSCR_PTR,  // 64-bit DMA addresses in descriptors
    EIP202_RING_64BIT_DMA_EXT_ADDR   // Enables extended DMA addresses, e.g.
                                    // 32-bit DMA addresses in descriptors
                                    // are added to a 64-bit base address
                                    // (only one 64-bit base address can be
                                    // defined for tokens and one for
                                    // packet data )
} eip202_ring_dma_address_mode_t;

// The EIP-202 HW master interface hardware data width
typedef enum
{
    EIP202_RING_DMA_ALIGNMENT_4_BYTES   = 0, // 4 bytes
    EIP202_RING_DMA_ALIGNMENT_8_BYTES   = 1, // 8 bytes
    EIP202_RING_DMA_ALIGNMENT_16_BYTES  = 2, // 16 bytes
    EIP202_RING_DMA_ALIGNMENT_32_BYTES  = 3  // 32 bytes
} eip202_ring_dma_alignment_t;

// bufferability control
typedef enum
{
    // Enables bufferability control for ownership word DMA writes
    EIP202_RING_BUF_CTRL_OWN_DMA_WRITES              = BIT_0,

    // Enables bufferability control for write/Read cache type control
    EIP202_RING_BUF_CTRL_WR_RD_CACHE                 = BIT_1,

    // Enables bufferability control for Result Token DMA writes
    // Note: this can be enabled for RDR only
    EIP202_RING_BUF_CTRL_RESULT_TOKEN_DMA_WRITES     = BIT_2,

    // Enables bufferability control for Descriptor Control Word DMA writes
    // Note: this can be enabled for RDR only
    EIP202_RING_BUF_CTRL_RDR_CONTROL_DMA_WRITES      = BIT_3

} eip202_ring_bufferability_control_t;

// Bit-mask for the events defined by eip202_ring_bufferability_control_t
typedef u32    eip202_ring_bufferability_settings_t;

// Autonomous Ring Mode (ARM) descriptor ring settings
typedef struct
{
    // Master data Bus Width (in 32-bit words)
    u32                                data_bus_width_word_count;

    // Endianness Conversion (EC) settings
    // mask enables the EC per DMA data type:
    // 1) Token, 2) Descriptor and 3) Packet data.
    eip202_ring_dma_byte_swap_data_type_mask_t byte_swap_data_type_mask;

    // Endianness Conversion method per DMA data type
    eip202_ring_dma_byte_swap_method_mask_t   byte_swap_token_mask;
    eip202_ring_dma_byte_swap_method_mask_t   byte_swap_descriptor_mask;
    eip202_ring_dma_byte_swap_method_mask_t   byte_swap_packet_mask;

    // bufferability control
    eip202_ring_bufferability_settings_t     bufferability;

    // Enable 64-bit DMA address support
    // Two different 64-bit DMA modes exist:
    //      1) 64-bit descriptor pointer and
    //      2) extended addressing.
    eip202_ring_dma_address_mode_t           dma_address_mode;

    // Ring size, in 32-bit words:
    //      The minimum value is the “command Descriptor offset”, see below.
    //      The maximum value is 4194303
    //      (setting a ring size of one word below 16M byte).
    u32                                ring_size_word_count;

    // Ring DMA resource handle
    dma_resource_handle_t                    ring_dma_handle;

    // Ring physical address that can be used for DMA
    eip202_device_address_t                   ring_dma_address;

    // command/Result Descriptor size (in 32-bit words)
    // 1) For CDR:
    // The minimum value in 32-bit address mode is 3 without the Additional
    // Token Pointer (ATP) and 4 with ATP.
    // The minimum value in 64-bit address mode is 5 without ATP and 7 with ATP.
    // The maximum value is the “command Descriptor fifo size”,
    // independent of the addressing mode.
    // 2) For RDR:
    // The minimum value is 3 for 32-bit addressable systems and
    // 5 for 64-bit addressable systems.
    // The maximum value is the “Result Descriptor fifo size”,
    // independent of the addressing mode.
    u32                                dscr_size_word_count;

    // Descriptor offset (in 32-bit words)
    // Can be used to align descriptor size for cache-line size, for example)
    u32                                dscr_offs_word_count;

    // offset of result token with respect to start of descriptor.
    u32                                token_offset_word_count;

    // command/Result Descriptor Fetch size (in 32-bit words)
    // The number of 32-bit words of Descriptor data that
    // the Ring Manager attempts to fetch from the ring in Host memory in one
    // contiguous block to the internal fifo. The Ring Manager waits until
    // the “Descriptor Fetch threshold” (see below) is exceeded.
    // Then it fetches “Descriptor Fetch size” 32-bit words or as many
    // words of descriptor data as available, from the ring.
    // This value must be an integer multiple of the “Descriptor offset”
    // and should not exceed the “Descriptor fifo size”.
    u32                                dscr_fetch_size_word_count;

    // command/Result Descriptor Fetch threshold (in 32-bit words)
    // In Autonomous Ring Mode this is the number of 32-bit words that must be
    // free in the Descriptor fifo before it fetches a new block of
    // Descriptors. This value must be an integer multiple of
    // the “Descriptor size” rounded up to a whole multiple of
    // the “Master data Bus Width”, and should not exceed the
    // “Descriptor fifo size”.
    // Example: if the “command Descriptor fifo size” is 5 words and the master
    //          data bus width is 128 bits (4 words), then this register must
    //          be programmed to at least value 8, since that is the next
    //          integer multiple of 4 after 5.
    u32                                dscr_threshold_word_count;

    // command/Result Descriptor Interrupt threshold
    // 1) For CDR  (in Descriptors):
    // The value in this field is compared with the number of prepared command
    // Descriptors pending in the CDR. When that number is less than or equal
    // to the value set in this field the cd_thresh_irq interrupt fires.
    // 2) For RDR  (in Descriptors or in Packets):
    // The rd_proc_thresh_irq interrupt is fired when the number of
    // the Processed Result Descriptors/Packets for the RDR exceeds the value
    // set by this parameter. The maximum number of packets is 127.
    u32                                int_threshold_dscr_count;

    // command/Result Descriptor Interrupt timeout
    // (units of 256 clock cycles of the HIA clock)
    // 1) For CDR:
    // The cd_timeout_irq interrupt is fired when the number of prepared
    // command descriptors pending in the CDR is non-zero and not decremented
    // for the amount of time specified by this value.
    // 2) For RDR:
    // The rd_proc_timeout_irq interrupt is fired when the number of Processed
    // Result Descriptors pending in the RDR is non-zero, remains constant and
    // has not reached the value specified by the “Result Descriptor Interrupt
    // threshold” parameter for the amount of time specified by this value.
    // 3) For CDR and RDR:
    // A value of zero disables the interrupt and stops the timeout counter
    // (useful to reduce power consumption).
    u32                                int_timeout_dscr_count;

} eip202_arm_ring_settings_t;

// EIP-202 HW options
typedef struct
{
    // number of statically configured Descriptor Rings
    u8 nof_rings;

    // number of statically configured Processing Engines,
    // value 0 indicates 8 PEs
    u8 nof_pes;

    // If true then 64 bit descriptors will contain a particle
    // size/fill level extension word to allow particle sizes larger
    // than 1 MB.
    bool f_exp_plf;

    // command Descriptor fifo size, the actual size is 2^cf_size 32-bit
    // words.
    u8 cf_size;

    // Result Descriptor fifo size, the actual size is 2^rf_size 32-bit
    // words.
    u8 rf_size;

    // Host interface type:
    // 0 = PLB, 1 = AHB, 2 = TCM, 3 = AXI
    u8 host_ifc;

    // Maximum supported DMA length is 2^(dma_len+1) – 1 bytes
    u8 dma_len;

    // Host interface data width:
    // 0 = 32 bits, 1 = 64 bits, 2 = 128 bits, 3 = 256 bits
    u8 hdw;

    // Target access block alignment. If this value is larger than 0,
    // the distance between 2 rings, 2 AICs and between the DFE and the DSE
    // in the slave memory map is increased by a factor of 2^tgt_align.
    // This means that ring control registers start at 2^(tgt_align+11) byte
    // boundaries (or a 2^(tgt_align+12) byte boundary for a combined
    // CDR/RDR block), the DFE and DSE will start at a 2^(tgt_align+10) byte
    // boundary and AICs will start at 2^(tgt_align+8) byte boundaries.
    u8 tgt_align;

    // 64-bit addressing mode:
    // false = 32-bit addressing,
    // true = 64-bit addressing
    bool f_addr64;

    /* Version number of EIP202 */
    u8 major_version;
    u8 minor_version;
    u8 patch_level;

} eip202_ring_options_t;

// Ring Admin data
typedef struct
{
    unsigned int ring_size_word_count;
    unsigned int desc_offs_word_count;

    unsigned int in_size;
    unsigned int in_tail;

    unsigned int out_size;
    unsigned int out_head;

    bool f_separate;
} eip202_ring_admin_t;


#endif /* INCLUDE_GUARD_EIP202_RING_TYPES_H */


/* end of file eip202_ring_types.h */
