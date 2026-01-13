// SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0-only)
/*
 * Copyright (c) 2026 Axiado Corporation
 * Copyright (c) 2008-2025 by Rambus, Inc. and/or its subsidiaries.
 */

#include "da_pec_internal.h"

#include "api_pec.h"
#include "api_pec_sg.h"
#include "api_dmabuf.h"

#include "log.h"

#include "clib.h"

#include "sa_builder.h"
#include "sa_builder_basic.h"
#include "token_builder.h"

/****************************************************************
 * Definitions and macros.
 * **************************************************************
 */

#ifdef AXI_MODIFICATIONS
#include "eip.h"
#endif

/*----------------------------------------------------------------------------
 * da_pec_iotoken_create
 */
bool da_pec_iotoken_create(io_token_input_dscr_t *const dscr_p,
			   void *const ext_p, u32 *data_p,
			   pec_command_descriptor_t *const pec_cmd_dscr)
{
	int io_token_rc;

	dscr_p->in_packet_byte_count = pec_cmd_dscr->src_pkt_byte_count;
	dscr_p->ext_p = ext_p;

	io_token_rc = io_token_create(dscr_p, data_p);
	if (io_token_rc < 0) {
		LOG_CRIT("DA_PEC: %s: io_token_create() error %d\n", __func__,
			 io_token_rc);
		return false;
	}

	pec_cmd_dscr->input_token_p = data_p;

	return true; // Success
}

/*----------------------------------------------------------------------------
 * da_pec_pe_get_one
 *
 * Attempt to receive a single packet from the result descriptor ring.
 * Try this a configurable number of times. When a packet is received,
 * check its status and perform header post-processing.
 *
 * out_token_dscr_p (output)
 *      Pointer to the Output Token descriptor that will be updated.
 *
 * out_token_data_p (output)
 *      Pointer to the Output Token buffer where the token will be stored.
 *
 * rd_p (output)
 *      Pointer to the result descriptor that will be updated.
 *
 * Return: 0 if no packet received (timeout),
 *           if packet IO error or if packet processing error.
 *        >0 number of packets received.
 */
unsigned int da_pec_pe_get_one(io_token_output_dscr_t *const out_token_dscr_p,
			       u32 *out_token_data_p,
			       pec_result_descriptor_t *rd_p)
{
	pec_status_t pecres;
	int loop_counter = DA_PEC_PKT_GET_RETRY_COUNT;

	int io_token_rc;

	zeroinit(*out_token_dscr_p);
	zeroinit(*rd_p);

	// Link data structures
	rd_p->output_token_p = out_token_data_p;

	while (loop_counter > 0) {
		// Try to get the processed packet from the driver
		unsigned int counter = 0;

		pecres = pec_packet_get(INTERFACE_ID, rd_p, 1, &counter);
		if (pecres != PEC_STATUS_OK) {
			LOG_CRIT("DA_PEC: pec_packet_get error %d\n", pecres);
			return 0; // IO error
		}

		if (counter) {
			io_token_rc = io_token_parse(out_token_data_p,
						     out_token_dscr_p);
			if (io_token_rc < 0) {
				LOG_CRIT("DA_PEC: io_token_parse error %d\n",
					 io_token_rc);
				return 0; // IO error
			}

			if (out_token_dscr_p->error_code != 0) {
				LOG_CRIT("DA_PEC: Result descriptor error 0x%x\n",
					 out_token_dscr_p->error_code);
				return 0; // Packet processing error
			}

			return counter; // Packets received
		}

		// Wait for DA_PEC_PKT_GET_TIMEOUT_MS milliseconds
		da_pec_usleep(DA_PEC_PKT_GET_TIMEOUT_MS * 1000);
		loop_counter--;
	} // while

	LOG_CRIT("DA_PEC: timeout when reading packet\n");

	return 0; // IO error (timeout, no result packet received)
}

#ifdef DA_PEC_USE_INVALIDATE_COMMANDS
/*----------------------------------------------------------------------------
 * da_pec_invalidate_rec
 */
bool da_pec_invalidate_rec(const dma_buf_handle_t rec_p)
{
	pec_status_t pec_rc;
	pec_command_descriptor_t cmd;
	pec_result_descriptor_t res;

	unsigned int count;

	io_token_input_dscr_t in_token_dscr;
	io_token_output_dscr_t out_token_dscr;
	u32 input_token[IOTOKEN_IN_WORD_COUNT];
	u32 output_token[IOTOKEN_OUT_WORD_COUNT];
	void *in_token_dscr_ext_p = NULL;

#ifdef DA_PEC_IOTOKEN_EXT
	io_token_input_dscr_ext_t in_token_dscr_ext;

	zeroinit(in_token_dscr_ext);
	in_token_dscr_ext_p = &in_token_dscr_ext;
#endif

	zeroinit(in_token_dscr);

	// Fill in the command descriptor for the Invalidate command
	zeroinit(cmd);

	cmd.src_pkt_handle = dma_buf_null_handle;
	cmd.dst_pkt_handle = dma_buf_null_handle;
	cmd.sa_handle1 = rec_p;
	cmd.sa_handle2 = dma_buf_null_handle;
	cmd.token_handle = dma_buf_null_handle;

#if defined(DA_PEC_IOTOKEN_EXT)
	in_token_dscr_ext.hw_services = IOTOKEN_CMD_INV_TR;
#endif

	if (!da_pec_iotoken_create(&in_token_dscr, in_token_dscr_ext_p,
				   input_token, &cmd))
		return false;

	// Issue command
	pec_rc = pec_packet_put(INTERFACE_ID, &cmd, 1, &count);
	if (pec_rc != PEC_STATUS_OK || count != 1) {
		LOG_CRIT("DA_PEC: %s: pec_packet_put() error %d, count %d\n",
			 __func__, pec_rc, count);
		return false;
	}

	// Receive the result packet ... do we care about contents ?
	if (da_pec_pe_get_one(&out_token_dscr, output_token, &res) < 1) {
		LOG_CRIT("DA_PEC: %s: da_pec_pe_get_one() failed\n", __func__);
		return false;
	}

	return true;
}
#endif

/*----------------------------------------------------------------------------
 * da_pec_aes_block_encrypt
 *
 * Encrypt a single AES block.
 */
