// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022-2026 Axiado Corporation.
 */

#include "da_pec_internal.h"

#include "api_dmabuf.h"
#include "api_pec.h"
#include "api_pec_sg.h"

#include "log.h"

#include "clib.h"

#include "eip.h"
#include "sa_builder.h"
#include "sa_builder_basic.h"
#include "sa_builder_ipsec.h"
#include "sa_builder_params_basic.h"
#include "token_builder.h"
#include <linux/delay.h>
#include <linux/netdevice.h>

// eip_device_find()
#include "device_mgmt.h"

// device_read32() and device_write32()
#include "device_rw.h"

#ifdef DEFAULT_IPSEC
#define DEMO_SPI 0xdeadbeef

static u8 example_aes_key[] = { 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
				0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50 };

static u8 example_sha1_hmac_key[] = { 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
				      0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e,
				      0x6f, 0x70, 0x71, 0x72, 0x73, 0x74 };

static u8 example_sha1_inner_digest[] = { 0x19, 0x0E, 0xE4, 0xA1, 0x38,
					  0xFA, 0x10, 0x99, 0xEF, 0x8F,
					  0x3D, 0x75, 0x82, 0xBB, 0xF9,
					  0x9F, 0x63, 0xEA, 0xD7, 0xD1 };

static u8 example_sha1_outer_digest[] = { 0x16, 0x5D, 0x33, 0x6C, 0x5E,
					  0x35, 0xB0, 0x22, 0xA0, 0x3B,
					  0x7B, 0x6D, 0x3E, 0x8B, 0x8A,
					  0xAA, 0x37, 0x2F, 0x2A, 0xF3 };

#endif

// TODO: this should updated with RAMBUS API created TR
/**
 * @brief eip_build_tx_tr Reconfiguring the transform record with the configuration taken from
 * rambus baremetal code
 *
 * @param transf_record - transform record base address
 */
void eip_build_tx_tr(u32 *transf_record)
{
	memset((void *)transf_record, 0, 64 * sizeof(u32));
	transf_record[0] = 0x00000100;
	transf_record[48] = 0x00020000;
	/* word[49]: Rediret enable=true and Destination Interface=0xF */
	transf_record[49] = 0x0000F800;
	transf_record[51] = NON_OFFLOAD_TR_CNTXT_REF;
	/* ohd_proto=36, esp_proto=0 */
	transf_record[52] = 0x00240000;
	transf_record[54] = 0xD0060000;
	transf_record[55] = 0xE156003F;
}

#ifdef DEFAULT_IPSEC
/**
 * @brief eip_create_tx_sa - creates the transform record and registers that with eip
 *
 * @param verbose - verbose logs are set or not
 * @param sa_handles - pointer to hold information about the sa
 * @return true - on successful creation of sa
 * @return false - on failure
 */
