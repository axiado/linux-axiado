/* dmares_gen.c
 *
 * This Module implements the Generic DMA Resource API
 */

/*****************************************************************************
* Copyright (c) 2012-2024 by Rambus, Inc. and/or its subsidiaries.
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

#include "dmares_mgmt.h"
#include "dmares_buf.h"
#include "dmares_addr.h"
#include "dmares_rw.h"

// helper functions, not part of the actual DMAResource API
#include "dmares_gen.h"

/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Default configuration
#include "c_dmares_gen.h"

#include "device_swap.h"        // device_swap_endian32

// Driver Framework Basic Definitions API
#include "basic_defs.h"         // u32, NULL, inline, bool,
                                // IDENTIFIER_NOT_USED

// Driver Framework C Run-Time Library Abstraction API
#include "clib.h"               // zeroinit()

// Logging API
#include "log.h"                // LOG_*

#ifdef HWPAL_DMARESOURCE_BANKS_ENABLE
// list API
#include "list.h"
#endif // HWPAL_DMARESOURCE_BANKS_ENABLE

// HW- and OS-specific abstraction API
#include "dmares_hwpal.h"


/*----------------------------------------------------------------------------
 * Definitions and macros
 */
#define DMARES_MAX_SIZE     (1 * 1024 * 1024) // 1 MB

#ifdef HWPAL_DMARESOURCE_BANKS_ENABLE
/*
 * Static (fixed-size) DMA banks properties implemented here:
 *
 * - One static bank contains one DMA pool;
 *
 * - Static bank types:
 *     1) HWPAL_DMARESOURCE_BANK_STATIC - dynamically allocated
 *     2) HWPAL_DMARESOURCE_BANK_STATIC_FIXED_ADDR - allocated on a fixed
 *        address configured in c_dmares_gen.h
 *
 * - One DMA Pool contains a fixed compile-time configurable number of blocks;
 *
 * - All blocks in one DMA pool have the same fixed compile-time configurable
 *   size;
 *
 * - The DMA pools for all the configured static banks are allocated
 *   in dma_resource_init() and freed in DMAResource_Uninit();
 *
 * - DMA resources can be allocated in a static bank using dma_resource_alloc()
 *   and they must be freed using dma_resource_release();
 *
 * - Only sub-sets of DMA resources allocated in a static bank can be
 *   registered in that bank using dma_resource_check_and_register();
 *   If the dma_resource_check_and_register() function is called for a static bank
 *   then it must use allocator type 82 ('R') and the required memory block
 *   must belong to an already allocated DMA resource in that bank;
 *
 * - The dma_resource_check_and_register() function can be called for a static
 *   bank also using allocator type 78 ('N') to register a DMA-unsafe buffer;
 *   These DMA resources must be subsequently freed using the dma_buf_release()
 *   function;
 *
 * - An "all-pool" DMA resource of size (nr_of_blocks * block_size) can be
 *   allocated in a static bank using dma_resource_alloc() where nr_of_blocks
 *   and block_size are compile-time configuration parameters
 *   (see HWPAL_DMARESOURCE_BANKS in c_dmares_gen.h);
 *   The dma_resource_check_and_register() function can be used to register
 *   sub-sets of this DMA resource; Only one such a all-pool DMA resource
 *   can be allocated in one static bank and must be freed using
 *   dma_resource_release() function;
 *
 * - No other DMA resources can be allocated in a static bank where an all-pool
 *   DMA resource is allocated.
 */
typedef struct
{
    // bank ID corresponding to DMA pool
    u8 bank;

    // bank type, see HWPAL_DMARESOURCE_BANK_*
    unsigned int bank_type;

    // True if bank must be shared between multiple concurrent applications
    bool f_apps_shared;

    // True if bank must be allocated in cached memory.
    // Note: implementation may still chose to allocate it
    //       in the non-cached memory.
    bool f_cached;

    // For static HWPAL_DMARESOURCE_BANK_STATIC_FIXED_ADDR bank types only,
    // otherwise ignored.
    // Note: by default physical addresses are used.
    void * addr; // bank address

    // list ID for elements associated with free blocks (DMA resources)
    // in DMA pool
    unsigned int pool_list_id;

    // list ID for used dangling elements which are not associated with
    // free blocks in DMA pool
    unsigned int dangling_list_id;

    // Pointer to array of elements which can be present either
    // in the Pool list or in the Dangling list
    // (but not in both at the same time)
    list_element_t * list_elements;

    // number of blocks in DMA pool
    unsigned int block_count;

    // Block size (in bytes)
    unsigned int block_byte_count;

    // DMA pool handle
    dma_resource_handle_t pool_handle;

    // Pointer to a list lock for concurrent access protection,
    // used for both lists
    void * lock_p;

    // When true no more DMA resource can be allocated in the bank's DMA Pool
    bool f_dma_pool_locked;

} hwpal_dma_resource_bank_t;

#define HWPAL_DMARESOURCE_BANK_ADD(_bank, _type, _shared, _cached, _addr, _blocks, _bsize) \
               {_bank, _type, _shared, _cached, (void*)(_addr), 0, 0, NULL, _blocks, _bsize, NULL, NULL, false}

#endif // HWPAL_DMARESOURCE_BANKS_ENABLE


/*----------------------------------------------------------------------------
 * Local variables
 */

#ifdef HWPAL_DMARESOURCE_BANKS_ENABLE
static hwpal_dma_resource_bank_t hwpal_dma_banks[] =
{
    HWPAL_DMARESOURCE_BANKS
};

// number of DMA pools supported calculated on hwpal_dma_banks defined
// in c_dmares_lkm.h
#define HWPAL_DMARESOURCE_BANK_COUNT \
        (sizeof(hwpal_dma_banks) / sizeof(hwpal_dma_resource_bank_t))
#endif // HWPAL_DMARESOURCE_BANKS_ENABLE

