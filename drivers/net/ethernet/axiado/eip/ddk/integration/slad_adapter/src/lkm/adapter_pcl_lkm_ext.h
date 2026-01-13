/* adapter_pcl_lkm_ext.h
 *
 * PCL DTL API Linux kernel implementation extensions
 *
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

#ifndef ADAPTER_PCL_LKM_EXT_H_
#define ADAPTER_PCL_LKM_EXT_H_


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Linux Kernel API
#include <linux/init.h>     // module_init, module_exit
#include <linux/module.h>   // EXPORT_SYMBOL


/*----------------------------------------------------------------------------
 * This module implements (provides) the following interface(s):
 */

#include "api_pcl.h"        // PCL API

#include "api_pcl_dtl.h"    // PCL DTL API


EXPORT_SYMBOL(pcl_init);
EXPORT_SYMBOL(pcl_uninit);
EXPORT_SYMBOL(pcl_flow_dma_buf_handle_get);
EXPORT_SYMBOL(pcl_flow_hash);
EXPORT_SYMBOL(pcl_flow_alloc);
EXPORT_SYMBOL(pcl_flow_add);
EXPORT_SYMBOL(pcl_flow_remove);
EXPORT_SYMBOL(pcl_flow_release);
EXPORT_SYMBOL(pcl_flow_get_read_only);
EXPORT_SYMBOL(pcl_transform_register);
EXPORT_SYMBOL(pcl_transform_un_register);
EXPORT_SYMBOL(pcl_transform_get_read_only);

EXPORT_SYMBOL(pcl_dtl_init);
EXPORT_SYMBOL(pcl_dtl_uninit);
EXPORT_SYMBOL(pcl_dtl_transform_add);
EXPORT_SYMBOL(pcl_dtl_transform_remove);
EXPORT_SYMBOL(pcl_dtl_hash_remove);


#endif // ADAPTER_PCL_LKM_EXT_H_


/* end of file adapter_pcl_lkm_ext.h */
