/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0-only) */
/*
 * Copyright (c) 2026 Axiado Corporation
 * Copyright (c) 2008-2025 by Rambus, Inc. and/or its subsidiaries.
 */

#ifndef DA_INTERNAL_H_
#define DA_INTERNAL_H_

/*----------------------------------------------------------------------------
 * This module implements (provides) the following interface(s):
 */

/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */
/* configuration switches */
#include "c_eip.h"

#include "api_pec.h"
#include "api_pec_sg.h"
#include "api_dmabuf.h"

#include "sa_builder.h"

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/delay.h>

#define da_pec_malloc(s) kmalloc(s, GFP_KERNEL)
#define da_pec_free(s) kfree(s)
#define da_pec_usleep(s) udelay(s)

/****************************************************************
 * Definitions and macros.
 * ***************************************************************
 */


// Standard IOToken API
#include "iotoken.h"

// Extended IOToken API
#include "iotoken_ext.h"

#if defined(IOTOKEN_USE_HW_SERVICE)
#define DA_PEC_IOTOKEN_EXT // Enable extended IOToken API implementation
#endif

#ifdef AXI_MODIFICATIONS

/* 1 for enabling Lookup mode; 0 for non-lookup mode
 * In case of lookup mode, DTL rule will be added in lookup table for each
 * valid marked pkt ariving from IP on default ring.
 * As of now LOOKUP works for ingress pkt only. All egress pkts on Use ring will be
 * sent to CDR.
 * In case of non-lookup mode, all egress pkts from all interface would be sent out.
 * NOTE: Lookup must be enabled when working with FWL.
 */
#define LOOKUP_ENABLED 1

#define NON_OFFLOAD_TR_CNTXT_REF 0xFFFFFFFF
#define HMAC_APP_ID 17

/* Add sample dtl rules for inbound traffic
 * These rules are added while bring up of eip0
 */
#define SAMPLE_DTL_RULE_ENABLED 0

/* number of lookup tables to be created in pcl_dtl_init */
/* This must be 1 as there is implementation limit in EIP-DDK */
#define LOOKUP_TABLE_CNT 1

#define INLINE_INTERFACE_ID 15

/* Default interface for non-matching pkt in lookup table. */
// Same as ADAPTER_CS_GLOBAL_REDIR_RING  integration/slad_adapter/src/c_adapter_global.h
#define DEFAULT_INTERFACE_ID 0
#ifndef ADAPTER_CS_GLOBAL_REDIR_RING
#define ADAPTER_CS_GLOBAL_REDIR_RING DEFAULT_INTERFACE_ID
#endif

/* Id 0-15 are used by eip interfaces,
 * so allocating the last number, 16 after eip interface
 */
#define EIPDTL_INTERFACE_ID 16

/* Maximum number of ring interface to be enabled
 * Ring-0 would always be Default interface and rest of the rings for user interface.
 * value must be greater than 0 and less or equal to number of enabled rings in EIP-DDK.
 * This should not be more than 4 because DDK has implementation limit of 4
 */

/* EIP-max ring - defined in shim_eip_common.h */
#ifndef RING_INTERFACE_CNT
#ifndef CONFIG_ARCH_AX3005
#define RING_INTERFACE_CNT 4
#else
#define RING_INTERFACE_CNT 12
#endif
#endif

#define INVALID_INTERFACE_ID 0xFF

/* This enables the feature for redirecting rejected-flow-pkt to EIP-Ring-3.
 * Here, Ring-3 is exclusively used for dumping such rejected-flow-pkt.
 * For such flow, EIP-lookup rule is added with destination ring as 3.
 * There won't be any ipstack interface associated with Ring-3 and all the pkts received
 * from EIP-Ring-3 will be dropped
 */
#define ENABLE_REJECTED_FLOW_TO_R3

#ifdef ENABLE_REJECTED_FLOW_TO_R3
/* Using the EIP-ring 3 for accepting rejected flow pkts. */
#ifndef CONFIG_ARCH_AX3005
#define REJECTED_FLOW_RULE_RING_ID 3
#else
#define REJECTED_FLOW_RULE_RING_ID 11
#endif
#endif /* ENABLE_REJECTED_FLOW_TO_R3 */

/* Scan DTL-lookup table periodically and delete the expired rules. */
#define DTL_EXPIRY_ENABLED 1

#define INTERFACE_ID DEFAULT_INTERFACE_ID