// except when x is zero,
// (x & (-x)) returns a value where all bits of `x' have
// been cleared except the right-most '1'
#define IS_POWER_OF_TWO(_x) (((_x) & (0 - (_x))) == (_x))


/*----------------------------------------------------------------------------
 * Forward declarations
 */

static inline void write32_volatile(u32 b, volatile void *addr)
{
    *(volatile u32 *) addr = b;
}

static inline u32 read32_volatile(const volatile void *addr)
{
    return *(const volatile u32 *) addr;
}


/*----------------------------------------------------------------------------
 * dma_resource_lib_is_sane_input
 *
 * Return true if the DMAResource defined by the given address pair
 * and properties appears to be valid.
 */
/* static */ bool
dma_resource_lib_is_sane_input(
        const dma_resource_addr_pair_t * addr_pair_p,
        const char * const allocator_ref_p,
        const dma_resource_properties_t * props_p)
{
    unsigned int alignment = (unsigned int)props_p->alignment;

    if ((alignment < 1) ||
        (alignment > hwpal_dma_resource_max_alignment_get()) ||
        !IS_POWER_OF_TWO(alignment))
    {
        LOG_CRIT(
            "dma_resource_lib_is_sane_input: "
            "Bad alignment value: %d\n",
            alignment);
        return false;
    }

    // we support up to 1 megabyte buffers
    if(props_p->size == 0 ||
       props_p->size >= DMARES_MAX_SIZE)
    {
        LOG_CRIT(
            "dma_resource_lib_is_sane_input: "
            "Bad size value: %d\n",
            props_p->size);
        return false;
    }

    if (addr_pair_p != NULL)
    {
        uintptr_t address = (unsigned int)(uintptr_t)addr_pair_p->address_p;

        // Reject NULL as address
        if (address == 0)
        {
            LOG_CRIT(
                "dma_resource_lib_is_sane_input: "
                "Bad address: %p\n",
                addr_pair_p->address_p);
            return false;
        }

        // If requested verify if address is consistent with alignment
        // Skip address alignment check for non-DMA safe buffers
        if (allocator_ref_p != NULL && *allocator_ref_p != 'N')
        {
            if ((address & (alignment-1)) != 0)
            {
                LOG_CRIT(
                    "dma_resource_lib_is_sane_input: "
                    "address (%p) alignment (0x%x bytes) check failed\n",
                    addr_pair_p->address_p,
                    alignment);
                return false;
            }
        }
    }

    return true;
}


/*----------------------------------------------------------------------------
 * dma_resource_lib_align_for_size
 */
/* static */ unsigned int
dma_resource_lib_align_for_size(
        const unsigned int byte_count,
        const unsigned int align_to)
{
    unsigned int aligned_byte_count = byte_count;

    // Check if alignment and padding for length alignment is required
    if (align_to > 1 && byte_count % align_to)
        aligned_byte_count = byte_count / align_to * align_to + align_to;

    return aligned_byte_count;
}


/*----------------------------------------------------------------------------
 * dma_resource_lib_align_for_address
 */
/* static */ unsigned int
dma_resource_lib_align_for_address(
        const unsigned int byte_count,
        const unsigned int align_to)
{
    unsigned int aligned_byte_count = byte_count;

    // Check if alignment is required
    if(align_to > 1)
    {
        // Speculative padding for address alignment
        aligned_byte_count += align_to;
    }

    return aligned_byte_count;
}


/*----------------------------------------------------------------------------
 * dma_resource_lib_lookup_domain
 *
 * Lookup given domain in rec_p->addr_pairs array.
 */
/* static */ dma_resource_addr_pair_t *
dma_resource_lib_lookup_domain(
        const dma_resource_record_t * rec_p,
        const dma_resource_addr_domain_t domain)
{
    const dma_resource_addr_pair_t * res = rec_p->addr_pairs;

    while (res->domain != domain)
    {
        if (res->domain == 0)
        {
            return NULL;
        }

        if (++res == rec_p->addr_pairs + DMARES_ADDRPAIRS_CAPACITY)
        {
            return NULL;
        }
    }

    {
        // Return 'res' but drop the 'const' qualifier
        dma_resource_addr_pair_t * rv = (dma_resource_addr_pair_t *)((uintptr_t)res);
        return rv;
    }
}


#ifdef HWPAL_DMARESOURCE_BANKS_ENABLE
/*----------------------------------------------------------------------------
 * dma_resource_lib_dma_pool_bank_get
 */
static hwpal_dma_resource_bank_t *
dma_resource_lib_dma_pool_bank_get(
        const u8 bank)
{
    unsigned int i, bank_count = HWPAL_DMARESOURCE_BANK_COUNT;

    // we only support pre-configured memory banks
    for(i = 0; i < bank_count; i++)
    {
        if (hwpal_dma_banks[i].bank == bank)
            return &hwpal_dma_banks[i];
    }

    return NULL;
}


/*----------------------------------------------------------------------------
 * dma_resource_lib_dma_pool_lock
 *
 * Checks if no memory blocks have already been obtained from
 * the DMA pool of the provided bank.
 *
 * Returns true and locks the bank when no dangling blocks are found.
 * Returns false if the pool is locked or if dangling blocks are found.
 */
static inline bool
dma_resource_lib_dma_pool_lock(
        hwpal_dma_resource_bank_t * const bank_p)
{
    list_status_t list_rc;
    unsigned long flags = 0;
    unsigned int element_count = 1;

    // DMA pool lock flag access protected by the bank lock
    hwpal_dma_resource_lock_acquire(bank_p->lock_p, &flags);

    if (bank_p->f_dma_pool_locked == false)
    {
        list_rc = list_get_list_element_count(bank_p->dangling_list_id,
                                           NULL,
                                           &element_count);
        if (list_rc != LIST_STATUS_OK)
        {
            hwpal_dma_resource_lock_release(bank_p->lock_p, &flags);

            LOG_CRIT("DMAResourceLib_DMAPool_IsAllocated: failed for list %d\n",
                     bank_p->dangling_list_id);

            return false;
        }

        if (element_count == 0)
            bank_p->f_dma_pool_locked = true;
    }

    hwpal_dma_resource_lock_release(bank_p->lock_p, &flags);

    if (element_count == 0)
        return true;
    else
        return false;
}


