/* eip97_global_event.c
 *
 * EIP-97 Global Control Driver Library
 * Event Management Module
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

#include "eip97_global_event.h"


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Default configuration
#include "c_eip97_global.h"

// Driver Framework Basic Definitions API
#include "basic_defs.h"                // u32

// Driver Framework device API
#include "device_types.h"              // device_handle_t

// EIP-97 Global Control Driver Library internal interfaces
#include "eip97_global_internal.h"
#include "eip202_global_level0.h"      // EIP-202 Level 0 macros
#include "eip96_level0.h"              // EIP-96 Level 0 macros
#include "eip97_global_fsm.h"          // state machine
#include "eip97_global_level0.h"

/*----------------------------------------------------------------------------
 * Definitions and macros
 */


/*----------------------------------------------------------------------------
 * Local variables
 */

#ifdef EIP97_REG_DBG_BASE
/*----------------------------------------------------------------------------
 * eip97_global_debug_statistics_get
 */
eip97_global_error_t
eip97_global_debug_statistics_get(
        eip97_global_io_area_t * const io_area_p,
        eip97_global_debug_statistics_t * const debug_statistics_p)
{
    device_handle_t device;
    unsigned int i;
    volatile eip97_true_io_area_t * const true_io_area_p = ioarea(io_area_p);

    EIP97_GLOBAL_CHECK_POINTER(io_area_p);
    EIP97_GLOBAL_CHECK_POINTER(debug_statistics_p);

    device = true_io_area_p->device;

    for (i = 0; i < 16; i++)
    {
        EIP97_DBG_RING_IN_COUNT_RD(device,
                                   i,
                                   &debug_statistics_p->ifc_packets_in[i]);
        EIP97_DBG_RING_OUT_COUNT_RD(device,
                                    i,
                                    &debug_statistics_p->ifc_packets_out[i]);

    }
    for (i = 0; i < EIP97_GLOBAL_MAX_NOF_PE_TO_USE; i++)
    {
        EIP97_DBG_PIPE_COUNT_RD(device,
                                i,
                                &debug_statistics_p->pipe_total_packets[i],
                                &debug_statistics_p->pipe_current_packets[i],
                                &debug_statistics_p->pipe_max_packets[i]);
        EIP97_DBG_PIPE_DCOUNT_RD(device,
                                 i,
                                 &debug_statistics_p->pipe_data_count[i]);
    }
    for (i = EIP97_GLOBAL_MAX_NOF_PE_TO_USE; i < 16; i++)
    {
        debug_statistics_p->pipe_total_packets[i] = 0;
        debug_statistics_p->pipe_current_packets[i] = 0;
        debug_statistics_p->pipe_max_packets[i] = 0;
        debug_statistics_p->pipe_data_count[i] = 0;
    }
    return 0;
}
#endif

/*----------------------------------------------------------------------------
 * eip97_global_dfe_status_get
 */
eip97_global_error_t
eip97_global_dfe_status_get(
        eip97_global_io_area_t * const io_area_p,
        const unsigned int pe_number,
        eip97_global_dfe_status_t * const dfe_status_p)
{
    device_handle_t device;
    volatile eip97_true_io_area_t * const true_io_area_p = ioarea(io_area_p);
    unsigned int dfedse_offset;
    dfedse_offset = eip97_dfedse_offset_get();

    EIP97_GLOBAL_CHECK_POINTER(io_area_p);
    EIP97_GLOBAL_CHECK_POINTER(dfe_status_p);

    if(pe_number >= EIP97_GLOBAL_MAX_NOF_PE_TO_USE)
        return EIP97_GLOBAL_ARGUMENT_ERROR;

    device = true_io_area_p->device;

    EIP202_DFE_TRD_STAT_RD(device,
                           dfedse_offset,
                           pe_number,
                           &dfe_status_p->cd_fifo_word32_count,
                           &dfe_status_p->dma_size,
                           &dfe_status_p->f_at_dma_busy,
                           &dfe_status_p->f_data_dma_busy,
                           &dfe_status_p->f_dma_error);

#ifdef EIP97_GLOBAL_DEBUG_FSM
    {
        eip97_global_error_t rv;

        if(dfe_status_p->f_dma_error)
            rv = eip97_global_state_set(
                    (volatile eip97_global_state_t*)&true_io_area_p->state,
                    EIP97_GLOBAL_STATE_FATAL_ERROR);
        else
            // Remain in the current state
            rv = eip97_global_state_set(
                    (volatile eip97_global_state_t*)&true_io_area_p->state,
                    (eip97_global_state_t)true_io_area_p->state);
        if(rv != EIP97_GLOBAL_NO_ERROR)
            return EIP97_GLOBAL_ILLEGAL_IN_STATE;
    }
#endif // EIP97_GLOBAL_DEBUG_FSM

    return EIP97_GLOBAL_NO_ERROR;
}


