/* dmares_lkm.c
 *
 * Linux Kernel-Mode implementation of the Driver Framework DMAResource API
 *
 */

/*****************************************************************************
* Copyright (c) 2010-2024 by Rambus, Inc. and/or its subsidiaries.
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
 * This module implements (provides) some of the following interface(s):
 */

#include "dmares_mgmt.h"
#include "dmares_buf.h"
#include "dmares_rw.h"

// internal API implemented here
#include "dmares_hwpal.h"


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Default configuration
#include "c_dmares_lkm.h"

#include "dmares_gen.h"         // Helpers from Generic DMAResource API

#include "device_swap.h"        // device_swap_endian32
#include "device_mgmt.h"        // eip_device_get_reference

// Driver Framework C Run-Time Library API
#include "clib.h"               // memset

// Driver Framework Basic Definitions API
#include "basic_defs.h"         // u32, NULL, inline, bool,
                                // IDENTIFIER_NOT_USED

// Logging API
#include "log.h"                // LOG_*

// Linux Kernel API
#include <linux/types.h>        // phys_addr_t
#include <linux/slab.h>         // kmalloc, kfree
#include <linux/dma-mapping.h>  // dma_sync_single_for_cpu, dma_alloc_coherent,
                                // dma_free_coherent
#include <linux/hardirq.h>      // in_atomic
#include <linux/ioport.h>       // resource

#ifdef HWPAL_LOCK_SLEEPABLE
#include <linux/mutex.h>        // mutex_*
#else
#include <linux/spinlock.h>     // spinlock_*
#endif
#include <linux/version.h>


/*----------------------------------------------------------------------------
 * Definitions and macros
 */
#ifdef HWPAL_64BIT_HOST
#ifdef HWPAL_DMARESOURCE_64BIT
#define HWPAL_DMA_FLAGS 0 // No special requirements for address.
#else
#define HWPAL_DMA_FLAGS GFP_DMA // 64-bit host, 32-bit memory addresses
#endif
#else
#define HWPAL_DMA_FLAGS 0 // No special requirements for address.
#endif

#define HWPAL_DMA_COHERENT_MAX_ATOMIC_SIZE 65536

/*
 Requirements on the records:
  - pre-allocated array of records
  - Total size of this array may not be limited by kmalloc size.
  - valid between Create and Destroy
  - re-use on a least-recently-used basis to make sure accidental continued
    use after destroy does not cause crashes, allowing us to detect the
    situation instead of crashing quickly.

 Requirements on the handles:
  - one handle per record
  - valid between Create and Destroy
  - quickly find the ptr-to-record belonging to the handle
  - detect continued use of a handle after Destroy
  - caller-hidden admin/status, thus not inside the record
  - report leaking handles upon exit

 Solution:
  - handle cannot be a record number (no post-destroy use detection possible)
  - handle is a pointer into the handles_p array. Each entry contains a record
    index pair (or HWPAL_INVALID_INDEX if no record is associated with it).
  - Array of records is divided into chunks, each chunk contains as many
    records as fits into kmalloc buffer.
  - Pointers to these chunks in record_chunk_ptrs_p array. Each record is
    accessed via an index pair (chunk number and index within chunk).
  - list of free locations in handles_p:  free_handles
  - list of free record index pairs     : free_records
 */

typedef struct
{
    int read_index;
    int write_index;
    u32 * nrs_p;
} hwpal_free_list_t;

typedef struct
{
    int cur_index;
} dma_resource_lib_in_use_handles_iterator_t;


/* Each chunk holds as many DMA resource records as will fit into a single
   kmalloc buffer, but not more than 65536 as we use a 16-bit index  */
#define MAX_RECORDS_PER_CHUNK \
    (KMALLOC_MAX_SIZE / sizeof(dma_resource_record_t) > 65536 ? 65536 : \
     KMALLOC_MAX_SIZE / sizeof(dma_resource_record_t))

/* Each DMA handle stores a 32-bit index pair consisting of two 16-bit
   fields to refer to the DMA resource record via record_chunk_ptrs_p.
   record_chunk_ptrs_p is an array of pointers to contiguous arrays
   (chunks) of DMA resource records.

   Those 16-bit fields (chunk number and record index) are combined into a
   single u32
   Special value HWPAL_INVALID_INDEX (0xffffffff) represents destroyed records.
*/

#define COMPOSE_RECNR(chunk, recidx) (((chunk) << 16) | recidx)
#define CHUNK_OF(recnr) (((recnr) >> 16) & 0xffff)
#define RECIDX_OF(recnr) ((recnr) & 0xffff)
#define HWPAL_INVALID_INDEX 0xffffffff

// Note: dma_get_cache_alignment() can be used in place of L1_CACHE_BYTES
//       but it does not work on the 64-bit PowerPC Freescale P5020DS!
#ifndef HWPAL_DMARESOURCE_DCACHE_LINE_SIZE
#define HWPAL_DMARESOURCE_DCACHE_LINE_SIZE      L1_CACHE_BYTES
#endif

#ifdef HWPAL_DMARESOURCE_UNCACHED_MAPPING
// arm and arm64 support this, powerpc does not
#define IOREMAP_CACHE   ioremap_cache
#else
#define IOREMAP_CACHE   ioremap
#endif // HWPAL_DMARESOURCE_UNCACHED_MAPPING

#ifdef HWPAL_DMARESOURCE_ALLOC_CACHE_COHERENT
#define HWPAL_DMARESOURCE_SET_MASK        dma_set_coherent_mask
#else
#define HWPAL_DMARESOURCE_SET_MASK        dma_set_mask
#endif


/*----------------------------------------------------------------------------
 * Local variables
 */

static int handles_count = 0; // remainder are valid only when this is != 0
static int chunks_count;

static u32 * handles_p;
static dma_resource_record_t ** record_chunk_ptrs_p;

// array of pointers to arrays.
static hwpal_free_list_t free_handles;
static hwpal_free_list_t free_records;

static void * hwpal_lock_p;


/*----------------------------------------------------------------------------
 * dma_resource_lib_idx_pair2_record_ptr
 *
 */
static inline dma_resource_record_t *
dma_resource_lib_idx_pair2_record_ptr(u32 idx_pair)
{
    if (idx_pair != HWPAL_INVALID_INDEX &&
        CHUNK_OF(idx_pair) < chunks_count &&
        RECIDX_OF(idx_pair) < MAX_RECORDS_PER_CHUNK &&
        CHUNK_OF(idx_pair) * MAX_RECORDS_PER_CHUNK +
        RECIDX_OF(idx_pair) < handles_count
        )
    {
        return record_chunk_ptrs_p[CHUNK_OF(idx_pair)] + RECIDX_OF(idx_pair);;
    }
    else
    {
        return NULL;
    }
}


/*----------------------------------------------------------------------------
 * dma_resource_lib_free_list_get
 *
 * Gets the next entry from the freelist. Returns HWPAL_INVALID_INDEX
 *  when the list is empty.
 */
static inline u32
dma_resource_lib_free_list_get(
        hwpal_free_list_t * const list_p)
{
    u32 nr = HWPAL_INVALID_INDEX;
    int read_index_updated = list_p->read_index + 1;

    if (read_index_updated >= handles_count)
        read_index_updated = 0;

    // if post-increment read_index == write_index, the list is empty
    if (read_index_updated != list_p->write_index)
    {
        // grab the next number
        nr = list_p->nrs_p[list_p->read_index];
        list_p->read_index = read_index_updated;
    }

    return nr;
}


/*----------------------------------------------------------------------------
 * dma_resource_lib_free_list_add
 *
 * Adds an entry to the freelist.
 */
static inline void
dma_resource_lib_free_list_add(
        hwpal_free_list_t * const list_p,
        u32 nr)
{
    if (list_p->write_index == list_p->read_index)
    {
        LOG_WARN(
            "dma_resource_lib_free_list_add: "
            "Attempt to add value %u to full list\n",
            nr);
        return;
    }

    if (nr == HWPAL_INVALID_INDEX)
    {
        LOG_WARN(
            "dma_resource_lib_free_list_add: "
            "Attempt to put invalid value: %u\n",
            nr);
        return;
    }

    {
        int write_index_updated = list_p->write_index + 1;
        if (write_index_updated >= handles_count)
            write_index_updated = 0;

        // store the number
        list_p->nrs_p[list_p->write_index] = nr;
        list_p->write_index = write_index_updated;
    }
}

/*----------------------------------------------------------------------------
 * DMAResourceLib_InUseHandles_*
 *
 * Helper functions to iterate over all currently in-use handles.
 *
 * Usage:
 *     dma_resource_lib_in_use_handles_iterator_t it;
 *     for (handle = dma_resource_lib_in_use_handles_first(&it);
 *          handle != NULL;
 *          handle = dma_resource_lib_in_use_handles_next(&it))
 *     { ...
 *
 */
