/* dmares_hwpal.h
 *
 * HW and OS abstraction API
 * for the Driver Framework DMAResource API implementation
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

#ifndef DMARES_HWPAL_H_
#define DMARES_HWPAL_H_


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Driver Framework Basic Definitions API
#include "basic_defs.h"         // u32, NULL, inline, bool

// Driver Framework DMAResource Types API
#include "dmares_types.h"

// Driver Framework C Library abstraction API
#include "clib.h"


/*----------------------------------------------------------------------------
 * Definitions and macros
 */

// Dynamic flag cannot be mixed with any other HWPAL_DMARESOURCE_BANK_* flags
#define HWPAL_DMARESOURCE_BANK_DYNAMIC                  0

// Maximum number of supported dynamic DMA banks
#define HWPAL_DMARESOURCE_MAX_NOF_BANKS                 30

// Static flags can be mixed with each other for the same bank
#define HWPAL_DMARESOURCE_BANK_STATIC                   (HWPAL_DMARESOURCE_MAX_NOF_BANKS + 1)
#define HWPAL_DMARESOURCE_BANK_STATIC_FIXED_ADDR        (HWPAL_DMARESOURCE_MAX_NOF_BANKS + 3)

#define HWPAL_DMARESOURCE_BANK_STATIC_LAST              HWPAL_DMARESOURCE_BANK_STATIC_FIXED_ADDR


// internal dma_resource_properties_t extension
typedef struct
{
    void * addr;
    unsigned int bank_type;
} hwpal_dma_resource_properties_ext_t;


/*----------------------------------------------------------------------------
 * hwpal_dma_resource_max_alignment_get
 */
unsigned int
hwpal_dma_resource_max_alignment_get(void);


/*----------------------------------------------------------------------------
 * hwpal_dma_resource_d_cache_alignment_get
 */
unsigned int
hwpal_dma_resource_d_cache_alignment_get(void);


/*----------------------------------------------------------------------------
 * hwpal_dma_resource_mem_alloc
 */
void *
hwpal_dma_resource_mem_alloc(
        size_t byte_count);


/*----------------------------------------------------------------------------
 * hwpal_dma_resource_mem_free
 */
void
hwpal_dma_resource_mem_free(
        void * buf_p);


/*----------------------------------------------------------------------------
 * hwpal_dma_resource_lock_alloc
 */
void *
hwpal_dma_resource_lock_alloc(void);


/*----------------------------------------------------------------------------
 * hwpal_dma_resource_lock_free
 */
void
hwpal_dma_resource_lock_free(void * lock_p);


/*----------------------------------------------------------------------------
 * hwpal_dma_resource_lock_acquire
 */
void
hwpal_dma_resource_lock_acquire(
        void * lock_p,
        unsigned long * flags);


/*----------------------------------------------------------------------------
 * hwpal_dma_resource_lock_release
 */
void
hwpal_dma_resource_lock_release(
        void * lock_p,
        unsigned long * flags);


/*----------------------------------------------------------------------------
 * hwpal_dma_resource_alloc
 */
int
hwpal_dma_resource_alloc(
        const dma_resource_properties_t requested_properties,
        const hwpal_dma_resource_properties_ext_t requested_properties_ext,
        dma_resource_addr_pair_t * const addr_pair_p,
        dma_resource_handle_t * const handle_p);


/*----------------------------------------------------------------------------
 * hwpal_dma_resource_release
 */
int
hwpal_dma_resource_release(
        const dma_resource_handle_t handle);


/*----------------------------------------------------------------------------
 * hwpal_dma_resource_init
 */
bool
hwpal_dma_resource_init(void);


/*----------------------------------------------------------------------------
 * hwpal_dma_resource_uninit
 */
void
hwpal_dma_resource_uninit(void);


/*----------------------------------------------------------------------------
 * hwpal_dma_resource_check_and_register
 */
int
hwpal_dma_resource_check_and_register(
        const dma_resource_properties_t requested_properties,
        const dma_resource_addr_pair_t addr_pair,
        const char allocator_ref,
        dma_resource_handle_t * const handle_p);


/*----------------------------------------------------------------------------
 * hwpal_dma_resource_record_update
 */
int
hwpal_dma_resource_record_update(
        const int identifier,
        dma_resource_record_t * const rec_p);


#endif // DMARES_HWPAL_H_


/* end of file dmares_hwpal.h */
