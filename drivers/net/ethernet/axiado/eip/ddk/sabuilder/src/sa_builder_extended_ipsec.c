/* sa_builder_extended_ipsec.c
 *
 * IPsec specific functions (for initialization of sa_builder_params_t
 * structures and for building the IPSec specifc part of an SA.) in the
 * Extended use case.
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
#include "c_sa_builder.h"

#ifdef SAB_ENABLE_IPSEC_EXTENDED
#include "sa_builder_extended_internal.h"

/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */
#include "basic_defs.h"
#include "log.h"
#include "sa_builder_internal.h" /* SABuilder_SetIpsecParams */
#include "sa_builder_ipsec.h"

/*----------------------------------------------------------------------------
 * Definitions and macros
 */
#define ESP_HDR_LEN 8
#define IPV4_HDR_LEN 20
#define IPV6_HDR_LEN 40

/*----------------------------------------------------------------------------
 * Local variables
 */


#ifdef SAB_ENABLE_EXTENDED_TUNNEL_HEADER
/*----------------------------------------------------------------------------
 * get16
 *
 * Read 16-bit value from byte array not changing the byte order.
 */
static u16
get16no(
        u8 *p,
        unsigned int offs)
{
    return (p[offs+1]<<8) | p[offs];
}
#endif

/*----------------------------------------------------------------------------
 * sa_builder_set_extended_ipsec_params
 *
 * Fill in IPsec-specific extensions into the SA.for Extended.
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
sa_builder_set_extended_ipsec_params(sa_builder_params_t *const sa_params_p,
                         sa_builder_state_t * const sa_state_p,
                         u32 * const sa_buffer_p)
{
    sa_builder_params_ipsec_t *sa_params_ipsec_p =
        (sa_builder_params_ipsec_t *)(sa_params_p->protocol_extension_p);
    u32 token_header_word = SAB_HEADER_DEFAULT;
    sa_builder_esp_protocol_t esp_proto;
    sa_builder_header_protocol_t header_proto;
    u8 pad_block_byte_count;
    u8 iv_byte_count;
    u8 icv_byte_count;
    u8 seq_offset;
    u8 ext_seq = 0;
    u8 anti_replay;
    u32 ccm_salt = 0;
    u32 flags = 0;
    u32 verify_instruction_word, ctx_instruction_word;
#ifdef SAB_ENABLE_EXTENDED_TUNNEL_HEADER
    u32 mtu_discount = 0;
    u32 check_sum = 0;
#endif
    IDENTIFIER_NOT_USED(sa_state_p);

    if (sa_params_ipsec_p == NULL)
    {
        LOG_CRIT("SABuilder: IPsec extension pointer is null\n");
        return SAB_INVALID_PARAMETER;
    }

    if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_ESP) == 0)
    {
        LOG_CRIT("SABuilder: IPsec only supports ESP.\n");
        return SAB_INVALID_PARAMETER;
    }

    if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_XFRM_API) != 0)
    {
        if(sa_params_p->crypto_mode != SAB_CRYPTO_MODE_CBC &&
           sa_params_p->crypto_mode != SAB_CRYPTO_MODE_GCM)
        {
            LOG_CRIT("SABuilder: IPsec for XFRM only supports CBC and GCM modes.\n");
            return SAB_INVALID_PARAMETER;
        }
        if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_NATT) != 0)
        {
            LOG_CRIT("SABuilder: IPsec for XFRM does not support NATT\n");
            return SAB_INVALID_PARAMETER;
        }

    }
    if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_NO_ANTI_REPLAY) != 0)
        anti_replay = 0;
    else
        anti_replay = 1;

    if (sa_params_p->direction == SAB_DIRECTION_OUTBOUND)
    {
        if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_XFRM_API) != 0)
            esp_proto = SAB_ESP_PROTO_OUT_XFRM_CBC;
        else
            esp_proto = SAB_ESP_PROTO_OUT_CBC;
        pad_block_byte_count = 4;
        if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_IPV6) !=0)
        {
            if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_XFRM_API) != 0)
            {
                    header_proto = SAB_HDR_IPV6_OUT_XFRM;
            }
            else if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_PROCESS_IP_HEADERS) !=0)
            {
                if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_TUNNEL) !=0)
                {
                    header_proto = SAB_HDR_IPV6_OUT_TUNNEL;
                }
                else
                {
                    header_proto = SAB_HDR_IPV6_OUT_TRANSP;
                }
            }
            else
            {
                header_proto = SAB_HDR_IPV6_OUT_TRANSP_HDRBYPASS;
            }
        }
        else
        {
            if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_XFRM_API) != 0)
            {
                    header_proto = SAB_HDR_IPV4_OUT_XFRM;
            }
            else if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_PROCESS_IP_HEADERS) !=0)
            {
                if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_TUNNEL) !=0)
                {
                    header_proto = SAB_HDR_IPV4_OUT_TUNNEL;
                }
                else
                {
                    header_proto = SAB_HDR_IPV4_OUT_TRANSP;
                }
            }
            else
            {
                header_proto = SAB_HDR_IPV4_OUT_TRANSP_HDRBYPASS;
            }
        }

        if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_LONG_SEQ) != 0)
            ext_seq = 1;
    }
    else
    {
        if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_XFRM_API) != 0)
            esp_proto = SAB_ESP_PROTO_IN_XFRM_CBC;
        else
            esp_proto = SAB_ESP_PROTO_IN_CBC;
        pad_block_byte_count = 4;
        token_header_word |= SAB_HEADER_PAD_VERIFY;

        if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_IPV6) !=0)
        {
            if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_XFRM_API) != 0)
            {
                    header_proto = SAB_HDR_IPV6_IN_XFRM;
            }
            else if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_PROCESS_IP_HEADERS) !=0)
            {
                if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_TUNNEL) !=0)
                {
                    header_proto = SAB_HDR_IPV6_IN_TUNNEL;
                }
                else
                {
                    header_proto = SAB_HDR_IPV6_IN_TRANSP;
                    token_header_word |= SAB_HEADER_UPD_HDR;
                }
            }
            else
            {
                header_proto = SAB_HDR_IPV6_IN_TRANSP_HDRBYPASS;
            }
        }
        else
        {
            if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_XFRM_API) != 0)
            {
                    header_proto = SAB_HDR_IPV4_IN_XFRM;
            }
            else if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_PROCESS_IP_HEADERS) !=0)
            {
                if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_TUNNEL) !=0)
                {
                    header_proto = SAB_HDR_IPV4_IN_TUNNEL;
                }
                else
                {
                    header_proto = SAB_HDR_IPV4_IN_TRANSP;
                    token_header_word |= SAB_HEADER_UPD_HDR;
                }
            }
            else
            {
                header_proto = SAB_HDR_IPV4_IN_TRANSP_HDRBYPASS;
            }
        }

        if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_LONG_SEQ) != 0)
            ext_seq = 1;
        anti_replay *= sa_params_ipsec_p->sequence_mask_bit_count / 32;
    }
    seq_offset = sa_params_p->offset_seq_num;

    switch (sa_params_p->crypto_algo)
    {
    case SAB_CRYPTO_NULL:
        iv_byte_count = 0;
                break;
    case SAB_CRYPTO_DES:
    case SAB_CRYPTO_3DES:
        iv_byte_count = 8;
        pad_block_byte_count = 8;
        break;
    case SAB_CRYPTO_AES:
    case SAB_CRYPTO_SM4:
    case SAB_CRYPTO_BC0:
        if (sa_params_p->crypto_mode == SAB_CRYPTO_MODE_CBC)
        {
            iv_byte_count = 16;
            pad_block_byte_count = 16;
        }
        else
        {
            if (sa_params_p->direction == SAB_DIRECTION_OUTBOUND)
                esp_proto = SAB_ESP_PROTO_OUT_CTR;
            else
                esp_proto = SAB_ESP_PROTO_IN_CTR;

            if (sa_params_p->iv_src == SAB_IV_SRC_IMPLICIT)
                iv_byte_count = 0;
            else
                iv_byte_count = 8;
        }
        break;
    case SAB_CRYPTO_CHACHA20:
        if (sa_params_p->direction == SAB_DIRECTION_OUTBOUND)
            esp_proto = SAB_ESP_PROTO_OUT_CHACHAPOLY;
        else
            esp_proto = SAB_ESP_PROTO_IN_CHACHAPOLY;

        if (sa_params_p->iv_src == SAB_IV_SRC_IMPLICIT)
            iv_byte_count = 0;
        else
            iv_byte_count = 8;
        break;
    default:
            LOG_CRIT("sa_builder_build_sa:"
                     "Unsupported Crypto algorithm\n");
            return SAB_INVALID_PARAMETER;
        ;
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
        case SAB_IV_SRC_PRNG:
            token_header_word |=
                SAB_HEADER_IV_PRNG;
            break;
        case SAB_IV_SRC_DEFAULT:
        case SAB_IV_SRC_SA: /* No action required */
        case SAB_IV_SRC_TOKEN:
            break;
        default:
            LOG_CRIT("sa_builder_build_sa:"
                     "Unsupported iv source\n");
            return SAB_INVALID_PARAMETER;
        }
    }

    if (sa_params_p->direction == SAB_DIRECTION_OUTBOUND &&
        sa_params_ipsec_p->pad_alignment >
        pad_block_byte_count &&
        sa_params_ipsec_p->pad_alignment <= 256)
        pad_block_byte_count =
            sa_params_ipsec_p->pad_alignment;

    switch(sa_params_p->auth_algo)
    {
    case SAB_AUTH_NULL:
        icv_byte_count = 0;
        ext_seq = 0;
        if (sa_params_p->direction == SAB_DIRECTION_OUTBOUND)
            esp_proto = SAB_ESP_PROTO_OUT_NULLAUTH;
        else
            esp_proto = SAB_ESP_PROTO_IN_NULLAUTH;

        break;
            case SAB_AUTH_HMAC_MD5:
    case SAB_AUTH_HMAC_SHA1:
    case SAB_AUTH_AES_XCBC_MAC:
    case SAB_AUTH_AES_CMAC_128:
        icv_byte_count = 12;
        break;
    case SAB_AUTH_HMAC_SHA2_224:
    case SAB_AUTH_HMAC_SHA2_256:
#ifdef CONFIG_EIP_IPSEC_OFFLOAD
	// All these protocols have a selectable ICV length.
	if (sa_params_ipsec_p->icv_byte_count == 12 ||
	    sa_params_ipsec_p->icv_byte_count == 16 ||
	    sa_params_ipsec_p->icv_byte_count == 32) {
	    icv_byte_count = sa_params_ipsec_p->icv_byte_count;
	} else {
	    icv_byte_count = 16;
	}
	break;
#endif
    case SAB_AUTH_HMAC_SM3:
        icv_byte_count = 16;
        break;
    case SAB_AUTH_HMAC_SHA2_384:
#ifdef CONFIG_EIP_IPSEC_OFFLOAD
	// All these protocols have a selectable ICV length.
	if (sa_params_ipsec_p->icv_byte_count == 24 ||
	    sa_params_ipsec_p->icv_byte_count == 48) {
	    icv_byte_count = sa_params_ipsec_p->icv_byte_count;
	} else {
	    icv_byte_count = 24;
	}
#else
	icv_byte_count = 24;
#endif
	break;
    case SAB_AUTH_HMAC_SHA2_512:
#ifdef CONFIG_EIP_IPSEC_OFFLOAD
	// All these protocols have a selectable ICV length.
	if (sa_params_ipsec_p->icv_byte_count == 32 ||
	    sa_params_ipsec_p->icv_byte_count == 64) {
	    icv_byte_count = sa_params_ipsec_p->icv_byte_count;
	} else {
	    icv_byte_count = 32;
	}
#else
	icv_byte_count = 32;
#endif
	break;
    case SAB_AUTH_AES_CCM:
    case SAB_AUTH_AES_GCM:
    case SAB_AUTH_AES_GMAC:
        // All these protocols have a selectable ICV length.
        if (sa_params_ipsec_p->icv_byte_count == 8 ||
            sa_params_ipsec_p->icv_byte_count == 12 ||
            sa_params_ipsec_p->icv_byte_count == 16)
        {
            icv_byte_count =
                        sa_params_ipsec_p->icv_byte_count;
        }
        else
        {
            icv_byte_count = 16;
        }
        switch (sa_params_p->auth_algo)
        {
            /* These protocols need specialized protocol codes
               for the token generator.*/
        case SAB_AUTH_AES_CCM:
            if (sa_params_p->direction == SAB_DIRECTION_OUTBOUND)
                esp_proto = SAB_ESP_PROTO_OUT_CCM;
            else
                esp_proto = SAB_ESP_PROTO_IN_CCM;

            ccm_salt =
                (sa_params_p->nonce_p[0] << 8) |
                (sa_params_p->nonce_p[1] << 16) |
                (sa_params_p->nonce_p[2] << 24) |
                SAB_CCM_FLAG_ADATA_L4 |
                ((icv_byte_count-2)*4);
            break;
        case SAB_AUTH_AES_GCM:
            if (sa_params_p->direction == SAB_DIRECTION_OUTBOUND)
            {
                if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_XFRM_API) != 0)
                    esp_proto = SAB_ESP_PROTO_OUT_XFRM_GCM;
                else
                    esp_proto = SAB_ESP_PROTO_OUT_GCM;
            }
            else
            {
                if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_XFRM_API) != 0)
                    esp_proto = SAB_ESP_PROTO_IN_XFRM_GCM;
                else
                    esp_proto = SAB_ESP_PROTO_IN_GCM;
            }
            break;
        case SAB_AUTH_AES_GMAC:
            if (sa_params_p->direction == SAB_DIRECTION_OUTBOUND)
                        esp_proto = SAB_ESP_PROTO_OUT_GMAC;
            else
                esp_proto = SAB_ESP_PROTO_IN_GMAC;
            break;
        default:
            ;
        }
        break;
    case SAB_AUTH_POLY1305:
        icv_byte_count = 16;
        break;
    default:
        LOG_CRIT("sa_builder_build_sa: unsupported authentication algorithm\n");
        return SAB_UNSUPPORTED_FEATURE;
    }


    /* flags variable */
    if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_IPV6) !=0)
        flags |= BIT_8;
    if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_PROCESS_IP_HEADERS) !=0)
        flags |= BIT_19;
    if (ext_seq !=0)
        flags |= BIT_29;
    if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_DEC_TTL) != 0)
        flags |= BIT_27;
    if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_CLEAR_DF) != 0)
        flags |= BIT_20;
    if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_SET_DF) != 0)
        flags |= BIT_21;
    if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_REPLACE_DSCP) != 0)
        flags |= BIT_22;
    if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_CLEAR_ECN) != 0)
        flags |= BIT_23;
    if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_APPEND_SEQNUM) != 0)
    {
        if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_LONG_SEQ) != 0)
            flags |= BIT_25;
        else
            flags |= BIT_24;
    }
    if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_TRANSPORT_NAT) != 0)
    {
        if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_TUNNEL) != 0)
        {
            LOG_CRIT("NAT only for transport\n");
            return SAB_INVALID_PARAMETER;
        }
        if (sa_params_p->direction==SAB_DIRECTION_INBOUND &&
            sa_params_ipsec_p->sequence_mask_bit_count > 128)
        {
            if (sa_state_p->f_large && large_transform_offset == 16)
            {
                LOG_CRIT(
                    "sa_builder_build_sa: Inbound NAT cannot be combined with \n"
                    " anti-replay mask > 128\n and HMAC-SHA384/512\n");
                return SAB_UNSUPPORTED_FEATURE;
            }
            else if (sa_params_p->offset_seq_num == SAB_SEQNUM_HI_FIX_OFFSET &&
                     sa_params_ipsec_p->sequence_mask_bit_count > 384)
            {
                LOG_CRIT(
                    "sa_builder_build_sa: Inbound NAT cannot be combined with \n"
                    " anti-replay mask > 384\n and HMAC-SHA384/512\n");
                return SAB_UNSUPPORTED_FEATURE;
            }
            else
            {
                sa_state_p->f_large = true;
            }
        }
        flags |= BIT_28;
    }

    /* Take care of the VERIFY and CTX token instructions */
    if (sa_params_p->direction == SAB_DIRECTION_OUTBOUND)
    {
        verify_instruction_word = SAB_VERIFY_NONE;
#ifdef SAB_ENABLE_TWO_FIXED_RECORD_SIZES
        if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_XFRM_API) != 0)
        {
            if (sa_state_p->f_large)
            {
                ctx_instruction_word = SAB_CTX_NONE + FIRMWARE_EIP207_CS_FLOW_TRC_RECORD_WORD_COUNT + large_transform_offset - 1;
            }
            else
#endif
            {
                ctx_instruction_word = SAB_CTX_NONE + FIRMWARE_EIP207_CS_FLOW_TRC_RECORD_WORD_COUNT - 1;
            }
        }
        else
        {
            ctx_instruction_word = SAB_CTX_OUT_SEQNUM +
                ((unsigned int)(ext_seq+1)<<24) + seq_offset;
        }
    }
    else
    {
        verify_instruction_word = SAB_VERIFY_PADSPI;
        if (icv_byte_count > 0)
        {
            verify_instruction_word += SAB_VERIFY_BIT_H + icv_byte_count;
        }
        if (anti_replay > 0 &&
            (sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_APPEND_SEQNUM) == 0 &&
            (sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_XFRM_API) == 0)
        {
            /* Skip verification of sequence number in sequence number append
               mode. */
            verify_instruction_word += SAB_VERIFY_BIT_SEQ;
        }
        if (icv_byte_count == 0 || anti_replay == 0 ||
            (sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_XFRM_API) != 0)
        {
#ifdef SAB_ENABLE_TWO_FIXED_RECORD_SIZES
            if (sa_state_p->f_large)
            {
                ctx_instruction_word = SAB_CTX_NONE + FIRMWARE_EIP207_CS_FLOW_TRC_RECORD_WORD_COUNT + large_transform_offset - 1;
            }
            else
#endif
            {
                ctx_instruction_word = SAB_CTX_NONE + FIRMWARE_EIP207_CS_FLOW_TRC_RECORD_WORD_COUNT - 1;
            }
        }
        else if (ext_seq != 0 ||
                 (anti_replay != 0 &&
                  sa_params_p->offset_seq_num + 2 == sa_params_p->offset_seq_mask))
        {
            if (anti_replay > 12)
                ctx_instruction_word = SAB_CTX_SEQNUM +
                     + seq_offset;
            else
                ctx_instruction_word = SAB_CTX_SEQNUM +
                    ((unsigned int)(2+anti_replay)<<24) + seq_offset;
        }
        else
        {
            ctx_instruction_word = SAB_CTX_INSEQNUM +
                ((unsigned int)(1+anti_replay)<<24) + seq_offset;
        }
    }