static inline dma_resource_record_t *
dma_resource_lib_in_use_handles_get(
        dma_resource_lib_in_use_handles_iterator_t * const it)
{
    dma_resource_record_t * rec_p;

    do
    {
        if (it->cur_index >= handles_count)
            return NULL;

        rec_p = dma_resource_lib_idx_pair2_record_ptr(handles_p[it->cur_index++]);

        if (rec_p != NULL && rec_p->magic != DMARES_RECORD_MAGIC)
            rec_p = NULL;
    }
    while(rec_p == NULL);

    return rec_p;
}


static inline dma_resource_record_t *
dma_resource_lib_in_use_handles_first(
        dma_resource_lib_in_use_handles_iterator_t * const it)
{
    it->cur_index = 0;
    return dma_resource_lib_in_use_handles_get(it);
}


static inline dma_resource_record_t *
dma_resource_lib_in_use_handles_next(
        dma_resource_lib_in_use_handles_iterator_t * const it)
{
    return dma_resource_lib_in_use_handles_get(it);
}


/*----------------------------------------------------------------------------
 * dma_resource_lib_is_sub_range_of
 *
 * Return true if the address range defined by `addr_pair1' and `size1' is
 * within the address range defined by `addr_pair2' and `size2'.
 */
static bool
dma_resource_lib_is_sub_range_of(
        const dma_resource_addr_pair_t * const addr_pair1,
        const unsigned int size1,
        const dma_resource_addr_pair_t * const addr_pair2,
        const unsigned int size2)
{
    if (addr_pair1->domain == addr_pair2->domain)
    {
        const u8 * addr1 = addr_pair1->address_p;
        const u8 * addr2 = addr_pair2->address_p;

        if ((size1 <= size2) &&
            (addr2 <= addr1) &&
            ((addr1 + size1) <= (addr2 + size2)))
        {
            return true;
        }
    }

    return false;
}


/*----------------------------------------------------------------------------
 * dma_resource_lib_find_matching_dma_resource
 *
 * Return a pointer to the DMAResource record for a currently allocated or
 * attached DMA buffer that matches the given `properties' and `addr_pair'.
 * The match can be either exact or indicate that the buffer defined by
 * `properties and `addr_pair' is a proper sub section of the allocated or
 * attached buffer.
 */
static dma_resource_record_t *
dma_resource_lib_find_matching_dma_resource(
        const dma_resource_properties_t * const properties,
        const dma_resource_addr_pair_t addr_pair)
{
    dma_resource_lib_in_use_handles_iterator_t it;
    dma_resource_addr_pair_t * pair_p;
    dma_resource_record_t * rec_p;
    unsigned int size;

    for (rec_p = dma_resource_lib_in_use_handles_first(&it);
         rec_p != NULL;
         rec_p = dma_resource_lib_in_use_handles_next(&it))
    {
        if (rec_p->allocator_ref == 'R' || rec_p->allocator_ref == 'N')
        {
            // skip registered buffers when looking for a match,
            // i.e. only consider allocated buffers.
            continue;
        }

        if (properties->bank != rec_p->props.bank  ||
            properties->size > rec_p->props.size ||
            properties->alignment > rec_p->props.alignment)
        {
            // obvious mismatch in properties
            continue;
        }

        size = properties->size;
        pair_p = dma_resource_lib_lookup_domain(rec_p, DMARES_DOMAIN_HOST);
        if (pair_p != NULL &&
            dma_resource_lib_is_sub_range_of(&addr_pair, size, pair_p,
                                            rec_p->props.size))
        {
            return rec_p;
        }

        pair_p = dma_resource_lib_lookup_domain(rec_p, DMARES_DOMAIN_BUS);
        if (pair_p != NULL &&
            dma_resource_lib_is_sub_range_of(&addr_pair, size, pair_p,
                                            rec_p->props.size))
        {
            return rec_p;
        }
    } // for

    return NULL;
}



/*----------------------------------------------------------------------------
 * dma_resource_lib_setup_record
 *
 * Setup most fields of a given DMAResource record, except for the
 * addr_pairs array.
 */
static void
dma_resource_lib_setup_record(
        const dma_resource_properties_t * const props_p,
        const char allocator_ref,
        dma_resource_record_t * const rec_p,
        const unsigned int allocated_size)
{
    rec_p->props = *props_p;
    rec_p->allocator_ref = allocator_ref;
    rec_p->buffer_size = allocated_size;
}


/*----------------------------------------------------------------------------
 * hwpal_dma_resource_max_alignment_get
 */
unsigned int
hwpal_dma_resource_max_alignment_get(void)
{
    return (1 * 1024 * 1024); // 1 MB
}


/*----------------------------------------------------------------------------
 * hwpal_dma_resource_d_cache_alignment_get
 */
unsigned int
hwpal_dma_resource_d_cache_alignment_get(void)
{
#ifdef HWPAL_ARCH_COHERENT
    unsigned int align_to = 1; // No cache line alignment required
#else
    unsigned int align_to = HWPAL_DMARESOURCE_DCACHE_LINE_SIZE;
#endif // HWPAL_ARCH_COHERENT

#if defined(HWPAL_DMARESOURCE_ALLOC_CACHE_COHERENT) || \
    defined (HWPAL_DRMARESOURCE_ALLOW_UNALIGNED_ADDRESS)
    align_to = 1; // No cache line alignment required
#endif

    return align_to;
}


/*----------------------------------------------------------------------------
 * hwpal_dma_resource_mem_alloc
 */
void *
hwpal_dma_resource_mem_alloc(
        size_t byte_count)
{
    gfp_t flags = 0;

    if (in_atomic())
        flags |= GFP_ATOMIC;    // non-sleepable
    else
        flags |= GFP_KERNEL;    // sleepable

    return kmalloc(byte_count, flags);
}


/*----------------------------------------------------------------------------
 * hwpal_dma_resource_mem_free
 */
void
hwpal_dma_resource_mem_free(
        void * buf_p)
{
    kfree (buf_p);
}


/*----------------------------------------------------------------------------
 * hwpal_dma_resource_lock_alloc
 */
void *
hwpal_dma_resource_lock_alloc(void)
{
#ifdef HWPAL_LOCK_SLEEPABLE
    struct mutex * hwpal_lock = hwpal_dma_resource_mem_alloc(sizeof(mutex));
    if (hwpal_lock == NULL)
        return NULL;

    LOG_INFO("hwpal_dma_resource_lock_alloc: lock = mutex\n");
    mutex_init(hwpal_lock);

    return hwpal_lock;
#else
    spinlock_t * HWPAL_SpinLock;

    size_t lock_size = sizeof(spinlock_t);
    if (lock_size == 0)
        lock_size = 4;

    HWPAL_SpinLock = hwpal_dma_resource_mem_alloc(lock_size);
    if (HWPAL_SpinLock == NULL)
        return NULL;

    LOG_INFO("hwpal_dma_resource_lock_alloc: lock = spinlock\n");
    spin_lock_init(HWPAL_SpinLock);

    return HWPAL_SpinLock;
#endif
}


/*----------------------------------------------------------------------------
 * hwpal_dma_resource_lock_free
 */
void
hwpal_dma_resource_lock_free(void * lock_p)
{
    hwpal_dma_resource_mem_free(lock_p);
}


/*----------------------------------------------------------------------------
 * hwpal_dma_resource_lock_acquire
 */
void
hwpal_dma_resource_lock_acquire(
        void * lock_p,
        unsigned long * flags)
{
#ifdef HWPAL_LOCK_SLEEPABLE
    IDENTIFIER_NOT_USED(flags);
    mutex_lock((struct mutex*)lock_p);
#else
    spin_lock_irqsave((spinlock_t *)lock_p, *flags);
#endif
}


/*----------------------------------------------------------------------------
 * hwpal_dma_resource_lock_release
 */
void
hwpal_dma_resource_lock_release(
        void * lock_p,
        unsigned long * flags)
{
#ifdef HWPAL_LOCK_SLEEPABLE
    IDENTIFIER_NOT_USED(flags);
    mutex_unlock((struct mutex*)lock_p);
#else
    spin_unlock_irqrestore((spinlock_t *)lock_p, *flags);
#endif
}


/*----------------------------------------------------------------------------
 * hwpal_dma_resource_alloc
 */
