/* api_pcl.h
 *
 * Packet Classification API.
 *
 */

/*****************************************************************************
* Copyright (c) 2011-2025 by Rambus, Inc. and/or its subsidiaries.
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


#ifndef API_PCL_H_
#define API_PCL_H_


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Driver Framework Basic Definitions API
#include "basic_defs.h"

// Dependency on DMA Buffer Allocation API (api_dmabuf.h)
#include "api_dmabuf.h"         // dma_buf_handle_t


/*----------------------------------------------------------------------------
 * Definitions and macros
 */
typedef enum
{
    PCL_STATUS_OK,              // Operation is successful
    PCL_STATUS_BUSY,            // device busy, try again later
    PCL_INVALID_PARAMETER,      // Invalid input parameter
    PCL_UNSUPPORTED_FEATURE,    // Feature is not implemented
    PCL_OUT_OF_MEMORY_ERROR,    // Out of memory
    PCL_ERROR                   // Operation has failed
} pcl_status_t;

/* flags for selector parameters */
#define PCL_SELECT_IPV4     BIT_0   // Packet is IPv4.
#define PCL_SELECT_IPV6     BIT_1   // Packet is IPv6.
#define PCL_SELECT_MACSEC   BIT_4   // Packet is MACsec

/*----------------------------------------------------------------------------
 * pcl_selector_params_t
 *
 * This data structure represents the packet parameters (such as IP addresses
 * and ports) that select a particular flow.
 */
typedef struct
{
    u32 flags;    // Bitwise or of zero or more PCL_SELECT_* flags.
    u8 *src_ip;    // IP (4 or 16 bytes) source address.
                       // (8-byte SCI for MACsec)
    u8 *dst_ip;    // IP (4 or 16 bytes) destination address.
                       // (6-byte MAC dest address for MACsec)
    u8 ip_proto;   // IP protocol (UDP, ESP).
    u16 src_port;  // source port for UDP.
    u16 dst_port;  // Destination port for UDP.
    u32 spi;      // spi in IPsec
    u16 epoch;    // epoch in DTLS.
    u16 ether_type; // Ether type parameter for MACsec.
    u8  an;     // an for MACsec.
} pcl_selector_params_t;

/* flags for flow parameters */
#define PCL_FLOW_INBOUND  BIT_1  // Flow is for inbound IPsec packets.
#define PCL_FLOW_SLOWPATH BIT_2  // Packets should not be processed in hardware

/*----------------------------------------------------------------------------
 * pcl_flow_params_t
 *
 * This data structure represents the flow data structure visible to the
 * application. The actual flow record may contain additional information,
 * such as the physical addresses of the transform record and links for the
 * Flow Hash Table.
 */
typedef struct
{
    u32 flow_id[4];       // The 128-bit flow ID computed by the flow hash
                              // function from the selector parameters.

    u32 flow_index;       // Reference to flow table entry in application.
                              // An Ipsec implementation may maintain a flow
                              // table in software that contains more
                              // information than is accessible to the
                              // Classification Engine. flow_index may be
                              // used by software to refer bo this software
                              // flow record.

    u32 flags;           // Bitwise of zero or more PCL_FLOW_* flags.
    u32 source_interface; // Representation of the network interface

    // Transform that is to be applied.
    dma_buf_handle_t transform;

    // Time last accessed, in engine clock cycles.
    u32 last_time_lo;
    u32 last_time_hi;

    // number of packets processed by this flow.
    u32 packets_counter_lo;
    u32 packets_counter_hi;

    // number of octets processed by this flow.
    u32 octets_counter_lo;
    u32 octets_counter_hi;

} pcl_flow_params_t;

/*----------------------------------------------------------------------------
 * pcl_transform_params_t
 *
 * This data structure represents parameters from the transform record
 * that the application must be able to access after the transform
 * record is created.
 */
