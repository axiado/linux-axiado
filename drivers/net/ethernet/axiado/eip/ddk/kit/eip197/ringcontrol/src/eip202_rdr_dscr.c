/* eip202_rdr_dscr.c
 *
 * EIP-202 Ring Control Driver Library
 * 1) Descriptor I/O Driver Library API implementation
 * 2) internal Result Descriptor interface implementation
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
#include "eip202_rdr.h"

// internal Result Descriptor interface
#include "eip202_rdr_dscr.h"


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Default configuration
#include "c_eip202_ring.h"

// EIP-202 Ring Control Driver Library internal interfaces
#include "eip202_ring_internal.h"
#include "eip202_rdr_level0.h"         // EIP-202 Level 0 macros
#include "eip202_rdr_fsm.h"             // RDR state machine
#include "eip202_rd_format.h"           // RD Format API

// Driver Framework Basic Definitions API
#include "basic_defs.h"                // IDENTIFIER_NOT_USED, bool, u32

// Driver Framework DMA Resource API
#include "dmares_types.h"         // types of the DMA resource API
#include "dmares_rw.h"            // read/write of the DMA resource API.

// Standard IOToken API
#include "iotoken.h"

#include "log.h"
/*----------------------------------------------------------------------------
 * Definitions and macros
 */

// EIP-202 HW limit for the number of packets to acknowledge at once
#define EIP202_RING_MAX_RD_PACKET_COUNT      127


/*----------------------------------------------------------------------------
 * Local variables
 */

/*----------------------------------------------------------------------------
 * eip202_lib_rdr_prepared_fill_level_get
 */
static eip202_ring_error_t
eip202_lib_rdr_prepared_fill_level_get(
        const device_handle_t device,
        volatile eip202_rdr_true_io_area_t * const true_io_area_p,
        unsigned int * const fill_level_dscr_count_p)
{
    unsigned int rd_word_count;
    eip202_ring_error_t rv;

    EIP202_RDR_PREP_COUNT_RD(device, (u32*)&rd_word_count);

    // Remain in the current state
    rv = eip202_rdr_state_set((volatile eip202_rdr_state_t*)&true_io_area_p->state,
                             (eip202_rdr_state_t)true_io_area_p->state);
    if(rv != EIP202_RING_NO_ERROR)
        return EIP202_RING_ILLEGAL_IN_STATE;

    *fill_level_dscr_count_p = rd_word_count /
                            (unsigned int)true_io_area_p->desc_offs_word_count;

    if(rd_word_count < (unsigned int)true_io_area_p->ring_size_word_count)
        (*fill_level_dscr_count_p)++;

    return EIP202_RING_NO_ERROR;
}


/*----------------------------------------------------------------------------
 * eip202_lib_rdr_processed_fill_level_finalize
 */
static eip202_ring_error_t
eip202_lib_rdr_processed_fill_level_finalize(
        const unsigned int rd_word_count,
        const unsigned int pkt_count,
        volatile eip202_rdr_true_io_area_t * const true_io_area_p,
        unsigned int * const fill_level_dscr_count_p,
        unsigned int * const fill_level_pkt_count_p)
{
#ifdef EIP202_RING_DEBUG_FSM
    eip202_ring_error_t rv;

    if(rd_word_count == 0)
        // CD Ring is empty
        rv = eip202_rdr_state_set((volatile eip202_rdr_state_t*)&true_io_area_p->state,
                                EIP202_RDR_STATE_INITIALIZED);
    else if(rd_word_count > 0 &&
            rd_word_count < (unsigned int)true_io_area_p->ring_size_word_count)
        // CD Ring is free
        rv = eip202_rdr_state_set((volatile eip202_rdr_state_t*)&true_io_area_p->state,
                                 EIP202_RDR_STATE_FREE);
    else if(rd_word_count == (unsigned int)true_io_area_p->ring_size_word_count)
        // CD Ring is full
        rv = eip202_rdr_state_set((volatile eip202_rdr_state_t*)&true_io_area_p->state,
                                 EIP202_RDR_STATE_FULL);
    else
        rv = EIP202_RING_ILLEGAL_IN_STATE;

    if(rv != EIP202_RING_NO_ERROR)
        return EIP202_RING_ILLEGAL_IN_STATE;
#endif

    *fill_level_dscr_count_p = rd_word_count /
                            (unsigned int)true_io_area_p->desc_offs_word_count;

    *fill_level_pkt_count_p = pkt_count;

    return EIP202_RING_NO_ERROR;
}