/*----------------------------------------------------------------------------
 * eip97_global_dse_status_get
 */
eip97_global_error_t
eip97_global_dse_status_get(
        eip97_global_io_area_t * const io_area_p,
        const unsigned int pe_number,
        eip97_global_dse_status_t * const dse_status_p)
{
    device_handle_t device;
    volatile eip97_true_io_area_t * const true_io_area_p = ioarea(io_area_p);
    unsigned int dfedse_offset;
    dfedse_offset = eip97_dfedse_offset_get();

    EIP97_GLOBAL_CHECK_POINTER(io_area_p);
    EIP97_GLOBAL_CHECK_POINTER(dse_status_p);

    if(pe_number >= EIP97_GLOBAL_MAX_NOF_PE_TO_USE)
        return EIP97_GLOBAL_ARGUMENT_ERROR;

    device = true_io_area_p->device;

    EIP202_DSE_TRD_STAT_RD(device,
                           dfedse_offset,
                           pe_number,
                           &dse_status_p->rd_fifo_word32_count,
                           &dse_status_p->dma_size,
                           &dse_status_p->f_data_flush_busy,
                           &dse_status_p->f_data_dma_busy,
                           &dse_status_p->f_dma_error);

#ifdef EIP97_GLOBAL_DEBUG_FSM
    {
        eip97_global_error_t rv;

        if(dse_status_p->f_dma_error)
            rv = eip97_global_state_set(
                    (volatile eip97_global_state_t*)&true_io_area_p->state,
                    EIP97_GLOBAL_STATE_FATAL_ERROR);
        else
            // Remain in the current state
            rv = eip97_global_state_set(
                    (volatile eip97_global_state_t*)&true_io_area_p->state,
                    (eip97_global_state_t)true_io_area_p->state);
        if(rv != EIP97_GLOBAL_NO_ERROR)
            return EIP97_GLOBAL_ILLEGAL_IN_STATE;
    }
#endif // EIP97_GLOBAL_DEBUG_FSM

    return EIP97_GLOBAL_NO_ERROR;
}


/*----------------------------------------------------------------------------
 * eip97_global_eip96_token_status_get
 */
eip97_global_error_t
eip97_global_eip96_token_status_get(
        eip97_global_io_area_t * const io_area_p,
        const unsigned int pe_number,
        eip96_token_status_t * const token_status_p)
{
    device_handle_t device;
    volatile eip97_true_io_area_t * const true_io_area_p = ioarea(io_area_p);

    EIP97_GLOBAL_CHECK_POINTER(io_area_p);
    EIP97_GLOBAL_CHECK_POINTER(token_status_p);

    if(pe_number >= EIP97_GLOBAL_MAX_NOF_PE_TO_USE)
        return EIP97_GLOBAL_UNSUPPORTED_FEATURE_ERROR;

    device = true_io_area_p->device;

#ifdef EIP97_GLOBAL_DEBUG_FSM
    {
        eip97_global_error_t rv;

        // Remain in the current state
        rv = eip97_global_state_set(
                (volatile eip97_global_state_t*)&true_io_area_p->state,
                (eip97_global_state_t)true_io_area_p->state);
        if(rv != EIP97_GLOBAL_NO_ERROR)
            return EIP97_GLOBAL_ILLEGAL_IN_STATE;
    }
#endif // EIP97_GLOBAL_DEBUG_FSM

    EIP96_TOKEN_CTRL_STAT_RD(device,
                             pe_number,
                             &token_status_p->active_token_count,
                             &token_status_p->f_token_location_available,
                             &token_status_p->f_result_token_available,
                             &token_status_p->f_token_read_active,
                             &token_status_p->f_context_cache_active,
                             &token_status_p->f_context_fetch,
                             &token_status_p->f_result_context,
                             &token_status_p->f_processing_held,
                             &token_status_p->f_busy);

    return EIP97_GLOBAL_NO_ERROR;
}


/*----------------------------------------------------------------------------
 * eip97_global_eip96_context_status_get
 */
