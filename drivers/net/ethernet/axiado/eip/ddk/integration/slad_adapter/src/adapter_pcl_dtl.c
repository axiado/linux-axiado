/* adapter_pcl_dtl.c
 *
 * Packet Classification (PCL) DTL API implementation.
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
******************************************************************************/

/*----------------------------------------------------------------------------
 * This module implements (provides) the following interface(s):
 */

// PCL API
#include "api_pcl.h"                // PCL_*

// PCL DTL API
#include "api_pcl_dtl.h"            // PCL_DTL_*


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Default Adapter configuration
#include "c_adapter_pcl.h"

// DMABuf API
#include "api_dmabuf.h"         // DMABuf_*

// Adapter DMABuf internal API
#include "adapter_dmabuf.h"

// Convert address to pair of 32-bit words.
#include "adapter_addrpair.h"

// Buffer allocation (non-DMA) API
#include "adapter_alloc.h"

// Adapter PCL internal API
#include "adapter_pcl.h"

// Adapter Locking internal API
#include "adapter_lock.h"       // Adapter_Lock_*

// EIP-207 Driver Library Flow Control Generic API
#include "eip207_flow_generic.h"

// EIP-207 Driver Library Flow Control DTL API
#include "eip207_flow_dtl.h"

// Driver Framework device API
#include "device_types.h"       // device_handle_t
#include "device_mgmt.h"        // Device_find

// Driver Framework DMAResource API
#include "dmares_types.h"       // dma_resource_handle_t
#include "dmares_mgmt.h"        // DMAResource management functions
#include "dmares_rw.h"          // DMAResource buffer access.
#include "dmares_addr.h"        // DMAResource addr translation functions.
#include "dmares_buf.h"         // DMAResource buffer allocations

// list API
#include "list.h"               // List_*

// Runtime power Management device Macros API
#include "rpm_device_macros.h"  // RPM_*

// Logging API
#include "log.h"

// Driver Framework C Run-Time Library API
#include "clib.h"               // memcpy, memset

// Driver Framework Basic Definitions API
#include "basic_defs.h"         // bool, u32


/*----------------------------------------------------------------------------
 * Definitions and macros
 */

// this implementation requires DMA resource banks
#ifndef ADAPTER_PCL_DMARESOURCE_BANKS_ENABLE
#error "Adapter DTL: ADAPTER_PCL_DMARESOURCE_BANKS_ENABLE not defined"
#endif

// Bit to indicate whether transformr record is large.
#define ADAPTER_PCL_TR_ISLARGE              BIT_4


/*----------------------------------------------------------------------------
 * Global constants
 */


/*----------------------------------------------------------------------------
 * pcl_dtl_null_handle
 *
 */
const pcl_dtl_hash_handle_t pcl_dtl_null_handle = { NULL };


/*----------------------------------------------------------------------------
 * Local variables
 */


/*----------------------------------------------------------------------------
 * Local prototypes
 */


/*-----------------------------------------------------------------------------
 * PCL API functions implementation
 *
 */

/*-----------------------------------------------------------------------------
 * pcl_flow_remove
 */
