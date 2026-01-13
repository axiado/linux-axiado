/* adapter_pec_pktbuf.c
 *
 * Helper functions to access packet data via DMABuf handles, possibly
 * in scatter-gather lists.
 */

/*****************************************************************************
* Copyright (c) 2020-2021 by Rambus, Inc. and/or its subsidiaries.
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

#include "adapter_pec_pktbuf.h"

/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Default Adapter PEC configuration
#include "c_adapter_pec.h"

#include "api_pec_sg.h"         // PEC_SG_* (the API we implement here)


// DMABuf API
#include "api_dmabuf.h"         // DMABuf_*

// Adapter DMABuf internal API
#include "adapter_dmabuf.h"

// Logging API
#include "log.h"

// Driver Framework DMAResource API
#include "dmares_types.h"       // dma_resource_handle_t
#include "dmares_mgmt.h"        // DMAResource management functions
#include "dmares_rw.h"          // DMAResource buffer access.
#include "dmares_addr.h"        // DMAResource addr translation functions.
#include "dmares_buf.h"         // DMAResource buffer allocations

// Driver Framework C Run-Time Library API
#include "clib.h"               // memcpy, memset

// Driver Framework Basic Definitions API
#include "basic_defs.h"         // bool, u32


/*----------------------------------------------------------------------------
 * Definitions and macros
 */

/*----------------------------------------------------------------------------
 * adapter_pec_pkt_data_get
 */
u8 *
adapter_pec_pkt_data_get(
        dma_buf_handle_t packet_handle,
        u8 *copy_buffer_p,
        unsigned int start_offs,
        unsigned int byte_count)
{
#ifdef ADAPTER_PEC_ENABLE_SCATTERGATHER
    unsigned int nof_particles;
    pec_sg_list_get_capacity(packet_handle, &nof_particles);
    if (nof_particles != 0)
    {
        unsigned int total_byte_count = 0;
        unsigned int bytes_copied = 0;
        unsigned int bytes_skip;
        unsigned int i;
        unsigned int fragment_byte_count;
        dma_buf_handle_t fragment_handle;
        u8 *fragment_ptr;
        for (i=0; i<nof_particles; i++)
        {
            pec_sg_list_read(packet_handle, i, &fragment_handle,
                            &fragment_byte_count, &fragment_ptr);
            if (total_byte_count + fragment_byte_count >=
                start_offs + byte_count)
            {
                // This is the last fragment to visit (possibly the first).
                if (bytes_copied == 0)
                {
                    // No need to copy everything is in single fragment.
                    return fragment_ptr + start_offs - total_byte_count;
                }
                else
                {
                    // Copy the final fragment.
                    memcpy(copy_buffer_p + bytes_copied,
                           fragment_ptr, byte_count - bytes_copied);
                    return copy_buffer_p;
                }
            }
            else if (total_byte_count + fragment_byte_count >
                     start_offs)
            {
                // This fragment contains data that must be copied
                if (bytes_copied == 0)
                {
                    // First fragment containing data to copy, may need to skip
                    // bytes at start of fragment.
                    bytes_skip = start_offs - total_byte_count;
                }
                else
                {   // Later fragments, copy from start of fragment.
                    bytes_skip = 0;
                }
                if (copy_buffer_p == NULL)
                    return NULL; // Skip copying altogether.
                memcpy(copy_buffer_p + bytes_copied,
                       fragment_ptr + bytes_skip,
                       fragment_byte_count - bytes_skip);
                bytes_copied += fragment_byte_count - bytes_skip;
            }
            total_byte_count += fragment_byte_count;
        }
        // We haven't collected enough data here, return NULL pointer.
        return NULL;
    }
    else
#endif
    {
        dma_resource_handle_t dma_handle =
            adapter_dma_buf_handle2_dma_resource_handle(packet_handle);
        u8 *packet_p;
        IDENTIFIER_NOT_USED(copy_buffer_p);
        IDENTIFIER_NOT_USED(byte_count);
        if (dma_resource_is_valid_handle(dma_handle))
        {
            packet_p = (u8 *)adapter_dma_resource_host_addr(dma_handle) + start_offs;
        }
        else
        {
            packet_p = NULL;
        }
        return packet_p;
    }
}

/*----------------------------------------------------------------------------
 * adapter_pec_pkt_byte_put
 */
void
adapter_pec_pkt_byte_put(
        dma_buf_handle_t packet_handle,
        unsigned int offset,
        unsigned int byte)
{
#ifdef ADAPTER_PEC_ENABLE_SCATTERGATHER
    unsigned int nof_particles;
    pec_sg_list_get_capacity(packet_handle, &nof_particles);
    if (nof_particles != 0)
    {
        unsigned int total_byte_count = 0;
        unsigned int i;
        unsigned int fragment_byte_count;
        dma_buf_handle_t fragment_handle;
        u8 *fragment_ptr;
        for (i=0; i<nof_particles; i++)
        {
            pec_sg_list_read(packet_handle, i, &fragment_handle,
                            &fragment_byte_count, &fragment_ptr);
            if (total_byte_count + fragment_byte_count > offset)
            {
                // Found the fragment where the byte must be changed..
                fragment_ptr[offset - total_byte_count] = byte;
                return;
            }
            total_byte_count += fragment_byte_count;
        }
    }
    else
#endif
    {
        dma_resource_handle_t dma_handle =
            adapter_dma_buf_handle2_dma_resource_handle(packet_handle);
        u8 *packet_p;
        if (dma_resource_is_valid_handle(dma_handle))
        {
            packet_p = (u8 *)adapter_dma_resource_host_addr(dma_handle) + offset;
            packet_p[0] = byte;
        }
    }
}




/* end of file adapter_pec_dmabuf.c */