eip97_global_error_t
eip97_global_eip96_context_status_get(
        eip97_global_io_area_t * const io_area_p,
        const unsigned int pe_number,
        eip96_context_status_t * const context_status_p)
{
    device_handle_t device;
    volatile eip97_true_io_area_t * const true_io_area_p = ioarea(io_area_p);

    EIP97_GLOBAL_CHECK_POINTER(io_area_p);
    EIP97_GLOBAL_CHECK_POINTER(context_status_p);

    if(pe_number >= EIP97_GLOBAL_MAX_NOF_PE_TO_USE)
        return EIP97_GLOBAL_ARGUMENT_ERROR;

    device = true_io_area_p->device;

    EIP96_CONTEXT_STAT_RD(device,
                          pe_number,
                          &context_status_p->error,
                          &context_status_p->available_token_count,
                          &context_status_p->f_active_context,
                          &context_status_p->f_next_context,
                          &context_status_p->f_result_context,
                          &context_status_p->f_error_recovery);

#ifdef EIP97_GLOBAL_DEBUG_FSM
    {
        eip97_global_error_t rv;

        if((context_status_p->error & EIP96_TIMEOUT_FATAL_ERROR_MASK) != 0)
            rv = eip97_global_state_set(
                    (volatile eip97_global_state_t*)&true_io_area_p->state,
                    EIP97_GLOBAL_STATE_FATAL_ERROR);
        else
            // Remain in the current state
            rv = eip97_global_state_set(
                    (volatile eip97_global_state_t*)&true_io_area_p->state,
                    (eip97_global_state_t)true_io_area_p->state);
        if(rv != EIP97_GLOBAL_NO_ERROR)
            return EIP97_GLOBAL_ILLEGAL_IN_STATE;
    }
#endif // EIP97_GLOBAL_DEBUG_FSM

    return EIP97_GLOBAL_NO_ERROR;
}


/*----------------------------------------------------------------------------
 * eip97_global_eip96_out_xfer_status_get
 */
eip97_global_error_t
eip97_global_eip96_out_xfer_status_get(
        eip97_global_io_area_t * const io_area_p,
        const unsigned int pe_number,
        eip96_output_transfer_status_t * const out_xfer_status_p)
{
    device_handle_t device;
    volatile eip97_true_io_area_t * const true_io_area_p = ioarea(io_area_p);

    EIP97_GLOBAL_CHECK_POINTER(io_area_p);
    EIP97_GLOBAL_CHECK_POINTER(out_xfer_status_p);

    if(pe_number >= EIP97_GLOBAL_MAX_NOF_PE_TO_USE)
        return EIP97_GLOBAL_ARGUMENT_ERROR;

    device = true_io_area_p->device;

#ifdef EIP97_GLOBAL_DEBUG_FSM
    {
        eip97_global_error_t rv;

        // Remain in the current state
        rv = eip97_global_state_set((volatile eip97_global_state_t*)&true_io_area_p->state,
                                    (eip97_global_state_t)true_io_area_p->state);
        if(rv != EIP97_GLOBAL_NO_ERROR)
            return EIP97_GLOBAL_ILLEGAL_IN_STATE;
    }
#endif // EIP97_GLOBAL_DEBUG_FSM

    EIP96_OUT_TRANS_CTRL_STAT_RD(device,
                                 pe_number,
                                 &out_xfer_status_p->available_word32_count,
                                 &out_xfer_status_p->min_transfer_word_count,
                                 &out_xfer_status_p->max_transfer_word_count,
                                 &out_xfer_status_p->transfer_size_mask);

    return EIP97_GLOBAL_NO_ERROR;
}


/*----------------------------------------------------------------------------
 * eip97_global_eip96_prng_status_get
 */
eip97_global_error_t
eip97_global_eip96_prng_status_get(
        eip97_global_io_area_t * const io_area_p,
        const unsigned int pe_number,
        eip96_prng_status_t * const prng_status_p)
{
    device_handle_t device;
    volatile eip97_true_io_area_t * const true_io_area_p = ioarea(io_area_p);

    EIP97_GLOBAL_CHECK_POINTER(io_area_p);
    EIP97_GLOBAL_CHECK_POINTER(prng_status_p);

    if(pe_number >= EIP97_GLOBAL_MAX_NOF_PE_TO_USE)
        return EIP97_GLOBAL_ARGUMENT_ERROR;

    device = true_io_area_p->device;

#ifdef EIP97_GLOBAL_DEBUG_FSM
    {
        eip97_global_error_t rv;

        // Remain in the current state
        rv = eip97_global_state_set((volatile eip97_global_state_t*)&true_io_area_p->state,
                                    (eip97_global_state_t)true_io_area_p->state);
        if(rv != EIP97_GLOBAL_NO_ERROR)
            return EIP97_GLOBAL_ILLEGAL_IN_STATE;
    }
#endif // EIP97_GLOBAL_DEBUG_FSM

    EIP96_PRNG_STAT_RD(device,
                       pe_number,
                       &prng_status_p->f_busy,
                       &prng_status_p->f_result_ready);

    return EIP97_GLOBAL_NO_ERROR;
}


/* end of file eip97_global_event.c */