bool da_pec_aes_block_encrypt(u8 *key_p, unsigned int key_byte_count,
			      u8 *in_data_p, u8 *out_data_p)
{
	int rc;
	sa_builder_params_t params;
	sa_builder_params_basic_t protocol_params;
	unsigned int sa_words = 0;

	dma_buf_status_t dma_status;
	dma_buf_properties_t dma_properties = { 0, 0, 0, 0 };
	dma_buf_host_address_t sa_host_address;
	dma_buf_host_address_t token_host_address;
	dma_buf_host_address_t pkt_host_address;

	dma_buf_handle_t sa_handle = { 0 };
	dma_buf_handle_t token_handle = { 0 };
	dma_buf_handle_t pkt_handle = { 0 };

	unsigned int tcr_words = 0;
	void *tcr_data = 0;
	unsigned int token_words = 0;
	unsigned int token_header_word;
	unsigned int token_max_words = 0;

	token_builder_params_t token_params;
	pec_command_descriptor_t cmd;
	pec_result_descriptor_t res;
	unsigned int count;

	io_token_input_dscr_t in_token_dscr;
	io_token_output_dscr_t out_token_dscr;
	u32 input_token[IOTOKEN_IN_WORD_COUNT];
	u32 output_token[IOTOKEN_OUT_WORD_COUNT];
	void *in_token_dscr_ext_p = NULL;

#ifdef DA_PEC_IOTOKEN_EXT
	io_token_input_dscr_ext_t in_token_dscr_ext;

	zeroinit(in_token_dscr_ext);
	in_token_dscr_ext_p = &in_token_dscr_ext;
#endif

	zeroinit(in_token_dscr);
	zeroinit(out_token_dscr);

	rc = sa_builder_init_basic(&params, &protocol_params,
				   SAB_DIRECTION_OUTBOUND);
	if (rc != 0) {
		LOG_CRIT("sa_builder_init_basic failed\n");
		goto error_exit;
	}
	// Add crypto key and parameters.
	params.crypto_algo = SAB_CRYPTO_AES;
	params.crypto_mode = SAB_CRYPTO_MODE_ECB;
	params.key_byte_count = key_byte_count;
	params.key_p = key_p;

	rc = sa_builder_get_sizes(&params, &sa_words, NULL, NULL);

	if (rc != 0) {
		LOG_CRIT("SA not created because of errors\n");
		goto error_exit;
	}

	dma_properties.f_cached = true;
	dma_properties.alignment = DA_PEC_DMA_ALIGNMENT_BYTE_COUNT;
	dma_properties.bank = DA_PEC_BANK_SA;
	dma_properties.size = MAX(4 * sa_words, DA_PEC_MIN_SA_BYTE_COUNT);

	dma_status =
		dma_buf_alloc(dma_properties, &sa_host_address, &sa_handle);
	if (dma_status != DMABUF_STATUS_OK) {
		rc = 1;
		LOG_CRIT("Allocation of SA failed\n");
		goto error_exit;
	}

	rc = sa_builder_build_sa(&params, (u32 *)sa_host_address.p, NULL, NULL);

	if (rc != 0) {
		LOG_CRIT("SA not created because of errors\n");
		goto error_exit;
	}

	rc = token_builder_get_context_size(&params, &tcr_words);

	if (rc != 0) {
		LOG_CRIT("token_builder_get_context_size returned errors\n");
		goto error_exit;
	}

	// The Token Context Record does not need to be allocated
	// in a DMA-safe buffer.
	tcr_data = da_pec_malloc(4 * tcr_words);
	if (!tcr_data) {
		rc = 1;
		LOG_CRIT("Allocation of TCR failed\n");
		goto error_exit;
	}

	rc = token_builder_build_context(&params, tcr_data);

	if (rc != 0) {
		LOG_CRIT("token_builder_build_context failed\n");
		goto error_exit;
	}

	rc = token_builder_get_size(tcr_data, &token_max_words);
	if (rc != 0) {
		LOG_CRIT("token_builder_get_size failed\n");
		goto error_exit;
	}

	// Allocate one buffer for the token and two packet buffers.

	dma_properties.f_cached = true;
	dma_properties.alignment = DA_PEC_DMA_ALIGNMENT_BYTE_COUNT;
	dma_properties.bank = DA_PEC_BANK_TOKEN;
	dma_properties.size = 4 * token_max_words;

	dma_status = dma_buf_alloc(dma_properties, &token_host_address,
				   &token_handle);
	if (dma_status != DMABUF_STATUS_OK) {
		rc = 1;
		LOG_CRIT("Allocation of token buffer failed.\n");
		goto error_exit;
	}

	dma_properties.f_cached = true;
	dma_properties.alignment = DA_PEC_DMA_ALIGNMENT_BYTE_COUNT;
	dma_properties.bank = DA_PEC_BANK_PACKET;
	dma_properties.size = 16;

	dma_status =
		dma_buf_alloc(dma_properties, &pkt_host_address, &pkt_handle);
	if (dma_status != DMABUF_STATUS_OK) {
		rc = 1;
		LOG_CRIT("Allocation of source packet buffer failed.\n");
		goto error_exit;
	}

	// Register the SA
	rc = pec_sa_register(INTERFACE_ID, sa_handle, dma_buf_null_handle,
			     dma_buf_null_handle);
	if (rc != PEC_STATUS_OK) {
		LOG_CRIT("pec_sa_register failed\n");
		goto error_exit;
	}

	// Copy input packet into source packet buffer.
	memcpy(pkt_host_address.p, in_data_p, 16);

	// Set Token Parameters if specified in test vector.
	zeroinit(token_params);

	// Prepare a token to process the packet.
	rc = token_builder_build_token(tcr_data, (u8 *)pkt_host_address.p, 16,
				       &token_params,
				       (u32 *)token_host_address.p,
				       &token_words, &token_header_word);
	if (rc != TKB_STATUS_OK) {
		LOG_CRIT("Token builder failed, rc: %d\n", rc);
		goto error_exit_unregister;
	}

	zeroinit(cmd);
	cmd.token_handle = token_handle;
	cmd.token_word_count = token_words;
	cmd.src_pkt_handle = pkt_handle;
	cmd.src_pkt_byte_count = 16;
	cmd.dst_pkt_handle = pkt_handle;
	cmd.sa_handle1 = sa_handle;
	cmd.sa_handle2 = dma_buf_null_handle;

#ifdef DA_PEC_IOTOKEN_EXT
	in_token_dscr_ext.hw_services = IOTOKEN_CMD_PKT_LAC;
#endif
	in_token_dscr.tkn_hdr_word_init = token_header_word;

	if (!da_pec_iotoken_create(&in_token_dscr, in_token_dscr_ext_p,
				   input_token, &cmd)) {
		rc = 1;
		goto error_exit_unregister;
	}

	rc = pec_packet_put(INTERFACE_ID, &cmd, 1, &count);
	if (rc != PEC_STATUS_OK && count != 1) {
		rc = 1;
		LOG_CRIT("%s(%d) error\n", __func__, __LINE__);
		goto error_exit_unregister;
	}

	if (da_pec_pe_get_one(&out_token_dscr, output_token, &res) == 0) {
		rc = 1;
		LOG_CRIT("error from da_pec_pe_get_one\n");
		goto error_exit_unregister;
	}
	memcpy(out_data_p, pkt_host_address.p, 16);

error_exit_unregister:
#ifdef DA_PEC_USE_INVALIDATE_COMMANDS
	if (!da_pec_invalidate_rec(sa_handle))
		rc = 1;
#endif
	pec_sa_un_register(INTERFACE_ID, sa_handle, dma_buf_null_handle,
			   dma_buf_null_handle);

error_exit:
	dma_buf_release(sa_handle);
	dma_buf_release(token_handle);
	dma_buf_release(pkt_handle);

	if (tcr_data)
		da_pec_free(tcr_data);

	return rc == 0;
}