/*----------------------------------------------------------------------------
 * dma_resource_lib_dma_pool_unlock
 *
 * Unlocks the DMA pool of the provided bank so that blocks
 * can be obtained from it using dma_resource_lib_dma_pool_get().
 */
static bool
dma_resource_lib_dma_pool_unlock(
        hwpal_dma_resource_bank_t * const bank_p)
{
    bool f_success = false;
    unsigned long flags = 0;

    // DMA pool lock flag access protected by the bank lock
    hwpal_dma_resource_lock_acquire(bank_p->lock_p, &flags);

    if (bank_p->f_dma_pool_locked)
    {
        bank_p->f_dma_pool_locked = false;
        f_success = true;
    }

    hwpal_dma_resource_lock_release(bank_p->lock_p, &flags);

    return f_success;
}


/*----------------------------------------------------------------------------
 * dma_resource_lib_dma_pool_free
 */
static void
dma_resource_lib_dma_pool_free(void)
{
    unsigned int i, bank_count = HWPAL_DMARESOURCE_BANK_COUNT;
    bool f_freed = false;

    LOG_INFO("%s: freeing %d DMA banks\n", __func__, bank_count);

    // Free allocated DMA pools
    for(i = 0; i < bank_count; i++)
    {
        if (hwpal_dma_banks[i].bank_type == HWPAL_DMARESOURCE_BANK_DYNAMIC)
        {
            LOG_INFO("%s: skipping dynamic bank %d\n",
                     __func__,
                     hwpal_dma_banks[i].bank);
            continue;
        }

        if (hwpal_dma_banks[i].pool_handle != NULL)
        {
            hwpal_dma_resource_release(hwpal_dma_banks[i].pool_handle);

            hwpal_dma_banks[i].pool_handle = NULL;
            f_freed = true;
        }

        if (hwpal_dma_banks[i].list_elements != NULL)
        {
            hwpal_dma_resource_mem_free(hwpal_dma_banks[i].list_elements);
            hwpal_dma_banks[i].list_elements = NULL;
            f_freed = true;
        }

        if (hwpal_dma_banks[i].lock_p != NULL)
        {
            hwpal_dma_resource_lock_free(hwpal_dma_banks[i].lock_p);
            hwpal_dma_banks[i].lock_p = NULL;
            f_freed = true;
        }

        if (f_freed)
        {
            LOG_INFO("%s: %d freed static bank %d at offset 0x%p (cached=%d), "
                    "for %d blocks, %d bytes each)\n",
                    __func__,
                    i,
                    hwpal_dma_banks[i].bank,
                    hwpal_dma_banks[i].addr,
                    hwpal_dma_banks[i].f_cached,
                    hwpal_dma_banks[i].block_count,
                    hwpal_dma_banks[i].block_byte_count);;
        }

        list_uninit(hwpal_dma_banks[i].pool_list_id, NULL);
        list_uninit(hwpal_dma_banks[i].dangling_list_id, NULL);
    } // for

}


/*----------------------------------------------------------------------------
 * dma_resource_lib_dma_pool_alloc
 *
 * Allocate DMA pools for static banks
 *
 * Return Values
 *     0    Success
 *     <0   error code (implementation specific)
 */
