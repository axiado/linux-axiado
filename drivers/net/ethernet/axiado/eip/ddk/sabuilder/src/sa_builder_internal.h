/* sa_builder_internal.h
 *
 * internal API of the EIP-96 SA Builder.
 * - layout of the control words.
 * - data structure that represents the SA builder state.
 * - Headers for shared functions and protocol-specific functions.
 */

/*****************************************************************************
* Copyright (c) 2011-2025 by Rambus, Inc. and/or its subsidiaries.
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


#ifndef SA_BUILDER_INTERNAL_H_
#define SA_BUILDER_INTERNAL_H_
#include "c_sa_builder.h"
#include "sa_builder.h"


/* type of packet field in Control Word 0 */

#define SAB_CW0_TOP_NULL_OUT        0x00000000
#define SAB_CW0_TOP_NULL_IN         0x00000001
#define SAB_CW0_TOP_HASH_OUT        0x00000002
#define SAB_CW0_TOP_HASH_IN         0x00000003
#define SAB_CW0_TOP_ENCRYPT         0x00000004
#define SAB_CW0_TOP_DECRYPT         0x00000005
#define SAB_CW0_TOP_ENCRYPT_HASH    0x00000006
#define SAB_CW0_TOP_DECRYPT_HASH    0x00000007
#define SAB_CW0_TOP_HASH_ENCRYPT    0x0000000e
#define SAB_CW0_TOP_HASH_DECRYPT    0x0000000f

/* flags to indicate SA properties to other software components.
   They will be stored inside the per-packet options field in the SA and
   as such they will never be user by hardware. */
#define SAB_CW0_SW_IS_LARGE         0x00000010

/* Cryptographic algorith, field (and key bit) */

#define SAB_CW0_CRYPTO_NULL         0x00000000
#define SAB_CW0_CRYPTO_DES          0x00010000
#define SAB_CW0_CRYPTO_ARC4         0x00030000
#define SAB_CW0_CRYPTO_3DES         0x00050000
#define SAB_CW0_CRYPTO_KASUMI       0x00070000
#define SAB_CW0_CRYPTO_AES_128      0x000b0000
#define SAB_CW0_CRYPTO_AES_192      0x000d0000
#define SAB_CW0_CRYPTO_AES_256      0x000f0000
#define SAB_CW0_CRYPTO_SNOW         0x00130000
#define SAB_CW0_CRYPTO_ZUC          0x00190000
#define SAB_CW0_CRYPTO_CHACHA20     0x00110000
#define SAB_CW0_CRYPTO_SM4          0x001b0000
#define SAB_CW0_CRYPTO_BC0          0x00190000

/* Hash and authentication algorithm field */

