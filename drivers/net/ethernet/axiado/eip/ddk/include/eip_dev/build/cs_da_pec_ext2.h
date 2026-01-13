/* cs_da_pec_ext2.h
 *
 * Demo Application Top-level configuration, Engine-specific extensions
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

#ifndef CS_DA_PEC_EXT2_H_
#define CS_DA_PEC_EXT2_H_


/*----------------------------------------------------------------------------
* This module uses (requires) the following interface(s):
 */

// Top-level product configuration
#include "cs_ddk197.h"


#if defined(DDK_EIP197_SRV_ICE)
// Enable Extended IOToken API implementation for Server with ice
#define DA_PEC_IOTOKEN_EXT_SRV_ICE
#elif defined(DDK_EIP197_SRV_ICEOCE)
// Enable Extended IOToken API implementation for Server with ice and oce
#define DA_PEC_IOTOKEN_EXT_SRV_ICEOCE
#else
#error "Unknown IOToken type"
#endif


#endif // CS_DA_PEC_EXT2_H_


/* end of file cs_da_pec_ext2.h */