static int
dma_resource_lib_dma_pool_alloc(void)
{
    unsigned int i, list_id, bank_count;
    dma_resource_properties_t pool_properties;
    hwpal_dma_resource_properties_ext_t pool_properties_ext;

    zeroinit(pool_properties);
    zeroinit(pool_properties_ext);

    list_id = 0;

    pool_properties.alignment  = HWPAL_DMARESOURCE_DMA_ALIGNMENT_BYTE_COUNT;
    pool_properties.f_cached    = true;

    bank_count = HWPAL_DMARESOURCE_BANK_COUNT;

    if (bank_count > HWPAL_DMARESOURCE_MAX_NOF_BANKS)
    {
       LOG_CRIT("%s: failed, maximum %d DMA banks supported\n",
                __func__,
                HWPAL_DMARESOURCE_MAX_NOF_BANKS);
       return -1;
    }

    LOG_INFO("%s: allocating %d DMA banks\n", __func__, bank_count);

    for(i = 0; i < bank_count; i++)
    {
        unsigned int j, aligned_block_byte_count;
        list_element_t * p;
        u8 * byte_p;

        // Only static banks require DMA pool allocation
        if (hwpal_dma_banks[i].bank_type == HWPAL_DMARESOURCE_BANK_DYNAMIC)
        {
            LOG_INFO("%s: skipping dynamic bank %d\n",
                     __func__,
                     hwpal_dma_banks[i].bank);
            continue;
        }

        // Only non-cached SFA DMA banks are supported at the moment
        if (hwpal_dma_banks[i].bank_type ==
                HWPAL_DMARESOURCE_BANK_STATIC_FIXED_ADDR &&
            hwpal_dma_banks[i].f_cached)
        {
            LOG_CRIT("%s: failed for bank %d, "
                     "cached SFA DMA banks not supported\n",
                     __func__,
                     hwpal_dma_banks[i].bank);
            goto fail;
        }

        aligned_block_byte_count = dma_resource_lib_align_for_size(
                                hwpal_dma_banks[i].block_byte_count,
                                (unsigned int)pool_properties.alignment);
                               /*hwpal_dma_resource_d_cache_alignment_get()*/

        pool_properties.bank      = hwpal_dma_banks[i].bank;
        pool_properties.size      = aligned_block_byte_count *
                                               hwpal_dma_banks[i].block_count;
        pool_properties.f_cached   = hwpal_dma_banks[i].f_cached;

        // Will be used only for hwpal_dma_banks[i].bank_type ==
        // HWPAL_DMARESOURCE_BANK_STATIC_FIXED_ADDR
        pool_properties_ext.addr      = hwpal_dma_banks[i].addr;
        pool_properties_ext.bank_type  = hwpal_dma_banks[i].bank_type;

        // Allocate DMA pool
        {
            dma_resource_addr_pair_t pool_addr_pair;

            zeroinit(pool_addr_pair);

            if (hwpal_dma_resource_alloc(pool_properties,
                                        pool_properties_ext,
                                        &pool_addr_pair,
                                        &hwpal_dma_banks[i].pool_handle) != 0)
                goto fail;

            byte_p = (u8*)pool_addr_pair.address_p;
        }

        // Allocate an array of list elements
        p = hwpal_dma_resource_mem_alloc(sizeof(list_element_t) *
                                           hwpal_dma_banks[i].block_count);
        if (p == NULL)
            goto fail;

        hwpal_dma_banks[i].f_dma_pool_locked = false;
        hwpal_dma_banks[i].list_elements   = p;
        hwpal_dma_banks[i].pool_list_id     = list_id++;
        hwpal_dma_banks[i].dangling_list_id = list_id++;

        if (list_init(hwpal_dma_banks[i].pool_list_id, NULL) != LIST_STATUS_OK ||
            list_init(hwpal_dma_banks[i].dangling_list_id, NULL) != LIST_STATUS_OK)
            goto fail;

        for(j = 0; j < hwpal_dma_banks[i].block_count; j++)
        {
            p->data_object_p = byte_p + j * aligned_block_byte_count;
            if (list_add_to_head(hwpal_dma_banks[i].pool_list_id, NULL, p++) !=
                    LIST_STATUS_OK)
                goto fail;
        }

        // Allocate a pool lock
        hwpal_dma_banks[i].lock_p = hwpal_dma_resource_lock_alloc();
        if (hwpal_dma_banks[i].lock_p == NULL)
            goto fail;

        LOG_INFO("%s: %d allocated static bank %d at offset 0x%p (cached=%d), "
                 "for %d blocks, %d (aligned %d) bytes each), handle 0x%p\n",
                 __func__,
                 i,
                 hwpal_dma_banks[i].bank,
                 hwpal_dma_banks[i].addr,
                 hwpal_dma_banks[i].f_cached,
                 hwpal_dma_banks[i].block_count,
                 hwpal_dma_banks[i].block_byte_count,
                 aligned_block_byte_count,
                 hwpal_dma_banks[i].pool_handle);
    } // for

    return 0;

fail:
    dma_resource_lib_dma_pool_free();

    return -1;
} // DMA pools allocation for static banks done


/*----------------------------------------------------------------------------
 * dma_resource_lib_dma_pool_put
 *
 * Put a DMA resource host address to a DMA pool

 * Return Values
 *     0    Success
 *     <0   error code (implementation specific)
 */
static int
dma_resource_lib_dma_pool_put(
        hwpal_dma_resource_bank_t * bank_p,
        void * const addr_p)
{
    list_status_t list_rc1, list_rc2;
    list_element_t * le_p;
    unsigned long flags = 0;

    // Dangling list access protected by lock
    hwpal_dma_resource_lock_acquire(bank_p->lock_p, &flags);

    list_rc1 = list_remove_from_tail(bank_p->dangling_list_id, NULL, &le_p);

    le_p->data_object_p = addr_p;

    list_rc2 = list_add_to_head(bank_p->pool_list_id, NULL, le_p);

    hwpal_dma_resource_lock_release(bank_p->lock_p, &flags);

    if (list_rc1 != LIST_STATUS_OK || list_rc2 != LIST_STATUS_OK)
    {
        LOG_CRIT("dma_resource_lib_dma_pool_put: failed\n");
        return -1;
    }

    return 0; // success
}


/*----------------------------------------------------------------------------
 * dma_resource_lib_dma_pool_get
 *
 * Get a DMA resource host address from a DMA pool
 *
 * Return Values
 *     0    Success
 *     <0   error code (implementation specific)
 */
static int
dma_resource_lib_dma_pool_get(
        hwpal_dma_resource_bank_t * bank_p,
        void ** const addr_pp)
{
    list_status_t list_rc1, list_rc2;
    list_element_t * le_p;
    unsigned long flags = 0;
    void *address;

    // Pool list access protected by lock
    hwpal_dma_resource_lock_acquire(bank_p->lock_p, &flags);

    if (bank_p->f_dma_pool_locked)
    {
        hwpal_dma_resource_lock_release(bank_p->lock_p, &flags);

        LOG_CRIT("dma_resource_lib_dma_pool_get: failed, DMA pool is locked\n");
        return -1;
    }

    list_rc1 = list_remove_from_tail(bank_p->pool_list_id, NULL, &le_p);
    if (list_rc1 != LIST_STATUS_OK)
    {
        hwpal_dma_resource_lock_release(bank_p->lock_p, &flags);

        LOG_CRIT("dma_resource_lib_dma_pool_get: failed, out of list elements\n");
        return -1;
    }

    address = le_p->data_object_p;
    list_rc2 = list_add_to_head(bank_p->dangling_list_id, NULL, le_p);

    hwpal_dma_resource_lock_release(bank_p->lock_p, &flags);

    if (list_rc2 != LIST_STATUS_OK)
    {
        LOG_CRIT("dma_resource_lib_dma_pool_get: "
                 "failed to add to list of elements\n");
        return -1;
    }

    *addr_pp = address;

    return 0;
}
#endif // HWPAL_DMARESOURCE_BANKS_ENABLE


