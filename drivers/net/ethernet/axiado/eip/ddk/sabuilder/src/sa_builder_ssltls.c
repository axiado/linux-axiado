/* sa_builder_ssltls.c
 *
 * SSL/TLS/DTLS specific functions (for initialization of sa_builder_params_t
 * structures and for building the SSL/TLS/DTLS specifc part of an SA.).
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
#include "sa_builder_ssltls.h"
#include "sa_builder_internal.h" /* sa_builder_set_ssltls_params */

/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */
#include "c_sa_builder.h"
#include "basic_defs.h"
#include "log.h"

#ifdef SAB_ENABLE_PROTO_SSLTLS

/*----------------------------------------------------------------------------
 * Definitions and macros
 */
#ifdef SAB_ENABLE_384BIT_SEQMASK
#define SAB_SEQUENCE_MAXBITS 384
#else
#define SAB_SEQUENCE_MAXBITS 128
#endif


/*----------------------------------------------------------------------------
 * Local variables
 */

/*----------------------------------------------------------------------------
 * sa_builder_init_ssltls
 *
 * This function initializes the sa_builder_params_t data structure and its
 * sa_builder_params_ssltls_t extension with sensible defaults for SSL, TLS
 * and DTLS processing.
 *
 * sa_params_p (output)
 *   Pointer to SA parameter structure to be filled in.
 * sa_params_ssltls_p (output)
 *   Pointer to SSLTLS parameter extension to be filled in
 * version (input)
 *   Version code for the desired protcol (choose one of the SAB_*_VERSION_*
 *   constants from sa_builder_params_ssltls.h).
 * direction (input)
 *   Must be one of SAB_DIRECTION_INBOUND or SAB_DIRECTION_OUTBOUND.
 *
 * Both the crypto and the authentication algorithm are initialized to
 * NULL. The crypto algorithm (which may remain NULL) must be set to
 * one of the algorithms supported by the protocol. The authentication
 * algorithm must also be set to one of the algorithms supported by
 * the protocol..Any required keys have to be specified as well.
 *
 * Both the sa_params_p and sa_params_ssltls_p input parameters must point
 * to valid storage where variables of the appropriate type can be
 * stored. This function initializes the link from sa_params_p to
 * SAParamsSSSLTLS_p.
 *
 * Return:
 * SAB_STATUS_OK on success
 * SAB_INVALID_PARAMETER when one of the pointer parameters is NULL
 *   or the remaining parameters have illegal values.
 */
