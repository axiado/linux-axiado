/* api_dmabuf.h
 *
 * Management of buffers that can be shared between the host and hardware
 * devices that utilize Direct Memory Access (DMA).
 *
 * Issues to take into account for these buffers:
 * - Start address alignment
 * - Cache line sharing for buffer head and tail
 * - Cache coherence
 * - address translation to device memory view
 */

/*****************************************************************************
* Copyright (c) 2007-2020 by Rambus, Inc. and/or its subsidiaries.
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

#ifndef INCLUDE_GUARD_API_DMABUF_H
#define INCLUDE_GUARD_API_DMABUF_H

#include "basic_defs.h"


/*----------------------------------------------------------------------------
 * dma_buf_handle_t
 *
 * This handle is a reference to a DMA buffer. It is returned when a buffer
 * is allocated or registered and it remains valid until the buffer is freed
 * or de-registered.
 *
 * The handle is set to NULL when dma_buf_handle_t handle.p is equal to NULL.
 *
 */
typedef struct
{
    void * p;
} dma_buf_handle_t;


/*----------------------------------------------------------------------------
 * dma_buf_host_address_t
 *
 * Buffer address that can be used by the host to access the buffer. This
 * address has been put in a data structure to make it type-safe.
 */
typedef struct
{
    void * p;
} dma_buf_host_address_t;


/*----------------------------------------------------------------------------
 * dma_buf_status_t
 *
 * Return values for all the API functions.
 */
typedef enum
{
    DMABUF_STATUS_OK,
    DMABUF_ERROR_BAD_ARGUMENT,
    DMABUF_ERROR_INVALID_HANDLE,
    DMABUF_ERROR_OUT_OF_MEMORY
} dma_buf_status_t;


/*----------------------------------------------------------------------------
 * dma_buf_properties_t
 *
 * Buffer properties. When allocating a buffer, these are the requested
 * properties for the buffer. When registering an externally allocated buffer,
 * these properties describe the buffer.
 *
 * For both uses, the data structure should be initialized to all zeros
 * before filling in some or all of the fields. This ensures forward
 * compatibility in case this structure is extended with more fields.
 *
 * Example usage:
 *     dma_buf_properties_t props = {0};
 *     props.fIsCached = true;
 */
typedef struct
{
    u32 size;       // size of the buffer
    u8 alignment;   // buffer start address alignment, for example
                         // 4 for 32bit
    u8 bank;        // can be used to indicate on-chip memory
    bool f_cached;        // true = SW needs to do coherency management
} dma_buf_properties_t;


/*----------------------------------------------------------------------------
 * dma_buf_null_handle
 *
 * This handle can be assigned to a variable of type dma_buf_handle_t.
 *
 */
extern const dma_buf_handle_t dma_buf_null_handle;


/*----------------------------------------------------------------------------
 * dma_buf_handle_is_same
 *
 * Check whether provided handle1 is equal to provided handle2.
 *
 * handle1_p
 *      First handle
 *
 * handle2_p
 *      Second handle
 *
 * Return values
 *      true:  provided handles are equal
 *      false: provided handles are not equal
 *
 */
bool
dma_buf_handle_is_same(
        const dma_buf_handle_t * const handle1_p,
        const dma_buf_handle_t * const handle2_p);


/*----------------------------------------------------------------------------
 * dma_buf_alloc
 *
 * Allocate a buffer of requested size that can be used for device DMA.
 *
 * requested_properties
 *     Requested properties of the buffer that will be allocated, including
 *     the size, start address alignment, etc. See above.
 *
 * buffer_p (output)
 *     Pointer to the memory location where the address of the buffer will be
 *     written by this function when allocation is successful. This address
 *     can then be used to access the driver on the host in the domain of the
 *     driver.
 *
 * handle_p (output)
 *     Pointer to the memory location when the handle will be returned.
 *
 * Return Values
 *     DMABUF_STATUS_OK: Success, handle_p was written.
 *     DMABUF_ERROR_BAD_ARGUMENT
 *     DMABUF_ERROR_OUT_OF_MEMORY: Failed to allocate a buffer or handle.
 */
dma_buf_status_t
dma_buf_alloc(
        const dma_buf_properties_t requested_properties,
        dma_buf_host_address_t * const buffer_p,
        dma_buf_handle_t * const handle_p);


/*----------------------------------------------------------------------------
 * dma_buf_register
 *
 * This function must be used to register an "alien" buffer that was allocated
 * somewhere else.
 *
 * actual_properties (input)
 *     properties that describe the buffer that is being registered.
 *
 * buffer_p (input)
 *     Pointer to the buffer. This pointer must be valid to use on the host
 *     in the domain of the driver.
 *
 * alternative_p (input)
 *     Some allocators return two addresses. This parameter can be used to
 *     pass this second address to the driver. The type is pointer to ensure
 *     it is always large enough to hold a system address, also in LP64
 *     architecture. Set to NULL if not used.
 *
 * allocator_ref (input)
 *     number to describe the source of this buffer. The exact numbers
 *     supported is implementation specific. This provides some flexibility
 *     for a specific implementation to support a number of "alien" buffers
 *     from different allocator and properly interpret and use the
 *     alternative_p parameter when translating the address to the device
 *     memory map. Set to zero when a default allocator is used. The type
 *     of the default allocator is implementation specific,
 *     refer to the DMABuf API Implementation Notes for details.
 *
 * handle_p (output)
 *     Pointer to the memory location when the handle will be returned.
 *
 * Return Values
 *     DMABUF_STATUS_OK: Success, handle_p was written.
 *     DMABUF_ERROR_BAD_ARGUMENT
 *     DMABUF_ERROR_OUT_OF_MEMORY: Failed to allocate a handle.
 */
dma_buf_status_t
dma_buf_register(
        const dma_buf_properties_t actual_properties,
        void * buffer_p,
        void * alternative_p,
        const char allocator_ref,
        dma_buf_handle_t * const handle_p);


/*----------------------------------------------------------------------------
 * dma_buf_release
 *
 * This function will close the handle that was returned by dma_buf_alloc or
 * dma_buf_register, meaning it must not be used anymore.
 * If the buffer was allocated through dma_buf_alloc, this function will also
 * free the buffer, meaning it must not be accessed anymore.
 *
 * handle (input)
 *     The handle that may be released.
 *
 * Return Values
 *     DMABUF_STATUS_OK
 *     DMABUF_ERROR_INVALID_HANDLE
 */
dma_buf_status_t
dma_buf_release(
        dma_buf_handle_t handle);

/*----------------------------------------------------------------------------
 * dma_buf_destroy
 *
 * Delete the handle from the allocated list. This doesn't free the memory
 *
 * handle (input)
 *     The handle that may be released.
 * Usage Note: This is reqd when build-skb is used, whether dma-handles alone needs recycle
 *
 */
void
dma_buf_destroy(
        dma_buf_handle_t handle);

#endif /* Include Guard */

/* end of file api_dmabuf.h */