/*----------------------------------------------------------------------------
 * dma_resource_init
 */
bool
dma_resource_init(void)
{
    if (hwpal_dma_resource_init() == false)
        return false;

#ifdef HWPAL_DMARESOURCE_BANKS_ENABLE
    if (dma_resource_lib_dma_pool_alloc() == 0)
        return true;
    else
    {
        LOG_CRIT("%s: DMA pool allocation failed\n", __func__);
        return false;
    }
#else
    return true;
#endif // HWPAL_DMARESOURCE_BANKS_ENABLE
}


/*----------------------------------------------------------------------------
 * dma_resource_uninit
 *
 * This function can be used to uninitialize the DMAResource administration.
 * The caller must make sure that handles will not be used after this function
 * returns.
 * If memory was allocated by dma_resource_init, this function will free it.
 */
void
dma_resource_uninit(void)
{
#ifdef HWPAL_DMARESOURCE_BANKS_ENABLE
    dma_resource_lib_dma_pool_free();
#endif // HWPAL_DMARESOURCE_BANKS_ENABLE

    hwpal_dma_resource_uninit();
}


/*----------------------------------------------------------------------------
 * dma_resource_alloc
 */
int
dma_resource_alloc(
        const dma_resource_properties_t requested_properties,
        dma_resource_addr_pair_t * const addr_pair_p,
        dma_resource_handle_t * const handle_p)
{
    hwpal_dma_resource_properties_ext_t pool_properties_ext;

#ifdef HWPAL_DMARESOURCE_BANKS_ENABLE
    hwpal_dma_resource_bank_t * bank_p;
#endif

    zeroinit(pool_properties_ext);

#ifdef HWPAL_DMARESOURCE_BANKS_ENABLE
    // Find the bank
    bank_p = dma_resource_lib_dma_pool_bank_get(requested_properties.bank);
    if (bank_p == NULL)
    {
        LOG_CRIT("dma_resource_alloc: failed for unsupported bank %d\n",
                 (int)requested_properties.bank);
        return -1; // bank not supported
    }

    if (bank_p->bank_type == HWPAL_DMARESOURCE_BANK_DYNAMIC)
        // handle non-static banks
        return hwpal_dma_resource_alloc(requested_properties,
                                       pool_properties_ext,
                                       addr_pair_p,
                                       handle_p);
    else
    {   // handle static banks

        if (requested_properties.size <= bank_p->block_byte_count)
        {
            int rc;
            dma_resource_addr_pair_t addr_pair;
            dma_resource_properties_t new_properties;

            if( dma_resource_lib_dma_pool_get(bank_p,
                                           &addr_pair.address_p) != 0)
            {
                LOG_WARN("dma_resource_alloc: failed for static bank %d\n",
                         (int)requested_properties.bank);
                return -1;
            }

            addr_pair.domain = DMARES_DOMAIN_HOST;
            // All memory blocks have the same fixed size in one bank
            new_properties = requested_properties;

            if (new_properties.size != bank_p->block_byte_count)
            {
                LOG_INFO(
                     "%s: changing requested resource size from %d to %d bytes "
                     "for static bank %d\n",
                     __func__,
                     new_properties.size,
                     bank_p->block_byte_count,
                     (int)requested_properties.bank);

                new_properties.size = bank_p->block_byte_count;
            }

            if (new_properties.f_cached != bank_p->f_cached)
            {
                LOG_INFO(
                     "%s: changing requested resource caching from %d to %d "
                     "for static bank %d\n",
                     __func__,
                     new_properties.f_cached,
                     bank_p->f_cached,
                     (int)requested_properties.bank);

                new_properties.f_cached = bank_p->f_cached;
            }

            rc = hwpal_dma_resource_check_and_register(new_properties,
                                                    addr_pair,
                                                    'R',
                                                    handle_p);
            if (rc != 0)
            {
                // Failed to register the memory block obtained from
                // the DMA pool as a DMA resource, put it back to the pool
                dma_resource_lib_dma_pool_put(bank_p, addr_pair.address_p);
            }
            else
            {
                addr_pair_p->domain    = addr_pair.domain;
                addr_pair_p->address_p = addr_pair.address_p;
            }

            return rc;
        }
        else if (requested_properties.size ==
                 bank_p->block_byte_count * bank_p->block_count)
        {   // bank-full DMA resource allocation is requested

            // Check that no DMA resources have been allocated in this bank yet
            // If true then lock the bank so that
            // no new DMA resources can be allocated in it
            if(dma_resource_lib_dma_pool_lock(bank_p))
            {
                // No DMA resources have been allocated in this bank yet
                *handle_p = bank_p->pool_handle;

                return 0; // bank-full DMA resource is locked
            }
            else
            {   // DMA resources have been already allocated in this bank
                LOG_CRIT("dma_resource_alloc: failed, "
                         "all-pool DMA resource unavailable for bank %d\n",
                         (int)requested_properties.bank);
                return -1;
            }
        }
        else
        {   // Requested size unsupported
            LOG_CRIT("dma_resource_alloc: failed, "
                     "requested size %d > bank block size %d (bytes)\n",
                     (int)requested_properties.size,
                     bank_p->block_byte_count);
            return -1;
        }
    }
#else
    return hwpal_dma_resource_alloc(requested_properties,
                                   pool_properties_ext,
                                   addr_pair_p,
                                   handle_p);
#endif // HWPAL_DMARESOURCE_BANKS_ENABLE
}


/*----------------------------------------------------------------------------
 * dma_resource_release
 */