pcl_status_t
pcl_flow_remove(
        const unsigned int interface_id,
        const unsigned int flow_hash_table_id,
        const pcl_flow_handle_t flow_handle)
{
    pcl_status_t pcl_rc = PCL_STATUS_OK;
    eip207_flow_error_t eip207_rc;
    eip207_flow_io_area_t * ioarea_p;
    list_element_t * element_p = (list_element_t*)flow_handle;
    eip207_flow_fr_dscr_t * flow_descriptor_p;
    adapter_pcl_device_instance_data_t * dev_p;

    LOG_INFO("\n\t pcl_flow_remove \n");

    // validate input parameters
    if (interface_id >= ADAPTER_PCL_MAX_FLUE_DEVICES ||
        flow_hash_table_id >= ADAPTER_CS_MAX_NOF_FLOW_HASH_TABLES_TO_USE ||
        element_p == NULL)
    {
        return PCL_INVALID_PARAMETER;
    }

    flow_descriptor_p = (eip207_flow_fr_dscr_t *)element_p->data_object_p;

    if (flow_descriptor_p == NULL)
    {
        LOG_CRIT("pcl_flow_remove: failed, invalid flow handle\n");
        return PCL_INVALID_PARAMETER;
    }

    // get interface ioarea
    dev_p = adapter_pcl_device_get(interface_id);
    ioarea_p = dev_p->eip207_io_area_p;

    if (adapter_lock_cs_get(&dev_p->adapter_pcl_dev_cs) == adapter_lock_null)
    {
        LOG_CRIT("pcl_flow_add: no device lock, not initialized?\n");
        return PCL_ERROR;
    }

    if (!adapter_lock_cs_enter(&dev_p->adapter_pcl_dev_cs))
        return PCL_STATUS_BUSY;

    if (RPM_DEVICE_IO_START_MACRO(ADAPTER_PCL_RPM_EIP207_DEVICE_ID,
                                  RPM_FLAG_SYNC) != RPM_SUCCESS)
    {
        adapter_lock_cs_leave(&dev_p->adapter_pcl_dev_cs);
        return PCL_ERROR;
    }

    LOG_INFO("\n\t\t eip207_flow_dtl_fr_remove \n");

    eip207_rc = eip207_flow_dtl_fr_remove(ioarea_p,
                                          flow_hash_table_id,
                                          flow_descriptor_p);
    if (eip207_rc != EIP207_FLOW_NO_ERROR)
    {
        LOG_CRIT("pcl_flow_remove: failed to remove FR, err=%d\n", eip207_rc);
        pcl_rc = PCL_ERROR;
    }

    (void)RPM_DEVICE_IO_STOP_MACRO(ADAPTER_PCL_RPM_EIP207_DEVICE_ID,
                                   RPM_FLAG_ASYNC);

    adapter_lock_cs_leave(&dev_p->adapter_pcl_dev_cs);

    return pcl_rc;
}


/*-----------------------------------------------------------------------------
 * pcl_transform_register
 */
pcl_status_t
pcl_transform_register(
        const dma_buf_handle_t transform_handle)
{
    unsigned int tr_word_count;
    dma_resource_handle_t dma_handle =
                adapter_dma_buf_handle2_dma_resource_handle(transform_handle);
    dma_resource_record_t * const rec_p =
                                dma_resource_handle2_record_ptr(dma_handle);

    LOG_INFO("\n\t pcl_transform_register \n");

    // validate parameter
    if (rec_p == NULL)
        return PCL_INVALID_PARAMETER;

    {
#ifndef ADAPTER_PCL_USE_LARGE_TRANSFORM_DISABLE
        u32 *tr_p = (u32*)adapter_dma_resource_host_addr(dma_handle);

        rec_p->f_is_large_transform = false;

        // Check whether the transform record is large.
        // Register that in the DMA resource record.
        if ((*tr_p & ADAPTER_PCL_TR_ISLARGE) != 0)
        {
            rec_p->f_is_large_transform = true;

            tr_word_count = eip207_flow_dtl_tr_large_word_count_get();
            *tr_p = *tr_p & ~ADAPTER_PCL_TR_ISLARGE;
            // Clear that bit in the transform record.
        }
        else
#endif // !ADAPTER_PCL_USE_LARGE_TRANSFORM_DISABLE
        {
            tr_word_count = eip207_flow_tr_word_count_get();
        }
    }

#ifdef ADAPTER_PEC_ARMRING_ENABLE_SWAP
    dma_resource_swap_endianness_set(dma_handle, true);
#endif

#ifdef ADAPTER_PCL_DMARESOURCE_BANKS_ENABLE
    if (rec_p->props.bank != ADAPTER_PCL_BANK_TRANSFORM)
    {
        LOG_CRIT("PCL Adapter: Invalid bank for Transform\n");
        return PCL_ERROR;
    }
#endif

    if (rec_p->props.size < (sizeof(u32) * tr_word_count))
    {
        LOG_CRIT("pcl_transform_register: supplied buffer too small\n");
        return PCL_ERROR;
    }

    dma_resource_write32_array(
            dma_handle,
            0,
            tr_word_count,
            adapter_dma_resource_host_addr(dma_handle));

    dma_resource_pre_dma(dma_handle, 0, sizeof(u32) * tr_word_count);

    return PCL_STATUS_OK;
}


/*-----------------------------------------------------------------------------
 * pcl_transform_un_register
 */
pcl_status_t
pcl_transform_un_register(
        const dma_buf_handle_t transform_handle)
{
    IDENTIFIER_NOT_USED(transform_handle.p);

    LOG_INFO("\n\t pcl_transform_un_register \n");

    // Kept for backwards-compatibility, nothing to do here

    return PCL_STATUS_OK;
}

/*-----------------------------------------------------------------------------
 * pcl_transform_get_read_only
 */
