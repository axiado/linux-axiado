/* token_builder_context.c
 *
 * This is the main implementation module for the EIP-96 Token Builder.
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

/*----------------------------------------------------------------------------
 * This module implements (provides) the following interface(s):
 */
#include "token_builder.h"


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */
#include "c_token_builder.h"
#include "basic_defs.h"
#include "clib.h"
#include "log.h"
#include "sa_builder_params.h"
#include "sa_builder_params_ipsec.h"

#include "token_builder_internal.h"
#include "sa_builder_params.h"
#ifdef TKB_ENABLE_PROTO_IPSEC
#include "sa_builder_params_ipsec.h"
#endif
#ifdef TKB_ENABLE_PROTO_SSLTLS
#include "sa_builder_params_ssltls.h"
#endif
#ifdef TKB_ENABLE_PROTO_BASIC
#include "sa_builder_params_basic.h"
#endif
#ifdef TKB_ENABLE_PROTO_SRTP
#include "sa_builder_params_srtp.h"
#endif


/*----------------------------------------------------------------------------
 * Definitions and macros
 */



/*----------------------------------------------------------------------------
 * token_builder_get_context_size
 *
 * Determine the size of the token context record in 32-bit words, which may
 * depend on the SA parameters.
 *
 * sa_params_p (input)
 *   Points to the SA parameters data structure
 * token_context_word32_count_p (output)
 *   Required size of the Token Context Record in 32-bit words.
 *
 * Note: This implementation has one Token Context Record format with a fixed
 *       size. Therefore this function does not look at the SA parameters.
 *
 * Return:
 * TKB_STATUS_OK if successful.
 * TKB_INVALID_PARAMETER if any of the pointer parameters is NULL or if
 *                      the contents of the input data structures are invalid.
 */
token_builder_status_t
token_builder_get_context_size(
    const sa_builder_params_t * const sa_params_p,
    unsigned int * const token_context_word32_count_p
    )
{
#ifdef TKB_STRICT_ARGS_CHECK
    if (sa_params_p == NULL || token_context_word32_count_p == NULL)
    {
        LOG_CRIT("token_builder_get_context_size: NULL pointer supplied\n");
        return TKB_INVALID_PARAMETER;
    }
#endif
    *token_context_word32_count_p =
        ((unsigned int)sizeof(token_builder_context_t) +  sizeof(u32) - 1) /
        sizeof(u32);
    return TKB_STATUS_OK;
}


#ifdef TKB_ENABLE_PROTO_IPSEC
#ifdef TKB_ENABLE_EXTENDED_IPSEC
/*----------------------------------------------------------------------------
 * token_builder_build_context_ipsec
 *
 * Fill in the context fields for extended IPsec operations.
 *
 * sa_params_p (input)
 *   Points to the SA parameters data structure. It is important that
 *   SABuilder_GetSIze() and sa_builder_build_sa() have already been called
 *   for this SA.
 * token_context_internal_p (output)
 *   Buffer to hold the Token Context Record.
 *
 * Return:
 * TKB_STATUS_OK if successful.
 * TKB_INVALID_PARAMETER if any of the pointer parameters is NULL or if
 *                      the contents of the SA parameters are invalid.
 */
static token_builder_status_t
token_builder_build_context_extended(
    const sa_builder_params_t * const sa_params_p,
    token_builder_context_t * const token_context_internal_p)
{
    u8 header_proto;
    sa_builder_params_ipsec_t *sa_params_ipsec_p;
    sa_params_ipsec_p = (sa_builder_params_ipsec_t *)
        (sa_params_p->protocol_extension_p);
    if (sa_params_p->direction == SAB_DIRECTION_OUTBOUND)
    {
        if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_IPV6) !=0)
        {
            if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_PROCESS_IP_HEADERS) !=0)
            {
                if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_TUNNEL) !=0)
                {
                    header_proto = TKB_HDR_IPV6_OUT_TUNNEL;
                }
                else
                {
                    header_proto = TKB_HDR_IPV6_OUT_TRANSP;
                }
            }
            else
            {
                header_proto = TKB_HDR_IPV6_OUT_TRANSP_HDRBYPASS;
            }
        }
        else
        {
            if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_PROCESS_IP_HEADERS) !=0)
            {
                if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_TUNNEL) !=0)
                {
                    header_proto = TKB_HDR_IPV4_OUT_TUNNEL;
                }
                else
                {
                    header_proto = TKB_HDR_IPV4_OUT_TRANSP;
                }
            }
            else
            {
                header_proto = TKB_HDR_IPV4_OUT_TRANSP_HDRBYPASS;
            }
        }

    }
    else
    {
        if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_IPV6) !=0)
        {
            if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_PROCESS_IP_HEADERS) !=0)
            {
                token_context_internal_p->token_header_word |= TKB_HEADER_UPD_HDR;
                if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_TUNNEL) !=0)
                {
                    header_proto = TKB_HDR_IPV6_IN_TUNNEL;
                }
                else
                {
                    header_proto = TKB_HDR_IPV6_IN_TRANSP;
                }
            }
            else
            {
                header_proto = TKB_HDR_IPV6_IN_TRANSP_HDRBYPASS;
            }
        }
        else
        {
            if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_PROCESS_IP_HEADERS) !=0)
            {
                token_context_internal_p->token_header_word |= TKB_HEADER_UPD_HDR;
                if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_TUNNEL) !=0)
                {
                    header_proto = TKB_HDR_IPV4_IN_TUNNEL;
                }
                else
                {
                    header_proto = TKB_HDR_IPV4_IN_TRANSP;
                }
            }
            else
            {
                header_proto = TKB_HDR_IPV4_IN_TRANSP_HDRBYPASS;
            }
        }

    }
    /* If NAT-T selected, select other header protocol range */
    if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_NATT) != 0)
        header_proto += (TKB_HDR_IPV4_OUT_TRANSP_HDRBYPASS_NATT -
                        TKB_HDR_IPV4_OUT_TRANSP_HDRBYPASS);

    token_context_internal_p->hproto = header_proto;

    token_context_internal_p->u.generic.ttl = sa_params_ipsec_p->ttl;
    token_context_internal_p->u.generic.dscp = sa_params_ipsec_p->dscp;
    token_context_internal_p->natt_ports =
        (sa_params_ipsec_p->natt_src_port >> 8) |
        ((sa_params_ipsec_p->natt_src_port & 0xff) << 8) |
        ((sa_params_ipsec_p->natt_dest_port & 0xff00) << 8) |
        ((sa_params_ipsec_p->natt_dest_port & 0xff) << 24 );

    token_context_internal_p->u.generic.esp_flags = 0;
    if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_CLEAR_DF) != 0)
        token_context_internal_p->u.generic.esp_flags |= TKB_ESP_FLAG_CLEAR_DF;
    if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_SET_DF) != 0)
        token_context_internal_p->u.generic.esp_flags |= TKB_ESP_FLAG_SET_DF;
    if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_REPLACE_DSCP) != 0)
        token_context_internal_p->u.generic.esp_flags |= TKB_ESP_FLAG_REPLACE_DSCP;
    if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_CLEAR_ECN) != 0)
        token_context_internal_p->u.generic.esp_flags |= TKB_ESP_FLAG_CLEAR_ECN;

    if (header_proto == TKB_HDR_IPV4_OUT_TUNNEL ||
        header_proto == TKB_HDR_IPV6_OUT_TUNNEL ||
        header_proto == TKB_HDR_IPV4_OUT_TUNNEL_NATT ||
        header_proto == TKB_HDR_IPV6_OUT_TUNNEL_NATT)
    {
        if (sa_params_ipsec_p->src_ip_addr_p == NULL ||
            sa_params_ipsec_p->dest_ip_addr_p == NULL)
        {
            LOG_CRIT("SABuilder: NULL pointer tunnel address.\n");
            return TKB_INVALID_PARAMETER;
        }
        if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_IPV4) != 0)
        {
            memcpy(token_context_internal_p->tunnel_ip,
                   sa_params_ipsec_p->src_ip_addr_p,
                   4);
            memcpy(token_context_internal_p->tunnel_ip + 4,
                   sa_params_ipsec_p->dest_ip_addr_p,
                   4);
        }
        else
        {
            memcpy(token_context_internal_p->tunnel_ip,
                   sa_params_ipsec_p->src_ip_addr_p,
                   16);
            memcpy(token_context_internal_p->tunnel_ip + 16,
                   sa_params_ipsec_p->dest_ip_addr_p,
                   16);
        }
    }
    else if ((header_proto == TKB_HDR_IPV4_OUT_TRANSP_NATT ||
              header_proto == TKB_HDR_IPV4_IN_TRANSP_NATT) &&
             (sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_TRANSPORT_NAT) !=0)
    {
        token_context_internal_p->u.generic.esp_flags |= TKB_ESP_FLAG_NAT;
        if (sa_params_ipsec_p->src_ip_addr_p == NULL ||
            sa_params_ipsec_p->dest_ip_addr_p == NULL)
        {
            LOG_CRIT("SABuilder: NULL pointer NAT address.\n");
            return TKB_INVALID_PARAMETER;
        }
        memcpy(token_context_internal_p->tunnel_ip,
               sa_params_ipsec_p->src_ip_addr_p,
               4);
        memcpy(token_context_internal_p->tunnel_ip + 4,
               sa_params_ipsec_p->dest_ip_addr_p,
               4);
    }
    else if ((header_proto == TKB_HDR_IPV6_OUT_TRANSP_NATT ||
              header_proto == TKB_HDR_IPV6_IN_TRANSP_NATT) &&
             (sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_TRANSPORT_NAT) !=0)
    {
        token_context_internal_p->u.generic.esp_flags |= TKB_ESP_FLAG_NAT;
        if (sa_params_ipsec_p->src_ip_addr_p == NULL ||
            sa_params_ipsec_p->dest_ip_addr_p == NULL)
        {
            LOG_CRIT("SABuilder: NULL pointer NAT address.\n");
            return TKB_INVALID_PARAMETER;
        }
        memcpy(token_context_internal_p->tunnel_ip,
               sa_params_ipsec_p->src_ip_addr_p,
               16);
        memcpy(token_context_internal_p->tunnel_ip + 16,
               sa_params_ipsec_p->dest_ip_addr_p,
               16);
    }
    return TKB_STATUS_OK;
}

