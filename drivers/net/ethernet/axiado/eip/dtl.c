// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * dtl.c - Functions for adding DTL rule into lookup table.
 *
 * Copyright (C) 2022-2026 Axiado Corporation.
 */

#include <linux/delay.h>
#include <net/xfrm.h>
#include <linux/netdevice.h>
#include <linux/timekeeping.h>
#include <linux/ktime.h>
#include <linux/wait.h>

#include "da_pec_internal.h"
#include "api_dmabuf.h"
#include "api_pec.h"
#include "api_pec_sg.h"
#include "api_pcl.h"
#include "api_pcl_dtl.h"
#include "log.h"
#include "clib.h"
#include "eip.h"
#include "sa_builder.h"
#include "sa_builder_basic.h"
#include "sa_builder_ipsec.h"
#include "sa_builder_params_basic.h"
#include "token_builder.h"
#include "device_mgmt.h"
#include "device_rw.h"

#include "dtl.h"
#include "adapter.h"

#define TR_WORD_CNT 64

/**
 * @brief Construct a new declare wait queue head object
 * for scanning DTL shadow lookup table and delting the expired rules.
 *
 */
static DECLARE_WAIT_QUEUE_HEAD(dtl_scan_wq);

/**
 * @brief Initialise classification engine and lookup table.
 * This also creates Shadow Lookup Table and Transform Record pool.
 *
 * @param interface_id Interface id for which lookup table is created.
 *           This must be 0 due to implementation limit in EIP-DDK code
 * @param flow_hash_table_cnt number of lookup tables to be created.
 *
 * @return EIP_STATUS_SUCCESS on success; otherwise non-zero.
 */
enum AX_EIP_STATUS eip_pcl_dtl_init(struct eip_public *eip_pub, int rid,
				    int flow_hash_table_cnt)
{
	int err;
	pcl_status_t pcl_st;

	LOG_DEBG("Initializing PCL for IF: %d\n", rid);

	pcl_st = pcl_init(rid, flow_hash_table_cnt);
	if (pcl_st != PCL_STATUS_OK) {
		LOG_CRIT("PCL could not be initialized, error=%d\n", pcl_st);
		err = EIP_STATUS_DDK_ERROR;
		goto err_pcl_init;
	}

	pcl_st = pcl_dtl_init(rid);
	if (pcl_st != PCL_STATUS_OK) {
		LOG_CRIT("PCL-DTL could not be initialized, error=%d\n",
			 pcl_st);
		err = EIP_STATUS_DDK_ERROR;
		goto err_pcl_dtl_init;
	}

	LOG_DEBG("Initialized PCL_DTL_ for interface ID %d\n", rid);

	/* Prepare a pool of TR and store them in Shadow Lookup Table */
	err = init_shd_lkp(eip_pub);
	if (err) {
		LOG_CRIT("Creation of TR pool failed");
		err = EIP_STATUS_INTERNAL_ERROR;
		goto err_slt_tr;
	}
	LOG_DEBG("Created Shd Lkp Tbl and TR Pool");
	return EIP_STATUS_SUCCESS;

err_slt_tr:
	eip_pcl_dtl_uninit(eip_pub, DEFAULT_INTERFACE_ID, LOOKUP_TABLE_CNT);
err_pcl_dtl_init:
	pcl_uninit(rid);
err_pcl_init:
	return err;
}

/**
 * @brief Store DTL-Rule handle in Shadow-Lookup-Table(SLT). While adding DTL rule into
 * eip-lookup table, this DTL-rule-handle is returned.
 *
 * @param eip_pub eip public structure.
 * @param sel DTL-Rule selectors parameters.
 * @param dtlh DTL-Rule handle which was retured while adding this rule into lookup table.
 *
 * @return int 0 if record is added successfully. Otherwise non-zero for error.
 */
#if DTL_EXPIRY_ENABLED

