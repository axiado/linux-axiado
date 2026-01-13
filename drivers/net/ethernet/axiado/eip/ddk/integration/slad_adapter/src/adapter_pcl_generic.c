/* adapter_pcl_generic.c
 *
 * Packet Classification (PCL) Generic API implementation.
 *
 * Notes:
 * - this implementation does not use SHDEVXS
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
******************************************************************************/

/*----------------------------------------------------------------------------
 * This module implements (provides) the following interface(s):
 */

// PCL API
#include "api_pcl.h"            // PCL_DTL_*

// Adapter PCL internal API
#include "adapter_pcl.h"        // AdapterPCL_*


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

// sleeping API/
#include "adapter_sleep.h"

// Adapter Locking internal API
#include "adapter_lock.h"       // Adapter_Lock_*

// Runtime power Management device Macros API
#include "rpm_device_macros.h"  // RPM_*

// Logging API
#include "log.h"

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

// Driver Framework C Run-Time Library API
#include "clib.h"               // memcpy, memset

// Driver Framework Basic Definitions API
#include "basic_defs.h"         // bool, u32

// EIP-207 Driver Library
#include "eip207_flow_generic.h"


/*----------------------------------------------------------------------------
 * Definitions and macros
 */

#if ADAPTER_PCL_MAX_DEVICE_DIGITS > 2
#error "ADAPTER_PCL_MAX_DEVICE_DIGITS > 2 unsupported"
#endif

// Maximum number of 32-bit words in a 4Gb bank - representable as u32
#define ADAPTER_PCL_MAX_BANK_WORDS ((u32)((1ULL<<32) / sizeof(u32)))


/*----------------------------------------------------------------------------
 * Local variables
 */

static adapter_pcl_device_instance_data_t dev_instance [ADAPTER_PCL_MAX_FLUE_DEVICES];

// DMA buffer allocation alignment, in bytes
static int adapter_pcl_dma_alignment_byte_count =
                                ADAPTER_DMABUF_ALIGNMENT_INVALID;

// Global lock and critical section for PCL API
static ADAPTER_LOCK_DEFINE(AdapterPCL_Lock);
static adapter_lock_cs_t adapter_pcl_cs;

// Cached values for RPM device resume operation
static eip207_flow_address_t eip207_flow_base_addr;
static eip207_flow_address_t eip207_transform_base_addr;
static eip207_flow_address_t eip207_flow_addr;
static eip207_flow_ht_t eip207_ht_params;


/*----------------------------------------------------------------------------
 * function definitions
 */


/*----------------------------------------------------------------------------
  adapter_pcl_lib_hash_table_entries_num_to_size

  Convert numerical value of number of hashtable entries, to corresponding enum

  number (input)
    value indicating number of hashtable entries (eg. 32)

  Return:
      eip207_flow_hash_table_entry_count_t value representing 'number'
      or
      EIP207_FLOW_HASH_TABLE_ENTRIES_MAX+1 indicating error

   Note: implemented using macros to ensure numeric values in step with enums
 */
static eip207_flow_hash_table_entry_count_t
adapter_pcl_lib_hash_table_entries_num_to_size(
        int number)
{
#define HASHTABLE_SIZE_CASE(num) \
    case (num): return EIP207_FLOW_HASH_TABLE_ENTRIES_##num

    switch (number)
    {
        HASHTABLE_SIZE_CASE(32);
        HASHTABLE_SIZE_CASE(64);
        HASHTABLE_SIZE_CASE(128);
        HASHTABLE_SIZE_CASE(256);
        HASHTABLE_SIZE_CASE(512);
        HASHTABLE_SIZE_CASE(1024);
        HASHTABLE_SIZE_CASE(2048);
        HASHTABLE_SIZE_CASE(4096);
        HASHTABLE_SIZE_CASE(8192);
        HASHTABLE_SIZE_CASE(16384);
        HASHTABLE_SIZE_CASE(32768);
        HASHTABLE_SIZE_CASE(65536);
        HASHTABLE_SIZE_CASE(131072);
        HASHTABLE_SIZE_CASE(262144);
        HASHTABLE_SIZE_CASE(524288);
        HASHTABLE_SIZE_CASE(1048576);

        default:
            return EIP207_FLOW_HASH_TABLE_ENTRIES_MAX + 1;  //invalid value
    }
#undef HASHTABLE_SIZE_CASE
}


/*----------------------------------------------------------------------------
 * adapter_pcl_lib_device_find
 *
 * Returns device handle corresponding to device interface id (index)
 * Appends characters corresponding to id number to the device base name.
 * The base name ADAPTER_PCL_FLUE_DEFAULT_DEVICE_NAME may optionally end in zero '0'.
 * @note Will handle interface index 0-99
 */
static device_handle_t
adapter_pcl_lib_device_find(unsigned int interface_id)
{
    char device_name_digit;
    // base name
    char device_name[]= ADAPTER_PCL_FLUE_DEFAULT_DEVICE_NAME "\0\0\0\0";

    // string's last char
    int device_name_digit_index = strlen(device_name) - 1;

    // parameter validation
    if (interface_id >= ADAPTER_PCL_MAX_FLUE_DEVICES ||    // config max
        interface_id > 99)   // algorithm max
    {
        return NULL;
    }

    // use final digit (0) if it's present, otherwise skip to end of base name
    device_name_digit = device_name[device_name_digit_index];
    if (device_name_digit != '0')
    {
        ++device_name_digit_index;
    }

    if (interface_id >= 10)   // two digit identifier, write tens digit
    {
        device_name[device_name_digit_index] = (interface_id / 10) + '0';
        ++device_name_digit_index;
        interface_id %= 10;
    }

    // write units digit
    device_name[device_name_digit_index] = interface_id + '0';

    // look up actual device
    return eip_device_find(device_name);
}


/*-----------------------------------------------------------------------------
 * adapter_pcl_int_to_ptr
 */
