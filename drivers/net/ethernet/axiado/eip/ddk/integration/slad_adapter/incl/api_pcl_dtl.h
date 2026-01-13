/* api_pcl_dtl.h
 *
 * Packet Classification API extensions for the Direct Transform Lookup (DTL).
 *
 * The PCL DTL API functions may be used only for those Transform
 * records which can be looked up directly by the Classification Engine
 * in the Flow Hash Table (without using a Flow Record).
 *
 * A Transform Record for direct lookup must be allocated using
 * the DMABuf API dma_buf_alloc() function. In order to determine the required
 * size of of the DMA-buffer the SA Builder API function sa_builder_get_sizes()
 * can be used. The allocated DMA-safe buffer subsequently must be filled in
 * with the transform data, for example by using the SA Builder API, namely
 * the sa_builder_build_sa() function. Finally the allocated and filled in
 * Transform Record must be added to the Flow Hash Table by means of
 * the pcl_dtl_transform_add() function.
 *
 * Several different hash values can reference the same transform record.
 * When the pcl_dtl_transform_add() function adds a transform record to
 * the Flow Hash Table it also adds a hash value and returns its hash handle.
 * In order to add a new hash value for the same transform record
 * the pcl_dtl_transform_add() function can be called again with another hash
 * ID value in the input transform parameters. If successful then this function
 * stores the hash handle in its output parameter for the hash ID added to
 * the Flow Hash Table for the requested transform record. In order to remove
 * the hash value from the the Flow Hash Table the pcl_dtl_transform_remove()
 * function must be called using the hash handle obtained in the previous call
 * to the pcl_dtl_transform_add() function. If the hash handle is set to NULL
 * then the pcl_dtl_transform_remove() function will removed all the hash values
 * associated with the requested transform record from the Flow Hash Table.
 *
 * When a Transform Record for direct lookup must be deleted then, first,
 * the pcl_dtl_transform_remove() function must be used. Second, the PEC API
 * pec_packet_put() function must be called for the Transform Record entry
 * invalidation in the Record Cache. Finally, in order to release the dynamic
 * resource storage the DMABuf API dma_buf_release() function must be used.
 *
 */

/*****************************************************************************
* Copyright (c) 2012-2020 by Rambus, Inc. and/or its subsidiaries.
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


#ifndef API_PCL_DTL_H_
#define API_PCL_DTL_H_


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Driver Framework Basic Definitions API
#include "basic_defs.h"

// Dependency on DMA Buffer Allocation API (api_dmabuf.h)
#include "api_dmabuf.h"         // dma_buf_handle_t

// Main PCL API
#include "api_pcl.h"            // pcl_status_t


/*----------------------------------------------------------------------------
 * Definitions and macros
 */

/*----------------------------------------------------------------------------
 * pcl_dtl_transform_params_t
 *
 * This data structure contains extended transform parameters for the transform
 * record that must be used for direct lookup.
 */
typedef struct
{
    // The 128-bit transform ID computed by the hash function
    // from the selector parameters. This uniquely identifies the hash entry.
    // One or multiple hash ID's (entries) can co-exist for the same transform.
    // This can be calculated by the pcl_flow_hash() function.
    u32 hash_id[4];

    // Reference to a generic transform parameters, set to NULL if not used
    pcl_transform_params_t * params_p;

} pcl_dtl_transform_params_t;


/*----------------------------------------------------------------------------
 * pcl_dtl_hash_handle_t
 *
 * This handle is a reference to a hash ID in the Flow Hash Table.
 *
 * The handle is set to NULL when pcl_dtl_hash_handle_t handle.p is equal
 * to NULL (or use HashHandle = pcl_dtl_null_handle).
 *
 */
typedef struct
{
    void * p;
} pcl_dtl_hash_handle_t;


/*----------------------------------------------------------------------------
 * pcl_dtl_null_handle
 *
 * This handle can be assigned to a variable of type pcl_dtl_hash_handle_t.
 *
 */
extern const pcl_dtl_hash_handle_t pcl_dtl_null_handle;


/*-----------------------------------------------------------------------------
 * pcl_dtl_init
 *
 * Initialize the PCL DTL functionality.
 *
 * API use order:
 *     This function must be called before any other PCL DTL API functions
 *     are called.
 *
 * interface_id (input)
 *     identifier of the Classification Engine interface
 *
 * Return (see pcl_status_t in api_pcl.h):
 *     PCL_STATUS_OK
 *     PCL_STATUS_BUSY
 *     PCL_INVALID_PARAMETER
 *     PCL_ERROR
 */
pcl_status_t
pcl_dtl_init(
        const unsigned int interface_id);


