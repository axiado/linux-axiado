/* adapter_pec_pktbuf.h
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

#ifndef ADAPTER_PEC_PKTBUF_H_
#define ADAPTER_PEC_PKTBUF_H_


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Driver Framework Basic Definitions API
#include "basic_defs.h"            // bool

// DMABuf API
#include "api_dmabuf.h"            // dma_buf_handle_t


/*----------------------------------------------------------------------------
 * Definitions and macros
 */


/*----------------------------------------------------------------------------
 * adapter_pec_pkt_data_get
 *
 * Obtain packet data from a DMA Buf handle in a contiguous buffer.
 * If the data already exists in a contiguous buffer, return a pointer to it.
 * If the desired data is spread across scatter particles, copy it into supplied
 * copy buffer.
 *
 * packet_handle (input)
 *     DMABuf handle representing the packet (either single buffer or scatter
 *     list).
 *
 * copy_buffer_p (input)
 *     Buffer to which the packet data must be copied if not contiguous.
 *     Can be NULL if data is assumed to be contiguous.
 *
 * start_offs (input)
 *     byte offset from start of the packet of first byte of packet to access.
 *
 * byte_count (input)
 *     Length of packet data to access.
 *
 * Return:
 *    Non-null pointer; access to packet data.
 *    NULL pointer: scatter list does not contain desired offset or supplied
 *                  copy buffer pointer was NULL and data was spread.
 */
u8 *
adapter_pec_pkt_data_get(
        dma_buf_handle_t packet_handle,
        u8 *copy_buffer_p,
        unsigned int start_offs,
        unsigned int byte_count);


/*----------------------------------------------------------------------------
 * adapter_pec_pkt_byte_put
 *
 * Write single byte at specific offset in packet data represented by
 * a DMABuf handle. This can be either a single buffer or a scatter list.
 *
 * packet_handle (input)
 *     DMABuf handle representing the packet to update.
 *
 * offset (input)
 *     byte offset within the packet to update.
 *
 * byte (input)
 *     byte value to write.
 */
void
adapter_pec_pkt_byte_put(
        dma_buf_handle_t packet_handle,
        unsigned int offset,
        unsigned int byte);


#endif /* ADAPTER_PEC_PKTBUF_H_ */


/* end of file adapter_pec_pktbuf.h */
