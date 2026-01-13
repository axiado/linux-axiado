/* eip202_rdr_event.c
 *
 * EIP-202 Ring Control Driver Library
 * CDR Event Management API implementation
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

// EIP-202 Ring Control Driver Library internal interfaces
#include "eip202_ring_internal.h"
#include "eip202_rdr_level0.h"         // EIP-202 Level 0 macros
#include "eip202_rdr_fsm.h"             // RDR state machine

// Driver Framework Basic Definitions API
#include "basic_defs.h"                // IDENTIFIER_NOT_USED, bool, u32


/*----------------------------------------------------------------------------
 * Definitions and macros
 */


/*----------------------------------------------------------------------------
 * Local variables
 */


/*----------------------------------------------------------------------------
 * eip202_rdr_status_get
 */
eip202_ring_error_t
eip202_rdr_status_get(
        eip202_ring_io_area_t * const io_area_p,
        eip202_rdr_status_t * const status_p)
{
    device_handle_t device;
    eip202_ring_error_t rv;
    volatile eip202_rdr_true_io_area_t * const true_io_area_p = rdrioarea(io_area_p);

    EIP202_RING_CHECK_POINTER(io_area_p);
    EIP202_RING_CHECK_POINTER(status_p);

    device = true_io_area_p->device;

    EIP202_RDR_PREP_COUNT_RD(device, &status_p->rd_prep_word_count);
    EIP202_RDR_PROC_COUNT_RD(device,
                             &status_p->rd_proc_word_count,
                             &status_p->rd_proc_pkt_word_count);

    EIP202_RDR_STAT_RD(device,
                       &status_p->f_dma_error,
                       &status_p->f_treshold_int,
                       &status_p->f_error,
                       &status_p->f_ou_flow_error,
                       &status_p->f_timeout_int,
                       &status_p->f_rd_buf_overflow_int,
                       &status_p->f_rd_overflow_int,
                       &status_p->rdfifo_word_count);

    // Transit to a new state
    if(status_p->f_dma_error)
        rv = eip202_rdr_state_set((volatile eip202_rdr_state_t*)&true_io_area_p->state,
                                 EIP202_RDR_STATE_FATAL_ERROR);
    else
        // Remain in the current state
        rv = eip202_rdr_state_set((volatile eip202_rdr_state_t*)&true_io_area_p->state,
                                 (eip202_rdr_state_t)true_io_area_p->state);
    if(rv != EIP202_RING_NO_ERROR)
        return EIP202_RING_ILLEGAL_IN_STATE;

    return EIP202_RING_NO_ERROR;
}


/*----------------------------------------------------------------------------
 * eip202_rdr_processed_fill_level_high_int_enable
 */
eip202_ring_error_t
eip202_rdr_processed_fill_level_high_int_enable(
        eip202_ring_io_area_t * const io_area_p,
        const unsigned int threshold_dscr_count,
        const unsigned int timeout,
        const bool f_int_per_packet)
{
    device_handle_t device;
    eip202_ring_error_t rv;
    volatile eip202_rdr_true_io_area_t * const true_io_area_p = rdrioarea(io_area_p);

    EIP202_RING_CHECK_POINTER(io_area_p);

    if(EIP202_RING_RD_INTERRUPTS_PER_PACKET_FLAG == 0 && f_int_per_packet)
        return EIP202_RING_ARGUMENT_ERROR;

    device = true_io_area_p->device;

    if (f_int_per_packet)
    {
        EIP202_RDR_THRESH_WR(device,
                             (u32)threshold_dscr_count,
                             // Set packet processing mode,
                             // e.g. interrupts per packet i.s.o. descriptor
                             true,
                             (u8)timeout);
    }
    else
    {
        EIP202_RDR_THRESH_WR(device,
                             (u32)threshold_dscr_count *
                             true_io_area_p->desc_offs_word_count,
                             // Set packet processing mode,
                             // e.g. interrupts per packet i.s.o. descriptor
                             false,
                             (u8)timeout);
    }

#ifdef EIP202_CLUSTERED_WRITES_DISABLE
    // Prevent clustered write operations, break them with a read operation
    // Note: Reading the EIP202_RDR_RING_BASE_ADDR_LO register
    //       has no side effects!
    eip202_rdr_read32(device, EIP202_RDR_RING_BASE_ADDR_LO);
#endif

    // Remain in the current state
    rv = eip202_rdr_state_set((volatile eip202_rdr_state_t*)&true_io_area_p->state,
                             (eip202_rdr_state_t)true_io_area_p->state);
    if(rv != EIP202_RING_NO_ERROR)
        return EIP202_RING_ILLEGAL_IN_STATE;

    return EIP202_RING_NO_ERROR;
}


/*----------------------------------------------------------------------------
 * eip202_rdr_processed_fill_level_high_int_clear_and_disable
 *
 */
eip202_ring_error_t
eip202_rdr_processed_fill_level_high_int_clear_and_disable(
        eip202_ring_io_area_t * const io_area_p,
        const bool f_ovfl_int_only)
{
    device_handle_t device;
    eip202_ring_error_t rv;
    volatile eip202_rdr_true_io_area_t * const true_io_area_p = rdrioarea(io_area_p);

    EIP202_RING_CHECK_POINTER(io_area_p);

    device = true_io_area_p->device;

    if(f_ovfl_int_only)
    {
        // Clear Descriptor and Buffer overflow interrupts
        EIP202_RDR_STAT_CLEAR_DSCR_BUF_OFLO_IRQ_WR(device);
    }
    else
    {
        // Disable Processed Descriptor threshold interrupt,
        // Disable timeout interrupt and stop timeout counter for
        // reducing power consumption
        EIP202_RDR_THRESH_WR(
                           device,
                           true_io_area_p->ring_size_word_count,
                           0,  // Set descriptor processing mode
                           0); // Disable timeout

#ifdef EIP202_CLUSTERED_WRITES_DISABLE
        // Prevent clustered write operations, break them with a read operation
        // Note: Reading the EIP202_RDR_RING_BASE_ADDR_LO register
        //       has no side effects!
        eip202_rdr_read32(device, EIP202_RDR_RING_BASE_ADDR_LO);
#endif

        // Clear all RDR interrupts
        EIP202_RDR_STAT_CLEAR_ALL_IRQ_WR(device);

#ifdef EIP202_CLUSTERED_WRITES_DISABLE
        // Prevent clustered write operations, break them with a read operation
        // Note: Reading the EIP202_RDR_RING_BASE_ADDR_LO register
        //       has no side effects!
        eip202_rdr_read32(device, EIP202_RDR_RING_BASE_ADDR_LO);
#endif
    }

    // Remain in the current state
    rv = eip202_rdr_state_set((volatile eip202_rdr_state_t*)&true_io_area_p->state,
                             (eip202_rdr_state_t)true_io_area_p->state);
    if(rv != EIP202_RING_NO_ERROR)
        return EIP202_RING_ILLEGAL_IN_STATE;

    return EIP202_RING_NO_ERROR;
}


/* end of file eip202_rdr_event.c */
