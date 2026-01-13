/* eip202_rdr_init.c
 *
 * EIP-202 Ring Control Driver Library
 * RDR Init/Reset API implementation
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

#include "eip202_rdr.h"


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Default configuration
#include "c_eip202_ring.h"

// EIP-202 Ring Control Driver Library Types API
#include "eip202_ring_types.h"          // EIP202_Ring_* types

// EIP-202 Ring Control Driver Library internal interfaces
#include "eip202_ring_internal.h"
#include "eip202_rdr_level0.h"          // EIP-202 Level 0 macros
#include "eip202_rdr_fsm.h"             // RDR state machine
#include "eip202_rdr_dscr.h"            // ring_helper callbacks
#include "eip202_rd_format.h"           // EIP-202 Result Descriptor

// ring_helper API
#include "ringhelper.h"

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
eip202_lib_rdr_detect(
        const device_handle_t device)
{
    u32 value;

    // read-write test one of the registers

    // Set MASK_31_BITS bits of the EIP202_RDR_RING_BASE_ADDR_LO register
    eip202_rdr_write32(device,
                       EIP202_RDR_RING_BASE_ADDR_LO,
                       MASK_31_BITS );

    value = eip202_rdr_read32(device, EIP202_RDR_RING_BASE_ADDR_LO);
    if ((value & MASK_31_BITS) != MASK_31_BITS)
        return false;

    // Clear MASK_31_BITS bits of the EIP202_RDR_RING_BASE_ADDR_LO register
    eip202_rdr_write32(device, EIP202_RDR_RING_BASE_ADDR_LO, 0);
    value = eip202_rdr_read32(device, EIP202_RDR_RING_BASE_ADDR_LO);
    if ((value & MASK_31_BITS) != 0)
       return false;

    return true;
}


/*----------------------------------------------------------------------------
 * eip202_lib_rdr_clear_all_descriptors
 *
 * Clear all descriptors
 */
