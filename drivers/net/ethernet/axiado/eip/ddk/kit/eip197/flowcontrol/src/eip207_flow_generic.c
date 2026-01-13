/* eip207_flow_generic.c
 *
 * Partial EIP-207 Flow Control Generic API implementation
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

// EIP-207 Driver Library Flow Control Generic API
#include "eip207_flow_generic.h"


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Default configuration
#include "c_eip207_flow.h"

// Driver Framework Basic Definitions API
#include "basic_defs.h"                 // u32

// Driver Framework C Run-time Library API
#include "clib.h"                       // zeroinit, memset, memcpy

// Driver Framework device API
#include "device_types.h"               // device_handle_t

// Driver Framework DMA Resource API
#include "dmares_types.h"
#include "dmares_rw.h"

// EIP-207 Flow Control Driver Library internal interfaces
#include "eip207_flow_level0.h"         // EIP-207 Level 0 macros
#include "eip207_flow_internal.h"

// EIP-207 Firmware API
#include "firmware_eip207_api_flow_cs.h" // Classification API: Flow Control
#include "firmware_eip207_api_cs.h"      // Classification API: General

/*----------------------------------------------------------------------------
 * Definitions and macros
 */


/*----------------------------------------------------------------------------
 * Local variables
 */


/*----------------------------------------------------------------------------
 * eip207_lib_flow_detect
 *
 * Checks the presence of EIP-207c hardware. Returns true when found.
 */
static bool
eip207_lib_flow_detect(
        const device_handle_t device)
{
    u32 value;

    value = eip207_flow_read32(device, EIP207_FLUE_FHT_REG_VERSION);
    if (!EIP207_FLUE_SIGNATURE_MATCH( value ))
        return false;

    // read-write test one of the registers

    // Set MASK_31_BITS bits of the EIP207_FLUE_REG_HASHBASE_LO register
    eip207_flow_write32( device,
                         EIP207_FLUE_FHT_REG_HASHBASE_LO(0),
                         ~MASK_2_BITS);
    value = eip207_flow_read32(device, EIP207_FLUE_FHT_REG_HASHBASE_LO(0));
    if ((value) != ~MASK_2_BITS)
        return false;

    // Clear MASK_31_BITS bits of the EIP207_FLUE_REG_HASHBASE_LO register
    eip207_flow_write32(device, EIP207_FLUE_FHT_REG_HASHBASE_LO(0), 0);
    value = eip207_flow_read32(device, EIP207_FLUE_FHT_REG_HASHBASE_LO(0));
    if (value != 0)
       return false;

    return true;
}


#ifdef EIP207_FLOW_DEBUG_FSM
/*----------------------------------------------------------------------------
 * EIP207Lib_Flow_State_Set
 *
 */
eip207_flow_error_t
eip207_flow_state_set(
        eip207_flow_state_t * const current_state,
        const eip207_flow_state_t new_state)
{
    switch(*current_state)
    {
        case EIP207_FLOW_STATE_INITIALIZED:
            switch(new_state)
            {
                case EIP207_FLOW_STATE_ENABLED:
                    *current_state = new_state;
                    break;
                case EIP207_FLOW_STATE_FATAL_ERROR:
                    *current_state = new_state;
                    break;
                default:
                    return EIP207_FLOW_ILLEGAL_IN_STATE;
            }
            break;

        case EIP207_FLOW_STATE_ENABLED:
            switch(new_state)
            {
                case EIP207_FLOW_STATE_INSTALLED:
                    *current_state = new_state;
                    break;
                case EIP207_FLOW_STATE_FATAL_ERROR:
                    *current_state = new_state;
                    break;
                default:
                    return EIP207_FLOW_ILLEGAL_IN_STATE;
            }
            break;

        case EIP207_FLOW_STATE_INSTALLED:
            switch(new_state)
            {
                case EIP207_FLOW_STATE_INSTALLED:
                    *current_state = new_state;
                    break;
                case EIP207_FLOW_STATE_ENABLED:
                    *current_state = new_state;
                    break;
                case EIP207_FLOW_STATE_FATAL_ERROR:
                    *current_state = new_state;
                    break;
                default:
                    return EIP207_FLOW_ILLEGAL_IN_STATE;
            }
            break;

        default:
            return EIP207_FLOW_ILLEGAL_IN_STATE;
    }

    return EIP207_FLOW_NO_ERROR;
}
#endif // EIP207_FLOW_DEBUG_FSM