int
hwpal_dma_resource_alloc(
        const dma_resource_properties_t requested_properties,
        const hwpal_dma_resource_properties_ext_t requested_properties_ext,
        dma_resource_addr_pair_t * const addr_pair_p,
        dma_resource_handle_t * const handle_p)
{
    dma_resource_properties_t actual_properties;
    dma_resource_addr_pair_t * pair_p;
    dma_resource_handle_t handle;
    dma_resource_record_t * rec_p = NULL;

    unsigned int align_to = requested_properties.alignment;

    zeroinit(actual_properties);

#ifdef HWPAL_DMARESOURCE_STRICT_ARGS_CHECKS
    if ((NULL == addr_pair_p) || (NULL == handle_p))
        return -1;

    if (!dma_resource_lib_is_sane_input(NULL, NULL, &requested_properties))
        return -1;
#endif

#ifdef HWPAL_DMARESOURCE_ALLOC_CACHE_COHERENT
    if (requested_properties_ext.bank_type ==
            HWPAL_DMARESOURCE_BANK_STATIC_FIXED_ADDR)
    {
        LOG_CRIT("dma_resource_alloc: fixed address DMA banks not supported for"
                 " cache-coherent allocations with "
                 "HWPAL_DMARESOURCE_ALLOC_CACHE_COHERENT\n");
        return -1;
    }
#endif // HWPAL_DMARESOURCE_ALLOC_CACHE_COHERENT

    // Allocate record
    handle = dma_resource_create_record();
    if (NULL == handle)
        return -1;

    rec_p = dma_resource_handle2_record_ptr(handle);
    if (NULL == rec_p)
    {
        dma_resource_destroy_record(handle);
        return -1;
    }

    actual_properties.bank       = requested_properties.bank;

#ifdef HWPAL_ARCH_COHERENT
    actual_properties.f_cached    = false;
#else
    if (requested_properties_ext.bank_type ==
                HWPAL_DMARESOURCE_BANK_STATIC_FIXED_ADDR)
        actual_properties.f_cached    = requested_properties.f_cached;
    else
        // This implementation does not allocate to non-cached resources
        actual_properties.f_cached    = true;
#endif // HWPAL_ARCH_COHERENT

#ifdef HWPAL_DMARESOURCE_ALLOC_CACHE_COHERENT
    actual_properties.f_cached    = false;
#endif // HWPAL_DMARESOURCE_ALLOC_CACHE_COHERENT

#ifdef HWPAL_TRACE_DMARESOURCE_BUF
    if (actual_properties.f_cached != requested_properties.f_cached)
    {
        LOG_INFO("%s: changing requested resource caching from %d to %d "
                 "for bank %d\n",
                 __func__,
                 requested_properties.f_cached,
                 actual_properties.f_cached,
                 requested_properties.bank);
    }
#endif // HWPAL_TRACE_DMARESOURCE_BUF

    if (actual_properties.f_cached &&
        hwpal_dma_resource_d_cache_alignment_get() > align_to)
    {
        align_to = hwpal_dma_resource_d_cache_alignment_get();
    }

    actual_properties.alignment  = align_to;

    // Hide the allocated size from the caller, since (s)he is not
    // supposed to access/use any space beyond what was requested
    actual_properties.size = requested_properties.size;

    rec_p->bank_type = requested_properties_ext.bank_type;

    // Allocate DMA resource
    {
        struct device * dma_device_p;
        size_t n = 0;
        void  * unaligned_addr_p = NULL;
        void * aligned_addr_p = NULL;
        dma_addr_t dma_addr = 0;
        phys_addr_t phys_addr = 0;
        device_data_t dev_data;

        zeroinit(dev_data);

        // Get device reference for this resource
        dma_device_p = eip_device_get_reference(NULL, &dev_data);

        // Step 1: Allocate a buffer
        if (requested_properties_ext.bank_type ==
                HWPAL_DMARESOURCE_BANK_STATIC_FIXED_ADDR)
        {
            struct resource * resource_p;
            void __iomem * io_mem_p;

            // Option 1: Fixed address buffer allocation

            phys_addr = (phys_addr_t)(uintptr_t)requested_properties_ext.addr +
                                        (phys_addr_t)(uintptr_t)dev_data.phys_addr;

            // Check bank address alignment
            if (phys_addr & (PAGE_SIZE-1))
            {
                dma_resource_destroy_record(handle);
                LOG_CRIT("dma_resource_alloc: unaligned fixed address for "
                         "bank %d, address 0x%p, page size %lu\n",
                         requested_properties.bank,
                         (void *)(uintptr_t)phys_addr,
                         PAGE_SIZE);
                return -1;
            }

            // Round size up to a multiple of PAGE_SIZE
            n = (requested_properties.size + (PAGE_SIZE-1)) & ~(PAGE_SIZE-1);

            resource_p = request_mem_region(phys_addr, n, "DMA-bank");
            if (!resource_p)
            {
                dma_resource_destroy_record(handle);
                LOG_CRIT("dma_resource_alloc: request_mem_region() failed, "
                        "resource addr 0x%p, size %d\n",
                         (void *)(uintptr_t)phys_addr,
                         (unsigned int)n);
                return -1;
            }

            if (requested_properties.f_cached)
                io_mem_p = IOREMAP_CACHE(phys_addr, n);
            else
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,0,0)
                io_mem_p = ioremap_cache(phys_addr, n);
#else
                io_mem_p = ioremap_nocache(phys_addr, n);
#endif
            // Ignore __iomem address space
            unaligned_addr_p = (void *)(uintptr_t)io_mem_p;

            if (!unaligned_addr_p)
            {
                release_mem_region(phys_addr, n);
                dma_resource_destroy_record(handle);
                LOG_CRIT("dma_resource_alloc: ioremap() failed, resource "
                         "addr 0x%p, cached %d, size %d\n",
                         (void *)(uintptr_t)phys_addr,
                         requested_properties.f_cached,
                         (unsigned int)n);
                return -1;
            }

#ifdef HWPAL_TRACE_DMARESOURCE_BUF
            LOG_INFO("dma_resource_alloc: allocated static bank at "
                     "phys addr 0x%p, offset 0x%p, size %d\n",
                     (void*)phys_addr,
                     (void*)requested_properties_ext.addr,
                     (unsigned int)n);
#endif // HWPAL_TRACE_DMARESOURCE_BUF

            if (PAGE_SIZE > align_to)
            {
                // ioremap granularity is PAGE_SIZE
                actual_properties.alignment  = align_to = PAGE_SIZE;
            }
        }
        else
        {
            gfp_t flags = HWPAL_DMA_FLAGS;

            // Option 2: Non-fixed dynamic address buffer allocation

            // Align if required
            n = dma_resource_lib_align_for_address(
                 dma_resource_lib_align_for_size(requested_properties.size,
                 align_to),
                 align_to);

            if (in_atomic())
                flags |= GFP_ATOMIC;    // non-sleepable
            else
                flags |= GFP_KERNEL;    // sleepable

#ifdef HWPAL_DMARESOURCE_ALLOC_CACHE_COHERENT
            if (n <= HWPAL_DMA_COHERENT_MAX_ATOMIC_SIZE)
                flags = GFP_ATOMIC;
            unaligned_addr_p = dma_alloc_coherent(
                                     dma_device_p, n, &dma_addr, flags);
#else
            unaligned_addr_p = kmalloc(n, flags);
#endif // HWPAL_DMARESOURCE_ALLOC_CACHE_COHERENT

            if (unaligned_addr_p == NULL)
            {
                LOG_CRIT("dma_resource_alloc: failed for handle 0x%p,"
                        " size %d\n",
                         handle,(unsigned int)n);
                dma_resource_destroy_record(handle);
                return -1;
            }
        }

        dma_resource_lib_setup_record(&actual_properties, 'A', rec_p, n);

        // Step 2: Align the allocated buffer
        {
            unsigned long alignment_offset;
            unsigned long unaligned_address = ((unsigned long)unaligned_addr_p);

            alignment_offset = unaligned_address % align_to;

            // Check if address needs to be aligned
            if( alignment_offset )
                aligned_addr_p =
                        (void*)(unaligned_address + align_to - alignment_offset);
            else
                aligned_addr_p = unaligned_addr_p; // No alignment required
        }

        // Step 3: Get the DMA address of the allocated buffer
        if (requested_properties_ext.bank_type ==
                        HWPAL_DMARESOURCE_BANK_STATIC_FIXED_ADDR)
        {
            dma_addr = phys_addr - (phys_addr_t)(uintptr_t)dev_data.phys_addr -
                            HWPAL_DMARESOURCE_BANK_STATIC_OFFSET;

#ifdef HWPAL_TRACE_DMARESOURCE_BUF
            LOG_INFO("dma_resource_alloc: handle 0x%p, "
                     "bus address requested/actual 0x%p/0x%p\n",
                     handle,
                     requested_properties_ext.addr,
                     (void*)dma_addr);
#endif // HWPAL_TRACE_DMARESOURCE_BUF
        }
        else
        {
#ifdef HWPAL_DMARESOURCE_MINIMUM_CACHE_CONTROL
            dma_addr = virt_to_phys (aligned_addr_p);
#else

#ifndef HWPAL_DMARESOURCE_ALLOC_CACHE_COHERENT
            dma_addr = dma_map_single(dma_device_p, aligned_addr_p, n,
                                     DMA_BIDIRECTIONAL);
            if (dma_mapping_error(dma_device_p, dma_addr))
            {
                LOG_WARN(
                        "dma_resource_alloc: "
                        "Failed to map DMA address for host address 0x%p, "
                        "for handle 0x%p\n",
                        aligned_addr_p,
                        handle);

                kfree(aligned_addr_p);

                dma_resource_destroy_record(handle);

                return -1;
            }
#endif // !HWPAL_DMARESOURCE_ALLOC_CACHE_COHERENT

#endif // HWPAL_DMARESOURCE_MINIMUM_CACHE_CONTROL

            if (dma_addr == 0)
            {
#ifdef HWPAL_DMARESOURCE_ALLOC_CACHE_COHERENT
                dma_free_coherent(dma_device_p, n, unaligned_addr_p, dma_addr);
#else
                kfree(unaligned_addr_p);
#endif

                dma_resource_destroy_record(handle);
                LOG_CRIT(
                    "dma_resource_alloc: "
                    "Failed to obtain DMA address for host address 0x%p, "
                    "for handle 0x%p\n",
                    aligned_addr_p,
                    handle);

                return -1;
            }

#ifdef HWPAL_TRACE_DMARESOURCE_BUF
            LOG_INFO("dma_resource_alloc: handle 0x%p, "
                     "bus address allocated/adjusted 0x%p / 0x%p\n",
                     handle,
                     (void*)dma_addr,
                     (void*)(dma_addr - HWPAL_DMARESOURCE_BANK_STATIC_OFFSET));
#endif // HWPAL_TRACE_DMARESOURCE_BUF

            dma_addr -= HWPAL_DMARESOURCE_BANK_STATIC_OFFSET;
        }

        // put the bus address first, presumably being the most
        // frequently looked-up domain.
        pair_p = rec_p->addr_pairs;
        pair_p->address_p = (void *)(uintptr_t)dma_addr;
        pair_p->domain = DMARES_DOMAIN_BUS;

        ++pair_p;
        pair_p->address_p = aligned_addr_p;
        pair_p->domain = DMARES_DOMAIN_HOST;

        // Return this address
        *addr_pair_p = *pair_p;

        // This host address will be used for freeing the allocated buffer
        ++pair_p;
        pair_p->address_p = unaligned_addr_p;
        pair_p->domain = DMARES_DOMAIN_HOST_UNALIGNED;

#ifdef HWPAL_TRACE_DMARESOURCE_BUF
        LOG_INFO("dma_resource_alloc (1/2): handle = 0x%p, allocator='%c', "
                 "size allocated/requested=%d/%d, \n"
                 "dma_resource_alloc (2/2): alignment/bank/cached=%d/%d/%d, "
                 "bus addr=0x%p, host addr un-/aligned=0x%p/0x%p\n",
                 handle, rec_p->allocator_ref,
                 rec_p->buffer_size, rec_p->props.size,
                 rec_p->props.alignment,rec_p->props.bank,rec_p->props.f_cached,
                 (void*)dma_addr,unaligned_addr_p,aligned_addr_p);
#endif
    } // Allocated DMA resource

    // return results
    *handle_p = handle;

    return 0;
}


