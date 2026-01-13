/* c_adapter_firmware.h
 *
 * Default Firmware Adapter Module configuration
 */

/*****************************************************************************
* Copyright (c) 2016-2020 by Rambus, Inc. and/or its subsidiaries.
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

#ifndef C_ADAPTER_FIRMWARE_H_
#define C_ADAPTER_FIRMWARE_H_

/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Top-level Driver Framework configuration
#include "cs_adapter.h"


/*----------------------------------------------------------------------------
 * Definitions and macros
 */


#ifndef ADAPTER_FIRMWARE_NAMELEN_MAX
#define ADAPTER_FIRMWARE_NAMELEN_MAX  128
#endif

#ifndef ADAPTER_FIRMWARE_PATH_PREFIX
#define ADAPTER_FIRMWARE_PATH_PREFIX  "/lib/firmware/"
#endif


#endif /* C_ADAPTER_MEMXS_H_ */


/* end of file c_adapter_firmware.h */
