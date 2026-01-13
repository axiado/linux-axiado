/* c_adapter_global.h
 *
 * Default Adapter Global configuration
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

#ifndef INCLUDE_GUARD_C_ADAPTER_GLOBAL_H
#define INCLUDE_GUARD_C_ADAPTER_GLOBAL_H

/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Top-level Adapter configuration
#include "cs_adapter.h"

#ifndef ADAPTER_GLOBAL_DRIVER_NAME
#define ADAPTER_GLOBAL_DRIVER_NAME     "Security"
#endif

#ifndef ADAPTER_GLOBAL_LICENSE
#define ADAPTER_GLOBAL_LICENSE         "Dual BSD/GPL"
#endif

#ifndef ADAPTER_GLOBAL_PRNG_SEED
// 8 words to seed the PRNG to provide IVs, input to
#define ADAPTER_GLOBAL_PRNG_SEED {0x48c24cfd, 0x6c07f742, \
                                  0xaee75681, 0x0f27c239, \
                                  0x79947198, 0xe2991275, \
                                  0x21ac3c7c, 0xd008c4b4}
#endif

#ifndef ADAPTER_GLOBAL_DEVICE_NAME
#define ADAPTER_GLOBAL_DEVICE_NAME           "EIP197_GLOBAL"
#endif // ADAPTER_GLOBAL_DEVICE_NAME

#ifndef ADAPTER_GLOBAL_RESET_MAX_RETRIES
#define ADAPTER_GLOBAL_RESET_MAX_RETRIES      1000
#endif // ADAPTER_GLOBAL_RESET_MAX_RETRIES

#ifndef ADAPTER_GLOBAL_EIP97_NOF_PES
#define ADAPTER_GLOBAL_EIP97_NOF_PES          1
#endif

#ifndef ADAPTER_GLOBAL_EIP97_RINGMASK
#define ADAPTER_GLOBAL_EIP97_RINGMASK         0x0001
#endif

#ifndef ADAPTER_GLOBAL_EIP97_PRIOMASK
#define ADAPTER_GLOBAL_EIP97_PRIOMASK         0
#endif

// Enables board control device support
//#define ADAPTER_GLOBAL_BOARDCTRL_SUPPORT_ENABLE

// Enables board reset via the board control device
//#define ADAPTER_GLOBAL_FPGA_HW_RESET_ENABLE

// Runtime power Management EIP-97 device identifier
#ifndef ADAPTER_GLOBAL_RPM_EIP97_DEVICE_ID
#define ADAPTER_GLOBAL_RPM_EIP97_DEVICE_ID  0
#endif

#ifndef ADAPTER_GLOBAL_RPM_EIP207_DEVICE_ID
#define ADAPTER_GLOBAL_RPM_EIP207_DEVICE_ID 0
#endif

// Specify a redirect ring.
#define ADAPTER_CS_GLOBAL_REDIR_RING 0


#endif /* Include Guard */


/* end of file c_adapter_global.h */