#ifdef SAB_ENABLE_EXTENDED_TUNNEL_HEADER
    /* Compute the maximum amount by which the packet can be enlarged,
       so discount that from the output MTU to judge whether a packet can
       be processed without fragmentation. */
    if (sa_params_p->direction == SAB_DIRECTION_OUTBOUND)
    {
        mtu_discount = ESP_HDR_LEN + 1 + pad_block_byte_count +
            iv_byte_count + icv_byte_count;

        if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_TUNNEL) !=0)
        {
            if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_IPV4) !=0)
                mtu_discount += IPV4_HDR_LEN;
            else
                mtu_discount += IPV6_HDR_LEN;

            // for IPv4 tunnel, pre-calculate checksum on IP addresses and store them in the transform record
            // this checksum does not include the final inversion and is performed on data
            // as they stored in the memory
            if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_IPV4) !=0)
            {
                // protection against NULL pointers
                if ((sa_params_ipsec_p->src_ip_addr_p != NULL)&&
                    (sa_params_ipsec_p->dest_ip_addr_p != NULL))
                {
                    // add the addresses (in order they are stored in the memory)
                    check_sum += get16no(sa_params_ipsec_p->src_ip_addr_p, 0);
                    check_sum += get16no(sa_params_ipsec_p->src_ip_addr_p, 2);
                    check_sum += get16no(sa_params_ipsec_p->dest_ip_addr_p, 0);
                    check_sum += get16no(sa_params_ipsec_p->dest_ip_addr_p, 2);

                    // process the carries
                    while ((check_sum>>16) != 0)
                        check_sum = (check_sum>>16) + (check_sum & 0xffff);
                }
            }
        }
    }
    /* Compute the checksum delta for internal NAT operations and for inbound
       transport NAT-T checksum fixup */
    if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_TUNNEL) == 0 &&
        (sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_CHECKSUM_FIX) != 0)
    {
        u8 ip_len = sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_IPV4?4:16;
        unsigned int i;
        // Compute source address delta only if both original and new source
        // addresses are provided, otherwise assume source address is unchanged.
        if (sa_params_ipsec_p->src_ip_addr_p != NULL &&
            sa_params_ipsec_p->orig_src_ip_addr_p != NULL)
        {
            for (i=0; i<ip_len; i+=2)
            {
                check_sum += get16no(sa_params_ipsec_p->src_ip_addr_p, i);
                check_sum += get16no(sa_params_ipsec_p->orig_src_ip_addr_p, i) ^ 0xffff;
            }
        }
        // Compute destination address delta only if both original and
        // new destination addresses are provided, otherwise assume
        // destination address is unchanged.
        if (sa_params_ipsec_p->dest_ip_addr_p != NULL &&
            sa_params_ipsec_p->orig_dest_ip_addr_p != NULL)
        {
            for (i=0; i<ip_len; i+=2)
            {
                check_sum += get16no(sa_params_ipsec_p->dest_ip_addr_p, i);
                check_sum += get16no(sa_params_ipsec_p->orig_dest_ip_addr_p, i) ^ 0xffff;
            }
        }
        // process the carries
        while ((check_sum>>16) != 0)
            check_sum = (check_sum>>16) + (check_sum & 0xffff);
    }