sa_builder_status_t
sa_builder_init_ssltls(
    sa_builder_params_t * const sa_params_p,
    sa_builder_params_ssltls_t * const sa_params_ssltls_p,
    const u16 version,
    const sa_builder_direction_t direction)
{
    int i;
#ifdef SAB_STRICT_ARGS_CHECK
    if (sa_params_p == NULL || sa_params_ssltls_p == NULL)
    {
        LOG_CRIT("sa_builder_init_ssltls: NULL pointer parameter supplied.\n");
        return SAB_INVALID_PARAMETER;
    }

    if (version != SAB_SSL_VERSION_3_0 &&
        version != SAB_TLS_VERSION_1_0 &&
        version != SAB_TLS_VERSION_1_1 &&
        version != SAB_TLS_VERSION_1_2 &&
        version != SAB_TLS_VERSION_1_3 &&
        version != SAB_DTLS_VERSION_1_0 &&
        version != SAB_DTLS_VERSION_1_2)
    {
        LOG_CRIT("sa_builder_init_ssltls: Invalid protocol version.\n");
        return SAB_INVALID_PARAMETER;
    }

    if (direction != SAB_DIRECTION_OUTBOUND &&
        direction != SAB_DIRECTION_INBOUND)
    {
        LOG_CRIT("sa_builder_init_esp: Invalid direction.\n");
        return SAB_INVALID_PARAMETER;
    }
#endif

    sa_params_p->protocol = SAB_PROTO_SSLTLS;
    sa_params_p->direction = direction;
    sa_params_p->protocol_extension_p = (void*)sa_params_ssltls_p;
    sa_params_p->flags = 0;
    sa_params_p->redirect_interface = 0;

    sa_params_p->crypto_algo = SAB_CRYPTO_NULL;
    sa_params_p->crypto_mode = SAB_CRYPTO_MODE_CBC;
    sa_params_p->iv_src = SAB_IV_SRC_DEFAULT;
    sa_params_p->crypto_parameter = 0;
    sa_params_p->key_byte_count = 0;
    sa_params_p->key_p = NULL;
    sa_params_p->iv_p = NULL;
    sa_params_p->nonce_p = NULL;

    sa_params_p->auth_algo = SAB_AUTH_NULL;
    sa_params_p->auth_key1_p = NULL;
    sa_params_p->auth_key2_p = NULL;
    sa_params_p->auth_key3_p = NULL;
    sa_params_p->auth_key_byte_count = 0;

    sa_params_p->offset_arc4_state_record = 0;
    sa_params_p->cw0 = 0;
    sa_params_p->cw1 = 0;
    sa_params_p->offset_digest0 = 0;
    sa_params_p->offset_digest1 = 0;
    sa_params_p->offset_seq_num = 0;
    sa_params_p->offset_seq_mask = 0;
    sa_params_p->offset_iv = 0;
    sa_params_p->offset_ij_ptr = 0;
    sa_params_p->offset_arc4_state = 0;
    sa_params_p->seq_num_word32_count = 0;
    sa_params_p->seq_mask_word32_count = 0;
    sa_params_p->iv_word32_count = 0;

    sa_params_ssltls_p->ssltls_flags = 0;
    sa_params_ssltls_p->version = version;
    sa_params_ssltls_p->epoch = 0;
    sa_params_ssltls_p->seq_num = 0;
    sa_params_ssltls_p->seq_num_hi = 0;
    for (i=0; i<12; i++)
        sa_params_ssltls_p->seq_mask[i] = 0;
    sa_params_ssltls_p->pad_alignment = 0;
    sa_params_ssltls_p->context_ref = 0;
    sa_params_ssltls_p->sequence_mask_bit_count = 0;
    sa_params_ssltls_p->icv_byte_count = 0;
    return SAB_STATUS_OK;
}

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
                          u32 * const sa_buffer_p)
{
    unsigned int iv_offset = 0;
    sa_builder_params_ssltls_t *sa_params_ssltls_p;
    bool f_fixed_seq_offset = false;
    sa_params_ssltls_p = (sa_builder_params_ssltls_t *)
        (sa_params_p->protocol_extension_p);
    if (sa_params_ssltls_p == NULL)
    {
        LOG_CRIT("SABuilder: SSLTLS extension pointer is null\n");
        return SAB_INVALID_PARAMETER;
    }

    /* Prohibit arc4 in DTLS and TLS1.3 */
    if (sa_params_p->crypto_algo == SAB_CRYPTO_ARCFOUR &&
        (sa_params_ssltls_p->version == SAB_TLS_VERSION_1_3||
         sa_params_ssltls_p->version == SAB_DTLS_VERSION_1_0||
         sa_params_ssltls_p->version == SAB_DTLS_VERSION_1_2))
    {
        LOG_CRIT("SABuilder: arc4 not allowed with DTLS/TLS1.3\n");
        return SAB_INVALID_PARAMETER;
    }

    /* Prohibit CBC mode or NULL crypto in TLS1.3 */
    if (sa_params_ssltls_p->version == SAB_TLS_VERSION_1_3 &&
        (sa_params_p->crypto_mode == SAB_CRYPTO_MODE_CBC ||
         sa_params_p->crypto_algo == SAB_CRYPTO_NULL))
    {
        LOG_CRIT("SABuilder: CBC or nullcrypto  not allowed with TLS1.3\n");
        return SAB_INVALID_PARAMETER;
    }

    /* AES GCM/CCM/Chacha20 is only allowed with TLS1.2/TLS1.3/DTLS1.2 */
    if ((sa_params_p->crypto_algo == SAB_CRYPTO_CHACHA20 ||
         sa_params_p->crypto_mode == SAB_CRYPTO_MODE_GCM||
         sa_params_p->crypto_mode == SAB_CRYPTO_MODE_CCM) &&
        sa_params_ssltls_p->version != SAB_TLS_VERSION_1_2 &&
        sa_params_ssltls_p->version != SAB_TLS_VERSION_1_3 &&
        sa_params_ssltls_p->version != SAB_DTLS_VERSION_1_2)
    {
        LOG_CRIT("SABuilder: AES-GCM/CCM/ChaCha20 only allowed with TLS/DTLS 1.2 and 1.3\n");
        return SAB_INVALID_PARAMETER;
    }

    /* Prohibit stateless arc4*/
    if (sa_params_p->crypto_algo == SAB_CRYPTO_ARCFOUR &&
        sa_params_p->crypto_mode != SAB_CRYPTO_MODE_STATEFUL)
    {
        LOG_CRIT("SABuilder: arc4 must be stateful\n");
        return SAB_INVALID_PARAMETER;
    }

    /* Check for supported algorithms and crypto modes in SSL/TLS */
    if ((sa_params_p->crypto_algo != SAB_CRYPTO_NULL &&
         sa_params_p->crypto_algo != SAB_CRYPTO_ARCFOUR &&
         sa_params_p->crypto_algo != SAB_CRYPTO_DES &&
         sa_params_p->crypto_algo != SAB_CRYPTO_3DES &&
         sa_params_p->crypto_algo != SAB_CRYPTO_AES &&
         sa_params_p->crypto_algo != SAB_CRYPTO_CHACHA20 &&
         sa_params_p->crypto_algo != SAB_CRYPTO_SM4 &&
         sa_params_p->crypto_algo != SAB_CRYPTO_BC0) ||
        (sa_params_p->crypto_algo != SAB_CRYPTO_NULL &&
         sa_params_p->crypto_mode != SAB_CRYPTO_MODE_STATEFUL &&
         sa_params_p->crypto_mode != SAB_CRYPTO_MODE_CBC &&
         sa_params_p->crypto_mode != SAB_CRYPTO_MODE_GCM &&
         sa_params_p->crypto_mode != SAB_CRYPTO_MODE_CCM &&
         sa_params_p->crypto_mode != SAB_CRYPTO_MODE_CHACHA_CTR32))
    {
        LOG_CRIT("SABuilder: SSLTLS crypto algorithm/mode not supported\n");
        return SAB_INVALID_PARAMETER;
    }

    /* Check for supported authentication algorithms in SSL/TLS */
    if (sa_params_p->auth_algo != SAB_AUTH_HMAC_MD5 &&
        sa_params_p->auth_algo != SAB_AUTH_SSLMAC_MD5 &&
        sa_params_p->auth_algo != SAB_AUTH_HMAC_SHA1 &&
        sa_params_p->auth_algo != SAB_AUTH_SSLMAC_SHA1 &&
        sa_params_p->auth_algo != SAB_AUTH_HMAC_SHA2_256 &&
        sa_params_p->auth_algo != SAB_AUTH_HMAC_SHA2_384 &&
        sa_params_p->auth_algo != SAB_AUTH_HMAC_SHA2_512 &&
        sa_params_p->auth_algo != SAB_AUTH_AES_GCM &&
        sa_params_p->auth_algo != SAB_AUTH_AES_CCM &&
        sa_params_p->auth_algo != SAB_AUTH_POLY1305 &&
        sa_params_p->auth_algo != SAB_AUTH_HMAC_SM3)
    {
        LOG_CRIT("SABuilder: SSLTLS: auth algorithm not supported\n");
        return SAB_INVALID_PARAMETER;
    }

    /* Add version to SA record */
    if (sa_buffer_p != NULL)
    {
        if (sa_params_ssltls_p->version == SAB_TLS_VERSION_1_3)
        {
            sa_buffer_p[sa_state_p->current_offset] = SAB_TLS_VERSION_1_2<<16;
            /* fixed type & version to be put in record for TLS1.3 */
        }
        else
        {
            sa_buffer_p[sa_state_p->current_offset] = sa_params_ssltls_p->version<<16;
        }
    }
    sa_state_p->current_offset += 1;

    /* Determine whether we will have a fixed sequence number offset */
    if (sa_params_p->direction == SAB_DIRECTION_INBOUND &&
        (sa_params_ssltls_p->version == SAB_DTLS_VERSION_1_0||
         sa_params_ssltls_p->version == SAB_DTLS_VERSION_1_2))
    {
        /* Determine size of sequence number mask in bits */
        if (sa_params_ssltls_p->sequence_mask_bit_count == 0)
        {
            if ((sa_params_ssltls_p->ssltls_flags & SAB_DTLS_MASK_128) != 0)
            {
                sa_params_ssltls_p->sequence_mask_bit_count = 128;
            }
            else if ((sa_params_ssltls_p->ssltls_flags & SAB_DTLS_MASK_32) != 0)
            {
                sa_params_ssltls_p->sequence_mask_bit_count = 32;
            }
            else
            {
                sa_params_ssltls_p->sequence_mask_bit_count = 64;
            }
        }
        if (sa_params_ssltls_p->sequence_mask_bit_count > SAB_SEQUENCE_MAXBITS ||
            (sa_params_ssltls_p->sequence_mask_bit_count & 0x1f) != 0)
        {
            LOG_CRIT("SABuilder: Illegal sequence mask size.\n");
            return SAB_INVALID_PARAMETER;
        }
#ifdef SAB_ENABLE_DEFAULT_FIXED_OFFSETS
        f_fixed_seq_offset = true;
#else
        if (sa_params_ssltls_p->sequence_mask_bit_count > 128 ||
            (sa_params_ssltls_p->ssltls_flags & SAB_DTLS_FIXED_SEQ_OFFSET) != 0)
        {
            f_fixed_seq_offset = true;
        }
#endif
        if (sa_params_ssltls_p->sequence_mask_bit_count == 32)
        {
            f_fixed_seq_offset = false; /* not supported for 32-bit mask */
        }
    }


    if (f_fixed_seq_offset)
    {
        /* Use a fixed sequence number offset for inbound if the hardware
           supports it. */
        /* Take care to insert the iv (nonce) just after the spi. */
        iv_offset = sa_state_p->current_offset;

        /* Select one of two fixed offsets for the sequence number */
        if (sa_state_p->current_offset < SAB_SEQNUM_LO_FIX_OFFSET)
        {
            sa_state_p->current_offset = SAB_SEQNUM_LO_FIX_OFFSET;
        }
        else
        {
            sa_state_p->current_offset = SAB_SEQNUM_HI_FIX_OFFSET;
        }

        /* Add sequence number */
        sa_params_p->offset_seq_num = sa_state_p->current_offset;
        sa_params_p->seq_num_word32_count = 2;
        sa_state_p->cw1 |= SAB_CW1_SEQNUM_STORE;
        sa_state_p->cw0 |= SAB_CW0_SPI | SAB_CW0_SEQNUM_48_FIX;
        if (sa_buffer_p != NULL)
        {
            sa_buffer_p[sa_state_p->current_offset] = sa_params_ssltls_p->seq_num;
            sa_buffer_p[sa_state_p->current_offset + 1] =
                    (sa_params_ssltls_p->seq_num_hi & 0xffff) |
                    (sa_params_ssltls_p->epoch << 16);
        }
        sa_state_p->current_offset += 2;
    }
    else
    {
        /* Add sequence number */
        sa_params_p->offset_seq_num = sa_state_p->current_offset;
        sa_params_p->seq_num_word32_count = 2;
        sa_state_p->cw1 |= SAB_CW1_SEQNUM_STORE;

        if (sa_buffer_p != NULL)
            sa_buffer_p[sa_state_p->current_offset] = sa_params_ssltls_p->seq_num;
        sa_state_p->current_offset += 1;
        if (sa_params_ssltls_p->version != SAB_DTLS_VERSION_1_0 &&
            sa_params_ssltls_p->version != SAB_DTLS_VERSION_1_2)
        {
            sa_state_p->cw0 |= SAB_CW0_SPI | SAB_CW0_SEQNUM_64;
            if (sa_buffer_p != NULL)
                sa_buffer_p[sa_state_p->current_offset] =
                    sa_params_ssltls_p->seq_num_hi;
        }
        else
        {
            sa_state_p->cw0 |= SAB_CW0_SPI | SAB_CW0_SEQNUM_48;
            if (sa_buffer_p != NULL)
                sa_buffer_p[sa_state_p->current_offset] =
                    (sa_params_ssltls_p->seq_num_hi & 0xffff) |
                    (sa_params_ssltls_p->epoch << 16);
        }
        sa_state_p->current_offset += 1;
    }

    if (sa_params_p->direction == SAB_DIRECTION_OUTBOUND)
    {

        if (sa_params_p->crypto_algo==SAB_CRYPTO_NULL)
            sa_state_p->cw0 |= SAB_CW0_TOP_HASH_OUT;
        else if (sa_params_p->crypto_mode == SAB_CRYPTO_MODE_GCM||
                 sa_params_p->crypto_algo == SAB_CRYPTO_CHACHA20)
            sa_state_p->cw0 |= SAB_CW0_TOP_ENCRYPT_HASH;
        else
            sa_state_p->cw0 |= SAB_CW0_TOP_HASH_ENCRYPT;

        /* Some versions of the hardware can update the sequence number
           early, so multiple engines can operate in parallel. */
        sa_state_p->cw1 |= SAB_CW1_EARLY_SEQNUM_UPDATE;
        sa_state_p->cw1 |= sa_params_p->offset_seq_num << 24;

        /* Take care of iv  */
        if (sa_params_p->crypto_mode == SAB_CRYPTO_MODE_CCM)
        {
            if (sa_params_ssltls_p->version == SAB_TLS_VERSION_1_3)
            {
                if (sa_params_p->nonce_p == NULL)
                {
                    return SAB_INVALID_PARAMETER;
                }
                sa_state_p->cw1 = (sa_state_p->cw1 & ~SAB_CW1_IV_MODE_MASK) |
                    SAB_CW1_IV_ORIG_SEQ|SAB_CW1_CRYPTO_NONCE_XOR;
                sa_state_p->cw1 |= SAB_CW1_IV0|SAB_CW1_IV1|SAB_CW1_IV2|
                    SAB_CW1_IV3 | SAB_CW1_CCM_IV_SHIFT;
                if (sa_buffer_p != NULL)
                {
                    sa_builder_lib_copy_key_mat(sa_buffer_p,
                                            sa_state_p->current_offset,
                                            sa_params_p->nonce_p,
                                            3 * sizeof(u32));
                    sa_buffer_p[sa_state_p->current_offset + 3] =
                        SAB_CCM_FLAG_L3 << 24;
                }
                sa_state_p->current_offset += 4;
            }
            else
            {
                if (sa_params_p->nonce_p == NULL)
                {
                    return SAB_INVALID_PARAMETER;
                }
                sa_state_p->cw1 = (sa_state_p->cw1 & ~SAB_CW1_IV_MODE_MASK) |
                    SAB_CW1_IV_ORIG_SEQ;
                sa_state_p->cw1 |= SAB_CW1_IV0|SAB_CW1_IV3 | SAB_CW1_CCM_IV_SHIFT;
                if (sa_buffer_p != NULL)
                {
                    sa_builder_lib_copy_key_mat(sa_buffer_p,
                                            sa_state_p->current_offset,
                                            sa_params_p->nonce_p,
                                            sizeof(u32));
                    sa_buffer_p[sa_state_p->current_offset + 1] =
                        SAB_CCM_FLAG_L3 << 24;
                }
                sa_state_p->current_offset += 2;
            }
        }
        else if (sa_params_ssltls_p->version == SAB_TLS_VERSION_1_3 ||
                 sa_params_p->crypto_algo == SAB_CRYPTO_CHACHA20)
        {
            if (sa_params_p->nonce_p == NULL)
            {
                return SAB_INVALID_PARAMETER;
            }
            sa_state_p->cw1 = (sa_state_p->cw1 & ~SAB_CW1_IV_MODE_MASK) |
                SAB_CW1_IV_ORIG_SEQ|SAB_CW1_CRYPTO_NONCE_XOR;
            sa_state_p->cw1 |= SAB_CW1_IV0|SAB_CW1_IV1|SAB_CW1_IV2;
            /* Always store the nonce (implicit salt) with TLS1.3 */
            sa_builder_lib_copy_key_mat(sa_buffer_p,
                                    sa_state_p->current_offset,
                                    sa_params_p->nonce_p, 3*sizeof(u32));
            sa_state_p->current_offset +=3;
        }
        else if (sa_params_p->crypto_mode == SAB_CRYPTO_MODE_GCM)
        {
            if (sa_params_p->iv_src == SAB_IV_SRC_DEFAULT)
                sa_params_p->iv_src = SAB_IV_SRC_SEQ;

            if (sa_params_p->iv_src == SAB_IV_SRC_SEQ)
            {
                sa_state_p->cw1 |= SAB_CW1_IV_ORIG_SEQ;
            }
            else if (sa_params_p->iv_src == SAB_IV_SRC_XORSEQ)
            {
                sa_state_p->cw1 |= SAB_CW1_IV_CTR|SAB_CW1_CRYPTO_NONCE_XOR;
                sa_state_p->cw1 |= SAB_CW1_IV0|SAB_CW1_IV1|SAB_CW1_IV2;
            }
            else if (sa_params_p->iv_src == SAB_IV_SRC_SA)
            {
                sa_state_p->cw1 |= SAB_CW1_IV_CTR | SAB_CW1_IV1 | SAB_CW1_IV2;
#ifdef SAB_STRICT_ARGS_CHECK
                if (sa_params_p->iv_p == NULL)
                {
                    LOG_CRIT("SABuilder: NULL pointer iv.\n");
                    return SAB_INVALID_PARAMETER;
                }
#endif
                sa_params_p->offset_iv = sa_state_p->current_offset;
                sa_params_p->iv_word32_count = 2;

                sa_builder_lib_copy_key_mat(sa_buffer_p, sa_state_p->current_offset,
                                        sa_params_p->iv_p, 8);
                sa_state_p->current_offset += 2;
            }
            else
            {
                sa_state_p->cw1 |= SAB_CW1_IV_CTR;
            }
            if (sa_params_p->nonce_p == NULL)
            {
                return SAB_INVALID_PARAMETER;
            }
            sa_state_p->cw1 |= SAB_CW1_IV0;
            /* Always store the nonce (implicit salt) with AES-GCM */
            sa_builder_lib_copy_key_mat(sa_buffer_p,
                                    sa_state_p->current_offset,
                                    sa_params_p->nonce_p,
                                    sizeof(u32));
            sa_state_p->current_offset +=1;
            if (sa_params_p->iv_src == SAB_IV_SRC_XORSEQ)
            {
                /* Store a fixed 8-byte value to XOR with sequence number */
                sa_builder_lib_copy_key_mat(sa_buffer_p,
                                    sa_state_p->current_offset,
                                    sa_params_p->iv_p,
                                        2*sizeof(u32));
                sa_state_p->current_offset +=2;
            }
        }
        else if (sa_state_p->iv_words > 0)
        { /* CBC mode, non-null */
            if (sa_params_ssltls_p->version == SAB_SSL_VERSION_3_0 ||
                sa_params_ssltls_p->version == SAB_TLS_VERSION_1_0)
            {
                sa_params_p->iv_src = SAB_IV_SRC_SA;
                sa_state_p->cw1 |= SAB_CW1_CRYPTO_STORE;
            }
            else if (sa_params_p->iv_src == SAB_IV_SRC_DEFAULT)
            {
                sa_params_p->iv_src = SAB_IV_SRC_PRNG;
            }
            sa_state_p->cw1 |= SAB_CW1_IV_FULL;
            if (sa_params_p->iv_src == SAB_IV_SRC_SA)
            {
                sa_state_p->cw1 |= SAB_CW1_IV0 | SAB_CW1_IV1;
                if(sa_state_p->iv_words == 4)
                    sa_state_p->cw1 |= SAB_CW1_IV2 | SAB_CW1_IV3;
#ifdef SAB_STRICT_ARGS_CHECK
                if (sa_params_p->iv_p == NULL)
                {
                    LOG_CRIT("SABuilder: NULL pointer iv.\n");
                    return SAB_INVALID_PARAMETER;
                }
#endif
                sa_params_p->offset_iv = sa_state_p->current_offset;
                sa_params_p->iv_word32_count = sa_state_p->iv_words;

                sa_builder_lib_copy_key_mat(sa_buffer_p,
                                        sa_state_p->current_offset,
                                        sa_params_p->iv_p,
                                        sa_state_p->iv_words * sizeof(u32));
                sa_state_p->current_offset += sa_state_p->iv_words;
            }
        }
    }
    else
    {   /* Inbound */
        if (sa_params_p->crypto_algo==SAB_CRYPTO_NULL)
            sa_state_p->cw0 |= SAB_CW0_TOP_HASH_IN;
        else if (sa_params_p->crypto_mode == SAB_CRYPTO_MODE_GCM ||
                 sa_params_p->crypto_algo == SAB_CRYPTO_CHACHA20)
            sa_state_p->cw0 |= SAB_CW0_TOP_HASH_DECRYPT;
        else
            sa_state_p->cw0 |= SAB_CW0_TOP_DECRYPT_HASH;

        if (sa_params_p->crypto_mode != SAB_CRYPTO_MODE_GCM &&
            sa_params_p->crypto_mode != SAB_CRYPTO_MODE_CCM &&
            sa_params_p->crypto_algo != SAB_CRYPTO_CHACHA20  &&
            sa_state_p->iv_words > 0)
        {
            sa_state_p->cw1 |= SAB_CW1_PREPKT_OP;
            if (sa_params_ssltls_p->version == SAB_SSL_VERSION_3_0)
            {
                sa_state_p->cw1 |= SAB_CW1_PAD_SSL;
            }
            else
            {
                sa_state_p->cw1 |= SAB_CW1_PAD_TLS;
            }
        }
        if (sa_params_ssltls_p->version == SAB_TLS_VERSION_1_3)
        {
            sa_state_p->cw1 |= SAB_CW1_PAD_TLS | SAB_CW1_CRYPTO_AEAD;
        }

        /* Add sequence mask for DTLS only. */
        if ((sa_params_ssltls_p->version == SAB_DTLS_VERSION_1_0 ||
             sa_params_ssltls_p->version == SAB_DTLS_VERSION_1_2))
        {
            unsigned int input_mask_word_count =
                sa_params_ssltls_p->sequence_mask_bit_count / 32;
            unsigned int alloc_mask_word_count;
            sa_params_p->offset_seq_mask = sa_state_p->current_offset;
            /* Determine the required hardware mask size in words and set
               control words accordingly. */
            if (input_mask_word_count  == 1)
            {
                alloc_mask_word_count = 2;
                sa_state_p->cw0 |= SAB_CW0_MASK_32;
            }
            else if (input_mask_word_count  == 2)
            {
                alloc_mask_word_count = 2;
                if (f_fixed_seq_offset)
                {
                    sa_state_p->cw0 |= SAB_CW0_MASK_64_FIX;
                }
                else
                {
                    sa_state_p->cw0 |= SAB_CW0_MASK_64;
                }
            }
            else if (input_mask_word_count <= 4)
            {
                alloc_mask_word_count = 4;
                if (f_fixed_seq_offset)
                {
                sa_state_p->cw0 |= SAB_CW0_MASK_128_FIX;
                }
                else
                {
                sa_state_p->cw0 |= SAB_CW0_MASK_128;
                }
            }
#ifdef SAB_ENABLE_256BIT_SEQMASK
            else if (input_mask_word_count <= 8)
            {
                alloc_mask_word_count = 8;
                sa_state_p->cw0 |= SAB_CW0_MASK_256_FIX;
            }
#endif
            else
            {
                alloc_mask_word_count = 12;
                sa_state_p->cw0 |= SAB_CW0_MASK_384_FIX;
            }
            if(sa_buffer_p != NULL)
            {
                unsigned int i;
                for (i = 0; i < input_mask_word_count; i++)
                    sa_buffer_p[sa_state_p->current_offset+i] =
                        sa_params_ssltls_p->seq_mask[i];
                /* If the input mask is smaller than the one picked by the
                   hardware, fill the remaining words with all-one, the
                   hardware will treat these words as invalid.
                */
                for (i= input_mask_word_count; i < alloc_mask_word_count; i++)
                    sa_buffer_p[sa_state_p->current_offset+i] = 0xffffffff;
            }
            sa_state_p->current_offset += alloc_mask_word_count;
            sa_params_p->seq_mask_word32_count = input_mask_word_count;
        }

        if (iv_offset == 0)
            iv_offset = sa_state_p->current_offset;
        if (sa_state_p->iv_words > 0 &&
            (sa_params_ssltls_p->version == SAB_SSL_VERSION_3_0 ||
             sa_params_ssltls_p->version == SAB_TLS_VERSION_1_0))
        {
            sa_params_p->iv_src = SAB_IV_SRC_SA;
            sa_state_p->cw1 |= SAB_CW1_IV_FULL;

            sa_state_p->cw1 |= SAB_CW1_IV0 | SAB_CW1_IV1 |
                SAB_CW1_CRYPTO_STORE;
            if(sa_state_p->iv_words == 4)
                    sa_state_p->cw1 |= SAB_CW1_IV2 | SAB_CW1_IV3;
#ifdef SAB_STRICT_ARGS_CHECK
            if (sa_params_p->iv_p == NULL)
            {
                LOG_CRIT("SABuilder: NULL pointer iv.\n");
                    return SAB_INVALID_PARAMETER;
            }
#endif
            sa_params_p->offset_iv = iv_offset;
            sa_params_p->iv_word32_count = sa_state_p->iv_words;

            sa_builder_lib_copy_key_mat(sa_buffer_p,
                                    iv_offset,
                                    sa_params_p->iv_p,
                                    sa_state_p->iv_words * sizeof(u32));
            iv_offset += sa_state_p->iv_words;
        }

        if (sa_params_p->crypto_mode == SAB_CRYPTO_MODE_CCM)
        {
            if (sa_params_ssltls_p->version == SAB_TLS_VERSION_1_3)
            {
                sa_state_p->cw1 = (sa_state_p->cw1 & ~SAB_CW1_IV_MODE_MASK) |
                    SAB_CW1_IV_ORIG_SEQ|SAB_CW1_CRYPTO_NONCE_XOR;
                sa_state_p->cw1 |= SAB_CW1_IV0|SAB_CW1_IV1|SAB_CW1_IV2|
                    SAB_CW1_IV3 | SAB_CW1_CCM_IV_SHIFT;
                if (sa_buffer_p != NULL)
                {
                    if (sa_params_p->nonce_p == NULL)
                    {
                        return SAB_INVALID_PARAMETER;
                    }
                    sa_builder_lib_copy_key_mat(sa_buffer_p,
                                            sa_state_p->current_offset,
                                            sa_params_p->nonce_p,
                                            3 * sizeof(u32));
                    sa_buffer_p[sa_state_p->current_offset + 3] =
                        SAB_CCM_FLAG_L3 << 24;
                }
                sa_state_p->current_offset += 4;
            }
            else
            {
                sa_state_p->cw1 = (sa_state_p->cw1 & ~SAB_CW1_IV_MODE_MASK);
                sa_state_p->cw1 |= SAB_CW1_IV0| SAB_CW1_IV3 | SAB_CW1_CCM_IV_SHIFT;
                if (sa_buffer_p != NULL)
                {
                    if (sa_params_p->nonce_p == NULL)
                    {
                        return SAB_INVALID_PARAMETER;
                    }
                    sa_builder_lib_copy_key_mat(sa_buffer_p,
                                            sa_state_p->current_offset,
                                            sa_params_p->nonce_p,
                                            sizeof(u32));
                    sa_buffer_p[sa_state_p->current_offset + 1] =
                        SAB_CCM_FLAG_L3 << 24;
                }
                sa_state_p->current_offset += 2;
            }
        }
        else if (sa_params_ssltls_p->version == SAB_TLS_VERSION_1_3 ||
            sa_params_p->crypto_algo == SAB_CRYPTO_CHACHA20)
        {

            if (sa_params_ssltls_p->version == SAB_DTLS_VERSION_1_2)
            {
                /* DTLS inbound extracts the sequence number from the packet, use delayed OTK mode */
                sa_state_p->cw1 |= SAB_CW1_IV_CTR|SAB_CW1_CRYPTO_NONCE_XOR;
            } else {
                /* regular TLS inbound internally increments the sequence number, so can save some cycles by starting OTK calc early */
                sa_state_p->cw1 = (sa_state_p->cw1 & ~SAB_CW1_IV_MODE_MASK) |
                    SAB_CW1_IV_ORIG_SEQ|SAB_CW1_CRYPTO_NONCE_XOR;
            }
            sa_state_p->cw1 |= SAB_CW1_IV0|SAB_CW1_IV1|SAB_CW1_IV2;
            /* Always store the nonce (implicit salt) with TLS1.3 */
            if (sa_params_p->nonce_p == NULL)
            {
                return SAB_INVALID_PARAMETER;
            }
            sa_builder_lib_copy_key_mat(sa_buffer_p,
                                    iv_offset,
                                    sa_params_p->nonce_p, 3*sizeof(u32));
            iv_offset +=3;
        }
        else if(sa_params_p->crypto_mode == SAB_CRYPTO_MODE_GCM)
        {
            sa_state_p->cw1 |= SAB_CW1_IV_CTR|SAB_CW1_IV0;
            if (sa_params_p->nonce_p == NULL)
            {
                return SAB_INVALID_PARAMETER;
            }
            /* Always store the nonce (implicit salt) with AES-GCM */
            sa_builder_lib_copy_key_mat(sa_buffer_p,
                                    iv_offset,
                                    sa_params_p->nonce_p, sizeof(u32));
            iv_offset +=1;
        }
        if (iv_offset > sa_state_p->current_offset)
        {
            sa_state_p->current_offset = iv_offset;
        }

    }

    return SAB_STATUS_OK;
}


#endif /* SAB_ENABLE_PROTO_SSLTLS */

/* end of file sa_builder_ssltls.c */