/*----------------------------------------------------------------------------
  internal Result Descriptor interface
  ---------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * eip202_rdr_write_cb
 */
int
eip202_rdr_write_cb(
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
    volatile eip202_rdr_true_io_area_t * const true_io_area_p =
                                            rdrioarea(callback_param1_p);
    if(callback_param1_p == NULL || descriptors_p == NULL)
        return -1;

    IDENTIFIER_NOT_USED(callback_param2);
    IDENTIFIER_NOT_USED(descriptor_count);
    IDENTIFIER_NOT_USED(total_write_limit);

    device = true_io_area_p->device;

    desc_offset_word_count = (unsigned int)true_io_area_p->desc_offs_word_count;

    // Write descriptors to RDR
    for(i = write_index; i < write_index + write_count; i++)
    {
        eip202_prepared_write(
            true_io_area_p->ring_handle,
            i * desc_offset_word_count,
            ((const eip202_arm_prepared_descriptor_t *)descriptors_p) +
                    descriptor_skip_count + n_written);

        n_written++;
    }

    if (n_written > 0)
    {
        // Call PreDMA to prepared descriptors in Ring DMA buffer for handover
        // to the device (the EIP-202 DMA Master)
        dma_resource_pre_dma(true_io_area_p->ring_handle,
                           write_index *
                              desc_offset_word_count *
                               (unsigned int)sizeof(u32),
                           n_written *
                             desc_offset_word_count *
                               (unsigned int)sizeof(u32));

        // CDS point: hand over written Prepared Descriptors to the device
        EIP202_RDR_PREP_COUNT_WR(device,
                                 (u16)(n_written * desc_offset_word_count),
                                 false);
    }

    return (int)n_written;
}


/*----------------------------------------------------------------------------
 * eip202_rdr_read_cb
 */