typedef struct
{
    // 32-bit sequence number of outbound transform.
    u32 sequence_number;

    // Time last of transform record access, in engine clock cycles.
    u32 last_time_lo;
    u32 last_time_hi;

    // number of packets processed for this transform.
    u32 packets_counter_lo;
    u32 packets_counter_hi;

    // number of octets processed for this transform.
    u32 octets_counter_lo;
    u32 octets_counter_hi;

} pcl_transform_params_t;

// Flow Record handle
typedef void * pcl_flow_handle_t;


/*----------------------------------------------------------------------------
 * pcl_init
 *
 * Initialize the classification functionality. After the initialization no
 * transform records are registered and no flow records exist. This function
 * must be called before any other function of this API is called.
 *
 * interface_id (input)
 *     identifier of the Classification Engine interface
 *
 * nof_flow_hash_tables (input)
 *     number of the Flow Hash Tables to set up
 *
 * Return:
 *     PCL_STATUS_OK
 *     PCL_INVALID_PARAMETER
 *     PCL_UNSUPPORTED_FEATURE
 *     PCL_ERROR
 */
pcl_status_t
pcl_init(
        const unsigned int interface_id,
        const unsigned int nof_flow_hash_tables);


/*----------------------------------------------------------------------------
 * pcl_uninit
 *
 * Uninitialize the classification functionality. Before this
 * function may be called, all the transform records must be unregistered and
 * all the flow records must be destroyed.
 *
 * interface_id (input)
 *     identifier of the Classification Engine interface
 *
 * Return:
 *     PCL_STATUS_OK
 *     PCL_INVALID_PARAMETER
 *     PCL_ERROR
 */
pcl_status_t
pcl_uninit(
        const unsigned int interface_id);


/*----------------------------------------------------------------------------
 * pcl_flow_dma_buf_handle_get
 *
 * Obtain dma_buf_handle_t type handle from the provided pcl_flow_handle_t type
 * handle.
 *
 * flow_handle (input)
 *     handle of the flow record.
 *
 * dma_handle_p (output)
 *     Pointer to the memory location where the handle representing
 *     the allocated flow record as a DMA resource (see DMABuf API)
 *     will be stored.
 *
 * Return:
 *     PCL_STATUS_OK
 *     PCL_INVALID_PARAMETER
 */
pcl_status_t
pcl_flow_dma_buf_handle_get(
        const pcl_flow_handle_t flow_handle,
        dma_buf_handle_t * const dma_handle_p);


/*----------------------------------------------------------------------------
 * pcl_flow_hash
 *
 * Compute a 128-bit hash value from the provided selector parameters.
 * This value can be assumed to be unique for every different
 * combination of system parameters. The hash computation is the exact
 * same computation that the classification hardware would do for a packet with
 * the same parameters. Which fields are hashed, depends on the specified
 * protocol (UDP will hash source and destination ports, ESP will hash spi).
 * This value is subsequently used to identify looked-up records.
 *
 * selector_params (input)
 *     Set of packet parameters to select a particular flow.
 *
 * flow_id_word32_array_of4 (output)
 *     128-bit hash value that will be used to uniquely identify a looked-up
 *     record. This must point to a buffer that can hold 4 32-bit words.
 *
 * Return:
 *     PCL_STATUS_OK
 *     PCL_INVALID_PARAMETER
 *     PCL_UNSUPPORTED_FEATURE
 *     PCL_ERROR
 */
pcl_status_t
pcl_flow_hash(
        const pcl_selector_params_t * const selector_params,
        u32 * flow_id_word32_array_of4);


