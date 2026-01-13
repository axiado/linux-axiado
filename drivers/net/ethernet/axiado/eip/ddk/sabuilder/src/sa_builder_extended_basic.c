/* sa_builder_extended_basic.c
 *
 * Basic opaeration specific functions (for initialization of
 * sa_builder_params_t structures and for building the Basic specific
 * part of an SA) in the Extended use case.
 */

/*****************************************************************************
* Copyright (c) 2016-2025 by Rambus, Inc. and/or its subsidiaries.
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
#ifdef SAB_ENABLE_BASIC_EXTENDED
#include "sa_builder_extended_internal.h"

/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */
#include "basic_defs.h"
#include "log.h"
#include "sa_builder_internal.h" /* sa_builder_set_basic_params */
#include "sa_builder_basic.h"

#include "firmware_eip207_api_cs.h"


/*----------------------------------------------------------------------------
 * Definitions and macros
 */

/*----------------------------------------------------------------------------
 * Local variables
 */


/*----------------------------------------------------------------------------
 * sa_builder_set_extended_basic_params
 *
 * Fill in Basic-specific extensions into the SA.for Extended.
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
sa_builder_set_extended_basic_params(sa_builder_params_t *const sa_params_p,
                                 sa_builder_state_t * const sa_state_p,
                                 u32 * const sa_buffer_p)
{
    sa_builder_params_basic_t *sa_params_basic_p =
        (sa_builder_params_basic_t *)(sa_params_p->protocol_extension_p);

    IDENTIFIER_NOT_USED(sa_state_p);

    if (sa_params_basic_p == NULL)
    {
        LOG_CRIT("sa_builder_init_basic: NULL pointer parameter supplied.\n");
        return SAB_INVALID_PARAMETER;
    }

#ifdef FIRMWARE_EIP207_CS_TR_IV_WORD_OFFSET
    /* These operations are specific to PDCP firmware */
    if (sa_params_p->auth_algo == SAB_AUTH_SNOW_UIA2 ||
        sa_params_p->auth_algo == SAB_AUTH_KASUMI_F9)
    {
        if(sa_buffer_p != NULL)
            sa_buffer_p[FIRMWARE_EIP207_CS_TR_IV_WORD_OFFSET] =
                sa_params_basic_p->fresh;
    }
