/* eip97_global_init.h
 *
 * EIP-97 Global Control Driver Library API:
 * Initialization, Un-initialization, configuration use case
 *
 * Refer to the EIP-97 Driver Library User Guide for information about
 * re-entrance and usage from concurrent execution contexts of this API
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

#ifndef EIP97_GLOBAL_INIT_H_
#define EIP97_GLOBAL_INIT_H_


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Driver Framework Basic Definitions API
#include "basic_defs.h"         // u8, u32, bool

// Driver Framework device API
#include "device_types.h"       // device_handle_t

// EIP-97 Global Control Driver Library Types API
#include "eip97_global_types.h" // EIP97_* types


/*----------------------------------------------------------------------------
 * Definitions and macros
 */

// Generic EIP HW version
typedef struct
{
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
} eip_version_t;

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
} eip202_options_t;

// EIP-202 HW options2
typedef struct
{
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

} eip202_options2_t;

// EIP-206 HW options
typedef struct
{
    // These bits encode the EIP number for the EIP-96 Packet Engine.
    // This field contains the value 96 (decimal) or 0x60.
    u8 pe_type;

    // Input-side classifier configuration:
    //    0 - no input classifier present
    //    1 - EIP-207 input classifier present
    u8 in_classifier;

    // Output-side classifier configuration:
    //    0 - no input classifier present
    //    1 - EIP-207 output classifier present
    u8 out_classifier;

    // number of MAC [9:8] media interface RX/TX channels multiplexed and
    // demultiplexed here, in range 0-8.
    u8 nof_mac_channels;

    // size of the Input data Buffer in kilobytes.
    u8 in_dbuf_size_kb;

    // size of the Input Token Buffer in kilobytes.
    u8 in_tbuf_size_kb;

    // size of the Output data Buffer in kilobytes.
    u8 out_dbuf_size_kb;

    // size of the Output Token Buffer in kilobytes.
    u8 out_tbuf_size_kb;

} eip206_options_t;

// EIP-96 HW options
typedef struct
{
    // If true AES is available
    bool f_aes;

    // If true AES-CFB-128 and AES-OFB-128 are available
    bool f_ae_sfb;

    // If true fast AES core is integrated (12.8 bits/cycle).
    // If false medium speed AES core is integrated (4.2 bits/cycle).
    bool f_ae_sspeed;

    // If true DES and 3-DES are available
    bool f_des;

    // If true (3-)DES-CFB-64 and (3-)DES-OFB-64 are available.
    bool f_de_sfb;

    // If true fast (4-round) DES core is integrated.
    // If false slow (3-round) DES core is integrated.
    bool f_de_sspeed;

    // arc4 availability
    // 0 - no arc4 is available
    // 1 - a slow speed arc4 core is integrated (3.5 bits/cycle)
    // 2 - a medium speed arc4 core is integrated (6.4 bits/cycle)
    // 3 - a high speed arc4 core is integrated (8.0 bits/cycle)
    u8 arc4;

    // If true AES-XTS is available
    bool f_aes_xts;

    // If true Wireless crypto algorithms (Kasumi, SNOW, ZUC) are available
    bool f_wireless;

    // If true MD5 is available
    bool f_md5;

    // If true SHA-1 is available
    bool f_sha1;

    // If true fast SHA-1 core is integrated (12.8 bits/cycle)
    // If false slow SHA-1 core is integrated (6.4 bits/cycle)
    bool f_sha1speed;

    // If true SHA-224/256 is available
    bool f_sha224_256;

    // If true SHA-384/512 is available
    bool f_sha384_512;

    // If true AES-XCBC-MAC is available.
    // This also supports CBC-MAC and CMAC operations
    bool f_xcbc_mac;

    // If true fast AES-CBC-MAC core is integrated (12.8 bits/cycle)
    // If false slow AES-CBC-MAC core is integrated (4.2 bits/cycle)
    bool f_cbc_ma_cspeed;

    // If true AES-CBC-MAC core accepts all key lengths (128/192/256 bits)
    // If false AES-CBC-MAC core accepts only keys with a length of 128 bits
    bool f_cbc_ma_ckeylens;

    // If true GHASH core is available
    bool f_ghash;

} eip96_options_t;

