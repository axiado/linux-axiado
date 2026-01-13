/* dmares_rw.h
 *
 * Driver Framework, DMAResource API, Read/Write and Pre/Post-DMA functions
 *
 * The document "Driver Framework Porting Guide" contains the detailed
 * specification of this API. The information contained in this header file
 * is for reference only.
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

#ifndef INCLUDE_GUARD_DMARES_RW_H
#define INCLUDE_GUARD_DMARES_RW_H

#include "basic_defs.h"         // bool, u32, inline
#include "dmares_types.h"       // dma_resource_handle_t


/*****************************************************************************
 * DMA Resource API
 *
 * This API is related to management of memory buffers that can be accessed by
 * the host as well as a device, using Direct Memory Access (DMA). A driver
 * must typically support many of these shared resources. This API helps with
 * the administration of these resources.
 *
 * This API maintains administration records that the caller can read and
 * write directly. A handle is also provided, to abstract the record.
 * The handle cannot be used to directly access the resource and is therefore
 * considered safe to pass around in the system, even to applications.
 *
 * Another important aspect of this API is to provide a point where resources
 * can be handed over between the host and the device. The driver library or
 * adapter must call the PreDMA and PostDMA functions to indicate the hand over
 * of access right between the host and the device for an entire resource or
 * part thereof. The implementation can use these calls to manage the
 * data coherence for the resource, for example in a cached system.
 *
 * Dynamic DMA resources such as DMA-safe buffers covered by this API
 * are different from static device resources (see the device API)
 * which represent device-internal resources with possible read and write
 * side-effects.
 *
 * Memory buffers are different from HWPAL_Device resources, which represent
 * static device-internal resources with possible read and write side-effects.
 *
 * On the fly endianness swapping for words is supported for DMA Resources by
 * means of the Read32 and Write32 functions.
 *
 * Note: If multiple devices with a different memory view need to use the same
 * DMA resource then the caller should consider separate records for each
 * device (for the same buffer).
 */

/*----------------------------------------------------------------------------
 * dma_resource_read32
 *
 * This function can be used to read one 32bit word from the DMA Resource
 * buffer.
 * If required (decided by dma_resource_record_t.device.f_swap_endianness),
 * on the fly endianness conversion of the value read will be performed before
 * it is returned to the caller.
 *
 * handle (input)
 *     handle for the DMA Resource to access.
 *
 * word_offset (input)
 *     offset in 32bit words, from the start of the DMA Resource to read from.
 *
 * Return value
 *     The value read.
 *
 * When the handle and word_offset parameters are not valid, the implementation
 * will return an unspecified value.
 */
u32
dma_resource_read32(
        const dma_resource_handle_t handle,
        const unsigned int word_offset);


/*----------------------------------------------------------------------------
 * dma_resource_write32
 *
 * This function can be used to write one 32bit word to the DMA Resource.
 * If required (decided by dma_resource_record_t.device.f_swap_endianness),
 * on the fly endianness conversion of the value to be written will be
 * performed.
 *
 * handle (input)
 *     handle for the DMA Resource to access.
 *
 * word_offset (input)
 *     offset in 32bit words, from the start of the DMA Resource to write to.
 *
 * value (input)
 *     The 32bit value to write.
 *
 * Return value
 *     None
 *
 * The write can only be successful when the handle and word_offset
 * parameters are valid.
 */
void
dma_resource_write32(
        const dma_resource_handle_t handle,
        const unsigned int word_offset,
        const u32 value);


/*----------------------------------------------------------------------------
 * dma_resource_read32_array
 *
 * This function perform the same task as dma_resource_read32 for an array of
 * consecutive 32bit words.
 *
 * See dma_resource_read32 for a more detailed description.
 *
 * handle (input)
 *     handle for the DMA Resource to access.
 *
 * start_word_offset (input)
 *     offset in 32bit words, from the start of the DMA Resource to start
 *     reading from. The word offset in incremented for every word.
 *
 * word_count (input)
 *     The number of 32bit words to transfer.
 *
 * values_p (input)
 *     Memory location to write the retrieved values to.
 *     Note the ability to let values_p point inside the DMAResource that is
 *     being read from, allowing for in-place endianness conversion.
 *
 * Return value
 *     None.
 *
 * The read can only be successful when the handle and start_word_offset
 * parameters are valid.
 */
void
dma_resource_read32_array(
        const dma_resource_handle_t handle,
        const unsigned int start_word_offset,
        const unsigned int word_count,
        u32 * values_p);


/*----------------------------------------------------------------------------
 * dma_resource_write32_array
 *
 * This function perform the same task as dma_resource_write32 for an array of
 * consecutive 32bit words.
 *
 * See dma_resource_write32 for a more detailed description.
 *
 * handle (input)
 *     handle for the DMA Resource to access.
 *
 * start_word_offset (input)
 *     offset in 32bit words, from the start of the DMA Resource to start
 *     writing from. The word offset in incremented for every word.
 *
 * word_count (input)
 *     The number of 32bit words to transfer.
 *
 * values_p (input)
 *     Pointer to the memory where the values to be written are located.
 *     Note the ability to let values_p point inside the DMAResource that is
 *     being written to, allowing for in-place endianness conversion.
 *
 * Return value
 *     None.
 *
 * The write can only be successful when the handle and start_word_offset
 * parameters are valid.
 */
void
dma_resource_write32_array(
        const dma_resource_handle_t handle,
        const unsigned int start_word_offset,
        const unsigned int word_count,
        const u32 * values_p);


/*----------------------------------------------------------------------------
 * dma_resource_pre_dma
 *
 * This function must be called when the host has finished accessing the
 * DMA resource and the device (using its DMA) is the next to access it.
 * It is possible to hand off the entire DMA resource, or only a selected part
 * of it by describing the part with a start offset and count.
 *
 * handle (input)
 *     handle for the DMA Resource to (partially) hand off.
 *
 * byte_offset (input)
 *     Start offset within the DMA resource for the selected part to hand off
 *     to the device.
 *
 * byte_count (input)
 *     number of bytes from byte_offset for the part of the DMA Resource to
 *     hand off to the device.
 *     Set to zero to hand off the entire DMA Resource.
 *
 * The driver framework implementation must use this call to ensure the data
 * coherence of the DMA Resource.
 */
void
dma_resource_pre_dma(
        const dma_resource_handle_t handle,
        const unsigned int byte_offset,
        const unsigned int byte_count);


/*----------------------------------------------------------------------------
 * dma_resource_post_dma
 *
 * This function must be called when the device has finished accessing the
 * DMA resource and the host can reclaim ownership and access it.
 * It is possible to reclaim ownership for the entire DMA resource, or only a
 * selected part of it by describing the part with a start offset and count.
 *
 * handle (input)
 *     handle for the DMA resource to (partially) hand off.
 *
 * byte_offset (input)
 *     Start offset within the DMA resource for the selected part to reclaim.
 *
 * byte_count (input)
 *     number of bytes from byte_offset for the part of the DMA Resource to
 *     reclaim.
 *     Set to zero to reclaim the entire DMA resource.
 *
 * The driver framework implementation must use this call to ensure the data
 * coherence of the DMA resource.
 */
void
dma_resource_post_dma(
        const dma_resource_handle_t handle,
        const unsigned int byte_offset,
        const unsigned int byte_count);


#endif /* Include Guard */

/* end of file dmares_rw.h */