/*----------------------------------------------------------------------------
 * token_builder_next_pkt_id
 *
 * Increment the pkt_id if the Token Context describes an IPv4 ESP Tunnel
 * operation
 *
 */
unsigned int
token_builder_next_pkt_id(
    const void * const token_context_p,
    unsigned int pkt_id)
{
    const token_builder_context_t * const token_context_internal_p =
        (const token_builder_context_t * const )token_context_p;
    if (token_context_internal_p->hproto == TKB_HDR_IPV4_OUT_TUNNEL ||
        token_context_internal_p->hproto == TKB_HDR_IPV4_OUT_TUNNEL_NATT)
    {
        return (pkt_id + 1) & MASK_16_BITS;
    }
    else
    {
        return pkt_id;
    }
}


#endif

/*----------------------------------------------------------------------------
 * token_builder_build_context_ipsec
 *
 * Create a Token Context Record for IPsec.
 *
 * sa_params_p (input)
 *   Points to the SA parameters data structure. It is important that
 *   SABuilder_GetSIze() and sa_builder_build_sa() have already been called
 *   for this SA.
 * token_context_internal_p (output)
 *   Buffer to hold the Token Context Record.
 *
 * Return:
 * TKB_STATUS_OK if successful.
 * TKB_INVALID_PARAMETER if any of the pointer parameters is NULL or if
 *                      the contents of the SA parameters are invalid.
 */
static token_builder_status_t
token_builder_build_context_ipsec(
    const sa_builder_params_t * const sa_params_p,
    token_builder_context_t * const token_context_internal_p)
{
    sa_builder_params_ipsec_t *sa_params_ipsec_p;
    sa_params_ipsec_p = (sa_builder_params_ipsec_t *)
        (sa_params_p->protocol_extension_p);
    if (sa_params_ipsec_p == NULL)
    {
        LOG_CRIT("token_builder_build_context:"
                 " IPsec extension pointer is NULL\n");
        return TKB_INVALID_PARAMETER;
    }
    if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_ESP) != 0)
    {
#ifdef TKB_ENABLE_IPSEC_ESP
        token_context_internal_p->ext_seq = 0;
        if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_NO_ANTI_REPLAY) != 0)
            token_context_internal_p->anti_replay = 0;
        else
            token_context_internal_p->anti_replay = 1;

        if (sa_params_p->direction == SAB_DIRECTION_OUTBOUND)
        {
            token_context_internal_p->protocol = TKB_PROTO_ESP_OUT;
            token_context_internal_p->pad_block_byte_count = 4;
            token_context_internal_p->iv_handling = TKB_IV_OUTBOUND_CBC;
            if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_LONG_SEQ) != 0)
                token_context_internal_p->ext_seq = 1;
        }
        else
        {
            token_context_internal_p->protocol = TKB_PROTO_ESP_IN;
            token_context_internal_p->pad_block_byte_count = 4;
            token_context_internal_p->token_header_word |=
                TKB_HEADER_PAD_VERIFY;
            token_context_internal_p->iv_handling = TKB_IV_INBOUND_CBC;
            if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_LONG_SEQ) != 0)
                token_context_internal_p->ext_seq = 1;
            if (token_context_internal_p->ext_seq == 0 &&
                sa_params_p->offset_seq_num + 2 == sa_params_p->offset_seq_mask &&
                token_context_internal_p->anti_replay != 0)
            {
                token_context_internal_p->ext_seq = 2;
                /* Special case, ext_seq==2 do not use extended sequence
                   number, but update context as if we have extended
                   sequence number. Special SA format fixed seqnum/mask
                   offsets.
                */
            }
            token_context_internal_p->anti_replay *=
                sa_params_ipsec_p->sequence_mask_bit_count / 32;
        }
        token_context_internal_p->seq_offset = sa_params_p->offset_seq_num;

        switch (sa_params_p->crypto_algo)
        {
        case SAB_CRYPTO_NULL:
            token_context_internal_p->iv_byte_count = 0;
            break;
        case SAB_CRYPTO_DES:
        case SAB_CRYPTO_3DES:
            token_context_internal_p->iv_byte_count = 8;
            token_context_internal_p->pad_block_byte_count = 8;
            break;
        case SAB_CRYPTO_AES:
        case SAB_CRYPTO_SM4:
        case SAB_CRYPTO_BC0:
            if (sa_params_p->crypto_mode == SAB_CRYPTO_MODE_CBC)
            {
                token_context_internal_p->iv_byte_count = 16;
                token_context_internal_p->pad_block_byte_count = 16;
            }
            else
            {
                if (sa_params_p->direction == SAB_DIRECTION_OUTBOUND)
                    token_context_internal_p->iv_handling =
                        TKB_IV_OUTBOUND_CTR;
                else
                    token_context_internal_p->iv_handling =
                        TKB_IV_INBOUND_CTR;
                if (sa_params_p->iv_src == SAB_IV_SRC_IMPLICIT)
                    token_context_internal_p->iv_byte_count = 0;
                else
                    token_context_internal_p->iv_byte_count = 8;
            }
            break;
        case SAB_CRYPTO_CHACHA20:
            if (sa_params_p->direction == SAB_DIRECTION_OUTBOUND)
                token_context_internal_p->iv_handling =
                    TKB_IV_OUTBOUND_CTR;
            else
                    token_context_internal_p->iv_handling =
                        TKB_IV_INBOUND_CTR;
            if (sa_params_p->iv_src == SAB_IV_SRC_IMPLICIT)
                token_context_internal_p->iv_byte_count = 0;
            else
                token_context_internal_p->iv_byte_count = 8;
            break;
        default:
            LOG_CRIT("token_builder_build_context:"
                     " unsupported crypto algorithm.\n");
            return TKB_INVALID_PARAMETER;
        }

        /* For all inbound and CTR mode outbound packets there is
           only one supported way to obtain the iv, which is already
           taken care of. Now handle outbound CBC. */
        if(sa_params_p->crypto_mode == SAB_CRYPTO_MODE_CBC &&
           sa_params_p->direction == SAB_DIRECTION_OUTBOUND &&
           sa_params_p->crypto_algo != SAB_CRYPTO_NULL)
        {
            switch (sa_params_p->iv_src)
            {
            case SAB_IV_SRC_DEFAULT:
            case SAB_IV_SRC_PRNG:
                token_context_internal_p->token_header_word |=
                    TKB_HEADER_IV_PRNG;
                break;
            case SAB_IV_SRC_SA: /* No action required */
                break;
            case SAB_IV_SRC_TOKEN:
                if (token_context_internal_p->iv_byte_count == 8)
                {
                    token_context_internal_p->iv_handling =
                        TKB_IV_OUTBOUND_TOKEN_2WORDS;
                    token_context_internal_p->token_header_word |=
                        TKB_HEADER_IV_TOKEN_2WORDS;
                }
                else
                {
                    token_context_internal_p->iv_handling =
                        TKB_IV_OUTBOUND_TOKEN_4WORDS;
                    token_context_internal_p->token_header_word |=
                        TKB_HEADER_IV_TOKEN_4WORDS;
                }
                break;
            default:
                LOG_CRIT("token_builder_build_context:"
                         "Unsupported iv source\n");
                return TKB_INVALID_PARAMETER;
            }
        }

        if (sa_params_p->direction == SAB_DIRECTION_OUTBOUND &&
            sa_params_ipsec_p->pad_alignment >
            token_context_internal_p->pad_block_byte_count &&
            sa_params_ipsec_p->pad_alignment <= 256)
            token_context_internal_p->pad_block_byte_count =
                sa_params_ipsec_p->pad_alignment;

        switch(sa_params_p->auth_algo)
        {
        case SAB_AUTH_NULL:
            token_context_internal_p->icv_byte_count = 0;
            token_context_internal_p->ext_seq = 0;
            break;
        case SAB_AUTH_HMAC_MD5:
        case SAB_AUTH_HMAC_SHA1:
        case SAB_AUTH_AES_XCBC_MAC:
        case SAB_AUTH_AES_CMAC_128:
            token_context_internal_p->icv_byte_count = 12;
            break;
        case SAB_AUTH_HMAC_SHA2_224:
        case SAB_AUTH_HMAC_SHA2_256:
        case SAB_AUTH_HMAC_SM3:
            token_context_internal_p->icv_byte_count = 16;
            break;
        case SAB_AUTH_HMAC_SHA2_384:
            token_context_internal_p->icv_byte_count = 24;
            break;
        case SAB_AUTH_HMAC_SHA2_512:
            token_context_internal_p->icv_byte_count = 32;
            break;
        case SAB_AUTH_AES_CCM:
        case SAB_AUTH_AES_GCM:
        case SAB_AUTH_AES_GMAC:
            // All these protocols have a selectable ICV length.
            if (sa_params_ipsec_p->icv_byte_count == 8 ||
                sa_params_ipsec_p->icv_byte_count == 12 ||
                sa_params_ipsec_p->icv_byte_count == 16)
            {
                token_context_internal_p->icv_byte_count =
                    sa_params_ipsec_p->icv_byte_count;
            }
            else
            {
                token_context_internal_p->icv_byte_count = 16;
            }
            switch (sa_params_p->auth_algo)
            {
                /* These protocols need specialized protocol codes
                   for the token generator.*/
            case SAB_AUTH_AES_CCM:
                if (sa_params_p->direction == SAB_DIRECTION_OUTBOUND)
                {
                    token_context_internal_p->protocol = TKB_PROTO_ESP_CCM_OUT;
                }
                else
                {
                    token_context_internal_p->protocol = TKB_PROTO_ESP_CCM_IN;
                }
                if (sa_params_p->nonce_p == NULL)
                {
                    LOG_CRIT("token_builder_build_context: NULL nonce pointer.\n");
                    return TKB_INVALID_PARAMETER;
                }
                token_context_internal_p->u.generic.ccm_salt =
                    (sa_params_p->nonce_p[0] << 8) |
                    (sa_params_p->nonce_p[1] << 16) |
                    (sa_params_p->nonce_p[2] << 24) |
                    TKB_CCM_FLAG_ADATA_L4 |
                    ((token_context_internal_p->icv_byte_count-2)*4);
                break;
            case SAB_AUTH_AES_GCM:
                if (sa_params_p->direction == SAB_DIRECTION_OUTBOUND)
                {
                    token_context_internal_p->protocol = TKB_PROTO_ESP_GCM_OUT;
                }
                else
                {
                    token_context_internal_p->protocol = TKB_PROTO_ESP_GCM_IN;
                }
                break;
            case SAB_AUTH_AES_GMAC:
                if (sa_params_p->direction == SAB_DIRECTION_OUTBOUND)
                {
                    token_context_internal_p->protocol = TKB_PROTO_ESP_GMAC_OUT;
                }
                else
                {
                    token_context_internal_p->protocol = TKB_PROTO_ESP_GMAC_IN;
                }
                break;
            default:
                ;
            }
            break;
        case SAB_AUTH_POLY1305:
            token_context_internal_p->icv_byte_count = 16;
            if (sa_params_p->direction == SAB_DIRECTION_OUTBOUND)
            {
                token_context_internal_p->protocol =
                    TKB_PROTO_ESP_CHACHAPOLY_OUT;
            }
            else
            {
                token_context_internal_p->protocol =
                    TKB_PROTO_ESP_CHACHAPOLY_IN;
            }
            break;
        default:
            LOG_CRIT("token_builder_build_context:"
                     " unsupported authentication algorithm.\n");
            return TKB_INVALID_PARAMETER;
        }
