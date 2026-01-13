/* adapter_ring_eip202.h
 *
 * Interface to EIP-202 ring-specific functionality.
 */

/*****************************************************************************
* Copyright (c) 2011-2023 by Rambus, Inc. and/or its subsidiaries.
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

#ifndef ADAPTER_RING_EIP202_H_
#define ADAPTER_RING_EIP202_H_


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Driver Framework Basic Definitions API
#include "basic_defs.h"         // bool


/*----------------------------------------------------------------------------
 * adapter_ring_eip202_configure
 *
 * This routine configures the Security-IP-202 ring-specific functionality
 * with parameters that are obtained via the Global Control
 * interface.
 *
 * host_data_width (input)
 *         Host interface data width:
 *              0 = 32 bits, 1 = 64 bits, 2 = 128 bits, 3 = 256 bits
 *
 * cf_size (input)
 *         command Descriptor fifo size, the actual size is 2^cf_size 32-bit
 *         words.
 *
 * rf_size (input)
 *         Result Descriptor fifo size, the actual size is 2^rf_size 32-bit
 *         words.
 *
 * nof_p_es (input)
 *         number of packet engines in the system.
 *
 * major_version (input)
 *         major version of the EIP202.
 *
 * minor_version (input)
 *         minor version of the EIP202.
 *
 * patch_level (input)
 *         Patch level of the EIP202.
 *
 * This function is re-entrant.
 */
void
adapter_ring_eip202_configure(
        const u8 host_data_width,
        const u8 cf_size,
        const u8 rf_size,
        const u8 nof_p_es,
        const u8 major_version,
        const u8 minor_version,
        const u8 patch_level);


#endif /* ADAPTER_RING_EIP202_H_ */


/* end of file adapter_ring_eip202.h */