/*-----------------------------------------------------------------------------
 * pcl_dtl_uninit
 *
 * Uninitialize the PCL DTL functionality.
 *
 * API use order:
 *     After this function is called none of the PCL DTL API functions
 *     can be called called except the pcl_dtl_init() function.
 *
 * interface_id (input)
 *     identifier of the Classification Engine interface
 *
 * Return (see pcl_status_t in api_pcl.h):
 *     PCL_STATUS_OK
 *     PCL_STATUS_BUSY
 *     PCL_INVALID_PARAMETER
 *     PCL_ERROR
 */
pcl_status_t
pcl_dtl_uninit(
        const unsigned int interface_id);


/*-----------------------------------------------------------------------------
 * pcl_dtl_transform_add
 *
 * Create a hash value (ID) for the provided Transform Record from
 * the provided transform parameters and add it to the Flow Hash Table.
 * One transform record may have multiple hash ID's associated with it. This
 * function can be called several times to add different hash ID's for
 * the same Transform Record in the Flow Hash Table.
 *
 * Note:  The same hash ID cannot be used for different Transform records.
 *
 * Note:  This function will return PCL_INVALID_PARAMETER for the transform
 *        parameters that were already successfully used to add a hash value
 *        to the Flow Hash Table for this record in an previous call
 *        to pcl_dtl_transform_add().
 *
 * interface_id (input)
 *     identifier of the Classification Engine interface
 *
 * flow_hash_table_id (input)
 *     Flow Hash Table identifier where the hash ID should be added
 *
 * transform_params (input)
 *     Contents of the hash ID that is to be added.
 *
 * xform_dma_handle (input)
 *     handle representing the Transform Record as returned by dma_buf_alloc().
 *     If this function returns with PCL_STATUS_OK, the new hash value
 *     for this Transform Record will be in use.
 *
 * hash_handle_p (output)
 *     Pointer to the memory location when the hash handle will be returned.
 *
 * Return (see pcl_status_t in api_pcl.h):
 *     PCL_STATUS_OK
 *     PCL_STATUS_BUSY
 *     PCL_INVALID_PARAMETER
 *     PCL_ERROR
 *     PCL_OUT_OF_MEMORY_ERROR
 */
pcl_status_t
pcl_dtl_transform_add(
        const unsigned int interface_id,
        const unsigned int flow_hash_table_id,
        const pcl_dtl_transform_params_t * const transform_params,
        const dma_buf_handle_t xform_dma_handle,
        pcl_dtl_hash_handle_t * const hash_handle_p);


/*-----------------------------------------------------------------------------
 * pcl_dtl_transform_remove
 *
 * Remove all the hash ID's associated with the Transform Record,
 * the dynamic storage resource represented by xform_dma_handle remains allocated
 * and it can be reused by another call to pcl_dtl_transform_add().
 * The application is responsible for releasing Transform Record buffer
 * via the dma_buf_release() function.
 *
 * The xform_dma_handle must represent a Transform Record that is currently
 * in the Flow Hash Table (as added by the pcl_dtl_transform_add() function).
 *
 * Note:  This function will return PCL_INVALID_PARAMETER for the transform
 *        record that has no more hash ID's in the Flow Hash Table associated
 *        with it.
 *
 * interface_id (input)
 *     identifier of the Classification Engine interface
 *
 * flow_hash_table_id (input)
 *     Flow Hash Table identifier where the Transform Record should be removed
 *
 * xform_dma_handle (input)
 *     handle of the Transform Record to be removed
 *
 * Return (see pcl_status_t in api_pcl.h):
 *     PCL_STATUS_OK
 *     PCL_STATUS_BUSY
 *     PCL_INVALID_PARAMETER
 *     PCL_ERROR
 */
pcl_status_t
pcl_dtl_transform_remove(
        const unsigned int interface_id,
        const unsigned int flow_hash_table_id,
        const dma_buf_handle_t xform_dma_handle);


/*-----------------------------------------------------------------------------
 * pcl_dtl_hash_remove
 *
 * Remove the hash ID referenced by the HashHandle from the Flow Hash Table.
 *
 * The HashHandle must reference the hash ID that is currently
 * in the Flow Hash Table (as returned by the pcl_dtl_transform_add()
 * function) for the specified Transform Record handle.
 *
 * interface_id (input)
 *     identifier of the Classification Engine interface
 *
 * flow_hash_table_id (input)
 *     Flow Hash Table identifier where the Transform Record should be removed
 *
 * HashHandle (input/output)
 *     handle for the hash ID to be removed
 *
 * Return (see pcl_status_t in api_pcl.h):
 *     PCL_STATUS_OK
 *     PCL_STATUS_BUSY
 *     PCL_INVALID_PARAMETER
 *     PCL_ERROR
 */
pcl_status_t
pcl_dtl_hash_remove(
        const unsigned int interface_id,
        const unsigned int flow_hash_table_id,
        pcl_dtl_hash_handle_t * const hash_handle_p);


#endif /* API_PCL_DTL_H_ */


/* end of file api_pcl_dtl.h */