// EIP-97 HW options
typedef struct
{
    // number of statically configured Processing Engines
    u8 nof_pes;

    // size of statically configured Input Token Buffer
    // The actual size is 2^in_tbuf_size in 32-bit words
    u8 in_tbuf_size;

    // size of statically configured Input data Buffer
    // The actual size is 2^in_dbuf_size in 32-bit words
    u8 in_dbuf_size;

    // size of statically configured Output Token Buffer
    // The actual size is 2^in_tbuf_size in 32-bit words
    u8 out_tbuf_size;

    // size of statically configured Output data Buffer
    // The actual size is 2^in_dbuf_size in 32-bit words
    u8 out_dbuf_size;

    // If true then the EIP(197) has a single central EIP-74 type DRBG.
    // If false then each EIP-96 has its own local PRNG.
    bool central_prng;

    // If true then a Token Generator is available in EIP-97 HW
    // If false the Host must supply the Token for the Processing Engine
    bool tg;

    // If true then a Transform Record Cache is available in EIP-97 HW
    bool trc;
} eip97_options_t;

// capabilities structure for EIP-97 HW
typedef struct
{
    // HIA
    eip202_options2_t   eip202_options2;
    eip202_options_t    eip202_options;
    eip_version_t       eip202_version;

    // Processing Engine
    eip206_options_t    eip206_options;
    eip_version_t       eip206_version;

    // Packet Engine
    eip96_options_t     eip96_options;
    eip_version_t       eip96_version;

    // EIP-97 HW shell
    eip97_options_t     eip97_options;
    eip_version_t       eip97_version;

} eip97_global_capabilities_t;

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

} eip97_global_ring_pe_map_t;


/*----------------------------------------------------------------------------
 * eip97_global_init
 *
 * This function performs the initialization of the EIP-97 Global Control HW
 * interface and transits the API to the Initialized state.
 *
 * This function returns the EIP97_GLOBAL_UNSUPPORTED_FEATURE_ERROR error code
 * when it detects a mismatch in the Global Control Driver Library configuration
 * and the use EIP-97 HW revision or configuration.
 *
 * Note: This function should be called either after the EIP-97 HW Reset or
 *       after the Global SW Reset.
 *
 * io_area_p (output)
 *     Pointer to the place holder in memory for the IO Area.
 *
 * device (input)
 *     handle for the Global Control device instance returned by eip_device_find.
 *
 * Return value
 *     EIP97_GLOBAL_NO_ERROR : operation is completed
 *     EIP97_GLOBAL_UNSUPPORTED_FEATURE_ERROR : not supported by the device.
 *     EIP97_GLOBAL_ARGUMENT_ERROR : Passed wrong argument
 *     EIP97_GLOBAL_ILLEGAL_IN_STATE : invalid API state transition
 */
eip97_global_error_t
eip97_global_init(
        eip97_global_io_area_t * const io_area_p,
        const device_handle_t device);


/*----------------------------------------------------------------------------
 * eip97_global_reset
 *
 * This function starts the Global SW Reset operation. If the reset operation
 * can be done immediately this function returns EIP97_GLOBAL_NO_ERROR.
 * Otherwise it will return EIP97_GLOBAL_BUSY_RETRY_LATER indicating
 * that the reset operation has been started and is ongoing.
 * The eip97_global_reset_is_done() function can be used to poll the device
 * for the completion of the reset operation.
 *
 * Note: This function must be called before calling the eip97_global_init()
 *       function only if the EIP-97 HW Reset was not done. Otherwise it still
 *       is can be called but it is not necessary.
 *
 * io_area_p (output)
 *     Pointer to the place holder in memory for the IO Area.
 *
 * device (input)
 *     handle for the Global Control device instance returned by eip_device_find.
 *
 * Return value
 *     EIP97_GLOBAL_NO_ERROR : Global SW Reset is done
 *     EIP97_GLOBAL_UNSUPPORTED_FEATURE_ERROR : not supported by the device.
 *     EIP97_GLOBAL_BUSY_RETRY_LATER: Global SW Reset is started but
 *                                    not completed yet
 *     EIP97_GLOBAL_ARGUMENT_ERROR : Passed wrong argument
 *     EIP97_GLOBAL_ILLEGAL_IN_STATE : invalid API state transition
 */