#endif
#ifdef FIRMWARE_EIP207_CS_FLOW_TR_BYTE_PARAM_WORD_OFFSET
    /* These operations are specific to non-PDCP firmware */
    if ((sa_params_p->crypto_algo != SAB_CRYPTO_NULL &&
         sa_params_p->crypto_algo != SAB_CRYPTO_ARCFOUR &&
         sa_params_p->crypto_algo != SAB_CRYPTO_CHACHA20 &&
         sa_params_p->auth_algo != SAB_AUTH_NULL &&
         sa_params_p->auth_algo != SAB_AUTH_AES_GCM &&
         sa_params_p->auth_algo != SAB_AUTH_AES_GMAC &&
         sa_params_p->auth_algo != SAB_AUTH_AES_CCM) ||
        (sa_params_p->crypto_algo == SAB_CRYPTO_NULL &&
         sa_params_p->auth_algo == SAB_AUTH_NULL))
    {
        /* Only combined crypto + hash and not AES-GCM/AES-GMAC/AES-CCM are
           supported */
        u32 token_header_word = SAB_HEADER_DEFAULT;
        sa_builder_esp_protocol_t esp_proto;
        sa_builder_header_protocol_t header_proto;
        u8 pad_block_byte_count = 1;
        u8 iv_byte_count = 0;
        u8 icv_byte_count = 0;
        u32 flags = 0;
        u32 verify_instruction_word, ctx_instruction_word;
        u32 iv_instruction_word = 0;

        if (sa_params_p->crypto_algo == SAB_CRYPTO_NULL)
        {
            esp_proto = SAB_ESP_PROTO_NONE;
            if ((sa_params_basic_p->basic_flags & SAB_BASIC_FLAG_XFRM_API) != 0)

                header_proto = SAB_HDR_BASIC_IN_NO_PAD;
            else
                header_proto = SAB_HDR_BYPASS;
            verify_instruction_word = SAB_VERIFY_NONE;
        }
        else
        {
            if ((sa_params_basic_p->basic_flags & SAB_BASIC_FLAG_ENCRYPT_AFTER_HASH) != 0)
            {
                if (sa_params_p->direction == SAB_DIRECTION_OUTBOUND)
                {
                    esp_proto = SAB_BASIC_PROTO_OUT_HASHENC;
                    header_proto = SAB_HDR_BASIC_OUT_TPAD;
                }
                else
                {
                    esp_proto = SAB_BASIC_PROTO_IN_DECHASH;
                    token_header_word |= SAB_HEADER_PAD_VERIFY;
                    header_proto = SAB_HDR_BASIC_IN_PAD;
                }
            }
            else
            {
                if (sa_params_p->direction == SAB_DIRECTION_OUTBOUND)
                {
                    esp_proto = SAB_BASIC_PROTO_OUT_ENCHASH;
                    header_proto = SAB_HDR_BASIC_OUT_ZPAD;
                }
                else
                {
                    esp_proto = SAB_BASIC_PROTO_IN_HASHDEC;
                    header_proto = SAB_HDR_BASIC_IN_NO_PAD;
                }
        }

            if ((sa_params_p->flags & SAB_FLAG_SUPPRESS_HEADER) == 0)
                flags |= BIT_29;

            switch(sa_params_p->crypto_algo)
            {
            case SAB_CRYPTO_DES:
            case SAB_CRYPTO_3DES:
                iv_byte_count = 8;
                pad_block_byte_count = 8;
            break;
            case SAB_CRYPTO_AES:
            case SAB_CRYPTO_SM4:
            case SAB_CRYPTO_BC0:
                iv_byte_count = 16;
                pad_block_byte_count = 16;
                break;
            default:
                LOG_CRIT("sa_builder_build_sa: unsupported crypto algorithm\n");
                return SAB_UNSUPPORTED_FEATURE;
            }

            switch(sa_params_p->crypto_mode)
            {
            case SAB_CRYPTO_MODE_ECB:
                iv_byte_count = 0;
                iv_instruction_word = 0x20000004; /* NOP instruction */
                break;
            case SAB_CRYPTO_MODE_CBC:
                if (sa_params_p->iv_src == SAB_IV_SRC_DEFAULT ||
                    sa_params_p->iv_src == SAB_IV_SRC_INPUT)
                {
                    iv_instruction_word = SA_RETR_HASH_IV0 + iv_byte_count;
                }
                else
                {
                    iv_instruction_word = SA_INS_NONE_IV0 + iv_byte_count;
                    iv_byte_count = 0;
                }
                if (sa_params_p->iv_src == SAB_IV_SRC_PRNG)
                    token_header_word |= SAB_HEADER_IV_PRNG;
                if ((sa_params_p->flags & SAB_FLAG_COPY_IV) != 0)
                {
                    iv_instruction_word |= BIT_25|BIT_24; /* iv to output & hash */
                }
                if ((sa_params_basic_p->basic_flags & SAB_BASIC_FLAG_ENCRYPT_AFTER_HASH) != 0)
                {
                    iv_instruction_word &= ~BIT_25; /* Do not hash iv for HASHENC */
                }
                break;
            case SAB_CRYPTO_MODE_CTR:
                iv_byte_count = 8;
                pad_block_byte_count = 1;
                if (sa_params_p->iv_src == SAB_IV_SRC_DEFAULT ||
                    sa_params_p->iv_src == SAB_IV_SRC_INPUT)
                {
                    iv_instruction_word = SA_RETR_HASH_IV1 + iv_byte_count;
                }
                else
                {
                    iv_instruction_word = SA_INS_NONE_IV1 + iv_byte_count;
                    iv_byte_count = 0;
                }
                if ((sa_params_p->flags & SAB_FLAG_COPY_IV) != 0)
                {
                    iv_instruction_word |= BIT_25|BIT_24; /* iv to output & hash */
                }
                break;
            case SAB_CRYPTO_MODE_ICM:
                iv_byte_count = 16;
                pad_block_byte_count = 1;
                if (sa_params_p->iv_src == SAB_IV_SRC_DEFAULT ||
                    sa_params_p->iv_src == SAB_IV_SRC_INPUT)
                {
                    iv_instruction_word = SA_RETR_HASH_IV0 + iv_byte_count;
                }
                else
                {
                    iv_instruction_word = SA_INS_NONE_IV0 + iv_byte_count;
                    iv_byte_count = 0;
                }
                if ((sa_params_p->flags & SAB_FLAG_COPY_IV) != 0)
                {
                    iv_instruction_word |= BIT_25|BIT_24; /* iv to output & hash */
                }
                break;
            default:
                LOG_CRIT("sa_builder_build_sa: unsupported crypto mode\n");
                return SAB_UNSUPPORTED_FEATURE;
            }

            switch(sa_params_p->auth_algo)
            {
            case SAB_AUTH_HASH_MD5:
            case SAB_AUTH_SSLMAC_MD5:
            case SAB_AUTH_HMAC_MD5:
                icv_byte_count = 16;
                break;
            case SAB_AUTH_HASH_SHA1:
            case SAB_AUTH_SSLMAC_SHA1:
            case SAB_AUTH_HMAC_SHA1:
                icv_byte_count = 20;
                break;
            case SAB_AUTH_HASH_SHA3_224:
            case SAB_AUTH_KEYED_HASH_SHA3_224:
            case SAB_AUTH_HMAC_SHA3_224:
                icv_byte_count = 28;
                break;
            case SAB_AUTH_HASH_SHA2_224:
            case SAB_AUTH_HMAC_SHA2_224:
            case SAB_AUTH_HASH_SHA2_256:
            case SAB_AUTH_HMAC_SHA2_256:
            case SAB_AUTH_HMAC_SM3:
            case SAB_AUTH_HASH_SM3:
            case SAB_AUTH_HASH_SHA3_256:
            case SAB_AUTH_KEYED_HASH_SHA3_256:
            case SAB_AUTH_HMAC_SHA3_256:
                icv_byte_count = 32;
                break;
            case SAB_AUTH_HASH_SHA3_384:
            case SAB_AUTH_KEYED_HASH_SHA3_384:
            case SAB_AUTH_HMAC_SHA3_384:
                icv_byte_count = 48;
                break;
            case SAB_AUTH_HASH_SHA2_384:
            case SAB_AUTH_HMAC_SHA2_384:
                if ((sa_params_basic_p->basic_flags & SAB_BASIC_FLAG_ENCRYPT_AFTER_HASH) != 0)
                    icv_byte_count = 48;
                else
                    icv_byte_count = 64;
                break;
            case SAB_AUTH_HASH_SHA3_512:
            case SAB_AUTH_KEYED_HASH_SHA3_512:
            case SAB_AUTH_HMAC_SHA3_512:
            case SAB_AUTH_HASH_SHA2_512:
            case SAB_AUTH_HMAC_SHA2_512:
                icv_byte_count = 64;
                break;
            case SAB_AUTH_AES_XCBC_MAC:
            case SAB_AUTH_AES_CMAC_128:
            case SAB_AUTH_AES_CMAC_192:
            case SAB_AUTH_AES_CMAC_256:
                icv_byte_count = 16;
                break;
            default:
                LOG_CRIT("sa_builder_build_sa: unsupported authentication algorithm\n");
                return SAB_UNSUPPORTED_FEATURE;
            }
            if (sa_params_basic_p->icv_byte_count != 0 &&
                sa_params_basic_p->icv_byte_count < icv_byte_count)
                icv_byte_count = sa_params_basic_p->icv_byte_count;

            /* Take care of the VERIFY and CTX token instructions */
            if (sa_params_p->direction == SAB_DIRECTION_OUTBOUND)
            {
                verify_instruction_word = SAB_VERIFY_NONE;
            }
            else
            {
                if ((sa_params_basic_p->basic_flags & SAB_BASIC_FLAG_ENCRYPT_AFTER_HASH) != 0)
                {
                    verify_instruction_word = SAB_VERIFY_PAD;
                }
                else
                {
                    verify_instruction_word = SAB_VERIFY_NONE;
                }
                verify_instruction_word += SAB_VERIFY_BIT_H + icv_byte_count;
            }
        }

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

        /* Write all parameters to their respective offsets */
        if (sa_buffer_p != NULL)
        {
#ifdef SAB_ENABLE_TWO_FIXED_RECORD_SIZES
            if (sa_state_p->f_large)
            {
                sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_FLAGS_WORD_OFFSET +
                           large_transform_offset] = flags;
                sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_HDRPROC_CTX_WORD_OFFSET +
                    large_transform_offset] = sa_params_basic_p->context_ref;
                sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_BYTE_PARAM_WORD_OFFSET +
                           large_transform_offset] =
                    SAB_PACKBYTES(iv_byte_count,icv_byte_count,header_proto,esp_proto);
                sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_TK_HDR_WORD_OFFSET +
                           large_transform_offset] = token_header_word;
                sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_PAD_ALIGN_WORD_OFFSET +
                           large_transform_offset] =
                    SAB_PACKBYTES(pad_block_byte_count/2, 0, 0, 0);
                sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_CCM_SALT_WORD_OFFSET +
                    large_transform_offset] =iv_instruction_word;
                sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_TK_VFY_INST_WORD_OFFSET +
                    large_transform_offset] = verify_instruction_word;
                sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_TK_CTX_INST_WORD_OFFSET +
                    large_transform_offset] = ctx_instruction_word;
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
                    sa_params_basic_p->context_ref;
;
                sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_BYTE_PARAM_WORD_OFFSET] =
                    SAB_PACKBYTES(iv_byte_count,icv_byte_count,header_proto,esp_proto);
                sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_TK_HDR_WORD_OFFSET] = token_header_word;
                sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_PAD_ALIGN_WORD_OFFSET] =
                    SAB_PACKBYTES(pad_block_byte_count/2, 0, 0, 0);
                sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_CCM_SALT_WORD_OFFSET] = iv_instruction_word;
                sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_TK_VFY_INST_WORD_OFFSET] =
                    verify_instruction_word;
                sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_TK_CTX_INST_WORD_OFFSET] =
                    ctx_instruction_word;
                sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_TIME_STAMP_LO_WORD_OFFSET] = 0;
                sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_TIME_STAMP_HI_WORD_OFFSET] = 0;
                sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_STAT_OCT_LO_WORD_OFFSET] = 0;
                sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_STAT_OCT_HI_WORD_OFFSET] = 0;
                sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_STAT_PKT_WORD_OFFSET] = 0;
            }
        }
    }
#endif
    return SAB_STATUS_OK;
}

#endif /* SAB_ENABLE_BASIC_EXTENDED */


/* end of file sa_builder_extended_basic.c */
