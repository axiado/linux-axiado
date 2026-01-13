/* adapter_sglist.c
 *
 * Packet Engine Control (PEC) Scatter Gather list API implementation.
 *
 */

/*****************************************************************************
* Copyright (c) 2008-2022 by Rambus, Inc. and/or its subsidiaries.
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

#include "api_pec_sg.h"         // PEC_SG_* (the API we implement here)


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Default Adapter PEC configuration
#include "c_adapter_pec.h"

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

#ifdef ADAPTER_PEC_ENABLE_SCATTERGATHER

// our internal definition of a scatter/gather list
// the DMA buffer for SGList store this (variable-length) structure
typedef struct
{
    unsigned int list_capacity;

    struct
    {
        dma_buf_handle_t handle;
        unsigned int byte_count;  // set by pec_sg_list_write and pec_packet_get
    } entries[1];       // variable-size allocated!!!

} pec_sg_list_t;


/*----------------------------------------------------------------------------
 * Adapter_Handle2SGList
 *
 * This function validates that the DMAResource handle is indeed an
 * SGList handle
 * and then returns the pointer to the pec_sg_list_t structure contained in
 * the buffer related to this handle.
 */
static pec_sg_list_t *
adapter_handle2_sg_list_ptr(
        dma_buf_handle_t sg_list_handle)
{
    dma_resource_handle_t * dma_handle =
        adapter_dma_buf_handle2_dma_resource_handle(sg_list_handle);
    dma_resource_record_t * rec_p;
    if (dma_resource_is_valid_handle(dma_handle))
        rec_p = dma_resource_handle2_record_ptr(dma_handle);
    else
        return NULL;

    // ensure it is an SGList
    if (rec_p->sg.is_sg_list != true)
        return NULL;

    return adapter_dma_resource_host_addr(
        adapter_dma_buf_handle2_dma_resource_handle(sg_list_handle));

}


/*----------------------------------------------------------------------------
 * adapter_sg_list_calc_alloc_size
 *
 * This function calculates the size of the buffer to be allocated to hold
 * an pec_sg_list_t with a given capacity.
 */
static inline unsigned int
adapter_sg_list_calc_alloc_size(
        const unsigned int list_capacity)
{
    pec_sg_list_t sg;
    unsigned int S = sizeof(pec_sg_list_t);

    S += (list_capacity - 1) * sizeof(sg.entries[1]);

    return S;
}
#endif /* ADAPTER_PEC_ENABLE_SCATTERGATHER */


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
        dma_buf_handle_t * const sg_list_handle_p)
{
#ifdef ADAPTER_PEC_ENABLE_SCATTERGATHER
    dma_resource_addr_pair_t host_addr;
    int dmares;
    dma_resource_properties_t props;

    zeroinit(props);

    if (list_capacity == 0 ||
        sg_list_handle_p == NULL)
    {
        return PEC_ERROR_BAD_PARAMETER;
    }

    // initialize the output parameter
    *sg_list_handle_p = dma_buf_null_handle;

    props.size      = adapter_sg_list_calc_alloc_size(list_capacity);
    props.alignment = adapter_dma_resource_alignment_get();

    dmares = dma_resource_alloc(props, &host_addr, &sg_list_handle_p->p);
    if (dmares != 0)
        return PEC_ERROR_INTERNAL;

    // set the special flag in the DMA Resource record for SGLists
    {
        dma_resource_handle_t dma_handle;
        dma_resource_record_t * rec_p;

        dma_handle = sg_list_handle_p->p;
        rec_p = dma_resource_handle2_record_ptr(dma_handle);

        if (!rec_p)
        {
            // corner case - avoid memory leak
            dma_resource_release(adapter_dma_buf_handle2_dma_resource_handle(
                                    *sg_list_handle_p));
            *sg_list_handle_p = dma_buf_null_handle;

            return PEC_ERROR_INTERNAL;
        }

        rec_p->sg.is_sg_list = true;
    }

    // initialize the SGList
    {
        pec_sg_list_t * const p = host_addr.address_p;
        memset(p, 0, props.size);
        p->list_capacity = list_capacity;
    }

    return PEC_STATUS_OK;
#else
    IDENTIFIER_NOT_USED(list_capacity);
    IDENTIFIER_NOT_USED(sg_list_handle_p);

    return PEC_ERROR_NOT_IMPLEMENTED;
#endif /* ADAPTER_PEC_ENABLE_SCATTERGATHER */
}


/*----------------------------------------------------------------------------
 * pec_sg_list_destroy
 *
 * This function must be used to destroy a SGList that was previously created
 * with pec_sg_list_create. The potentially referred fragments in the list are
 * not freed by the implementation!
 *
 * sg_list_handle (input)
 *     The handle to the SGList as returned by pec_sg_list_create.
 *
 * dma_buf_release may be called instead of this function.
 */
