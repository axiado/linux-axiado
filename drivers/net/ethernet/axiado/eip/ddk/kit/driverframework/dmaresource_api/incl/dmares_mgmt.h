/* dmares_mgmt.h
 *
 * Driver Framework, DMAResource API, Management functions
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

#ifndef INCLUDE_GUARD_DMARES_MGMT_H
#define INCLUDE_GUARD_DMARES_MGMT_H

#include "basic_defs.h"         // bool, u32, inline
#include "dmares_types.h"       // DMAResource_Handle/Record/AddrPair_t


/*****************************************************************************
 * DMA Resource API
 *
 * This API is related to management of memory buffers that can be accessed by
 * the host as well as a device, using Direct Memory Access (DMA). A driver
 * must typically support many of these shared resources. This API helps with
 * administration of these resources.
 *
 * This API maintains administration records that the caller can read and
 * write directly. A handle is also provided to abstract the record.
 * The handle cannot be used to directly access the resource and is therefore
 * considered safe to pass around in the system, even to applications.
 *
 * Another important aspect of this API is to provide a point where resources
 * can be handed over between the host and the device. The driver library or
 * adapter must call the PreDMA and PostDMA functions to indicate the hand over
 * of access right between the host and the device for an entire resource, or
 * part thereof. The implementation can use these calls to manage the
 * data coherence for the resource, for example in a cached system.
 *
 * Dynamic DMA resources such as DMA-safe buffers covered by this API
 * are different from static device resources (see the device API)
 * which represent device-internal resources with possible read and write
 * side-effects.
 *
 * On the fly endianness conversion (byte swap) for integers is supported for
 * DMA resources by means of the Read32 and Write32 functions.
 *
 * Note: If multiple devices with a different memory view need to use the same
 * DMA resource, then the caller should consider separate records for each
 * device (for the same buffer).
 */


/*----------------------------------------------------------------------------
 * dma_resource_init
 *
 * This function must be used to initialize the DMAResource administration.
 * It must be called before any of the other DMAResource_* functions may be
 * called. It may be called anew only after dma_resource_uninit has been called.
 *
 * Return value
 *     true   Initialization successfully, rest of the API may now be used.
 *     false  Initialization failed.
 */
bool
dma_resource_init(void);


/*----------------------------------------------------------------------------
 * dma_resource_uninit
 *
 * This function can be used to uninitialize the DMAResource administration.
 * The caller must make sure that handles will not be used after this function
 * returns.
 * If memory was allocated by dma_resource_init, this function will free it.
 */
void
dma_resource_uninit(void);


/*----------------------------------------------------------------------------
 * dma_resource_create_record
 *
 * This function can be used to create a record. The function returns a handle
 * for the record. Use dma_resource_handle2_record_ptr to access the record.
 * Destroy the record when no longer required, see DMAResource_Destroy.
 * This function initializes the record to all zeros.
 *
 * Return Values
 *     handle for the DMA Resource.
 *     NULL is returned when the creation failed.
 */
dma_resource_handle_t
dma_resource_create_record(void);


/*----------------------------------------------------------------------------
 * dma_resource_destroy_record
 *
 * This function invalidates the handle and the record instance.
 *
 * handle
 *     A valid handle that was once returned by dma_resource_create_record or
 *     one of the DMA Buffer Management functions (dma_resource_alloc /
 *     dma_resource_check_and_register / dma_resource_attach).
 *
 * Return Values
 *     None
 */
void
dma_resource_destroy_record(
        const dma_resource_handle_t handle);


/*----------------------------------------------------------------------------
 * dma_resource_is_valid_handle
 *
 * This function tells whether a handle is valid.
 *
 * handle
 *     A valid handle that was once returned by dma_resource_create_record or
 *     one of the DMA Buffer Management functions (dma_resource_alloc /
 *     dma_resource_check_and_register / dma_resource_attach).
 *
 * Return value
 *     true   The handle is valid
 *     false  The handle is NOT valid
 */
bool
dma_resource_is_valid_handle(
        const dma_resource_handle_t handle);


/*----------------------------------------------------------------------------
 * dma_resource_handle2_record_ptr
 *
 * This function can be used to get a pointer to the DMA resource record
 * (dma_resource_record_t) for the provided handle. The pointer is valid until
 * the record and handle are destroyed.
 *
 * handle
 *     A valid handle that was once returned by dma_resource_create_record or
 *     one of the DMA Buffer Management functions (dma_resource_alloc /
 *     dma_resource_check_and_register / dma_resource_attach).
 *
 * Return value
 *     Pointer to the dma_resource_record_t memory for this handle.
 *     NULL is returned if the handle is invalid.
 */
dma_resource_record_t *
dma_resource_handle2_record_ptr(
        const dma_resource_handle_t handle);


#endif /* Include Guard */

/* end of file dmares_mgmt.h */
