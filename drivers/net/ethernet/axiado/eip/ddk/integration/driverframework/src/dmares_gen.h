/* dmares_gen.h
 *
 * Declare functions exported by "dmares_gen.c" that implements large parts
 * of the DMAResource API. The exported functions are to be used by module(s)
 * that implement the remaining parts of the DMAResource API.
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

#ifndef DMARES_GEN_H_
#define DMARES_GEN_H_


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Driver Framework Basic Definitions API
#include "basic_defs.h"         // bool

// Driver Framework DMAResource Types API
#include "dmares_types.h"


/*----------------------------------------------------------------------------
 * Definitions and macros
 */


/*----------------------------------------------------------------------------
 * dma_resource_lib_is_sane_input
 */
bool
dma_resource_lib_is_sane_input(
        const dma_resource_addr_pair_t * addr_pair_p,
        const char * const allocator_ref_p,
        const dma_resource_properties_t * props_p);


/*----------------------------------------------------------------------------
 * dma_resource_lib_align_for_size
 */
unsigned int
dma_resource_lib_align_for_size(
        const unsigned int byte_count,
        const unsigned int align_to);


/*----------------------------------------------------------------------------
 * dma_resource_lib_align_for_address
 */
unsigned int
dma_resource_lib_align_for_address(
        const unsigned int byte_count,
        const unsigned int align_to);


/*----------------------------------------------------------------------------
 * dma_resource_lib_lookup_domain
 */
dma_resource_addr_pair_t *
dma_resource_lib_lookup_domain(
        const dma_resource_record_t * rec_p,
        const dma_resource_addr_domain_t domain);


#endif // DMARES_GEN_H_


/* end of file dmares_gen.h */