pcl_status_t
pcl_transform_get_read_only(
        const dma_buf_handle_t transform_handle,
        pcl_transform_params_t * const transform_params_p)
{
    eip207_flow_tr_dscr_t transform_descriptor;
    eip207_flow_tr_output_data_t transform_data;
    eip207_flow_error_t res;
    eip207_flow_io_area_t * ioarea_p;
    adapter_pcl_device_instance_data_t * dev_p;
    dma_resource_record_t * rec_p = NULL;

    LOG_INFO("\n\t pcl_transform_get_read_only \n");

    if( adapter_pcl_dma_buf_to_tr_dscr(
            transform_handle, &transform_descriptor, &rec_p) != PCL_STATUS_OK)
        return PCL_ERROR;

    // get interface ioarea
    dev_p = adapter_pcl_device_get(0);
    ioarea_p = dev_p->eip207_io_area_p;
    if (ioarea_p == NULL)
    {
        LOG_CRIT("pcl_transform_get_read_only: failed, not initialized\n");
        return PCL_ERROR;
    }

    {
#ifndef ADAPTER_PCL_USE_LARGE_TRANSFORM_DISABLE
        if (rec_p->f_is_large_transform)
        {
            res = eip207_flow_dtl_tr_large_read(
                ioarea_p,
                0,
                &transform_descriptor,
                &transform_data);
        }
        else
#else
        IDENTIFIER_NOT_USED(rec_p);
#endif // !ADAPTER_PCL_USE_LARGE_TRANSFORM_DISABLE
        {
            res = eip207_flow_tr_read(
                ioarea_p,
                0,
                &transform_descriptor,
                &transform_data);
        }
        if (res != EIP207_FLOW_NO_ERROR)
        {
            LOG_CRIT("pcl_transform_get_read_only: "
                    "Failed to remove transform record\n");
            return PCL_ERROR;
        }
    }

    transform_params_p->sequence_number   = transform_data.sequence_number;
    transform_params_p->packets_counter_lo = transform_data.packets_counter;
    transform_params_p->packets_counter_hi = 0;
    transform_params_p->octets_counter_lo  = transform_data.octets_counter_lo;
    transform_params_p->octets_counter_hi  = transform_data.octets_counter_hi;

    return PCL_STATUS_OK;
}


/*-----------------------------------------------------------------------------
 * PCL DTL API functions implementation
 *
 */


/*-----------------------------------------------------------------------------
 * pcl_dtl_transform_add
 */