/*----------------------------------------------------------------------------
 * eip207_flow_io_area_byte_count_get
 */
unsigned int
eip207_flow_io_area_byte_count_get(void)
{
    return sizeof(eip207_flow_true_io_area_t);
}


/*----------------------------------------------------------------------------
 * eip207_flow_ht_entry_word_count_get
 */
unsigned int
eip207_flow_ht_entry_word_count_get(void)
{
    return EIP207_FLOW_HT_ENTRY_WORD_COUNT;
}


/*----------------------------------------------------------------------------
 * eip207_flow_init
 */
eip207_flow_error_t
eip207_flow_init(
        eip207_flow_io_area_t * const io_area_p,
        const device_handle_t device)
{
    volatile eip207_flow_true_io_area_t * const true_io_area_p = ioarea(io_area_p);

    EIP207_FLOW_CHECK_POINTER(io_area_p);

    // Detect presence of EIP-207c HW hardware
    if (!eip207_lib_flow_detect(device))
        return EIP207_FLOW_UNSUPPORTED_FEATURE_ERROR;

    // Initialize the IO Area
    true_io_area_p->device = device;

#ifdef EIP207_FLOW_DEBUG_FSM
    {
        true_io_area_p->rec_installed_counter = 0;

        true_io_area_p->state = (u32)EIP207_FLOW_STATE_INITIALIZED;
    }
#endif // EIP207_FLOW_DEBUG_FSM

    return EIP207_FLOW_NO_ERROR;
}


/*----------------------------------------------------------------------------
 * eip207_flow_id_compute
 *
 */
eip207_flow_error_t
eip207_flow_id_compute(
        eip207_flow_io_area_t * const io_area_p,
        const unsigned int hash_table_id,
        const eip207_flow_selector_params_t * const selector_params_p,
        eip207_flow_id_t * const flow_id)
{
    u32 h1, h2, h3, h4;
    u32 w;
    const u32 * p;
    u32 count, data [FIRMWARE_EIP207_CS_FLOW_HASH_ID_INPUT_WORD_COUNT];
    device_handle_t device;
    firmware_eip207_cs_flow_selector_params_t selectors;
    volatile eip207_flow_true_io_area_t * const true_io_area_p = ioarea(io_area_p);

    EIP207_FLOW_CHECK_POINTER(io_area_p);
    EIP207_FLOW_CHECK_POINTER(selector_params_p);
    EIP207_FLOW_CHECK_POINTER(flow_id);

    device = true_io_area_p->device;

    // Read the iv from the flow hash engine in the classification engine
    // The iv must have been already installed in the flow hash engine
    // via the Global Classification Control API
    EIP207_FHASH_IV_RD(device, hash_table_id, &h1, &h2, &h3, &h4);

    // Install the selectors for the flow hash ID calculation
    zeroinit(selectors);
    selectors.flags   = selector_params_p->flags;
    selectors.dst_ip_p = selector_params_p->dst_ip_p;
    selectors.dst_port = selector_params_p->dst_port;
    selectors.ip_proto = selector_params_p->ip_proto;
    selectors.spi     = selector_params_p->spi;
#ifdef FIRMWARE_EIP207_CS_FLOW_DTLS_SUPPORTED
    selectors.epoch     = selector_params_p->epoch;
#endif
    selectors.src_ip_p = selector_params_p->src_ip_p;
    selectors.src_port = selector_params_p->src_port;
#ifdef FIRMWARE_EIP207_CS_FLOW_SELECT_MACSEC
    selectors.ether_type = selector_params_p->ether_type;
    selectors.an      = selector_params_p->an;
#endif
    firmware_eip207_cs_flow_selectors_reorder(&selectors, data, &count);

    p = data;

    while (p < &data[count - 3])
    {
        w = *p++;
        h2 ^= w;
        h1 += w;
        h1 += h1 << 10;
        h1 ^= h1 >> 6;

        w = *p++;
        h3 ^= w;
        h1 += w;
        h1 += h1 << 10;
        h1 ^= h1 >> 6;

        w = *p++;
        h4 ^= w;
        h1 += w;
        h1 += h1 << 10;
        h1 ^= h1 >> 6;

        /* Mixing step for the 96 bits in h2-h4.  The code comes from a
           hash table lookup function by Robert J. Jenkins, and has been
           presented on numerous web pages and in a Dr. Dobbs Journal
           sometimes in late 90's.

           h1 is computed according to the one-at-a-time hash function,
           presented in the same article. */
        h2 -= h3;  h2 -= h4;  h2 ^= h4 >> 13;
        h3 -= h4;  h3 -= h2;  h3 ^= h2 << 8;
        h4 -= h2;  h4 -= h3;  h4 ^= h3 >> 13;
        h2 -= h3;  h2 -= h4;  h2 ^= h4 >> 12;
        h3 -= h4;  h3 -= h2;  h3 ^= h2 << 16;
        h4 -= h2;  h4 -= h3;  h4 ^= h3 >> 5;
        h2 -= h3;  h2 -= h4;  h2 ^= h4 >> 3;
        h3 -= h4;  h3 -= h2;  h3 ^= h2 << 10;
        h4 -= h2;  h4 -= h3;  h4 ^= h3 >> 15;
    } // while

    w = *p++;
    h1 += w;
    h1 += h1 << 10;
    h1 ^= h1 >> 6;
    h2 ^= w;

    if (p < data + count)
    {
        w = *p++;
        h1 += w;
        h1 += h1 << 10;
        h1 ^= h1 >> 6;
        h3 ^= w;

        if (p < data + count)
        {
            w = *p++;
            h1 += w;
            h1 += h1 << 10;
            h1 ^= h1 >> 6;
            h4 ^= w;
        }
    }

    flow_id->word32[0] = h1;
    flow_id->word32[1] = h2;
    flow_id->word32[2] = h3;
    flow_id->word32[3] = h4;

    return EIP207_FLOW_NO_ERROR;
}