/*----------------------------------------------------------------------------
 * hwpal_dma_resource_release
 */
int
hwpal_dma_resource_release(
        const dma_resource_handle_t handle)
{
    dma_resource_record_t * rec_p;
    dma_addr_t dma_addr = 0;

#ifdef HWPAL_TRACE_DMARESOURCE_BUF
    void* unaligned_addr_p = NULL;
#endif

    rec_p = dma_resource_handle2_record_ptr(handle);
    if (rec_p == NULL)
    {
        LOG_WARN(
            "dma_resource_release: "
            "Invalid handle %p\n",
            handle);
        return -1;
    }

    // request the kernel to unmap the DMA resource
    if (rec_p->allocator_ref == 'A' || rec_p->allocator_ref == 'k' ||
        rec_p->allocator_ref == 'R')
    {
        dma_resource_addr_pair_t * pair_p;
        struct device * dma_device_p;

        pair_p = dma_resource_lib_lookup_domain(rec_p, DMARES_DOMAIN_BUS);
        if (pair_p == NULL)
        {
            LOG_WARN(
                "dma_resource_release: "
                "No bus address found for handle %p?\n",
                handle);
            return -1;
        }

        dma_addr = (dma_addr_t)(uintptr_t)pair_p->address_p;

        pair_p = dma_resource_lib_lookup_domain(rec_p,
                                             DMARES_DOMAIN_HOST_UNALIGNED);
        if (pair_p == NULL)
        {
            LOG_WARN(
                "dma_resource_release: "
                "No host address found for handle %p?\n",
                handle);
            return -1;
        }

        // Get device reference for this resource
        dma_device_p = eip_device_get_reference(NULL, NULL);

#ifndef HWPAL_DMARESOURCE_MINIMUM_CACHE_CONTROL

#ifndef HWPAL_DMARESOURCE_ALLOC_CACHE_COHERENT
        if(rec_p->allocator_ref != 'R' &&
           rec_p->bank_type != HWPAL_DMARESOURCE_BANK_STATIC_FIXED_ADDR)
        {
            dma_unmap_single(dma_device_p,
                             dma_addr + HWPAL_DMARESOURCE_BANK_STATIC_OFFSET,
                             rec_p->buffer_size, DMA_BIDIRECTIONAL);
#ifdef HWPAL_TRACE_DMARESOURCE_BUF
            LOG_INFO("dma_resource_release: handle 0x%p, "
                     "bus address freed/adjusted 0x%p / 0x%p\n",
                     handle,
                     (void*)(dma_addr + HWPAL_DMARESOURCE_BANK_STATIC_OFFSET),
                     (void*)dma_addr);
#endif // HWPAL_TRACE_DMARESOURCE_BUF
        }
#endif // !HWPAL_DMARESOURCE_ALLOC_CACHE_COHERENT

#endif // HWPAL_DMARESOURCE_MINIMUM_CACHE_CONTROL

        if(rec_p->allocator_ref == 'A')
        {
#ifdef HWPAL_DMARESOURCE_ALLOC_CACHE_COHERENT

            #ifdef HWPAL_TRACE_DMARESOURCE_BUF
            LOG_INFO("dma_resource_release: handle 0x%p, "
                     "bus address freed/adjusted 0x%p / 0x%p\n",
                     handle,
                     (void*)(dma_addr + HWPAL_DMARESOURCE_BANK_STATIC_OFFSET),
                     (void*)dma_addr);
#endif // HWPAL_TRACE_DMARESOURCE_BUF

            dma_free_coherent(dma_device_p,
                              rec_p->buffer_size,
                              pair_p->address_p,
                              dma_addr + HWPAL_DMARESOURCE_BANK_STATIC_OFFSET);
#else
            if (rec_p->bank_type == HWPAL_DMARESOURCE_BANK_STATIC_FIXED_ADDR)
            {
                device_data_t dev_data;

                zeroinit(dev_data);
                eip_device_get_reference(NULL, &dev_data);

                iounmap((void __iomem *)pair_p->address_p);

#ifdef HWPAL_TRACE_DMARESOURCE_BUF
                LOG_INFO("dma_resource_release: handle 0x%p, "
                         "bus address freed/adjusted 0x%p / 0x%p\n",
                         handle,
                         (void*)(dma_addr + (phys_addr_t)dev_data.phys_addr +
                                     HWPAL_DMARESOURCE_BANK_STATIC_OFFSET),
                         (void*)dma_addr);
#endif // HWPAL_TRACE_DMARESOURCE_BUF

                release_mem_region(dma_addr + (phys_addr_t)(uintptr_t)dev_data.phys_addr +
                                     HWPAL_DMARESOURCE_BANK_STATIC_OFFSET,
                                   rec_p->buffer_size);
            }
            else
                kfree(pair_p->address_p);
#endif
        }

#ifdef HWPAL_TRACE_DMARESOURCE_BUF
        unaligned_addr_p = pair_p->address_p;
#endif
    }

#ifdef HWPAL_TRACE_DMARESOURCE_BUF
    LOG_INFO("dma_resource_release (1/2): "
             "handle = 0x%p, allocator='%c', "
             "size allocated/requested=%d/%d, \n"
             "dma_resource_release (2/2): "
             "alignment/bank/cached=%d/%d/%d, "
             "bus addr=0x%p, unaligned host addr=0x%p\n",
             handle, rec_p->allocator_ref,
             rec_p->buffer_size, rec_p->props.size,
             rec_p->props.alignment, rec_p->props.bank, rec_p->props.f_cached,
             (void*)dma_addr, unaligned_addr_p);
#endif

    // free administration resources
    rec_p->magic = 0;
    dma_resource_destroy_record(handle);

    return 0;
}