static inline void *
adapter_pcl_int_to_ptr(
        const unsigned int value)
{
    union number
    {
        void * p;
        uintptr_t value;
    } N;

    N.value = (uintptr_t)value;

    return N.p;
}


#ifdef ADAPTER_PCL_RPM_EIP207_DEVICE_ID
/*-----------------------------------------------------------------------------
 * adapter_pcl_resume
 */
static int
adapter_pcl_resume(void * p)
{
    eip207_flow_error_t res;
    adapter_pcl_device_instance_data_t * dev_p;
    int interface_id = *(int *)p;

    // only one hash table in this implementation
    const unsigned int hashtable_id = 0;

    LOG_INFO("\n\t %s \n", __func__);

    if (interface_id < 0 || interface_id < ADAPTER_PCL_RPM_EIP207_DEVICE_ID)
        return -1; // error

    interface_id -= ADAPTER_PCL_RPM_EIP207_DEVICE_ID;

    // select current device global instance data, on validated interface_id
    dev_p = &dev_instance[interface_id];

#ifdef ADAPTER_PCL_DMARESOURCE_BANKS_ENABLE
    LOG_INFO("\n\t\t eip207_flow_rc_base_addr_set \n");

    // Restore base addresses for flow and transform records
    res = eip207_flow_rc_base_addr_set(dev_p->eip207_io_area_p,
                                      hashtable_id,
                                      &eip207_flow_base_addr,
                                      &eip207_transform_base_addr);
    if (res != EIP207_FLOW_NO_ERROR)
    {
        LOG_CRIT("%s: set records base address failed, error %d\n",
                 __func__,
                 res);
        return -3;
    }
#endif // ADAPTER_PCL_DMARESOURCE_BANKS_ENABLE

    LOG_INFO("\n\t\t eip207_flow_hash_table_install \n");

    // Restore flue hashtable
    res =  eip207_flow_hash_table_install(dev_p->eip207_io_area_p,
                                         hashtable_id,
                                         &eip207_ht_params,
                                         ADAPTER_PCL_ENABLE_FLUE_CACHE,
                                         false);
    if (res != EIP207_FLOW_NO_ERROR)
    {
        LOG_CRIT("%s: eip207_flow_hash_table_install() failed, error %d\n",
                 __func__,
                 res);
        return -4;
    }

    return 0;
}
#endif


/*-----------------------------------------------------------------------------
 * Adapter PCL internal API functions implementation
 */

/*----------------------------------------------------------------------------
 * adapter_pcl_dma_buf_to_tr_dscr
 */
pcl_status_t
adapter_pcl_dma_buf_to_tr_dscr(
        const dma_buf_handle_t transform_handle,
        eip207_flow_tr_dscr_t * const tr_dscr_p,
        dma_resource_record_t ** rec_pp)
{
    dma_resource_handle_t dma_handle =
        adapter_dma_buf_handle2_dma_resource_handle(transform_handle);

    dma_resource_addr_pair_t phys_addr;
    dma_resource_record_t * rec_p;

    rec_p = dma_resource_handle2_record_ptr(dma_handle);

    if (rec_p == NULL)
        return PCL_ERROR;

#ifdef ADAPTER_PCL_DMARESOURCE_BANKS_ENABLE
    if (rec_p->props.bank != ADAPTER_PCL_BANK_TRANSFORM)
    {
        LOG_CRIT("PCL Adapter: Invalid bank for Transform\n");
        return PCL_ERROR;
    }
#endif

    if (dma_resource_translate(dma_handle, DMARES_DOMAIN_BUS, &phys_addr) < 0)
    {
        LOG_CRIT("PCL_Transform: Failed to obtain physical address.\n");
        return PCL_ERROR;
    }

    adapter_addr_to_word_pair(phys_addr.address_p, 0,
                           &tr_dscr_p->dma_addr.addr,
                           &tr_dscr_p->dma_addr.upper_addr);

    tr_dscr_p->dma_handle = dma_handle;

    *rec_pp = rec_p;

    return PCL_STATUS_OK;
}


/*-----------------------------------------------------------------------------
 * adapter_pcl_device_get
 */
adapter_pcl_device_instance_data_t *
adapter_pcl_device_get(
        const unsigned int interface_id)
{
    return &dev_instance[interface_id];
}


/*-----------------------------------------------------------------------------
 * adapter_pcl_list_id_get
 */
pcl_status_t
adapter_pcl_list_id_get(
        const unsigned int interface_id,
        unsigned int * const list_id_p)
{
    // validate parameter
    if (list_id_p == NULL)
        return PCL_INVALID_PARAMETER;

    *list_id_p = dev_instance[interface_id].free_list_id;
    return PCL_STATUS_OK;
}


/*-----------------------------------------------------------------------------
 * Adapter PCL API functions implementation
 */

/*----------------------------------------------------------------------------
 * pcl_init
 */
