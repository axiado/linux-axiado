/* adapter_dmabuf.c
 *
 * Implementation of the DMA Buffer Allocation API.
 */

/*****************************************************************************
* Copyright (c) 2008-2020 by Rambus, Inc. and/or its subsidiaries.
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

// DMABuf API
#include "api_dmabuf.h"

// Adapter DMABuf internal API
#include "adapter_dmabuf.h"


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Logging API
#include "log.h"

// Driver Framework DMAResource API
#include "dmares_types.h"
#include "dmares_mgmt.h"
#include "dmares_buf.h"
#include "dmares_addr.h"

// Driver Framework Basic Definitions API
#include "basic_defs.h"

// Driver Framework C Run-Time Library API
#include "clib.h"               // memcmp


/*----------------------------------------------------------------------------
 * Definitions and macros
 */


/*----------------------------------------------------------------------------
 * Local variables
 */

/*----------------------------------------------------------------------------
 * dma_buf_null_handle
 *
 */
const dma_buf_handle_t dma_buf_null_handle = { NULL };

// Initial DMA buffer alignment setting
static int adapter_dma_buf_alignment = ADAPTER_DMABUF_ALIGNMENT_INVALID;


/*----------------------------------------------------------------------------
 * adapter_dma_buf_handle2_dma_resource_handle
 */
dma_resource_handle_t
adapter_dma_buf_handle2_dma_resource_handle(
        dma_buf_handle_t handle)
{
    if (handle.p == NULL)
    {
        return NULL;
    }
    else
    {
        return (dma_resource_handle_t)handle.p;
    }
}


/*----------------------------------------------------------------------------
 * adapter_dma_resource_handle2_dma_buf_handle
 */
dma_buf_handle_t
adapter_dma_resource_handle2_dma_buf_handle(
        dma_resource_handle_t handle)
{
    dma_buf_handle_t dma_buf_handle;

    dma_buf_handle.p = handle;

    return dma_buf_handle;
}


/*----------------------------------------------------------------------------
 * adapter_dma_resource_is_foreign_allocated
 */
bool
adapter_dma_resource_is_foreign_allocated(
        dma_resource_handle_t handle)
{
    dma_resource_record_t * rec_p;

    rec_p = dma_resource_handle2_record_ptr(handle);

    if(!rec_p)
    {
        return false;
    }
    else
    {
        return (rec_p->allocator_ref != 'A' && rec_p->allocator_ref != 'R');
    }
}


/*----------------------------------------------------------------------------
 * adapter_dma_resource_host_addr
 */
void *
adapter_dma_resource_host_addr(
        dma_resource_handle_t handle)
{
    dma_resource_addr_pair_t host_addr;

    dma_resource_translate(handle, DMARES_DOMAIN_HOST, &host_addr);

    return host_addr.address_p;
}


/*----------------------------------------------------------------------------
 * adapter_dma_resource_is_sub_range_of
 *
 * Return true if the address range defined by handle1 is
 * within the address range defined by handle2.
 */
bool
adapter_dma_resource_is_sub_range_of(
        const dma_resource_handle_t handle1,
        const dma_resource_handle_t handle2)
{
    dma_resource_addr_pair_t addr_pair1, addr_pair2;

    dma_resource_translate(handle1, DMARES_DOMAIN_HOST, &addr_pair1);
    dma_resource_translate(handle2, DMARES_DOMAIN_HOST, &addr_pair2);

    if (addr_pair1.domain == addr_pair2.domain)
    {
        const u8 * addr1 = addr_pair1.address_p;
        const u8 * addr2 = addr_pair2.address_p;
        const dma_resource_record_t * const rec1_p =
                                dma_resource_handle2_record_ptr(handle1);
        const dma_resource_record_t * const rec2_p =
                                dma_resource_handle2_record_ptr(handle2);

        if ((rec1_p->props.size <= rec2_p->props.size) &&
            (addr2 <= addr1) &&
            ((addr1 + rec1_p->props.size) <= (addr2 + rec2_p->props.size)))
        {
            return true;
        }
    }

    return false;
}


/*----------------------------------------------------------------------------
 * adapter_dma_resource_alignment_set
 */
void
adapter_dma_resource_alignment_set(
        const int alignment)
{
    adapter_dma_buf_alignment = alignment;

    return;
}


/*----------------------------------------------------------------------------
 * adapter_dma_resource_alignment_get
 */
int
adapter_dma_resource_alignment_get(void)
{
    return adapter_dma_buf_alignment;
}


/*----------------------------------------------------------------------------
 * dma_buf_handle_is_same
 */
bool
dma_buf_handle_is_same(
        const dma_buf_handle_t * const handle1_p,
        const dma_buf_handle_t * const handle2_p)
{
    if (memcmp(handle1_p, handle2_p, sizeof(dma_buf_handle_t)) == 0)
    {
        return true;
    }

    return false;
}


/*----------------------------------------------------------------------------
 * dma_buf_alloc
 */
