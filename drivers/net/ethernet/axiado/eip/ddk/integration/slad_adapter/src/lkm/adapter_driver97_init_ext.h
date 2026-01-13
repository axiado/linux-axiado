/* adapter_driver97_init_ext.h
 *
 * Linux kernel specific Adapter extensions
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


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Top-level Adapter configuration
#include "cs_adapter.h"

// Linux Kernel API
#include <linux/init.h>     // module_init, module_exit
#include <linux/module.h>   // EXPORT_SYMBOL


/*----------------------------------------------------------------------------
 * This module implements (provides) the following interface(s):
 */

#include "api_dmabuf.h"     // DMABuf API


MODULE_LICENSE(ADAPTER_LICENSE);

module_init(driver97_init);
module_exit(driver97_exit);

EXPORT_SYMBOL(dma_buf_null_handle);
EXPORT_SYMBOL(dma_buf_handle_is_same);
EXPORT_SYMBOL(dma_buf_alloc);
EXPORT_SYMBOL(dma_buf_register);
EXPORT_SYMBOL(dma_buf_release);


// PEC API LKM implementation extensions
#include "adapter_pec_lkm_ext.h"


/* end of file adapter_driver97_init_ext.h */
