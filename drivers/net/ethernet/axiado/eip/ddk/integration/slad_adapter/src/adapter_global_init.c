/* adapter_global_init.c
 *
 * Global Control initialization module.
 */

/*****************************************************************************
* Copyright (c) 2011-2022 by Rambus, Inc. and/or its subsidiaries.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are
* met:
*
* 1. Redistributions of source code must retain the above copyright
* notice, this list of conditions and the following disclaimer.
*
* 2. Redistributions in binary form must reproduce the above copyright
* notice, this list of conditions and the following disclaimer in the
* documentation and/or other materials provided with the distribution.
*
* 3. Neither the name of the copyright holder nor the names of its
* contributors may be used to endorse or promote products derived from
* this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
* A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
* HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
* THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*****************************************************************************/

/*----------------------------------------------------------------------------
 * This module implements (provides) the following interface(s):
 */
#include "adapter_global_init.h"


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Default Adapter configuration
#include "c_adapter_global.h"   // ADAPTER_GLOBAL_PRNG_SEED

// Global Control API: Packet I/O
#include "api_global_eip97.h"

// Driver Framework Basic Definitions API
#include "basic_defs.h"         // u8, u32, bool

// Driver Framework C Library API
#include "clib.h"               // memcpy

// EIP-97 Driver Library Global Control API
#include "eip97_global_event.h" // Event Management
#include "eip97_global_init.h"  // Init/Uninit
#include "eip97_global_prng.h"  // PRNG Control

#include "device_types.h"       // device_handle_t
#include "device_mgmt.h"        // Device_find
#include "log.h"                // Log API

#include "adapter_global_internal.h"

/*----------------------------------------------------------------------------
 * Definitions and macros
 */


/*----------------------------------------------------------------------------
 * Local variables
 */
static const u32 global_prng_key[8] = ADAPTER_GLOBAL_PRNG_SEED;

/*----------------------------------------------------------------------------
 * bool_to_string()
 *
 * Convert boolean value to string.
 */
static const char *
bool_to_string(
        const bool b)
{
    if (b)
        return "true";
    else
        return "false";
}

/*----------------------------------------------------------------------------
 * adapter_global_status_report()
 *
 * Obtain all available global status information from the EIP-97 driver
 * and report it.
 */
