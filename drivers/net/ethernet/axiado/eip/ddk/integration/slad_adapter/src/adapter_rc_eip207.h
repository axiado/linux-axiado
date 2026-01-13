/* adapter_rc_eip207.h
 *
 * Interface to Security-IP-207 Record Cache functionality.
 */

/*****************************************************************************
* Copyright (c) 2011-2020 by Rambus, Inc. and/or its subsidiaries.
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

#ifndef ADAPTER_RC_EIP207_H_
#define ADAPTER_RC_EIP207_H_


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Driver Framework Basic Definitions API
#include "basic_defs.h"         // bool


/*----------------------------------------------------------------------------
 * adapter_rc_eip207_configure
 *
 * This routine configures the Security-IP-207 Record Cache functionality
 * with EIP-207 HW parameters that can be obtained via the HW datasheet or
 * the Security-IP-207 Global Control interface, for example.
 *
 * f_enabled_trc (input)
 *         True when the EIP-207 Transform Record Cache is enabled
 *
 * f_enabled_arc4_rc (input)
 *         True when the EIP-207 arc4 Record Cache is enabled
 *
 * f_combined (input)
 *         True when the EIP-207 trc and arc4_rc are combined
 *
 * This function is re-entrant.
 */
void
adapter_rc_eip207_configure(
        const bool f_enabled_trc,
        const bool f_enabled_arc4_rc,
        const bool f_combined);


#endif /* ADAPTER_RC_EIP207_H_ */


/* end of file adapter_rc_eip207.h */