/*----------------------------------------------------------------------------
 * eip207_flow_fr_word_count_get
 */
unsigned int
eip207_flow_fr_word_count_get(void)
{
    return FIRMWARE_EIP207_CS_FRC_RECORD_WORD_COUNT;
}


/*----------------------------------------------------------------------------
 * eip207_flow_fr_read
 */
eip207_flow_error_t
eip207_flow_fr_read(
        eip207_flow_io_area_t * const io_area_p,
        const unsigned int hash_table_id,
        eip207_flow_fr_dscr_t * const fr_dscr_p,
        eip207_flow_fr_output_data_t * const flow_data_p)
{
    eip207_flow_error_t rv;

    EIP207_FLOW_CHECK_POINTER(io_area_p);
    EIP207_FLOW_CHECK_POINTER(fr_dscr_p);
    EIP207_FLOW_CHECK_POINTER(flow_data_p);
    EIP207_FLOW_CHECK_INT_ATMOST(hash_table_id + 1,
                                 EIP207_FLOW_MAX_NOF_FLOW_HASH_TABLES_TO_USE);

    IDENTIFIER_NOT_USED(hash_table_id);

#ifdef EIP207_FLOW_CONSISTENCY_CHECK
    // Consistency check for the provided flow descriptor
    if (fr_dscr_p->dma_addr.addr == EIP207_FLOW_RECORD_DUMMY_ADDRESS)
        return EIP207_FLOW_INTERNAL_ERROR;
#endif // EIP207_FLOW_CONSISTENCY_CHECK

    // Prepare the flow record for reading
    dma_resource_post_dma(fr_dscr_p->dma_handle, 0, 0);

    // Read the flow record data

    // Recent record Packets counter
    flow_data_p->packets_counter =
            dma_resource_read32(fr_dscr_p->dma_handle,
                               FIRMWARE_EIP207_CS_FLOW_FR_STAT_PKT_WORD_OFFSET);

    // Recent record time stamp
    rv = eip207_flow_internal_read64(
                       fr_dscr_p->dma_handle,
                       FIRMWARE_EIP207_CS_FLOW_FR_TIME_STAMP_LO_WORD_OFFSET,
                       FIRMWARE_EIP207_CS_FLOW_FR_TIME_STAMP_HI_WORD_OFFSET,
                       &flow_data_p->last_time_lo,
                       &flow_data_p->last_time_hi);
    if (rv != EIP207_FLOW_NO_ERROR)
        return rv;

    // Recent record Octets counter
    rv = eip207_flow_internal_read64(
                       fr_dscr_p->dma_handle,
                       FIRMWARE_EIP207_CS_FLOW_FR_STAT_OCT_LO_WORD_OFFSET,
                       FIRMWARE_EIP207_CS_FLOW_FR_STAT_OCT_HI_WORD_OFFSET,
                       &flow_data_p->octets_counter_lo,
                       &flow_data_p->octets_counter_hi);
    if (rv != EIP207_FLOW_NO_ERROR)
        return rv;

#ifdef EIP207_FLOW_DEBUG_FSM
    {
        volatile eip207_flow_true_io_area_t * const true_io_area_p = ioarea(io_area_p);
        u32 state = true_io_area_p->state;

        // Remain in the current state
        rv = eip207_flow_state_set(
                (eip207_flow_state_t* const)&state,
                (eip207_flow_state_t)true_io_area_p->state);

        true_io_area_p->state = state;

        if (rv != EIP207_FLOW_NO_ERROR)
            return EIP207_FLOW_ILLEGAL_IN_STATE;
    }
#else
    IDENTIFIER_NOT_USED(io_area_p);
#endif // EIP207_FLOW_DEBUG_FSM

    return EIP207_FLOW_NO_ERROR;
}