pcl_status_t
pcl_init(
        const unsigned int interface_id,
        const unsigned int nof_flow_hash_tables)
{
    eip207_flow_error_t res;
    device_handle_t eip207_device;
    adapter_pcl_device_instance_data_t * dev_p;

    unsigned int ioarea_size_bytes;
    unsigned int descr_total_size_bytes;
    unsigned int hashtable_entry_size_words, hashtable_total_size_words;

    list_element_t * element_pool_p = NULL;
    unsigned char * rec_dscr_pool_p = NULL;
    eip207_flow_io_area_t * ioarea_p = NULL;
    void * descr_area_ptr = NULL;

    // includes overflow records
    const unsigned int total_hasharea_element_count =
                            (ADAPTER_PCL_FLOW_HASH_OVERFLOW_COUNT +
                                    ADAPTER_PCL_FLOW_HASH_ENTRIES_COUNT);

    dma_resource_handle_t hashtable_dma_handle = NULL;

    // only one hash table in this implementation
    const unsigned int hashtable_id = 0;

    LOG_INFO("\n\t pcl_init \n");

    // check number of hash tables against implementation & config limits
    if (nof_flow_hash_tables == 0 ||   // validity
        nof_flow_hash_tables > 1 || // implementation limit
        nof_flow_hash_tables > ADAPTER_CS_MAX_NOF_FLOW_HASH_TABLES_TO_USE)
    {
        LOG_CRIT("pcl_init: Invalid number (%d) of hash tables\n",
                 nof_flow_hash_tables);
        return PCL_INVALID_PARAMETER;
    }

    adapter_lock_cs_set(&adapter_pcl_cs, &AdapterPCL_Lock);

    if (!adapter_lock_cs_enter(&adapter_pcl_cs))
        return PCL_STATUS_BUSY;

    // select current device global instance data, on validated interface_id
    dev_p = &dev_instance[interface_id];

    // state consistency check
    if (dev_p->pcl_is_initialized)
    {
        LOG_CRIT("pcl_init: Already initialized\n");
        adapter_lock_cs_leave(&adapter_pcl_cs);
        return PCL_ERROR;
    }

    // Initialize instance variables to 0/NULL defaults
    zeroinit(*dev_p);

    // Allocate device lock
    dev_p->adapter_pcl_dev_lock = adapter_lock_alloc();
    if (dev_p->adapter_pcl_dev_lock == adapter_lock_null)
    {
        LOG_CRIT("pcl_init: PutLock allocation failed\n");
        adapter_lock_cs_leave(&adapter_pcl_cs);
        return PCL_OUT_OF_MEMORY_ERROR;
    }
    adapter_lock_cs_set(&dev_p->adapter_pcl_dev_cs,
                         dev_p->adapter_pcl_dev_lock);

    // identify device, check interface_id
    eip207_device = adapter_pcl_lib_device_find(interface_id);
    if (eip207_device == NULL)
    {
        LOG_CRIT("pcl_init: Cannot find EIP-207 device, id=%d\n", interface_id);
        goto error_exit;
    }

    // Get DMA buffer allocation alignment
    adapter_pcl_dma_alignment_byte_count = adapter_dma_resource_alignment_get();
    if (adapter_pcl_dma_alignment_byte_count == ADAPTER_DMABUF_ALIGNMENT_INVALID)
    {
#if ADAPTER_PCL_DMA_ALIGNMENT_BYTE_COUNT == 0
        LOG_CRIT("pcl_init: Failed to get DMA alignment\n");
        goto error_exit;
#else
        adapter_pcl_dma_alignment_byte_count =
                                ADAPTER_PCL_DMA_ALIGNMENT_BYTE_COUNT;
#endif
    }

    // allocate io_area non-DMA memory
    ioarea_size_bytes = eip207_flow_io_area_byte_count_get();
    ioarea_p = adapter_alloc(ioarea_size_bytes);

    if (ioarea_p == NULL)
    {
        LOG_CRIT("pcl_init: Cannot allocate io_area\n");
        goto error_exit;
    }

    // store value for global use
    dev_p->eip207_io_area_p = ioarea_p;

    if (RPM_DEVICE_INIT_START_MACRO(ADAPTER_PCL_RPM_EIP207_DEVICE_ID + interface_id,
                                    0, // No suspend callback is used
                                    adapter_pcl_resume) != RPM_SUCCESS)
        goto error_exit;

    LOG_INFO("\n\t\t eip207_flow_init \n");

    // hand io_area memory to driver
    res = eip207_flow_init(dev_p->eip207_io_area_p,
                           eip207_device);
    if (res != EIP207_FLOW_NO_ERROR)
    {
        LOG_CRIT("pcl_init: eip207_flow_init() failed\n");
        goto fail;  // exit which frees allocated resources
    }

#ifdef ADAPTER_PCL_DMARESOURCE_BANKS_ENABLE
    {
        unsigned int record_word_count;

        record_word_count = eip207_flow_tr_word_count_get();

        if (record_word_count * sizeof(u32) >
                ADAPTER_TRANSFORM_RECORD_BYTE_COUNT)
        {
            LOG_CRIT("pcl_init: Bad Record size in transform bank, "
                     "is %d, at least %d required\n",
                     (int)(ADAPTER_TRANSFORM_RECORD_BYTE_COUNT *
                               sizeof(u32)),
                     record_word_count);
            goto fail;  // exit which frees allocated resources
        }

        record_word_count = eip207_flow_fr_word_count_get();

        if (record_word_count * sizeof(u32) >
                           ADAPTER_PCL_FLOW_RECORD_BYTE_COUNT)
        {
            LOG_CRIT("pcl_init: Bad Record size in flow bank, "
                     "is %d, at least %d required\n",
                     (int)(ADAPTER_PCL_FLOW_RECORD_BYTE_COUNT *
                               sizeof(u32)),
                     record_word_count);
            goto fail;  // exit which frees allocated resources
        }
    }

    { // Set the SA pool base address.
        int dmares;
        dma_resource_handle_t dma_handle;
        dma_resource_properties_t dma_properties;
        dma_resource_addr_pair_t dma_addr;

        zeroinit(eip207_flow_base_addr);
        zeroinit(eip207_transform_base_addr);

        // Perform a full-bank allocation in transform bank to obtain the bank base
        // address.
        {
            dma_properties.alignment = adapter_pcl_dma_alignment_byte_count;
            dma_properties.bank      = ADAPTER_PCL_BANK_TRANSFORM;
            dma_properties.f_cached   = false;
            dma_properties.size      = ADAPTER_TRANSFORM_RECORD_COUNT *
                                          ADAPTER_TRANSFORM_RECORD_BYTE_COUNT;
            dmares = dma_resource_alloc(dma_properties,
                                       &dma_addr,
                                       &dma_handle);
            if (dmares != 0)
            {
                LOG_CRIT(
                        "pcl_init: allocate transforms base address failed\n");
                goto fail;  // exit which frees allocated resources
            }

            // Derive the physical address from the DMA resource.
            if (dma_resource_translate(dma_handle,
                                      DMARES_DOMAIN_BUS,
                                      &dma_addr)             < 0)
            {
                dma_resource_release(dma_handle);
                LOG_CRIT(
                      "pcl_init: translate transforms base address failed\n");
                goto fail;  // exit which frees allocated resources
            }

            adapter_addr_to_word_pair(dma_addr.address_p,
                                   0,
                                   &eip207_transform_base_addr.addr,
                                   &eip207_transform_base_addr.upper_addr);

            // Release the DMA resource
            dma_resource_release(dma_handle);
        }

        // Perform a size 0 allocation in flow bank to obtain the bank base
        // address.
        dma_properties.alignment = adapter_pcl_dma_alignment_byte_count;
        dma_properties.bank      = ADAPTER_PCL_BANK_FLOW;
        dma_properties.f_cached   = false;
        dma_properties.size      = ADAPTER_PCL_FLOW_RECORD_COUNT *
                                          ADAPTER_PCL_FLOW_RECORD_BYTE_COUNT;
        dmares = dma_resource_alloc(dma_properties,
                                   &dma_addr,
                                   &dma_handle);
        if (dmares != 0)
        {
            LOG_CRIT("pcl_init: allocate flow base address failed\n");
            goto fail;  // exit which frees allocated resources
        }

        // Derive the physical address from the DMA resource.
        if (dma_resource_translate(dma_handle, DMARES_DOMAIN_BUS, &dma_addr) < 0)
        {
            dma_resource_release(dma_handle);
            LOG_CRIT("pcl_init: translate flow base address failed\n");
            goto fail;  // exit which frees allocated resources
        }

        adapter_addr_to_word_pair(dma_addr.address_p, 0,
                               &eip207_flow_base_addr.addr,
                               &eip207_flow_base_addr.upper_addr);

        // Release the DMA resource - handle to zero-size request.
        dma_resource_release(dma_handle);

        LOG_INFO("\n\t\t eip207_flow_rc_base_addr_set \n");

        // set base addresses for flow and transform records
        res = eip207_flow_rc_base_addr_set(
                ioarea_p,
                hashtable_id,
                &eip207_flow_base_addr,
                &eip207_transform_base_addr);
        if (res != EIP207_FLOW_NO_ERROR)
        {
            LOG_CRIT("pcl_init: set records base address failed\n");
            goto fail;  // exit which frees allocated resources
        }

    }
#endif // ADAPTER_PCL_DMARESOURCE_BANKS_ENABLE

    // Install Hashtable:
    // - calculate amount of required descriptor memory & allocate it
    // - calculate amount of required DMA-safe hashtable+overflow memory &
    //   allocate it
    // - fill in struct for HT install, call install function

    // descriptor memory
    descr_total_size_bytes = eip207_flow_hte_dscr_byte_count_get() *
                                               total_hasharea_element_count;

    descr_area_ptr = adapter_alloc(descr_total_size_bytes);
    if (descr_area_ptr == NULL)
    {
        LOG_CRIT("pcl_init: Cannot allocate descriptor area\n");
        goto fail;  // exit which frees allocated resources
    }

    // store value for global use
    dev_p->eip207_descriptor_area_p = descr_area_ptr;

    // get required memory size for hash table
    // = (hash table size + overflow entries)*bucket size
    hashtable_entry_size_words = eip207_flow_ht_entry_word_count_get();
    hashtable_total_size_words =
            hashtable_entry_size_words * total_hasharea_element_count;

    // check total size of hashtable region (lookup + overflow)
    // would not exceed a 32-bit addressable bank of 4GB
    if (hashtable_total_size_words >= ADAPTER_PCL_MAX_BANK_WORDS)
    {
        LOG_CRIT("pcl_init: Too many hashtable lookup elements for bank\n");
        goto fail;  // exit which frees allocated resources
    }

    // get DMA-safe memory for hashtable in appropriate bank
    {
        dma_resource_properties_t table_properties;
        dma_resource_addr_pair_t table_host_addr;
        dma_resource_addr_pair_t phys_addr;
        int dmares;

        // required DMA buffer properties
        table_properties.alignment = adapter_pcl_dma_alignment_byte_count;
        // Hash table DMA bank
        table_properties.bank      = ADAPTER_PCL_BANK_FLOWTABLE;
        table_properties.f_cached   = false;
        // Check if this does not exceed 4 GB, do it somewhere above
        // size in bytes
        table_properties.size      = hashtable_total_size_words *
                                                        sizeof(u32);

        // Perform a full-bank allocation in flow table bank to obtain
        // the bank base address.
        dmares = dma_resource_alloc(table_properties,
                                   &table_host_addr,
                                   &hashtable_dma_handle);

#ifdef ADAPTER_PCL_ENABLE_SWAP
        dma_resource_swap_endianness_set(hashtable_dma_handle, true);
#endif
        if (dmares != 0)
        {
            LOG_CRIT("pcl_init: Failed to allocate flow hash table\n");
            goto fail;  // exit which frees allocated resources
        }

        // get physical address from handle
        if (dma_resource_translate(hashtable_dma_handle,
                                  DMARES_DOMAIN_BUS,
                                  &phys_addr) < 0)
        {
            LOG_CRIT("pcl_init: Failed to obtain physical address.\n");
            goto fail;  // exit which frees allocated resources
        }

        zeroinit(eip207_flow_addr);

        // physical address as upper and lower (eip207_flow_address_t)
        adapter_addr_to_word_pair(phys_addr.address_p, 0,
                               &eip207_flow_addr.addr,
                               &eip207_flow_addr.upper_addr);

        // fill in flue hashtable descriptor fields
        zeroinit(eip207_ht_params);

        // handle
        eip207_ht_params.ht_dma_handle    = hashtable_dma_handle;

        // translated addr
        eip207_ht_params.ht_dma_address_p = &eip207_flow_addr;

        // Convert numerical value to enum
        eip207_ht_params.ht_table_size     =
                            adapter_pcl_lib_hash_table_entries_num_to_size(
                                         ADAPTER_PCL_FLOW_HASH_ENTRIES_COUNT);
        eip207_ht_params.dt_p             = descr_area_ptr;

        // hash table plus overflow
        eip207_ht_params.dt_entry_count    = total_hasharea_element_count;
    }

    LOG_INFO("\n\t\t eip207_flow_hash_table_install \n");

    // install flue hashtable
    res =  eip207_flow_hash_table_install(ioarea_p,
                                         hashtable_id,
                                         &eip207_ht_params,
                                         ADAPTER_PCL_ENABLE_FLUE_CACHE,
                                         true);
    if (res != EIP207_FLOW_NO_ERROR)
    {
        LOG_CRIT("pcl_init: eip207_flow_hash_table_install failed\n");
        goto fail;  // exit which frees allocated resources
    }

    // Initialize the free list of record descriptors
    {
        unsigned int i;
        list_element_t * element_p;
        unsigned char * rec_dscr_p;
        unsigned int record_byte_count = MAX(eip207_flow_fr_dscr_byte_count_get(),
            eip207_flow_tr_dscr_byte_count_get());

        // Set the free list ID
        dev_p->free_list_id = ADAPTER_PCL_LIST_ID_OFFSET + interface_id;

        // Allocate a pool of list elements
        element_pool_p = adapter_alloc(sizeof(list_element_t) *
                                        total_hasharea_element_count);
        if (element_pool_p == NULL)
        {
            LOG_CRIT("pcl_init: free list allocation failed\n");
            goto fail;
        }

        if (list_init(dev_p->free_list_id, NULL) != LIST_STATUS_OK)
        {
            LOG_CRIT("pcl_init: free list initialization failed\n");
            goto fail;
        }

        // Allocate a pool of record descriptors
        rec_dscr_pool_p = adapter_alloc( record_byte_count *
                                            total_hasharea_element_count);
        if (rec_dscr_pool_p == NULL)
        {
            LOG_CRIT("pcl_init: record descriptor allocation failed\n");
            goto fail;
        }

        // Populate the free list with the elements (record descriptors)
        element_p = element_pool_p;
        element_p->data_object_p = rec_dscr_p = rec_dscr_pool_p;
        for(i = 0; i < total_hasharea_element_count; i++)
        {
            if (list_add_to_head(dev_p->free_list_id,
                               NULL,
                               element_p) == LIST_STATUS_OK)
            {
                if (i < total_hasharea_element_count - 1)
                {
                    element_p++;
                    rec_dscr_p += record_byte_count;
                    element_p->data_object_p = rec_dscr_p;
                }
            }
            else
            {
                LOG_CRIT("pcl_init: free list population failed\n");
                goto fail;
            }
        }

        dev_p->rec_dscr_pool_p = rec_dscr_pool_p;
        dev_p->element_pool_p = element_pool_p;
    }  // Record descriptors free list initialized

    // set remaining instance variables, on success
    dev_p->eip207_hashtable_dma_handle = hashtable_dma_handle;
    dev_p->pcl_is_initialized = true;

    (void)RPM_DEVICE_INIT_STOP_MACRO(ADAPTER_PCL_RPM_EIP207_DEVICE_ID + interface_id);

    adapter_lock_cs_leave(&adapter_pcl_cs);

    return PCL_STATUS_OK;

fail:
    (void)RPM_DEVICE_INIT_STOP_MACRO(ADAPTER_PCL_RPM_EIP207_DEVICE_ID + interface_id);

    // free list data structures
    adapter_free(element_pool_p);
    adapter_free(rec_dscr_pool_p);
    list_uninit(dev_p->free_list_id, NULL);

    // free memory areas and DMA-safe memory
    adapter_free(dev_p->eip207_io_area_p);
    adapter_free(dev_p->eip207_descriptor_area_p);
    if (hashtable_dma_handle != NULL)
        dma_resource_release(hashtable_dma_handle);

error_exit:
    adapter_lock_cs_leave(&adapter_pcl_cs);

    return PCL_ERROR;
}


