/* dmares_record.h
 *
 * Driver Framework, DMAResource Record Definition
 *
 * The document "Driver Framework Porting Guide" contains the detailed
 * specification of this API. The information contained in this header file
 * is for reference only.
 */

/*****************************************************************************
* Copyright (c) 2010-2022 by Rambus, Inc. and/or its subsidiaries.
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

#ifndef INCLUDE_GUARD_DMARES_REC_H
#define INCLUDE_GUARD_DMARES_REC_H


#include "cs_adapter.h"     // enable switches for conditional fields
#include "basic_defs.h"     // u8, u32

/*----------------------------------------------------------------------------
 * addr_trans_domain_t
 *
 * This is a list of domains that can be supported by the implementation.
 * Can be used with variables of the dma_resource_addr_domain_t type
 *
 */
typedef enum
{
    DMARES_DOMAIN_UNKNOWN = 0,
    DMARES_DOMAIN_HOST,
    DMARES_DOMAIN_HOST_UNALIGNED,
    DMARES_DOMAIN_BUS
} addr_trans_domain_t;


// Maximum number of address/domain pairs stored per DMA resource.
#define DMARES_ADDRPAIRS_CAPACITY 3

typedef struct
{
    u32 magic;     // signature used to validate handles

    // DMA resource properties: size, alignment, bank & f_cached
    dma_resource_properties_t props;

    // Storage for upto N address/domain pairs.
    dma_resource_addr_pair_t addr_pairs[DMARES_ADDRPAIRS_CAPACITY];

    // if true, 32-bit words are swapped when transferred to/from
    // the DMA resource
    bool f_swap_endianness;

    // this implementation supports the following allocator references:
    // 'A' -> this DMA resource has been obtained through dma_resource_alloc
    // 'R' -> this DMA resource has been obtained through
    //        dma_resource_check_and_register using Linux DMA API
    // 'k' -> this DMA resource has been obtained through
    //        dma_resource_check_and_register using Linux kmalloc() allocator
    // 'N' -> used to register buffers that do not need to be DMA-safe
    // 'T' -> this DMA resource has been obtained through dma_resource_attach
    char allocator_ref;

    // maximum data amount that can be stored in bytes, e.g. allocated size
    unsigned int buffer_size;

#ifndef ADAPTER_REMOVE_BOUNCEBUFFERS
    struct
    {
        // bounce buffer for dma_resource_check_and_register'ed buffers
        // note: used only when concurrency is impossible
        //       (PE source packets allow concurrency!!)
        dma_resource_handle_t bounce_handle;
    } bounce;
#endif

#ifdef ADAPTER_PEC_ENABLE_SCATTERGATHER
    struct
    {
        // for secure detection of PEC_SGList handles
        bool is_sg_list;
    } sg;
#endif

    bool f_is_new_sa;

#ifndef ADAPTER_USE_LARGE_TRANSFORM_DISABLE
    bool f_is_large_transform;
#endif

    // Implementation-specific DMA resource context
    void * context_p;

    // type of DMA bank
    unsigned int bank_type;

} dma_resource_record_t;

#define DMARES_RECORD_MAGIC 0xde42b5e7


#endif // INCLUDE_GUARD_DMARES_REC_H


/* end of file dmares_record.h */