static inline void
eip202_lib_rdr_clear_all_descriptors(
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
 * eip202_rdr_init
 *
 */
eip202_ring_error_t
eip202_rdr_init(
        eip202_ring_io_area_t * io_area_p,
        const device_handle_t device,
        const eip202_rdr_settings_t * const rdr_settings_p)
{
    eip202_ring_error_t rv;
    volatile eip202_rdr_true_io_area_t * const true_io_area_p = rdrioarea(io_area_p);

    EIP202_RING_CHECK_POINTER(io_area_p);

    // Initialize the IO Area
    true_io_area_p->device = device;
    true_io_area_p->state = (unsigned int)EIP202_RDR_STATE_UNINITIALIZED;

    // Check if the CPU integer size is enough to store 32-bit value
    if(sizeof(unsigned int) < sizeof(u32))
        return EIP202_RING_UNSUPPORTED_FEATURE_ERROR;

    // Detect presence of EIP-202 CDR hardware
    if(!eip202_lib_rdr_detect(device))
        return EIP202_RING_UNSUPPORTED_FEATURE_ERROR;

    // Extension of 32-bit pointers to 64-bit addresses not supported.
    if(rdr_settings_p->params.dma_address_mode == EIP202_RING_64BIT_DMA_EXT_ADDR)
        return EIP202_RING_UNSUPPORTED_FEATURE_ERROR;

    if(rdr_settings_p->params.dscr_offs_word_count == 0 ||
       rdr_settings_p->params.dscr_offs_word_count <
       rdr_settings_p->params.dscr_size_word_count)
        return EIP202_RING_ARGUMENT_ERROR;

    // Ring size cannot be smaller than one descriptor size or
    // larger than 4194303 (16MB / 4 - 1), in 32-bit words
    if(rdr_settings_p->params.ring_size_word_count <
       rdr_settings_p->params.dscr_offs_word_count ||
       rdr_settings_p->params.ring_size_word_count > 4194303)
        return EIP202_RING_ARGUMENT_ERROR;

    if(rdr_settings_p->params.dscr_size_word_count >
             rdr_settings_p->params.token_offset_word_count + io_token_out_word_count_get())
        return EIP202_RING_ARGUMENT_ERROR;

    if(rdr_settings_p->params.dscr_fetch_size_word_count %
                            rdr_settings_p->params.dscr_offs_word_count)
        return EIP202_RING_ARGUMENT_ERROR;

    if( rdr_settings_p->params.int_threshold_dscr_count *
            rdr_settings_p->params.dscr_offs_word_count >
                      rdr_settings_p->params.ring_size_word_count )
        return EIP202_RING_ARGUMENT_ERROR;

    // Configure the Ring Helper
    true_io_area_p->ring_helper_callbacks.write_func_p = &eip202_rdr_write_cb;
    true_io_area_p->ring_helper_callbacks.read_func_p = &eip202_rdr_read_cb;
    true_io_area_p->ring_helper_callbacks.status_func_p = &eip202_rdr_status_cb;
    true_io_area_p->ring_helper_callbacks.callback_param1_p = io_area_p;
    true_io_area_p->ring_helper_callbacks.callback_param2 = 0;
    true_io_area_p->ring_handle = rdr_settings_p->params.ring_dma_handle;
    true_io_area_p->desc_offs_word_count = rdr_settings_p->params.dscr_offs_word_count;
    true_io_area_p->ring_size_word_count = rdr_settings_p->params.ring_size_word_count;
    true_io_area_p->token_offset_word_count = rdr_settings_p->params.token_offset_word_count;
    true_io_area_p->packet_found = false;

    // Initialize one ring_helper instance for one RDR instance
    if( ring_helper_init(
         (volatile ring_helper_t*)&true_io_area_p->ring_helper,
         (volatile ring_helper_callback_interface_t*)&true_io_area_p->ring_helper_callbacks,
         false, // One RDR as combined rings
         (unsigned int)(rdr_settings_p->params.ring_size_word_count /
             rdr_settings_p->params.dscr_offs_word_count),
         (unsigned int)(rdr_settings_p->params.ring_size_word_count /
             rdr_settings_p->params.dscr_offs_word_count)) < 0)
        return EIP202_RING_ARGUMENT_ERROR;

    // Transit to a new state
    rv = eip202_rdr_state_set((volatile eip202_rdr_state_t*)&true_io_area_p->state,
                             EIP202_RDR_STATE_INITIALIZED);
    if(rv != EIP202_RING_NO_ERROR)
        return EIP202_RING_ILLEGAL_IN_STATE;

    // Prepare the RDR DMA buffer
    // Initialize all descriptors with zero for RDR
    eip202_lib_rdr_clear_all_descriptors(
            true_io_area_p->ring_handle,
            rdr_settings_p->params.dscr_offs_word_count,
            rdr_settings_p->params.dscr_size_word_count,
            rdr_settings_p->params.ring_size_word_count /
                 rdr_settings_p->params.dscr_offs_word_count);

    // Call PreDMA to make sure engine sees it
    dma_resource_pre_dma(true_io_area_p->ring_handle,
                       0,
                       (unsigned int)(true_io_area_p->ring_size_word_count*4));

    EIP202_RDR_RING_BASE_ADDR_LO_WR(
                       device,
                       rdr_settings_p->params.ring_dma_address.addr);

    EIP202_RDR_RING_BASE_ADDR_HI_WR(
                       device,
                       rdr_settings_p->params.ring_dma_address.upper_addr);

    EIP202_RDR_RING_SIZE_WR(
                       device,
                       rdr_settings_p->params.ring_size_word_count);

    EIP202_RDR_DESC_SIZE_WR(
                       device,
                       rdr_settings_p->params.dscr_size_word_count,
                       rdr_settings_p->params.dscr_offs_word_count,
                       rdr_settings_p->params.dma_address_mode == EIP202_RING_64BIT_DMA_DSCR_PTR);

    EIP202_RDR_CFG_WR(device,
                      rdr_settings_p->params.dscr_fetch_size_word_count,
                      rdr_settings_p->params.dscr_threshold_word_count,
#ifdef EIP202_RDR_OWNERSHIP_WORD_ENABLE
                      true);  // Ownership write mode
#else
                      rdr_settings_p->f_continuous_scatter); // Normal mode, no ownership words are used.
/* Always enable ownership words for continuous scatter */
#endif

    // Disable Processed Descriptor threshold interrupt,
    // Disable timeout interrupt and stop timeout counter for
    // reducing power consumption
    EIP202_RDR_THRESH_WR(
                       device,
                       true_io_area_p->ring_size_word_count,
                       0,  // Set descriptor processing mode
                       0); // Disable timeout

    EIP202_RDR_DMA_CFG_WR(
                       device,
                       (u8)rdr_settings_p->params.byte_swap_descriptor_mask,
                       (u8)rdr_settings_p->params.byte_swap_packet_mask,

                       // bufferability control for DMA writes of
                       EIP202_RING_RD_RES_BUF,  // result token
                       EIP202_RING_RD_CTRL_BUF, // descriptor control words
                       EIP202_RING_RD_OWN_BUF,  // ownership words

                       EIP202_RING_RD_WR_CACHE_CTRL, // Write cache type control
                       EIP202_RING_RD_RD_CACHE_CTRL, // Read cache type control
                       EIP202_RING_RD_PROT_VALUE,
                       EIP202_RING_DATA_PROT_VALUE,
                       rdr_settings_p->f_continuous_scatter|EIP202_RDR_PAD_TO_OFFSET); // Result descriptor padding
/* Note: for continuous scatter: always set RDR_PAD_TO_OFFSET. */
    return EIP202_RING_NO_ERROR;
}