/*----------------------------------------------------------------------------
 * pcl_uninit
 */
pcl_status_t
pcl_uninit(
        const unsigned int interface_id)
{
    adapter_pcl_device_instance_data_t * dev_p;

    LOG_INFO("\n\t pcl_uninit \n");

    if (interface_id >= ADAPTER_PCL_MAX_FLUE_DEVICES)
    {
        return PCL_INVALID_PARAMETER;
    }

    adapter_lock_cs_set(&adapter_pcl_cs, &AdapterPCL_Lock);

    if (!adapter_lock_cs_enter(&adapter_pcl_cs))
        return PCL_STATUS_BUSY;

    // select current device instance data, on validated interface_id
    dev_p = &dev_instance[interface_id];

    if (!dev_p->pcl_is_initialized)
    {
        LOG_CRIT("pcl_uninit: Not initialized\n");
        adapter_lock_cs_leave(&adapter_pcl_cs);
        return PCL_ERROR;
    }

    if (!adapter_lock_cs_enter(&dev_p->adapter_pcl_dev_cs))
    {
        adapter_lock_cs_leave(&dev_p->adapter_pcl_dev_cs);
        adapter_lock_cs_leave(&adapter_pcl_cs);
        return PCL_STATUS_BUSY;
    }

    if (RPM_DEVICE_UNINIT_START_MACRO(ADAPTER_PCL_RPM_EIP207_DEVICE_ID + interface_id,
                                      false) != RPM_SUCCESS)
    {
        adapter_lock_cs_leave(&dev_p->adapter_pcl_dev_cs);
        adapter_lock_cs_leave(&adapter_pcl_cs);
        return PCL_ERROR;
    }

    // free list data structures
    adapter_free(dev_p->element_pool_p);
    adapter_free(dev_p->rec_dscr_pool_p);
    list_uninit(dev_p->free_list_id, NULL);

    // pool list data structures
    adapter_free(dev_p->list_element_pool_p);
    adapter_free(dev_p->list_pool_p);
    if (dev_p->list_p)
    {
        list_uninit(LIST_DUMMY_LIST_ID, dev_p->list_p);
        dev_p->list_p = NULL;
    }

    // free memory areas and DMA-safe memory
    adapter_free(dev_p->eip207_io_area_p);
    adapter_free(dev_p->eip207_descriptor_area_p);
    dma_resource_release(dev_p->eip207_hashtable_dma_handle);

    // reset globals
    dev_p->eip207_io_area_p              = NULL;
    dev_p->eip207_hashtable_dma_handle  = NULL;
    dev_p->eip207_descriptor_area_p     = NULL;
    dev_p->rec_dscr_pool_p                = NULL;
    dev_p->element_pool_p                = NULL;

    dev_p->pcl_is_initialized = false;

    adapter_pcl_dma_alignment_byte_count = ADAPTER_DMABUF_ALIGNMENT_INVALID;

    adapter_lock_cs_leave(&dev_p->adapter_pcl_dev_cs);

    // Free device lock
    adapter_lock_free(adapter_lock_cs_get(&dev_p->adapter_pcl_dev_cs));
    adapter_lock_cs_set(&dev_p->adapter_pcl_dev_cs, adapter_lock_null);

    (void)RPM_DEVICE_UNINIT_STOP_MACRO(ADAPTER_PCL_RPM_EIP207_DEVICE_ID + interface_id);

    adapter_lock_cs_leave(&adapter_pcl_cs);

    return PCL_STATUS_OK;
}