#define SAB_CW0_AUTH_NULL           0x00000000
#define SAB_CW0_AUTH_HASH_MD5       0x00000000
#define SAB_CW0_AUTH_HASH_SHA1      0x01000000
#define SAB_CW0_AUTH_HASH_SHA2_256  0x01800000
#define SAB_CW0_AUTH_HASH_SHA2_224  0x02000000
#define SAB_CW0_AUTH_HASH_SHA2_512  0x02800000
#define SAB_CW0_AUTH_HASH_SHA2_384  0x03000000
#define SAB_CW0_AUTH_HASH_SHA3_256  0x05800000
#define SAB_CW0_AUTH_HASH_SHA3_224  0x06000000
#define SAB_CW0_AUTH_HASH_SHA3_512  0x06800000
#define SAB_CW0_AUTH_HASH_SHA3_384  0x07000000
#define SAB_CW0_AUTH_HASH_SM3       0x03800000
#define SAB_CW0_AUTH_KEYED_HASH_SHA3_256  0x05a00000
#define SAB_CW0_AUTH_KEYED_HASH_SHA3_224  0x06200000
#define SAB_CW0_AUTH_KEYED_HASH_SHA3_512  0x06a00000
#define SAB_CW0_AUTH_KEYED_HASH_SHA3_384  0x07200000
#define SAB_CW0_AUTH_HASH_SAVE_SHA3_256  0x05c00000
#define SAB_CW0_AUTH_HASH_SAVE_SHA3_224  0x06400000
#define SAB_CW0_AUTH_HASH_SAVE_SHA3_512  0x06c00000
#define SAB_CW0_AUTH_HASH_SAVE_SHA3_384  0x07400000
#define SAB_CW0_AUTH_SSLMAC_SHA1    0x00e00000
#define SAB_CW0_AUTH_HMAC_MD5       0x00600000
#define SAB_CW0_AUTH_HMAC_SHA1      0x01600000
#define SAB_CW0_AUTH_HMAC_SHA2_256  0x01e00000
#define SAB_CW0_AUTH_HMAC_SHA2_224  0x02600000
#define SAB_CW0_AUTH_HMAC_SHA2_512  0x02e00000
#define SAB_CW0_AUTH_HMAC_SHA2_384  0x03600000
#define SAB_CW0_AUTH_HMAC_SHA3_256  0x05e00000
#define SAB_CW0_AUTH_HMAC_SHA3_224  0x06600000
#define SAB_CW0_AUTH_HMAC_SHA3_512  0x06e00000
#define SAB_CW0_AUTH_HMAC_SHA3_384  0x07600000
#define SAB_CW0_AUTH_HMAC_SM3       0x03e00000
#define SAB_CW0_AUTH_CMAC_128       0x00c00000
#define SAB_CW0_AUTH_CMAC_192       0x01400000
#define SAB_CW0_AUTH_CMAC_256       0x01c00000
#define SAB_CW0_AUTH_GHASH          0x02400000
#define SAB_CW0_AUTH_KASUMI_F9      0x03c00000
#define SAB_CW0_AUTH_SNOW_UIA2      0x03400000
#define SAB_CW0_AUTH_ZUC_EIA3       0x02C00000
#define SAB_CW0_AUTH_POLY1305       0x07800000
#define SAB_CW0_AUTH_KEYED_HASH_POLY1305 0x07c00000

/* Add this when hash value must be loaded from context */
#define SAB_CW0_HASH_LOAD_DIGEST    0x00200000

/* spi present */
#define SAB_CW0_SPI                 0x08000000

/* Sequence number size */
#define SAB_CW0_SEQNUM_32           0x10000000
#define SAB_CW0_SEQNUM_48           0x20000000
#define SAB_CW0_SEQNUM_64           0x30000000
/* Sequence number size encodings for 'fixed offset' */
#define SAB_CW0_SEQNUM_32_FIX       0x00008000
#define SAB_CW0_SEQNUM_64_FIX       0x10008000
#define SAB_CW0_SEQNUM_48_FIX       0x20008000


/* mask size */
#define SAB_CW0_MASK_64             0x40000000
#define SAB_CW0_MASK_32             0x80000000
#define SAB_CW0_MASK_128            0xc0000000
/* mask size encodings for 'fixed offset' */
#define SAB_CW0_MASK_64_FIX         0x00008000
#define SAB_CW0_MASK_128_FIX        0x40008000
#define SAB_CW0_MASK_256_FIX        0xc0008000
#define SAB_CW0_MASK_384_FIX        0x80008000
#define SAB_CW0_MASK_1024_FIX       0x40008000
#define SAB_CW0_SEQNUM_APPEND       0x00004000