int
eip202_rdr_read_cb(
        void * const callback_param1_p,
        const int callback_param2,
        const unsigned int read_index,
        const unsigned int read_limit,
        void * descriptors_p,
        const unsigned int descriptor_skip_count)
{
    int interface_id;
    device_handle_t device;
    unsigned int i, desc_offset_word_count;
    bool f_got_descriptor, f_last_segment = false, f_first_segment = false;
    unsigned int got_dscr_count = 0, got_pkt_count = 0;
    volatile eip202_rdr_true_io_area_t * const true_io_area_p =
                                       rdrioarea(callback_param1_p);

    if(callback_param1_p == NULL || descriptors_p == NULL)
        return -1;

    /* param2 passed as ring-id */
    interface_id = callback_param2;

    device = true_io_area_p->device;

    desc_offset_word_count = (unsigned int )true_io_area_p->desc_offs_word_count;

    // Read descriptors from RDR
    for(i = read_index; i < read_index + read_limit; i++)
    {
        eip202_arm_result_descriptor_t * current_result_desc_p =
            ((eip202_arm_result_descriptor_t *)descriptors_p) +
                    descriptor_skip_count + got_dscr_count;

#define EIP202_RING_RDR_ALL_DESCRIPTORS_TO_GET_DONE             \
         (true_io_area_p->acknowledged_rd_count + got_dscr_count >=  \
              true_io_area_p->rd_to_get_count)

// This can be true only if the pkt_requested_count parameter in
// the eip202_rdr_descriptor_get() function is set to a non-zero value
#define EIP202_RING_RDR_ALL_PACKETS_TO_GET_DONE                 \
         (true_io_area_p->pkt_to_get_count > 0 &&                   \
          true_io_area_p->acknowledged_pkt_count + got_pkt_count >=  \
              true_io_area_p->pkt_to_get_count)

        // Stop reading the descriptors if all the requested
        // descriptors and packet chains have been read
        if(EIP202_RING_RDR_ALL_PACKETS_TO_GET_DONE)
        {
            if(EIP202_RING_RDR_ALL_DESCRIPTORS_TO_GET_DONE)
                break; // for
        }

        // Call PostDMA before reading descriptors from
        // the EIP-202 DMA Master
        dma_resource_post_dma(true_io_area_p->ring_handle,
                            i * desc_offset_word_count *
                              (unsigned int)sizeof(u32),
                            desc_offset_word_count *
                              (unsigned int)sizeof(u32));

        // Check if a processed result descriptor is received
#if defined(EIP202_RDR_OWNERSHIP_WORD_ENABLE)
        {
            u32 ownership_word =
                        dma_resource_read32(true_io_area_p->ring_handle,
                                           i * desc_offset_word_count +
                                                 desc_offset_word_count - 1);
            f_got_descriptor = (ownership_word == EIP202_RDR_OWNERSHIP_WORD_PATTERN);
            LOG_DEBG("%s: interface=%d rdr_index=%d sign=0x%x",
                     __func__, interface_id, i, ownership_word);
#ifdef EIP202_RING_BUS_KEEPALIVE_WORKAROUND
            // Read from the device to solve a keep-alive problem in some
            // bus environments.
            device_read32(device,0);
#endif
        }
#elif defined(EIP202_RING_ANTI_DMA_RACE_CONDITION_CDS)
        current_result_desc_p->token_p[io_token_out_mark_offset_get()] =
                dma_resource_read32(true_io_area_p->ring_handle,
                                   i * desc_offset_word_count +
                                   true_io_area_p->token_offset_word_count +
                                   io_token_out_mark_offset_get());
        f_got_descriptor = (io_token_mark_check(current_result_desc_p->token_p) == 0);
#else
        f_got_descriptor = true; // according to EIP202_RDR_PROC_COUNT_RD()
#endif

        if (f_got_descriptor)
        {
            // Read Result Descriptor
            eip202_read_descriptor(current_result_desc_p,
                                  true_io_area_p->ring_handle,
                                  i * desc_offset_word_count,
                                  desc_offset_word_count,
                                  true_io_area_p->token_offset_word_count,
                                  &f_last_segment,
                                  &f_first_segment);

            // Stop reading the descriptors if all the requested
            // packet chains have been read and a new processed packet descriptor
            // chain is detected
            if(f_first_segment && EIP202_RING_RDR_ALL_PACKETS_TO_GET_DONE)
                break; // for

            if (f_first_segment)
                true_io_area_p->packet_found = true;

            got_dscr_count++;

            if(true_io_area_p->packet_found && f_last_segment)
            {
                got_pkt_count++;
                true_io_area_p->packet_found = false;
            }

#if defined(EIP202_RING_ANTI_DMA_RACE_CONDITION_CDS) || \
    defined(EIP202_RDR_OWNERSHIP_WORD_ENABLE)
            // Clear this descriptor
            eip202_clear_descriptor(current_result_desc_p,
                                   true_io_area_p->ring_handle,
                                   i * desc_offset_word_count,
                                   true_io_area_p->token_offset_word_count,
                                   desc_offset_word_count);

            // Ensure next PostDMA does not undo the clear operation above
            dma_resource_pre_dma(
                true_io_area_p->ring_handle,
                i * desc_offset_word_count * (unsigned int)sizeof(u32),
                desc_offset_word_count * (unsigned int)sizeof(u32));
#endif
        }
        else
        {
            // The f_got_descriptor is set in eip202_read_descriptor() and
            // depends on the Application ID field in the result descriptor
            // when EIP202_RING_ANTI_DMA_RACE_CONDITION_CDS is defined.
            // In case of a packet descriptor chain the Engine writes the
            // Application ID field for the last segment descriptor only
            // but we need to acknowledge all the descriptors of the chain
            break; // for
        }
    } // for

    // Check if there are processed result descriptors and packets
    // that must be acknowledged
    if (got_dscr_count > 0)
    {
        unsigned int new_got_pkt_count;

#if EIP202_RING_RD_INTERRUPTS_PER_PACKET_FLAG != 1
        got_pkt_count = 0;
#endif

        // EIP-202 HW limits the number of packets to acknowledge at once to
        // EIP202_RING_MAX_RD_PACKET_COUNT packets
        new_got_pkt_count = MIN(got_pkt_count, EIP202_RING_MAX_RD_PACKET_COUNT);

        // CDS point: hand over read Result Descriptors to the device
        EIP202_RDR_PROC_COUNT_WR(
                device,
                (u16)(got_dscr_count * desc_offset_word_count),
                (u8)new_got_pkt_count,
                false);
    }

    // Update acknowledged packets counter
    true_io_area_p->acknowledged_pkt_count += got_pkt_count;

    // Update acknowledged descriptors counter
    true_io_area_p->acknowledged_rd_count += got_dscr_count;

    return (int)got_dscr_count;
}


