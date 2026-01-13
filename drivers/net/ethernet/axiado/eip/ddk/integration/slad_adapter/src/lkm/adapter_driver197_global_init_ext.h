/* adapter_driver197_global_init_ext.h
 *
 * Linux kernel specific Adapter Global Control extensions
 */

/*****************************************************************************
* Copyright (c) 2012-2022 by Rambus, Inc. and/or its subsidiaries.
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


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Top-level Adapter configuration
#include "c_adapter_global.h"   // ADAPTER_GLOBAL_LICENSE

// Linux Kernel API
#include <linux/init.h>     // module_init, module_exit
#include <linux/module.h>   // EXPORT_SYMBOL


/*----------------------------------------------------------------------------
 * This module implements (provides) the following interface(s):
 */

// Global Classification Control Init API
#include "api_global_eip207.h"

// Global Classification Control status API
#include "api_global_status_eip207.h"

// Global Packet I/O Control Init API
#include "api_global_eip97.h"

// Global Packet I/O Control status API
#include "api_global_status_eip97.h"

// Global DRBG Control API
#include "api_global_eip74.h"

MODULE_LICENSE(ADAPTER_GLOBAL_LICENSE);

// Global Classification Control Init API
EXPORT_SYMBOL(global_control207_capabilities_get);
EXPORT_SYMBOL(global_control207_init);
EXPORT_SYMBOL(global_control207_uninit);

// Global Packet I/O Control status API
EXPORT_SYMBOL(global_control97_capabilities_get);
EXPORT_SYMBOL(global_control97_init);
EXPORT_SYMBOL(global_control97_uninit);
EXPORT_SYMBOL(global_control97_configure);

// Global Classification Control status API
EXPORT_SYMBOL(global_control207_status_get);
EXPORT_SYMBOL(global_control207_global_stats_get);
EXPORT_SYMBOL(global_control207_clock_count_get);
EXPORT_SYMBOL(global_control207_firmware_configure);

// Global Packet I/O Control status API
EXPORT_SYMBOL(global_control97_debug_statistics_get);
EXPORT_SYMBOL(global_control97_dfe_status_get);
EXPORT_SYMBOL(global_control97_dse_status_get);
EXPORT_SYMBOL(global_control97_token_status_get);
EXPORT_SYMBOL(global_control97_context_status_get);
EXPORT_SYMBOL(global_control97_interrupt_status_get);
EXPORT_SYMBOL(global_control97_out_xfer_status_get);
EXPORT_SYMBOL(global_control97_prng_status_get);
EXPORT_SYMBOL(global_control97_prng_reseed);
EXPORT_SYMBOL(global_control97_interfaces_get);

// Global DRBG Control API
EXPORT_SYMBOL(global_control74_init);
EXPORT_SYMBOL(global_control74_uninit);
EXPORT_SYMBOL(global_control74_capabilities_get);
EXPORT_SYMBOL(global_control74_status_get);
EXPORT_SYMBOL(global_control74_reseed);
EXPORT_SYMBOL(global_control74_clear);
#ifdef ADAPTER_EIP74_INTERRUPTS_ENABLE
EXPORT_SYMBOL(global_control74_notify_request);
#endif

module_init(driver197_global_init);
module_exit(driver197_global_exit);

/* end of file adapter_driver197_init_ext.h */
