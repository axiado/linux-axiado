/* api_pec_sg.h
 *
 * Packet Engine Control API (PEC)
 * Extension for Scatter/Gather (fragmented packet buffers)
 *
 * This API can be used to perform transforms on security protocol packets
 * for a set of security network protocols like IPSec, MACSec, sRTP, SSL,
 * DTLS, etc.
 *
 * Please note that this is a generic API that can be used for many transform
 * engines. A separate document will detail the exact fields and parameters
 * required for the SA and Descriptors.
 */

/*****************************************************************************
* Copyright (c) 2007-2022 by Rambus, Inc. and/or its subsidiaries.
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

#ifndef INCLUDE_GUARD_API_PEC_SG_H
#define INCLUDE_GUARD_API_PEC_SG_H

#include "basic_defs.h"
#include "api_dmabuf.h"         // dma_buf_handle_t
#include "api_pec.h"            // pec_status_t


/*----------------------------------------------------------------------------
 * pec_sg_list_create
 *
 * This function must be used to create a list that can hold references to
 * packet buffer fragments. The returned handle can be used in pec_packet_put
 * instead of a normal (contiguous) buffers.
 *
 * list_capacity (input)
 *     The number of scatter and/or gather fragments that this list can hold.
 *
 * sg_list_handle_p (output)
 *     Pointer to the output parameter that will be filled in with the handle
 *     that represents the newly created SGList.
 */
pec_status_t
pec_sg_list_create(
        const unsigned int list_capacity,
        dma_buf_handle_t * const sg_list_handle_p);


/*----------------------------------------------------------------------------
 * pec_sg_list_destroy
 *
 * This function must be used to destroy a SGList that was previously created
 * with pec_sg_list_create. The potentially referred fragments in the list are
 * not freed by the implementation!
 *
 * sg_list_handle (input)
 *     The handle to the SGList as returned by pec_sg_list_create.
 */
pec_status_t
pec_sg_list_destroy(
        dma_buf_handle_t sg_list_handle);


/*----------------------------------------------------------------------------
 * pec_sg_list_write
 *
 * This function can be used to write a specific entry in the SGList with a
 * packet fragment buffer information (handle, bytes used)
 *
 * sg_list_handle (input)
 *     The handle to the SGList as returned by pec_sg_list_create.
 *
 * index (input)
 *     Position in the SGList to write. This value must be in the range from
 *     0..list_capacity-1.
 *
 * fragment_handle (input)
 *     handle for the fragment buffer.
 *
 * fragment_byte_count (input)
 *     number of bytes used in this fragment. Only used for gather.
 */
pec_status_t
pec_sg_list_write(
        dma_buf_handle_t sg_list_handle,
        const unsigned int index,
        dma_buf_handle_t fragment_handle,
        const unsigned int fragment_byte_count);


/*----------------------------------------------------------------------------
 * pec_sg_list_read
 *
 * This function can be used to read one entry in the SGList. The function
 * returns the handle together with the host address and size of the buffer.
 *
 * sg_list_handle (input)
 *     The handle to the SGList as returned by pec_sg_list_create.
 *
 * index (input)
 *     Position in the SGList to write. This value must be in the range from
 *     0..list_capacity-1.
 *
 * fragment_handle_p (output)
 *     Pointer to the output parameter that will receive the DMABuf handle
 *     stored in this position of the list.
 *     This parameter is optional and may be set to NULL.
 *
 * fragment_size_in_bytes_p (output)
 *     Pointer to the output parameter that will receive the size of the
 *     buffer represented by the fragment_handle.
 *     This parameter is optional and may be set to NULL.
 *
 * fragment_ptr_p (output)
 *     Pointer to the output parameter (of type u8 *) that will receive
 *     the address to the start of th ebuffer represented by fragment_handle.
 *     This parameter is optional and may be set to NULL.
 */
pec_status_t
pec_sg_list_read(
        dma_buf_handle_t sg_list_handle,
        const unsigned int index,
        dma_buf_handle_t * const fragment_handle_p,
        unsigned int * const fragment_size_in_bytes_p,
        u8 ** const fragment_ptr_p);


/*----------------------------------------------------------------------------
 * pec_sg_list_get_capacity
 *
 * This helper function can be used to retrieve the capacity of an SGList,
 * so the caller does not have to remember this.
 *
 * sg_list_handle (input)
 *     The handle to the SGList as returned by pec_sg_list_create.
 *
 * list_capacity_p (output)
 *     Pointer to the output variable that will receive the capacity of the
 *     SGList, assumingn sg_list_handle is valid.
 */
pec_status_t
pec_sg_list_get_capacity(
        dma_buf_handle_t sg_list_handle,
        unsigned int * const list_capacity_p);


#endif /* Include Guard */

/* end of file api_pec_sg.h */