/*----------------------------------------------------------------------------
 * eip202_rdr_status_cb
 */
int
eip202_rdr_status_cb(
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
 * eip202_rdr_fill_level_get
 */
eip202_ring_error_t
eip202_rdr_fill_level_get(
        eip202_ring_io_area_t * const io_area_p,
        unsigned int * fill_level_dscr_count_p)
{
    int fill_level;
    volatile eip202_rdr_true_io_area_t * const true_io_area_p = rdrioarea(io_area_p);

    EIP202_RING_CHECK_POINTER(io_area_p);
    EIP202_RING_CHECK_POINTER(fill_level_dscr_count_p);

    fill_level = ring_helper_fill_level_get(
            (volatile ring_helper_t*)&true_io_area_p->ring_helper);

    if(fill_level < 0)
        return EIP202_RING_UNSUPPORTED_FEATURE_ERROR;
    else
    {
        *fill_level_dscr_count_p = (unsigned int)fill_level;
        return EIP202_RING_NO_ERROR;
    }
}


/*----------------------------------------------------------------------------
 * eip202_rdr_prepared_fill_level_get
 */
eip202_ring_error_t
eip202_rdr_prepared_fill_level_get(
        eip202_ring_io_area_t * const io_area_p,
        unsigned int * fill_level_dscr_count_p)
{
    device_handle_t device;
    volatile eip202_rdr_true_io_area_t * const true_io_area_p = rdrioarea(io_area_p);

    EIP202_RING_CHECK_POINTER(io_area_p);
    EIP202_RING_CHECK_POINTER(fill_level_dscr_count_p);

    device = true_io_area_p->device;

    return eip202_lib_rdr_prepared_fill_level_get(device,
                                               true_io_area_p,
                                               fill_level_dscr_count_p);
}


/*----------------------------------------------------------------------------
 * eip202_rdr_write_prepared_control_word
 *
 * This helper function returns the control word that can be written to
 * the logical prepared descriptor.
 *
 * This function is re-entrant.
 *
 */
u32
eip202_rdr_write_prepared_control_word(
        const eip202_rdr_prepared_control_t * const  prepared_ctrl_p)
{
    return eip202_rd_make_control_word(prepared_ctrl_p->expected_result_word_count,
                                     prepared_ctrl_p->prep_segment_byte_count,
                                     prepared_ctrl_p->f_first_segment,
                                     prepared_ctrl_p->f_last_segment);
}


/*----------------------------------------------------------------------------
 * eip202_rdr_descriptor_prepare
 */
eip202_ring_error_t
eip202_rdr_descriptor_prepare(
        eip202_ring_io_area_t * const io_area_p,
        const eip202_arm_prepared_descriptor_t * prepared_dscr_p,
        const unsigned int dscr_requested_count,
        unsigned int * const dscr_prepared_count_p,
        unsigned int * fill_level_dscr_count_p)
{
    int res, fill_level;
    unsigned int rd_word_count, rd_free_count, dscr_new_requested_count;
    volatile eip202_rdr_true_io_area_t * const true_io_area_p = rdrioarea(io_area_p);

    EIP202_RING_CHECK_POINTER(io_area_p);
    EIP202_RING_CHECK_POINTER(prepared_dscr_p);
    EIP202_RING_CHECK_POINTER(dscr_prepared_count_p);
    EIP202_RING_CHECK_POINTER(fill_level_dscr_count_p);

    // Check how many more descriptors can be added (prepared) to RDR
    fill_level = ring_helper_fill_level_get(
            (volatile ring_helper_t*)&true_io_area_p->ring_helper);
    if(fill_level < 0)
        return EIP202_RING_ARGUMENT_ERROR; // error, RDR admin corrupted

    // Check if RDR is full
    rd_word_count = ((unsigned int)fill_level *
            (unsigned int)true_io_area_p->desc_offs_word_count);
    if(rd_word_count == (unsigned int)true_io_area_p->ring_size_word_count)
    {
        // RD Ring is full
        *fill_level_dscr_count_p = (unsigned int)fill_level;
        *dscr_prepared_count_p = 0;

        // Remain in the current state
        return eip202_rdr_state_set((volatile eip202_rdr_state_t*)&true_io_area_p->state,
                                   (eip202_rdr_state_t)true_io_area_p->state);
    }

    // Calculate the maximum number of descriptors that can be added to RDR
    rd_free_count =
            ((unsigned int)true_io_area_p->ring_size_word_count - rd_word_count) /
                    (unsigned int)true_io_area_p->desc_offs_word_count;

    dscr_new_requested_count = MIN(rd_free_count, dscr_requested_count);

    res = ring_helper_put((volatile ring_helper_t*)&true_io_area_p->ring_helper,
                         prepared_dscr_p,
                         (int)dscr_new_requested_count);
    if(res >= 0)
        *dscr_prepared_count_p = (unsigned int)res;
    else
        return EIP202_RING_ARGUMENT_ERROR;

    // Get the current RDR fill level
    fill_level = ring_helper_fill_level_get(
            (volatile ring_helper_t*)&true_io_area_p->ring_helper);
    if(fill_level < 0)
        return EIP202_RING_ARGUMENT_ERROR; // error, RDR admin corrupted
    else
    {
        *fill_level_dscr_count_p = (unsigned int)fill_level;
        return EIP202_RING_NO_ERROR;
    }
}


/*----------------------------------------------------------------------------
 * eip202_rdr_processed_fill_level_get
 */
eip202_ring_error_t
eip202_rdr_processed_fill_level_get(
        eip202_ring_io_area_t * const io_area_p,
        unsigned int * fill_level_dscr_count_p,
        unsigned int * fill_level_pkt_count_p)
{
// If configured then the driver cannot rely on the register counter
#if defined(EIP202_RING_ANTI_DMA_RACE_CONDITION_CDS) || \
    defined(EIP202_RDR_OWNERSHIP_WORD_ENABLE)
    IDENTIFIER_NOT_USED(fill_level_dscr_count_p);
    IDENTIFIER_NOT_USED(fill_level_pkt_count_p);
    IDENTIFIER_NOT_USED(io_area_p);

    return EIP202_RING_UNSUPPORTED_FEATURE_ERROR;
#else
    device_handle_t device;
    unsigned int rd_word_count, pkt_count;
    volatile eip202_rdr_true_io_area_t * const true_io_area_p = rdrioarea(io_area_p);

    EIP202_RING_CHECK_POINTER(io_area_p);
    EIP202_RING_CHECK_POINTER(fill_level_dscr_count_p);
    EIP202_RING_CHECK_POINTER(fill_level_pkt_count_p);

    device = true_io_area_p->device;

    {
        u8 value8;
        u32 value32;

        EIP202_RDR_PROC_COUNT_RD(device, &value32, &value8);

        rd_word_count = (unsigned int)value32;
        pkt_count = (unsigned int)value8;
    }

    return eip202_lib_rdr_processed_fill_level_finalize(rd_word_count,
                                                     pkt_count,
                                                     true_io_area_p,
                                                     fill_level_dscr_count_p,
                                                     fill_level_pkt_count_p);
#endif // EIP202_RING_ANTI_DMA_RACE_CONDITION_CDS
}


/*----------------------------------------------------------------------------
 * eip202_rdr_read_processed_control_word
 */
void
eip202_rdr_read_processed_control_word(
        eip202_arm_result_descriptor_t * const  res_dscr_p,
        eip202_rdr_result_control_t * const rd_control_p,
        eip202_rdr_result_token_t * const res_token_p)
{
    IDENTIFIER_NOT_USED(res_token_p);

    eip202_rd_read_control_word(res_dscr_p->proc_control_word,
                               NULL,
                               rd_control_p,
                               NULL);

    return;
}


/*----------------------------------------------------------------------------
 * eip202_rdr_read_processed_bypass_data
 */
void
eip202_rdr_read_processed_bypass_data(
        const eip202_rdr_result_token_t * const  res_token_p,
        eip202_rdr_bypass_data_t * const bd_p)
{
    eip202_rd_read_bypass_data(res_token_p->bypass_data_p,
                              res_token_p->bypass_word_count,
                              bd_p);

    return;
}


/*----------------------------------------------------------------------------
 * eip202_rdr_descriptor_get
 */
eip202_ring_error_t
eip202_rdr_descriptor_get(
        eip202_ring_io_area_t * const io_area_p,
        eip202_arm_result_descriptor_t * result_dscr_p,
        const unsigned int pkt_requested_count,
        const unsigned int dscr_requested_count,
        unsigned int * const dscr_done_count_p,
        unsigned int * fill_level_dscr_count_p)
{
    device_handle_t device;
    int res;
    unsigned int rd_word_count, proc_dsrc_count, proc_pkt_count;
    volatile eip202_rdr_true_io_area_t * const true_io_area_p = rdrioarea(io_area_p);

    EIP202_RING_CHECK_POINTER(io_area_p);
    EIP202_RING_CHECK_POINTER(result_dscr_p);
    EIP202_RING_CHECK_POINTER(dscr_done_count_p);
    EIP202_RING_CHECK_POINTER(fill_level_dscr_count_p);

    if(dscr_requested_count == 0)
        return EIP202_RING_ARGUMENT_ERROR;

    device = true_io_area_p->device;

    // Check how many descriptors can be obtained
    {
        u8 value8;
        u32 value32;

#ifdef EIP202_RDR_OWNERSHIP_WORD_ENABLE
        value32 = MIN(pkt_requested_count, dscr_requested_count);
        value8  = MIN(EIP202_RING_MAX_RD_PACKET_COUNT, value32);
        value32 = value8 * true_io_area_p->desc_offs_word_count;
        IDENTIFIER_NOT_USED(device);
#else
        EIP202_RDR_PROC_COUNT_RD(device, &value32, &value8);
#endif
        rd_word_count = (unsigned int)value32;
        proc_pkt_count = (unsigned int)value8;
    }

    // Check if RDR is empty or
    // if RDR has no fully processed packet descriptor chain
    if(rd_word_count == 0 ||
       (pkt_requested_count != 0 && proc_pkt_count == 0))
    {
        // Nothing to do
        *fill_level_dscr_count_p = 0;
        *dscr_done_count_p = 0;

        return eip202_rdr_state_set((volatile eip202_rdr_state_t*)&true_io_area_p->state,
                                   EIP202_RDR_STATE_INITIALIZED);
    }

    proc_dsrc_count = rd_word_count /
                    (unsigned int)true_io_area_p->desc_offs_word_count;

    true_io_area_p->acknowledged_rd_count = 0;
    true_io_area_p->acknowledged_pkt_count = 0;
    true_io_area_p->rd_to_get_count = MIN(proc_dsrc_count, dscr_requested_count);
    true_io_area_p->pkt_to_get_count = MIN(proc_pkt_count, pkt_requested_count);

    // Get processed (result) descriptors
    res = ring_helper_get((volatile ring_helper_t*)&true_io_area_p->ring_helper,
// If configured then the driver cannot rely on the register counter
#if defined(EIP202_RING_ANTI_DMA_RACE_CONDITION_CDS) || \
    defined(EIP202_RDR_OWNERSHIP_WORD_ENABLE)
                         -1, // Certainly available RD count unknown
#else
                         (int)true_io_area_p->rd_to_get_count,
#endif // EIP202_RING_ANTI_DMA_RACE_CONDITION_CDS
                         result_dscr_p,
                         (int)dscr_requested_count);
    if(res >= 0)
        *dscr_done_count_p = (unsigned int)res;
    else
        return EIP202_RING_ARGUMENT_ERROR;

    // Increase the fill level by the number of successfully got descriptors
    rd_word_count -= ((unsigned int)res *
                         (unsigned int)true_io_area_p->desc_offs_word_count);

    // Increase the fill level by the number of acknowledged packets
    proc_pkt_count -= true_io_area_p->acknowledged_pkt_count;

    return eip202_lib_rdr_processed_fill_level_finalize(rd_word_count,
                                                     proc_pkt_count,
                                                     true_io_area_p,
                                                     fill_level_dscr_count_p,
                                                     &proc_pkt_count);
}


/* end of file eip202_rdr_dscr.c */