/* Crypto feedback mode */
#define SAB_CW1_CRYPTO_MODE_ECB      0x00000000
#define SAB_CW1_CRYPTO_MODE_CBC      0x00000001
#define SAB_CW1_CRYPTO_MODE_F8_UEA   0x00000001
#define SAB_CW1_CRYPTO_MODE_CTR      0x00000002
#define SAB_CW1_CRYPTO_MODE_ICM      0x00000003
#define SAB_CW1_CRYPTO_MODE_OFB      0x00000004
#define SAB_CW1_CRYPTO_MODE_CFB      0x00000005
#define SAB_CW1_CRYPTO_MODE_CTR_LOAD 0x00000006
#define SAB_CW1_CRYPTO_MODE_XTS      0x00000007
#define SAB_CW1_CRYPTO_MODE_CHACHA256 0x00000000
#define SAB_CW1_CRYPTO_MODE_CHACHA128 0x00000001
#define SAB_CW1_CRYPTO_MODE_CHACHA_CTR32 0x00000002
#define SAB_CW1_CRYPTO_MODE_CHACHA_CTR64 0x00000000
#define SAB_CW1_CRYPTO_MODE_CHACHA_POLY_OTK 0x00000004
#define SAB_CW1_CRYPTO_AEAD              0x00000008
#define SAB_CW1_CRYPTO_NONCE_XOR         0x00000010

/* iv words load */
#define SAB_CW1_IV0                  0x00000020
#define SAB_CW1_IV1                  0x00000040
#define SAB_CW1_IV2                  0x00000080
#define SAB_CW1_IV3                  0x00000100

#define SAB_CW1_DIGEST_CNT           0x00000200

/* iv mode */
#define SAB_CW1_IV_FULL              0x00000000
#define SAB_CW1_IV_CTR               0x00000400
#define SAB_CW1_IV_ORIG_SEQ          0x00000800
#define SAB_CW1_IV_INCR_SEQ          0x00000c00
#define SAB_CW1_IV_MODE_MASK         0x00000c00

#define SAB_CW1_CRYPTO_STORE        0x00001000

#define SAB_CW1_PREPKT_OP           0x00002000

/* Pad type */
#define SAB_CW1_PAD_ZERO            0x00000000
#define SAB_CW1_PAD_PKCS7           0x00004000
#define SAB_CW1_PAD_CONST           0x00008000
#define SAB_CW1_PAD_RTP             0x0000c000
#define SAB_CW1_PAD_IPSEC           0x00010000
#define SAB_CW1_PAD_TLS             0x00014000
#define SAB_CW1_PAD_SSL             0x00018000
#define SAB_CW1_PAD_IPSEC_NOCHECK   0x0001c000


#define SAB_CW1_ENCRYPT_HASHRES     0x00020000
#define SAB_CW1_MACSEC_SEQCHECK     0x00040000
#define SAB_CW1_WIRELESS_DIR        0x00040000
#define SAB_CW1_HASH_STORE          0x00080000

#define SAB_CW1_EXT_CIPHER_SET      0x00100000
#define SAB_CW1_ARC4_IJ_PTR         0x00100000
#define SAB_CW1_ARC4_STATE_SEL      0x00200000
#define SAB_CW1_CCM_IV_SHIFT        0x00200000
#define SAB_CW1_XTS_STATEFUL        0x00200000
#define SAB_CW1_SEQNUM_STORE        0x00400000
#define SAB_CW1_NO_MASK_UPDATE      0x00800000
#define SAB_CW1_EARLY_SEQNUM_UPDATE 0x40000000
#define SAB_CW1_CNTX_FETCH_MODE     0x80000000

#define SAB_SEQNUM_LO_FIX_OFFSET    32
#define SAB_SEQNUM_HI_FIX_OFFSET    48


/* Flag byte for CCM. first byte in counter mode iv, equal to L-1.
 */
#define SAB_CCM_FLAG_L4             0x3
#define SAB_CCM_FLAG_L3             0x2

/* This structure represents the internal state of the SA builder, shared
   between all sub-functions, e.g. for encryption, hash and protocol-specific
   extensions.
 */
typedef struct
{
    unsigned int current_offset; /* Current word offset within the SA */
    u32 cw0; /* Control word 0 */
    u32 cw1; /* Control word 1 */
    unsigned int cipher_key_words; /* size of the cipher key in words */
    unsigned int iv_words; /* size of the iv in words */
    bool arc4_state; /* Is arc4 state used? */
    bool f_large; /* Is this a large transform record? */
    bool f_large_mask; /* Do we use a large sequence number mask? */
} sa_builder_state_t;