#else /* TKB_ENABLE_IPSEC_ESP */
        LOG_CRIT("token_builder_build_context: ESP not supported\n");
        return TKB_INVALID_PARAMETER;
#endif /* TKB_ENABLE_IPSEC_ESP */
    }
    else if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_AH) != 0)
    {
#ifdef TKB_ENABLE_IPSEC_AH

#else /* TKB_ENABLE_IPSEC_AH */
        LOG_CRIT("token_builder_build_context: AH not supported\n");
        return TKB_INVALID_PARAMETER;
#endif /* TKB_ENABLE_IPSEC_AH */
    }
    else
    {
        LOG_CRIT("token_builder_build_context: Neither ESP nor AH set\n");
        return TKB_INVALID_PARAMETER;
    }
#ifdef TKB_ENABLE_EXTENDED_IPSEC
    if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_EXT_PROCESSING) != 0)
    {
        token_builder_status_t rc;

        rc = token_builder_build_context_extended(sa_params_p,
                                                token_context_internal_p);
        return rc;
    }
#endif
    return TKB_STATUS_OK;
}
#endif


#ifdef TKB_ENABLE_PROTO_SSLTLS
/*----------------------------------------------------------------------------
 * token_builder_build_context_ssltls
 *
 * Create a Token Context Record for SSL/TLS/DTLS
 *
 * sa_params_p (input)
 *   Points to the SA parameters data structure. It is important that
 *   SABuilder_GetSIze() and sa_builder_build_sa() have already been called
 *   for this SA.
 * token_context_internal_p (output)
 *   Buffer to hold the Token Context Record.
 *
 * Return:
 * TKB_STATUS_OK if successful.
 * TKB_INVALID_PARAMETER if any of the pointer parameters is NULL or if
 *                      the contents of the SA parameters are invalid.
 */