/*----------------------------------------------------------------------------
 * eip202_rdr_reset
 *
 */
eip202_ring_error_t
eip202_rdr_reset(
        eip202_ring_io_area_t * const io_area_p,
        const device_handle_t device)
{
    eip202_ring_error_t rv;
    volatile eip202_rdr_true_io_area_t * const true_io_area_p = rdrioarea(io_area_p);

    EIP202_RING_CHECK_POINTER(io_area_p);

    // Initialize the IO Area
    memset((void*)io_area_p, 0, sizeof(*true_io_area_p));
    true_io_area_p->device = device;
    true_io_area_p->state = (unsigned int)EIP202_RDR_STATE_UNKNOWN;

    // Transit to a new state
    rv = eip202_rdr_state_set((volatile eip202_rdr_state_t*)&true_io_area_p->state,
                             EIP202_RDR_STATE_UNINITIALIZED);
    if(rv != EIP202_RING_NO_ERROR)
        return EIP202_RING_ILLEGAL_IN_STATE;

    // Clear RDR count
    EIP202_RDR_PREP_COUNT_WR(device, 0, true);
    EIP202_RDR_PROC_COUNT_WR(device, 0, 0, true);

    // Re-init RDR
    EIP202_RDR_PREP_PNTR_DEFAULT_WR(device);
    EIP202_RDR_PROC_PNTR_DEFAULT_WR(device);

    // Restore default register values
    EIP202_RDR_RING_BASE_ADDR_LO_DEFAULT_WR(device);
    EIP202_RDR_RING_BASE_ADDR_HI_DEFAULT_WR(device);
    EIP202_RDR_RING_SIZE_DEFAULT_WR(device);
    EIP202_RDR_DESC_SIZE_DEFAULT_WR(device);
    EIP202_RDR_CFG_DEFAULT_WR(device);
    EIP202_RDR_DMA_CFG_DEFAULT_WR(device);
    EIP202_RDR_THRESH_DEFAULT_WR(device);

    // Clear and disable all RDR interrupts
    EIP202_RDR_STAT_CLEAR_ALL_IRQ_WR(device);

    return EIP202_RING_NO_ERROR;
}


/*----------------------------------------------------------------------------
 * eip202_rdr_dump
 *
 */
void
eip202_rdr_dump(
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


/* end of file eip202_rdr_init.c */