static void
adapter_global_status_report(void)
{
    global_control97_error_t rc;
    unsigned int i;
    unsigned int nof_p_es;
    global_control97_interfaces_get(&nof_p_es,NULL,NULL,NULL);

    LOG_INFO("\n\t\t adapter_global_status_report \n");

    LOG_CRIT("Global status of the EIP-97\n");

    for (i=0; i < nof_p_es; i++)
    {
        LOG_CRIT("Packet Engine %d status\n", i);
        {
            global_control97_dfe_status_t dfe_status;

            rc = global_control97_dfe_status_get(i, &dfe_status);

            if (rc != GLOBAL_CONTROL_NO_ERROR)
            {
                continue;
            }

            LOG_CRIT("DFE status: CD fifo Words: %d, DMA size: %d\n"
                     "AtDMA busy: %s, DataDMA busy: %s, DMA err: %s\n",
                     dfe_status.cd_fifo_word32_count,
                     dfe_status.dma_size,
                     bool_to_string(dfe_status.f_at_dma_busy),
                     bool_to_string(dfe_status.f_data_dma_busy),
                     bool_to_string(dfe_status.f_dma_error));
        }

        {
            global_control97_dse_status_t dse_status;

            rc = global_control97_dse_status_get(i, &dse_status);

            if (rc != GLOBAL_CONTROL_NO_ERROR)
            {
                continue;
            }

            LOG_CRIT("DSE status: RD fifo Words: %d, DMA size: %d\n"
                     "data flush  busy: %s, DataDMA busy: %s, DMA err: %s\n",
                     dse_status.rd_fifo_word32_count,
                     dse_status.dma_size,
                     bool_to_string(dse_status.f_data_flush_busy),
                     bool_to_string(dse_status.f_data_dma_busy),
                     bool_to_string(dse_status.f_dma_error));
        }

        {
            global_control97_token_status_t token_status;
            rc = global_control97_token_status_get(i, &token_status);

            if (rc != GLOBAL_CONTROL_NO_ERROR)
            {
                continue;
            }

            LOG_CRIT("Token status: Active: %d, loc available: %s\n"
                     "res available: %s, read active: %s, ccache active: %s\n"
                     "cntx fetch: %s, res cntx: %s\n"
                     "processing held: %s, busy: %s\n",
                     token_status.active_token_count,
                     bool_to_string(token_status.f_token_location_available),
                     bool_to_string(token_status.f_result_token_available),
                     bool_to_string(token_status.f_token_read_active),
                     bool_to_string(token_status.f_context_cache_active),
                     bool_to_string(token_status.f_context_fetch),
                     bool_to_string(token_status.f_result_context),
                     bool_to_string(token_status.f_processing_held),
                     bool_to_string(token_status.f_busy));
        }

        {
            global_control97_context_status_t context_status;

            rc = global_control97_context_status_get(i, &context_status);

            if (rc != GLOBAL_CONTROL_NO_ERROR)
            {
                continue;
            }

            LOG_CRIT("Context status: Err mask: %04x, Available: %d\n"
                     "Active cntx: %s, next cntx: %s, result cntx: %s"
                     " Err recov: %s\n",
                     context_status.error,
                     context_status.available_token_count,
                     bool_to_string(context_status.f_active_context),
                     bool_to_string(context_status.f_next_context),
                     bool_to_string(context_status.f_result_context),
                     bool_to_string(context_status.f_error_recovery));
        }

        {
            global_control97_interrupt_status_t interrupt_status;

            rc = global_control97_interrupt_status_get(i, &interrupt_status);

            if (rc != GLOBAL_CONTROL_NO_ERROR)
            {
                continue;
            }

            LOG_CRIT("Interrupt status: input DMA err: %s, output DMA err %s \n"
                     "pkt proc err: %s, pkt timeout: %s, f a t a l err: %s, "
                     "PE int out: %s\n"
                     "inp DMA enable: %s, outp DMA enable %s, "
                     "pkt proc enable: %s\n"
                     "pkt timeout enable: %s, f a t a l enable: %s,"
                     "PE int out enable: %s\n",
                     bool_to_string(interrupt_status.f_input_dma_error),
                     bool_to_string(interrupt_status.f_output_dma_error),
                     bool_to_string(interrupt_status.f_packet_processing_error),
                     bool_to_string(interrupt_status.f_packet_timeout),
                     bool_to_string(interrupt_status.f_fatal_error),
                     bool_to_string(interrupt_status.f_pe_interrupt_out),
                     bool_to_string(interrupt_status.f_input_dma_error_enabled),
                     bool_to_string(interrupt_status.f_output_dma_error_enabled),
                     bool_to_string(interrupt_status.f_packet_processing_enabled),
                     bool_to_string(interrupt_status.f_packet_timeout_enabled),
                     bool_to_string(interrupt_status.f_fatal_error_enabled),
                     bool_to_string(interrupt_status.f_pe_interrupt_out_enabled));
        }
        {
            global_control97_output_transfer_status_t out_xfer_status;

            rc = global_control97_out_xfer_status_get(i, &out_xfer_status);

            if (rc != GLOBAL_CONTROL_NO_ERROR)
            {
                continue;
            }

            LOG_CRIT("Output Transfer status: availabe: %d, "
                     "min: %d, max: %d, size mask: %d\n",
                     out_xfer_status.available_word32_count,
                     out_xfer_status.min_transfer_word_count,
                     out_xfer_status.max_transfer_word_count,
                     out_xfer_status.transfer_size_mask);
        }
        {
            global_control97_prng_status_t prng_status;

            rc = global_control97_prng_status_get(i, &prng_status);

            if (rc == GLOBAL_CONTROL_ERROR_NOT_IMPLEMENTED)
            {
                LOG_CRIT("No PRNG present in EIP-96\n");
            }
            else if (rc == GLOBAL_CONTROL_NO_ERROR)
            {
                    LOG_CRIT("PRNG status: busy: %s, res ready: %s\n",
                             bool_to_string(prng_status.f_busy),
                             bool_to_string(prng_status.f_result_ready));
            }
        }
    }
#ifdef ADAPTER_GLOBAL_DBG_STATISTICS
    {
        global_control97_debug_statistics_t debug_statistics;

        rc = global_control97_debug_statistics_get(&debug_statistics);

        if (rc != GLOBAL_CONTROL_NO_ERROR)
        {
           return;
        }

        LOG_CRIT("\nInterface DBG Statistics:\n");
        for (i=0; i<16; i++)
        {
            if(debug_statistics.ifc_packets_in[i] != 0 ||
               debug_statistics.ifc_packets_out[i] != 0)
            {
                LOG_CRIT("\t\tInterface #%2u Input pkts=%10u Output pkts=%10u\n",
                         i,
                         (unsigned int)debug_statistics.ifc_packets_in[i],
                         (unsigned int)debug_statistics.ifc_packets_out[i]);
            }
        }
        LOG_CRIT("\nPipe DBG Statistics:\n");
        for (i=0; i <nof_p_es; i++)
        {
            LOG_CRIT("\t\tPipe #%2u Total=%10u data=%10u Cur=%3u Max=%3u\n",
                     i,
                     (unsigned int)debug_statistics.pipe_total_packets[i],
                     (unsigned int)debug_statistics.pipe_data_count[i],
                     (unsigned int)debug_statistics.pipe_current_packets[i],
                     (unsigned int)debug_statistics.pipe_max_packets[i]);
        }
    }
#endif
}


