/* eip202_cdr_dscr.c
 *
 * EIP-202 Ring Control Driver Library
 * 1) Descriptor I/O Driver Library API implementation
 * 2) internal command Descriptor interface implementation
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

// Descriptor I/O Driver Library API implementation
#include "eip202_cdr.h"

// internal command Descriptor interface
#include "eip202_cdr_dscr.h"


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Default configuration
#include "c_eip202_ring.h"

// EIP-202 Ring Control Driver Library internal interfaces
#include "eip202_ring_internal.h"
#include "eip202_cdr_level0.h"         // EIP-202 Level 0 macros
#include "eip202_cdr_fsm.h"             // CDR state machine
#include "eip202_cd_format.h"           // CD Format API

// Driver Framework Basic Definitions API
#include "basic_defs.h"                // IDENTIFIER_NOT_USED, bool, u32

// Driver Framework DMA Resource API
#include "dmares_types.h"         // types of the DMA resource API
#include "dmares_rw.h"            // read/write of the DMA resource API.


/*----------------------------------------------------------------------------
 * Definitions and macros
 */


/*----------------------------------------------------------------------------
 * Local variables
 */


/*----------------------------------------------------------------------------
 * eip202_lib_cdr_fill_level_finalize
 *
 */
static eip202_ring_error_t
eip202_lib_cdr_fill_level_finalize(
        const unsigned int cd_word_count,
        volatile eip202_cdr_true_io_area_t * const true_io_area_p,
        unsigned int * const fill_level_dscr_count_p)
{
#ifdef EIP202_RING_DEBUG_FSM
    eip202_ring_error_t rv;

    if(cd_word_count == 0)
        // CD Ring is empty
        rv = eip202_cdr_state_set((volatile eip202_cdr_state_t*)&true_io_area_p->state,
                                EIP202_CDR_STATE_INITIALIZED);
    else if(cd_word_count > 0 &&
            cd_word_count < (unsigned int)true_io_area_p->ring_size_word_count)
        // CD Ring is free
        rv = eip202_cdr_state_set((volatile eip202_cdr_state_t*)&true_io_area_p->state,
                                 EIP202_CDR_STATE_FREE);
    else if(cd_word_count == (unsigned int)true_io_area_p->ring_size_word_count)
        // CD Ring is full
        rv = eip202_cdr_state_set((volatile eip202_cdr_state_t*)&true_io_area_p->state,
                                 EIP202_CDR_STATE_FULL);
    else
        rv = EIP202_RING_ILLEGAL_IN_STATE;

    if(rv != EIP202_RING_NO_ERROR)
        return EIP202_RING_ILLEGAL_IN_STATE;
#endif

    // Store actual fill level
    *fill_level_dscr_count_p = cd_word_count /
                            (unsigned int)true_io_area_p->desc_offs_word_count;

    // Return actual fill level plus one descriptor to distinguish
    // ring full from ring empty
    if(cd_word_count < (unsigned int)true_io_area_p->ring_size_word_count)
        (*fill_level_dscr_count_p)++;

    return EIP202_RING_NO_ERROR;
}


/*----------------------------------------------------------------------------
  internal command Descriptor interface
  ---------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * eip202_cdr_write_cb
 */
int
eip202_cdr_write_cb(
        void * const callback_param1_p,
        const int callback_param2,
        const unsigned int write_index,
        const unsigned int write_count,
        const unsigned int total_write_limit,
        const void * descriptors_p,
        const int descriptor_count,
        const unsigned int descriptor_skip_count)
{
    device_handle_t device;
    unsigned int i, desc_offset_word_count;
    unsigned int n_written = 0;
    volatile eip202_cdr_true_io_area_t * const true_io_area_p =
                                            cdrioarea(callback_param1_p);
    if(callback_param1_p == NULL || descriptors_p == NULL)
        return -1;

    IDENTIFIER_NOT_USED(callback_param2);
    IDENTIFIER_NOT_USED(descriptor_count);
    IDENTIFIER_NOT_USED(total_write_limit);

    device = true_io_area_p->device;

    desc_offset_word_count = (unsigned int)true_io_area_p->desc_offs_word_count;

    // Write descriptors to CDR
    for(i = write_index; i < write_index + write_count; i++)
    {
        eip202_cd_write(
            true_io_area_p->ring_handle,
            i * desc_offset_word_count,
            ((const eip202_arm_command_descriptor_t *)descriptors_p) +
                                            descriptor_skip_count + n_written,
            true_io_area_p->f_atp);

        n_written++;
    }

    if (n_written > 0)
    {
        // Call PreDMA to prepared descriptors in Ring DMA buffer for handover
        // to the device (the EIP-202 DMA Master)
        dma_resource_pre_dma(true_io_area_p->ring_handle,
                           write_index * desc_offset_word_count *
                             (unsigned int)sizeof(u32),
                           n_written * desc_offset_word_count *
                             (unsigned int)sizeof(u32));

        // CDS point: hand over written command Descriptors to the device
        EIP202_CDR_COUNT_WR(device,
                            (u16)(n_written * desc_offset_word_count),
                            false);
    }

    return (int) n_written;
}


/*----------------------------------------------------------------------------
 * eip202_cdr_read_cb
 */