int
dma_resource_release(
        const dma_resource_handle_t handle)
{
#ifdef HWPAL_DMARESOURCE_BANKS_ENABLE
    hwpal_dma_resource_bank_t * bank_p;
    dma_resource_record_t * rec_p;

    rec_p = dma_resource_handle2_record_ptr(handle);
    if (rec_p == NULL)
    {
        LOG_CRIT("dma_resource_release: invalid handle %p\n", handle);
        return -1;
    }

    // Find the bank
    bank_p = dma_resource_lib_dma_pool_bank_get(rec_p->props.bank);
    if (bank_p == NULL)
    {
        LOG_CRIT("dma_resource_release: failed for unsupported bank %d\n",
                 (int)rec_p->props.bank);
        return -1; // bank not supported
    }

    // handle non-static banks
    if (bank_p->bank_type == HWPAL_DMARESOURCE_BANK_DYNAMIC)
        return hwpal_dma_resource_release(handle);
    else
    {
        // Check if the all-pool DMA resource release is requested
        if (handle == bank_p->pool_handle)
        {
            // Unlock the DMA pool in the bank so that new DMA resources
            // can be allocated in it
            if (dma_resource_lib_dma_pool_unlock(bank_p) == false)
            {
                LOG_CRIT("dma_resource_release: failed, "
                         "all-pool DMA resource for bank %d "
                         "already released\n",
                         (int)rec_p->props.bank);
                return -1;
            }
            else
                // DMA pool handle will be released by dma_resource_uninit()
                return 0; // bank-full DMA resource is unlocked
        }

        if (rec_p->allocator_ref == 'R')
        {
            dma_resource_addr_pair_t * addr_pair_p;

            addr_pair_p = dma_resource_lib_lookup_domain(rec_p, DMARES_DOMAIN_HOST);
            if (addr_pair_p == NULL)
            {
                LOG_CRIT("dma_resource_release: "
                         "host address lookup failed for handle %p\n",
                         handle);
                return -1;
            }

            // Check if resource belongs to a DMA pool
            if (bank_p->block_byte_count == rec_p->props.size)
            {
                // It does, put it back to pool
                if (dma_resource_lib_dma_pool_put(bank_p,
                                               addr_pair_p->address_p) != 0)
                {
                    LOG_CRIT("dma_resource_release: "
                             "put to DMA pool failed for handle %p\n",
                             handle);
                    hwpal_dma_resource_release(handle);
                    return -1;
                }
            }
        }

        return hwpal_dma_resource_release(handle);
    }
#else
    return hwpal_dma_resource_release(handle);
#endif // HWPAL_DMARESOURCE_BANKS_ENABLE
}


/*----------------------------------------------------------------------------
 * dma_resource_check_and_register
 */
int
dma_resource_check_and_register(
        const dma_resource_properties_t requested_properties,
        const dma_resource_addr_pair_t addr_pair,
        const char allocator_ref,
        dma_resource_handle_t * const handle_p)
{
#ifdef HWPAL_DMARESOURCE_BANKS_ENABLE
    hwpal_dma_resource_bank_t * bank_p;

    // Find the bank
    bank_p = dma_resource_lib_dma_pool_bank_get(requested_properties.bank);
    if (bank_p == NULL)
    {
        LOG_CRIT(
            "dma_resource_check_and_register: failed for unsupported bank %d\n",
            (int)requested_properties.bank);

        return -1; // bank not supported
    }

    // handle non-static banks
    if (bank_p->bank_type == HWPAL_DMARESOURCE_BANK_DYNAMIC)
        return hwpal_dma_resource_check_and_register(requested_properties,
                                                  addr_pair,
                                                  allocator_ref,
                                                  handle_p);
    else // handle static banks in a different way
    {
        // Only allocators 'R' and 'N' are supported for static banks
        if (allocator_ref != 'R' && allocator_ref != 'N')
        {
            LOG_CRIT(
                "dma_resource_check_and_register: failed, "
                "allocator %c unsupported for static bank %d\n",
                allocator_ref,
                (int)requested_properties.bank);
            return -1;
        }

        // Check if requested size is less than the bank memory block size
        if ((allocator_ref == 'R' &&
             requested_properties.size < bank_p->block_byte_count) ||
             allocator_ref == 'N')
        {
            return hwpal_dma_resource_check_and_register(requested_properties,
                                                      addr_pair,
                                                      allocator_ref,
                                                      handle_p);
        }
        else
        {
            LOG_CRIT(
                "dma_resource_check_and_register: failed, "
                "requested size %d >= block size %d in static bank %d\n",
                requested_properties.size,
                bank_p->block_byte_count,
                (int)requested_properties.bank);
            return -1;
        }
    }
#else
    return hwpal_dma_resource_check_and_register(requested_properties,
                                              addr_pair,
                                              allocator_ref,
                                              handle_p);
#endif // HWPAL_DMARESOURCE_BANKS_ENABLE
}


/*----------------------------------------------------------------------------
 * dma_resource_read32
 */
u32
dma_resource_read32(
        const dma_resource_handle_t handle,
        const unsigned int word_offset)
{
    dma_resource_addr_pair_t * pair_p;
    dma_resource_record_t * rec_p;

    rec_p = dma_resource_handle2_record_ptr(handle);
#ifndef HWPAL_DMARESOURCE_OPT1
    if (rec_p == NULL)
    {
        LOG_WARN(
            "dma_resource_read32: "
            "Invalid handle %p\n",
            handle);

        return 0;
    }
#endif

#ifdef HWPAL_DMARESOURCE_STRICT_ARGS_CHECKS
    if (word_offset * 4 >= rec_p->props.size)
    {
        LOG_WARN(
            "dma_resource_read32: "
            "Invalid word_offset %u for handle %p\n",
            word_offset,
            handle);

        return 0;
    }
#endif

    pair_p = dma_resource_lib_lookup_domain(rec_p, DMARES_DOMAIN_HOST);
    if (pair_p == NULL)
    {
        LOG_WARN(
            "dma_resource_read32: "
            "No host address found for handle %p?\n",
            handle);

        return 0;
    }
    else
    {
        u32 * address_p = pair_p->address_p;
        u32 value = read32_volatile(address_p + word_offset);

#ifndef HWPAL_DMARESOURCE_OPT2
        // swap endianness, if required
        if (rec_p->f_swap_endianness)
            value = device_swap_endian32(value);
#endif

#ifdef HWPAL_TRACE_DMARESOURCE_READ
        log_formatted_message(
            "dma_resource_read32:  "
            "(handle %p) "
            "0x%08x = [%u] "
            "(swap=%d)\n",
            handle,
            value,
            word_offset,
            rec_p->f_swap_endianness);
#endif

        return value;
    }
}


