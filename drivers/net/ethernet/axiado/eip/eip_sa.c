// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022-2026 Axiado Corporation.
 */

#ifdef CONFIG_EIP_IPSEC_OFFLOAD
#include "log.h"
#include "da_pec_internal.h"
#include "api_dmabuf.h"
#include "sa_builder.h"
#include "sa_builder_basic.h"
#include "sa_builder_ipsec.h"
#include "sa_builder_params_basic.h"
#include "eip.h"
#include "dtl.h"

/**
 * @brief calculate_inner_outer_keys - calculates the inner and outer digest keys
 * for the hmac key
 *
 * @param params - sa params
 * @param xs - xfrm state pointer
 * @return int - zero on success
 *			   - otherwise negative value
 */
int calculate_inner_outer_keys(sa_builder_params_t *params,
			       sa_builder_params_ipsec_t *ipsec_params,
			       struct xfrm_state *xs,
			       struct hmac_precompute_wq *wq)
{
	int key_byte_count = xs->aalg->alg_key_len / 8;
	u8 *inner_digest, *outer_digest;

	LOG_DEBG("auth algorithm name %s key length %d trunc length %d\n",
		 xs->aalg->alg_name, xs->aalg->alg_key_len,
		 xs->aalg->alg_trunc_len);

	inner_digest = kmalloc(key_byte_count, GFP_KERNEL);
	outer_digest = kmalloc(key_byte_count, GFP_KERNEL);
	if (!inner_digest || !outer_digest) {
		LOG_WARN("memory allocation failed for the inner digest keys\n");
		return -1;
	}
#ifdef ENABLE_REJECTED_FLOW_TO_R3
	da_pec_sync_hmac_precompute(params->auth_algo, xs->aalg->alg_key,
				    key_byte_count, inner_digest, outer_digest,
				    wq);
#else
	da_pec_hmac_precompute(params->auth_algo, xs->aalg->alg_key,
			       key_byte_count, inner_digest, outer_digest)
#endif

	params->auth_key1_p = inner_digest;
	params->auth_key2_p = outer_digest;
	ipsec_params->icv_byte_count = xs->aalg->alg_trunc_len / 8;

	return 0;
}

/**
 * @brief Set the crypto alg params
 * @param params - sa builder params
 * @param xs - pointer to xfrm state
 * @return int - zero if there is no error
 */
int set_crypto_alg(sa_builder_params_t *params, struct xfrm_state *xs)
{
	if (!strcmp(xs->ealg->alg_name, aes_cbc_crypto_name)) {
		LOG_DEBG("aes cbc key length %d\n", xs->ealg->alg_key_len);
		params->crypto_algo = SAB_CRYPTO_AES;
		params->crypto_mode = SAB_CRYPTO_MODE_CBC;
		params->key_byte_count = xs->ealg->alg_key_len / 8;
		params->key_p = xs->ealg->alg_key;
	} else {
		LOG_WARN("unsupported crypto algorithm\n");
		return -1;
	}

	return 0;
}

/**
 * @brief Set the auth alg params
 * @param params - sa builder params
 * @param xs - pointer to xfrm state
 * @return int - zero if there is no error
 */
int set_auth_alg(sa_builder_params_t *params,
		 sa_builder_params_ipsec_t *ipsec_params, struct xfrm_state *xs,
		 struct hmac_precompute_wq *wq)
{
	if (!strcmp(xs->aalg->alg_name, md5_96_hmac_auth_name)) {
		params->auth_algo = SAB_AUTH_HMAC_MD5;
	} else if (!strcmp(xs->aalg->alg_name, md5_128_hmac_auth_name)) {
		params->auth_algo = SAB_AUTH_HMAC_MD5;
	} else if (!strcmp(xs->aalg->alg_name, sha1_96_hmac_auth_name)) {
		params->auth_algo = SAB_AUTH_HMAC_SHA1;
	} else if (!strcmp(xs->aalg->alg_name, sha1_160_hmac_auth_name)) {
		params->auth_algo = SAB_AUTH_HMAC_SHA1;
	} else if (!strcmp(xs->aalg->alg_name, sha2_256_96_hmac_auth_name)) {
		params->auth_algo = SAB_AUTH_HMAC_SHA2_256;
	} else if (!strcmp(xs->aalg->alg_name, sha2_256_128_hmac_auth_name)) {
		params->auth_algo = SAB_AUTH_HMAC_SHA2_256;
	} else if (!strcmp(xs->aalg->alg_name, sha2_384_hmac_auth_name)) {
		params->auth_algo = SAB_AUTH_HMAC_SHA2_384;
	} else if (!strcmp(xs->aalg->alg_name, sha2_512_hmac_auth_name)) {
		params->auth_algo = SAB_AUTH_HMAC_SHA2_512;
	} else {
		LOG_WARN("unsupported auth algorithm\n");
		return -1;
	}

	/* All above supported algorithms are hmac algos */
	if (calculate_inner_outer_keys(params, ipsec_params, xs, wq))
		return -1;