#endif

    /* If NAT-T selected, select other header protocol range */
    if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_NATT) != 0)
        header_proto += (SAB_HDR_IPV4_OUT_TRANSP_HDRBYPASS_NATT -
                        SAB_HDR_IPV4_OUT_TRANSP_HDRBYPASS);

    /* Write all parameters to their respective offsets */
    if (sa_buffer_p != NULL)
    {
#ifdef SAB_ENABLE_TWO_FIXED_RECORD_SIZES
        if (sa_state_p->f_large)
        {
            sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_FLAGS_WORD_OFFSET +
                       large_transform_offset] = flags;
            sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_HDRPROC_CTX_WORD_OFFSET +
                       large_transform_offset] =
                sa_params_ipsec_p->context_ref;
            sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_BYTE_PARAM_WORD_OFFSET +
                       large_transform_offset] =
                SAB_PACKBYTES(iv_byte_count,icv_byte_count,header_proto,esp_proto);
            sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_TK_HDR_WORD_OFFSET +
                       large_transform_offset] = token_header_word;
            if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_CHECKSUM_FIX) != 0 &&
                (sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_TUNNEL) == 0)
                sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_PAD_ALIGN_WORD_OFFSET +
                    large_transform_offset] =
                    SAB_PACKBYTES(pad_block_byte_count/2,
                                  0,
                                  check_sum & 0xff,
                                  check_sum >> 8);
            else if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_TUNNEL) != 0)
                sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_PAD_ALIGN_WORD_OFFSET +
                           large_transform_offset] =
                    SAB_PACKBYTES(pad_block_byte_count/2,
                                  0,
                                  sa_params_ipsec_p->ttl,
                                  sa_params_ipsec_p->dscp);
            else
                sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_PAD_ALIGN_WORD_OFFSET
                           + large_transform_offset] =
                    SAB_PACKBYTES(pad_block_byte_count/2,
                                  0,
                                  0,
                                  0);

            sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_CCM_SALT_WORD_OFFSET +
                       large_transform_offset] = ccm_salt;
            sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_TK_VFY_INST_WORD_OFFSET +
                       large_transform_offset] =
                verify_instruction_word;
            sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_TK_CTX_INST_WORD_OFFSET +
                       large_transform_offset] =
                ctx_instruction_word;
            sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_TIME_STAMP_LO_WORD_OFFSET +
                       large_transform_offset] = 0;
            sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_TIME_STAMP_HI_WORD_OFFSET +
                       large_transform_offset] = 0;
            sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_STAT_OCT_LO_WORD_OFFSET +
                       large_transform_offset] = 0;
            sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_STAT_OCT_HI_WORD_OFFSET +
                       large_transform_offset] = 0;
            sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_STAT_PKT_WORD_OFFSET +
                       large_transform_offset] = 0;
