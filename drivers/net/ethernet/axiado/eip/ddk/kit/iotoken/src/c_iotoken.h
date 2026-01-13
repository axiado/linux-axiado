/* c_iotoken.h
 *
 * Default configuration file
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

#ifndef C_IOTOKEN_H_
#define C_IOTOKEN_H_

/*----------------------------------------------------------------------------
 * This module implements (provides) the following interface(s):
 */


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Top-level configuration
#include "cs_iotoken.h"


/*----------------------------------------------------------------------------
 * Definitions and macros
 */

// Enable extended errors
//#define IOTOKEN_EXTENDED_ERRORS_ENABLE

// Enable strict argument checking (input parameters)

//#define IOTOKEN_STRICT_ARGS

// number of 32-bit words passed from Input to Output Token
// without modifications
#ifndef IOTOKEN_BYPASS_WORD_COUNT
#define IOTOKEN_BYPASS_WORD_COUNT        0
#endif

#ifndef LOG_SEVERITY_MAX
#define LOG_SEVERITY_MAX LOG_SEVERITY_CRIT
#endif


// Enable PKt ID increment
//#IOTOKEN_INCREMENT_PKTID

#ifndef IOTOKEN_PADDING_DEFAULT_ON
#define IOTOKEN_PADDING_DEFAULT_ON 0
#endif

// Header Align for inbound DTLS
#ifndef IOTOKEN_DTLS_HDR_ALIGN
#define IOTOKEN_DTLS_HDR_ALIGN 0
#endif


/* end of file c_iotoken.h */


#endif /* C_IOTOKEN_H_ */