	return 0;
}

/**
 * @brief create ipsec sa
 *
 * @param verbose - true if we need debug logs
 * @param xs - xfrm state
 * @param sa_idx - context reference for ipsec offload
 * @param rx - true if is for rx, otherwise false
 * @param sa - ipsec offload sa
 * @return true - on success
 * @return false - on failure
 */
bool eip_create_ipsec_sa(bool verbose, struct xfrm_state *xs,
			 struct eip_priv *priv, u16 sa_idx, bool rx,
			 struct ipsec_handle *sa)
{
	sa_builder_status_t sa_status;
	sa_builder_params_t params;
	sa_builder_params_ipsec_t ipsec_params;
	unsigned int sa_words = 0;
	int rc;
	bool tunnel = false;
	u32 spi = 0;

	spi |= (xs->id.spi & 0x000000ff) << 24u;
	spi |= (xs->id.spi & 0x0000ff00) << 8u;
	spi |= (xs->id.spi & 0x00ff0000) >> 8u;
	spi |= (xs->id.spi & 0xff000000) >> 24u;

	dma_buf_status_t dma_status;
	dma_buf_properties_t dma_properties = { 0, 0, 0, 0 };

	if (xs->props.mode == XFRM_MODE_TUNNEL)
		tunnel = true;

	sa_status = sa_builder_init_esp(&params, &ipsec_params, spi,
					tunnel ? SAB_IPSEC_TUNNEL : SAB_IPSEC_TRANSPORT,
					SAB_IPSEC_IPV4,
					rx ? SAB_DIRECTION_INBOUND : SAB_DIRECTION_OUTBOUND);
	if (sa_status != SAB_STATUS_OK) {
		LOG_CRIT("DA_PEC_PCL: sa_builder_init_esp failed\n");
		goto error_exit;
	}

	if (set_crypto_alg(&params, xs))
		goto error_exit;

	if (set_auth_alg(&params, &ipsec_params, xs,
			 &priv->eip_pub->hmac_key_offload_wait_q)) {
		goto error_exit;
	}

	if (rx)
		params.redirect_interface = IPSEC_LOOKASIDE_INTERFACE;
	else
		params.redirect_interface = INLINE_INTERFACE_ID;
	params.flags |= SAB_FLAG_REDIRECT;
	// Create a reference to the header processor context.
	ipsec_params.context_ref = sa_idx;

	ipsec_params.ipsec_flags |= SAB_IPSEC_PROCESS_IP_HEADERS;

	if (tunnel) {
		ipsec_params.src_ip_addr_p = (s8 *)&xs->props.saddr.a4;
		ipsec_params.dest_ip_addr_p = (s8 *)&xs->id.daddr.a4;
	}

	sa_status = sa_builder_get_sizes(&params, &sa_words, NULL, NULL);

	if (verbose) {
		LOG_INFO("ret %d SA size=%u words for outbound\n",
			 sa_status, sa_words);
	}
	}
	if (sa_status != SAB_STATUS_OK) {
		LOG_CRIT("DA_PEC_PCL: SA not created because of errors\n");
		goto error_exit;
	}

	// Allocate a DMA-safe buffer for the SA.
	dma_properties.f_cached = true;
	dma_properties.alignment = DA_PEC_DMA_ALIGNMENT_BYTE_COUNT;
	dma_properties.bank = DA_PEC_BANK_SA;
	dma_properties.size = sa_words * sizeof(u32);

	dma_status =
		dma_buf_alloc(dma_properties, &sa->host_address, &sa->handle);
	if (dma_status != DMABUF_STATUS_OK) {
		LOG_CRIT("DA_PEC_PCL Allocation of outbound SA failed\n");
		goto error_exit;
	}

	// Now we can actually build the SA in the DMA-safe buffer.
	sa_status = sa_builder_build_sa(&params, (u32 *)sa->host_address.p,
					NULL, NULL);

	if (sa_status != SAB_STATUS_OK) {
		LOG_CRIT("DA_PEC_PCL: SA not created because of errors\n");
		goto error_dma_exit;
	}
	if (verbose) {
		LOG_INFO("DA_PEC_PCL: Outbound transform record created\n");
		log_hex_dump("DA_PEC_PCL: Outbound transform record", 0,
			     sa->host_address.p, sa_words * sizeof(u32));
	}

	/* Register the SAs */
	rc = pec_sa_register(1, sa->handle, dma_buf_null_handle,
			     dma_buf_null_handle);
	if (rc != PEC_STATUS_OK) {
		LOG_CRIT("pec_sa_register failed\n");
		goto error_dma_exit;
	}

	kfree(params.auth_key1_p);
	kfree(params.auth_key2_p);

	return true;

error_dma_exit:
	dma_buf_release(sa->handle);
error_exit:
	LOG_DEBG("error in creating the transform record\n");
	kfree(params.auth_key1_p);
	kfree(params.auth_key2_p);
	return false;
}

#endif