enum AX_EIP_STATUS add_shd_lkp_rec(struct eip_public *eip_pub,
				   pcl_selector_params_t *sel,
				   pcl_dtl_hash_handle_t *dtlh, int mrk_rule_id,
				   bool is_slt)
{
	struct shadow_dtl_rec *sdtl_p = NULL;
	int sl_curr_idx = -1;

	LOG_DEBG("Adding SLT record: DTL_hndl: 0x%X, rule_id = %d", dtlh->p,
		 mrk_rule_id);

	if (is_slt) {
		sdtl_p = eip_pub->slt_ptr;
		sl_curr_idx = eip_pub->slt_curr_free_idx;
	} else {
		sdtl_p = eip_pub->sl_fifo_ptr;
		sl_curr_idx = eip_pub->sl_fifo_curr_free_idx;
	}

	LOG_DEBG("Adding SLT record at idx[%d]: DTL_hndl: 0x%X", sl_curr_idx,
		 dtlh->p);

	if (sdtl_p[sl_curr_idx].freed) {
		/* Store DTL handle */
		memcpy(&sdtl_p[sl_curr_idx].dtlh, dtlh,
		       sizeof(pcl_dtl_hash_handle_t));

		/* Store selectors params */
		memcpy(&sdtl_p[sl_curr_idx].sel.sip, sel->src_ip, IP_SIZE);
		memcpy(&sdtl_p[sl_curr_idx].sel.dip, sel->dst_ip, IP_SIZE);
		sdtl_p[sl_curr_idx].sel.sport = sel->src_port;
		sdtl_p[sl_curr_idx].sel.dport = sel->dst_port;
		sdtl_p[sl_curr_idx].sel.proto = sel->ip_proto;
		sdtl_p[sl_curr_idx].sel.spi = sel->spi;
		sdtl_p[sl_curr_idx].sel.epoch = sel->epoch;
		sdtl_p[sl_curr_idx].sel.flags = sel->flags;

		/* Set the freed flag to Non-Empty */
		sdtl_p[sl_curr_idx].freed = false;

		/* Store rule id extracted from skb-mark */
		sdtl_p[sl_curr_idx].mrk_rule_id = mrk_rule_id;
		LOG_DEBG("%s: Sh-tbl_idx = %d; Stored rule id = %d", __func__,
			 sl_curr_idx, sdtl_p[sl_curr_idx].mrk_rule_id);

	} else {
		LOG_CRIT("internal error while adding Shadow Lookup Table record");
		return EIP_STATUS_INTERNAL_ERROR;
	}

	/* Increment curr fifo index by one. */
	if (is_slt) {
		sl_curr_idx = (sl_curr_idx + 1) % SLT_RECORD_CNT;
		eip_pub->slt_curr_free_idx = sl_curr_idx;
	} else {
		sl_curr_idx = (sl_curr_idx + 1) % SL_FIFO_RECORD_CNT;
		eip_pub->sl_fifo_curr_free_idx = sl_curr_idx;
	}

	/* Check if the updated curr idx is free or not. If it's not free,
	 * delete DTL rule and rec associated with it
	 */
	if (!sdtl_p[sl_curr_idx].freed) {
		/* Remove DTL rule from EIP-lookup table */
		delete_dtl_rule(eip_pub, sl_curr_idx, is_slt);

		/* Remove Shadow Table record even if removing DTL fails. */
		if (remove_shd_lkp_rec(eip_pub, sl_curr_idx, is_slt)) {
			LOG_CRIT("error in removing SL record.");
			return EIP_STATUS_INTERNAL_ERROR;
		}
		LOG_DEBG("Freed up next entry in SL");
	}

	LOG_DEBG("Successfully added record into Shadow Lkp ");
	return EIP_STATUS_SUCCESS;
}
#endif /* DTL_EXPIRY_ENABLED */

/**
 * @brief  Remove record from Shadow Lookup Table(SLT).Make sure that DTL rule
 * from EIP-Lookup table is removed before calling this func. Here, Transform
 * Record stats for the associated SLT-record are also cleared. However the TR
 * associated with SLT-record is not removed and memory for the TR is also not freed.
 *
 * WARN : make sure that DTL record from EIP lookup table is deleted before removing
 * DTL record from Shadow table/fifo.
 *
 * @param slt_idx record index that is to be removed from SLT.
 *
 * @return int 0 if successful. Non-zero for error.
 */
#if DTL_EXPIRY_ENABLED
enum AX_EIP_STATUS remove_shd_lkp_rec(struct eip_public *eip_pub, int sl_idx,
				      bool is_slt)
{
	struct shadow_dtl_rec *sdtl_p = NULL;

	LOG_DEBG("Removing SLT record for idx: %d", sl_idx);

	if (is_slt)
		sdtl_p = eip_pub->slt_ptr;
	else
		sdtl_p = eip_pub->sl_fifo_ptr;

	/* Clear DTL handle record mem */
	memset(&sdtl_p[sl_idx].dtlh, 0, sizeof(pcl_dtl_hash_handle_t));
	/* Clear Selector param mem */
	memset(&sdtl_p[sl_idx].sel, 0, sizeof(struct selectors));
	sdtl_p[sl_idx].freed = true;
	sdtl_p[sl_idx].mrk_rule_id = -1;

	LOG_DEBG("Removed SL record.");
	return EIP_STATUS_SUCCESS;
}
#endif /* DTL_EXPIRY_ENABLED */

/**
 * @brief Fetch DTL-Rule handle associated with idx from Shadow Lookup Table(SLT)
 *
 * @param idx index for the record in SLT which contains DTL handle.
 * @return DTL Rule handle for record in EIP-Lookup-Table.
 */
pcl_dtl_hash_handle_t *get_dtl_handle(struct eip_public *eip_pub, int idx,
				      bool is_slt)
{
	struct shadow_dtl_rec *sdtl_p;

	if (is_slt)
		sdtl_p = eip_pub->slt_ptr;
	else
		sdtl_p = eip_pub->sl_fifo_ptr;

	if (!sdtl_p[idx].freed)
		return &sdtl_p[idx].dtlh;

	LOG_DEBG("SLT record at %d is EMPTY.", idx);

	return NULL;
}

/**
 * @brief Remove DTL rule from EIP-DTL-Lookup Table only.
 * This does not UnRegister/Remove the Transform Record associated
 * with DTL rule. This also does not remove record from Shadow Lookup Table.
 *
 * @param eip_pub eip public structure.
 * @param idx index in SHadow Lookup Table for DTL rule.
 *
 * @return EIP_STATUS_SUCCESS if success. Non-zero on error.
 */