/*----------------------------------------------------------------------------
 * sa_builder_lib_copy_key_mat
 *
 * Copy a key into the SA
 *
 * destination_p (input)
 *   Destination (word-aligned) of the SA record.
 * offset (input)
 *   Word offset of the key in the SA record where it must be stored.
 * source_p (input)
 *   source (byte aligned) of the data.
 * key_byte_count (input)
 *   size of the key in bytes.
 *
 * destination_p is allowed to be a null pointer, in which case no key
 * will be written.
 */
void
sa_builder_lib_copy_key_mat(u32 * const destination_p,
                        const unsigned int offset,
                        const u8 * const source_p,
                        const unsigned int key_byte_count);


/*----------------------------------------------------------------------------
 * sa_builder_lib_copy_key_mat_swap
 *
 * Copy a key into the SA with the words byte-swapped.
 *
 * destination_p (input)
 *   Destination (word-aligned) to store the data.
 * offset (input)
 *   Word offset of the key in the SA record where it must be stored.
 * source_p (input)
 *   source (byte aligned) of the data.
 * key_byte_count (input)
 *   size of the key in bytes.
 *
 * destination_p is allowed to be a null pointer, in which case no key
 * will be written.
 */
void
sa_builder_lib_copy_key_mat_swap(u32 * const destination_p,
                            const unsigned int offset,
                            const u8 * const source_p,
                            const unsigned int key_byte_count);


/*----------------------------------------------------------------------------
 * sa_builder_lib_zero_fill
 *
 * Fill an area in the SA with zero bytes.
 *
 * destination_p (input)
 *   Destination (word-aligned) of the SA record.
 *
 * offset (input)
 *   Word offset of the area in the SA that must be zero-filled.
 *
 * byte_count (input)
 *   number of bytes to write.
 *
 * destination_p is allowed to be a null pointer, in which case no zeroes
 * will be written.
 */
void
sa_builder_lib_zero_fill(
        u32 * const destination_p,
        const unsigned int offset,
        const unsigned int byte_count);

#ifdef SAB_ENABLE_PROTO_BASIC
/*----------------------------------------------------------------------------
 * sa_builder_set_ssltls_params
 *
 * Fill in Basic Crypto and hash specific extensions into the SA.
 *
 * sa_params_p (input)
 *   The SA parameters structure from which the SA is derived.
 * sa_state_p (input, output)
 *   Variables containing information about the SA being generated/
 * sa_buffer_p (input, output).
 *   The buffer in which the SA is built. If NULL, no SA will be built, but
 *   state variables in sa_state_p will still be updated.
 *
 * Return:
 * SAB_STATUS_OK on success
 * SAB_INVALID_PARAMETER when sa_params_p is invalid, or if any of
 *    the buffer arguments  is a null pointer while the corresponding buffer
 *    would be required for the operation.
 * SAB_UNSUPPORTED_FEATURE when sa_params_p describes an operations that
 *    is not supported on the hardware for which this SA builder
 *    is configured.
 */
sa_builder_status_t
sa_builder_set_basic_params(sa_builder_params_t *const sa_params_p,
                          sa_builder_state_t * const sa_state_p,
                          u32 * const sa_buffer_p);

#endif


#ifdef SAB_ENABLE_PROTO_IPSEC
/*----------------------------------------------------------------------------
 * sa_builder_set_ipsec_params
 *
 * Fill in IPsec-specific extensions into the SA.
 *
 * sa_params_p (input)
 *   The SA parameters structure from which the SA is derived.
 * sa_state_p (input, output)
 *   Variables containing information about the SA being generated/
 * sa_buffer_p (input, output).
 *   The buffer in which the SA is built. If NULL, no SA will be built, but
 *   state variables in sa_state_p will still be updated.
 *
 * Return:
 * SAB_STATUS_OK on success
 * SAB_INVALID_PARAMETER when sa_params_p is invalid, or if any of
 *    the buffer arguments  is a null pointer while the corresponding buffer
 *    would be required for the operation.
 * SAB_UNSUPPORTED_FEATURE when sa_params_p describes an operations that
 *    is not supported on the hardware for which this SA builder
 *    is configured.
 */