#ifdef SAB_ENABLE_EXTENDED_TUNNEL_HEADER
            sa_buffer_p[FIMRWARE_EIP207_CS_FLOW_TR_NATT_PORTS_WORD_OFFSET +
                       large_transform_offset] =
                SAB_PACKBYTES(sa_params_ipsec_p->natt_src_port >> 8,
                              sa_params_ipsec_p->natt_src_port & 0xff,
                              sa_params_ipsec_p->natt_dest_port >> 8,
                              sa_params_ipsec_p->natt_dest_port & 0xff);

            if (header_proto == SAB_HDR_IPV4_OUT_TUNNEL ||
                header_proto == SAB_HDR_IPV6_OUT_TUNNEL ||
                header_proto == SAB_HDR_IPV4_OUT_TUNNEL_NATT ||
                header_proto == SAB_HDR_IPV6_OUT_TUNNEL_NATT ||
                (sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_TRANSPORT_NAT) != 0)
            {
#ifdef SAB_STRICT_ARGS_CHECK
                if (sa_params_ipsec_p->src_ip_addr_p == NULL ||
                    sa_params_ipsec_p->dest_ip_addr_p == NULL)
                {
                    LOG_CRIT("SABuilder: NULL pointer tunnel address.\n");
                    return SAB_INVALID_PARAMETER;
                }
#endif
                sa_builder_lib_copy_key_mat(sa_buffer_p,
                                        FIRMWARE_EIP207_CS_FLOW_TR_TUNNEL_SRC_WORD_OFFSET + large_transform_offset,
                                        sa_params_ipsec_p->src_ip_addr_p,
                                        (sa_params_ipsec_p->ipsec_flags&SAB_IPSEC_IPV4)!=0?4:16);
                sa_builder_lib_copy_key_mat(sa_buffer_p,
                                        FIRMWARE_EIP207_CS_FLOW_TR_TUNNEL_DST_WORD_OFFSET + large_transform_offset,
                                        sa_params_ipsec_p->dest_ip_addr_p,
                                        (sa_params_ipsec_p->ipsec_flags&SAB_IPSEC_IPV4)!=0?4:16);

#ifdef FIRMWARE_EIP207_CS_FLOW_TR_CHECKSUM_WORD_OFFSET
                // checksum (only for IPv4)
                if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_IPV4) !=0)
                    sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_CHECKSUM_WORD_OFFSET +
                        large_transform_offset] = check_sum;