#ifdef CONFIG_EIP_IPSEC_OFFLOAD
#ifndef REJECTED_FLOW_RULE_RING_ID
#ifndef CONFIG_ARCH_AX3005
#define IPSEC_LOOKASIDE_INTERFACE 3
#else
#define IPSEC_LOOKASIDE_INTERFACE 11
#endif
#define INTERFACE_ID IPSEC_LOOKASIDE_INTERFACE
#else
#define IPSEC_LOOKASIDE_INTERFACE REJECTED_FLOW_RULE_RING_ID
#endif
#endif

#ifdef DRIVER_MAX_RX_DESCR
#define MAX_RD_COUNT (DRIVER_MAX_RX_DESCR - 1)
#else
#error "Please define PEC_MAX_LOGICDESCR"
#endif
#define EIP_RD_REFILL_THRESH (MAX_RD_COUNT / 4)

#ifdef CONFIG_EIP_IPSEC_OFFLOAD
#define DEFAULT_IPSEC 0

// Default DMA bank for non-DTL transforms
#ifndef DA_PEC_PCL_BANK_TRANSFORM
#define DA_PEC_PCL_BANK_TRANSFORM 0
#endif

// Default DMA bank for non-DTL transforms
#ifndef DA_PEC_PCL_BANK_TRANSFORM
#define DA_PEC_PCL_BANK_TRANSFORM 0
#endif

// Default DMA buffer allocation alignment (4 - 4 bytes)
#ifndef DA_PEC_PCL_DMA_ALIGNMENT_BYTE_COUNT
#define DA_PEC_PCL_DMA_ALIGNMENT_BYTE_COUNT 4
#endif

#endif /*CONFIG_EIP_IPSEC_OFFLOAD*/

/* 1 for enable; 0 for disable
 * If enabled,  - pkts received from RDR are dumped.
 *              - Sample UDP pkt generator is enabled.
 *              - Sample DTL entry is added in lookup mode.
 */
#ifdef LOG_SEVERITY_MAX
#if (LOG_SEVERITY_MAX > 2)
#define NETDEV_DEBUG 1
#else
#define NETDEV_DEBUG 0
#endif
#else
#define NETDEV_DEBUG 1
#endif

/**
 * @brief Return status for EIP-DEV driver functions.
 */
enum AX_EIP_STATUS {
	EIP_STATUS_SUCCESS = 0,
	EIP_STATUS_INVALID_PARAM = 1,
	EIP_STATUS_NULL_POINTER = 2,
	EIP_STATUS_MEM_ERROR = 3,
	EIP_STATUS_INTERNAL_ERROR = 4,
	EIP_STATUS_DDK_ERROR = 5
};

struct pkt_parse_stat {
	unsigned int pkt_index;
	int token_parse_ret_code;
	int token_desc_error_code;
	int ctx_id;
};

struct rdr_pkts_stat {
	int interface_id;
	unsigned int exp_pkt_get_cnt;
	unsigned int actual_pkt_get_cnt;
	struct pkt_parse_stat pkt_status[MAX_RD_COUNT];
};

struct hmac_precompute_wq {
	wait_queue_head_t queue;
	int condition;
	u8 *inner_key;
	u8 *outer_key;
};
#endif

/*----------------------------------------------------------------------------
 * da_pec_pe_get_one
 *
 * Attempt to receive a single packet from the result descriptor ring.
 * Try this a configurable number of times. When a packet is received,
 * check its status and perform header post-processing.
 *
 * out_token_dscr_p (output)
 *     Pointer to the Output Token descriptor that will be updated.
 *
 * out_token_data_p (output)
 *     Pointer to the Output Token buffer where the token will be stored.
 *
 * rd_p (output)
 *     Pointer to the result descriptor that will be updated.
 *
 * Return
 *     0  : no packet received (timeout), IO error
 *            or packet processing error.
 *     >0 : number of packets received.
 */