#if DTL_EXPIRY_ENABLED
enum AX_EIP_STATUS delete_dtl_rule(struct eip_public *eip_pub, int idx,
				   bool is_slt)
{
	pcl_dtl_hash_handle_t *dtl_h;
	pcl_status_t pcl_st;

	/* Delete DTL record from EIP lookup Table */
	dtl_h = get_dtl_handle(eip_pub, idx, is_slt);
	if (!dtl_h) {
		LOG_CRIT("error: Failed to get dtl-handle. idx: %d", idx);
		return EIP_STATUS_INTERNAL_ERROR;
	}

	/* Remove DTL-rule from EIP Lookup table */
	/* interface_id = 0, lookup-table-id = 0; due to DDK implementation limit */
	pcl_st = pcl_dtl_hash_remove(0, 0, dtl_h);
	if (pcl_st != PCL_STATUS_OK) {
		LOG_CRIT("pcl_dtl_hash_remove failed, rc = %d\n", pcl_st);
		return EIP_STATUS_INTERNAL_ERROR;
	}
	LOG_DEBG("SUCCESS: Removed DTL rule from shd-idx %d", idx);
	return EIP_STATUS_SUCCESS;
}
#endif

/**
 * @brief Submit DTL rule to lookup table by calling EIP-DDK API.
 * After adding DTL-rule into lookup table, the DTL-rule-handle is returned by DDK.
 * This handle is stored in Shadow-Lookup-Table.
 *
 * @param eip_pub eip public structure.
 * @param dtl_prm Input buffer containing one DTL rule.
 *
 * @return EIP_STATUS_SUCCESS on success; otherwise non-zero.
 */
enum AX_EIP_STATUS submit_dtl_rule(struct eip_public *eip_pub,
				   struct dtl_params *dtl_prm, int mrk_rule_id)
{
	pcl_status_t pcl_st;
	pcl_dtl_transform_params_t pcl_dtl_tr_prm;
	int rc;
	pcl_selector_params_t pcl_sel_prm;
	bool is_slt;

	zeroinit(pcl_sel_prm);
	zeroinit(pcl_dtl_tr_prm);

	pcl_sel_prm.ip_proto = dtl_prm->rule_sel->protocol;
	pcl_sel_prm.src_ip = dtl_prm->rule_sel->src_ip;
	pcl_sel_prm.dst_ip = dtl_prm->rule_sel->dst_ip;
	pcl_sel_prm.src_port = dtl_prm->rule_sel->src_port;
	pcl_sel_prm.dst_port = dtl_prm->rule_sel->dst_port;
	pcl_sel_prm.spi = dtl_prm->rule_sel->spi;
	pcl_sel_prm.epoch = dtl_prm->rule_sel->epoch;
	pcl_sel_prm.flags = dtl_prm->rule_sel->flags;

	pcl_st = pcl_flow_hash(&pcl_sel_prm, pcl_dtl_tr_prm.hash_id);
	if (pcl_st != PCL_STATUS_OK) {
		LOG_CRIT("PEC_Flow_Hash failed\n");
		rc = EIP_STATUS_DDK_ERROR;
		goto error_exit;
	}

	LOG_DEBG("HASHID[0]: 0x%08X, [1]: 0x%08X, [2]: 0x%08X, [3]: 0x%08X",
		 pcl_dtl_tr_prm.hash_id[0], pcl_dtl_tr_prm.hash_id[1],
		 pcl_dtl_tr_prm.hash_id[2], pcl_dtl_tr_prm.hash_id[3]);

	if (pcl_sel_prm.ip_proto == IPPROTO_TCP)
		dtl_prm->tr_h = get_tr(eip_pub, dtl_prm->dest_rid, true);
	else
		dtl_prm->tr_h = get_tr(eip_pub, dtl_prm->dest_rid, false);
	LOG_DEBG("TR_Phy_handle: 0x%08X", dtl_prm->tr_h->phy_addr.p);

	/* Add the Inbound DTL record into Flow_Hash_Table 0 of interface_id 0. */
	pcl_st = pcl_dtl_transform_add(DEFAULT_INTERFACE_ID,
				       dtl_prm->lookup_table_id,
				       &pcl_dtl_tr_prm, dtl_prm->tr_h->phy_addr,
				       &dtl_prm->dtl_hash_h);
	if (pcl_st != PCL_STATUS_OK) {
		LOG_WARN("PEC_DTL_Transform_Add failed\n");
		rc = EIP_STATUS_DDK_ERROR;
		goto error_exit;
	}
	LOG_DEBG("%s: DTL handle: 0x%X\n", __func__, dtl_prm->dtl_hash_h.p);

#if DTL_EXPIRY_ENABLED
	/* Add SLT record for newly added DTL rule */
	is_slt = (pcl_sel_prm.ip_proto == IPPROTO_TCP) ? true : false;

	rc = add_shd_lkp_rec(eip_pub, &pcl_sel_prm, &dtl_prm->dtl_hash_h,
			     mrk_rule_id, is_slt);
	if (rc) {
		LOG_CRIT("Adding record into Shadow Lookup Table FAILED\n");
		rc = EIP_STATUS_INTERNAL_ERROR;
		goto error_slt;
	}

#endif /* DTL_EXPIRY_ENABLED */

	LOG_DEBG("DTL RULE ADD - SUCCESS");
	return EIP_STATUS_SUCCESS;
#if DTL_EXPIRY_ENABLED
error_slt:
	/* Delete DTL Rule from EIP-Lookup Table. */
	pcl_dtl_hash_remove(0, 0, &dtl_prm->dtl_hash_h);
#endif /* DTL_EXPIRY_ENABLED */
error_exit:
	LOG_WARN("DTL RULE ADD - FAILED");
	return rc; /* error return */
}