#endif
            }

            sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_PATH_MTU_WORD_OFFSET +
                       large_transform_offset] =  mtu_discount;
#endif /* SAB_ENABLE_EXTENDED_TUNNEL_HEADER */
        }
        else
#endif /* SAB_ENABLE_TWO_FIXED_RECORD_SIZES */
        {
            sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_FLAGS_WORD_OFFSET] = flags;
            sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_HDRPROC_CTX_WORD_OFFSET] =
                sa_params_ipsec_p->context_ref;
            sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_BYTE_PARAM_WORD_OFFSET] =
                SAB_PACKBYTES(iv_byte_count,icv_byte_count,header_proto,esp_proto);
            sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_TK_HDR_WORD_OFFSET] = token_header_word;
            if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_CHECKSUM_FIX) != 0 &&
                (sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_TUNNEL) == 0)
                sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_PAD_ALIGN_WORD_OFFSET] =
                    SAB_PACKBYTES(pad_block_byte_count/2,
                                  0,
                                  check_sum & 0xff,
                                  check_sum >> 8);
            else if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_TUNNEL) != 0)
                sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_PAD_ALIGN_WORD_OFFSET] =
                    SAB_PACKBYTES(pad_block_byte_count/2,
                                  0,
                                  sa_params_ipsec_p->ttl,
                                  sa_params_ipsec_p->dscp);
            else
                sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_PAD_ALIGN_WORD_OFFSET] =
                    SAB_PACKBYTES(pad_block_byte_count/2,
                                  0,
                                  0,
                                  0);
            sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_CCM_SALT_WORD_OFFSET] = ccm_salt;
            sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_TK_VFY_INST_WORD_OFFSET] =
                verify_instruction_word;
            sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_TK_CTX_INST_WORD_OFFSET] =
                ctx_instruction_word;
            sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_TIME_STAMP_LO_WORD_OFFSET] = 0;
            sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_TIME_STAMP_HI_WORD_OFFSET] = 0;
            sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_STAT_OCT_LO_WORD_OFFSET] = 0;
            sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_STAT_OCT_HI_WORD_OFFSET] = 0;
            sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_STAT_PKT_WORD_OFFSET] = 0;
