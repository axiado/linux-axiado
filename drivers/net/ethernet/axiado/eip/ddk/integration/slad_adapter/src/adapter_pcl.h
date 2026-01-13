/* adapter_pcl.h
 *
 * Adapter PCL internal interface
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

#ifndef INCLUDE_GUARD_ADAPTER_PCL_INTERNAL_H
#define INCLUDE_GUARD_ADAPTER_PCL_INTERNAL_H

/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

#include "basic_defs.h"         // bool

// EIP-207 Driver Library
#include "eip207_flow_generic.h"

// list API
#include "list.h"               // List_*

// DMABuf API
#include "api_dmabuf.h"         // DMABuf_*

// Adapter Locking internal API
#include "adapter_lock.h"       // Adapter_Lock_*


/*----------------------------------------------------------------------------
 * Definitions and macros
 */

typedef struct
{
    volatile bool pcl_is_initialized;
    eip207_flow_io_area_t * eip207_io_area_p;

    void * eip207_descriptor_area_p;
    dma_resource_handle_t eip207_hashtable_dma_handle;

    unsigned int free_list_id;
    list_element_t * element_pool_p;
    unsigned char * rec_dscr_pool_p;

    // Pointer to pool list
    void * list_p;

    // Pointer to elements for pool of lists
    list_element_t * list_element_pool_p;

    // Pointer to a pool of lists
    unsigned char * list_pool_p;

    // device-specific lock and critical section
    adapter_lock_t adapter_pcl_dev_lock;
    adapter_lock_cs_t adapter_pcl_dev_cs;

} adapter_pcl_device_instance_data_t;


/*-----------------------------------------------------------------------------
 * adapter_pcl_device_get
 *
 * Obtain PCL device pointer for the provided interface id
 */
adapter_pcl_device_instance_data_t *
adapter_pcl_device_get(
        const unsigned int interface_id);


/*-----------------------------------------------------------------------------
  adapter_pcl_dma_buf_to_tr_dscr

  Convert DMABuf handle to Transform record descriptor

  transform_handle (input)
    DMABuf handle representing the transform.

  tr_dscr_p (output)
    Pointer to TR descriptor.

  rec_pp (output)
    Pointer to memory location where pointer to DMAesource record descriptor
    will be stored.

  Return:
      PCL_STATUS_OK if succeeded.
      PCL_ERROR
*/
pcl_status_t
adapter_pcl_dma_buf_to_tr_dscr(
        const dma_buf_handle_t transform_handle,
        eip207_flow_tr_dscr_t * const tr_dscr_p,
        dma_resource_record_t ** rec_pp);


/*-----------------------------------------------------------------------------
 * adapter_pcl_list_id_get
 *
 * Obtain list pointer for the provided interface id
 *
 * Return:
 *     PCL_STATUS_OK if succeeded.
 *     PCL_ERROR
 */
pcl_status_t
adapter_pcl_list_id_get(
        const unsigned int interface_id,
        unsigned int * const list_id_p);


#endif /* Include Guard */


/* end of file adapter_pcl.h */