/*----------------------------------------------------------------------------
 * da_pec_aes_gcm_hkey
 *
 * Compute the H_KEY for AES-GCM
 */
bool da_pec_aes_gcm_hkey(u8 *key_p, unsigned int key_byte_count, u8 *h_key_p)
{
	static u8 zero[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

	if (!da_pec_aes_block_encrypt(key_p, key_byte_count, zero, h_key_p))
		return false;
	// byte-swap the HKEY
	{
		u8 t;
		unsigned int i;

		for (i = 0; i < 4; i++) {
			t = h_key_p[4 * i + 3];
			h_key_p[4 * i + 3] = h_key_p[4 * i];
			h_key_p[4 * i] = t;
			t = h_key_p[4 * i + 2];
			h_key_p[4 * i + 2] = h_key_p[4 * i + 1];
			h_key_p[4 * i + 1] = t;
		}
	}
	return true;
}

/*----------------------------------------------------------------------------
 * da_pec_basic_hash
 */
bool da_pec_basic_hash(sa_builder_auth_t hash_algo, u8 *input_p,
		       unsigned int input_byte_count, u8 *output_p,
		       unsigned int output_byte_count, bool f_finalize)
{
	int rc;
	sa_builder_params_t params;
	sa_builder_params_basic_t protocol_params;
	unsigned int sa_words = 0;

	static u8 dummy_auth_key[64];

	dma_buf_status_t dma_status;
	dma_buf_properties_t dma_properties = { 0, 0, 0, 0 };
	dma_buf_host_address_t sa_host_address;
	dma_buf_host_address_t token_host_address;
	dma_buf_host_address_t pkt_host_address;

	dma_buf_handle_t sa_handle = { 0 };
	dma_buf_handle_t token_handle = { 0 };
	dma_buf_handle_t pkt_handle = { 0 };

	unsigned int tcr_words = 0;
	void *tcr_data = 0;
	unsigned int token_words = 0;
	unsigned int token_header_word;
	unsigned int token_max_words = 0;

	token_builder_params_t token_params;
	pec_command_descriptor_t cmd;
	pec_result_descriptor_t res;
	unsigned int count;

	io_token_input_dscr_t in_token_dscr;
	io_token_output_dscr_t out_token_dscr;
	u32 input_token[IOTOKEN_IN_WORD_COUNT];
	u32 output_token[IOTOKEN_OUT_WORD_COUNT];
	void *in_token_dscr_ext_p = NULL;

#ifdef DA_PEC_IOTOKEN_EXT
	io_token_input_dscr_ext_t in_token_dscr_ext;

	zeroinit(in_token_dscr_ext);
	in_token_dscr_ext_p = &in_token_dscr_ext;
#endif

	zeroinit(in_token_dscr);
	zeroinit(out_token_dscr);

	rc = sa_builder_init_basic(&params, &protocol_params,
				   SAB_DIRECTION_OUTBOUND);
	if (rc != 0) {
		LOG_CRIT("DA_PEC: sa_builder_init_basic failed\n");
		goto error_exit;
	}
	// Add crypto key and parameters.
	params.auth_algo = hash_algo;
	params.auth_key1_p = dummy_auth_key;
	/* Dummy authentication key must be supplied when SAB_FLAG_HASH_INTERMEDIATE is set. */
	if (!f_finalize)
		params.flags |= SAB_FLAG_HASH_SAVE | SAB_FLAG_HASH_INTERMEDIATE;
	params.flags |= SAB_FLAG_SUPPRESS_PAYLOAD;
	protocol_params.icv_byte_count = output_byte_count;

	rc = sa_builder_get_sizes(&params, &sa_words, NULL, NULL);

	if (rc != 0) {
		LOG_CRIT("DA_PEC: SA not created because of errors\n");
		goto error_exit;
	}

	dma_properties.f_cached = true;
	dma_properties.alignment = DA_PEC_DMA_ALIGNMENT_BYTE_COUNT;
	dma_properties.bank = DA_PEC_BANK_SA;
	dma_properties.size = MAX(4 * sa_words, DA_PEC_MIN_SA_BYTE_COUNT);

	dma_status =
		dma_buf_alloc(dma_properties, &sa_host_address, &sa_handle);
	if (dma_status != DMABUF_STATUS_OK) {
		rc = 1;
		LOG_CRIT("DA_PEC: Allocation of SA failed\n");
		goto error_exit;
	}

	rc = sa_builder_build_sa(&params, (u32 *)sa_host_address.p, NULL, NULL);

	if (rc != 0) {
		LOG_CRIT("DA_PEC: SA not created because of errors\n");
		goto error_exit;
	}

	rc = token_builder_get_context_size(&params, &tcr_words);

	if (rc != 0) {
		LOG_CRIT("DA_PEC: token_builder_get_context_size returned errors\n");
		goto error_exit;
	}

	// The Token Context Record does not need to be allocated
	// in a DMA-safe buffer.
	tcr_data = da_pec_malloc(4 * tcr_words);
	if (!tcr_data) {
		rc = 1;
		LOG_CRIT("DA_PEC: Allocation of TCR failed\n");
		goto error_exit;
	}

	rc = token_builder_build_context(&params, tcr_data);

	if (rc != 0) {
		LOG_CRIT("DA_PEC: token_builder_build_context failed\n");
		goto error_exit;
	}

	rc = token_builder_get_size(tcr_data, &token_max_words);
	if (rc != 0) {
		LOG_CRIT("DA_PEC: token_builder_get_size failed\n");
		goto error_exit;
	}

	// Allocate one buffer for the token and two packet buffers.

	dma_properties.f_cached = true;
	dma_properties.alignment = DA_PEC_DMA_ALIGNMENT_BYTE_COUNT;
	dma_properties.bank = DA_PEC_BANK_TOKEN;
	dma_properties.size = 4 * token_max_words;

	dma_status = dma_buf_alloc(dma_properties, &token_host_address,
				   &token_handle);
	if (dma_status != DMABUF_STATUS_OK) {
		rc = 1;
		LOG_CRIT("DA_PEC: Allocation of token buffer failed.\n");
		goto error_exit;
	}

	dma_properties.f_cached = true;
	dma_properties.alignment = DA_PEC_DMA_ALIGNMENT_BYTE_COUNT;
	dma_properties.bank = DA_PEC_BANK_PACKET;
	dma_properties.size = MAX(input_byte_count, output_byte_count);

	dma_status =
		dma_buf_alloc(dma_properties, &pkt_host_address, &pkt_handle);
	if (dma_status != DMABUF_STATUS_OK) {
		rc = 1;
		LOG_CRIT("DA_PEC: Allocation of source packet buffer failed.\n");
		goto error_exit;
	}

	// Register the SA
	rc = pec_sa_register(INTERFACE_ID, sa_handle, dma_buf_null_handle,
			     dma_buf_null_handle);
	if (rc != PEC_STATUS_OK) {
		LOG_CRIT("DA_PEC: pec_sa_register failed\n");
		goto error_exit;
	}

	// Copy input packet into source packet buffer.
	memcpy(pkt_host_address.p, input_p, input_byte_count);

	// Set Token Parameters if specified in test vector.
	zeroinit(token_params);
	token_params.packet_flags |= TKB_PACKET_FLAG_HASHFIRST |
				     TKB_PACKET_FLAG_HASHAPPEND;
	if (f_finalize)
		token_params.packet_flags |= TKB_PACKET_FLAG_HASHFINAL;

	// Prepare a token to process the packet.
	rc = token_builder_build_token(tcr_data, (u8 *)pkt_host_address.p,
				       input_byte_count, &token_params,
				       (u32 *)token_host_address.p,
				       &token_words, &token_header_word);
	if (rc != TKB_STATUS_OK) {
		LOG_CRIT("DA_PEC: Token builder failed, rc: %d\n", rc);
		goto error_exit_unregister;
	}

	zeroinit(cmd);
	cmd.token_handle = token_handle;
	cmd.token_word_count = token_words;
	cmd.src_pkt_handle = pkt_handle;
	cmd.src_pkt_byte_count = input_byte_count;
	cmd.dst_pkt_handle = pkt_handle;
	cmd.sa_handle1 = sa_handle;
	cmd.sa_handle2 = dma_buf_null_handle;

#if defined(DA_PEC_IOTOKEN_EXT)
	in_token_dscr_ext.hw_services = IOTOKEN_CMD_PKT_LAC;
#endif
	in_token_dscr.tkn_hdr_word_init = token_header_word;

	if (!da_pec_iotoken_create(&in_token_dscr, in_token_dscr_ext_p,
				   input_token, &cmd)) {
		rc = 1;
		goto error_exit_unregister;
	}

	rc = pec_packet_put(INTERFACE_ID, &cmd, 1, &count);
	if (rc != PEC_STATUS_OK && count != 1) {
		rc = 1;
		LOG_CRIT("DA_PEC: pec_packet_put error\n");
		goto error_exit_unregister;
	}

	if (da_pec_pe_get_one(&out_token_dscr, output_token, &res) < 1) {
		rc = 1;
		LOG_CRIT("DA_PEC: error from da_pec_pe_get_one\n");
		goto error_exit_unregister;
	}
	memcpy(output_p, pkt_host_address.p, output_byte_count);

error_exit_unregister:
#ifdef DA_PEC_USE_INVALIDATE_COMMANDS
	if (!da_pec_invalidate_rec(sa_handle))
		rc = 1;
#endif
	pec_sa_un_register(INTERFACE_ID, sa_handle, dma_buf_null_handle,
			   dma_buf_null_handle);

error_exit:
	dma_buf_release(sa_handle);
	dma_buf_release(token_handle);
	dma_buf_release(pkt_handle);

	if (tcr_data)
		da_pec_free(tcr_data);

	return rc == 0;
}

#ifndef DA_PEC_HAVE_HW_PRECOMPUTE
/*----------------------------------------------------------------------------
 * da_pec_hmac_precompute standard implementation with several hash ops
 */
bool da_pec_hmac_precompute(sa_builder_auth_t auth_algo, u8 *auth_key_p,
			    unsigned int auth_key_byte_count, u8 *inner_p,
			    u8 *outer_p)
{
	sa_builder_auth_t hash_algo;
	unsigned int blocksize, hashsize, digestsize;
	static u8 pad_block[128], hashed_key[128];
	unsigned int i;

	switch (auth_algo) {
	case SAB_AUTH_HMAC_MD5:
		hash_algo = SAB_AUTH_HASH_MD5;
		blocksize = 64;
		hashsize = 16;
		digestsize = 16;
		break;
	case SAB_AUTH_HMAC_SHA1:
		hash_algo = SAB_AUTH_HASH_SHA1;
		blocksize = 64;
		hashsize = 20;
		digestsize = 20;
		break;
	case SAB_AUTH_HMAC_SHA2_224:
		hash_algo = SAB_AUTH_HASH_SHA2_224;
		blocksize = 64;
		hashsize = 28;
		digestsize = 32;
		break;
	case SAB_AUTH_HMAC_SHA2_256:
		hash_algo = SAB_AUTH_HASH_SHA2_256;
		blocksize = 64;
		hashsize = 32;
		digestsize = 32;
		break;
	case SAB_AUTH_HMAC_SHA2_384:
		hash_algo = SAB_AUTH_HASH_SHA2_384;
		blocksize = 128;
		hashsize = 48;
		digestsize = 64;
		break;
	case SAB_AUTH_HMAC_SHA2_512:
		hash_algo = SAB_AUTH_HASH_SHA2_512;
		blocksize = 128;
		hashsize = 64;
		digestsize = 64;
		break;
	default:
		LOG_CRIT("DA_PEC: Unknown HMAC algorithm\n");
		return false;
	}

	memset(hashed_key, 0, blocksize);
	if (auth_key_byte_count <= blocksize) {
		memcpy(hashed_key, auth_key_p, auth_key_byte_count);
	} else {
		if (!da_pec_basic_hash(hash_algo, auth_key_p,
				       auth_key_byte_count, hashed_key,
				       hashsize, true)) {
			return false;
		}
	}

	for (i = 0; i < blocksize; i++)
		pad_block[i] = hashed_key[i] ^ 0x36;
	if (!da_pec_basic_hash(hash_algo, pad_block, blocksize, inner_p,
			       digestsize, false)) {
		return false;
	}

	for (i = 0; i < blocksize; i++)
		pad_block[i] = hashed_key[i] ^ 0x5c;
	if (!da_pec_basic_hash(hash_algo, pad_block, blocksize, outer_p,
			       digestsize, false)) {
		return false;
	}
	return true;
}

#else
/*----------------------------------------------------------------------------
 * da_pec_hmac_precompute complete operation in single packet
 *
 * This operation will build a separate context to do the precompute
 * and provide the precomputes as packet output data.
 */
bool da_pec_hmac_precompute(sa_builder_auth_t auth_algo, u8 *auth_key_p,
			    unsigned int auth_key_byte_count, u8 *inner_p,
			    u8 *outer_p)
{
	int rc;
	sa_builder_params_t params;
	sa_builder_params_basic_t protocol_params;
	unsigned int sa_words = 0;

	dma_buf_status_t dma_status;
	dma_buf_properties_t dma_properties = { 0, 0, 0, 0 };
	dma_buf_host_address_t sa_host_address;
	dma_buf_host_address_t token_host_address;
	dma_buf_host_address_t pkt_host_address;

	dma_buf_handle_t sa_handle = { 0 };
	dma_buf_handle_t token_handle = { 0 };
	dma_buf_handle_t pkt_handle = { 0 };

	unsigned int tcr_words = 0;
	void *tcr_data = 0;
	unsigned int token_words = 0;
	unsigned int token_header_word;
	unsigned int token_max_words = 0;

	token_builder_params_t token_params;
	pec_command_descriptor_t cmd;
	pec_result_descriptor_t res;
	unsigned int count;

	io_token_input_dscr_t in_token_dscr;
	io_token_output_dscr_t out_token_dscr;
	u32 input_token[IOTOKEN_IN_WORD_COUNT];
	u32 output_token[IOTOKEN_OUT_WORD_COUNT];
	void *in_token_dscr_ext_p = NULL;

#ifdef DA_PEC_IOTOKEN_EXT
	io_token_input_dscr_ext_t in_token_dscr_ext;

	zeroinit(in_token_dscr_ext);
	in_token_dscr_ext_p = &in_token_dscr_ext;
#endif

	zeroinit(in_token_dscr);
	zeroinit(out_token_dscr);

	rc = sa_builder_init_basic(&params, &protocol_params,
				   SAB_DIRECTION_OUTBOUND);
	if (rc != 0) {
		LOG_CRIT("sa_builder_init_basic failed\n");
		goto error_exit;
	}
	// Add crypto key and parameters.
	params.auth_algo = auth_algo;
	protocol_params.basic_flags |= SAB_BASIC_FLAG_HMAC_PRECOMPUTE;
	rc = sa_builder_get_sizes(&params, &sa_words, NULL, NULL);

	if (rc != 0) {
		LOG_CRIT("SA not created because of errors\n");
		goto error_exit;
	}

	dma_properties.f_cached = true;
	dma_properties.alignment = DA_PEC_DMA_ALIGNMENT_BYTE_COUNT;
	dma_properties.bank = DA_PEC_BANK_SA;
	dma_properties.size = MAX(4 * sa_words, DA_PEC_MIN_SA_BYTE_COUNT);

	dma_status =
		dma_buf_alloc(dma_properties, &sa_host_address, &sa_handle);
	if (dma_status != DMABUF_STATUS_OK) {
		rc = 1;
		LOG_CRIT("Allocation of SA failed\n");
		goto error_exit;
	}

	rc = sa_builder_build_sa(&params, (u32 *)sa_host_address.p, NULL, NULL);

	if (rc != 0) {
		LOG_CRIT("SA not created because of errors\n");
		goto error_exit;
	}

	rc = token_builder_get_context_size(&params, &tcr_words);

	if (rc != 0) {
		LOG_CRIT("token_builder_get_context_size returned errors\n");
		goto error_exit;
	}

	// The Token Context Record does not need to be allocated
	// in a DMA-safe buffer.
	tcr_data = da_pec_malloc(4 * tcr_words);
	if (!tcr_data) {
		rc = 1;
		LOG_CRIT("Allocation of TCR failed\n");
		goto error_exit;
	}

	rc = token_builder_build_context(&params, tcr_data);

	if (rc != 0) {
		LOG_CRIT("token_builder_build_context failed\n");
		goto error_exit;
	}

	rc = token_builder_get_size(tcr_data, &token_max_words);
	if (rc != 0) {
		LOG_CRIT("token_builder_get_size failed\n");
		goto error_exit;
	}

	// Allocate one buffer for the token and two packet buffers.

	dma_properties.f_cached = true;
	dma_properties.alignment = DA_PEC_DMA_ALIGNMENT_BYTE_COUNT;
	dma_properties.bank = DA_PEC_BANK_TOKEN;
	dma_properties.size = 4 * token_max_words;

	dma_status = dma_buf_alloc(dma_properties, &token_host_address,
				   &token_handle);
	if (dma_status != DMABUF_STATUS_OK) {
		rc = 1;
		LOG_CRIT("Allocation of token buffer failed.\n");
		goto error_exit;
	}

	dma_properties.f_cached = true;
	dma_properties.alignment = DA_PEC_DMA_ALIGNMENT_BYTE_COUNT;
	dma_properties.bank = DA_PEC_BANK_PACKET;
	dma_properties.size = MAX(64, auth_key_byte_count);

	dma_status =
		dma_buf_alloc(dma_properties, &pkt_host_address, &pkt_handle);
	if (dma_status != DMABUF_STATUS_OK) {
		rc = 1;
		LOG_CRIT("Allocation of source packet buffer failed.\n");
		goto error_exit;
	}

	// Register the SA
	rc = pec_sa_register(INTERFACE_ID, sa_handle, dma_buf_null_handle,
			     dma_buf_null_handle);
	if (rc != PEC_STATUS_OK) {
		LOG_CRIT("pec_sa_register failed\n");
		goto error_exit;
	}

	// Copy input packet into source packet buffer.
	memcpy(pkt_host_address.p, auth_key_p, auth_key_byte_count);

	// Set Token Parameters if specified in test vector.
	zeroinit(token_params);

	// Prepare a token to process the packet.
	rc = token_builder_build_token(tcr_data, (u8 *)pkt_host_address.p,
				       auth_key_byte_count, &token_params,
				       (u32 *)token_host_address.p,
				       &token_words, &token_header_word);
	if (rc != TKB_STATUS_OK) {
		LOG_CRIT("Token builder failed, rc: %d\n", rc);
		goto error_exit_unregister;
	}

	zeroinit(cmd);
	cmd.token_handle = token_handle;
	cmd.token_word_count = token_words;
	cmd.src_pkt_handle = pkt_handle;
	cmd.src_pkt_byte_count = auth_key_byte_count;
	cmd.dst_pkt_handle = pkt_handle;
	cmd.sa_handle1 = sa_handle;
	cmd.sa_handle2 = dma_buf_null_handle;

#ifdef DA_PEC_IOTOKEN_EXT
	in_token_dscr_ext.hw_services = IOTOKEN_CMD_PKT_LAC;
#endif
	in_token_dscr.tkn_hdr_word_init = token_header_word;

	if (!da_pec_iotoken_create(&in_token_dscr, in_token_dscr_ext_p,
				   input_token, &cmd)) {
		rc = 1;
		goto error_exit_unregister;
	}

	rc = pec_packet_put(INTERFACE_ID, &cmd, 1, &count);
	if (rc != PEC_STATUS_OK && count != 1) {
		rc = 1;
		LOG_CRIT("%s(%d) error\n", __func__, __LINE__);
		goto error_exit_unregister;
	}

	if (da_pec_pe_get_one(&out_token_dscr, output_token, &res) == 0) {
		rc = 1;
		LOG_CRIT("error from da_pec_pe_get_one\n");
		goto error_exit_unregister;
	}

	memcpy(inner_p, pkt_host_address.p, res.dst_pkt_byte_count / 2);
	memcpy(outer_p, pkt_host_address.p + res.dst_pkt_byte_count / 2,
	       res.dst_pkt_byte_count / 2);

error_exit_unregister:
#ifdef DA_PEC_USE_INVALIDATE_COMMANDS
	if (!da_pec_invalidate_rec(sa_handle))
		rc = 1;
#endif
	pec_sa_un_register(INTERFACE_ID, sa_handle, dma_buf_null_handle,
			   dma_buf_null_handle);

error_exit:
	dma_buf_release(sa_handle);
	dma_buf_release(token_handle);
	dma_buf_release(pkt_handle);

	if (tcr_data)
		da_pec_free(tcr_data);

	return rc == 0;
}

/*----------------------------------------------------------------------------
 * da_pec_hmac_hw_precompute
 */
bool da_pec_hmac_hw_precompute(u8 *auth_key_p, unsigned int auth_key_byte_count,
			       dma_buf_handle_t sa_handle, void *token_context)
{
	int rc;

	dma_buf_status_t dma_status;
	dma_buf_properties_t dma_properties = { 0, 0, 0, 0 };
	dma_buf_host_address_t token_host_address;
	dma_buf_host_address_t pkt_host_address;

	dma_buf_handle_t token_handle = { 0 };
	dma_buf_handle_t pkt_handle = { 0 };

	unsigned int token_words = 0;
	unsigned int token_header_word;
	unsigned int token_max_words = 0;

	token_builder_params_t token_params;
	pec_command_descriptor_t cmd;
	pec_result_descriptor_t res;
	unsigned int count;

	io_token_input_dscr_t in_token_dscr;
	io_token_output_dscr_t out_token_dscr;
	u32 input_token[IOTOKEN_IN_WORD_COUNT];
	u32 output_token[IOTOKEN_OUT_WORD_COUNT];
	void *in_token_dscr_ext_p = NULL;

#ifdef DA_PEC_IOTOKEN_EXT
	io_token_input_dscr_ext_t in_token_dscr_ext;

	zeroinit(in_token_dscr_ext);
	in_token_dscr_ext_p = &in_token_dscr_ext;
#endif

	zeroinit(in_token_dscr);
	zeroinit(out_token_dscr);

	rc = token_builder_get_size(token_context, &token_max_words);
	if (rc != 0) {
		LOG_CRIT("token_builder_get_size failed\n");
		goto error_exit;
	}

	// Allocate one buffer for the token and one packet buffer.

	dma_properties.f_cached = true;
	dma_properties.alignment = DA_PEC_DMA_ALIGNMENT_BYTE_COUNT;
	dma_properties.bank = DA_PEC_BANK_TOKEN;
	dma_properties.size = 4 * token_max_words;

	dma_status = dma_buf_alloc(dma_properties, &token_host_address,
				   &token_handle);
	if (dma_status != DMABUF_STATUS_OK) {
		rc = 1;
		LOG_CRIT("Allocation of token buffer failed.\n");
		goto error_exit;
	}

	dma_properties.f_cached = true;
	dma_properties.alignment = DA_PEC_DMA_ALIGNMENT_BYTE_COUNT;
	dma_properties.bank = DA_PEC_BANK_PACKET;
	dma_properties.size = auth_key_byte_count;

	dma_status =
		dma_buf_alloc(dma_properties, &pkt_host_address, &pkt_handle);
	if (dma_status != DMABUF_STATUS_OK) {
		rc = 1;
		LOG_CRIT("Allocation of source packet buffer failed.\n");
		goto error_exit;
	}

	// Copy input packet into source packet buffer.
	memcpy(pkt_host_address.p, auth_key_p, auth_key_byte_count);

	// Set Token Parameters if specified in test vector.
	zeroinit(token_params);

	// Prepare a token to process the packet.
	rc = token_builder_build_token(token_context, (u8 *)pkt_host_address.p,
				       auth_key_byte_count, &token_params,
				       (u32 *)token_host_address.p,
				       &token_words, &token_header_word);
	if (rc != TKB_STATUS_OK) {
		LOG_CRIT("Token builder failed, rc: %d\n", rc);
		goto error_exit;
	}

	zeroinit(cmd);
	cmd.token_handle = token_handle;
	cmd.token_word_count = token_words;
	cmd.src_pkt_handle = pkt_handle;
	cmd.src_pkt_byte_count = auth_key_byte_count;
	cmd.dst_pkt_handle = pkt_handle;
	cmd.sa_handle1 = sa_handle;
	cmd.sa_handle2 = dma_buf_null_handle;

#ifdef DA_PEC_IOTOKEN_EXT
	in_token_dscr_ext.hw_services = IOTOKEN_CMD_PKT_LAC;
#endif
	in_token_dscr.tkn_hdr_word_init = token_header_word;

	if (!da_pec_iotoken_create(&in_token_dscr, in_token_dscr_ext_p,
				   input_token, &cmd)) {
		rc = 1;
		goto error_exit;
	}

	rc = pec_packet_put(INTERFACE_ID, &cmd, 1, &count);
	if (rc != PEC_STATUS_OK && count != 1) {
		rc = 1;
		LOG_CRIT("%s(%d) error\n", __func__, __LINE__);
		goto error_exit;
	}

	if (da_pec_pe_get_one(&out_token_dscr, output_token, &res) == 0) {
		rc = 1;
		LOG_CRIT("error from da_pec_pe_get_one\n");
		goto error_exit;
	}

error_exit:
	dma_buf_release(token_handle);
	dma_buf_release(pkt_handle);

	return rc == 0;
}
#endif

/*----------------------------------------------------------------------------
 * da_pec_shift_cmac_subkey
 */
void da_pec_shift_cmac_subkey(u8 *inp, u8 *outp)
{
	unsigned int i;
	bool f = (inp[0] & 0x80) != 0;

	for (i = 0; i < 15; i++)
		outp[i] = (inp[i] << 1) | ((inp[i + 1] >> 7) & 0x1);
	outp[15] = inp[15] << 1;
	if (f)
		outp[15] ^= 0x87;
}

/* end of file da_pec_support.c */

#ifdef AXI_MODIFICATIONS
/*----------------------------------------------------------------------------
 * da_pec_pe_get_multi
 *
 * Attempt to receive multi packets from the result descriptor ring.
 *
 *
 * out_token_dscr_p (output)
 *     Pointer to 2D array of the Output Token descriptor that will be updated.
 *
 * out_token_data_p (output)
 *     Pointer to 2D array of the Output Token buffer where the token will be
 * stored.
 *
 * rd_p (output)
 *     Pointer 2D array of to the result descriptor that will be updated.
 *
 * pkt_get_stat_p (input/output)
 *     Pointer to struct rdr_pkts_stat for keeping the status record of each pkt.
 *     pkt_get_stat_p->exp_pkt_get_cnt (input) :
 *     number of expected pkts to be returned by pec_packet_get()
 *     pkt_get_stat_p->actual_pkt_get_cnt (output :
 *     number of actaul pkts returned by pec_packet_get()
 *     pkt_get_stat_p->pkt_status[MAX_RD_COUNT] : describes status of each pkt.
 *
 *   Return: 0    : no packets received within timeout.
 *         -1   : if error in pec_packet_get().
 *         >0   : number of packets received
 *
 */
unsigned int da_pec_pe_get_multi(io_token_output_dscr_t *const out_token_dscr_p,
				 u32 out_token_data[MAX_RD_COUNT][12],
				 pec_result_descriptor_t *rd_p,
				 struct rdr_pkts_stat *pkt_get_stat_p)
{
	pec_status_t pecres;
	int io_token_rc;
	int i;
	unsigned int counter = 0;

	zeroinit(*out_token_dscr_p);
	zeroinit(*rd_p);
	pkt_get_stat_p->actual_pkt_get_cnt = 0;

	// Fill RD data structures
	for (i = 0; i < pkt_get_stat_p->exp_pkt_get_cnt; i++)
		rd_p[i].output_token_p = out_token_data[i];

	// Try to get the processed packet from the driver
	pecres = pec_packet_get(pkt_get_stat_p->interface_id, rd_p,
				pkt_get_stat_p->exp_pkt_get_cnt, &counter);
	if (pecres != PEC_STATUS_OK) {
		LOG_CRIT("pec_inline_packet_get error %d\n", pecres);
		pkt_get_stat_p->actual_pkt_get_cnt = 0;
		return -1; // IO error
	}
	if (counter) {
		int pkt_i;

		pkt_get_stat_p->actual_pkt_get_cnt = counter;
		for (pkt_i = 0; pkt_i < counter; pkt_i++) {
#ifdef CONFIG_EIP_IPSEC_OFFLOAD
			io_token_output_dscr_ext_t out_token_dscr_ext;

			zeroinit(out_token_dscr_ext);
			out_token_dscr_p[pkt_i].ext_p = &out_token_dscr_ext;
#endif
			io_token_rc = io_token_parse(out_token_data[pkt_i],
						     &out_token_dscr_p[pkt_i]);
			pkt_get_stat_p->pkt_status[pkt_i].token_parse_ret_code =
				io_token_rc;
#ifdef CONFIG_EIP_IPSEC_OFFLOAD
			pkt_get_stat_p->pkt_status[pkt_i].ctx_id =
				out_token_dscr_ext.nph_context;
#endif

			if (io_token_rc < 0) {
				LOG_CRIT("For Pkt_i [%u], io_token_parse error code %d\n",
					 pkt_i, io_token_rc);
			} else {
				pkt_get_stat_p->pkt_status[pkt_i]
					.token_desc_error_code =
					out_token_dscr_p[pkt_i].error_code;
				if (out_token_dscr_p[pkt_i].error_code != 0) {
					LOG_CRIT("For Pkt_i [%u], Result descriptor error 0x%x\n",
						 pkt_i,
						 out_token_dscr_p[i].error_code);
				}
			}
		}
		return counter;
	}
	return 0; // IO error (timeout, no result packet received)
}

/*----------------------------------------------------------------------------
 * da_pec_sync_hmac_precompute complete operation in single packet
 *
 * This operation will build a separate context to do the precompute
 * and provide the precomputes as packet output data.
 */
bool da_pec_sync_hmac_precompute(sa_builder_auth_t auth_algo, u8 *auth_key_p,
				 unsigned int auth_key_byte_count, u8 *inner_p,
				 u8 *outer_p, struct hmac_precompute_wq *wq)
{
	int rc;
	sa_builder_params_t params;
	sa_builder_params_basic_t protocol_params;
	unsigned int sa_words = 0;

	dma_buf_status_t dma_status;
	dma_buf_properties_t dma_properties = { 0, 0, 0, 0 };
	dma_buf_host_address_t sa_host_address;
	dma_buf_host_address_t token_host_address;
	dma_buf_host_address_t pkt_host_address;

	dma_buf_handle_t sa_handle = { 0 };
	dma_buf_handle_t token_handle = { 0 };
	dma_buf_handle_t pkt_handle = { 0 };

	unsigned int tcr_words = 0;
	void *tcr_data = 0;
	unsigned int token_words = 0;
	unsigned int token_header_word;
	unsigned int token_max_words = 0;

	token_builder_params_t token_params;
	pec_command_descriptor_t cmd;
	unsigned int count;

	io_token_input_dscr_t in_token_dscr;
	u32 input_token[IOTOKEN_IN_WORD_COUNT];
	void *in_token_dscr_ext_p = NULL;

#ifdef DA_PEC_IOTOKEN_EXT
	io_token_input_dscr_ext_t in_token_dscr_ext;

	zeroinit(in_token_dscr_ext);
	in_token_dscr_ext_p = &in_token_dscr_ext;
#endif

	zeroinit(in_token_dscr);

	rc = sa_builder_init_basic(&params, &protocol_params,
				   SAB_DIRECTION_OUTBOUND);
	if (rc != 0) {
		LOG_CRIT("sa_builder_init_basic failed\n");
		goto error_exit;
	}
	// Add crypto key and parameters.
	params.auth_algo = auth_algo;
	protocol_params.basic_flags |= SAB_BASIC_FLAG_HMAC_PRECOMPUTE;
	rc = sa_builder_get_sizes(&params, &sa_words, NULL, NULL);

	if (rc != 0) {
		LOG_CRIT("SA not created because of errors\n");
		goto error_exit;
	}

	dma_properties.f_cached = true;
	dma_properties.alignment = DA_PEC_DMA_ALIGNMENT_BYTE_COUNT;
	dma_properties.bank = DA_PEC_BANK_SA;
	dma_properties.size = MAX(4 * sa_words, DA_PEC_MIN_SA_BYTE_COUNT);

	dma_status =
		dma_buf_alloc(dma_properties, &sa_host_address, &sa_handle);
	if (dma_status != DMABUF_STATUS_OK) {
		rc = 1;
		LOG_CRIT("Allocation of SA failed\n");
		goto error_exit;
	}

	rc = sa_builder_build_sa(&params, (u32 *)sa_host_address.p, NULL, NULL);

	if (rc != 0) {
		LOG_CRIT("SA not created because of errors\n");
		goto error_exit;
	}

	rc = token_builder_get_context_size(&params, &tcr_words);

	if (rc != 0) {
		LOG_CRIT("token_builder_get_context_size returned errors\n");
		goto error_exit;
	}

	// The Token Context Record does not need to be allocated
	// in a DMA-safe buffer.
	tcr_data = da_pec_malloc(4 * tcr_words);
	if (!tcr_data) {
		rc = 1;
		LOG_CRIT("Allocation of TCR failed\n");
		goto error_exit;
	}

	rc = token_builder_build_context(&params, tcr_data);

	if (rc != 0) {
		LOG_CRIT("token_builder_build_context failed\n");
		goto error_exit;
	}

	rc = token_builder_get_size(tcr_data, &token_max_words);
	if (rc != 0) {
		LOG_CRIT("token_builder_get_size failed\n");
		goto error_exit;
	}

	// Allocate one buffer for the token and two packet buffers.

	dma_properties.f_cached = true;
	dma_properties.alignment = DA_PEC_DMA_ALIGNMENT_BYTE_COUNT;
	dma_properties.bank = DA_PEC_BANK_TOKEN;
	dma_properties.size = 4 * token_max_words;

	dma_status = dma_buf_alloc(dma_properties, &token_host_address,
				   &token_handle);
	if (dma_status != DMABUF_STATUS_OK) {
		rc = 1;
		LOG_CRIT("Allocation of token buffer failed.\n");
		goto error_exit;
	}

	dma_properties.f_cached = true;
	dma_properties.alignment = DA_PEC_DMA_ALIGNMENT_BYTE_COUNT;
	dma_properties.bank = DA_PEC_BANK_PACKET;
	dma_properties.size = MAX(64, auth_key_byte_count);

	dma_status =
		dma_buf_alloc(dma_properties, &pkt_host_address, &pkt_handle);
	if (dma_status != DMABUF_STATUS_OK) {
		rc = 1;
		LOG_CRIT("Allocation of source packet buffer failed.\n");
		goto error_exit;
	}

	// Register the SA
	rc = pec_sa_register(REJECTED_FLOW_RULE_RING_ID, sa_handle,
			     dma_buf_null_handle, dma_buf_null_handle);
	if (rc != PEC_STATUS_OK) {
		LOG_CRIT("pec_sa_register failed\n");
		goto error_exit;
	}

	// Copy input packet into source packet buffer.
	memcpy(pkt_host_address.p, auth_key_p, auth_key_byte_count);

	// Set Token Parameters if specified in test vector.
	zeroinit(token_params);

	// Prepare a token to process the packet.
	rc = token_builder_build_token(tcr_data, (u8 *)pkt_host_address.p,
				       auth_key_byte_count, &token_params,
				       (u32 *)token_host_address.p,
				       &token_words, &token_header_word);
	if (rc != TKB_STATUS_OK) {
		LOG_CRIT("Token builder failed, rc: %d\n", rc);
		goto error_exit_unregister;
	}
	cmd.user_p = 0;
	cmd.token_handle = token_handle;
	cmd.token_word_count = token_words;
	cmd.src_pkt_handle = pkt_handle;
	cmd.src_pkt_byte_count = auth_key_byte_count;
	cmd.dst_pkt_handle = pkt_handle;
	cmd.bypass_word_count = 0;
	cmd.sa_handle1 = sa_handle;
	cmd.sa_handle2 = dma_buf_null_handle;
	cmd.sa_word_count = 128;

#ifdef DA_PEC_IOTOKEN_EXT_SRV_ICE
	in_token_dscr_ext.hw_services = IOTOKEN_CMD_PKT_LAC;
#endif
	in_token_dscr.tkn_hdr_word_init = token_header_word;

	in_token_dscr.app_id = HMAC_APP_ID;

	if (!da_pec_iotoken_create(&in_token_dscr, in_token_dscr_ext_p,
				   input_token, &cmd)) {
		rc = 1;
		goto error_exit_unregister;
	}

	/* TODO: Make copy of result also part of critcial section. so that only
	 * new input will be given only after complete process of the result
	 * Relese the lock after wait condition gets successful
	 */
	wq->inner_key = inner_p;
	wq->outer_key = outer_p;
	rc = pec_packet_put(REJECTED_FLOW_RULE_RING_ID, &cmd, 1, &count);
	if (rc != PEC_STATUS_OK && count != 1) {
		rc = 1;
		LOG_CRIT("%s(%d) error\n", __func__, __LINE__);
		goto error_exit_unregister;
	}
	wq->condition = 0;
	wait_event(wq->queue, wq->condition != 0);

error_exit_unregister:
	pec_sa_un_register(REJECTED_FLOW_RULE_RING_ID, sa_handle,
			   dma_buf_null_handle, dma_buf_null_handle);

error_exit:
	dma_buf_release(sa_handle);
	dma_buf_release(token_handle);
	dma_buf_release(pkt_handle);

	if (tcr_data)
		da_pec_free(tcr_data);

	return rc == 0;
}
#endif