int
eip202_cdr_read_cb(
        void * const callback_param1_p,
        const int callback_param2,
        const unsigned int read_index,
        const unsigned int read_limit,
        void * descriptors_p,
        const unsigned int descriptor_skip_count)
{
    IDENTIFIER_NOT_USED(callback_param1_p);
    IDENTIFIER_NOT_USED(callback_param2);
    IDENTIFIER_NOT_USED(read_index);
    IDENTIFIER_NOT_USED(read_limit);
    IDENTIFIER_NOT_USED(descriptors_p);
    IDENTIFIER_NOT_USED(descriptor_skip_count);

    // Not used for CDR

    return -1;
}


/*----------------------------------------------------------------------------
 * eip202_cdr_status_cb
 */
int
eip202_cdr_status_cb(
        void * const callback_param1_p,
        const int callback_param2,
        int * const device_read_pos_p)
{
    IDENTIFIER_NOT_USED(callback_param1_p);
    IDENTIFIER_NOT_USED(callback_param2);

    *device_read_pos_p = -1;  // not used

    return 0;
}


/*----------------------------------------------------------------------------
  Descriptor I/O Driver Library API implementation
  ---------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * eip202_cdr_fill_level_get
 */
eip202_ring_error_t
eip202_cdr_fill_level_get(
        eip202_ring_io_area_t * const io_area_p,
        unsigned int * fill_level_dscr_count_p)
{
    device_handle_t device;
    unsigned int cd_word_count;
    volatile eip202_cdr_true_io_area_t * const true_io_area_p = cdrioarea(io_area_p);

    EIP202_RING_CHECK_POINTER(io_area_p);
    EIP202_RING_CHECK_POINTER(fill_level_dscr_count_p);

    device = true_io_area_p->device;

    {
        u32 value32;

        EIP202_CDR_COUNT_RD(device, &value32);

        true_io_area_p->count_rd       = value32;
        true_io_area_p->f_valid_count_rd = true;

        cd_word_count = (unsigned int)value32;
    }

    return eip202_lib_cdr_fill_level_finalize(cd_word_count,
                                            true_io_area_p,
                                            fill_level_dscr_count_p);
}


/*----------------------------------------------------------------------------
 * eip202_cdr_write_control_word
 */
u32
eip202_cdr_write_control_word(
        const eip202_cdr_control_t * const  command_ctrl_p)
{
    return eip202_cd_make_control_word(command_ctrl_p->token_word_count,
                                     command_ctrl_p->segment_byte_count,
                                     command_ctrl_p->f_first_segment,
                                     command_ctrl_p->f_last_segment,
                                     command_ctrl_p->f_force_engine,
                                     command_ctrl_p->engine_id);
}


/*----------------------------------------------------------------------------
 * eip202_cdr_descriptor_put
 *
 */
eip202_ring_error_t
eip202_cdr_descriptor_put(
        eip202_ring_io_area_t * const io_area_p,
        const eip202_arm_command_descriptor_t * command_dscr_p,
        const unsigned int dscr_requested_count,
        unsigned int * const dscr_done_count_p,
        unsigned int * fill_level_dscr_count_p)
{
    device_handle_t device;
    int res;
    unsigned int cd_word_count, cd_free_count, cd_new_requested_count;
    volatile eip202_cdr_true_io_area_t * const true_io_area_p = cdrioarea(io_area_p);

    EIP202_RING_CHECK_POINTER(io_area_p);
    EIP202_RING_CHECK_POINTER(command_dscr_p);
    EIP202_RING_CHECK_POINTER(dscr_done_count_p);
    EIP202_RING_CHECK_POINTER(fill_level_dscr_count_p);

    device = true_io_area_p->device;

    // Check how many descriptors can be put
    {
        u32 value32;

        if (true_io_area_p->f_valid_count_rd)
        {
            value32 = true_io_area_p->count_rd;
            true_io_area_p->f_valid_count_rd = false;
        }
        else
        {
            EIP202_CDR_COUNT_RD(device, &value32);
            true_io_area_p->count_rd = value32;
        }

        cd_word_count = (unsigned int)value32;
    }

    // Check if CDR is full
    if(cd_word_count == (unsigned int)true_io_area_p->ring_size_word_count)
    {
        // CD Ring is full
        *fill_level_dscr_count_p = cd_word_count /
                            (unsigned int)true_io_area_p->desc_offs_word_count;
        *dscr_done_count_p = 0;

        return eip202_cdr_state_set((volatile eip202_cdr_state_t*)&true_io_area_p->state,
                                 EIP202_CDR_STATE_FULL);
    }

    cd_free_count =
            ((unsigned int)true_io_area_p->ring_size_word_count - cd_word_count) /
                    (unsigned int)true_io_area_p->desc_offs_word_count;

    cd_new_requested_count = MIN(cd_free_count, dscr_requested_count);

    // Put command descriptors to CDR
    res = ring_helper_put((volatile ring_helper_t*)&true_io_area_p->ring_helper,
                         command_dscr_p,
                         (int)cd_new_requested_count);
    if(res >= 0)
        *dscr_done_count_p = (unsigned int)res;
    else
        return EIP202_RING_ARGUMENT_ERROR;

    // Increase the fill level by the number of successfully put descriptors
    cd_word_count += ((unsigned int)res *
                         (unsigned int)true_io_area_p->desc_offs_word_count);

    return eip202_lib_cdr_fill_level_finalize(cd_word_count,
                                           true_io_area_p,
                                           fill_level_dscr_count_p);
}


/* end of file eip202_cdr_dscr.c */