/*-----------------------------------------------------------------------------
 * pcl_flow_dma_buf_handle_get
 */
pcl_status_t
pcl_flow_dma_buf_handle_get(
        const pcl_flow_handle_t flow_handle,
        dma_buf_handle_t * const dma_handle_p)
{
    dma_resource_handle_t dma_res_handle;
    eip207_flow_fr_dscr_t * flow_descriptor_p;
    list_element_t * element_p = (list_element_t*)flow_handle;

    LOG_INFO("\n\t pcl_flow_dma_buf_handle_get \n");

    if (dma_handle_p == NULL ||
        element_p == NULL)
    {
        LOG_CRIT("pcl_flow_dma_buf_handle_get: failed, invalid DMABuf handle or Flow handle\n");
        return PCL_INVALID_PARAMETER;
    }

    flow_descriptor_p = (eip207_flow_fr_dscr_t *)element_p->data_object_p;

    if (flow_descriptor_p == NULL)
    {
        LOG_CRIT("pcl_flow_dma_buf_handle_get: failed, invalid flow handle\n");
        return PCL_INVALID_PARAMETER;
    }

    dma_res_handle = flow_descriptor_p->dma_handle;

    *dma_handle_p = adapter_dma_resource_handle2_dma_buf_handle(dma_res_handle);

    return PCL_STATUS_OK;
}


