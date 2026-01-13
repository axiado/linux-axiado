/* eip202_cdr_init.c
 *
 * EIP-202 Ring Control Driver Library
 * CDR Init/Reset API implementation
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

/*----------------------------------------------------------------------------
 * This module implements (provides) the following interface(s):
 */

#include "eip202_cdr.h"


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Default configuration
#include "c_eip202_ring.h"

// EIP-202 Ring Control Driver Library Types API
#include "eip202_ring_types.h"          // EIP202_Ring_* types

// EIP-202 Ring Control Driver Library internal interfaces
#include "eip202_ring_internal.h"
#include "eip202_cdr_level0.h"          // EIP-202 Level 0 macros
#include "eip202_cdr_fsm.h"             // CDR state machine
#include "eip202_cdr_dscr.h"            // ring_helper callbacks
#include "eip202_cd_format.h"           // EIP-202 command Descriptor

// ring_helper API
#include "ringhelper.h"                // ring_helper_init

// Driver Framework Basic Definitions API
#include "basic_defs.h"                // IDENTIFIER_NOT_USED, bool, u32

// Driver Framework device API
#include "device_types.h"              // device_handle_t

// Driver Framework DMA Resource API
#include "dmares_types.h"         // types of the DMA resource API
#include "dmares_rw.h"            // read/write of the DMA resource API.

// Driver Framework C Run-Time Library API
#include "clib.h"

// Standard IOToken API
#include "iotoken.h"                   // io_token_in_word_count_get()


/*----------------------------------------------------------------------------
 * Definitions and macros
 */


/*----------------------------------------------------------------------------
 * Local variables
 */


/*----------------------------------------------------------------------------
 * EIP202Lib_Detect
 *
 * Checks the presence of EIP-202 HIA hardware. Returns true when found.
 */
static bool
eip202_lib_cdr_detect(
        const device_handle_t device)
{
    u32 value;

    // read-write test one of the registers

    // Set MASK_31_BITS bits of the EIP202_CDR_RING_BASE_ADDR_LO register
    eip202_cdr_write32(device,
                       EIP202_CDR_RING_BASE_ADDR_LO,
                       MASK_31_BITS );

    value = eip202_cdr_read32(device, EIP202_CDR_RING_BASE_ADDR_LO);
    if ((value & MASK_31_BITS) != MASK_31_BITS)
        return false;

    // Clear MASK_31_BITS bits of the EIP202_CDR_RING_BASE_ADDR_LO register
    eip202_cdr_write32(device, EIP202_CDR_RING_BASE_ADDR_LO, 0);
    value = eip202_cdr_read32(device, EIP202_CDR_RING_BASE_ADDR_LO);
    if ((value & MASK_31_BITS) != 0)
       return false;

    return true;
}


/*----------------------------------------------------------------------------
 * eip202_lib_cdr_clear_all_descriptors
 *
 * Clear all descriptors
 */
static inline void
eip202_lib_cdr_clear_all_descriptors(
        dma_resource_handle_t handle,
        const u32 descriptor_spacing_word_count,
        const u32 descriptor_size_word_count,
        const u32 number_of_descriptors)
{
    unsigned int i, j;

    for(i = 0; i < number_of_descriptors; i++)
        for(j = 0; j < descriptor_size_word_count; j++)
            dma_resource_write32(handle, i * descriptor_spacing_word_count + j, 0);
}


/*----------------------------------------------------------------------------
 * eip202_cdr_init
 *
 */