#ifdef CONFIG_EIP_IPSEC_OFFLOAD
/**
 * @brief add lookup rule for ipsec
 *
 * @param id - xfrm sa id
 * @param sa_handle_in - sa handle
 * @return int zero on success
 */
int submit_dtl_rule_ipsec(struct xfrm_id id, dma_buf_handle_t sa_handle_in)
{
	u8 dest_ip[4];
	u32 spi = 0;

	pcl_status_t pcl_st;
	pcl_dtl_transform_params_t pcl_dtl_tr_prm;

	pcl_selector_params_t pcl_sel_prm;
	pcl_dtl_hash_handle_t sa_hash_handle;

	zeroinit(pcl_sel_prm);
	zeroinit(pcl_dtl_tr_prm);

	pcl_sel_prm.ip_proto = id.proto;
	pcl_sel_prm.dst_ip = ((u8 *)&id.daddr.a4);
	spi |= (id.spi & 0x000000ff) << 24u;
	spi |= (id.spi & 0x0000ff00) << 8u;
	spi |= (id.spi & 0x00ff0000) >> 8u;
	spi |= (id.spi & 0xff000000) >> 24u;
	pcl_sel_prm.spi = spi;

	pcl_st = pcl_flow_hash(&pcl_sel_prm, pcl_dtl_tr_prm.hash_id);
	if (pcl_st != PCL_STATUS_OK) {
		LOG_CRIT(": PEC_Flow_Hash failed\n");
		goto error_exit;
	}

	LOG_DEBG("HASHID[0]: 0x%08X, [1]: 0x%08X, [2]: 0x%08X, [3]: 0x%08X",
		 pcl_dtl_tr_prm.hash_id[0], pcl_dtl_tr_prm.hash_id[1],
		 pcl_dtl_tr_prm.hash_id[2], pcl_dtl_tr_prm.hash_id[3]);

	/* Add the Inbound DTL entry into Flow_Hash_Table 0 of interface_id 0. */
	pcl_st = pcl_dtl_transform_add(0, 0, &pcl_dtl_tr_prm, sa_handle_in,
				       &sa_hash_handle);

	if (pcl_st != PCL_STATUS_OK) {
		LOG_WARN(": PEC_DTL_Transform_Add failed\n");
		goto error_exit;
	}

	LOG_DEBG("DTL RULE ADD - SUCCESS");
	return 0;

error_exit:
	LOG_WARN("DTL RULE ADD - FAILED");
	return -1; /* error return */
}
#endif

/**
 * @brief Delete DTL rule from lookup table. DTL-Hash is removed from EIP-lookup
 * table and Shadow Lookup Table as well. Here the TR associated with DTL rule is
 * NOT removed. However TR-stats are cleared.
 *
 * @param sel DTL rule parameters.
 *
 * @return EIP_STATUS_SUCCESS if DTL-rule is deleted successfully. Otherwise Non-zero.
 */
int delete_lookup_rule(struct selectors *sel)
{
	int ret = EIP_STATUS_SUCCESS;
#if DTL_EXPIRY_ENABLED
	struct shadow_dtl_rec *sltp = NULL;
	struct selectors *curr_sel = NULL;
	struct eip_public *eip_pub = get_eip_public();
	int i, sl_rec_idx = -1;
	bool is_slt = true;
	int rec_cnt_limit = 0;

	if (sel->proto == IPPROTO_TCP) {
		sltp = eip_pub->slt_ptr;
		rec_cnt_limit = SLT_RECORD_CNT;
		is_slt = true;
	} else {
		sltp = eip_pub->sl_fifo_ptr;
		rec_cnt_limit = SL_FIFO_RECORD_CNT;
		is_slt = false;
	}

	sl_rec_idx = -1;

	/* Find Shadow-Lookup-Tbl Record for this tuple. */
	for (i = 0; i < rec_cnt_limit; i++) {
		if (sltp[i].freed)
			continue;
		curr_sel = &sltp[i].sel;

		/* Compare for only 5-tuples. No need to comapre other members. */
		if (memcmp(curr_sel->sip, sel->sip, 4) ||
		    memcmp(curr_sel->dip, sel->dip, 4) ||
		    curr_sel->proto != sel->proto ||
		    curr_sel->sport != sel->sport ||
		    curr_sel->dport != sel->dport) {
			continue;
		} else {
			LOG_DEBG("Found Shd Lkp Tbl Record id: %d", i);
			sl_rec_idx = i;
			break;
		}
	}

	if (sl_rec_idx != -1) {
		/* Remove DTL rule from EIP-lookup table */
		delete_dtl_rule(eip_pub, sl_rec_idx, is_slt);

		/* TODO  call delete_dtl_rule(idx, false) for fifo rec deletion */

		/* Remove Shadow Table record even if removing DTL fails. */
		if (remove_shd_lkp_rec(eip_pub, sl_rec_idx, is_slt)) {
			LOG_CRIT("error in removing SL Table record.");
			ret = EIP_STATUS_INTERNAL_ERROR;
		}
	} else {
		LOG_INFO("Not Found Shadow Lookup fifo Record.");
		ret = EIP_STATUS_INTERNAL_ERROR;
	}
#else
	LOG_INFO(" Deleting DTL-lookup rule is not supported as of now.");
#endif /* DTL_EXPIRY_ENABLED */
	return ret;
}
EXPORT_SYMBOL(delete_lookup_rule);