pcl_status_t
pcl_dtl_transform_add(
        const unsigned int interface_id,
        const unsigned int flow_hash_table_id,
        const pcl_dtl_transform_params_t * const transform_params,
        const dma_buf_handle_t xform_dma_handle,
        pcl_dtl_hash_handle_t * const hash_handle_p)
{
    pcl_status_t pcl_rc = PCL_ERROR;
    pcl_status_t pcl_rc2;
    eip207_flow_error_t eip207_rc;
    eip207_flow_io_area_t * ioarea_p;
    eip207_flow_tr_dscr_t * tr_dscr_p;
    eip207_flow_tr_input_data_t tr_inputdata;
    adapter_pcl_device_instance_data_t * dev_p;
    void * hash_list_p = NULL;
    list_status_t list_rc;
    dma_resource_record_t * rec_p = NULL;
    list_element_t * element_p = NULL;

    LOG_INFO("\n\t %s \n", __func__);

    // Validate input parameters
    if (interface_id >= ADAPTER_PCL_MAX_FLUE_DEVICES ||
        flow_hash_table_id >= ADAPTER_CS_MAX_NOF_FLOW_HASH_TABLES_TO_USE)
        return PCL_INVALID_PARAMETER;

    // Get interface ioarea
    dev_p = adapter_pcl_device_get(interface_id);
    ioarea_p = dev_p->eip207_io_area_p;

    if (dev_p->list_p == NULL)
    {
        LOG_CRIT("%s: failed, not initialized\n", __func__);
        return PCL_ERROR;
    }

    if (adapter_lock_cs_get(&dev_p->adapter_pcl_dev_cs) == adapter_lock_null)
    {
        LOG_CRIT("%s: failed, no device lock, not initialized?\n", __func__);
        return PCL_ERROR;
    }

    if (!adapter_lock_cs_enter(&dev_p->adapter_pcl_dev_cs))
        return PCL_STATUS_BUSY;

    // Try to add new hash value to Flow Hash Table
    {
        unsigned int list_id;

        pcl_rc2 = adapter_pcl_list_id_get(interface_id, &list_id);
        if (pcl_rc2 != PCL_STATUS_OK)
        {
            LOG_CRIT("%s: failed to get free list\n", __func__);
            goto error_exit;
        }

        list_rc = list_remove_from_tail(list_id, NULL, &element_p);
        if (list_rc != LIST_STATUS_OK)
        {
            LOG_CRIT(
                "%s: failed to get free element from list\n", __func__);
            goto error_exit;
        }

        // Note: element_p and tr_dscr_p must be valid by implementation!
        tr_dscr_p = (eip207_flow_tr_dscr_t*)element_p->data_object_p;

        // Set list element with the transform record descriptor data
        pcl_rc2 = adapter_pcl_dma_buf_to_tr_dscr(xform_dma_handle, tr_dscr_p, &rec_p);
        if (pcl_rc2 != PCL_STATUS_OK)
        {
            LOG_CRIT("%s: failed, invalid transform\n", __func__);
            pcl_rc = PCL_INVALID_PARAMETER;
            goto error_exit;
        }

        // Convert transform parameters into EIP-207 transform parameters
        zeroinit(tr_inputdata);
        tr_inputdata.hash_id.word32[0] = transform_params->hash_id[0];
        tr_inputdata.hash_id.word32[1] = transform_params->hash_id[1];
        tr_inputdata.hash_id.word32[2] = transform_params->hash_id[2];
        tr_inputdata.hash_id.word32[3] = transform_params->hash_id[3];

#ifndef ADAPTER_PCL_USE_LARGE_TRANSFORM_DISABLE
        tr_inputdata.f_large = rec_p->f_is_large_transform;
#else
        tr_inputdata.f_large = false;
#endif

        LOG_INFO("\n\t\t eip207_flow_dtl_tr_add \n");

        // Add new hash value for this transform record to Flow Hash Table
        eip207_rc = eip207_flow_dtl_tr_add(ioarea_p,
                                           flow_hash_table_id,
                                           tr_dscr_p,
                                           &tr_inputdata);
        if (eip207_rc == EIP207_FLOW_OUT_OF_MEMORY_ERROR)
        {
            LOG_CRIT("%s: failed to install transform, "
                     "out of memory\n", __func__);
            pcl_rc = PCL_OUT_OF_MEMORY_ERROR;
        }
        else if (eip207_rc == EIP207_FLOW_NO_ERROR)
        {
            pcl_rc = PCL_STATUS_OK;
        }
        else
        {
            LOG_CRIT("%s: failed to install transform, "
                     "eip207_flow_dtl_tr_add() error %d\n",
                     __func__,
                     eip207_rc);
        }

        if (pcl_rc != PCL_STATUS_OK)
        {
            LOG_CRIT("%s: failed to add hash value for transform record\n",
                     __func__);

            // Return not added element to free list
            list_rc = list_add_to_head(list_id, NULL, element_p);
            if (list_rc != LIST_STATUS_OK)
            {
                LOG_CRIT("%s: failed to update free list\n", __func__);
            }

            pcl_rc = PCL_INVALID_PARAMETER;
            goto error_exit;
        }
    } // Done adding new hash value to Flow Hash Table

    if (rec_p->context_p)
    {
        // Get list element
        list_element_t * tmp_element_p = rec_p->context_p;

        // Get hash list that contains all transform record descriptors
        hash_list_p = tmp_element_p->data_object_p;
    }

    if(hash_list_p == NULL)
    {
        // Create hash list for transform record

        list_element_t * tmp_element_p = NULL;

        // Get a free list from pool of lists
        list_rc = list_remove_from_tail(LIST_DUMMY_LIST_ID,
                                      dev_p->list_p,
                                      &tmp_element_p);
        if (list_rc != LIST_STATUS_OK)
        {
            LOG_CRIT("%s: failed to get free element from pool list\n",
                     __func__);
            goto error_exit;
        }

        // Note: tmp_element_p must be valid by implementation!
        hash_list_p = tmp_element_p->data_object_p;

        // Initialize list instance
        if (list_init(LIST_DUMMY_LIST_ID, hash_list_p) != LIST_STATUS_OK)
        {
            LOG_CRIT("%s: list initialization failed\n", __func__);
            goto error_exit;
        }

        // Store list element in the transform record descriptor
        rec_p->context_p = tmp_element_p;
    } // Created hash list for transform record

    // Add new hash value to list of hash values for this transform record
    list_rc = list_add_to_head(LIST_DUMMY_LIST_ID, hash_list_p, element_p);
    if (list_rc != LIST_STATUS_OK)
    {
        LOG_CRIT("%s: failed to update hash list for transform record\n",
                 __func__);
    }

    hash_handle_p->p = element_p;

error_exit:

    adapter_lock_cs_leave(&dev_p->adapter_pcl_dev_cs);
    return pcl_rc;
}


