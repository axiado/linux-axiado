/* dmares_types.h
 *
 * Driver Framework, DMAResource API, type Definitions
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

#ifndef INCLUDE_GUARD_DMARES_TYPES_H
#define INCLUDE_GUARD_DMARES_TYPES_H

#include "basic_defs.h"         // bool, u8, u32, inline


/*----------------------------------------------------------------------------
 * dma_resource_handle_t
 *
 * This handle represents a DMA Resource Record that holds information about
 * a memory resource that can be accessed by a device using DMA, from another
 * memory domain in the same host or from another host (CPU/DSP).
 */

typedef void * dma_resource_handle_t;

typedef unsigned int dma_resource_addr_domain_t;


/*----------------------------------------------------------------------------
 * dma_resource_addr_pair_t
 *
 * This type describes a dynamic resource address coupled with its memory
 * domain. The caller is encouraged to store the address with the domain
 * information.
 * The use of 'void *' for the address_p avoids unsafe void-pointer function
 * output parameters in 99% of all cases. However, in some odd cases the
 * address_p part needs to be adapted and that is why domain must be located
 * first in the struct. It also guarantees that address_p part starts always
 * at the same offset.
 */
typedef struct
{
    dma_resource_addr_domain_t domain;    // Defines domain of address_p
    void * address_p;                   // type ensures 64-bit support
} dma_resource_addr_pair_t;


/*----------------------------------------------------------------------------
 * dma_resource_properties_t
 *
 * Buffer properties. When allocating a buffer, these are the requested
 * properties for the buffer. When registering or attaching to an externally
 * allocated buffer these properties describe the already allocated buffer.
 * The exact fields and values supported is implementation specific.
 *
 * For both uses, the data structure should be initialized to all zeros
 * before filling in some or all of the fields. This ensures forward
 * compatibility in case this structure is extended with more fields.
 *
 * Example usage:
 *     dma_resource_properties_t props = {0};
 *     props.fIsCached = true;
 */
typedef struct
{
    u32 size;       // size of the buffer in bytes
    int alignment;       // buffer start address alignment
                         // examples: 4 for 32bit
    u8 bank;        // can be used to indicate on-chip memory
    bool f_cached;        // true = SW needs to control the coherency management
} dma_resource_properties_t;


/*----------------------------------------------------------------------------
 * dma_resource_record_t
 *
 * This type is the record that describes a DMAResource. The details of the
 * type are implementation specific and therefore in a separate header file.
 *
 * Several DMAResource API functions return a handle to a newly instantiated
 * record. Use dma_resource_handle2_record_ptr to get a pointer to the actual
 * record and to access fields in the record.
 */

#include "dmares_record.h"   // dma_resource_record_t definition


#endif /* Include Guard */

/* end of file dmares_types.h */
