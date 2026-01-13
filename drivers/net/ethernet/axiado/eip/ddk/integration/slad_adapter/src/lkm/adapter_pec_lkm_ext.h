/* adapter_pec_lkm_ext.h
 *
 * PEC API implementation,
 * Linux kernel specific extensions
 *
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

#ifndef ADAPTER_PEC_LKM_EXT_H_
#define ADAPTER_PEC_LKM_EXT_H_


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

#include "api_pec.h"        // PEC API

#ifdef ADAPTER_PEC_ENABLE_SCATTERGATHER
#include "api_pec_sg.h"     // PEC sg API
#endif /* ADAPTER_PEC_ENABLE_SCATTERGATHER */

#include "iotoken.h"        // IOToken API

#ifdef ADAPTER_AUTO_TOKENBUILDER
#include "token_builder.h"
#include "sa_builder.h"
#include "sa_builder_ipsec.h"
#include "sa_builder_ssltls.h"
#include "sa_builder_basic.h"
#include "sa_builder_srtp.h"
#endif

// PEC API
EXPORT_SYMBOL(pec_capabilities_get);
EXPORT_SYMBOL(pec_init);
EXPORT_SYMBOL(pec_uninit);
EXPORT_SYMBOL(pec_sa_register);
EXPORT_SYMBOL(pec_sa_un_register);
EXPORT_SYMBOL(pec_packet_put);
EXPORT_SYMBOL(pec_packet_get);
EXPORT_SYMBOL(pec_scatter_preload);
EXPORT_SYMBOL(pec_command_notify_request);
EXPORT_SYMBOL(pec_result_notify_request);
EXPORT_SYMBOL(pec_cd_control_write);
EXPORT_SYMBOL(pec_rd_status_read);
EXPORT_SYMBOL(pec_put_dump);
EXPORT_SYMBOL(pec_get_dump);

#ifdef ADAPTER_PEC_ENABLE_SCATTERGATHER
EXPORT_SYMBOL(pec_sg_list_create);
EXPORT_SYMBOL(pec_sg_list_destroy);
EXPORT_SYMBOL(pec_sg_list_write);
EXPORT_SYMBOL(pec_sg_list_read);
EXPORT_SYMBOL(pec_sg_list_get_capacity);
EXPORT_SYMBOL(pec_scatter_preload);
EXPORT_SYMBOL(pec_scatter_preload_notify_request);
#endif /* ADAPTER_PEC_ENABLE_SCATTERGATHER */

// IOToken API
EXPORT_SYMBOL(io_token_create);
EXPORT_SYMBOL(io_token_parse);
EXPORT_SYMBOL(io_token_in_word_count_get);
EXPORT_SYMBOL(io_token_out_word_count_get);
EXPORT_SYMBOL(io_token_sa_addr_update);
EXPORT_SYMBOL(io_token_sa_reuse_update);
EXPORT_SYMBOL(io_token_mark_set);
EXPORT_SYMBOL(io_token_mark_check);
EXPORT_SYMBOL(io_token_packet_legth_get);
EXPORT_SYMBOL(io_token_bypass_legth_get);
EXPORT_SYMBOL(io_token_error_code_get);

#ifdef ADAPTER_AUTO_TOKENBUILDER
// SABuilder API
EXPORT_SYMBOL(sa_builder_get_sizes);
EXPORT_SYMBOL(sa_builder_build_sa);
EXPORT_SYMBOL(sa_builder_init_esp);
EXPORT_SYMBOL(sa_builder_init_ssltls);
EXPORT_SYMBOL(sa_builder_init_basic);
EXPORT_SYMBOL(sa_builder_init_srtp);

// TokenBuilder API
EXPORT_SYMBOL(token_builder_get_context_size);
EXPORT_SYMBOL(token_builder_build_context);
EXPORT_SYMBOL(token_builder_get_size);
EXPORT_SYMBOL(token_builder_build_token);
#endif


#endif // ADAPTER_PEC_LKM_EXT_H_


/* end of file adapter_pec_lkm_ext.h */