/*-----------------------------------------------------------------------------
 * pcl_dtl_transform_remove
 */
pcl_status_t
pcl_dtl_transform_remove(
        const unsigned int interface_id,
        const unsigned int flow_hash_table_id,
        const dma_buf_handle_t xform_dma_handle)
{
    pcl_status_t pcl_rc = PCL_ERROR;
    pcl_status_t pcl_rc2;
    adapter_pcl_device_instance_data_t * dev_p;
    eip207_flow_error_t eip207_rc;
    eip207_flow_io_area_t * ioarea_p;
    eip207_flow_tr_dscr_t * tr_dscr_p;
    list_status_t list_rc;
    list_element_t * element_p;
    list_element_t * list_element_p;
    unsigned int list_id;
    void * hash_list_p;

    dma_resource_handle_t dma_handle =
        adapter_dma_buf_handle2_dma_resource_handle(xform_dma_handle);

    dma_resource_record_t * rec_p = dma_resource_handle2_record_ptr(dma_handle);

    LOG_INFO("\n\t %s \n", __func__);

    // Validate input parameters
    if (interface_id >= ADAPTER_PCL_MAX_FLUE_DEVICES ||
        flow_hash_table_id >= ADAPTER_CS_MAX_NOF_FLOW_HASH_TABLES_TO_USE ||
        rec_p == NULL)
        return PCL_INVALID_PARAMETER;

    // Get interface ioarea
    dev_p = adapter_pcl_device_get(interface_id);
    ioarea_p = dev_p->eip207_io_area_p;

    if (dev_p->list_p == NULL)
    {
        LOG_CRIT("%s: failed, not initialized\n", __func__);
        return PCL_ERROR;
    }

    if (adapter_lock_cs_get(&dev_p->adapter_pcl_dev_cs) == adapter_lock_null)
    {
        LOG_CRIT("%s: no device lock, not initialized?\n", __func__);
        return PCL_ERROR;
    }

    if (!adapter_lock_cs_enter(&dev_p->adapter_pcl_dev_cs))
        return PCL_STATUS_BUSY;

    pcl_rc2 = adapter_pcl_list_id_get(interface_id, &list_id);
    if (pcl_rc2 != PCL_STATUS_OK)
    {
        LOG_CRIT("%s: failed to get free list\n", __func__);
        goto error_exit;
    }

    // Get list element
    list_element_p = rec_p->context_p;
    if (list_element_p == NULL || list_element_p->data_object_p == NULL)
    {
        pcl_rc = PCL_INVALID_PARAMETER;
        goto error_exit;
    }

    // Get hash list that contains all transform record descriptors
    // for this transform
    hash_list_p = list_element_p->data_object_p;

    while (hash_list_p)
    {
        // Get the element from hash list that references record descriptor
        list_rc = list_remove_from_tail(LIST_DUMMY_LIST_ID,
                                      hash_list_p,
                                      &element_p);
        if (list_rc != LIST_STATUS_OK || element_p == NULL)
        {
            LOG_CRIT(
                "%s: failed to get element from hash list\n", __func__);
            goto error_exit;
        }

        // Retrieve transform record descriptor
        tr_dscr_p = element_p->data_object_p;
        if (tr_dscr_p == NULL)
        {
            LOG_CRIT("%s: failed, invalid transform handle\n", __func__);
            pcl_rc = PCL_INVALID_PARAMETER;
            goto error_exit;
        }

        if (RPM_DEVICE_IO_START_MACRO(ADAPTER_PCL_RPM_EIP207_DEVICE_ID + interface_id,
                                      RPM_FLAG_SYNC) != RPM_SUCCESS)
        {
            pcl_rc = PCL_ERROR;
            goto error_exit;
        }

        LOG_INFO("\n\t\t eip207_flow_dtl_tr_remove \n");

        eip207_rc = eip207_flow_dtl_tr_remove(ioarea_p,
                                              flow_hash_table_id,
                                              tr_dscr_p);

        (void)RPM_DEVICE_IO_STOP_MACRO(ADAPTER_PCL_RPM_EIP207_DEVICE_ID + interface_id,
                                       RPM_FLAG_ASYNC);

        if (eip207_rc != EIP207_FLOW_NO_ERROR)
        {
            LOG_CRIT("%s: failed to remove TR, "
                     "eip207_flow_dtl_tr_remove() error %d\n",
                    __func__,
                    eip207_rc);
            // Return element to hash list
            list_rc = list_add_to_head(LIST_DUMMY_LIST_ID, hash_list_p, element_p);
            if (list_rc != LIST_STATUS_OK)
            {
                LOG_CRIT("%s: failed to update free list\n", __func__);
            }
            goto error_exit;
        }

        {
            unsigned int count;

            // Return the element to the free list
            list_rc = list_add_to_head(list_id, NULL, element_p);
            if (list_rc != LIST_STATUS_OK)
            {
                LOG_CRIT(
                    "%s: failed to get free element from list\n", __func__);
                goto error_exit;
            }

            // Check if transform record has any hash values still referencing it
            list_get_list_element_count(LIST_DUMMY_LIST_ID, hash_list_p, &count);
            if (count == 0)
            {
                // Return the element to the free list
                list_rc = list_add_to_head(LIST_DUMMY_LIST_ID,
                                         dev_p->list_p,
                                         list_element_p);
                if (list_rc != LIST_STATUS_OK)
                {
                    LOG_CRIT("%s: failed to return element to pool list\n",
                              __func__);
                    goto error_exit;
                }

                rec_p->context_p = hash_list_p = NULL;
            }
        }
    } // while

    pcl_rc = PCL_STATUS_OK;

error_exit:
    adapter_lock_cs_leave(&dev_p->adapter_pcl_dev_cs);
    return pcl_rc;
}