dma_buf_status_t
dma_buf_alloc(
        const dma_buf_properties_t requested_properties,
        dma_buf_host_address_t * const buffer_p,
        dma_buf_handle_t * const handle_p)
{
    dma_resource_handle_t dma_handle;
    dma_resource_addr_pair_t addr_pair;
    dma_resource_properties_t actual_properties;

    zeroinit(addr_pair);
    zeroinit(actual_properties);

    if (handle_p == NULL ||
        buffer_p == NULL)
    {
        return DMABUF_ERROR_BAD_ARGUMENT;
    }

    // initialize the output parameters
    handle_p->p = NULL;
    buffer_p->p = NULL;

    actual_properties.size       = requested_properties.size;
    actual_properties.bank       = requested_properties.bank;
    actual_properties.f_cached    = requested_properties.f_cached;

    if (adapter_dma_buf_alignment != ADAPTER_DMABUF_ALIGNMENT_INVALID &&
        requested_properties.alignment < adapter_dma_buf_alignment)
        actual_properties.alignment = adapter_dma_buf_alignment;
    else
        actual_properties.alignment = requested_properties.alignment;

    if( !dma_resource_alloc(actual_properties,&addr_pair,&dma_handle) )
    {
        // set the output parameters
        handle_p->p = (void*)dma_handle;
        buffer_p->p = addr_pair.address_p;

        LOG_INFO("dma_buf_alloc: allocated handle=%p, host addr=%p, "
                 "alignment requested/actual %d/%d, "
                 "bank requested %d, cached requested %d\n",
                 handle_p->p,
                 buffer_p->p,
                 requested_properties.alignment,
                 actual_properties.alignment,
                 requested_properties.bank,
                 requested_properties.f_cached);

        return DMABUF_STATUS_OK;
    }
    else
    {
        return DMABUF_ERROR_OUT_OF_MEMORY;
    }
}

/*----------------------------------------------------------------------------
 * dma_buf_destroy
 * Only invalidates the handle and corresponding record. No free of memories
 */
void
dma_buf_destroy(
        dma_buf_handle_t handle)
{
    dma_resource_handle_t dma_handle =
            adapter_dma_buf_handle2_dma_resource_handle(handle);

    LOG_INFO("dma_buf_destroy: handle to destroy=%p\n",handle.p);

    dma_resource_destroy_record(dma_handle);
}

/*----------------------------------------------------------------------------
 * dma_buf_register
 */
dma_buf_status_t
dma_buf_register(
        const dma_buf_properties_t requested_properties,
        void * buffer_p,
        void * alternative_p,
        const char allocator_ref,
        dma_buf_handle_t * const handle_p)
{
    dma_resource_handle_t dma_handle;
    char actual_allocator;
    dma_resource_addr_pair_t addr_pair;
    dma_resource_properties_t actual_properties;

    zeroinit(addr_pair);
    zeroinit(actual_properties);

    if (handle_p == NULL ||
        buffer_p == NULL)
    {
        return DMABUF_ERROR_BAD_ARGUMENT;
    }

    // initialize the output parameter
    handle_p->p = NULL;

    actual_properties.size       = requested_properties.size;
    actual_properties.bank       = requested_properties.bank;
    actual_properties.f_cached    = requested_properties.f_cached;

    if (adapter_dma_buf_alignment != ADAPTER_DMABUF_ALIGNMENT_INVALID &&
        requested_properties.alignment < adapter_dma_buf_alignment)
        actual_properties.alignment = adapter_dma_buf_alignment;
    else
        actual_properties.alignment = requested_properties.alignment;

    actual_allocator = allocator_ref;

    if( allocator_ref == 'k'  || allocator_ref == 'N' || allocator_ref == 'R'
        || allocator_ref == 'C')
    {
        // 'N' is used to register buffers that do not need to be DMA-safe.
        // 'R' is used to register (subranges of) buffers that are already
        //     allocated with dma_resource_alloc()/dma_buf_alloc().
        // 'k' is supported for Linux kmalloc() allocator only,
        //     e.g. allocator_ref = 'k' for streaming DMA mappings
        // 'C' is supported for coherent buffers.
        addr_pair.domain     = DMARES_DOMAIN_HOST;
        addr_pair.address_p  = buffer_p;
    }
    else if( allocator_ref == 0 )
    {
        // Linux kmalloc() allocator is used,
        // e.g. allocator_ref = 'k' for streaming DMA mappings
        actual_allocator = 'k';
        addr_pair.domain     = DMARES_DOMAIN_HOST;
        addr_pair.address_p  = buffer_p;
    }
    else
    {
        return DMABUF_ERROR_BAD_ARGUMENT;
    }

    if( dma_resource_check_and_register(actual_properties,addr_pair,
            actual_allocator,&dma_handle) == 0 )
    {
        if( actual_allocator == 'C' )
        {
            // Add bus address for the resource for allocator_ref = 'C'
            addr_pair.domain     = DMARES_DOMAIN_BUS;
            addr_pair.address_p  = alternative_p;

            dma_resource_add_pair(dma_handle,addr_pair);
        }

        // set the output parameters
        handle_p->p = (void*)dma_handle;

        LOG_INFO("dma_buf_register: registered handle=%p, host addr=%p, "
                 "allocator=%d, alignment requested/actual %d/%d, "
                 "bank requested %d, cached requested %d\n",
                 handle_p->p,
                 buffer_p,
                 allocator_ref,
                 requested_properties.alignment,
                 actual_properties.alignment,
                 requested_properties.bank,
                 requested_properties.f_cached);

        return DMABUF_STATUS_OK;
    }
    else
    {
        return DMABUF_ERROR_OUT_OF_MEMORY;
    }
}


/*----------------------------------------------------------------------------
 * dma_buf_release
 */
dma_buf_status_t
dma_buf_release(
        dma_buf_handle_t handle)
{
    dma_resource_handle_t dma_handle =
            adapter_dma_buf_handle2_dma_resource_handle(handle);

    LOG_INFO("dma_buf_release: handle to release=%p\n",handle.p);

    if( dma_resource_release(dma_handle) == 0 )
    {
        return DMABUF_STATUS_OK;
    }
    else
    {
        return DMABUF_ERROR_INVALID_HANDLE;
    }
}


/* end of file adapter_dmabuf.c */