pec_status_t
pec_sg_list_destroy(
        dma_buf_handle_t sg_list_handle)
{
#ifdef ADAPTER_PEC_ENABLE_SCATTERGATHER
    int dmares;

    dmares = dma_resource_release(
        adapter_dma_buf_handle2_dma_resource_handle(sg_list_handle));

    if (dmares == 0)
        return PEC_STATUS_OK;

    return PEC_ERROR_BAD_HANDLE;
#else
    IDENTIFIER_NOT_USED(sg_list_handle.p);

    return PEC_ERROR_NOT_IMPLEMENTED;
#endif /* ADAPTER_PEC_ENABLE_SCATTERGATHER */
}


/*----------------------------------------------------------------------------
 * pec_sg_list_write
 */
pec_status_t
pec_sg_list_write(
        dma_buf_handle_t sg_list_handle,
        const unsigned int index,
        dma_buf_handle_t fragment_handle,
        const unsigned int fragment_byte_count)
{
#ifdef ADAPTER_PEC_ENABLE_SCATTERGATHER
    pec_sg_list_t * sg_list_p;

    sg_list_p = adapter_handle2_sg_list_ptr(sg_list_handle);
    if (sg_list_p == NULL)
        return PEC_ERROR_BAD_HANDLE;

    if (index >= sg_list_p->list_capacity)
        return PEC_ERROR_BAD_PARAMETER;

    sg_list_p->entries[index].handle = fragment_handle;
    sg_list_p->entries[index].byte_count = fragment_byte_count;

    return PEC_STATUS_OK;
#else
    IDENTIFIER_NOT_USED(sg_list_handle.p);
    IDENTIFIER_NOT_USED(index);
    IDENTIFIER_NOT_USED(fragment_handle.p);
    IDENTIFIER_NOT_USED(fragment_byte_count);

    return PEC_ERROR_NOT_IMPLEMENTED;
#endif /* ADAPTER_PEC_ENABLE_SCATTERGATHER */
}


/*----------------------------------------------------------------------------
 * pec_sg_list_read
 */
pec_status_t
pec_sg_list_read(
        dma_buf_handle_t sg_list_handle,
        const unsigned int index,
        dma_buf_handle_t * const fragment_handle_p,
        unsigned int * const fragment_size_in_bytes_p,
        u8 ** const fragment_ptr_p)
{
#ifdef ADAPTER_PEC_ENABLE_SCATTERGATHER
    pec_sg_list_t * sg_list_p;

    // initialize the output parameters
    {
        if (fragment_handle_p)
            *fragment_handle_p = dma_buf_null_handle;

        if (fragment_size_in_bytes_p)
            *fragment_size_in_bytes_p = 0;

        if (fragment_ptr_p)
            *fragment_ptr_p = NULL;
    }

    sg_list_p = adapter_handle2_sg_list_ptr(sg_list_handle);
    if (sg_list_p == NULL)
        return PEC_ERROR_BAD_HANDLE;

    if (index >= sg_list_p->list_capacity)
        return PEC_ERROR_BAD_PARAMETER;

    // fill in the output parameters
    {
        if (fragment_handle_p)
            *fragment_handle_p = sg_list_p->entries[index].handle;

        if (fragment_size_in_bytes_p)
            *fragment_size_in_bytes_p = sg_list_p->entries[index].byte_count;

        if (fragment_ptr_p)
        {
            // retrieve the host address from the DMA resource record
            dma_resource_handle_t dma_handle;
            dma_resource_record_t * rec_p;

            dma_handle = adapter_dma_buf_handle2_dma_resource_handle(
                sg_list_p->entries[index].handle);
            rec_p = dma_resource_handle2_record_ptr(dma_handle);
            if (rec_p)
            {
                // it is a valid handle
                *fragment_ptr_p = adapter_dma_resource_host_addr(dma_handle);
            }
        }
    }

    return PEC_STATUS_OK;
#else
    IDENTIFIER_NOT_USED(sg_list_handle.p);
    IDENTIFIER_NOT_USED(index);
    IDENTIFIER_NOT_USED(fragment_handle_p);
    IDENTIFIER_NOT_USED(fragment_size_in_bytes_p);
    IDENTIFIER_NOT_USED(fragment_ptr_p);

    return PEC_ERROR_NOT_IMPLEMENTED;
#endif /* ADAPTER_PEC_ENABLE_SCATTERGATHER */
}


/*----------------------------------------------------------------------------
 * pec_sg_list_get_capacity
 */
pec_status_t
pec_sg_list_get_capacity(
        dma_buf_handle_t sg_list_handle,
        unsigned int * const list_capacity_p)
{
#ifdef ADAPTER_PEC_ENABLE_SCATTERGATHER
    pec_sg_list_t * sg_list_p;

    if (list_capacity_p == NULL)
        return PEC_ERROR_BAD_PARAMETER;

    // initialize the output parameter
    *list_capacity_p = 0;

    sg_list_p = adapter_handle2_sg_list_ptr(sg_list_handle);
    if (sg_list_p != NULL)
        *list_capacity_p = sg_list_p->list_capacity;

    return PEC_STATUS_OK;
#else
    IDENTIFIER_NOT_USED(sg_list_handle.p);
    IDENTIFIER_NOT_USED(list_capacity_p);

    return PEC_ERROR_NOT_IMPLEMENTED;
#endif /* ADAPTER_PEC_ENABLE_SCATTERGATHER */
}



/* end of file adapter_pec_sglist.c */