/**
 * @brief Delete DTL rule from lookup table. DTL-Hash is removed from EIP-lookup
 * table and Shadow Lookup Table as well. Here the TR associated with DTL rule is
 * NOT removed. However TR-stats are cleared.
 *
 * @param rule_id Rule id associated with the flow-rule.
 *
 * @return EIP_STATUS_SUCCESS if DTL-rule is deleted successfully. Otherwise Non-zero.
 */
int delete_lookup_rule_by_id(int rule_id)
{
#if DTL_EXPIRY_ENABLED
	struct eip_public *eip_pub = get_eip_public();
	struct shadow_dtl_rec *sltp = NULL;
	int i, sl_rec_idx = -1;
	bool is_slt = true;
	int rec_cnt_limit = 0;
	int rc = EIP_STATUS_SUCCESS;

	sltp = eip_pub->slt_ptr;
	rec_cnt_limit = SLT_RECORD_CNT;
	is_slt = true;

	sl_rec_idx = -1;

	LOG_DEBG("%s: Scanning SLT for rule id %d", __func__, rule_id);
	/* Find Shadow-Lookup-Tbl Record for this tuple. */
	for (i = 0; i < rec_cnt_limit; i++) {
		if (sltp[i].freed)
			continue;
		LOG_DEBG("%s:idx[%d] => mrk_rule_id = %d", __func__, i,
			 sltp[i].mrk_rule_id);

		/* Check if the rule_id is matching to shadow-entry */
		if (sltp[i].mrk_rule_id == rule_id) {
			LOG_DEBG("Found Shd Lkp Tbl Record id: %d", i);

			/* Remove DTL rule from EIP-lookup table */
			delete_dtl_rule(eip_pub, i, is_slt);

			/* Remove Shadow Table record even if removing DTL fails. */
			rc = remove_shd_lkp_rec(eip_pub, i, is_slt);
		}
	}

	LOG_DEBG("%s: Scanning SL fifo", __func__);
	sltp = eip_pub->sl_fifo_ptr;
	rec_cnt_limit = SL_FIFO_RECORD_CNT;
	is_slt = false;

	/* Find Shadow-Lookup-fifo Record for this tuple. */
	for (i = 0; i < rec_cnt_limit; i++) {
		if (sltp[i].freed)
			continue;
		/* Check if the rule_id is matching to shadow-entry */
		if (sltp[i].mrk_rule_id == rule_id) {
			LOG_DEBG("Found Shd Lkp Fifo Record id: %d", i);
			//sl_rec_idx = i;
			/* Remove DTL rule from EIP-lookup table */
			delete_dtl_rule(eip_pub, i, is_slt);

			/* Remove Shadow Table record even if removing DTL fails. */
			rc = remove_shd_lkp_rec(eip_pub, i, is_slt);
		}
	}

	LOG_DEBG("Done for removing EIP-lookup rule and SL record. rc = %d",
		 rc);
	return rc;
#else
	LOG_INFO(" Deleting DTL-lookup rule is not supported as of now.");
	return EIP_STATUS_SUCCESS;
#endif /* DTL_EXPIRY_ENABLED */
}
EXPORT_SYMBOL(delete_lookup_rule_by_id);

/**
 * @brief Prepare a static transform record for DTL rule.
 *
 * @param trh (O/P) TR handle which will be filled after creating TR.
 * @param rid Ring for which TR to create.
 *
 * @return EIP_STATUS_SUCCESS on success; otherwise non-zero.
 */
enum AX_EIP_STATUS prepare_tr(struct tr_handle *trh, u8 rid)
{
	dma_buf_properties_t dma_properties = { 0, 0, 0, 0 };
	dma_buf_status_t dma_st;
	pcl_status_t pcl_st;
	u32 tr_temp[TR_WORD_CNT];
	int ret, i;

	/* Allocate DMA memory for TR */
	dma_properties.f_cached = true;
	dma_properties.alignment = DA_PEC_DMA_ALIGNMENT_BYTE_COUNT;
	dma_properties.bank = DA_PEC_BANK_SA;
	dma_properties.size = MAX(4 * TR_WORD_CNT, DA_PEC_MIN_SA_BYTE_COUNT);

	dma_st = dma_buf_alloc(dma_properties, &trh->virt_addr, &trh->phy_addr);
	if (dma_st != DMABUF_STATUS_OK) {
		LOG_CRIT("dma alloc for TR failed. ret status : %d", dma_st);
		return EIP_STATUS_MEM_ERROR;
	}
	LOG_DEBG("Dest intf = %u; TR addr: virt: 0x%X, phy: 0x%X", rid,
		 trh->virt_addr.p, trh->phy_addr.p);

	/* Set value for TR */
	memset((void *)tr_temp, 0, TR_WORD_CNT * sizeof(u32));

	/* eip-96 context control words. 48-> token header, 49-> b11 Redir-En, b12-b15 dest-ring */
	tr_temp[0] = 0x00000100;
	tr_temp[48] = 0x00020000;
	/* Set redirect i/f and enable redirect bit(11) */
	tr_temp[49] = ((rid & 0xF) << 12) | (0x1 << 11);
	tr_temp[51] = NON_OFFLOAD_TR_CNTXT_REF;
	tr_temp[54] = 0xd0060000;
	tr_temp[55] = 0xe156003f;