/*----------------------------------------------------------------------------
 * dma_resource_write32
 */
void
dma_resource_write32(
        const dma_resource_handle_t handle,
        const unsigned int word_offset,
        const u32 value)
{
    dma_resource_addr_pair_t * pair_p;
    dma_resource_record_t * rec_p;

    rec_p = dma_resource_handle2_record_ptr(handle);
#ifndef HWPAL_DMARESOURCE_OPT1
    if (rec_p == NULL)
    {
        LOG_WARN(
            "dma_resource_write32: "
            "Invalid handle %p\n",
            handle);

        return;
    }
#endif

#ifdef HWPAL_DMARESOURCE_STRICT_ARGS_CHECKS
    if (word_offset * 4 >= rec_p->props.size)
    {
        LOG_WARN(
            "dma_resource_write32: "
            "Invalid word_offset %u for handle %p\n",
            word_offset,
            handle);

        return;
    }
#endif

    pair_p = dma_resource_lib_lookup_domain(rec_p, DMARES_DOMAIN_HOST);
    if (pair_p == NULL)
    {
        LOG_WARN(
            "dma_resource_write32: "
            "No host address found for handle %p?\n",
            handle);

        return;
    }
    else
    {
        u32 * address_p = pair_p->address_p;
        u32 write_value = value;
#ifdef HWPAL_TRACE_DMARESOURCE_WRITE
        log_formatted_message(
            "dma_resource_write32: "
            "(handle %p) "
            "[%u] = 0x%08x "
            "(swap=%d)\n",
            handle,
            word_offset,
            value,
            rec_p->f_swap_endianness);
#endif

#ifndef HWPAL_DMARESOURCE_OPT2
        // swap endianness, if required
        if (rec_p->f_swap_endianness)
            write_value = device_swap_endian32(write_value);
#endif

        write32_volatile(write_value, address_p + word_offset);
    }
}


/*----------------------------------------------------------------------------
 * dma_resource_read32_array
 */
void
dma_resource_read32_array(
        const dma_resource_handle_t handle,
        const unsigned int start_word_offset,
        const unsigned int word_count,
        u32 * values_p)
{
    dma_resource_addr_pair_t * pair_p;
    dma_resource_record_t * rec_p;

    if (word_count == 0)
        return;

    rec_p = dma_resource_handle2_record_ptr(handle);
    if (rec_p == NULL)
    {
        LOG_WARN(
            "dma_resource_read32_array: "
            "Invalid handle %p\n",
            handle);
        return;
    }

#ifdef HWPAL_DMARESOURCE_STRICT_ARGS_CHECKS
    if ((start_word_offset + word_count - 1) * 4 >= rec_p->props.size)
    {
        LOG_WARN(
            "dma_resource_read32_array: "
            "Invalid range: %u - %u\n",
            start_word_offset,
            start_word_offset + word_count - 1);
        return;
    }
#endif

    pair_p = dma_resource_lib_lookup_domain(rec_p, DMARES_DOMAIN_HOST);
    if (pair_p == NULL)
    {
        LOG_WARN(
            "dma_resource_read32_array: "
            "No host address found for handle %p?\n",
            handle);

        return;
    }
    else
    {
        u32 * address_p = pair_p->address_p;
        unsigned int i;

        for (i = 0; i < word_count; i++)
        {
            u32 value = read32_volatile(address_p + start_word_offset + i);

            // swap endianness, if required
            if (rec_p->f_swap_endianness)
                value = device_swap_endian32(value);

            values_p[i] = value;
        } // for
#ifdef HWPAL_TRACE_DMARESOURCE_READ
        if (values_p == address_p + start_word_offset)
        {
            log_formatted_message(
                "dma_resource_read32_array: "
                "(handle %p) "
                "[%u..%u] IN-PLACE "
                "(swap=%d)\n",
                handle,
                start_word_offset,
                start_word_offset + word_count - 1,
                rec_p->f_swap_endianness);
        }
        else
        {
            log_formatted_message(
                "dma_resource_read32_array: "
                "(handle %p) "
                "[%u..%u] "
                "(swap=%d)\n",
                handle,
                start_word_offset,
                start_word_offset + word_count - 1,
                rec_p->f_swap_endianness);
        }
#endif
    }
}


/*----------------------------------------------------------------------------
 * dma_resource_write32_array
 */