bool eip_create_tx_sa(bool verbose, struct sa_handle *sa_handles)
{
	sa_builder_status_t sa_status;
	sa_builder_params_t params;
	sa_builder_params_ipsec_t ipsec_params;
	unsigned int sa_words = 0;
	int rc;

	dma_buf_status_t dma_status;
	dma_buf_properties_t dma_properties = { 0, 0, 0, 0 };

	u8 inner_digest[20], outer_digest[20];

	sa_status = sa_builder_init_esp(&params, &ipsec_params, DEMO_SPI,
					SAB_IPSEC_TRANSPORT, SAB_IPSEC_IPV4,
					SAB_DIRECTION_OUTBOUND);

	if (sa_status != SAB_STATUS_OK) {
		LOG_CRIT("DA_PEC_PCL: sa_builder_init_esp failed\n");
		goto error_exit;
	}

	// Add crypto key and parameters.
	params.crypto_algo = SAB_CRYPTO_AES;
	params.crypto_mode = SAB_CRYPTO_MODE_CBC;
	params.key_byte_count = 16;
	params.key_p = example_aes_key;

	// Add authentication key and parameters.
	params.auth_algo = SAB_AUTH_HMAC_SHA1;

	params.redirect_interface = 15;
	params.flags |= SAB_FLAG_REDIRECT;
	// Create a reference to the header processor context.
	ipsec_params.context_ref = 1;

	ipsec_params.ipsec_flags |= SAB_IPSEC_PROCESS_IP_HEADERS;

	// da_pec_hmac_precompute(SAB_AUTH_HMAC_SHA1, example_sha1_hmac_key, 20,
	//					   inner_digest, outer_digest);

	params.auth_key1_p = example_sha1_inner_digest;
	params.auth_key2_p = example_sha1_outer_digest;

	if (verbose) {
		log_hex_dump("DA_PEC: Inner Digest", 0, inner_digest, 20);
		log_hex_dump("DA_PEC: Outer Digest", 0, outer_digest, 20);
	}
	sa_status = sa_builder_get_sizes(&params, &sa_words, NULL, NULL);

	if (verbose)
		LOG_INFO("rc %d SA size=%u words for outbound\n", sa_status, sa_words);
	if (sa_status != SAB_STATUS_OK) {
		LOG_CRIT("DA_PEC_PCL: SA not created because of errors\n");
		goto error_exit;
	}

	// Allocate a DMA-safe buffer for the SA.
	dma_properties.f_cached = true;
	dma_properties.alignment = DA_PEC_DMA_ALIGNMENT_BYTE_COUNT;
	dma_properties.bank = DA_PEC_BANK_SA;
	dma_properties.size = sa_words * sizeof(u32);

	dma_status = dma_buf_alloc(dma_properties, &sa_handles->sa_host_address,
				   &sa_handles->sa_handle_out);
	if (dma_status != DMABUF_STATUS_OK) {
		LOG_CRIT("DA_PEC_PCL Allocation of outbound SA failed\n");
		goto error_exit;
	}

	// Now we can actually build the SA in the DMA-safe buffer.
	sa_status = sa_builder_build_sa(&params,
					(u32 *)sa_handles->sa_host_address.p, NULL, NULL);

	if (sa_status != SAB_STATUS_OK) {
		LOG_CRIT("DA_PEC_PCL: SA not created because of errors\n");
		goto error_dma_exit;
	}
	if (verbose) {
		LOG_INFO("DA_PEC_PCL: Outbound transform record created\n");

		log_hex_dump("DA_PEC_PCL: Outbound transform record", 0,
			     sa_handles->sa_host_address.p,
			     sa_words * sizeof(u32));
	}

	/* Register the SAs */
	rc = pec_sa_register(1, sa_handles->sa_handle_out, dma_buf_null_handle,
			     dma_buf_null_handle);
	if (rc != PEC_STATUS_OK) {
		LOG_CRIT("pec_sa_register failed\n");
		goto error_dma_exit;
	}

	return true;

error_dma_exit:
	dma_buf_release(sa_handles->sa_handle_out);
error_exit:
	return false;
}

#else
/**
 * @brief eip_create_tx_sa - creates the transform record and registers that with eip
 *
 * @param verbose - verbose logs are set or not
 * @param sa_handles - pointer to hold information about the sa
 * @return true - on successful creation of sa
 * @return false - on failure
 */