static token_builder_status_t
token_builder_build_context_ssltls(
    const sa_builder_params_t * const sa_params_p,
    token_builder_context_t * const token_context_internal_p)
{
    sa_builder_params_ssltls_t *sa_params_ssltls_p;
    sa_params_ssltls_p = (sa_builder_params_ssltls_t *)
        (sa_params_p->protocol_extension_p);
    if (sa_params_ssltls_p == NULL)
    {
        LOG_CRIT("token_builder_build_context:"
                 " SSL/TLS extension pointer is NULL\n");
        return TKB_INVALID_PARAMETER;
    }

    token_context_internal_p->ext_seq = 0;
    token_context_internal_p->anti_replay = 1;
    token_context_internal_p->iv_offset = 0;
    token_context_internal_p->digest_word_count = 0;
    token_context_internal_p->u.generic.esp_flags = 0;
    if (sa_params_p->direction == SAB_DIRECTION_OUTBOUND)
    {
        token_context_internal_p->protocol = TKB_PROTO_SSLTLS_OUT;
        token_context_internal_p->pad_block_byte_count = 1;
        token_context_internal_p->iv_handling = TKB_IV_OUTBOUND_CBC;
    }
    else
    {
        token_context_internal_p->protocol = TKB_PROTO_SSLTLS_IN;
        token_context_internal_p->pad_block_byte_count = 1;
        if (sa_params_p->crypto_algo != SAB_CRYPTO_NULL &&
            sa_params_p->crypto_algo != SAB_CRYPTO_ARCFOUR)
            token_context_internal_p->token_header_word |=
                TKB_HEADER_PAD_VERIFY;
        token_context_internal_p->iv_handling = TKB_IV_INBOUND_CBC;
    }
    token_context_internal_p->seq_offset = sa_params_p->offset_seq_num;

    switch (sa_params_p->crypto_algo)
    {
    case SAB_CRYPTO_NULL:
        token_context_internal_p->iv_byte_count = 0;
        token_context_internal_p->u.generic.update_handling = TKB_UPD_NULL;
        break;
    case SAB_CRYPTO_ARCFOUR:
        token_context_internal_p->iv_byte_count = 0;
        token_context_internal_p->u.generic.update_handling = TKB_UPD_ARC4;
        token_context_internal_p->iv_offset = sa_params_p->offset_ij_ptr;
        break;
    case SAB_CRYPTO_DES:
    case SAB_CRYPTO_3DES:
        token_context_internal_p->iv_byte_count = 8;
        token_context_internal_p->pad_block_byte_count = 8;
        token_context_internal_p->u.generic.update_handling = TKB_UPD_IV2;
        token_context_internal_p->iv_offset = sa_params_p->offset_iv;
        break;
    case SAB_CRYPTO_AES:
    case SAB_CRYPTO_SM4:
    case SAB_CRYPTO_BC0:
        if (sa_params_p->crypto_mode == SAB_CRYPTO_MODE_GCM)
        {
            token_context_internal_p->iv_byte_count = 8;
            token_context_internal_p->u.generic.update_handling = TKB_UPD_NULL;
            token_context_internal_p->iv_offset = sa_params_p->offset_iv;
            if (sa_params_p->direction == SAB_DIRECTION_OUTBOUND)
            {
                token_context_internal_p->iv_handling = TKB_IV_OUTBOUND_CTR;
                if (sa_params_ssltls_p->version == SAB_TLS_VERSION_1_3)
                {
                    token_context_internal_p->protocol =
                        TKB_PROTO_TLS13_GCM_OUT;
                }
                else
                {
                    token_context_internal_p->protocol =
                        TKB_PROTO_SSLTLS_GCM_OUT;
                }
            }
            else
            {
                token_context_internal_p->iv_handling = TKB_IV_INBOUND_CTR;
                if (sa_params_ssltls_p->version == SAB_TLS_VERSION_1_3)
                {
                    token_context_internal_p->protocol =
                        TKB_PROTO_TLS13_GCM_IN;
                }
                else
                {
                    token_context_internal_p->protocol =
                        TKB_PROTO_SSLTLS_GCM_IN;
                }
            }
        }
        else if (sa_params_p->crypto_mode == SAB_CRYPTO_MODE_CCM)
        {
            token_context_internal_p->iv_byte_count = 8;
            token_context_internal_p->u.generic.update_handling = TKB_UPD_NULL;
            token_context_internal_p->iv_offset = sa_params_p->offset_iv;
            if (sa_params_p->direction == SAB_DIRECTION_OUTBOUND)
            {
                token_context_internal_p->iv_handling = TKB_IV_OUTBOUND_CTR;
                if (sa_params_ssltls_p->version == SAB_TLS_VERSION_1_3)
                {
                    token_context_internal_p->protocol =
                        TKB_PROTO_TLS13_CCM_OUT;
                }
                else
                {
                    token_context_internal_p->protocol =
                        TKB_PROTO_SSLTLS_CCM_OUT;
                }
            }
            else
            {
                token_context_internal_p->iv_handling = TKB_IV_INBOUND_CTR;
                if (sa_params_ssltls_p->version == SAB_TLS_VERSION_1_3)
                {
                    token_context_internal_p->protocol =
                        TKB_PROTO_TLS13_CCM_IN;
                }
                else
                {
                    token_context_internal_p->protocol =
                        TKB_PROTO_SSLTLS_CCM_IN;
                }
            }
            if (sa_params_ssltls_p->icv_byte_count == 0)
                token_context_internal_p->icv_byte_count = 16;
            else if (sa_params_ssltls_p->icv_byte_count == 8 ||
                     sa_params_ssltls_p->icv_byte_count == 16)
            {
                token_context_internal_p->icv_byte_count = sa_params_ssltls_p->icv_byte_count;
            }
            else
            {
                LOG_CRIT("token_builder_build_context:"
                         " TLS AES-CCM rquires tag length of 8 or 16\n");
                return TKB_INVALID_PARAMETER;
            }
            token_context_internal_p->u.generic.ccm_salt = TKB_CCM_FLAG_ADATA_L3 |
                ((token_context_internal_p->icv_byte_count-2)*4);
        }
        else
        {
            token_context_internal_p->iv_byte_count = 16;
            token_context_internal_p->pad_block_byte_count = 16;
            token_context_internal_p->u.generic.update_handling = TKB_UPD_IV4;
            token_context_internal_p->iv_offset = sa_params_p->offset_iv;
        }
        break;
    case SAB_CRYPTO_CHACHA20:
        token_context_internal_p->iv_byte_count = 0;
        token_context_internal_p->u.generic.update_handling = TKB_UPD_NULL;
        if (sa_params_p->direction == SAB_DIRECTION_OUTBOUND)
        {
            if (sa_params_ssltls_p->version == SAB_TLS_VERSION_1_3)
            {
                token_context_internal_p->protocol = TKB_PROTO_TLS13_CHACHAPOLY_OUT;
            }
            else
            {
                token_context_internal_p->protocol = TKB_PROTO_SSLTLS_CHACHAPOLY_OUT;
            }
        }
        else
        {
            if (sa_params_ssltls_p->version == SAB_TLS_VERSION_1_3)
            {
                token_context_internal_p->protocol = TKB_PROTO_TLS13_CHACHAPOLY_IN;
            }
            else
            {
                token_context_internal_p->protocol = TKB_PROTO_SSLTLS_CHACHAPOLY_IN;
            }
        }
        break;
    default:
        LOG_CRIT("token_builder_build_context:"
                 " unsupported crypto algorithm.\n");
        return TKB_INVALID_PARAMETER;
    }

    /* For all inbound and ARCFOUR outbound packets there is
       only one supported way to obtain the iv, which is already
       taken care of. Now handle outbound CBC. */
    if(sa_params_p->crypto_mode == SAB_CRYPTO_MODE_CBC &&
       sa_params_p->direction == SAB_DIRECTION_OUTBOUND &&
       sa_params_p->crypto_algo != SAB_CRYPTO_NULL)
    {
        switch (sa_params_p->iv_src)
        {
        case SAB_IV_SRC_DEFAULT:
        case SAB_IV_SRC_PRNG:
            token_context_internal_p->token_header_word |=
                TKB_HEADER_IV_PRNG;
            break;
        case SAB_IV_SRC_SA: /* No action required */
            break;
        case SAB_IV_SRC_TOKEN:
            if (token_context_internal_p->iv_byte_count == 8)
            {
                token_context_internal_p->iv_handling =
                    TKB_IV_OUTBOUND_TOKEN_2WORDS;
                token_context_internal_p->token_header_word |=
                    TKB_HEADER_IV_TOKEN_2WORDS;
            }
            else
            {
                token_context_internal_p->iv_handling =
                    TKB_IV_OUTBOUND_TOKEN_4WORDS;
                token_context_internal_p->token_header_word |=
                    TKB_HEADER_IV_TOKEN_4WORDS;
            }
            break;
        default:
            LOG_CRIT("token_builder_build_context:"
                     "Unsupported iv source\n");
            return TKB_INVALID_PARAMETER;
        }
    }

    /* In SSL and TLS1.0 the iv does not appear explicitly in the packet */
    if (sa_params_ssltls_p->version == SAB_SSL_VERSION_3_0 ||
        sa_params_ssltls_p->version == SAB_TLS_VERSION_1_0)
        token_context_internal_p->iv_byte_count = 0;
    else if (token_context_internal_p->u.generic.update_handling == TKB_UPD_IV2 ||
             token_context_internal_p->u.generic.update_handling == TKB_UPD_IV4)
        token_context_internal_p->u.generic.update_handling = TKB_UPD_BLK;

    /* Sequence numbers appear explicitly in the packet in DTLS */
    if (sa_params_ssltls_p->version == SAB_DTLS_VERSION_1_0 ||
        sa_params_ssltls_p->version == SAB_DTLS_VERSION_1_2)
    {
        if ((sa_params_ssltls_p->ssltls_flags & SAB_DTLS_CAPWAP) != 0)
            token_context_internal_p->u.generic.esp_flags |= TKB_DTLS_FLAG_CAPWAP;

        if ((sa_params_ssltls_p->ssltls_flags & SAB_DTLS_NO_ANTI_REPLAY) != 0)
            token_context_internal_p->ext_seq = 1;
        else
            token_context_internal_p->ext_seq = 1 +
                sa_params_ssltls_p->sequence_mask_bit_count / 32;
#ifdef TKB_ENABLE_EXTENDED_DTLS
        if ((sa_params_ssltls_p->ssltls_flags &  SAB_DTLS_EXT_PROCESSING) != 0)
        {
            if (sa_params_p->direction == SAB_DIRECTION_OUTBOUND)
            {
                if ((sa_params_ssltls_p->ssltls_flags & SAB_DTLS_IPV6) != 0)
                    token_context_internal_p->hproto = TKB_HDR_IPV6_OUT_DTLS;
                else
                    token_context_internal_p->hproto = TKB_HDR_IPV4_OUT_DTLS;
            }
            else
            {
                token_context_internal_p->hproto = TKB_HDR_DTLS_UDP_IN;
                if ((sa_params_ssltls_p->ssltls_flags &  SAB_DTLS_PLAINTEXT_HEADERS) != 0)
                    token_context_internal_p->u.generic.esp_flags |= TKB_DTLS_FLAG_PLAINTEXT_HDR;
            }
        }
#endif
    }

    /* Version field is not hashed in SSL 3.0. */
    if (sa_params_ssltls_p->version == SAB_SSL_VERSION_3_0)
        token_context_internal_p->anti_replay = 0;


    switch(sa_params_p->auth_algo)
    {
    case SAB_AUTH_HMAC_MD5:
    case SAB_AUTH_SSLMAC_MD5:
        token_context_internal_p->icv_byte_count = 16;
        break;
    case SAB_AUTH_HMAC_SHA1:
    case SAB_AUTH_SSLMAC_SHA1:
        token_context_internal_p->icv_byte_count = 20;
        break;
    case SAB_AUTH_HMAC_SHA2_224:
        token_context_internal_p->icv_byte_count = 28;
        break;
    case SAB_AUTH_HMAC_SHA2_256:
    case SAB_AUTH_HMAC_SM3:
        token_context_internal_p->icv_byte_count = 32;
        break;
    case SAB_AUTH_HMAC_SHA2_384:
        token_context_internal_p->icv_byte_count = 48;
        break;
    case SAB_AUTH_HMAC_SHA2_512:
        token_context_internal_p->icv_byte_count = 64;
        break;
    case SAB_AUTH_AES_GCM:
        token_context_internal_p->icv_byte_count = 16;
        break;
    case SAB_AUTH_AES_CCM:
        /* Was already set earlier */
        break;
    case SAB_AUTH_POLY1305:
        token_context_internal_p->icv_byte_count = 16;
        break;
    default:
        LOG_CRIT("token_builder_build_context:"
                 " unsupported authentication algorithm.\n");
        return TKB_INVALID_PARAMETER;
    }
    if (sa_params_ssltls_p->icv_byte_count != 0 &&
        sa_params_ssltls_p->icv_byte_count < token_context_internal_p->icv_byte_count)
    {
        token_context_internal_p->icv_byte_count = sa_params_ssltls_p->icv_byte_count;
    }
    return TKB_STATUS_OK;
}
#endif