eip202_ring_error_t
eip202_cdr_init(
        eip202_ring_io_area_t * io_area_p,
        const device_handle_t device,
        const eip202_cdr_settings_t * const cdr_settings_p)
{
    u16 cdfifo_word_count;
    eip202_ring_error_t rv;
    volatile eip202_cdr_true_io_area_t * const true_io_area_p = cdrioarea(io_area_p);

    EIP202_RING_CHECK_POINTER(io_area_p);

    // Initialize the IO Area
    true_io_area_p->f_valid_count_rd = false;
    true_io_area_p->device = device;
    true_io_area_p->state = (unsigned int)EIP202_CDR_STATE_UNINITIALIZED;

    // Check if the CPU integer size is enough to store 32-bit value
    if(sizeof(unsigned int) < sizeof(u32))
        return EIP202_RING_UNSUPPORTED_FEATURE_ERROR;

    // Detect presence of EIP-202 CDR hardware
    if(!eip202_lib_cdr_detect(device))
        return EIP202_RING_UNSUPPORTED_FEATURE_ERROR;

    if(cdr_settings_p->f_at_pto_token)
        return EIP202_RING_UNSUPPORTED_FEATURE_ERROR;

    // Extension of 32-bit pointers to 64-bit addresses not supported.
    if(cdr_settings_p->params.dma_address_mode == EIP202_RING_64BIT_DMA_EXT_ADDR)
        return EIP202_RING_UNSUPPORTED_FEATURE_ERROR;

    if(cdr_settings_p->params.dscr_offs_word_count == 0 ||
       cdr_settings_p->params.dscr_offs_word_count <
       cdr_settings_p->params.dscr_size_word_count)
        return EIP202_RING_ARGUMENT_ERROR;

    // Ring size cannot be smaller than one descriptor size or
    // larger than 4194303 (16MB / 4 - 1), in 32-bit words
    if(cdr_settings_p->params.ring_size_word_count <
       cdr_settings_p->params.dscr_offs_word_count ||
       cdr_settings_p->params.ring_size_word_count > 4194303)
        return EIP202_RING_ARGUMENT_ERROR;

    // Read command Descriptor fifo size (in 32-bit words)
    EIP202_CDR_STAT_FIFO_SIZE_RD(device, &cdfifo_word_count);

    if(cdr_settings_p->params.dscr_size_word_count >
             EIP202_CD_CTRL_DATA_MAX_WORD_COUNT + io_token_in_word_count_get() ||
       cdr_settings_p->params.dscr_size_word_count > cdfifo_word_count)
        return EIP202_RING_ARGUMENT_ERROR;

    if(cdr_settings_p->params.dscr_fetch_size_word_count > cdfifo_word_count ||
       cdr_settings_p->params.dscr_threshold_word_count > cdfifo_word_count)
        return EIP202_RING_ARGUMENT_ERROR;

    if(cdr_settings_p->params.dscr_fetch_size_word_count %
                            cdr_settings_p->params.dscr_offs_word_count)
        return EIP202_RING_ARGUMENT_ERROR;

    if( cdr_settings_p->params.int_threshold_dscr_count >
        cdr_settings_p->params.ring_size_word_count )
        return EIP202_RING_ARGUMENT_ERROR;

    // Configure the Ring Helper
    true_io_area_p->ring_helper_callbacks.write_func_p = &eip202_cdr_write_cb;
    true_io_area_p->ring_helper_callbacks.read_func_p = &eip202_cdr_read_cb;
    true_io_area_p->ring_helper_callbacks.status_func_p = &eip202_cdr_status_cb;
    true_io_area_p->ring_helper_callbacks.callback_param1_p = io_area_p;
    true_io_area_p->ring_helper_callbacks.callback_param2 = 0;
    true_io_area_p->ring_handle = cdr_settings_p->params.ring_dma_handle;
    true_io_area_p->desc_offs_word_count = cdr_settings_p->params.dscr_offs_word_count;
    true_io_area_p->ring_size_word_count = cdr_settings_p->params.ring_size_word_count;
    true_io_area_p->f_atp = cdr_settings_p->f_atp;

    // Initialize one ring_helper instance for one CDR instance
    if( ring_helper_init(
         (volatile ring_helper_t*)&true_io_area_p->ring_helper,
         (volatile ring_helper_callback_interface_t*)&true_io_area_p->ring_helper_callbacks,
         true, // Separate CDR ring
         (unsigned int)(cdr_settings_p->params.ring_size_word_count /
             cdr_settings_p->params.dscr_offs_word_count),
         (unsigned int)(cdr_settings_p->params.ring_size_word_count /
                        cdr_settings_p->params.dscr_offs_word_count)) < 0)
        return EIP202_RING_ARGUMENT_ERROR;

    // Transit to a new state
    rv = eip202_cdr_state_set((volatile eip202_cdr_state_t*)&true_io_area_p->state,
                             EIP202_CDR_STATE_INITIALIZED);
    if(rv != EIP202_RING_NO_ERROR)
        return EIP202_RING_ILLEGAL_IN_STATE;

    // Prepare the CDR DMA buffer
    // Initialize all descriptors with zero for CDR
    eip202_lib_cdr_clear_all_descriptors(
            true_io_area_p->ring_handle,
            cdr_settings_p->params.dscr_offs_word_count,
            cdr_settings_p->params.dscr_size_word_count,
            cdr_settings_p->params.ring_size_word_count /
                 cdr_settings_p->params.dscr_offs_word_count);

    // Call PreDMA to make sure engine sees it
    dma_resource_pre_dma(true_io_area_p->ring_handle,
                       0,
                       (unsigned int)(true_io_area_p->ring_size_word_count*4));

    EIP202_CDR_RING_BASE_ADDR_LO_WR(
                       device,
                       cdr_settings_p->params.ring_dma_address.addr);

    EIP202_CDR_RING_BASE_ADDR_HI_WR(
                       device,
                       cdr_settings_p->params.ring_dma_address.upper_addr);

    EIP202_CDR_RING_SIZE_WR(
                       device,
                       cdr_settings_p->params.ring_size_word_count);

    EIP202_CDR_DESC_SIZE_WR(
                       device,
                       cdr_settings_p->params.dscr_size_word_count,
                       cdr_settings_p->params.dscr_offs_word_count,
                       cdr_settings_p->f_at_pto_token,
                       cdr_settings_p->f_atp,
                       cdr_settings_p->params.dma_address_mode == EIP202_RING_64BIT_DMA_DSCR_PTR);

    EIP202_CDR_CFG_WR(
                       device,
                       cdr_settings_p->params.dscr_fetch_size_word_count,
                       cdr_settings_p->params.dscr_threshold_word_count);

    EIP202_CDR_DMA_CFG_WR(
                       device,
                       (u8)cdr_settings_p->params.byte_swap_descriptor_mask,
                       (u8)cdr_settings_p->params.byte_swap_packet_mask,
                       (u8)cdr_settings_p->params.byte_swap_token_mask,
                       // bufferability control
                       true,  // Buffer Ownership Word DMA writes
                       EIP202_RING_CD_WR_CACHE_CTRL, // Write cache type control
                       EIP202_RING_CD_RD_CACHE_CTRL, // Read cache type control
                       EIP202_RING_CD_PROT_VALUE,
                       EIP202_RING_DATA_PROT_VALUE,
                       EIP202_RING_ACD_PROT_VALUE);
    return EIP202_RING_NO_ERROR;
}


