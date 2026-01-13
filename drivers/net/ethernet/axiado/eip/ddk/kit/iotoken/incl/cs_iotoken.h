/* cs_iotoken.h
 *
 * Top-level configuration file
 */

/*****************************************************************************
* Copyright (c) 2016-2022 by Rambus, Inc. and/or its subsidiaries.
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

#ifndef CS_IOTOKEN_H_
#define CS_IOTOKEN_H_

/*----------------------------------------------------------------------------
 * This module implements (provides) the following interface(s):
 */
#include "cs_ddk197.h"

/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */
// Enable extended errors
#ifdef DDK_EIP197_EXTENDED_ERRORS_ENABLE
#define IOTOKEN_EXTENDED_ERRORS_ENABLE
#endif


/*----------------------------------------------------------------------------
 * Definitions and macros
 */

// Enable strict argument checking (input parameters)
#define IOTOKEN_STRICT_ARGS

#ifdef DDK_EIP197_FW_IOTOKEN_METADATA_ENABLE
// number of 32-bit words passed from Input to Output Token
// without modifications
#define IOTOKEN_BYPASS_WORD_COUNT        4
#else
#define IOTOKEN_BYPASS_WORD_COUNT        0
#endif

/* end of file cs_iotoken.h */


#endif /* CS_IOTOKEN_H_ */