bool eip_create_tx_sa(bool verbose, struct sa_handle *sa_handles)
{
	int rc;

	unsigned int sa_words = 0;

	sa_builder_params_t params;
	sa_builder_params_basic_t basic_params;

	dma_buf_status_t dma_status;
	dma_buf_properties_t dma_properties = { 0, 0, 0, 0 };

	memset(&sa_handles->sa_handle_out, 0, sizeof(dma_buf_handle_t));

	sa_handles->tcr_words = 0;
	sa_handles->tcr_data_out = 0;
	sa_handles->token_words = 0;
	sa_handles->token_header_word = 0;
	sa_handles->token_max_words_out = 0;

	rc = sa_builder_init_basic(&params, &basic_params,
				   SAB_DIRECTION_OUTBOUND);
	if (rc != 0) {
		LOG_CRIT("eip sa builder failed\n");
		goto error_exit;
	}

	rc = sa_builder_get_sizes(&params, &sa_words, NULL, NULL);
	if (verbose)
		LOG_INFO("rc %d SA size=%u words for outbound\n", rc, sa_words);
	if (rc != 0) {
		LOG_CRIT("SA not created because of errors\n");
		goto error_exit;
	}

	rc = token_builder_get_context_size(&params, &sa_handles->tcr_words);
	if (rc != 0) {
		LOG_CRIT("token_builder_get_context_size returned errors\n");
		goto error_exit;
	}

	/* The Token Context Record does not need to be allocated
	 * in a DMA-safe buffer.
	 */
	sa_handles->tcr_data_out = da_pec_malloc(4 * sa_handles->tcr_words);
	if (!sa_handles->tcr_data_out) {
		LOG_CRIT("Allocation of outbound TCR failed\n");
		goto error_exit;
	}

	rc = token_builder_build_context(&params, sa_handles->tcr_data_out);
	if (rc != 0) {
		LOG_CRIT("token_builder_build_context failed\n");
		goto error_exit;
	}

	rc = token_builder_get_size(sa_handles->tcr_data_out,
				    &sa_handles->token_max_words_out);
	if (rc != 0) {
		LOG_CRIT("token_builder_get_size failed\n");
		goto error_exit;
	}

	dma_properties.f_cached = true;
	dma_properties.alignment = DA_PEC_DMA_ALIGNMENT_BYTE_COUNT;
	dma_properties.bank = DA_PEC_BANK_SA;
	dma_properties.size = MAX(4 * sa_words, DA_PEC_MIN_SA_BYTE_COUNT);

	dma_status = dma_buf_alloc(dma_properties, &sa_handles->sa_host_address,
				   &sa_handles->sa_handle_out);
	if (dma_status != DMABUF_STATUS_OK) {
		rc = 1;
		LOG_CRIT("Allocation of outbound SA failed\n");
		goto error_exit;
	}

	rc = sa_builder_build_sa(&params, (u32 *)sa_handles->sa_host_address.p,
				 NULL, NULL);
	if (rc != 0) {
		LOG_CRIT("SA not created because of errors\n");
		goto error_exit;
	}

	/* This is done to make inline work and make the code as close as the
	 * baremetal code. This will be removed once we are able to make Rambus
	 * SA APIS works for inline
	 */
	eip_build_tx_tr((u32 *)sa_handles->sa_host_address.p);

	/* Register the SAs */
	rc = pec_sa_register(INTERFACE_ID, sa_handles->sa_handle_out,
			     dma_buf_null_handle, dma_buf_null_handle);
	if (rc != PEC_STATUS_OK) {
		LOG_CRIT("pec_sa_register failed\n");
		goto error_exit;
	}

	dma_properties.f_cached = true;
	dma_properties.alignment = DA_PEC_DMA_ALIGNMENT_BYTE_COUNT;
	dma_properties.bank = DA_PEC_BANK_TOKEN;
	dma_properties.size = 4 * sa_handles->token_max_words_out;

	dma_status = dma_buf_alloc(dma_properties,
				   &sa_handles->token_host_address,
				   &sa_handles->token_handle);
	if (dma_status != DMABUF_STATUS_OK) {
		rc = 1;
		LOG_CRIT("Allocation of token buffer failed.\n");
		goto error_exit;
	}

error_exit:
	if (rc)
		eip_delete_tx_sa(sa_handles);
	return rc == 0;
}
#endif

/**
 * @brief eip_delete_tx_sa - delete tx sa
 *
 * @param sa_handles SA handle
 * @return int 0 on success(always returns 0)
 */
int eip_delete_tx_sa(struct sa_handle *sa_handles)
{
	if (sa_handles) {
		dma_buf_release(sa_handles->sa_handle_out);

		if (sa_handles->tcr_data_out)
			da_pec_free(sa_handles->tcr_data_out);

		dma_buf_release(sa_handles->token_handle);
	}
	return 0;
}