/*----------------------------------------------------------------------------
 * hwpal_dma_resource_init
 */
bool
hwpal_dma_resource_init(void)
{
    bool alloc_failed = false;
    unsigned int max_handles = HWPAL_DMA_NRESOURCES;

    {
        struct device * dma_device_p;
        int res;


        // Get device reference for this resource
        dma_device_p = eip_device_get_reference(NULL, NULL);

        LOG_INFO("%s: device DMA address mask 0x%016Lx\n",
                 __func__,
                 (long long unsigned int)HWPAL_DMARESOURCE_ADDR_MASK);

        // Set DMA mask wider, so the DMA API will not try to bounce
        res = HWPAL_DMARESOURCE_SET_MASK(dma_device_p,
                                         HWPAL_DMARESOURCE_ADDR_MASK);
        if ( res != 0)
        {
            LOG_CRIT("%s: failed, host does not support DMA address "
                     "mask 0x%016Lx\n",
                     __func__,
                     (long long unsigned int)HWPAL_DMARESOURCE_ADDR_MASK);
            return false;
        }
    }

    // already initialized?
    if (handles_count != 0)
        return false;

    hwpal_lock_p = hwpal_dma_resource_lock_alloc();
    if (hwpal_lock_p == NULL)
    {
        LOG_CRIT("hwpal_dma_resource_init: record lock allocation failed\n");
        return false;
    }

    chunks_count = (max_handles + MAX_RECORDS_PER_CHUNK - 1) /
        MAX_RECORDS_PER_CHUNK;

    if (chunks_count > 65535 ||
        chunks_count * sizeof(void *) > KMALLOC_MAX_SIZE)
    {
        LOG_CRIT(
            "hwpal_dma_resource_init: "
            "Too many chunks desired: %u\n",
            chunks_count);
        return false;
    }

    LOG_INFO(
         "hwpal_dma_resource_init: "
         "Allocate %d records in %d chunks, "
         "DMA address size (bytes) %d, host address size  (bytes) %d\n",
         max_handles, chunks_count,
         (int)sizeof(dma_addr_t),
         (int)sizeof(void*));

    LOG_INFO("hwpal_dma_resource_init: D-cache line size %d\n",
             HWPAL_DMARESOURCE_DCACHE_LINE_SIZE);

#ifdef HWPAL_64BIT_HOST
    if (sizeof(void*) != 8 || sizeof(long) < 8)
    {
        LOG_CRIT("\n\nHWPAL_DMAResource_Init: ERROR, 64-bit host specified, "
                 "but data sizes do not agree\n");
        return false;
    }
#else
    if (sizeof(void*) != 4)
    {
        LOG_CRIT("\n\nHWPAL_DMAResource_Init: ERROR, 32-bit host specified, "
                 "but data sizes do not agree\n");
        return false;
    }
#endif

    if (sizeof(dma_addr_t) > sizeof(void*))
    {
        LOG_WARN(
            "\n\nHWPAL_DMAResource_Init: WARNING, "
            "unsupported host DMA address size %d\n\n",
            (int)sizeof(dma_addr_t));
    }

    record_chunk_ptrs_p =
            hwpal_dma_resource_mem_alloc(chunks_count * sizeof(void *));
    if (record_chunk_ptrs_p)
    {
        // Allocate each of the chunks in the record_chunk_ptrs_p array,
        // all of size MAX_RECORDS_PER_CHUNK, except the last one.
        int records_left, records_this_chunk, i;

        // Pre-initialize with null, in case of early error exit
        memset(
            record_chunk_ptrs_p,
            0,
            chunks_count * sizeof(dma_resource_record_t*));

        i=0;
        records_left = max_handles;

        while ( records_left )
        {
            if (records_left > MAX_RECORDS_PER_CHUNK)
                records_this_chunk = MAX_RECORDS_PER_CHUNK;
            else
                records_this_chunk = records_left;

            record_chunk_ptrs_p[i] =
                    hwpal_dma_resource_mem_alloc(records_this_chunk *
                                            sizeof(dma_resource_record_t));
            if( record_chunk_ptrs_p[i] == NULL )
            {
                LOG_CRIT(
                    "hwpal_dma_resource_init:"
                    "Allocation failed chunk %d\n",i);
                alloc_failed = true;
                break;
            }

            records_left -= records_this_chunk;
            i++;
        }
        LOG_INFO(
            "hwpal_dma_resource_init:"
            "Allocated %d chunks last one=%d others=%d, total=%d\n",
            i,
            records_this_chunk,
            (int)MAX_RECORDS_PER_CHUNK,
            (int)((i-1) * MAX_RECORDS_PER_CHUNK + records_this_chunk));
    }

    if (max_handles * sizeof(u32) > KMALLOC_MAX_SIZE)
    { // Too many handles for kmalloc, allocate with get_free_pages.
        int order = get_order(max_handles * sizeof(u32));

        LOG_INFO(
             "hwpal_dma_resource_init: "
             "Handles & freelist allocated by get_free_pages order=%d\n",
             order);

        handles_p = (void*) __get_free_pages(GFP_KERNEL, order);
        free_handles.nrs_p = (void*) __get_free_pages(GFP_KERNEL, order);
        free_records.nrs_p = (void*) __get_free_pages(GFP_KERNEL, order);
    }
    else
    {
        handles_p = hwpal_dma_resource_mem_alloc(max_handles * sizeof(u32));
        free_handles.nrs_p =
                hwpal_dma_resource_mem_alloc(max_handles * sizeof(u32));
        free_records.nrs_p =
                hwpal_dma_resource_mem_alloc(max_handles * sizeof(u32));
    }

    // if any allocation failed, free the whole lot
    if (record_chunk_ptrs_p == NULL ||
        handles_p == NULL ||
        free_handles.nrs_p == NULL ||
        free_records.nrs_p == NULL ||
        alloc_failed)
    {
        LOG_CRIT(
            "hwpal_dma_resource_init: "
            "RP=%p HP=%p FH=%p FR=%p AF=%d\n",
            record_chunk_ptrs_p,
            handles_p,
            free_handles.nrs_p,
            free_records.nrs_p,
            alloc_failed);

        if (record_chunk_ptrs_p)
        {
            int i;
            for (i = 0; i < chunks_count; i++)
            {
                if(record_chunk_ptrs_p[i])
                    hwpal_dma_resource_mem_free(record_chunk_ptrs_p[i]);
            }
            hwpal_dma_resource_mem_free(record_chunk_ptrs_p);
        }

        if (max_handles * sizeof(u32) > KMALLOC_MAX_SIZE)
        { // Were allocated with get_free pages.
            int order = get_order(max_handles * sizeof(u32));

            if (handles_p)
                free_pages((unsigned long)handles_p, order);

            if (free_handles.nrs_p)
                free_pages((unsigned long)free_handles.nrs_p, order);

            if (free_records.nrs_p)
                free_pages((unsigned long)free_records.nrs_p, order);
        }
        else
        {
            if (handles_p)
                hwpal_dma_resource_mem_free(handles_p);

            if (free_handles.nrs_p)
                hwpal_dma_resource_mem_free(free_handles.nrs_p);

            if (free_records.nrs_p)
                hwpal_dma_resource_mem_free(free_records.nrs_p);
        }
        record_chunk_ptrs_p = NULL;
        handles_p = NULL;
        free_handles.nrs_p = NULL;
        free_records.nrs_p = NULL;

        if (hwpal_lock_p != NULL)
            hwpal_dma_resource_lock_free(hwpal_lock_p);

        return false;
    }

    // initialize the record numbers freelist
    // initialize the handle numbers freelist
    // initialize the handles array
    {
        unsigned int i;
        unsigned int chunk=0;
        unsigned int recidx=0;

        for (i = 0; i < max_handles; i++)
        {
            handles_p[i] = HWPAL_INVALID_INDEX;
            free_handles.nrs_p[i] = max_handles - 1 - i;
            free_records.nrs_p[i] = COMPOSE_RECNR(chunk,recidx);
            recidx++;
            if(recidx == MAX_RECORDS_PER_CHUNK)
            {
                chunk++;
                recidx = 0;
            }
        }

        free_handles.read_index = 0;
        free_handles.write_index = 0;

        free_records.read_index = 0;
        free_records.write_index = 0;
    }

    handles_count = max_handles;

    return true;
}


/*----------------------------------------------------------------------------
 * hwpal_dma_resource_uninit
 *
 * This function can be used to uninitialize the DMAResource administration.
 * The caller must make sure that handles will not be used after this function
 * returns.
 * If memory was allocated by hwpal_dma_resource_init, this function will
 * free it.
 */