#ifdef TKB_ENABLE_PROTO_BASIC
/*----------------------------------------------------------------------------
 * token_builder_build_context_basic
 *
 * Create a Token Context Record for Basic crypto
 *
 * sa_params_p (input)
 *   Points to the SA parameters data structure. It is important that
 *   SABuilder_GetSIze() and sa_builder_build_sa() have already been called
 *   for this SA.
 * token_context_internal_p (output)
 *   Buffer to hold the Token Context Record.
 *
 * Return:
 * TKB_STATUS_OK if successful.
 * TKB_INVALID_PARAMETER if any of the pointer parameters is NULL or if
 *                      the contents of the SA parameters are invalid.
 */
static token_builder_status_t
token_builder_build_context_basic(
    const sa_builder_params_t * const sa_params_p,
    token_builder_context_t * const token_context_internal_p)
{
    sa_builder_params_basic_t *sa_params_basic_p;
    sa_params_basic_p = (sa_builder_params_basic_t *)
        (sa_params_p->protocol_extension_p);
    if (sa_params_basic_p == NULL)
    {
        LOG_CRIT("token_builder_build_context:"
                 " Basic extension pointer is NULL\n");
        return TKB_INVALID_PARAMETER;
    }

    token_context_internal_p->ext_seq = 0;
    token_context_internal_p->anti_replay = 0;
    token_context_internal_p->iv_offset = 0;
    token_context_internal_p->seq_offset = 0;
    token_context_internal_p->pad_block_byte_count = 1;
    token_context_internal_p->iv_byte_count = 0;
    token_context_internal_p->icv_byte_count = 0;
    token_context_internal_p->digest_word_count = 0;
    token_context_internal_p->iv_handling = TKB_IV_OUTBOUND_CBC;
    token_context_internal_p->u.generic.update_handling = TKB_UPD_NULL;
    token_context_internal_p->u.generic.ccm_salt = 0;

    if (sa_params_p->crypto_algo != SAB_CRYPTO_NULL)
    {   /* Basic crypto */
        if (sa_params_p->auth_algo == SAB_AUTH_NULL)
        {
            token_context_internal_p->protocol = TKB_PROTO_BASIC_CRYPTO;
        }
        else if ((sa_params_basic_p->basic_flags & SAB_BASIC_FLAG_ENCRYPT_AFTER_HASH) != 0)
        {
            if (sa_params_p->direction == SAB_DIRECTION_OUTBOUND)
            {
                token_context_internal_p->protocol = TKB_PROTO_BASIC_HASHENC;
            }
            else
            {
                token_context_internal_p->protocol = TKB_PROTO_BASIC_DECHASH;
                token_context_internal_p->token_header_word |=
                    TKB_HEADER_PAD_VERIFY;
            }
        }
#ifdef TKB_ENABLE_CRYPTO_CHACHAPOLY
        else if (sa_params_p->crypto_algo == SAB_CRYPTO_CHACHA20)
        {
            if (sa_params_p->direction == SAB_DIRECTION_OUTBOUND)
            {
                token_context_internal_p->protocol = TKB_PROTO_BASIC_CHACHAPOLY_OUT;
            }
            else
            {
                token_context_internal_p->protocol = TKB_PROTO_BASIC_CHACHAPOLY_IN;
            }
        }
#endif
        else
        {
            token_context_internal_p->protocol = TKB_PROTO_BASIC_CRYPTHASH;
        }
        switch (sa_params_p->crypto_algo)
        {
        case SAB_CRYPTO_ARCFOUR:
            token_context_internal_p->iv_byte_count = 0;
            if (sa_params_p->crypto_mode != SAB_CRYPTO_MODE_STATELESS)
                token_context_internal_p->u.generic.update_handling = TKB_UPD_ARC4;
            token_context_internal_p->iv_offset = sa_params_p->offset_ij_ptr;
            break;
        case SAB_CRYPTO_DES:
        case SAB_CRYPTO_3DES:
            token_context_internal_p->iv_byte_count = 8;
            token_context_internal_p->pad_block_byte_count = 8;
            token_context_internal_p->u.generic.update_handling = TKB_UPD_IV2;
            token_context_internal_p->iv_offset = sa_params_p->offset_iv;
            break;
        case SAB_CRYPTO_AES:
        case SAB_CRYPTO_SM4:
        case SAB_CRYPTO_BC0:
            token_context_internal_p->iv_byte_count = 16;
            token_context_internal_p->pad_block_byte_count = 16;
            if (sa_params_p->crypto_mode== SAB_CRYPTO_MODE_EEA2)
            {
                token_context_internal_p->iv_handling = TKB_IV_AES_EEA2;
                token_context_internal_p->u.generic.update_handling = TKB_UPD_NULL;
            } else
            {
                token_context_internal_p->u.generic.update_handling = TKB_UPD_IV4;
            }
            token_context_internal_p->iv_offset = sa_params_p->offset_iv;
            break;
#ifdef TKB_ENABLE_CRYPTO_WIRELESS
        case SAB_CRYPTO_KASUMI:
            token_context_internal_p->iv_byte_count = 8;
            token_context_internal_p->pad_block_byte_count = 8;
            token_context_internal_p->iv_handling = TKB_IV_KASUMI_F8;
            token_context_internal_p->u.generic.update_handling = TKB_UPD_NULL;
            token_context_internal_p->iv_offset = sa_params_p->offset_iv;
            break;
        case SAB_CRYPTO_SNOW:
            token_context_internal_p->iv_byte_count = 16;
            token_context_internal_p->pad_block_byte_count = 1;
            token_context_internal_p->iv_handling = TKB_IV_SNOW_UEA2;
            token_context_internal_p->u.generic.update_handling = TKB_UPD_NULL;
            token_context_internal_p->iv_offset = sa_params_p->offset_iv;
            break;
        case SAB_CRYPTO_ZUC:
            token_context_internal_p->iv_byte_count = 16;
            token_context_internal_p->pad_block_byte_count = 1;
            token_context_internal_p->iv_handling = TKB_IV_ZUC_EEA3;
            token_context_internal_p->u.generic.update_handling = TKB_UPD_NULL;
            token_context_internal_p->iv_offset = sa_params_p->offset_iv;
            break;
#endif
#ifdef TKB_ENABLE_CRYPTO_CHACHAPOLY
        case SAB_CRYPTO_CHACHA20:
            token_context_internal_p->iv_byte_count = 16;
            token_context_internal_p->pad_block_byte_count = 1;
            token_context_internal_p->u.generic.update_handling = TKB_UPD_NULL;
            break;
#endif
        default:
            LOG_CRIT("token_builder_build_context:"
                     " unsupported crypto algorithm.\n");
            return TKB_INVALID_PARAMETER;
        }

        switch (sa_params_p->crypto_mode)
        {
        case SAB_CRYPTO_MODE_ECB:
            token_context_internal_p->iv_byte_count = 0;
            token_context_internal_p->iv_handling = TKB_IV_OUTBOUND_CBC;
            token_context_internal_p->u.generic.update_handling = TKB_UPD_NULL;
            break;
        case SAB_CRYPTO_MODE_BASIC:
        case SAB_CRYPTO_MODE_CBC:
        case SAB_CRYPTO_MODE_ICM:
        case SAB_CRYPTO_MODE_CFB:
        case SAB_CRYPTO_MODE_OFB:
        case SAB_CRYPTO_MODE_XTS:
        case SAB_CRYPTO_MODE_XTS_STATEFUL:
        case SAB_CRYPTO_MODE_CHACHA_CTR32:
        case SAB_CRYPTO_MODE_CHACHA_CTR64:
            if (sa_params_p->crypto_mode == SAB_CRYPTO_MODE_BASIC)
            {
                token_context_internal_p->iv_handling = TKB_IV_OUTBOUND_CBC;
                if (sa_params_p->crypto_algo == SAB_CRYPTO_KASUMI)
                {
                    token_context_internal_p->iv_byte_count = 0;
                    token_context_internal_p->u.generic.update_handling = TKB_UPD_NULL;
                    break;
                }
                else
                {
                    token_context_internal_p->pad_block_byte_count = 1;
                }
            }
            else if (sa_params_p->crypto_mode == SAB_CRYPTO_MODE_ICM)
            {
                token_context_internal_p->pad_block_byte_count = 1;
            }
            else if (sa_params_p->crypto_mode == SAB_CRYPTO_MODE_XTS ||
                     sa_params_p->crypto_mode == SAB_CRYPTO_MODE_XTS_STATEFUL)
            {
                token_context_internal_p->pad_block_byte_count = 1;
                token_context_internal_p->protocol = TKB_PROTO_BASIC_XTS_CRYPTO;
            }

            if (sa_params_p->iv_src == SAB_IV_SRC_TOKEN)
            {
                token_context_internal_p->u.generic.update_handling = TKB_UPD_NULL;
                if (token_context_internal_p->iv_byte_count == 8)
                {
                    token_context_internal_p->iv_byte_count = 0;
                    if ((sa_params_p->flags & SAB_FLAG_COPY_IV) != 0)
                        token_context_internal_p->iv_handling =
                            TKB_IV_COPY_TOKEN_2WORDS;
                    else
                        token_context_internal_p->iv_handling =
                            TKB_IV_OUTBOUND_TOKEN_2WORDS;
                    token_context_internal_p->token_header_word |=
                        TKB_HEADER_IV_TOKEN_2WORDS;
                }
                else
                {
                    token_context_internal_p->iv_byte_count = 0;
                    if ((sa_params_p->flags & SAB_FLAG_COPY_IV) != 0)
                        token_context_internal_p->iv_handling =
                            TKB_IV_COPY_TOKEN_4WORDS;
                    else
                        token_context_internal_p->iv_handling =
                            TKB_IV_OUTBOUND_TOKEN_4WORDS;
                    token_context_internal_p->token_header_word |=
                        TKB_HEADER_IV_TOKEN_4WORDS;
                }
            }
            else if (sa_params_p->iv_src == SAB_IV_SRC_INPUT)
            {
                token_context_internal_p->u.generic.update_handling = TKB_UPD_NULL;
                if ((sa_params_p->flags & SAB_FLAG_COPY_IV) != 0)
                    token_context_internal_p->iv_handling = TKB_IV_COPY_CBC;
                else
                    token_context_internal_p->iv_handling = TKB_IV_INBOUND_CBC;
            }
            else
            {
                if(sa_params_p->iv_src ==  SAB_IV_SRC_PRNG)
                {
                    token_context_internal_p->token_header_word |=
                        TKB_HEADER_IV_PRNG;
                    token_context_internal_p->u.generic.update_handling = TKB_UPD_NULL;
                }
                if ((sa_params_p->flags & SAB_FLAG_COPY_IV) != 0)
                {
                    if (token_context_internal_p->iv_byte_count==8)
                        token_context_internal_p->iv_handling =
                            TKB_IV_OUTBOUND_2WORDS;
                    else
                        token_context_internal_p->iv_handling =
                            TKB_IV_OUTBOUND_4WORDS;
                }
                token_context_internal_p->iv_byte_count = 0;
            }

            break;
        case SAB_CRYPTO_MODE_CTR:
        case SAB_CRYPTO_MODE_CCM:
        case SAB_CRYPTO_MODE_GCM:
        case SAB_CRYPTO_MODE_GMAC:
            token_context_internal_p->pad_block_byte_count = 1;
            token_context_internal_p->u.generic.update_handling = TKB_UPD_NULL;

            if (sa_params_p->iv_src == SAB_IV_SRC_TOKEN)
            {
                token_context_internal_p->iv_byte_count = 0;
                if ((sa_params_p->flags & SAB_FLAG_COPY_IV) != 0)
                    token_context_internal_p->iv_handling =
                        TKB_IV_COPY_TOKEN_4WORDS;
                else
                    token_context_internal_p->iv_handling =
                        TKB_IV_OUTBOUND_TOKEN_4WORDS;
                token_context_internal_p->token_header_word |=
                    TKB_HEADER_IV_TOKEN_4WORDS;
            }
            else if (sa_params_p->iv_src == SAB_IV_SRC_INPUT)
            {
                token_context_internal_p->iv_byte_count = 8;
                if ((sa_params_p->flags & SAB_FLAG_COPY_IV) != 0)
                    token_context_internal_p->iv_handling = TKB_IV_COPY_CTR;
                else
                    token_context_internal_p->iv_handling = TKB_IV_INBOUND_CTR;
            }
            else
            {
                token_context_internal_p->iv_byte_count = 0;
                if ((sa_params_p->flags & SAB_FLAG_COPY_IV) != 0)
                    token_context_internal_p->iv_handling = TKB_IV_OUTBOUND_CTR;
            }
            break;
#ifdef TKB_ENABLE_CRYPTO_WIRELESS
        case SAB_CRYPTO_MODE_F8:
        case SAB_CRYPTO_MODE_UEA2:
        case SAB_CRYPTO_MODE_EEA2:
        case SAB_CRYPTO_MODE_EEA3:
            token_context_internal_p->pad_block_byte_count = 1;
            token_context_internal_p->u.generic.ccm_salt =
                (sa_params_basic_p->bearer << 3) |
                (sa_params_basic_p->direction<<2);
            if (token_context_internal_p->iv_byte_count == 8)
                token_context_internal_p->token_header_word |=
                    TKB_HEADER_IV_TOKEN_2WORDS;
            else
                token_context_internal_p->token_header_word |=
                    TKB_HEADER_IV_TOKEN_4WORDS;
            token_context_internal_p->iv_byte_count = 0;
            break;
#endif
            default:
                ;
        }
    }
    else if (sa_params_p->auth_algo == SAB_AUTH_NULL)
    {
        token_context_internal_p->protocol = TKB_PROTO_BASIC_BYPASS;
    }
    else if (sa_params_basic_p->basic_flags & SAB_BASIC_FLAG_HMAC_PRECOMPUTE &&
             sa_params_p->offset_digest1 != 0)
    {
        token_context_internal_p->protocol = TKB_PROTO_BASIC_HMAC_PRECOMPUTE;
    }
    else
    {
        if ((sa_params_basic_p->basic_flags & SAB_BASIC_FLAG_HASH_UNALIGNED) !=0)
        {
            token_context_internal_p->protocol = TKB_PROTO_BASIC_HASH_UNALIGNED;
        }
        else
        {
            token_context_internal_p->protocol = TKB_PROTO_BASIC_HASH;
        }
    }
    if (sa_params_p->auth_algo != SAB_AUTH_NULL)
    {
        bool f_is_hmac = false;
        /* Basic hash */
        if ((sa_params_p->flags & (SAB_FLAG_HASH_SAVE|
                                  SAB_FLAG_HASH_INTERMEDIATE)) != 0)
        {
            /* We intend to store the hash state.*/
            token_context_internal_p->seq_offset = sa_params_p->offset_digest0;
        }
        if(sa_params_p->crypto_algo == SAB_CRYPTO_NULL)
        {
            /* Use ext_seq variable to specify that payload will be copied */
            if ((sa_params_p->flags & SAB_FLAG_SUPPRESS_PAYLOAD) == 0)
                token_context_internal_p->ext_seq = 1;
        }
        else
        {
            /* Use ext_seq variable to specify that header
             * (authenticated data( will be copied */
            if ((sa_params_p->flags & SAB_FLAG_SUPPRESS_HEADER) == 0)
                token_context_internal_p->ext_seq = 1;
        }
        token_context_internal_p->hash_block_size = 64;
        token_context_internal_p->f_first = true;
        token_context_internal_p->msg_len = 0;

        switch(sa_params_p->auth_algo)
        {
        case SAB_AUTH_HASH_MD5:
            token_context_internal_p->icv_byte_count = 16;
            token_context_internal_p->digest_word_count = 16 + 4;
            break;
        case SAB_AUTH_HMAC_MD5:
            token_context_internal_p->icv_byte_count = 16;
            token_context_internal_p->digest_word_count = 4;
            f_is_hmac = true;
            break;
        case SAB_AUTH_HASH_SHA1:
            token_context_internal_p->icv_byte_count = 20;
            token_context_internal_p->digest_word_count = 16 + 5;
            break;
        case SAB_AUTH_HMAC_SHA1:
            token_context_internal_p->icv_byte_count = 20;
            token_context_internal_p->digest_word_count = 5;
            f_is_hmac = true;
            break;
        case SAB_AUTH_HASH_SHA2_224:
            token_context_internal_p->icv_byte_count = 32;
            token_context_internal_p->digest_word_count = 16 + 8;
            break;
        case SAB_AUTH_HMAC_SHA2_224:
            token_context_internal_p->icv_byte_count = 32;
            token_context_internal_p->digest_word_count = 8;
            f_is_hmac = true;
            break;
        case SAB_AUTH_HASH_SHA2_256:
        case SAB_AUTH_HASH_SM3:
            token_context_internal_p->icv_byte_count = 32;
            token_context_internal_p->digest_word_count = 16 + 8;
            break;
        case SAB_AUTH_HMAC_SHA2_256:
        case SAB_AUTH_HMAC_SM3:
            token_context_internal_p->icv_byte_count = 32;
            token_context_internal_p->digest_word_count = 8;
            f_is_hmac = true;
            break;
        case SAB_AUTH_HASH_SHA2_384:
            token_context_internal_p->icv_byte_count = 64;
            token_context_internal_p->digest_word_count = 16 + 16;
            token_context_internal_p->hash_block_size = 128;
            break;
        case SAB_AUTH_HMAC_SHA2_384:
            if ((sa_params_basic_p->basic_flags & SAB_BASIC_FLAG_ENCRYPT_AFTER_HASH) != 0)
                token_context_internal_p->icv_byte_count = 48;
            else
                token_context_internal_p->icv_byte_count = 64;
            token_context_internal_p->digest_word_count = 16;
            token_context_internal_p->hash_block_size = 128;
            f_is_hmac = true;
            break;
        case SAB_AUTH_HASH_SHA2_512:
            token_context_internal_p->icv_byte_count = 64;
            token_context_internal_p->digest_word_count = 16 + 16;
            token_context_internal_p->hash_block_size = 128;
            break;
        case SAB_AUTH_HMAC_SHA2_512:
            token_context_internal_p->icv_byte_count = 64;
            token_context_internal_p->digest_word_count = 16;
            token_context_internal_p->hash_block_size = 128;
            f_is_hmac = true;
            break;
        case SAB_AUTH_HASH_SHA3_224:
        case SAB_AUTH_HMAC_SHA3_224:
        case SAB_AUTH_KEYED_HASH_SHA3_224:
            token_context_internal_p->icv_byte_count = 28;
            if ((sa_params_p->flags & SAB_FLAG_HASH_INTERMEDIATE) != 0)
            {
                token_context_internal_p->digest_word_count = 50;
            }
            else
            {
                token_context_internal_p->digest_word_count = 7;
            }
            token_context_internal_p->hash_block_size = 144;
            break;
        case SAB_AUTH_HASH_SHA3_256:
        case SAB_AUTH_HMAC_SHA3_256:
        case SAB_AUTH_KEYED_HASH_SHA3_256:
            token_context_internal_p->icv_byte_count = 32;
            if ((sa_params_p->flags & SAB_FLAG_HASH_INTERMEDIATE) != 0)
            {
                token_context_internal_p->digest_word_count = 50;
            }
            else
            {
                token_context_internal_p->digest_word_count = 8;
            }
            token_context_internal_p->hash_block_size = 136;
            break;
        case SAB_AUTH_HASH_SHA3_384:
        case SAB_AUTH_HMAC_SHA3_384:
        case SAB_AUTH_KEYED_HASH_SHA3_384:
            token_context_internal_p->icv_byte_count = 48;
            if ((sa_params_p->flags & SAB_FLAG_HASH_INTERMEDIATE) != 0)
            {
                token_context_internal_p->digest_word_count = 50;
            }
            else
            {
                token_context_internal_p->digest_word_count = 12;
            }
            token_context_internal_p->hash_block_size = 104;
            break;
        case SAB_AUTH_HASH_SHA3_512:
        case SAB_AUTH_HMAC_SHA3_512:
        case SAB_AUTH_KEYED_HASH_SHA3_512:
            token_context_internal_p->icv_byte_count = 64;
            if ((sa_params_p->flags & SAB_FLAG_HASH_INTERMEDIATE) != 0)
            {
                token_context_internal_p->digest_word_count = 50;
            }
            else
            {
                token_context_internal_p->digest_word_count = 16;
            }
            token_context_internal_p->hash_block_size = 72;
            break;
        case SAB_AUTH_SHAKE_256:
        case SAB_AUTH_CSHAKE_256:
            token_context_internal_p->digest_word_count = 50;
            token_context_internal_p->hash_block_size = 136;
            token_context_internal_p->protocol = TKB_PROTO_BASIC_SHAKE;
            /* Set desired padding byte */
            token_context_internal_p->icv_byte_count = sa_params_p->auth_algo == SAB_AUTH_CSHAKE_256?0x4:0x1f;
            break;

        case SAB_AUTH_AES_XCBC_MAC:
        case SAB_AUTH_AES_CMAC_128:
        case SAB_AUTH_AES_CMAC_192:
        case SAB_AUTH_AES_CMAC_256:
            token_context_internal_p->icv_byte_count = 16;
            token_context_internal_p->digest_word_count = 4;
            break;
        case SAB_AUTH_AES_CCM:
            if (sa_params_p->direction == SAB_DIRECTION_OUTBOUND)
                token_context_internal_p->protocol = TKB_PROTO_BASIC_CCM_OUT;
            else
                token_context_internal_p->protocol = TKB_PROTO_BASIC_CCM_IN;
            token_context_internal_p->icv_byte_count = 16;
            token_context_internal_p->digest_word_count = 4;
            // Adjust byte count now as it influences the SALT value.
            if (sa_params_basic_p->icv_byte_count != 0 &&
                sa_params_basic_p->icv_byte_count <
                token_context_internal_p->icv_byte_count)
                token_context_internal_p->icv_byte_count =
                    sa_params_basic_p->icv_byte_count;
            if (sa_params_p->nonce_p == NULL)
            {
                LOG_CRIT("token_builder_build_context: NULL nonce pointer.\n");
                return TKB_INVALID_PARAMETER;
            }
            token_context_internal_p->u.generic.ccm_salt =
                (sa_params_p->nonce_p[0] << 8) |
                (sa_params_p->nonce_p[1] << 16) |
                (sa_params_p->nonce_p[2] << 24) |
                TKB_CCM_FLAG_ADATA_L4 |
                ((token_context_internal_p->icv_byte_count-2)*4);
            break;
        case SAB_AUTH_AES_GCM:
            if ((sa_params_p->flags & SAB_FLAG_HASH_INTERMEDIATE) != 0)
            {
                if (sa_params_p->direction == SAB_DIRECTION_OUTBOUND)
                    token_context_internal_p->protocol = TKB_PROTO_BASIC_GCM_OUT_CONT;
                else
                    token_context_internal_p->protocol = TKB_PROTO_BASIC_GCM_IN_CONT;
                token_context_internal_p->seq_offset = sa_params_p->offset_digest0;
            }
            else
            {
                if (sa_params_p->direction == SAB_DIRECTION_OUTBOUND)
                    token_context_internal_p->protocol = TKB_PROTO_BASIC_GCM_OUT;
                else
                    token_context_internal_p->protocol = TKB_PROTO_BASIC_GCM_IN;
            }
            token_context_internal_p->icv_byte_count = 16;
            token_context_internal_p->digest_word_count = 4;
            break;
        case SAB_AUTH_AES_GMAC:
            if (sa_params_p->direction == SAB_DIRECTION_OUTBOUND)
                token_context_internal_p->protocol = TKB_PROTO_BASIC_GMAC_OUT;
            else
                token_context_internal_p->protocol = TKB_PROTO_BASIC_GMAC_IN;
            token_context_internal_p->icv_byte_count = 16;
            token_context_internal_p->digest_word_count = 4;
            break;
#ifdef TKB_ENABLE_CRYPTO_WIRELESS
        case SAB_AUTH_KASUMI_F9:
            token_context_internal_p->protocol = TKB_PROTO_BASIC_KASUMI_HASH;
            token_context_internal_p->icv_byte_count = 4;
            token_context_internal_p->digest_word_count = 4;
            token_context_internal_p->u.generic.ccm_salt =
                sa_params_basic_p->fresh;
            break;
        case SAB_AUTH_SNOW_UIA2:
            token_context_internal_p->protocol = TKB_PROTO_BASIC_SNOW_HASH;
            token_context_internal_p->icv_byte_count = 4;
            token_context_internal_p->digest_word_count = 4;
            token_context_internal_p->u.generic.ccm_salt =
                sa_params_basic_p->fresh;
            break;
        case SAB_AUTH_ZUC_EIA3:
            token_context_internal_p->protocol = TKB_PROTO_BASIC_ZUC_HASH;
            token_context_internal_p->icv_byte_count = 4;
            token_context_internal_p->digest_word_count = 4;
            token_context_internal_p->u.generic.ccm_salt =
                (sa_params_basic_p->bearer << 3);
            break;
        case SAB_AUTH_AES_EIA2:
            token_context_internal_p->protocol = TKB_PROTO_BASIC_AES_EIA2;
            token_context_internal_p->icv_byte_count = 4;
            token_context_internal_p->digest_word_count = 4;
            token_context_internal_p->u.generic.ccm_salt =
                (sa_params_basic_p->bearer << 3) |
                (sa_params_basic_p->direction << 2);
            break;
#endif
#ifdef TKB_ENABLE_CRYPTO_CHACHAPOLY
        case SAB_AUTH_KEYED_HASH_POLY1305:
        case SAB_AUTH_POLY1305:
            token_context_internal_p->icv_byte_count = 16;
            token_context_internal_p->digest_word_count = 4;
            break;
#endif
        default:
            LOG_CRIT("token_builder_build_context:"
                     " unsupported authentication algorithm.\n");
            return TKB_INVALID_PARAMETER;
        }
        if (sa_params_basic_p->icv_byte_count != 0 &&
            sa_params_basic_p->icv_byte_count <
            token_context_internal_p->icv_byte_count)
            token_context_internal_p->icv_byte_count =
                sa_params_basic_p->icv_byte_count;
        if ((sa_params_basic_p->basic_flags & SAB_BASIC_FLAG_EXTRACT_ICV) != 0 ||
            ((sa_params_p->auth_algo == SAB_AUTH_AES_CCM ||
                 sa_params_p->auth_algo == SAB_AUTH_AES_GCM ||
              sa_params_p->auth_algo == SAB_AUTH_AES_GMAC) &&
             token_context_internal_p->seq_offset == 0) ||
            (sa_params_p->auth_algo == SAB_AUTH_AES_GCM &&
             (sa_params_p->flags & SAB_FLAG_HASH_INTERMEDIATE) != 0 &&
             sa_params_p->direction == SAB_DIRECTION_INBOUND))
            token_context_internal_p->anti_replay =
                token_context_internal_p->icv_byte_count;
        if (f_is_hmac && (sa_params_p->flags & SAB_FLAG_HASH_INTERMEDIATE) != 0)
            token_context_internal_p->digest_word_count += 32;

        if (f_is_hmac) token_context_internal_p->f_first = false;
    }
    return TKB_STATUS_OK;
}
#endif