sa_builder_status_t
sa_builder_set_ipsec_params(sa_builder_params_t *const sa_params_p,
                         sa_builder_state_t * const sa_state_p,
                         u32 * const sa_buffer_p);

#endif

#ifdef SAB_ENABLE_PROTO_SSLTLS
/*----------------------------------------------------------------------------
 * sa_builder_set_ssltls_params
 *
 * Fill in SSL/TLS/DTLS-specific extensions into the SA.
 *
 * sa_params_p (input)
 *   The SA parameters structure from which the SA is derived.
 * sa_state_p (input, output)
 *   Variables containing information about the SA being generated/
 * sa_buffer_p (input, output).
 *   The buffer in which the SA is built. If NULL, no SA will be built, but
 *   state variables in sa_state_p will still be updated.
 *
 * Return:
 * SAB_STATUS_OK on success
 * SAB_INVALID_PARAMETER when sa_params_p is invalid, or if any of
 *    the buffer arguments  is a null pointer while the corresponding buffer
 *    would be required for the operation.
 * SAB_UNSUPPORTED_FEATURE when sa_params_p describes an operations that
 *    is not supported on the hardware for which this SA builder
 *    is configured.
 */
sa_builder_status_t
sa_builder_set_ssltls_params(sa_builder_params_t *const sa_params_p,
                          sa_builder_state_t * const sa_state_p,
                          u32 * const sa_buffer_p);

#endif


#ifdef SAB_ENABLE_PROTO_SRTP
/*----------------------------------------------------------------------------
 * sa_builder_set_srtp_params
 *
 * Fill in SRTP-specific extensions into the SA.
 *
 * sa_params_p (input)
 *   The SA parameters structure from which the SA is derived.
 * sa_state_p (input, output)
 *   Variables containing information about the SA being generated/
 * sa_buffer_p (input, output).
 *   The buffer in which the SA is built. If NULL, no SA will be built, but
 *   state variables in sa_state_p will still be updated.
 *
 * Return:
 * SAB_STATUS_OK on success
 * SAB_INVALID_PARAMETER when sa_params_p is invalid, or if any of
 *    the buffer arguments  is a null pointer while the corresponding buffer
 *    would be required for the operation.
 * SAB_UNSUPPORTED_FEATURE when sa_params_p describes an operations that
 *    is not supported on the hardware for which this SA builder
 *    is configured.
 */
sa_builder_status_t
sa_builder_set_srtp_params(sa_builder_params_t *const sa_params_p,
                          sa_builder_state_t * const sa_state_p,
                          u32 * const sa_buffer_p);

#endif


#ifdef SAB_ENABLE_PROTO_MACSEC
/*----------------------------------------------------------------------------
 * sa_builder_set_macsec_params
 *
 * Fill in MACsec-specific extensions into the SA.
 *
 * sa_params_p (input)
 *   The SA parameters structure from which the SA is derived.
 * sa_state_p (input, output)
 *   Variables containing information about the SA being generated/
 * sa_buffer_p (input, output).
 *   The buffer in which the SA is built. If NULL, no SA will be built, but
 *   state variables in sa_state_p will still be updated.
 *
 * Return:
 * SAB_STATUS_OK on success
 * SAB_INVALID_PARAMETER when sa_params_p is invalid, or if any of
 *    the buffer arguments  is a null pointer while the corresponding buffer
 *    would be required for the operation.
 * SAB_UNSUPPORTED_FEATURE when sa_params_p describes an operations that
 *    is not supported on the hardware for which this SA builder
 *    is configured.
 */
sa_builder_status_t
sa_builder_set_macsec_params(sa_builder_params_t *const sa_params_p,
                          sa_builder_state_t * const sa_state_p,
                          u32 * const sa_buffer_p);

#endif



#endif /* SA_BUILDER_INTERNAL_H_ */


/* end of file sa_builder_internal.h */