/*----------------------------------------------------------------------------
 * pcl_flow_hash
 */
pcl_status_t
pcl_flow_hash(
        const pcl_selector_params_t * const selector_params,
        u32 * flow_id_word32_array_of4)
{
    eip207_flow_selector_params_t eip207_selector_params;
    eip207_flow_id_t flow_id;
    eip207_flow_error_t res;
    const unsigned int hashtable_id = 0;    // implementation limit

    LOG_INFO("\n\t pcl_flow_hash \n");

    if (selector_params == NULL || flow_id_word32_array_of4 == NULL)
        return PCL_ERROR;

    zeroinit(eip207_selector_params);
    zeroinit(flow_id);

    eip207_selector_params.flags   = selector_params->flags;
    eip207_selector_params.src_ip_p = selector_params->src_ip;
    eip207_selector_params.dst_ip_p = selector_params->dst_ip;
    eip207_selector_params.ip_proto = selector_params->ip_proto;
    eip207_selector_params.src_port = selector_params->src_port;
    eip207_selector_params.dst_port = selector_params->dst_port;
    eip207_selector_params.spi     = selector_params->spi;
    eip207_selector_params.epoch   = selector_params->epoch;
    eip207_selector_params.ether_type  = selector_params->ether_type;
    eip207_selector_params.an      = selector_params->an;

    if (RPM_DEVICE_IO_START_MACRO(ADAPTER_PCL_RPM_EIP207_DEVICE_ID,
                                  RPM_FLAG_SYNC) != RPM_SUCCESS)
        return PCL_ERROR;

    LOG_INFO("\n\t\t eip207_flow_id_compute \n");

    res = eip207_flow_id_compute(
            dev_instance[hashtable_id].eip207_io_area_p,
            hashtable_id,
            &eip207_selector_params,
            &flow_id);

    // Note: only one EIP-207 hash table is supported!
    (void)RPM_DEVICE_IO_STOP_MACRO(ADAPTER_PCL_RPM_EIP207_DEVICE_ID,
                                   RPM_FLAG_ASYNC);

    if (res != EIP207_FLOW_NO_ERROR)
    {
        LOG_CRIT("pcl_flow_hash: operation failed\n");
        return PCL_ERROR;
    }

    flow_id_word32_array_of4[0] = flow_id.word32[0];
    flow_id_word32_array_of4[1] = flow_id.word32[1];
    flow_id_word32_array_of4[2] = flow_id.word32[2];
    flow_id_word32_array_of4[3] = flow_id.word32[3];

    return PCL_STATUS_OK;
}


/*-----------------------------------------------------------------------------
 * pcl_flow_alloc
 */