#ifdef TKB_ENABLE_PROTO_SRTP
/*----------------------------------------------------------------------------
 * token_builder_build_context_srtp
 *
 * Create a Token Context Record for SRTP
 *
 * sa_params_p (input)
 *   Points to the SA parameters data structure. It is important that
 *   SABuilder_GetSIze() and sa_builder_build_sa() have already been called
 *   for this SA.
 * token_context_internal_p (output)
 *   Buffer to hold the Token Context Record.
 *
 * Return:
 * TKB_STATUS_OK if successful.
 * TKB_INVALID_PARAMETER if any of the pointer parameters is NULL or if
 *                      the contents of the SA parameters are invalid.
 */
static token_builder_status_t
token_builder_build_context_srtp(
    const sa_builder_params_t * const sa_params_p,
    token_builder_context_t * const token_context_internal_p)
{
    // Note: SRTP may not make use of the Token Context fields
    // cw0, cw1 CCMAalt and update_handling as they occupy the same
    // space as the SaltKey fields.
    sa_builder_params_srtp_t *sa_params_srtp_p;
    sa_params_srtp_p = (sa_builder_params_srtp_t *)
        (sa_params_p->protocol_extension_p);
    if (sa_params_srtp_p == NULL)
    {
        LOG_CRIT("token_builder_build_context:"
                 " SRTP extension pointer is NULL\n");
        return TKB_INVALID_PARAMETER;
    }

    if (sa_params_p->direction == SAB_DIRECTION_OUTBOUND)
        token_context_internal_p->protocol = TKB_PROTO_SRTP_OUT;
    else
        token_context_internal_p->protocol = TKB_PROTO_SRTP_IN;

    token_context_internal_p->icv_byte_count = sa_params_srtp_p->icv_byte_count;

    if (sa_params_p->crypto_algo != SAB_CRYPTO_NULL)
    {
        token_context_internal_p->iv_handling = TKB_IV_OUTBOUND_TOKEN_SRTP;

        token_context_internal_p->iv_byte_count = 16;
        token_context_internal_p->token_header_word |=
            TKB_HEADER_IV_TOKEN_4WORDS;
        if (sa_params_p->nonce_p == NULL)
        {
            LOG_CRIT("token_builder_build_context: NULL nonce pointer.\n");
            return TKB_INVALID_PARAMETER;
        }

        token_context_internal_p->u.srtp.salt_key0 =
            (sa_params_p->nonce_p[0]) |
            (sa_params_p->nonce_p[1] << 8) |
            (sa_params_p->nonce_p[2] << 16) |
            (sa_params_p->nonce_p[3] << 24);
        token_context_internal_p->u.srtp.salt_key1 =
            (sa_params_p->nonce_p[4]) |
            (sa_params_p->nonce_p[5] << 8) |
            (sa_params_p->nonce_p[6] << 16) |
            (sa_params_p->nonce_p[7] << 24);
        token_context_internal_p->u.srtp.salt_key2 =
            (sa_params_p->nonce_p[8]) |
            (sa_params_p->nonce_p[9] << 8) |
            (sa_params_p->nonce_p[10] << 16) |
            (sa_params_p->nonce_p[11] << 24);
        token_context_internal_p->u.srtp.salt_key3 =
            (sa_params_p->nonce_p[12]) |
            (sa_params_p->nonce_p[13] << 8);
    }
    else
    {
        token_context_internal_p->iv_handling = TKB_IV_OUTBOUND_CBC;
        token_context_internal_p->iv_byte_count = 0;
    }

    if ((sa_params_srtp_p->srtp_flags & SAB_SRTP_FLAG_SRTCP) != 0)
    {
        token_context_internal_p->ext_seq = 4;
    }
    else
    {
        token_context_internal_p->ext_seq = 0;
    }

    if ((sa_params_srtp_p->srtp_flags & SAB_SRTP_FLAG_INCLUDE_MKI) != 0)
    {
        token_context_internal_p->anti_replay = 4;
    }
    else
    {
        token_context_internal_p->anti_replay = 0;
    }
    return TKB_STATUS_OK;
}
#endif