/*-----------------------------------------------------------------------------
 * pcl_dtl_hash_remove
 */
pcl_status_t
pcl_dtl_hash_remove(
        const unsigned int interface_id,
        const unsigned int flow_hash_table_id,
        pcl_dtl_hash_handle_t * const hash_handle_p)
{
    pcl_status_t pcl_rc = PCL_ERROR;
    pcl_status_t pcl_rc2;
    eip207_flow_error_t eip207_rc;
    eip207_flow_io_area_t * ioarea_p;
    eip207_flow_tr_dscr_t * tr_dscr_p;
    list_element_t * element_p;
    adapter_pcl_device_instance_data_t * dev_p;
    dma_resource_record_t * rec_p;

    LOG_INFO("\n\t %s \n", __func__);

    // Validate input parameters
    if (interface_id >= ADAPTER_PCL_MAX_FLUE_DEVICES ||
        flow_hash_table_id >= ADAPTER_CS_MAX_NOF_FLOW_HASH_TABLES_TO_USE)
        return PCL_INVALID_PARAMETER;

    // Get interface ioarea
    dev_p = adapter_pcl_device_get(interface_id);
    ioarea_p = dev_p->eip207_io_area_p;

    if (dev_p->list_p == NULL)
    {
        LOG_CRIT("%s: failed, not initialized\n", __func__);
        return PCL_ERROR;
    }

    if (adapter_lock_cs_get(&dev_p->adapter_pcl_dev_cs) == adapter_lock_null)
    {
        LOG_CRIT("%s: no device lock, not initialized?\n", __func__);
        return PCL_ERROR;
    }

    if (!adapter_lock_cs_enter(&dev_p->adapter_pcl_dev_cs))
        return PCL_STATUS_BUSY;

    // Retrieve transform record descriptor
    element_p = hash_handle_p->p;
    if (element_p == NULL)
    {
        LOG_CRIT("%s: failed, invalid hash handle\n", __func__);
        pcl_rc = PCL_INVALID_PARAMETER;
        goto error_exit;
    }

    tr_dscr_p = element_p->data_object_p;
    if (tr_dscr_p == NULL)
    {
        LOG_CRIT("%s: failed, invalid hash handle for transform descriptor\n",
                 __func__);
        pcl_rc = PCL_INVALID_PARAMETER;
        goto error_exit;
    }

    rec_p = dma_resource_handle2_record_ptr(tr_dscr_p->dma_handle);
    if (rec_p == NULL || rec_p->context_p == NULL)
    {
        LOG_CRIT("%s: failed, invalid hash handle for DMA resource\n",
                 __func__);
        pcl_rc = PCL_INVALID_PARAMETER;
        goto error_exit;
    }

    if (RPM_DEVICE_IO_START_MACRO(ADAPTER_PCL_RPM_EIP207_DEVICE_ID + interface_id,
                                  RPM_FLAG_SYNC) != RPM_SUCCESS)
    {
        pcl_rc = PCL_ERROR;
        goto error_exit;
    }

    LOG_INFO("\n\t\t eip207_flow_dtl_tr_remove \n");

    eip207_rc = eip207_flow_dtl_tr_remove(ioarea_p, flow_hash_table_id, tr_dscr_p);

    (void)RPM_DEVICE_IO_STOP_MACRO(ADAPTER_PCL_RPM_EIP207_DEVICE_ID + interface_id,
                                   RPM_FLAG_ASYNC);

    if (eip207_rc != EIP207_FLOW_NO_ERROR)
    {
        LOG_CRIT("%s: failed to remove TR, "
                 "eip207_flow_dtl_tr_remove() error %d\n",
                __func__,
                eip207_rc);
        goto error_exit;
    }

    {
        unsigned int list_id, count;
        list_status_t list_rc;
        list_element_t * list_element_p = rec_p->context_p;
        void * hash_list_p = list_element_p->data_object_p;

        // Remove the hash value from the transform record list
        list_rc = list_remove_anywhere(LIST_DUMMY_LIST_ID,
                                      hash_list_p,
                                      element_p);
        if (list_rc != LIST_STATUS_OK)
        {
            LOG_CRIT("%s: failed to remove hash from record list\n", __func__);
        }

        pcl_rc2 = adapter_pcl_list_id_get(interface_id, &list_id);
        if (pcl_rc2 != PCL_STATUS_OK)
        {
            LOG_CRIT("%s: failed to get free list\n", __func__);
            goto error_exit;
        }

        // Return the element to the free list
        list_rc = list_add_to_head(list_id, NULL, element_p);
        if (list_rc != LIST_STATUS_OK)
        {
            LOG_CRIT(
                "%s: failed to get free element from list\n", __func__);
            goto error_exit;
        }

        // Check if transform record has any hash values still referencing it
        list_get_list_element_count(LIST_DUMMY_LIST_ID, hash_list_p, &count);
        if (count == 0)
        {
            // Return the element to the free list
            list_rc = list_add_to_head(LIST_DUMMY_LIST_ID,
                                     dev_p->list_p,
                                     list_element_p);
            if (list_rc != LIST_STATUS_OK)
            {
                LOG_CRIT("%s: failed to return element to pool list\n",
                          __func__);
                goto error_exit;
            }

            rec_p->context_p = NULL;
        }
    }

    // Invalidate hash handle
    hash_handle_p->p = NULL;

    pcl_rc = PCL_STATUS_OK;

error_exit:
    adapter_lock_cs_leave(&dev_p->adapter_pcl_dev_cs);
    return pcl_rc;
}