	/* Copy local TR into DMA buff */
	for (i = 0; i < TR_WORD_CNT; i++) {
		*(u32 *)(uintptr_t)(trh->virt_addr.p + i * 4) = tr_temp[i];
		if (tr_temp[i])
			LOG_DEBG("TR[%d]: 0x%08X", i, tr_temp[i]);
	}

	/* Register TR with PCL */
	pcl_st = pcl_transform_register(trh->phy_addr);
	if (pcl_st != PCL_STATUS_OK) {
		LOG_CRIT("TR Reg. with PCL Failed. pcl_st = %d\n", pcl_st);
		ret = EIP_STATUS_DDK_ERROR;
		goto err_tr_reg;
	}
	return EIP_STATUS_SUCCESS;

err_tr_reg:
	dma_buf_release(trh->phy_addr);
	return ret;
}

/**
 * @brief  Allocate DDR memory for Shadow Lookup Table(SLT). Initialise
 *  each record in SLT. Create a separate TR for each SLT-record. Each TR
 * is initialised with NULL crypto as of now.
 *
 * @param eip_pub eip-dev public data ptr.
 * @return EIP_STATUS_SUCCESS on success. Non-zero on error.
 */
enum AX_EIP_STATUS init_shd_lkp(struct eip_public *eip_pub)
{
	int ret, i;

	LOG_DEBG("Creating Shadow Lookup Table\n");
	/* Allocate memory for Shadow Lookup Table(SLT) */
	eip_pub->slt_ptr = kmalloc(sizeof(*eip_pub->slt_ptr) * SLT_RECORD_CNT, GFP_KERNEL);
	if (!eip_pub->slt_ptr) {
		LOG_CRIT("Failed to allocate mem for Shadow Lookup Table");
		ret = EIP_STATUS_MEM_ERROR;
		goto err;
	}

	memset(eip_pub->slt_ptr, 0,
	       sizeof(struct shadow_dtl_rec) * SLT_RECORD_CNT);
	/* Initialise curr free ind with 0, i.e. place for the first record. */
	eip_pub->slt_curr_free_idx = 0;

	for (i = 0; i < SLT_RECORD_CNT; i++) {
		eip_pub->slt_ptr[i].freed = true;
		eip_pub->slt_ptr[i].mrk_rule_id = -1;
	}

	/* Create TR for SL Table. Skip Ring-0 */
	for (i = 0; i < RING_INTERFACE_CNT; i++) {
		/* TODO exclude REJECTED_FLOW_RULE_RING_ID, IPSEC_LOOKASIDE_INTERFACE */
		if (i == DEFAULT_INTERFACE_ID)
			continue;
		ret = prepare_tr(&eip_pub->sl_tbl_trh[i], i);
		if (ret) {
			LOG_CRIT("error in creating TR for SLT");
			ret = EIP_STATUS_INTERNAL_ERROR;
			goto err;
		}
	}

	LOG_DEBG("Created TR  for lookup table ");

	/* Now creating fifo for Shadow DTL */
	eip_pub->sl_fifo_ptr =
		kmalloc(sizeof(*eip_pub->sl_fifo_ptr) * SL_FIFO_RECORD_CNT, GFP_KERNEL);
	if (!eip_pub->sl_fifo_ptr) {
		LOG_CRIT("Failed to allocate mem for Shadow Lookup fifo");
		ret = EIP_STATUS_MEM_ERROR;
		goto err;
	}

	memset(eip_pub->sl_fifo_ptr, 0,
	       sizeof(struct shadow_dtl_rec) * SL_FIFO_RECORD_CNT);
	/* Initialise curr free ind with 0, i.e. place for the first record. */
	eip_pub->sl_fifo_curr_free_idx = 0;
	for (i = 0; i < SL_FIFO_RECORD_CNT; i++) {
		eip_pub->sl_fifo_ptr[i].freed = true;
		eip_pub->sl_fifo_ptr[i].mrk_rule_id = -1;
	}
	/* Create TR for SL fifo. Skip Ring-0 */
	for (i = 0; i < RING_INTERFACE_CNT; i++) {
		/* TODO exclude REJECTED_FLOW_RULE_RING_ID, IPSEC_LOOKASIDE_INTERFACE */
		if (i == DEFAULT_INTERFACE_ID)
			continue;
		ret = prepare_tr(&eip_pub->sl_fifo_trh[i], i);
		if (ret) {
			LOG_CRIT("error in creating TR for SL fifo");
			ret = EIP_STATUS_INTERNAL_ERROR;
			goto err;
		}
		LOG_DEBG("tr_h[%d] : 0x%08X", i,
			 eip_pub->sl_fifo_trh[i].phy_addr);
	}

	LOG_INFO("Created TR list for Shadow Lookup fifo");
	return EIP_STATUS_SUCCESS;

err:
	uninit_shd_lkp(eip_pub);
	return ret;
}

/**
 * @brief Delete the transform records in the lookup
 *
 * @param eip_pub eip-dev public data ptr.
 */