void
hwpal_dma_resource_uninit(void)
{
    // exit if not initialized
    if (handles_count == 0)
        return;

    // find resource leaks
#ifdef HWPAL_TRACE_DMARESOURCE_LEAKS
    {
        int i;
        bool f_first_print = true;

        for (i = 0; i < handles_count; i++)
        {
            u32 idx_pair = handles_p[i];

            if (idx_pair != HWPAL_INVALID_INDEX)
            {
                if (f_first_print)
                {
                    f_first_print = false;
                    log_formatted_message(
                        "hwpal_dma_resource_uninit found leaking handles:\n");
                }

                log_formatted_message(
                    "handle %p => "
                    "Record %u\n",
                    handles_p + i,
                    idx_pair);

                {
                    dma_resource_addr_pair_t * pair_p;
                    dma_resource_record_t * rec_p =
                        dma_resource_lib_idx_pair2_record_ptr(idx_pair);

                    if(rec_p != NULL)
                    {
                        pair_p = dma_resource_lib_lookup_domain(rec_p,
                                                          DMARES_DOMAIN_HOST);

                        log_formatted_message(
                            "  allocated_size = %d\n"
                            "  alignment = %d\n"
                            "  bank = %d\n"
                            "  bank_type = %d\n"
                            "  Host address = %p\n",
                            rec_p->buffer_size,
                            rec_p->props.alignment,
                            rec_p->props.bank,
                            rec_p->bank_type,
                            pair_p!=NULL?pair_p->address_p:NULL);
                    }
                    else
                    {
                        log_formatted_message(" bad index pair\n");
                    }
                }
            } // if
        } // for

        if (f_first_print)
            log_formatted_message(
                "hwpal_dma_resource_uninit: no leaks found\n");
    }
#endif /* HWPAL_TRACE_DMARESOURCE_LEAKS */

    {
        int i;

        for (i = 0; i < chunks_count; i++)
            hwpal_dma_resource_mem_free(record_chunk_ptrs_p[i]);
    }
    hwpal_dma_resource_mem_free(record_chunk_ptrs_p);

    if (handles_count * sizeof(u32) > KMALLOC_MAX_SIZE)
    { // Were allocated with get_free pages.
        int order = get_order(handles_count * sizeof(u32));

        free_pages((unsigned long)handles_p, order);
        free_pages((unsigned long)free_handles.nrs_p, order);
        free_pages((unsigned long)free_records.nrs_p, order);
    }
    else
    {
        hwpal_dma_resource_mem_free(free_handles.nrs_p);
        hwpal_dma_resource_mem_free(free_records.nrs_p);
        hwpal_dma_resource_mem_free(handles_p);
    }

    if (hwpal_lock_p != NULL)
        hwpal_dma_resource_lock_free(hwpal_lock_p);

    free_handles.nrs_p = NULL;
    free_records.nrs_p = NULL;
    handles_p = NULL;
    record_chunk_ptrs_p = NULL;

    handles_count = 0;
}


/*----------------------------------------------------------------------------
 * hwpal_dma_resource_check_and_register
 */
int
hwpal_dma_resource_check_and_register(
        const dma_resource_properties_t requested_properties,
        const dma_resource_addr_pair_t addr_pair,
        const char allocator_ref,
        dma_resource_handle_t * const handle_p)
{
    dma_resource_properties_t actual_properties = requested_properties;
    dma_resource_addr_pair_t * pair_p;
    dma_resource_record_t * rec_p;
    dma_resource_handle_t handle;
    dma_addr_t dma_addr = 0;

#ifdef HWPAL_TRACE_DMARESOURCE_BUF
    void* host_addr = NULL;
#endif

#ifdef HWPAL_DMARESOURCE_STRICT_ARGS_CHECKS
    if(allocator_ref != 'N' &&
       ((uintptr_t)addr_pair.address_p & (actual_properties.alignment - 1)) !=0)
    {
        // Warn against unaligned addresses, but do not fail.
        LOG_WARN("dma_resource_check_and_register: "
                 "address not aligned to %d\n",actual_properties.alignment);
        actual_properties.alignment = 1;
    }

    if (NULL == handle_p)
    {
        return -1;
    }

    if (!dma_resource_lib_is_sane_input(&addr_pair,
                                    &allocator_ref,
                                    &actual_properties))
    {
        return 1;
    }

    if (addr_pair.domain != DMARES_DOMAIN_HOST)
    {
        LOG_WARN(
            "hwpal_dma_resource_check_and_register: "
            "Unsupported domain: %u\n",
            addr_pair.domain);
        return 1;
    }
#endif

    if (allocator_ref != 'k' && allocator_ref != 'R' &&
        allocator_ref != 'N' && allocator_ref != 'C')
    {
        LOG_WARN(
            "hwpal_dma_resource_check_and_register: "
            "Unsupported allocator_ref: %c\n",
            allocator_ref);

        return 1;
    }

    // allocate record -> handle & rec_p
    handle = dma_resource_create_record();
    if (handle == NULL)
    {
        return -1;
    }

    rec_p = dma_resource_handle2_record_ptr(handle);
    if (rec_p == NULL)
    {
        return -1;
    }

#ifdef HWPAL_ARCH_COHERENT
    actual_properties.f_cached    = false;
#else
    actual_properties.f_cached    = requested_properties.f_cached;
#endif // HWPAL_ARCH_COHERENT

#ifdef HWPAL_DMARESOURCE_ALLOC_CACHE_COHERENT
    actual_properties.f_cached    = false;
#endif // HWPAL_DMARESOURCE_ALLOC_CACHE_COHERENT

    if (allocator_ref == 'k' &&
        ((uintptr_t)addr_pair.address_p &
          (hwpal_dma_resource_d_cache_alignment_get()-1)) != 0)
    {
        dma_resource_destroy_record(handle);
        LOG_CRIT("dma_resource_check_and_register: "
                 "address not aligned to cache line size %d\n",
                 hwpal_dma_resource_d_cache_alignment_get());
        return -1;
    }


    dma_resource_lib_setup_record(
            &actual_properties,
            allocator_ref,
            rec_p,
            actual_properties.size);

    pair_p = rec_p->addr_pairs;
    if (allocator_ref == 'k' || allocator_ref == 'R')
    {
        struct device * dma_device_p;

        // Get device reference for this resource
        dma_device_p = eip_device_get_reference(NULL, NULL);

#ifdef HWPAL_DMARESOURCE_MINIMUM_CACHE_CONTROL
        if (!is_kernel_addr ((unsigned long)addr_pair.address_p))
        {
            dma_resource_destroy_record(handle);
            LOG_WARN(
                "hwpal_dma_resource_check_and_register: "
                "Unsupported host address: 0x%p\n",
                addr_pair.address_p);

            return -1;
        }

        dma_addr = (dma_addr_t)virt_to_phys (addr_pair.address_p);
#else
        if (allocator_ref == 'k')
        {
            // Note: this function can create a bounce buffer!
            dma_addr = dma_map_single(dma_device_p,
                                     addr_pair.address_p,
                                     actual_properties.size,
                                     DMA_BIDIRECTIONAL);
            if (dma_mapping_error(dma_device_p, dma_addr))
            {
                LOG_WARN(
                    "hwpal_dma_resource_check_and_register: "
                    "Failed to map DMA address for host address 0x%p, "
                    "for handle 0x%p\n",
                    addr_pair.address_p,
                    handle);

                if (dma_addr)
                    kfree(addr_pair.address_p);

                return -1;
            }

            rec_p->props.f_cached = true; // always cached for kmalloc()

#ifdef HWPAL_TRACE_DMARESOURCE_BUF
            LOG_INFO("dma_resource_alloc: handle 0x%p, "
                     "bus address requested/adjusted 0x%p / 0x%p\n",
                     handle,
                     (void*)dma_addr,
                     (void*)(dma_addr - HWPAL_DMARESOURCE_BANK_STATIC_OFFSET));
#endif // HWPAL_TRACE_DMARESOURCE_BUF

            dma_addr -= HWPAL_DMARESOURCE_BANK_STATIC_OFFSET;
        }
        else if (allocator_ref == 'R')
        {
            dma_resource_record_t * parent_rec_p;
            dma_resource_addr_pair_t *parent_host_pair_p,*parent_bus_pair_p;
            u32 subset_offset;

            parent_rec_p = dma_resource_lib_find_matching_dma_resource(
                                                        &actual_properties,
                                                        addr_pair);
            if (parent_rec_p == NULL)
            {
                dma_addr = 0;
                LOG_CRIT("hwpal_dma_resource_check_and_register: "
                         "Failed to match DMA resource, "
                         "alignment/bank/size %d/%d/%d, host addr 0x%p\n",
                         actual_properties.alignment,
                         actual_properties.bank,
                         actual_properties.size,
                         addr_pair.address_p);
            }
            else
            {
                rec_p->props.f_cached = parent_rec_p->props.f_cached;
                rec_p->bank_type      = parent_rec_p->bank_type;

                parent_host_pair_p = dma_resource_lib_lookup_domain(
                                                           parent_rec_p,
                                                           DMARES_DOMAIN_HOST);

                parent_bus_pair_p = dma_resource_lib_lookup_domain(
                                                            parent_rec_p,
                                                            DMARES_DOMAIN_BUS);

                if (parent_host_pair_p == NULL || parent_bus_pair_p == NULL)
                {
                    dma_addr = 0;
                    LOG_CRIT("hwpal_dma_resource_check_and_register: "
                             "Failed to lookup parent DMA domain, "
                             "alignment/bank/size %d/%d/%d, "
                             "domain host/bus %p/%p\n",
                             actual_properties.alignment,
                             actual_properties.bank,
                             actual_properties.size,
                             parent_host_pair_p,
                             parent_bus_pair_p);
                }
                else
                {
                    subset_offset = (u32)((u8 *)addr_pair.address_p -
                                       (u8 *)parent_host_pair_p->address_p);
                    dma_addr = (dma_addr_t)(uintptr_t)parent_bus_pair_p->address_p +
                                                                subset_offset;
#ifdef HWPAL_TRACE_DMARESOURCE_BUF
                    LOG_INFO("hwpal_dma_resource_check_and_register: "
                             "Registered subset buffer, "
                             "parent bus addr 0x%p, child offset %u\n",
                             parent_bus_pair_p->address_p,
                             subset_offset);
#endif // HWPAL_TRACE_DMARESOURCE_BUF
                }
            }
        }
#endif // HWPAL_DMARESOURCE_MINIMUM_CACHE_CONTROL

        if (0 == dma_addr)
        {
            dma_resource_destroy_record(handle);
            LOG_CRIT("hwpal_dma_resource_check_and_register: "
                     "Failed to obtain DMA address for host address 0x%p, "
                     "for handle 0x%p\n",
                     addr_pair.address_p,
                     handle);

            return -1;
        }

        pair_p->address_p = (void *)(uintptr_t)dma_addr;
        pair_p->domain = DMARES_DOMAIN_BUS;

        ++pair_p;
        pair_p->address_p = addr_pair.address_p;
        pair_p->domain = DMARES_DOMAIN_HOST;

        // This host address will be used for freeing the allocated buffer
        ++pair_p;
        pair_p->address_p = addr_pair.address_p;
        pair_p->domain = DMARES_DOMAIN_HOST_UNALIGNED;

#ifdef HWPAL_TRACE_DMARESOURCE_BUF
        host_addr = addr_pair.address_p;
#endif
    }
    else if (allocator_ref == 'N' || allocator_ref == 'C')
    {
        pair_p->address_p = addr_pair.address_p;
        pair_p->domain = DMARES_DOMAIN_HOST;

        // This host address will be used for freeing the allocated buffer
        ++pair_p;
        pair_p->address_p = addr_pair.address_p;
        pair_p->domain = DMARES_DOMAIN_HOST_UNALIGNED;

#ifdef HWPAL_TRACE_DMARESOURCE_BUF
        host_addr = addr_pair.address_p;
        dma_addr = 0;
#endif
    }

#ifdef HWPAL_TRACE_DMARESOURCE_BUF
    LOG_INFO("hwpal_dma_resource_check_and_register (1/2): "
             "handle = 0x%p, allocator='%c', "
             "size allocated/requested=%d/%d, \n"
             "hwpal_dma_resource_check_and_register (2/2): "
             "alignment/bank/cached=%d/%d/%d, "
             "bus addr=0x%p, host addr=0x%p\n",
             handle, rec_p->allocator_ref,
             rec_p->buffer_size, rec_p->props.size,
             rec_p->props.alignment, rec_p->props.bank, rec_p->props.f_cached,
             (void*)dma_addr, host_addr);
#endif

    *handle_p = handle;
    return 0;
}


