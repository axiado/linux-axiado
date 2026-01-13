/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (C) 2022-2026 Axiado Corporation. */

#ifndef _DTL_H_
#define _DTL_H_

#include "api_pcl.h"
#include "api_pcl_dtl.h"
#include "adapter.h"
#include "da_pec_internal.h"

enum AX_EIP_STATUS eip_pcl_dtl_init(struct eip_public *eip_pub,
				    int interface_id, int flow_hash_table_cnt);
enum AX_EIP_STATUS add_dtl_rule(struct eip_public *eip_pub,
				struct rule_selectors *rule_sel, int dest_rid);
enum AX_EIP_STATUS prepare_dtl_rule(struct dtl_params *dtl_prm,
				    struct rule_selectors *rule, u8 rid);

enum AX_EIP_STATUS submit_dtl_rule(struct eip_public *eip_pub,
				   struct dtl_params *dtl_prm, int mrk_rule_id);
int delete_lookup_rule(struct selectors *sel);
int delete_lookup_rule_by_id(int mrk_rule_id);
int add_rejected_flow_rule(struct selectors *sel);
enum AX_EIP_STATUS init_shd_lkp(struct eip_public *eip_pub);
void uninit_shd_lkp(struct eip_public *eip_pub);
#if DTL_EXPIRY_ENABLED
enum AX_EIP_STATUS delete_dtl_rule(struct eip_public *eip_pub, int idx,
				   bool is_slt);
enum AX_EIP_STATUS remove_shd_lkp_rec(struct eip_public *eip_pub, int sl_idx,
				      bool is_slt);
#endif /* DTL_EXPIRY_ENABLED */
int flush_dtl_rules(struct eip_public *eip_pub);
int eip_pcl_dtl_uninit(struct eip_public *eip_pub, int interface_id,
		       int flow_hash_table_cnt);

#ifdef CONFIG_EIP_IPSEC_OFFLOAD
int submit_dtl_rule_ipsec(struct xfrm_id id, dma_buf_handle_t sa_handle_in);
#endif /* CONFIG_EIP_IPSEC_OFFLOAD*/

#endif /* _DTL_H_ */
