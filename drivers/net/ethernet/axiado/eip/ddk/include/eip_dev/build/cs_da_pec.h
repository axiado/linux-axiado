/* cs_da_pec.h
 *
 * Demo Application Top-level configuration
 */

/*****************************************************************************
* Copyright (c) 2011-2022 by Rambus, Inc. and/or its subsidiaries.
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

#ifndef CS_DA_PEC_H_
#define CS_DA_PEC_H_

/*----------------------------------------------------------------------------
* This module uses (requires) the following interface(s):
 */


// platform-specific conf parameters
#include "cs_da_pec_ext.h"

// Engine-specific conf parameters
#include "cs_da_pec_ext2.h"

// Logging levels
#ifndef LOG_SEVERITY_MAX
#define LOG_SEVERITY_MAX LOG_SEVERITY_WARN
#endif
#include "log.h"

// enable gather-usage for AES-GCM
//#define DA_PEC_GATHER

//Do we need to use invalidate commands for SA records?
#define DA_PEC_USE_INVALIDATE_COMMANDS

#ifdef DDK_EIP197_FW33_FEATURES
#define DA_PEC_HAVE_HW_PRECOMPUTE
#endif

#ifdef DDK_EIP197_FW36_FEATURES
#define DA_PEC_HMAC_CONT
#define DA_PEC_GCM_CONT
#endif

#endif // CS_DA_PEC_H_


/* end of file cs_da_pec.h */