/*----------------------------------------------------------------------------
 * token_builder_build_context
 *
 * Create a Token Context Record.
 *
 * sa_params_p (input)
 *   Points to the SA parameters data structure. It is important that
 *   SABuilder_GetSIze() and sa_builder_build_sa() have already been called
 *   for this SA.
 * token_context_p (output)
 *   Buffer to hold the Token Context Record. It must point to a valid
 *   storage buffer that is large enough to hold the Token
 *   Context. Before calling this function, the application must call
 *   TokeBuilder_GetContextSize() with the same SA parameters and
 *   allocate a buffer of that size.
 *
 * Return:
 * TKB_STATUS_OK if successful.
 * TKB_INVALID_PARAMETER if any of the pointer parameters is NULL or if
 *                      the contents of the SA parameters are invalid.
 */
token_builder_status_t
token_builder_build_context(
    const sa_builder_params_t * const sa_params_p,
    void * const token_context_p
    )
{
    token_builder_context_t *token_context_internal_p =
        (token_builder_context_t *)token_context_p;
    token_builder_status_t rc;
#ifdef TKB_STRICT_ARGS_CHECK
    if (sa_params_p == NULL || token_context_p == NULL)
    {
        LOG_CRIT("token_builder_build_context: NULL pointer supplied\n");
        return TKB_INVALID_PARAMETER;
    }
#endif

    token_context_internal_p->token_header_word = TKB_HEADER_DEFAULT;

    token_context_internal_p->u.generic.cw0 = sa_params_p->cw0;
    token_context_internal_p->u.generic.cw1 = sa_params_p->cw1;
    token_context_internal_p->hproto = TKB_HDR_BYPASS;
    switch (sa_params_p->protocol)
    {
#ifdef TKB_ENABLE_PROTO_IPSEC
    case SAB_PROTO_IPSEC:
        rc = token_builder_build_context_ipsec(sa_params_p,
                                             token_context_internal_p);
    break;
#endif  /* TKB_ENABLE_PROTO_IPSEC */
#ifdef TKB_ENABLE_PROTO_SSLTLS
    case SAB_PROTO_SSLTLS:
        rc = token_builder_build_context_ssltls(sa_params_p,
                                             token_context_internal_p);
    break;
#endif
#ifdef TKB_ENABLE_PROTO_BASIC
    case SAB_PROTO_BASIC:
        rc = token_builder_build_context_basic(sa_params_p,
                                             token_context_internal_p);
    break;
#endif
#ifdef TKB_ENABLE_PROTO_SRTP
    case SAB_PROTO_SRTP:
        rc = token_builder_build_context_srtp(sa_params_p,
                                             token_context_internal_p);
    break;
#endif
    default:
        LOG_CRIT("token_builder_build_context: unsupported protocol.\n");
        return TKB_INVALID_PARAMETER;
    }

    if (rc != TKB_STATUS_OK)
    {
        return rc;
    }

    if (token_context_internal_p->protocol != TKB_PROTO_BASIC_HMAC_PRECOMPUTE &&
        sa_params_p->offset_digest1 != 0 &&
        sa_params_p->auth_key2_p == NULL)
    {
        /* HMAC key desired, but not supplied, convert context temporarily
           to context prepare context. After the first token, the Token
           Builder will switch it back */
        token_context_internal_p->protocol_next =
            token_context_internal_p->protocol;
        token_context_internal_p->hproto_next =
            token_context_internal_p->hproto;
        token_context_internal_p->iv_handling_next =
            token_context_internal_p->iv_handling;
        token_context_internal_p->digest_word_count_next =
            token_context_internal_p->digest_word_count;
        token_context_internal_p->header_word_fields_next =
            (token_context_internal_p->token_header_word >> 22) & MASK_8_BITS;
        token_context_internal_p->protocol = TKB_PROTO_BASIC_HMAC_CTXPREPARE;
        token_context_internal_p->hproto = TKB_HDR_BYPASS;
        token_context_internal_p->iv_handling = TKB_IV_INBOUND_CTR;
        /* Clear fields in Token Header word, to prevent iv from token etc.*/
        token_context_internal_p->token_header_word &= 0xc03fffff;
        token_context_internal_p->digest_word_count =
            sa_params_p->offset_digest1 - sa_params_p->offset_digest0;
        token_context_internal_p->digest_offset = sa_params_p->offset_digest0;
    }

    LOG_INFO("TokenBuilderBuildContext: created context, header word=%08x\n"
             "  proto=%d hproto=%d padalign=%d ivsize=%d icvsize=%d\n"
             "  seqoffset=%d extseq=%d antireplay=%d ivhandling=%d\n",
             token_context_internal_p->token_header_word,
             token_context_internal_p->protocol,
             token_context_internal_p->hproto,
             token_context_internal_p->pad_block_byte_count,
             token_context_internal_p->iv_byte_count,
             token_context_internal_p->icv_byte_count,
             token_context_internal_p->seq_offset,
             token_context_internal_p->ext_seq,
             token_context_internal_p->anti_replay,
             token_context_internal_p->iv_handling);

    return TKB_STATUS_OK;
}




/* end of file token_builder_context.c */