/*----------------------------------------------------------------------------
 * eip207_flow_tr_word_count_get
 */
unsigned int
eip207_flow_tr_word_count_get(void)
{
    return FIRMWARE_EIP207_CS_TRC_RECORD_WORD_COUNT;
}


/*----------------------------------------------------------------------------
 * eip207_flow_tr_read
 */
eip207_flow_error_t
eip207_flow_tr_read(
        eip207_flow_io_area_t * const io_area_p,
        const unsigned int hash_table_id,
        const eip207_flow_tr_dscr_t * const tr_dscr_p,
        eip207_flow_tr_output_data_t * const xform_data_p)
{
    eip207_flow_error_t rv;
    u32 value32, seq_nr_word_offset;

    EIP207_FLOW_CHECK_POINTER(io_area_p);
    EIP207_FLOW_CHECK_POINTER(tr_dscr_p);
    EIP207_FLOW_CHECK_POINTER(xform_data_p);
    EIP207_FLOW_CHECK_INT_ATMOST(hash_table_id + 1,
                                 EIP207_FLOW_MAX_NOF_FLOW_HASH_TABLES_TO_USE);

    IDENTIFIER_NOT_USED(io_area_p);
    IDENTIFIER_NOT_USED(hash_table_id);

    // Prepare the transform record for reading
    dma_resource_post_dma(tr_dscr_p->dma_handle, 0, 0);

    // Read the transform record data

    // Recent record Packets counter
    xform_data_p->packets_counter =
            dma_resource_read32(tr_dscr_p->dma_handle,
                               FIRMWARE_EIP207_CS_FLOW_TR_STAT_PKT_WORD_OFFSET);

    // Read Token Context Instruction word
    value32 =
        dma_resource_read32(tr_dscr_p->dma_handle,
                           FIRMWARE_EIP207_CS_FLOW_TR_TK_CTX_INST_WORD_OFFSET);

    // Extract the Sequence number word offset from the read value
    firmware_eip207_cs_flow_seq_num_offset_read(value32, &seq_nr_word_offset);

    // Read the sequence number
    xform_data_p->sequence_number =
            dma_resource_read32(tr_dscr_p->dma_handle, seq_nr_word_offset);

    // Recent record time stamp
    rv = eip207_flow_internal_read64(
                   tr_dscr_p->dma_handle,
                   FIRMWARE_EIP207_CS_FLOW_TR_TIME_STAMP_LO_WORD_OFFSET,
                   FIRMWARE_EIP207_CS_FLOW_TR_TIME_STAMP_HI_WORD_OFFSET,
                   &xform_data_p->last_time_lo,
                   &xform_data_p->last_time_hi);
    if (rv != EIP207_FLOW_NO_ERROR)
        return rv;

    // Recent record Octets counter
    rv = eip207_flow_internal_read64(
                   tr_dscr_p->dma_handle,
                   FIRMWARE_EIP207_CS_FLOW_TR_STAT_OCT_LO_WORD_OFFSET,
                   FIRMWARE_EIP207_CS_FLOW_TR_STAT_OCT_HI_WORD_OFFSET,
                   &xform_data_p->octets_counter_lo,
                   &xform_data_p->octets_counter_hi);
    if (rv != EIP207_FLOW_NO_ERROR)
        return rv;

    return EIP207_FLOW_NO_ERROR;
}


/* end of file eip207_flow_generic.c */
