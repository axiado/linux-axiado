/* sa_builder_extended_dtls.c
 *
 * DTLS specific functions (for initialization of sa_builder_params_t
 * structures and for building the DTLS specifc part of an SA.) in the
 * Extended use case.
 */

/*****************************************************************************
* Copyright (c) 2011-2022 by Rambus, Inc. and/or its subsidiaries.
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
#ifdef SAB_ENABLE_DTLS_EXTENDED
#include "sa_builder_extended_internal.h"

/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */
#include "basic_defs.h"
#include "log.h"
#include "sa_builder_internal.h" /* SABuilder_SetDTLSParams */
#include "sa_builder_ssltls.h"

/*----------------------------------------------------------------------------
 * Definitions and macros
 */

/*----------------------------------------------------------------------------
 * Local variables
 */


/*----------------------------------------------------------------------------
 * sa_builder_set_extended_dtls_params
 *
 * Fill in DTLS-specific extensions into the SA.for Extended.
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
sa_builder_set_extended_dtls_params(sa_builder_params_t *const sa_params_p,
                         sa_builder_state_t * const sa_state_p,
                         u32 * const sa_buffer_p)
{
    sa_builder_params_ssltls_t *sa_params_ssltls_p =
        (sa_builder_params_ssltls_t *)(sa_params_p->protocol_extension_p);
    u32 token_header_word = SAB_HEADER_DEFAULT;
    sa_builder_esp_protocol_t esp_proto;
    sa_builder_header_protocol_t header_proto;
    u8 pad_block_byte_count;
    u8 iv_byte_count;
    u8 icv_byte_count;
    u8 seq_offset;
    u8 anti_replay;
    u32 flags = 0;
    u32 verify_instruction_word, ctx_instruction_word;

    IDENTIFIER_NOT_USED(sa_state_p);

    if (sa_params_ssltls_p == NULL)
    {
        LOG_CRIT("SABuilder: SSLTLS extension pointer is null\n");
        return SAB_INVALID_PARAMETER;
    }

    if ((sa_params_ssltls_p->version != SAB_DTLS_VERSION_1_0 &&
         sa_params_ssltls_p->version != SAB_DTLS_VERSION_1_2) ||
        (sa_params_p->crypto_algo != SAB_CRYPTO_AES &&
         sa_params_p->crypto_algo != SAB_CRYPTO_CHACHA20 &&
         sa_params_p->crypto_algo != SAB_CRYPTO_3DES &&
         sa_params_p->crypto_algo != SAB_CRYPTO_SM4 &&
         sa_params_p->crypto_algo != SAB_CRYPTO_NULL))
    {
        if (sa_buffer_p != 0)
            LOG_CRIT("SABuilder: SSLTLS record only for look-aside\n");
        // No extended transform record can be created, however it can
        // still be valid for host look-aside.
        return SAB_STATUS_OK;
    }

    if ((sa_params_ssltls_p->ssltls_flags & SAB_DTLS_NO_ANTI_REPLAY) != 0)
        anti_replay = 0;
    else
        anti_replay = 1;

    if (sa_params_p->direction == SAB_DIRECTION_OUTBOUND)
    {
        if (sa_params_p->crypto_algo == SAB_CRYPTO_CHACHA20)
        {
            esp_proto = SAB_DTLS_PROTO_OUT_CHACHAPOLY;
            pad_block_byte_count = 0;
            iv_byte_count = 0;
        }
        else if (sa_params_p->auth_algo == SAB_AUTH_AES_GCM)
        {
            esp_proto = SAB_DTLS_PROTO_OUT_GCM;
            pad_block_byte_count = 0;
            iv_byte_count = 8;
        }
        else if (sa_params_p->crypto_algo == SAB_CRYPTO_NULL)
        {
            esp_proto = SAB_DTLS_PROTO_OUT_CHACHAPOLY;
            pad_block_byte_count = 0;
            iv_byte_count = 0;
        }
        else
        {
            switch (sa_params_p->iv_src)
            {
            case SAB_IV_SRC_DEFAULT:
            case SAB_IV_SRC_PRNG:
                token_header_word |=
                    SAB_HEADER_IV_PRNG;
                break;
            case SAB_IV_SRC_SA: /* No action required */
            case SAB_IV_SRC_TOKEN:
                break;
            default:
                LOG_CRIT("sa_builder_build_sa:"
                         "Unsupported iv source\n");
                return SAB_INVALID_PARAMETER;
            }
            esp_proto = SAB_DTLS_PROTO_OUT_CBC;
            if (sa_params_p->crypto_algo == SAB_CRYPTO_3DES)
            {
                pad_block_byte_count = 8;
                iv_byte_count = 8;
            }
            else
            {
                pad_block_byte_count = 16;
                iv_byte_count = 16;
            }
        }

        if ((sa_params_ssltls_p->ssltls_flags & SAB_DTLS_IPV6) !=0)
        {
            if ((sa_params_ssltls_p->ssltls_flags & SAB_DTLS_CAPWAP) !=0)
            {
                header_proto = SAB_HDR_IPV6_OUT_DTLS_CAPWAP;
            }
            else
            {
                header_proto = SAB_HDR_IPV6_OUT_DTLS;
            }
        }
        else
        {
            if ((sa_params_ssltls_p->ssltls_flags & SAB_DTLS_CAPWAP) !=0)
            {
                header_proto = SAB_HDR_IPV4_OUT_DTLS_CAPWAP;
            }
            else
            {
                header_proto = SAB_HDR_IPV4_OUT_DTLS;
            }
        }
    }
    else
    {
        if (sa_params_p->crypto_algo == SAB_CRYPTO_CHACHA20)
        {
            esp_proto = SAB_DTLS_PROTO_IN_CHACHAPOLY;
            pad_block_byte_count = 0;
            iv_byte_count = 0;
        }
        else if (sa_params_p->auth_algo == SAB_AUTH_AES_GCM)
        {
            esp_proto = SAB_DTLS_PROTO_IN_GCM;
            pad_block_byte_count = 0;
            iv_byte_count = 8;
        }
        else if (sa_params_p->crypto_algo == SAB_CRYPTO_NULL)
        {
            esp_proto = SAB_DTLS_PROTO_IN_CHACHAPOLY;
            pad_block_byte_count = 0;
            iv_byte_count = 0;
        }
        else
        {
            esp_proto = SAB_DTLS_PROTO_IN_CBC;
            token_header_word |= SAB_HEADER_PAD_VERIFY;
            if (sa_params_p->crypto_algo == SAB_CRYPTO_3DES)
            {
                pad_block_byte_count = 8;
                iv_byte_count = 8;
            }
            else
            {
                pad_block_byte_count = 16;
                iv_byte_count = 16;
            }
        }

        if ((sa_params_ssltls_p->ssltls_flags & SAB_DTLS_IPV6) !=0)
        {
            if ((sa_params_ssltls_p->ssltls_flags & SAB_DTLS_CAPWAP) !=0)
            {
                header_proto = SAB_HDR_IPV6_IN_DTLS_CAPWAP;
            }
            else
            {
                header_proto = SAB_HDR_IPV6_IN_DTLS;
            }
        }
        else
        {
            if ((sa_params_ssltls_p->ssltls_flags & SAB_DTLS_CAPWAP) !=0)
            {
                header_proto = SAB_HDR_IPV4_IN_DTLS_CAPWAP;
            }
            else
            {
                header_proto = SAB_HDR_IPV4_IN_DTLS;
            }
        }

        anti_replay *= sa_params_ssltls_p->sequence_mask_bit_count / 32;
    }
    seq_offset = sa_params_p->offset_seq_num;

    if (sa_params_p->direction == SAB_DIRECTION_OUTBOUND &&
        sa_params_ssltls_p->pad_alignment >
        pad_block_byte_count &&
        sa_params_ssltls_p->pad_alignment <= 256)
        pad_block_byte_count =
            sa_params_ssltls_p->pad_alignment;

    switch(sa_params_p->auth_algo)
    {
    case SAB_AUTH_HMAC_MD5:
        icv_byte_count = 16;
        break;
    case SAB_AUTH_HMAC_SHA1:
        icv_byte_count = 20;
        break;
    case SAB_AUTH_HMAC_SHA2_256:
    case SAB_AUTH_HMAC_SM3:
        icv_byte_count = 32;
        break;
    case SAB_AUTH_HMAC_SHA2_384:
        icv_byte_count = 48;
        break;
    case SAB_AUTH_HMAC_SHA2_512:
        icv_byte_count = 64;
        break;
    case SAB_AUTH_AES_GCM:
        icv_byte_count = 16;
        break;
    case SAB_AUTH_POLY1305:
        icv_byte_count = 16;
        break;
    break;
    default:
        LOG_CRIT("sa_builder_build_sa: unsupported authentication algorithm\n");
        return SAB_UNSUPPORTED_FEATURE;
    }

    /* flags variable */
    if ((sa_params_ssltls_p->ssltls_flags & SAB_DTLS_IPV6) !=0)
        flags |= BIT_8;
    if ((sa_params_ssltls_p->ssltls_flags & SAB_DTLS_PROCESS_IP_HEADERS) !=0)
        flags |= BIT_19;
    if ((sa_params_ssltls_p->ssltls_flags & SAB_DTLS_PLAINTEXT_HEADERS) !=0)
        flags |= BIT_29;

    /* Take care of the VERIFY and CTX token instructions */
    if (sa_params_p->direction == SAB_DIRECTION_OUTBOUND)
    {
        verify_instruction_word = SAB_VERIFY_NONE;
        ctx_instruction_word = SAB_CTX_OUT_SEQNUM +
            ((unsigned int)(2<<24)) + seq_offset;
    }
    else
    {
        if (pad_block_byte_count != 0)
        {
            verify_instruction_word = SAB_VERIFY_PAD;
        }
        else
        {
            verify_instruction_word = SAB_VERIFY_NONE;
        }
        if (icv_byte_count > 0)
        {
            verify_instruction_word += SAB_VERIFY_BIT_H + icv_byte_count;
        }
        if (anti_replay > 0)
        {
            verify_instruction_word += SAB_VERIFY_BIT_SEQ;
        }
        ctx_instruction_word = SAB_CTX_SEQNUM +
            ((unsigned int)(2+anti_replay)<<24) + seq_offset;
    }
    /* Write all parameters to their respective offsets */
    if (sa_buffer_p != NULL)
    {
#ifdef SAB_ENABLE_TWO_FIXED_RECORD_SIZES
        if (sa_state_p->f_large)
        {
            sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_FLAGS_WORD_OFFSET +
                       large_transform_offset] = flags;
            sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_HDRPROC_CTX_WORD_OFFSET +
                large_transform_offset] = sa_params_ssltls_p->context_ref;
            sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_BYTE_PARAM_WORD_OFFSET +
                       large_transform_offset] =
                SAB_PACKBYTES(iv_byte_count,icv_byte_count,header_proto,esp_proto);
            sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_TK_HDR_WORD_OFFSET +
                       large_transform_offset] = token_header_word;
            sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_PAD_ALIGN_WORD_OFFSET +
                       large_transform_offset] =
                SAB_PACKBYTES(pad_block_byte_count/2, 0, 0, 0);
            sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_CCM_SALT_WORD_OFFSET +
                       large_transform_offset] =0;
            sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_TK_VFY_INST_WORD_OFFSET +
                       large_transform_offset] = verify_instruction_word;
            sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_TK_CTX_INST_WORD_OFFSET +
                       large_transform_offset] = ctx_instruction_word;
            sa_buffer_p[FIMRWARE_EIP207_CS_FLOW_TR_NATT_PORTS_WORD_OFFSET +
                       large_transform_offset] =
                sa_params_ssltls_p->version;
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
        }
        else