/*----------------------------------------------------------------------------
 * adapter_global_init()
 *
 */
bool
adapter_global_init(void)
{
    unsigned int nof_p_es, nof_rings, nof_la_interfaces, nof_inline_interfaces;
    global_control97_error_t rc;
    unsigned int i;

    LOG_INFO("\n\t\t adapter_global_init \n");

    // Initialize the device
    rc = global_control97_init(false);
    if (rc != GLOBAL_CONTROL_NO_ERROR)
    {
        LOG_CRIT("Adaptar_Global_Init: EIP97 initialization failed\n");
        return false; // error
    }

    global_control97_interfaces_get(&nof_p_es,
                                   &nof_rings,
                                   &nof_la_interfaces,
                                   &nof_inline_interfaces);

    // First read the device capabilities
    {
        global_control97_capabilities_t capabilities;

        rc = global_control97_capabilities_get(&capabilities);
        if ( rc != GLOBAL_CONTROL_NO_ERROR)
        {
            LOG_CRIT("global_control97_capabilities_get returned error\n");
        }
        else
        {
            LOG_CRIT("Global EIP-97 capabilities: %s\n",
                     capabilities.sz_text_description);
        }
    }

    // Enable the rings for the packet engines
    {
        global_control97_ring_pe_map_t ring_pe_map;

        zeroinit(ring_pe_map);

        // Enable rings
        ring_pe_map.ring_pe_mask = (1 << (nof_rings + nof_la_interfaces + nof_inline_interfaces)) - 1;

        // Set rings priority
        ring_pe_map.ring_prio_mask = ADAPTER_GLOBAL_EIP97_PRIOMASK;

        for (i=0; i < nof_p_es; i++)
        {
            rc = global_control97_configure(i, &ring_pe_map);
            if (rc != GLOBAL_CONTROL_NO_ERROR)
            {
                LOG_CRIT("Ring configuration failed for PE %d\n", i);
                global_control97_uninit();
                return false; // error
            }
        }
    }

    {
        global_control97_prng_status_t prng_status;
        // Check whether we have a PRNG.
        rc = global_control97_prng_status_get(0, &prng_status);

        if (rc != GLOBAL_CONTROL_NO_ERROR && rc != GLOBAL_CONTROL_ERROR_NOT_IMPLEMENTED)
        {
                return false;
        }

        if (rc != GLOBAL_CONTROL_ERROR_NOT_IMPLEMENTED)
        {
            for (i=0; i < nof_p_es; i++)
            {
                global_control97_prng_reseed_t prng_reseed;

                prng_reseed.seed_lo = global_prng_key[0];
                prng_reseed.seed_hi = global_prng_key[1];
                prng_reseed.key0_lo = global_prng_key[2];
                prng_reseed.key0_hi = global_prng_key[3];
                prng_reseed.key1_lo = global_prng_key[4];
                prng_reseed.key1_hi = global_prng_key[5];
                prng_reseed.lfsr_lo = global_prng_key[6];
                prng_reseed.lfsr_hi = global_prng_key[7];

                rc = global_control97_prng_reseed(i, &prng_reseed);

                if (rc != GLOBAL_CONTROL_NO_ERROR)
                {
                    LOG_CRIT("Could not reseed PRNG of PE#%d\n",i);
                    global_control97_uninit();
                    return false; // error
                }
            }
        }
        else
        {
            LOG_CRIT("No PRNG in PEs, skip initialization\n");
        }
    }

    adapter_global_status_report();

    return true; // success
}


/*----------------------------------------------------------------------------
 * adapter_global_uninit()
 *
 */
void
adapter_global_uninit(void)
{
    LOG_INFO("\n\t\t adapter_global_uninit \n");

    adapter_global_status_report();

    global_control97_uninit();
}


/* end of file adapter_global_eip97.c */