void uninit_shd_lkp(struct eip_public *eip_pub)
{
	u16 i;

	/* Delete TR for SL fifo. Skip Ring-0 */
	for (i = 0; i < RING_INTERFACE_CNT; i++) {
		if (eip_pub->sl_fifo_trh[i].phy_addr.p) {
			dma_buf_release(eip_pub->sl_fifo_trh[i].phy_addr);
			eip_pub->sl_fifo_trh[i].phy_addr.p = NULL;
		}
	}

	/* Delete TR for SL Table. Skip Ring-0 */
	for (i = 0; i < RING_INTERFACE_CNT; i++) {
		if (eip_pub->sl_tbl_trh[i].phy_addr.p) {
			dma_buf_release(eip_pub->sl_tbl_trh[i].phy_addr);
			eip_pub->sl_tbl_trh[i].phy_addr.p = NULL;
		}
	}

	kfree(eip_pub->sl_fifo_ptr);
	eip_pub->sl_fifo_ptr = NULL;
	kfree(eip_pub->slt_ptr);
	eip_pub->slt_ptr = NULL;
}

/**
 * @brief Return the Transform Record handle from Shadow Lookup Table(SLT).
 * TR handle from current free record in SLT is returned.
 *
 * @param eip_pub eip public data
 * @param rid Rind for which tr to get
 *
 * @return tr_handle ptr
 */
struct tr_handle *get_tr(struct eip_public *eip_pub, u8 rid, bool is_slt)
{
	struct tr_handle *trh;

	LOG_DEBG("%s: rid : %u; is_slt : %d", __func__, rid, is_slt);
#if DTL_EXPIRY_ENABLED
	if (is_slt) {
		LOG_DEBG("tcp");
		/* Get TR handle from Shadow Lookup Table record */
		trh = &eip_pub->sl_tbl_trh[rid];
	} else {
		LOG_DEBG("non-tcp");
		/* Get TR handle from Shadow Lookup fifo record */
		trh = &eip_pub->sl_fifo_trh[rid];
	}
	LOG_DEBG("%s: = 0x%08X", __func__, trh->phy_addr);

#else
	trh = &eip_pub->slt_ptr[0].trh;
#endif /* DTL_EXPIRY_ENABLED */

	return trh;
}

/**
 * @brief Free up memory of Shadow Lookup Table.
 * [1] Remove DTL rules if it exists. [2] Unregister TR and Release TR mem.
 * [3] Release SLT mem.
 *
 * @param priv eip-dev private data
 */
void release_slt_tr(struct eip_public *eip_pub)
{
	// TODO release TR handle of SL-Table and SL-fifo.

	/* Release SLT mem */
	kfree(eip_pub->slt_ptr);
	eip_pub->slt_ptr = NULL;

	/* Release SL fifo mem */
	kfree(eip_pub->sl_fifo_ptr);
	eip_pub->sl_fifo_ptr = NULL;
}

/**
 * @brief Prepare DTL rule from rule-selector.
 *
 * @param dtl_prm (O/P) DTL rule will be filled here.
 * @param rule (I/P) params for preparing DTL rule.
 * @param rid (I/P) ring id
 *
 * @return EIP_STATUS_SUCCESS on success; otherwise non-zero.
 */
enum AX_EIP_STATUS prepare_dtl_rule(struct dtl_params *dtl_prm,
				    struct rule_selectors *rule, u8 rid)
{
	/* Assign rule to dtl_prm */
	dtl_prm->rule_sel = rule;
	/* Set spi and epoch value as 0 for now */
	dtl_prm->rule_sel->epoch = 0;

	if (rule->protocol == IPPROTO_ESP) {
		dtl_prm->rule_sel->spi = rule->spi;
		/* For ESP pkts flags should be set to zero */
		dtl_prm->rule_sel->flags = 0x00000000U;
	} else {
		/* Default case for Non-ESP packets */
		dtl_prm->rule_sel->spi = 0;
		dtl_prm->rule_sel->flags = 0x00000001U;
	}

	/* This must be 0 due to implementation limit in eip. */
	dtl_prm->dest_rid = rid;
	dtl_prm->lookup_table_id = 0;

	LOG_DEBG("%s:Prepared dtl rule", __func__);
	return EIP_STATUS_SUCCESS;
}

/**
 * @brief Top level function for ading DTL rule into lookup table.
 *
 * @param eip_pub eip public structure
 * @param rule_sel (I/P) params for creating DTL rule.
 * @param dest_if (I/P) Destination ring ID
 *
 * @return EIP_STATUS_SUCCESS on success; otherwise non-zero.
 */
enum AX_EIP_STATUS add_dtl_rule(struct eip_public *eip_pub,
				struct rule_selectors *rule_sel, int dest_rid)
{
	struct dtl_params dtl_prm;
	int err;

	/* For mark_pkt_res values : For ACCEPT = Non zero; REJECT = zero
	 * If skb-mark says to reject the flow, then DO NOT ADD DTL flow.
	 */
	if (!rule_sel->m_pkt.pkt_res) {
		LOG_DEBG("Not adding DTL rule for dest_rid(%d) as SKB_mark is for reject.",
			 dest_rid);
		return EIP_STATUS_SUCCESS;
	}

	/* Fill dtl_prm from rule_sel */
	err = prepare_dtl_rule(&dtl_prm, rule_sel, dest_rid);
	if (err) {
		LOG_CRIT("dtl rule preparation - failed. err = %d", err);
		return EIP_STATUS_INTERNAL_ERROR;
	}

