/* eip202_global_init.h
 *
 * EIP-202 Global Init interface
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

#ifndef EIP202_GLOBAL_INIT_H_
#define EIP202_GLOBAL_INIT_H_


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Driver Framework Basic Definitions API
#include "basic_defs.h"             // BIT definitions, bool, u32

// Driver Framework device API
#include "device_types.h"           // device_handle_t


/*----------------------------------------------------------------------------
 * Definitions and macros
 */

// Ring PE assignment map
typedef struct
{
    // PE is selected via the index in the ring_pe_mask array,
    // index i selects PE i
    // Bit N:
    //     0 - ring N is not assigned (cleared) to this PE
    //     1 - ring N is assigned to this PE
    u32 ring_pe_mask;

    // PE is selected via the index in the ring_prio_mask array,
    // index i selects PE i
    // Bit N:
    //     0 - ring N is low priority
    //     1 - ring N is high priority
    u32 ring_prio_mask;

    // CDR Slots
    // CDR0: bits 3-0,   CDR1: bits 7-4,   CDR2: bits 11-8,  CDR3: bits 15-12
    // CDR4: bits 19-16, CDR5: bits 23-20, CDR6: bits 27-24, CDR7: bits 31-28
    u32 ring_slots0;

    // CDR Slots
    // CDR8:  bits 3-0,   CDR9:  bits 7-4,   CDR10: bits 11-8, CDR11: bits 15-12
    // CDR12: bits 19-16, CDR13: bits 23-20, CDR14: bits 27-24
    u32 ring_slots1;

} eip202_global_ring_pe_map_t;

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

    // number of statically configured Look-aside interfaces
    u8 nof_la_ifs;

    // number of statically configured Inline interfaces
    u8 nof_in_ifs;

    // AXI master interface only
    // number of write channels for a high-performance AXI master interface
    // minus 1
    u8 nof_axi_wr_chs;

    // AXI master interface only
    // number of read clusters for a high-performance AXI master interface.
    u8 nof_axi_rd_clusters;

    // AXI master interface only
    // number of read channels per read cluster for a high-performance AXI
    // master interface, minus 1
    u8 nof_axi_rd_cpc;

    // The basic EIP number.
    u8 eip_number;

    // The complement of the basic EIP number.
    u8 complmt_eip_number;

    // Hardware Patch Level.
    u8 hw_patch_level;

    // minor Hardware revision.
    u8 min_hw_revision;

    // major Hardware revision.
    u8 maj_hw_revision;

} eip202_capabilities_t;


/*----------------------------------------------------------------------------
 * Local variables
 */



/*----------------------------------------------------------------------------
 * EIP202 Global Functions
 *
 */

/*----------------------------------------------------------------------------
 * eip202_global_detect
 *
 * Checks the presence of EIP-202 HIA hardware. Returns true when found.
 *
 * device (input)
 *     device handle of the hardware.
 *
 * Return value
 *     true  : Success
 *     false : Failure
 */
bool
eip202_global_detect(
        const device_handle_t device);


/*----------------------------------------------------------------------------
 * eip202_global_endianness_slave_configure
 *
 * Configure Endianness Conversion method
 * for the EIP-202 slave (MMIO) interface
 *
 * device (input)
 *     device handle of the hardware.
 *
 * Return value
 *     true  : Success
 *     false : Failure
 */
bool
eip202_global_endianness_slave_configure(
        const device_handle_t device);


/*----------------------------------------------------------------------------
 * eip202_global_init
 *
 * Initialize the ring, LA-fifo and inline interfaces of the EIP202 hardware.
 *
 * device (input)
 *     device handle of the hardware.
 * nof_pe (input)
 *     number of packet engines in the device.
 * nof_la (input)
 *     number of look-aside FIFOs of the device.
 * ipbuf_min (input)
 *     Minimum input packet burst size.
 * ipbuf_max (input)
 *     Maximum input packet burst size.
 * itbuf_min (input)
 *     Minimum input token burst size.
 * itbuf_max (input)
 *     Maximum input token burst size.
 * opbuf_min (input)
 *     Minimum output packet burst size.
 * opbuf_max (input)
 *     Maximum output packet burst size.
 */
void
eip202_global_init(
        const device_handle_t device,
        unsigned int nof_pe,
        unsigned int nof_la,
        u8 ipbuf_min,
        u8 ipbuf_max,
        u8 itbuf_min,
        u8 itbuf_max,
        u8 opbuf_min,
        u8 opbuf_max);


/*----------------------------------------------------------------------------
 * eip202_global_reset
 *
 * Reset the EIP202 hardware.
 *
 * device (input)
 *     device handle of the hardware.
 * nof_pe (input)
 *     number of packet engines in the hardware.
 *
 * Return value
 *     true  : Success
 *     false : Failure
 */
bool
eip202_global_reset(
        const device_handle_t device,
        const unsigned int nof_pe);


/*----------------------------------------------------------------------------
 * eip202_global_reset_is_done
 *
 * Check if a reset operation (started with eip202_global_reset) is completed
 * for a specified Packet Engine.
 *
 * device (input)
 *     device handle of the hardware.
 * p_enr (input)
 *     number of the packet engine to check.
 *
 * Return value
 *     true  : Reset is completed
 *     false : Reset is not completed
 */
bool
eip202_global_reset_is_done(
        const device_handle_t device,
        const unsigned int p_enr);


/*----------------------------------------------------------------------------
 * eip202_global_hw_revision_get
 *
 * Read hardware revision and capabilities of the EIP202 hardware.
 *
 * device (input)
 *     device handle of the hardware.
 * capabilities_p (output)
 *     Hardware options of the EIP202.
 */
void
eip202_global_hw_revision_get(
        const device_handle_t device,
        eip202_capabilities_t * const capabilities_p);


/*----------------------------------------------------------------------------
 * eip202_global_configure
 *
 * Configure the EIP202 for a single Packet Engine.
 *
 * device (input)
 *     device handle of the hardware.
 * pe_number (input)
 *     number of the Packet Engine to configure.
 * ring_pe_map_p (input)
 *     Structure containing the configuration.
 */
void
eip202_global_configure(
        const device_handle_t device,
        const unsigned int pe_number,
        const eip202_global_ring_pe_map_t * const ring_pe_map_p);


#endif /* EIP202_GLOBAL_INIT_H_ */


/* end of file eip202_global_init.h */
