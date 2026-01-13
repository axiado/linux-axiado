/* token_builder_internal.h
 *
 * internal APIs for the Token Builder implementation.
 * This includes thee actual definition of the Token Context Record.
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


#ifndef TOKEN_BUILDER_INTERNAL_H_
#define TOKEN_BUILDER_INTERNAL_H_


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */
#include "token_builder.h"
#include "basic_defs.h"

/*----------------------------------------------------------------------------
 * Definitions and macros
 */

/* Various fields in the token header word.
 */
#define TKB_HEADER_RC_NO_REUSE       0x00000000
#define TKB_HEADER_RC_REUSE          0x00100000
#define TKB_HEADER_RC_AUTO_REUSE     0x00200000

#define TKB_HEADER_IP                0x00020000

#ifdef TKB_NO_INPUT_PACKET_POINTER
#define TKB_HEADER_IP_OPTION TKB_HEADER_IP
#else
#define TKB_HEADER_IP_OPTION 0
#endif

#ifdef TKB_AUTODETECT_CONTEXT_REUSE
#define TKB_HEADER_RC_OPTION TKB_HEADER_RC_AUTO_REUSE
#else
#define TKB_HEADER_RC_OPTION TKB_HEADER_RC_NO_REUSE
#endif

#define TKB_HEADER_DEFAULT           (TKB_HEADER_RC_OPTION | TKB_HEADER_IP_OPTION)

#define TKB_HEADER_C                 0x02000000


#define TKB_HEADER_UPD_HDR           0x00400000
#define TKB_HEADER_APP_RES           0x00800000

#define TKB_HEADER_PAD_VERIFY        0x01000000
#define TKB_HEADER_IV_DEFAULT        0x00000000
#define TKB_HEADER_IV_PRNG           0x04000000
#define TKB_HEADER_IV_TOKEN_2WORDS   0x18000000
#define TKB_HEADER_IV_TOKEN_4WORDS   0x1c000000

#define TKB_HEADER_U                 0x20000000

/* CCM flag byte, includes Adata and L=4, M has to be filled in */
#define TKB_CCM_FLAG_ADATA_L4        0x43
/* CCM flag byte, includes Adata and L=3, M has to be filled in */
#define TKB_CCM_FLAG_ADATA_L3        0x42

#define TKB_ESP_FLAG_CLEAR_DF      BIT_0
#define TKB_ESP_FLAG_SET_DF       BIT_1
#define TKB_ESP_FLAG_REPLACE_DSCP  BIT_2
#define TKB_ESP_FLAG_CLEAR_ECN     BIT_3
#define TKB_ESP_FLAG_NAT           BIT_6
#define TKB_DTLS_FLAG_PLAINTEXT_HDR BIT_4
#define TKB_DTLS_FLAG_CAPWAP       BIT_5

/* The protocol values have to agree with those used in token_builder_core.c
 */