/*-----------------------------------------------------------------------------
 * pcl_flow_lookup
 *
 * Find a flow in the Flow Hash Table with a matching flow ID.
 * If no such flow is found, return a null handle.
 *
 * interface_id (input)
 *     identifier of the Classification Engine interface
 *
 * flow_hash_table_id (input)
 *     Flow Hash Table identifier that should be used for the flow record
 *     lookup
 *
 * flow_id_word32_array_of4 (input)
 *     128-bit flow ID to find.
 *
 * flow_handle_p (output)
 *     handle to access the flow record found, e.g. via pcl_flow_get_read_only()
 *     (or null handle of no flow found).
 *
 * Return:
 *     PCL_STATUS_OK:           flow record is found
 *     PCL_INVALID_PARAMETER
 *     PCL_ERROR:               no flow record is found
 */
pcl_status_t
pcl_flow_lookup(
        const unsigned int interface_id,
        const unsigned int flow_hash_table_id,
        const u32 * flow_id_word32_array_of4,
        pcl_flow_handle_t * const flow_handle_p);


/*-----------------------------------------------------------------------------
 * pcl_flow_alloc
 *
 * Allocate all dynamic storage resources for a flow record, but do not
 * add a flow record to the table.
 *
 * Note:  This function will allocate a buffer to store the actual flow record
 *        and all administration used by the driver (represented by flow_handle).
 *
 * interface_id (input)
 *     identifier of the Classification Engine interface
 *
 * flow_hash_table_id (input)
 *     Flow Hash Table identifier where the flow record should be allocated
 *
 * flow_handle_p (output)
 *     handle to represent the allocated flow record. This will be used
 *     as an input parameter to other flow related functions in this API.
 *
 * Return:
 *     PCL_STATUS_OK
 *     PCL_INVALID_PARAMETER
 *     PCL_ERROR
 */
pcl_status_t
pcl_flow_alloc(
        const unsigned int interface_id,
        const unsigned int flow_hash_table_id,
        pcl_flow_handle_t * const flow_handle_p);


/*-----------------------------------------------------------------------------
 * pcl_flow_add
 *
 * Create a new flow record from the supplied parameters and add it to the
 * Flow Hash Table. Its dynamic storage resources must already be allocated.
 *
 * Note:  This function will add a flow record to the flow table.
 *        The flow_handle must represent an allocated, but unused
 *        flow record buffer.
 *        No flow record may already exist with the same flow ID.
 *        The application is responsible for this.
 *
 * interface_id (input)
 *     identifier of the Classification Engine interface
 *
 * flow_hash_table_id (input)
 *     Flow Hash Table identifier where the flow record should be added
 *
 * flow_params (input)
 *     Contents of the flow that is to be added.
 *
 * flow_handle (input)
 *     handle representing the storage space for the flow record
 *     (as returned by pcl_flow_alloc). If this function returns with
 *     PCL_STATUS_OK, the flow record will be in use and the handle can be
 *     used with pcl_flow_remove() or pcl_flow_get_read_only().
 *
 * Return:
 *     PCL_STATUS_OK
 *     PCL_INVALID_PARAMETER
 *     PCL_ERROR
 */
pcl_status_t
pcl_flow_add(
        const unsigned int interface_id,
        const unsigned int flow_hash_table_id,
        const pcl_flow_params_t * const flow_params,
        const pcl_flow_handle_t flow_handle);


/*-----------------------------------------------------------------------------
 * pcl_flow_remove
 *
 * Remove the indicated flow record from the Flow Hash Table. If the
 * flow record references a transform record then that transform
 * record must be unregistered after the flow record is removed.
 * The flow handle must represent a flow record that is currently in the
 * Flow Hash Table (by pcl_flow_add).
 *
 * When this function returns, the dynamic storage resources of the flow
 * record are still allocated and they can be reused by another call to
 * pcl_flow_add() or the resources can be released by pcl_flow_release().
 *
 * interface_id (input)
 *     identifier of the Classification Engine interface
 *
 * flow_hash_table_id (input)
 *     Flow Hash Table identifier where the flow record should be removed
 *
 * flow_handle (input)
 *     handle of the flow record to be removed.
 *
 * Return:
 *     PCL_STATUS_OK
 *     PCL_INVALID_PARAMETER
 *     PCL_ERROR
 */