/*-----------------------------------------------------------------------------
 * pcl_dtl_init
 */
pcl_status_t
pcl_dtl_init(
        const unsigned int interface_id)
{
    pcl_status_t pcl_rc = PCL_ERROR;
    adapter_pcl_device_instance_data_t * dev_p;

    LOG_INFO("\n\t %s \n", __func__);

    // Validate input parameters.
    if (interface_id >= ADAPTER_PCL_MAX_FLUE_DEVICES)
        return PCL_INVALID_PARAMETER;

    // Get interface ioarea
    dev_p = adapter_pcl_device_get(interface_id);

    if (dev_p->list_p != NULL)
    {
        LOG_CRIT("%s: failed, already initialized\n", __func__);
        return PCL_ERROR;
    }

    if (adapter_lock_cs_get(&dev_p->adapter_pcl_dev_cs) == adapter_lock_null)
    {
        LOG_CRIT("%s: failed, no device lock, not initialized?\n", __func__);
        return PCL_ERROR;
    }

    if (!adapter_lock_cs_enter(&dev_p->adapter_pcl_dev_cs))
        return PCL_STATUS_BUSY;

    // Create pool of lists for record descriptors
    {
        unsigned int i;
        void * list_p;
        list_element_t * list_element_pool_p;
        list_element_t * element_p;
        unsigned char * list_pool_p;
        unsigned int list_instance_byte_count = list_get_instance_byte_count();

        // Allocate PCL DTL list instance that will chain other lists
        list_p = adapter_alloc(list_instance_byte_count);
        if (list_p == NULL)
        {
            LOG_CRIT("%s: list pool allocation failed\n", __func__);
            goto error_exit;
        }
        memset(list_p, 0, list_instance_byte_count);

        // Initialize PCL DTL list instance
        if (list_init(LIST_DUMMY_LIST_ID,
                      list_p) != LIST_STATUS_OK)
        {
            LOG_CRIT("%s: list pool initialization failed\n", __func__);
            goto error_exit;
        }

        // Allocate a pool of list elements
        list_element_pool_p = adapter_alloc(sizeof(list_element_t) *
                                            ADAPTER_PCL_FLOW_RECORD_COUNT);
        if (list_element_pool_p == NULL)
        {
            LOG_CRIT("%s: pool elements allocation failed\n", __func__);
            adapter_free(list_p);
            goto error_exit;
        }

        // Allocate a pool of lists,
        // one list for one transform record
        list_pool_p = adapter_alloc(list_instance_byte_count *
                                           ADAPTER_PCL_FLOW_RECORD_COUNT);
        if (list_pool_p == NULL)
        {
            LOG_CRIT("%s:  pool lists allocation failed\n", __func__);
            adapter_free(list_p);
            adapter_free(list_element_pool_p);
            goto error_exit;
        }
        memset(list_pool_p,
               0,
               list_instance_byte_count * ADAPTER_PCL_FLOW_RECORD_COUNT);
        dev_p->list_pool_p = list_pool_p;

        // Populate the pool list with the elements (lists)
        element_p = list_element_pool_p;
        element_p->data_object_p = list_pool_p;
        for (i = 0; i < ADAPTER_PCL_FLOW_RECORD_COUNT; i++)
        {
            if (list_add_to_head(LIST_DUMMY_LIST_ID,
                               list_p,
                               element_p) == LIST_STATUS_OK)
            {
                if (i + 1 < ADAPTER_PCL_FLOW_RECORD_COUNT)
                {
                    element_p++;
                    list_pool_p += list_instance_byte_count;
                    element_p->data_object_p = list_pool_p;
                }
            }
            else
            {
                LOG_CRIT("%s: pool list population failed\n", __func__);
                dev_p->list_pool_p = NULL;
                adapter_free(list_p);
                adapter_free(list_element_pool_p);
                adapter_free(list_pool_p);
                goto error_exit;
            }
        } // for

        dev_p->list_p            = list_p;
        dev_p->list_element_pool_p = list_element_pool_p;

        pcl_rc = PCL_STATUS_OK;
    }  // Created pool of lists for record descriptors

error_exit:

    adapter_lock_cs_leave(&dev_p->adapter_pcl_dev_cs);
    return pcl_rc;
}