unsigned int da_pec_pe_get_one(io_token_output_dscr_t *const out_token_dscr_p,
			       u32 *out_token_data_p,
			       pec_result_descriptor_t *rd_p);

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
 *     pkt_get_stat_p->exp_pkt_get_cnt (input) : number of expected pkts to be
 * returned by pec_packet_get(); pkt_get_stat_p->actual_pkt_get_cnt (output : nuber
 * of actaul pkts returned by pec_packet_get();
 *     pkt_get_stat_p->pkt_status[MAX_RD_COUNT] : describes status of each pkt.
 *
 *   Return: 0    : no packets received within timeout.
 *         -1   : if error in pec_packet_get().
 *         >0   : number of packets received
 *
 */

unsigned int
da_pec_pe_get_multi(io_token_output_dscr_t *const out_token_dscr_p,
		    u32 out_token_data_p[MAX_RD_COUNT][IOTOKEN_OUT_WORD_COUNT],
		    pec_result_descriptor_t *rd_p,
		    struct rdr_pkts_stat *pkt_get_stat_p);

/*----------------------------------------------------------------------------
 * da_pec_finalize_dst_pkt
 */
bool da_pec_finalize_dst_pkt(dma_buf_handle_t dma_buf_dst_pkt_handle);

/*----------------------------------------------------------------------------
 * da_pec_inline_null_crypto - Loopback(Getting pkt and putting the same pkt.)
 *
 */
bool da_pec_inline_null_crypto(const bool f_verbose,
			       const unsigned int min_pkt_size,
			       const unsigned int max_pkt_size,
			       const unsigned int size_step);

#endif

/*----------------------------------------------------------------------------
 * da_pec_iotoken_create
 */
bool da_pec_iotoken_create(io_token_input_dscr_t *const dscr_p,
			   void *const ext_p, u32 *data_p,
			   pec_command_descriptor_t *const pec_cmd_dscr);

/*----------------------------------------------------------------------------
 * da_pec_invalidate_rec
 *
 * Invalidate specified transform record from the cache.
 *
 * rec_p (input)
 *     DMABuf handle of record .
 *
 * Return
 *     true  : the invalidate succeeded
 *     false : the invalidate failed
 */
bool da_pec_invalidate_rec(const dma_buf_handle_t rec_p);

/*----------------------------------------------------------------------------
 * da_pec_hmac_precompute
 *
 * Precompute the inner and outer digest parameters required by the SA Builder
 * for the HMAC operation.
 *
 * auth_algo (Input)
 *     One of the HMAC algorithms supported by the SA Builder.
 *
 * auth_key_p (input)
 *     HMAC key.
 *
 * auth_key_byte_count (input)
 *     size of the HMAC key in bytes.
 *
 * inner_p (output)
 *     Computed inner digest.
 *
 * outer_p (output)
 *     Computed outer digest
 *
 * Return
 *     true  : Success
 *     false : failure
 */
bool da_pec_hmac_precompute(sa_builder_auth_t auth_algo, u8 *auth_key_p,
			    unsigned int auth_key_byte_count, u8 *inner_p,
			    u8 *outer_p);

/*----------------------------------------------------------------------------
 * da_pec_sync_hmac_precompute
 *
 * Precompute the inner and outer digest parameters required by the SA Builder
 * for the HMAC operation. This will wait until result received on the rx thread
 *
 * auth_algo (Input)
 *     One of the HMAC algorithms supported by the SA Builder.
 *
 * auth_key_p (input)
 *     HMAC key.
 *
 * auth_key_byte_count (input)
 *     size of the HMAC key in bytes.
 *
 * inner_p (output)
 *     Computed inner digest.
 *
 * outer_p (output)
 *     Computed outer digest
 *
 * wq(wait queue)
 *     Wait queue to synchronize between the rx thread and hmac precompute
 * Return
 *     true  : Success
 *     false : failure
 */

bool da_pec_sync_hmac_precompute(sa_builder_auth_t auth_algo, u8 *auth_key_p,
				 unsigned int auth_key_byte_count, u8 *inner_p,
				 u8 *outer_p, struct hmac_precompute_wq *wq);

/*----------------------------------------------------------------------------
 * da_pec_hmac_hw_precompute
 *
 * Precompute the inner and outer digest parameters required by the SA Builder
 * for the HMAC operation. Update them directly into the context record.
 * which was previously built without the inner and outer digest parameters,
 * using hardware assisted operation
 *
 * auth_key_p (input)
 *     HMAC key.
 *
 * auth_key_byte_count (input)
 *     size of the HMAC key in bytes.
 *
 * sa_handle (input, output)
 *     SA (context) in which to update the HMAC key.
 *
 * token_context (input, output)
 *     Token context record
 */
bool da_pec_hmac_hw_precompute(u8 *auth_key_p, unsigned int auth_key_byte_count,
			       dma_buf_handle_t sa_handle, void *token_context);

/*----------------------------------------------------------------------------
 * da_pec_basic_hash
 * Compute a basic hash of a message.
 *
 * hash_algo (input)
 *     Hash algorithm
 *
 * input_p (input)
 *     Message data to be hashed.
 *
 * input_byte_count (input)
 *     number of bytes in the message to be hashed.
 *
 * output_p (output)
 *     Resulting hash of the message.
 *
 * output_byte_count (input)
 *     Expedcted number of bytes of the hash.
 *
 * Finalize (input)
 *     If true, finalize the message (appending padding and message length)
 *     before computing the hash of the message.
 *     If false, do not finalize, just run the hash compression function on
 *     the data block supplied and return the contents of the intermediate
 *     digest. This is required for precomputing the HMAC inner and outer
 *     digest parameters for the SA builder.
 *
 * Return
 *     true  : Success
 *     false : failure
 */
bool da_pec_basic_hash(sa_builder_auth_t hash_algo, u8 *input_p,
		       unsigned int input_byte_count, u8 *output_p,
		       unsigned int output_byte_count, bool f_finalize);

/*----------------------------------------------------------------------------
 * da_pec_aes_block_encrypt
 *
 * Encrypt a single AES block.
 */
bool da_pec_aes_block_encrypt(u8 *key_p, unsigned int key_byte_count,
			      u8 *in_data_p, u8 *out_data_p);

/*----------------------------------------------------------------------------
 * da_pec_aes_gcm_hkey
 *
 * Compute the H_KEY for AES-GCM
 */
bool da_pec_aes_gcm_hkey(u8 *key_p, unsigned int key_byte_count,
			 u8 *out_data_p);

/*----------------------------------------------------------------------------
 * da_pec_shift_cmac_subkey
 *
 * Derive K2 and K1 subkeys from encrypted all-zero block.
 */
void da_pec_shift_cmac_subkey(u8 *inp, u8 *outp);

/*----------------------------------------------------------------------------
 * da_pec_basic_crypt_hash
 *
 */
bool da_pec_basic_crypt_hash(const bool f_verbose,
			     const unsigned int min_pkt_size,
			     const unsigned int max_pkt_size,
			     const unsigned int size_step);

/*----------------------------------------------------------------------------
 * da_pec_gcm
 *
 */
bool da_pec_basic_aes_gcm(const bool f_verbose, const unsigned int min_pkt_size,
			  const unsigned int max_pkt_size,
			  const unsigned int size_step);

/*----------------------------------------------------------------------------
 * da_pec_tls
 *
 */
bool da_pec_tls(const bool f_verbose, const unsigned int min_pkt_size,
		const unsigned int max_pkt_size, const unsigned int size_step);

/*----------------------------------------------------------------------------
 * da_pec_ipsec_esp
 *
 */
bool da_pec_ipsec_esp(const bool f_verbose, const unsigned int min_pkt_size,
		      const unsigned int max_pkt_size,
		      const unsigned int size_step);

/*----------------------------------------------------------------------------
 * da_pec_basic_hmac
 *
 */
bool da_pec_basic_hmac(const bool f_verbose);

/*----------------------------------------------------------------------------
 * da_pec_basic_gcm2
 *
 */
bool da_pec_basic_aes_gcm2(const bool f_verbose);

/*----------------------------------------------------------------------------
 * da_pec_basic_sha3
 *
 */
bool da_pec_basic_sha3(const bool f_verbose);

/*----------------------------------------------------------------------------
 * da_pec_basic_shake
 *
 */
bool da_pec_basic_shake(const bool f_verbose, const bool f_c_shake);

/*----------------------------------------------------------------------------
 * da_pec_basic_wireless
 *
 */
bool da_pec_basic_wireless(const bool f_verbose);

/*----------------------------------------------------------------------------
 * da_pec_basic_aes_wireless
 *
 */
bool da_pec_basic_aes_wireless(const bool f_verbose);

/*----------------------------------------------------------------------------
 * da_pec_basic_multihash
 *
 */
bool da_pec_basic_multihash(const bool f_verbose);

/*----------------------------------------------------------------------------
 * da_pec_basic_multihash_unaligned
 *
 */
bool da_pec_basic_multihash_unaligned(const bool f_verbose);

/*----------------------------------------------------------------------------
 * da_pec_basic_hmac_cont
 *
 */
bool da_pec_basic_hmac_cont(const bool f_verbose);

/*----------------------------------------------------------------------------
 * da_pec_basic_gcm_cont
 *
 */
bool da_pec_basic_gcm_cont(const bool f_verbose);

#endif /* DA_INTERNAL_H_ */

/* end of file da_internal.h */