typedef enum
{
    TKB_PROTO_ESP_OUT = 0,
    TKB_PROTO_ESP_IN = 1,
    TKB_PROTO_ESP_CCM_OUT = 2,
    TKB_PROTO_ESP_CCM_IN = 3,
    TKB_PROTO_ESP_GCM_OUT = 4,
    TKB_PROTO_ESP_GCM_IN = 5,
    TKB_PROTO_ESP_GMAC_OUT = 6,
    TKB_PROTO_ESP_GMAC_IN = 7,
    TKB_PROTO_SSLTLS_OUT = 8,
    TKB_PROTO_SSLTLS_IN = 9,
    TKB_PROTO_BASIC_CRYPTO = 10,
    TKB_PROTO_BASIC_HASH = 11,
    TKB_PROTO_SRTP_OUT = 12,
    TKB_PROTO_SRTP_IN = 13,
    TKB_PROTO_BASIC_CRYPTHASH = 14,
    TKB_PROTO_BASIC_CCM_OUT = 15,
    TKB_PROTO_BASIC_CCM_IN = 16,
    TKB_PROTO_BASIC_GCM_OUT = 17,
    TKB_PROTO_BASIC_GCM_IN = 18,
    TKB_PROTO_BASIC_GMAC_OUT = 19,
    TKB_PROTO_BASIC_GMAC_IN = 20,
    TKB_PROTO_SSLTLS_GCM_OUT = 21,
    TKB_PROTO_SSLTLS_GCM_IN = 22,
    TKB_PROTO_BASIC_XTS_CRYPTO = 23,
    TKB_PROTO_BASIC_KASUMI_HASH = 24,
    TKB_PROTO_BASIC_SNOW_HASH = 25,
    TKB_PROTO_BASIC_AES_EIA2 = 26,
    TKB_PROTO_BASIC_ZUC_HASH = 27,
    TKB_PROTO_BASIC_HASHENC = 28,
    TKB_PROTO_BASIC_DECHASH = 29,
    TKB_PROTO_BASIC_CHACHAPOLY_OUT = 30,
    TKB_PROTO_BASIC_CHACHAPOLY_IN = 31,
    TKB_PROTO_TLS13_GCM_OUT = 32,
    TKB_PROTO_TLS13_GCM_IN = 33,
    TKB_PROTO_TLS13_CHACHAPOLY_OUT = 34,
    TKB_PROTO_TLS13_CHACHAPOLY_IN = 35,
    TKB_PROTO_ESP_CHACHAPOLY_OUT = 36,
    TKB_PROTO_ESP_CHACHAPOLY_IN = 37,
    TKB_PROTO_SSLTLS_CHACHAPOLY_OUT = 38,
    TKB_PROTO_SSLTLS_CHACHAPOLY_IN = 39,
    TKB_PROTO_SSLTLS_CCM_OUT = 40,
    TKB_PROTO_SSLTLS_CCM_IN = 41,
    TKB_PROTO_TLS13_CCM_OUT = 42,
    TKB_PROTO_TLS13_CCM_IN = 43,
    TKB_PROTO_BASIC_HMAC_PRECOMPUTE = 44,
    TKB_PROTO_BASIC_HMAC_CTXPREPARE = 45,
    TKB_PROTO_BASIC_BYPASS = 46,
    TKB_PROTO_BASIC_HASH_UNALIGNED = 47,
    TKB_PROTO_BASIC_GCM_OUT_CONT = 48,
    TKB_PROTO_BASIC_GCM_IN_CONT = 49,
    TKB_PROTO_BASIC_SHAKE = 50,
} token_builder_protocol_t;

/* The header protocol values have to agree with those used in
 * token_builder_core.c.
 *
 * Header protocol operations in the token builder attempt to achieve the
 * same results as firmware using the extended IPsec operations.
 *
 * Values are chosen the same as in sa_builder_extended_internal.h
 */
typedef enum
{
    TKB_HDR_BYPASS = 0,
    TKB_HDR_IPV4_OUT_TRANSP_HDRBYPASS = 1,
    TKB_HDR_IPV4_OUT_TUNNEL = 2,
    TKB_HDR_IPV4_IN_TRANSP_HDRBYPASS = 3,
    TKB_HDR_IPV4_IN_TUNNEL = 4,
    TKB_HDR_IPV4_OUT_TRANSP = 5,
    TKB_HDR_IPV4_IN_TRANSP = 6,
    TKB_HDR_IPV6_OUT_TUNNEL = 7,
    TKB_HDR_IPV6_IN_TUNNEL = 8,
    TKB_HDR_IPV6_OUT_TRANSP_HDRBYPASS = 9,
    TKB_HDR_IPV6_IN_TRANSP_HDRBYPASS = 10,
    TKB_HDR_IPV6_OUT_TRANSP = 11,
    TKB_HDR_IPV6_IN_TRANSP = 12,

    /* DTLS and DLTS-CAPWAP */
    TKB_HDR_IPV4_OUT_DTLS = 13,
    TKB_HDR_IPV6_OUT_DTLS = 33,
    TKB_HDR_DTLS_UDP_IN = 14,

    /* IPsec with NAT-T. Must be in the same order as the non NAT-T
       counterparts */
    TKB_HDR_IPV4_OUT_TRANSP_HDRBYPASS_NATT = 21,
    TKB_HDR_IPV4_OUT_TUNNEL_NATT = 22,
    TKB_HDR_IPV4_IN_TRANSP_HDRBYPASS_NATT = 23,
    TKB_HDR_IPV4_IN_TUNNEL_NATT = 24,
    TKB_HDR_IPV4_OUT_TRANSP_NATT = 25,
    TKB_HDR_IPV4_IN_TRANSP_NATT = 26,
    TKB_HDR_IPV6_OUT_TUNNEL_NATT = 27,
    TKB_HDR_IPV6_IN_TUNNEL_NATT = 28,
    TKB_HDR_IPV6_OUT_TRANSP_HDRBYPASS_NATT = 29,
    TKB_HDR_IPV6_IN_TRANSP_HDRBYPASS_NATT = 30,
    TKB_HDR_IPV6_OUT_TRANSP_NATT = 31,
    TKB_HDR_IPV6_IN_TRANSP_NATT = 32,

} token_builder_hdr_proto_t;