void
dma_resource_write32_array(
        const dma_resource_handle_t handle,
        const unsigned int start_word_offset,
        const unsigned int word_count,
        const u32 * values_p)
{
    dma_resource_addr_pair_t * pair_p;
    dma_resource_record_t * rec_p;

    if (word_count == 0)
        return;

    rec_p = dma_resource_handle2_record_ptr(handle);
    if (rec_p == NULL)
    {
        LOG_WARN(
            "dma_resource_write32_array: "
            "Invalid handle %p\n",
            handle);
        return;
    }

#ifdef HWPAL_DMARESOURCE_STRICT_ARGS_CHECKS
    if ((start_word_offset + word_count - 1) * 4 >= rec_p->props.size)
    {
        LOG_WARN(
            "dma_resource_write32_array: "
            "Invalid range: %u - %u\n",
            start_word_offset,
            start_word_offset + word_count - 1);
        return;
    }
#endif

    pair_p = dma_resource_lib_lookup_domain(rec_p, DMARES_DOMAIN_HOST);
    if (pair_p == NULL)
    {
        LOG_WARN(
            "dma_resource_write32_array: "
            "No host address found for handle %p?\n",
            handle);

        return;
    }
    else
    {
        u32 * address_p = pair_p->address_p;
        unsigned int i;

        for (i = 0; i < word_count; i++)
        {
            u32 value = values_p[i];

            // swap endianness, if required
            if (rec_p->f_swap_endianness)
                value = device_swap_endian32(value);

            write32_volatile(value, address_p + start_word_offset + i);
        } // for
#ifdef HWPAL_TRACE_DMARESOURCE_WRITE
        if (values_p == address_p + start_word_offset)
        {
            log_formatted_message(
                "dma_resource_write32_array: "
                "(handle %p) "
                "[%u..%u] IN-PLACE "
                "(swap=%d)\n",
                handle,
                start_word_offset,
                start_word_offset + word_count - 1,
                rec_p->f_swap_endianness);
        }
        else
        {
            log_formatted_message(
                "dma_resource_write32_array: "
                "(handle %p) "
                "[%u..%u] "
                "(swap=%d)\n",
                handle,
                start_word_offset,
                start_word_offset + word_count - 1,
                rec_p->f_swap_endianness);
        }
#endif /* HWPAL_TRACE_DMARESOURCE_WRITE */
    }
}


/*----------------------------------------------------------------------------
 * dma_resource_swap_endianness_set
 */
int
dma_resource_swap_endianness_set(
        const dma_resource_handle_t handle,
        const bool f_swap_endianness)
{
    dma_resource_record_t * rec_p;

    rec_p = dma_resource_handle2_record_ptr(handle);
    if (rec_p == NULL)
    {
        LOG_WARN(
            "dma_resource_swap_endianness_set: "
            "Invalid handle %p\n",
            handle);
        return -1;
    }

    rec_p->f_swap_endianness = f_swap_endianness;
    return 0;
}


/*----------------------------------------------------------------------------
 * dma_resource_swap_endianness_get
 */
int
dma_resource_swap_endianness_get(
        const dma_resource_handle_t handle)
{
    dma_resource_record_t * rec_p;

    rec_p = dma_resource_handle2_record_ptr(handle);
    if (rec_p == NULL)
    {
        LOG_WARN(
            "dma_resource_swap_endianness_get: "
            "Invalid handle %p\n",
            handle);
        return -1;
    }

    if (rec_p->f_swap_endianness)
    {
        return 1;
    }
    return 0;
}


/*----------------------------------------------------------------------------
 * dma_resource_add_pair
 */
int
dma_resource_add_pair(
        const dma_resource_handle_t handle,
        const dma_resource_addr_pair_t pair)
{
    dma_resource_record_t * rec_p;
    dma_resource_addr_pair_t * addr_pair_p;

    rec_p = dma_resource_handle2_record_ptr(handle);
    if (rec_p == NULL)
    {
        LOG_WARN(
            "dma_resource_add_pair: "
            "Invalid handle %p\n",
            handle);
        return -1;
    }

    // check if this pair already exists
    addr_pair_p = dma_resource_lib_lookup_domain(rec_p, pair.domain);
    if (addr_pair_p)
    {
        LOG_INFO(
            "dma_resource_add_pair: "
            "Replacing address for handle %p?\n",
            handle);
    }
    else
    {
        // find a free slot to store this domain info
        addr_pair_p = dma_resource_lib_lookup_domain(rec_p, 0);
        if (addr_pair_p == NULL)
        {
            LOG_WARN(
                "dma_resource_add_pair: "
                "Table overflow for handle %p\n",
                handle);
            return -2;
        }
    }

    if (!dma_resource_lib_is_sane_input(&pair, &rec_p->allocator_ref, &rec_p->props))
    {
        return -3;
    }

    *addr_pair_p = pair;
    return 0;
}


/*----------------------------------------------------------------------------
 * dma_resource_translate
 *
 * This function implements a fake address translation which only extracts
 * the required address information from the corresponding resource record
 *
 * All the address information is stored in the resource record in the
 * dma_resource_alloc() and dma_resource_check_and_register() functions
 *
 */
int
dma_resource_translate(
        const dma_resource_handle_t handle,
        const dma_resource_addr_domain_t dest_domain,
        dma_resource_addr_pair_t * const pair_out_p)
{
    dma_resource_record_t * rec_p;
    dma_resource_addr_pair_t * pair_p;

    if (NULL == pair_out_p)
    {
        return -1;
    }

    rec_p = dma_resource_handle2_record_ptr(handle);
    if (rec_p == NULL)
    {
        LOG_WARN(
            "dma_resource_translate: "
            "Invalid handle %p\n",
            handle);
        return -1;
    }

    switch (dest_domain)
    {
        case DMARES_DOMAIN_HOST_UNALIGNED:
        case DMARES_DOMAIN_HOST:
        case DMARES_DOMAIN_BUS:
            pair_p = dma_resource_lib_lookup_domain(rec_p, dest_domain);
            if (pair_p != NULL)
            {
                *pair_out_p = *pair_p;
                return 0;
            }
            break;

        default:
            LOG_WARN(
                "dma_resource_translate: "
                "Unsupported domain %u\n",
                dest_domain);
            pair_out_p->address_p = NULL;
            pair_out_p->domain = DMARES_DOMAIN_UNKNOWN;
            return -1;
    } // switch

    LOG_WARN(
        "dma_resource_translate: "
        "No address for domain %u (handle=%p)\n",
        dest_domain,
        handle);

    pair_out_p->address_p = NULL;
    pair_out_p->domain = DMARES_DOMAIN_UNKNOWN;
    return -1;
}


/* end of file dmares_gen.c */