/*----------------------------------------------------------------------------
 * eip202_cdr_reset
 *
 */
eip202_ring_error_t
eip202_cdr_reset(
        eip202_ring_io_area_t * const io_area_p,
        const device_handle_t device)
{
    eip202_ring_error_t rv;
    volatile eip202_cdr_true_io_area_t * const true_io_area_p = cdrioarea(io_area_p);

    EIP202_RING_CHECK_POINTER(io_area_p);

    // Initialize the IO Area
    memset((void*)io_area_p, 0, sizeof(*true_io_area_p));
    true_io_area_p->device = device;
    true_io_area_p->state = (unsigned int)EIP202_CDR_STATE_UNKNOWN;

    // Transit to a new state
    rv = eip202_cdr_state_set((volatile eip202_cdr_state_t*)&true_io_area_p->state,
                             EIP202_CDR_STATE_UNINITIALIZED);
    if(rv != EIP202_RING_NO_ERROR)
        return EIP202_RING_ILLEGAL_IN_STATE;

    // Clear CDR count
    EIP202_CDR_COUNT_WR(device, 0, true);

    // Re-init CDR
    EIP202_CDR_POINTER_DEFAULT_WR(device);

    // Restore default register values
    EIP202_CDR_RING_BASE_ADDR_LO_DEFAULT_WR(device);
    EIP202_CDR_RING_BASE_ADDR_HI_DEFAULT_WR(device);
    EIP202_CDR_RING_SIZE_DEFAULT_WR(device);
    EIP202_CDR_DESC_SIZE_DEFAULT_WR(device);
    EIP202_CDR_CFG_DEFAULT_WR(device);
    EIP202_CDR_DMA_CFG_DEFAULT_WR(device);

    // Clear and disable all CDR interrupts
    EIP202_CDR_STAT_CLEAR_ALL_IRQ_WR(device);

    return EIP202_RING_NO_ERROR;
}


/*----------------------------------------------------------------------------
 * eip202_cdr_options_get
 *
 */
eip202_ring_error_t
eip202_cdr_options_get(
        const device_handle_t device,
        eip202_ring_options_t * const options_p)
{
    u32 rev;

    EIP202_RING_CHECK_POINTER(options_p);

    // Note: thie register does not exist in all versions of the device.
    //       If it exists, the options register is also available.
    rev = eip202_cdr_read32(device, EIP202_CDR_VERSION);
    if ( !EIP202_CDR_REV_SIGNATURE_MATCH((u16)rev))
    {
        // No local CDR version and options registers available,
        // function not supported
        return EIP202_RING_UNSUPPORTED_FEATURE_ERROR;
    }

    EIP202_CDR_OPTIONS_RD(device,
                          &options_p->nof_rings,
                          &options_p->nof_pes,
                          &options_p->f_exp_plf,
                          &options_p->cf_size,
                          &options_p->rf_size,
                          &options_p->host_ifc,
                          &options_p->dma_len,
                          &options_p->hdw,
                          &options_p->tgt_align,
                          &options_p->f_addr64);
    options_p->major_version = (rev >> 24) & MASK_4_BITS;
    options_p->minor_version = (rev >> 20) & MASK_4_BITS;
    options_p->patch_level   = (rev >> 16) & MASK_4_BITS;


    return EIP202_RING_NO_ERROR;
}


/*----------------------------------------------------------------------------
 * eip202_cdr_dump
 *
 */
void
eip202_cdr_dump(
        eip202_ring_io_area_t * const io_area_p,
        eip202_ring_admin_t * const ring_admin_p)
{
    eip202_rdr_true_io_area_t * const true_io_area_p =
                            (eip202_rdr_true_io_area_t * const)io_area_p;

    if(!true_io_area_p)
        return;

    if(!ring_admin_p)
        return;

    ring_admin_p->in_size           = true_io_area_p->ring_helper.in_size;
    ring_admin_p->in_tail           = true_io_area_p->ring_helper.in_tail;
    ring_admin_p->out_size          = true_io_area_p->ring_helper.out_size;
    ring_admin_p->out_head          = true_io_area_p->ring_helper.out_head;

    ring_admin_p->f_separate         = true_io_area_p->ring_helper.f_separate;

    ring_admin_p->desc_offs_word_count = true_io_area_p->desc_offs_word_count;
    ring_admin_p->ring_size_word_count = true_io_area_p->ring_size_word_count;
}


/* end of file eip202_cdr_init.c */