/*----------------------------------------------------------------------------
 * hwpal_dma_resource_record_update
 */
int
hwpal_dma_resource_record_update(
        const int identifier,
        dma_resource_record_t * const rec_p)
{
    IDENTIFIER_NOT_USED(identifier);
    IDENTIFIER_NOT_USED(rec_p);

    return 0; // Success, empty implementation
}

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
dma_resource_create_record(void)
{
    unsigned long flags;
    u32 handle_nr;
    u32 idx_pair = 0;

    // return NULL when not initialized
    if (handles_count == 0)
        return NULL;

    hwpal_dma_resource_lock_acquire(hwpal_lock_p, &flags);

    handle_nr = dma_resource_lib_free_list_get(&free_handles);
    if (handle_nr != HWPAL_INVALID_INDEX)
    {
        idx_pair = dma_resource_lib_free_list_get(&free_records);
        if (idx_pair == HWPAL_INVALID_INDEX)
        {
            dma_resource_lib_free_list_add(&free_handles, handle_nr);
            handle_nr = HWPAL_INVALID_INDEX;
        }
    }

    hwpal_dma_resource_lock_release(hwpal_lock_p, &flags);

    // return NULL when reservation failed
    if (handle_nr == HWPAL_INVALID_INDEX)
    {
        LOG_CRIT("DMAResource_Create_Record: "
                 "reservation failed, out of handles\n");
        return NULL;
    }

    // initialize the record
    {
        dma_resource_record_t * rec_p =
                dma_resource_lib_idx_pair2_record_ptr(idx_pair);
        if(rec_p == NULL)
        {
            LOG_CRIT(
                "DMAResource_Create_Record: "
                "Bad index pair returned %u\n",idx_pair);
            return NULL;
        }
        memset(rec_p, 0, sizeof(dma_resource_record_t));
        rec_p->magic = DMARES_RECORD_MAGIC;
    }

    // initialize the handle
    handles_p[handle_nr] = idx_pair;

    // fill in the handle position
    return handles_p + handle_nr;
}


/*----------------------------------------------------------------------------
 * dma_resource_destroy_record
 *
 * This function invalidates the handle and the record instance.
 *
 * handle
 *     A valid handle that was once returned by dma_resource_create_record or
 *     one of the DMA Buffer Management functions (Alloc/Register/Attach).
 *
 * Return Values
 *     None
 */
void
dma_resource_destroy_record(
        const dma_resource_handle_t handle)
{
    if (dma_resource_is_valid_handle(handle))
    {
        u32 * p = (u32 *)handle;
        u32 idx_pair = *p;

        if (dma_resource_lib_idx_pair2_record_ptr(idx_pair) != NULL)
        {
            unsigned long flags;
            int handle_nr = p - handles_p;

            // note handle is no longer value
            *p = HWPAL_INVALID_INDEX;

            hwpal_dma_resource_lock_acquire(hwpal_lock_p, &flags);

            // add the handle_nr and idx_pair to respective LRU lists
            dma_resource_lib_free_list_add(&free_handles, handle_nr);
            dma_resource_lib_free_list_add(&free_records, idx_pair);

            hwpal_dma_resource_lock_release(hwpal_lock_p, &flags);
        }
        else
        {
            LOG_WARN(
                "DMAResource_Destroy: "
                "handle %p was already destroyed\n",
                handle);
        }
    }
    else
    {
        LOG_WARN(
            "DMAResource_Destroy: "
            "Invalid handle %p\n",
            handle);
    }
}


/*----------------------------------------------------------------------------
 * dma_resource_is_valid_handle
 *
 * This function tells whether a handle is valid.
 *
 * handle
 *     A valid handle that was once returned by dma_resource_create_record or
 *     one of the DMA Buffer Management functions (Alloc/Register/Attach).
 *
 * Return value
 *     true   The handle is valid
 *     false  The handle is NOT valid
 */
bool
dma_resource_is_valid_handle(
        const dma_resource_handle_t handle)
{
    u32 * p = (u32 *)handle;

    if (p < handles_p ||
        p >= handles_p + handles_count)
    {
        return false;
    }

    // check that the handle has not been destroyed yet
    if (*p == HWPAL_INVALID_INDEX)
    {
        return false;
    }

    return true;
}


/*----------------------------------------------------------------------------
 * dma_resource_handle2_record_ptr
 *
 * This function can be used to get a pointer to the DMA resource record
 * (dma_resource_record_t) for the provided handle. The pointer is valid until
 * the record and handle are destroyed.
 *
 * handle
 *     A valid handle that was once returned by dma_resource_create_record or
 *     one of the DMA Buffer Management functions (Alloc/Register/Attach).
 *
 * Return value
 *     Pointer to the dma_resource_record_t memory for this handle.
 *     NULL is returned if the handle is invalid.
 */