	/* Submit the dtl rule to eip-api */
	err = submit_dtl_rule(eip_pub, &dtl_prm, rule_sel->m_pkt.rule_id);
	if (err) {
		LOG_WARN("dtl rule putting - failed. err = %d", err);
		return EIP_STATUS_INTERNAL_ERROR;
	}
	return EIP_STATUS_SUCCESS;
}

int add_rejected_flow_rule(struct selectors *sel)
{
#ifdef ENABLE_REJECTED_FLOW_TO_R3
	struct dtl_params dtl_prm;
	struct rule_selectors rule_sel;
	struct eip_public *eip_pub = get_eip_public();
	int err;

	memset(&rule_sel, 0, sizeof(struct rule_selectors));

	/* Try to delete the rule if it has been already added. Don't care for
	 * return value. If rule is present, it will be deleted; otherwise proceed
	 * further. In any case Reject Flow rule needs to be added
	 */
	delete_lookup_rule(sel);

	/* Fill rule selector struct from selector struct. */
	memcpy(rule_sel.src_ip, sel->sip, IP_SIZE);
	memcpy(rule_sel.dst_ip, sel->dip, IP_SIZE);
	rule_sel.src_port = sel->sport;
	rule_sel.dst_port = sel->dport;
	rule_sel.protocol = sel->proto;

	/* Fill dtl_prm from rule_sel */
	err = prepare_dtl_rule(&dtl_prm, &rule_sel, REJECTED_FLOW_RULE_RING_ID);
	if (err) {
		LOG_CRIT("dtl rule preparation - failed. err = %d", err);
		return EIP_STATUS_INTERNAL_ERROR;
	}

	/* Submit the dtl rule to eip-api */
	err = submit_dtl_rule(eip_pub, &dtl_prm, REJECTED_FLOW_RULE_ID);
	if (err) {
		LOG_CRIT("dtl rule putting - failed. err = %d", err);
		return EIP_STATUS_INTERNAL_ERROR;
	}

	return 0;
#else /* ENABLE_REJECTED_FLOW_TO_R3 */
	LOG_CRIT("ERROR : Reject-Flow Feature is NOT enabled in EIP Driver at Compile time!!!");
	return 1;
#endif /* ENABLE_REJECTED_FLOW_TO_R3 */
}
EXPORT_SYMBOL(add_rejected_flow_rule);

/**
 * @brief Remove all DTL ules from shadow fifo/table and EIP lookup tables.
 *
 * @param eip_pub ptr to eip_public struct.
 * @return int 0 on success; Non-zero on failure.
 */
int flush_dtl_rules(struct eip_public *eip_pub)
{
	int sl_idx;
	bool is_slt;
	int ret = EIP_STATUS_SUCCESS;
	struct shadow_dtl_rec *sltp = NULL;

	if (!eip_pub || !eip_pub->slt_ptr || !eip_pub->sl_fifo_ptr) {
		LOG_CRIT("ERROR: NULL pointer");
		return EIP_STATUS_INVALID_PARAM;
	}
	LOG_DEBG("Flushing EIP rules from shadow table and EIP-lookup tables");

	/* Scan and flush Shd Lookup Record table */
	is_slt = true;
	sltp = eip_pub->slt_ptr;
	for (sl_idx = 0; sl_idx < SLT_RECORD_CNT; sl_idx++) {
		if (sltp[sl_idx].freed)
			continue;
		/* Remove EIP-Lookup record */
		if (delete_dtl_rule(eip_pub, sl_idx, is_slt)) {
			LOG_CRIT("error in removing DTL rule, %d", ret);
			return EIP_STATUS_INTERNAL_ERROR;
		}
		if (remove_shd_lkp_rec(eip_pub, sl_idx, is_slt)) {
			LOG_CRIT("error in removing SL Table record.");
			return EIP_STATUS_INTERNAL_ERROR;
		}
	}
	LOG_CRIT("Cleared SLT");
	/* Scan and flush SL fifo Record table */
	is_slt = false;
	sltp = eip_pub->sl_fifo_ptr;
	for (sl_idx = 0; sl_idx < SL_FIFO_RECORD_CNT; sl_idx++) {
		if (sltp[sl_idx].freed)
			continue;
		/* Remove EIP-Lookup record */
		if (delete_dtl_rule(eip_pub, sl_idx, is_slt)) {
			LOG_CRIT("error in removing DTL rule, %d", ret);
			return EIP_STATUS_INTERNAL_ERROR;
		}
		if (remove_shd_lkp_rec(eip_pub, sl_idx, is_slt)) {
			LOG_CRIT("error in removing SL fifo record.");
			return EIP_STATUS_INTERNAL_ERROR;
		}
	}
	LOG_CRIT("Cleared SL fifo");
	return ret;
}

/**
 * @brief Uninitialise classification engine and lookup table.
 *
 * @param eip_pub eip public structure
 * @param interface_id interface_id Interface id for which lookup table is created.
 * @param flow_hash_table_cnt number of lookup tables that was created.
 *
 * @return EIP_STATUS_SUCCESS on success; otherwise non-zero.
 */
int eip_pcl_dtl_uninit(struct eip_public *eip_pub, int interface_id,
		       int flow_hash_table_cnt)
{
	pcl_dtl_uninit(interface_id);
	pcl_uninit(interface_id);

	uninit_shd_lkp(eip_pub);
	return EIP_STATUS_SUCCESS;
}
