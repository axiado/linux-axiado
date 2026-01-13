/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (C) 2022-2026 Axiado Corporation. */

#ifdef CONFIG_EIP_IPSEC_OFFLOAD
#ifndef _EIP_IPSEC_
#define _EIP_IPSEC_
#include <net/xfrm.h>

#define AES_SHA1_PAD_LENGTH_OFFSET 14
#define ESP_IPHDR_SKIP_BYTES 24
#define ETH_IPHDR_COPY_BYTES 18

#define MAX_TX_IPSEC_SA 30
#define MAX_RX_IPSEC_SA 30

struct ipsec_handle {
	dma_buf_host_address_t host_address;
	dma_buf_handle_t handle;
	struct xfrm_state *xs;
	bool used;
};

struct eip_ipsec {
	/* TODO: add locking mechanism for this
	 * This will be at a used by ipsec call backs
	 * and also in tx packet path
	 * Both are process contexts
	 */
	struct ipsec_handle *tx_ipsec_handle;
	u16 tx_ipsec_sa_cnt;
	struct ipsec_handle *rx_ipsec_handle;
	u16 rx_ipsec_sa_cnt;
};

/* Supported encryption algorithms */
static const char aes_cbc_crypto_name[] = "cbc(aes)";

/* Support Authentication algorithms */
static const char md5_96_hmac_auth_name[] = "md5";
static const char md5_128_hmac_auth_name[] = "hmac(md5)";
static const char sha1_96_hmac_auth_name[] = "sha1";
static const char sha1_160_hmac_auth_name[] = "hmac(sha1)";
static const char sha2_256_96_hmac_auth_name[] = "sha256";
static const char sha2_256_128_hmac_auth_name[] = "hmac(sha256)";
static const char sha2_384_hmac_auth_name[] = "hmac(sha384)";
static const char sha2_512_hmac_auth_name[] = "hmac(sha512)";

#endif /* _EIP_IPSEC_ */
#endif /* CONFIG_EIP_IPSEC_OFFLOAD */