#ifdef SAB_ENABLE_EXTENDED_TUNNEL_HEADER
            sa_buffer_p[FIMRWARE_EIP207_CS_FLOW_TR_NATT_PORTS_WORD_OFFSET] =
                SAB_PACKBYTES(sa_params_ipsec_p->natt_src_port >> 8,
                              sa_params_ipsec_p->natt_src_port & 0xff,
                              sa_params_ipsec_p->natt_dest_port >> 8,
                              sa_params_ipsec_p->natt_dest_port & 0xff);

            if (header_proto == SAB_HDR_IPV4_OUT_TUNNEL ||
                header_proto == SAB_HDR_IPV6_OUT_TUNNEL ||
                header_proto == SAB_HDR_IPV4_OUT_TUNNEL_NATT ||
                header_proto == SAB_HDR_IPV6_OUT_TUNNEL_NATT ||
                (sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_TRANSPORT_NAT) != 0)
            {
#ifdef SAB_STRICT_ARGS_CHECK
                if (sa_params_ipsec_p->src_ip_addr_p == NULL ||
                    sa_params_ipsec_p->dest_ip_addr_p == NULL)
                {
                    LOG_CRIT("SABuilder: NULL pointer tunnel address.\n");
                    return SAB_INVALID_PARAMETER;
                }
#endif
                sa_builder_lib_copy_key_mat(sa_buffer_p,
                                        FIRMWARE_EIP207_CS_FLOW_TR_TUNNEL_SRC_WORD_OFFSET,
                                        sa_params_ipsec_p->src_ip_addr_p,
                                        (sa_params_ipsec_p->ipsec_flags&SAB_IPSEC_IPV4)!=0?4:16);
                sa_builder_lib_copy_key_mat(sa_buffer_p,
                                        FIRMWARE_EIP207_CS_FLOW_TR_TUNNEL_DST_WORD_OFFSET,
                                        sa_params_ipsec_p->dest_ip_addr_p,
                                        (sa_params_ipsec_p->ipsec_flags&SAB_IPSEC_IPV4)!=0?4:16);

#ifdef FIRMWARE_EIP207_CS_FLOW_TR_CHECKSUM_WORD_OFFSET
                // checksum (only for IPv4)
                if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_IPV4) !=0)
                    sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_CHECKSUM_WORD_OFFSET] = check_sum;
#endif

            }
            sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_PATH_MTU_WORD_OFFSET] =  mtu_discount;
#endif /* SAB_ENABLE_EXTENDED_TUNNEL_HEADER */
        }
    }
    return SAB_STATUS_OK;
}


#endif /* SAB_ENABLE_IPSEC_EXTENDED */


/* end of file sa_builder_extended_ipsec.c */