/* The iv handling values have to agree with those used in token_builder_core.c
 */
typedef enum
{
    TKB_IV_INBOUND_CTR = 0,
    TKB_IV_INBOUND_CBC = 1,
    TKB_IV_OUTBOUND_CTR = 2,
    TKB_IV_OUTBOUND_CBC = 3,
    TKB_IV_COPY_CBC = 4,
    TKB_IV_COPY_CTR = 5,
    TKB_IV_OUTBOUND_2WORDS = 6,
    TKB_IV_OUTBOUND_4WORDS = 7,
    TKB_IV_OUTBOUND_TOKEN_2WORDS = 8,
    TKB_IV_COPY_TOKEN_2WORDS = 9,
    TKB_IV_OUTBOUND_TOKEN_SRTP = 10,
    TKB_IV_KASUMI_F8 = 11,
    TKB_IV_SNOW_UEA2 = 12,
    TKB_IV_AES_EEA2 = 13,
    TKB_IV_ZUC_EEA3 = 14,
    TKB_IV_OUTBOUND_TOKEN_4WORDS = 15,
    TKB_IV_COPY_TOKEN_4WORDS = 16,
} token_builder_iv_handling_t;


/* Context update handling for SSL/TLS. The values have to agree with
   those used in token_builder_core.c
*/
typedef enum
{
    TKB_UPD_NULL = 0,
    TKB_UPD_ARC4 = 1,
    TKB_UPD_IV2 = 2,
    TKB_UPD_IV4 = 3,
    TKB_UPD_BLK = 4,
} token_builder_update_handling_t;


/* The token_builder_context_t (Token Context Record) contains all
   information that the Token Builder requires for each SA.
*/
typedef struct
{
    u32 token_header_word;
    u8 protocol;
    u8 hproto;
    u8 iv_byte_count;
    u8 icv_byte_count;
    u8 pad_block_byte_count;
    u8 ext_seq;
    u8 anti_replay;
    u8 seq_offset;
    u8 iv_offset;
    u8 digest_word_count;
    u8 iv_handling;
    u8 digest_offset;
    u8 protocol_next;
    u8 hproto_next;
    u8 iv_handling_next;
    u8 digest_word_count_next;
    u8 header_word_fields_next;
    u32 natt_ports;
    union {
        struct {
            u8 update_handling;
            u8 esp_flags;
            u8 ttl;
            u8 dscp;
            u32 ccm_salt;
            u32 cw0,cw1; /* Context control words */
        } generic;
        struct { // Make the SRTP salt key overlap with fields not used in SRTP.
            u32 salt_key0,salt_key1,salt_key2,salt_key3;
        } srtp;
    } u;
    bool f_first; /* First block is to be hashed */
    u8 hash_block_size; /* size of hash block in bytes */
    u8 msg_len; /* number of bytes stored in tunnel_ip when used as hash data buffer */
    /* Use the buffer also to store message data for unaligned hashing */
    u8 tunnel_ip[TKB_MAX_HASHBLOCK_BYTE_COUNT];
} token_builder_context_t;


#endif /* TOKEN_BUILDER_INTERNAL_H_ */

/* end of file token_builder_internal.h */