#endif /* SAB_ENABLE_TWO_FIXED_RECORD_SIZES */
        {
            sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_FLAGS_WORD_OFFSET] = flags;
            sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_HDRPROC_CTX_WORD_OFFSET] =
                sa_params_ssltls_p->context_ref;
            sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_BYTE_PARAM_WORD_OFFSET] =
                SAB_PACKBYTES(iv_byte_count,icv_byte_count,header_proto,esp_proto);
            sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_TK_HDR_WORD_OFFSET] = token_header_word;
            sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_PAD_ALIGN_WORD_OFFSET] =
                SAB_PACKBYTES(pad_block_byte_count/2, 0, 0, 0);
            sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_CCM_SALT_WORD_OFFSET] = 0;
            sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_TK_VFY_INST_WORD_OFFSET] =
                verify_instruction_word;
            sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_TK_CTX_INST_WORD_OFFSET] =
                ctx_instruction_word;
            sa_buffer_p[FIMRWARE_EIP207_CS_FLOW_TR_NATT_PORTS_WORD_OFFSET] =
                sa_params_ssltls_p->version;
            sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_TIME_STAMP_LO_WORD_OFFSET] = 0;
            sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_TIME_STAMP_HI_WORD_OFFSET] = 0;
            sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_STAT_OCT_LO_WORD_OFFSET] = 0;
            sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_STAT_OCT_HI_WORD_OFFSET] = 0;
            sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_STAT_PKT_WORD_OFFSET] = 0;
        }
    }
    return SAB_STATUS_OK;
}


#endif /* SAB_ENABLE_DTLS_EXTENDED */


/* end of file sa_builder_extended_dtls.c */