/*-----------------------------------------------------------------------------
 * pcl_dtl_uninit
 */
pcl_status_t
pcl_dtl_uninit(
        const unsigned int interface_id)
{
    adapter_pcl_device_instance_data_t * dev_p;

    LOG_INFO("\n\t %s \n", __func__);

    // Validate input parameters.
    if (interface_id >= ADAPTER_PCL_MAX_FLUE_DEVICES)
        return PCL_INVALID_PARAMETER;

    // Get interface ioarea
    dev_p = adapter_pcl_device_get(interface_id);
    if (dev_p->list_p == NULL)
    {
        LOG_CRIT("%s: failed, not initialized\n", __func__);
        return PCL_ERROR;
    }

    if (adapter_lock_cs_get(&dev_p->adapter_pcl_dev_cs) == adapter_lock_null)
    {
        LOG_CRIT("%s: failed, no device lock, not initialized?\n", __func__);
        return PCL_ERROR;
    }

    if (!adapter_lock_cs_enter(&dev_p->adapter_pcl_dev_cs))
        return PCL_STATUS_BUSY;

    // pool list data structures
    adapter_free(dev_p->list_element_pool_p);
    adapter_free(dev_p->list_pool_p);
    list_uninit(LIST_DUMMY_LIST_ID, dev_p->list_p);
    adapter_free(dev_p->list_p);

    dev_p->list_p               = NULL;
    dev_p->list_element_pool_p    = NULL;
    dev_p->list_pool_p           = NULL;

    adapter_lock_cs_leave(&dev_p->adapter_pcl_dev_cs);

    return PCL_STATUS_OK;
}


/* end of file adapter_pcl_dtl.c */