eip97_global_error_t
eip97_global_reset(
        eip97_global_io_area_t * const io_area_p,
        const device_handle_t device);


/*----------------------------------------------------------------------------
 * eip97_global_reset_is_done
 *
 * This function checks the status of the started by the eip97_global_reset()
 * function Global SW Reset operation for the EIP-97 device.
 *
 * io_area_p (input)
 *     Pointer to the place holder in memory for the IO Area.
 *
 * Return value
 *     EIP97_GLOBAL_NO_ERROR : Global SW Reset operation is done
 *     EIP97_GLOBAL_BUSY_RETRY_LATER: Global SW Reset is started but
 *                                    not completed yet
 *     EIP97_GLOBAL_ARGUMENT_ERROR : Passed wrong argument
 *     EIP97_GLOBAL_ILLEGAL_IN_STATE : invalid API state transition
 */
eip97_global_error_t
eip97_global_reset_is_done(
        eip97_global_io_area_t * const io_area_p);


/*----------------------------------------------------------------------------
 * eip97_global_hw_revision_get
 *
 * This function returns EIP-97, EIP202 HIA and EIP-96 PE hardware revision
 * information in the capabilities_p data structure.
 *
 * io_area_p (input)
 *     Pointer to the place holder in memory for the IO Area.
 *
 * capabilities_p (output)
 *     Pointer to the place holder in memory where the device capability
 *     information will be stored.
 *
 * Return value
 *     EIP97_GLOBAL_NO_ERROR : operation is completed
 *     EIP97_GLOBAL_ARGUMENT_ERROR : Passed wrong argument
 */
eip97_global_error_t
eip97_global_hw_revision_get(
        eip97_global_io_area_t * const io_area_p,
        eip97_global_capabilities_t * const capabilities_p);


/*----------------------------------------------------------------------------
 * eip97_global_configure
 *
 * This function performs the Ring to PE assignment and configures
 * the Ring priority. The EIP-97 device supports multiple Ring interfaces
 * as well as multiple PE's. One ring can be assigned to the same or different
 * PE's. Multiple rings can be assigned to the same PE.
 *
 * This function transits the API from the Initialized to the enabled state
 * when the ring(s) assignment to the PE(s) is performed successfully.
 *
 * This function transits the API from the enabled to the Initialized state
 * when the ring(s) assignment to the PE(s) is cleared.
 *
 * This function keeps the API in the enabled state when the ring(s) assignment
 * to the PE(s) is changed but not cleared completely.
 *
 * io_area_p (input)
 *     Pointer to the place holder in memory for the IO Area.
 *
 * pe_number (input)
 *     number of the PE that must be configured.
 *
 * ring_pe_map_p (input)
 *     Pointer to the data structure that contains the Ring PE assignment map.
 *
 * Return value
 *     EIP97_GLOBAL_NO_ERROR : operation is completed
 *     EIP97_GLOBAL_ARGUMENT_ERROR : Passed wrong argument
 *     EIP97_GLOBAL_ILLEGAL_IN_STATE : invalid API state transition
 */
eip97_global_error_t
eip97_global_configure(
        eip97_global_io_area_t * const io_area_p,
        const unsigned int pe_number,
        const eip97_global_ring_pe_map_t * const ring_pe_map_p);


/*----------------------------------------------------------------------------
 * eip97_supported_funcs_get
 *
 * Return a bitmask of the supported functions (EIP96 options register).
 */
unsigned int
eip97_supported_funcs_get(void);


#endif /* EIP97_GLOBAL_INIT_H_ */


/* end of file eip97_global_init.h */