pcl_status_t
pcl_flow_alloc(
        const unsigned int interface_id,
        const unsigned int flow_hash_table_id,
        pcl_flow_handle_t * const flow_handle_p)
{
    pcl_status_t pcl_rc = PCL_ERROR;
    dma_resource_handle_t dma_handle;
    dma_resource_properties_t dma_properties;
    dma_resource_addr_pair_t host_addr;
    dma_resource_addr_pair_t phys_addr;
    int dmares;
    eip207_flow_fr_dscr_t * flow_descriptor_p;
    unsigned int fr_word_count;
    list_element_t * element_p;
    adapter_pcl_device_instance_data_t * dev_p;

    LOG_INFO("\n\t pcl_flow_alloc \n");

    IDENTIFIER_NOT_USED(flow_hash_table_id);

    // validate interface id
    if (interface_id >= ADAPTER_PCL_MAX_FLUE_DEVICES)
        return PCL_INVALID_PARAMETER;

    dev_p = &dev_instance[interface_id];

    if (adapter_lock_cs_get(&dev_p->adapter_pcl_dev_cs) == adapter_lock_null)
    {
        LOG_CRIT("pcl_flow_alloc: no device lock, not initialized?\n");
        return PCL_ERROR;
    }

    if (!adapter_lock_cs_enter(&dev_p->adapter_pcl_dev_cs))
        return PCL_STATUS_BUSY;

    // select current device instance data, on validated interface_id
    if (!dev_p->pcl_is_initialized)
    {
        LOG_CRIT("pcl_flow_alloc: Not initialized\n");
        goto error_exit;
    }

    *flow_handle_p = NULL;

    LOG_INFO("\n\t\t eip207_flow_fr_word_count_get \n");

    // Get the required size of the DMA buffer for the flow record.
    fr_word_count = eip207_flow_fr_word_count_get();

    // Allocate a buffer for the flow record (DMA).
    dma_properties.alignment = adapter_pcl_dma_alignment_byte_count;
    dma_properties.bank      = ADAPTER_PCL_BANK_FLOW;
    dma_properties.f_cached   = false;
    dma_properties.size      = 4 * fr_word_count;

    dmares = dma_resource_alloc(dma_properties, &host_addr, &dma_handle);
    if (dmares != 0)
    {
        LOG_CRIT("pcl_flow_alloc: could not allocate buffer\n");
        goto error_exit;
    }

#ifdef ADAPTER_PCL_ENABLE_SWAP
    dma_resource_swap_endianness_set(dma_handle, true);
#endif

    // Get a record descriptor from the free list
    {
        list_status_t list_rc =
                list_remove_from_tail(dev_p->free_list_id,
                                    NULL,
                                    &element_p);
        if (list_rc != LIST_STATUS_OK)
        {
            LOG_CRIT("pcl_flow_alloc: failed to allocate record\n");
            goto error_exit;
        }

        // Get the flow record descriptor place-holder from the list element
        flow_descriptor_p = (eip207_flow_fr_dscr_t*)element_p->data_object_p;
        if (flow_descriptor_p == NULL)
        {
            LOG_CRIT("pcl_flow_alloc: failed to get record descriptor\n");
            goto error_exit;
        }
    }

    // Fill in the descriptor.
    flow_descriptor_p->dma_handle = dma_handle;

    if (dma_resource_translate(dma_handle, DMARES_DOMAIN_BUS, &phys_addr) < 0)
    {
        LOG_CRIT("PCL_FlowAlloc: Failed to obtain physical address.\n");
        dma_resource_release(dma_handle);
        goto error_exit;
    }

    adapter_addr_to_word_pair(phys_addr.address_p, 0,
                           &flow_descriptor_p->dma_addr.addr,
                           &flow_descriptor_p->dma_addr.upper_addr);

    *flow_handle_p = (pcl_flow_handle_t)element_p;

    pcl_rc = PCL_STATUS_OK;

error_exit:
    adapter_lock_cs_leave(&dev_p->adapter_pcl_dev_cs);

    return pcl_rc;
}


/*-----------------------------------------------------------------------------
 * pcl_flow_add
 */
pcl_status_t
pcl_flow_add(
        const unsigned int interface_id,
        const unsigned int flow_hash_table_id,
        const pcl_flow_params_t * const flow_params,
        const pcl_flow_handle_t flow_handle)
{
    pcl_status_t pcl_rc = PCL_ERROR;
    dma_resource_handle_t dma_handle;
    dma_resource_addr_pair_t phys_addr;
    eip207_flow_error_t res;
    eip207_flow_fr_input_data_t flow_data;
    eip207_flow_fr_dscr_t * flow_descriptor_p;
    list_element_t * element_p = (list_element_t*)flow_handle;
    adapter_pcl_device_instance_data_t * dev_p;

    LOG_INFO("\n\t pcl_flow_add \n");

    // validate interface id
    if (interface_id >= ADAPTER_PCL_MAX_FLUE_DEVICES)
        return PCL_INVALID_PARAMETER;

    // check valid flow handle
    if (element_p == NULL)
        return PCL_INVALID_PARAMETER;

    flow_descriptor_p = (eip207_flow_fr_dscr_t *)element_p->data_object_p;

    // check valid flow record descriptor
    if (flow_descriptor_p == NULL)
        return PCL_INVALID_PARAMETER;

    dev_p = &dev_instance[interface_id];

    if (adapter_lock_cs_get(&dev_p->adapter_pcl_dev_cs) == adapter_lock_null)
    {
        LOG_CRIT("pcl_flow_add: no device lock, not initialized?\n");
        return PCL_ERROR;
    }

    if (!adapter_lock_cs_enter(&dev_p->adapter_pcl_dev_cs))
        return PCL_STATUS_BUSY;

    // check interface is initialised
    if (!dev_p->pcl_is_initialized)
    {
        LOG_CRIT("pcl_flow_add: Not initialized\n");
        goto error_exit;
    }

    // Fill in the input data
    dma_handle = adapter_dma_buf_handle2_dma_resource_handle(flow_params->transform);
    if (dma_handle)
    {
        if (dma_resource_translate(dma_handle, DMARES_DOMAIN_BUS, &phys_addr) < 0)
        {
            LOG_CRIT("pcl_flow_add: Failed to obtain physical address.\n");
            goto error_exit;
        }
    }
    else
    {
        phys_addr.domain    = DMARES_DOMAIN_BUS;
        phys_addr.address_p = adapter_pcl_int_to_ptr(
                                    eip207_flow_record_dummy_addr_get());
    }

    flow_data.flags               = flow_params->flags;
    flow_data.hash_id.word32[0]    = flow_params->flow_id[0];
    flow_data.hash_id.word32[1]    = flow_params->flow_id[1];
    flow_data.hash_id.word32[2]    = flow_params->flow_id[2];
    flow_data.hash_id.word32[3]    = flow_params->flow_id[3];
    flow_data.sw_fr_reference     = flow_params->flow_index;
    adapter_addr_to_word_pair(phys_addr.address_p,0,
                           &flow_data.xform_dma_addr.addr,
                           &flow_data.xform_dma_addr.upper_addr);

    flow_data.f_large = false;

#ifndef ADAPTER_PCL_USE_LARGE_TRANSFORM_DISABLE
    /* Determine whether we have a large transform record. */
    if (dma_handle)
    {
        dma_resource_record_t * rec_p = dma_resource_handle2_record_ptr(dma_handle);

        if (rec_p->f_is_large_transform)
           flow_data.f_large = true;
    }
#endif // !ADAPTER_PCL_USE_LARGE_TRANSFORM_DISABLE

    LOG_INFO("\n\t\t eip207_flow_fr_add \n");

    // Add the record
    res = eip207_flow_fr_add(dev_p->eip207_io_area_p,
                             flow_hash_table_id,
                             flow_descriptor_p,
                             &flow_data);
    if (res == EIP207_FLOW_OUT_OF_MEMORY_ERROR)
    {
        LOG_CRIT("PCL_FLow_Add: failed to install flow, out of memory\n");
    }
    else if (res == EIP207_FLOW_NO_ERROR)
    {
       pcl_rc = PCL_STATUS_OK;
    }
    else
    {
        LOG_CRIT("PCL_FLow_Add: failed to install flow, internal error\n");
    }

error_exit:

    adapter_lock_cs_leave(&dev_p->adapter_pcl_dev_cs);
    return pcl_rc;
}