pcl_status_t
pcl_flow_remove(
        const unsigned int interface_id,
        const unsigned int flow_hash_table_id,
        const pcl_flow_handle_t flow_handle);


/*-----------------------------------------------------------------------------
 * pcl_flow_release
 *
 * Release all dynamic storage resources of a flow record. The
 * flow_handle must represent an allocated, but unused flow record
 * buffer.
 *
 * interface_id (input)
 *     identifier of the Classification Engine interface
 *
 * flow_hash_table_id (input)
 *     Flow Hash Table identifier where the flow record was originally
 *     allocated.
 *
 * flow_handle (input)
 *     handle of the flow record to be released. When this function returns,
 *     this handle us no longer a valid input to any of the flow related
 *     API functions.
 *
 * Return:
 *     PCL_STATUS_OK
 *     PCL_INVALID_PARAMETER
 *     PCL_ERROR
 */
pcl_status_t
pcl_flow_release(
        const unsigned int interface_id,
        const unsigned int flow_hash_table_id,
        const pcl_flow_handle_t flow_handle);


/*-----------------------------------------------------------------------------
 * pcl_flow_get_read_only
 *
 * Read the contents of a flow record with no intent to update it.
 *
 * flow_handle (input)
 *     handle of the flow record to be read.
 *
 * flow_params_p (output)
 *     Contents read from the flow record (storage space provided by
 *     application).
 *
 * Return:
 *     PCL_STATUS_OK
 *     PCL_INVALID_PARAMETER
 *     PCL_ERROR
 */
pcl_status_t
pcl_flow_get_read_only(
        const pcl_flow_handle_t flow_handle,
        pcl_flow_params_t * const flow_params_p);


/*-----------------------------------------------------------------------------
 * pcl_transform_register
 *
 * Register the transform record. Flow records may only reference transform
 * records that have been made registered with this function.
 *
 * Before this function is called, the application has already
 * allocated a buffer through the DMABuf API and has already filled it
 * with transform data, for example via using the SA Builder API.
 *
 * TransformDMAHandle (input)
 *     DMABuf handle of the transform to be added.
 *
 * Return:
 *     PCL_STATUS_OK when operation was successful.
 *     PCL_INVALID_PARAMETER
 *     PCL_ERROR
 */
pcl_status_t
pcl_transform_register(
        const dma_buf_handle_t xform_dma_handle);


/*-----------------------------------------------------------------------------
 * pcl_transform_un_register
 *
 * Unregister the transform. The buffer represented
 * by xform_dma_handle remains allocated and the application is responsible
 * for releasing it. The flow record must be removed before its transform
 * record can be unregistered.
 *
 * Note: When this function is called, no flow records may reference
 *       the unregistered transform record.
 *
 * xform_dma_handle (input)
 *     DMABuf handle of the transform to be removed.
 *
 * Return:
 *     PCL_STATUS_OK when operation was successful.
 *     PCL_INVALID_PARAMETER
 *     PCL_ERROR
 */
pcl_status_t
pcl_transform_un_register(
        const dma_buf_handle_t xform_dma_handle);


/*-----------------------------------------------------------------------------
 * pcl_transform_get_read_only
 *
 * Read a subset of the contents of the transform record into a
 * parameters structure.
 *
 * xform_dma_handle (input)
 *     DMABuf handle of the transform to be accessed.
 *
 * transform_params_p (output)
 *     Transform parameters read from the transform record.
 *
 * Return:
 *     PCL_STATUS_OK
 *     PCL_INVALID_PARAMETER
 *     PCL_ERROR
 */
pcl_status_t
pcl_transform_get_read_only(
        const dma_buf_handle_t xform_dma_handle,
        pcl_transform_params_t * const transform_params_p);


#endif /* API_PCL_H_ */


/* end of file api_pcl.h */