dma_resource_record_t *
dma_resource_handle2_record_ptr(
        const dma_resource_handle_t handle)
{
    u32 * p = (u32 *)handle;

#ifdef HWPAL_DMARESOURCE_STRICT_ARGS_CHECKS
    if(!dma_resource_is_valid_handle(handle))
    {
        return NULL;
    }

    if (p != NULL)
    {
        u32 idx_pair = *p;

        dma_resource_record_t* rec_p =
                dma_resource_lib_idx_pair2_record_ptr(idx_pair);

        if(rec_p != NULL && rec_p->magic == DMARES_RECORD_MAGIC)
        {
            return rec_p; // ## RETURN ##
        }
        else
        {
            return NULL; // ## RETURN ##
        }
    }
#else
    return record_chunk_ptrs_p[CHUNK_OF(*p)] + RECIDX_OF(*p);
#endif

    return NULL;
}


/*----------------------------------------------------------------------------
 * dma_resource_pre_dma
 */
void
dma_resource_pre_dma(
        const dma_resource_handle_t handle,
        const unsigned int byte_offset,
        const unsigned int byte_count)
{
    dma_resource_record_t *rec_p;
    unsigned int n_bytes = byte_count;
    unsigned int offset = byte_offset;

    rec_p = dma_resource_handle2_record_ptr(handle);
    if (rec_p == NULL)
    {
        LOG_WARN(
            "dma_resource_pre_dma: "
            "Invalid handle %p\n",
            handle);
        return;
    }

    if (n_bytes == 0)
    {
        // Prepare the whole resource for the DMA operation
        n_bytes = rec_p->props.size;
        offset = 0;
    }

    if ((offset >= rec_p->props.size) ||
        (n_bytes > rec_p->props.size) ||
        (offset + n_bytes > rec_p->props.size))
    {
        LOG_CRIT(
            "dma_resource_pre_dma: "
            "Invalid range 0x%08x-0x%08x (not in 0x0-0x%08x)\n",
            byte_offset,
            byte_offset + byte_count,
            rec_p->props.size);
        return;
    }

    if (rec_p->props.f_cached &&
        (rec_p->allocator_ref == 'k' || rec_p->allocator_ref == 'A' ||
         rec_p->allocator_ref == 'R'))
    {
        dma_resource_addr_pair_t * pair_p;

#ifdef HWPAL_DMARESOURCE_MINIMUM_CACHE_CONTROL
        dma_addr_t dma_addr;

        pair_p = dma_resource_lib_lookup_domain(rec_p, DMARES_DOMAIN_HOST);
        if (pair_p == NULL)
        {
            LOG_WARN(
                "dma_resource_pre_dma: "
                "No host address found for handle %p?\n",
                handle);

            return;
        }

        dma_addr = (dma_addr_t)pair_p->address_p;
#else
        dma_addr_t dma_addr;

        pair_p = dma_resource_lib_lookup_domain(rec_p, DMARES_DOMAIN_BUS);
        if (pair_p == NULL)
        {
            LOG_WARN(
                "dma_resource_pre_dma: "
                "No bus address found for handle %p?\n",
                handle);

            return;
        }

        dma_addr = (dma_addr_t)(uintptr_t)pair_p->address_p;
#endif // HWPAL_DMARESOURCE_MINIMUM_CACHE_CONTROL
        IDENTIFIER_NOT_USED(dma_addr);

#ifdef HWPAL_TRACE_DMARESOURCE_PREPOSTDMA
        log_formatted_message(
            "dma_resource_pre_dma: "
            "handle=%p, "
            "offset=%u, size=%u, "
            "allocator='%c', "
            "addr=0x%p\n",
            handle,
            offset, n_bytes,
            rec_p->allocator_ref,
            (void*)dma_addr);
#endif

#ifndef HWPAL_ARCH_COHERENT
        {
            struct device * dma_device_p;
            size_t size = n_bytes;

            // Get device reference for this resource
            dma_device_p = eip_device_get_reference(NULL, NULL);

#ifdef HWPAL_DMARESOURCE_MINIMUM_CACHE_CONTROL
            {
                u8* dma_buffer_p = (u8*)dma_addr;

                dma_cache_sync(dma_device_p, dma_buffer_p + offset,
                               size, DMA_TO_DEVICE);
            }
#else
            dma_sync_single_range_for_device(dma_device_p, dma_addr,
                    offset, size, DMA_BIDIRECTIONAL);
#endif // HWPAL_DMARESOURCE_MINIMUM_CACHE_CONTROL
        }
#endif
    }
}


/*----------------------------------------------------------------------------
 * dma_resource_post_dma
 */
void
dma_resource_post_dma(
        const dma_resource_handle_t handle,
        const unsigned int byte_offset,
        const unsigned int byte_count)
{
    dma_resource_record_t *rec_p;
    unsigned int n_bytes = byte_count;
    unsigned int offset = byte_offset;

    rec_p = dma_resource_handle2_record_ptr(handle);
    if (rec_p == NULL)
    {
        LOG_WARN(
            "dma_resource_post_dma: "
            "Invalid handle %p\n",
            handle);
        return;
    }

    if (n_bytes == 0)
    {
        // Prepare the whole resource for the DMA operation
        n_bytes = rec_p->props.size;
        offset = 0;
    }

    if ((byte_offset >= rec_p->props.size) ||
        (n_bytes > rec_p->props.size) ||
        (byte_offset + n_bytes > rec_p->props.size))
    {
        LOG_CRIT(
            "dma_resource_post_dma: "
            "Invalid range 0x%08x-0x%08x (not in 0x0-0x%08x)\n",
            byte_offset,
            byte_offset + n_bytes,
            rec_p->props.size);
        return;
    }

    if (rec_p->props.f_cached &&
        (rec_p->allocator_ref == 'k' || rec_p->allocator_ref == 'A' ||
         rec_p->allocator_ref == 'R'))
    {
        dma_resource_addr_pair_t * pair_p;
#ifdef HWPAL_DMARESOURCE_MINIMUM_CACHE_CONTROL
        dma_addr_t dma_addr;

        pair_p = dma_resource_lib_lookup_domain(rec_p, DMARES_DOMAIN_HOST);
        if (pair_p == NULL)
        {
            LOG_WARN(
                "dma_resource_post_dma: "
                "No host address found for handle %p?\n",
                handle);

            return;
        }

        dma_addr = (dma_addr_t)pair_p->address_p;
#else
        dma_addr_t dma_addr;

        pair_p = dma_resource_lib_lookup_domain(rec_p, DMARES_DOMAIN_BUS);
        if (pair_p == NULL)
        {
            LOG_WARN(
                "dma_resource_post_dma: "
                "No bus address found for handle %p?\n",
                handle);

            return;
        }

        dma_addr = (dma_addr_t)(uintptr_t)pair_p->address_p;
#endif // HWPAL_DMARESOURCE_MINIMUM_CACHE_CONTROL

        IDENTIFIER_NOT_USED(dma_addr);
        IDENTIFIER_NOT_USED(offset);

#ifdef HWPAL_TRACE_DMARESOURCE_PREPOSTDMA
        log_formatted_message(
            "dma_resource_post_dma: "
            "handle=%p, "
            "offset=%u, size=%u), "
            "allocator='%c', "
            "addr 0x%p\n",
            handle,
            offset, n_bytes,
            rec_p->allocator_ref,
            (void*)dma_addr);
#endif

#ifndef HWPAL_ARCH_COHERENT
        {
            struct device * dma_device_p;
            size_t size = n_bytes;

            // Get device reference for this resource
            dma_device_p = eip_device_get_reference(NULL, NULL);

#ifdef HWPAL_DMARESOURCE_MINIMUM_CACHE_CONTROL
            {
                u8* dma_buffer_p = (u8*)dma_addr;

                dma_cache_sync(dma_device_p, dma_buffer_p + offset,
                               size, DMA_FROM_DEVICE);
            }
#else
            dma_sync_single_range_for_cpu(dma_device_p, dma_addr, offset,
                                          size, DMA_BIDIRECTIONAL);
#endif // HWPAL_DMARESOURCE_MINIMUM_CACHE_CONTROL
        }
#endif
    }
}


/*----------------------------------------------------------------------------
 * dma_resource_attach
 */
int
dma_resource_attach(
        const dma_resource_properties_t actual_properties,
        const dma_resource_addr_pair_t addr_pair,
        dma_resource_handle_t * const handle_p)
{
    IDENTIFIER_NOT_USED(handle_p);
    IDENTIFIER_NOT_USED(addr_pair.address_p);
    IDENTIFIER_NOT_USED(actual_properties.alignment);

    return -1; // Not implemented
}


/* end of file dmares_lkm.c */