/*-----------------------------------------------------------------------------
 * pcl_flow_release
 */
pcl_status_t
pcl_flow_release(
        const unsigned int interface_id,
        const unsigned int flow_hash_table_id,
        const pcl_flow_handle_t flow_handle)
{
    pcl_status_t pcl_rc = PCL_ERROR;
    eip207_flow_fr_dscr_t * flow_descriptor_p;
    list_element_t * element_p = (list_element_t*)flow_handle;
    adapter_pcl_device_instance_data_t * dev_p;

    LOG_INFO("\n\t pcl_flow_release \n");

    IDENTIFIER_NOT_USED(interface_id);
    IDENTIFIER_NOT_USED(flow_hash_table_id);
    IDENTIFIER_NOT_USED(flow_handle);

    // check valid flow handle
    if (element_p == NULL)
        return PCL_INVALID_PARAMETER;

    flow_descriptor_p = (eip207_flow_fr_dscr_t *)element_p->data_object_p;

    // check valid flow record descriptor
    if (flow_descriptor_p == NULL)
        return PCL_INVALID_PARAMETER;

    dev_p = &dev_instance[interface_id];

    if (adapter_lock_cs_get(&dev_p->adapter_pcl_dev_cs) == adapter_lock_null)
    {
        LOG_CRIT("pcl_flow_release: no device lock, not initialized?\n");
        return PCL_ERROR;
    }

    if (!adapter_lock_cs_enter(&dev_p->adapter_pcl_dev_cs))
        return PCL_STATUS_BUSY;

    // check interface is initialised
    if (!dev_p->pcl_is_initialized)
    {
        LOG_CRIT("pcl_flow_release: Not initialized\n");
        goto error_exit;
    }

    dma_resource_release(flow_descriptor_p->dma_handle);

    // Put the record descriptor back on the free list
    {
        list_status_t list_rc;

        list_rc = list_add_to_head(dev_p->free_list_id,
                                 NULL,
                                 element_p);
        if (list_rc != LIST_STATUS_OK)
        {
            LOG_CRIT("pcl_flow_release: "
                     "failed to put descriptor on the free list\n");
            goto error_exit;
        }
    }

    pcl_rc = PCL_STATUS_OK;

error_exit:
    adapter_lock_cs_leave(&dev_p->adapter_pcl_dev_cs);

    return pcl_rc;
}


/*-----------------------------------------------------------------------------
 * pcl_flow_get_read_only
 */
pcl_status_t
pcl_flow_get_read_only(
        const pcl_flow_handle_t flow_handle,
        pcl_flow_params_t * const flow_params_p)
{
    eip207_flow_fr_dscr_t * flow_descriptor_p;
    eip207_flow_fr_output_data_t flow_data;
    eip207_flow_error_t res;
    list_element_t * element_p = (list_element_t*)flow_handle;

    LOG_INFO("\n\t pcl_flow_get_read_only \n");

    // check valid flow handle
    if (element_p == NULL)
        return PCL_INVALID_PARAMETER;

    flow_descriptor_p = (eip207_flow_fr_dscr_t *)element_p->data_object_p;

    // check valid flow record descriptor
    if (flow_descriptor_p == NULL)
        return PCL_INVALID_PARAMETER;

    LOG_INFO("\n\t\t eip207_flow_fr_read \n");

    res = eip207_flow_fr_read(dev_instance[0].eip207_io_area_p,
                             0,
                             flow_descriptor_p,
                             &flow_data);
    if (res == EIP207_FLOW_ARGUMENT_ERROR)
    {
        return PCL_INVALID_PARAMETER;
    }
    else if (res != EIP207_FLOW_NO_ERROR)
    {
        return PCL_ERROR;
    }

    flow_params_p->last_time_lo = flow_data.last_time_lo;
    flow_params_p->last_time_hi = flow_data.last_time_hi;

    flow_params_p->packets_counter_lo = flow_data.packets_counter;
    flow_params_p->packets_counter_hi = 0;

    flow_params_p->octets_counter_lo = flow_data.octets_counter_lo;
    flow_params_p->octets_counter_hi = flow_data.octets_counter_hi;

    return PCL_STATUS_OK;
}

/* end of file adapter_pcl_generic.c */
