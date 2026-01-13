/* dmares_addr.h
 *
 * Driver Framework, DMAResource API, address Translation functions
 * Translates an {address + domain} to an address in a requested domain.
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

#ifndef INCLUDE_GUARD_DMARES_ADDR_H
#define INCLUDE_GUARD_DMARES_ADDR_H

#include "dmares_types.h"       // DMAResource_AddrDomain/addr_pair/Handle_t

/*----------------------------------------------------------------------------
 * dma_resource_translate
 *
 * Attempts to provide an address for a DMA Resource that can be used in the
 * requested address domain. Typically used to get the address that can be
 * used by a device to perform DMA.
 *
 * handle (input)
 *     The handle to the DMA Resource record that was returned by
 *     dma_resource_alloc, dma_resource_check_and_register or dma_resource_attach.
 *
 * dest_domain (input)
 *     The requested domain to translate the address to.
 *     Please check the implementation notes for supported domains.
 *
 * pair_out_p (output)
 *     Pointer to the memory location when the converted address plus domain
 *     will be written.
 *
 * Return Values
 *     0    Success
 *     <0   error code (implementation dependent)
 */
int
dma_resource_translate(
        const dma_resource_handle_t handle,
        const dma_resource_addr_domain_t dest_domain,
        dma_resource_addr_pair_t * const pair_out_p);


/*----------------------------------------------------------------------------
 * dma_resource_add_pair
 *
 * This function can be used to register another address pair known to the
 * caller for a DMA Resource.
 * The information will be stored in the DMA Resource Record and can be used
 * by dma_resource_translate.
 * Typically used when an external DMA-safe buffer allocator returns two
 * addresses (for example virtual and physical).
 * Note: How many address pairs are supported is implementation specific.
 *
 * handle (input)
 *     The handle to the DMA Resource record that was returned by
 *     dma_resource_check_and_register or dma_resource_attach.
 *
 * pair (input)
 *     address pair (address + domain) to be associated with the DMA Resource.
 *
 * Return Values
 *     0    Success
 *     <0   error code (implementation dependent)
 */
int
dma_resource_add_pair(
        const dma_resource_handle_t handle,
        const dma_resource_addr_pair_t pair);


#endif /* Include Guard */

/* end of file dmares_addr.h */
