/* sa_builder_basic.c
 *
 * Basic Crypto/hash specific functions (for initialization of
 * sa_builder_params_t structures and for building the Basic Crypto/hash
 * specific part of an SA).
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
#include "sa_builder_basic.h"
#include "sa_builder_internal.h" /* sa_builder_set_basic_params */

/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */
#include "c_sa_builder.h"
#include "basic_defs.h"
#include "log.h"

#ifdef SAB_ENABLE_PROTO_BASIC

/*----------------------------------------------------------------------------
 * Definitions and macros
 */


/*----------------------------------------------------------------------------
 * Local variables
 */

/*----------------------------------------------------------------------------
 * sa_builder_init_basic
 *
 * This function initializes the sa_builder_params_t data structure and
 * its sa_builder_params_basic_t extension with sensible defaults for
 * basic crypto and hash processing.
 *
 * sa_params_p (output)
 *   Pointer to SA parameter structure to be filled in.
 *
 * sa_params_basic_p (output)
 *   Pointer to Basic parameter extension to be filled in
 *
 * direction (input)
 *   Must be one of SAB_DIRECTION_INBOUND or SAB_DIRECTION_OUTBOUND.
 *
 * Both the crypto and the authentication algorithm are initialized to
 * NULL. Either the cipher algorithm or the authentication algorithm
 * or both can be set to one of the supported algorithms for
 * basic crypto or basic hash or HMAC. If they are both left at NULL. the
 * SA will be a bypass SA. The crypto mode and iv source
 * can be specified as well.  Any required keys have to be specified
 * as well.
 *
 * Both the sa_params_p and sa_params_basic_p input parameters must point
 * to valid storage where variables of the appropriate type can be
 * stored. This function initializes the link from sa_params_p to
 * sa_params_basic_p.
 *
 * Return:
 * SAB_STATUS_OK on success
 * SAB_INVALID_PARAMETER when one of the pointer parameters is NULL
 *   or the remaining parameters have illegal values.
 */
sa_builder_status_t
sa_builder_init_basic(
        sa_builder_params_t * const sa_params_p,
        sa_builder_params_basic_t * const sa_params_basic_p,
        const sa_builder_direction_t direction)
{
#ifdef SAB_STRICT_ARGS_CHECK
    if (sa_params_p == NULL || sa_params_basic_p == NULL)
    {
        LOG_CRIT("sa_builder_init_basic: NULL pointer parameter supplied.\n");
        return SAB_INVALID_PARAMETER;
    }

    if (direction != SAB_DIRECTION_OUTBOUND &&
        direction != SAB_DIRECTION_INBOUND)
    {
        LOG_CRIT("sa_builder_init_basic: Invalid direction.\n");
        return SAB_INVALID_PARAMETER;
    }
#endif

    sa_params_p->protocol = SAB_PROTO_BASIC;
    sa_params_p->direction = direction;
    sa_params_p->protocol_extension_p = (void*)sa_params_basic_p;
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

    sa_params_basic_p->basic_flags = 0;
    sa_params_basic_p->digest_block_count = 0;
    sa_params_basic_p->icv_byte_count = 0;

    sa_params_basic_p->fresh = 0;
    sa_params_basic_p->bearer = 0;
    sa_params_basic_p->direction = 0;

    sa_params_basic_p->context_ref = 0;
    return SAB_STATUS_OK;
}


/*----------------------------------------------------------------------------
 * sa_builder_set_basic_params
 *
 * Fill in Basic Crypto and hash-specific extensions into the SA.
 *
 * sa_params_p (input)
 *   The SA parameters structure from which the SA is derived.
 *
 * sa_state_p (input, output)
 *   Variables containing information about the SA being generated.
 *
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
sa_builder_set_basic_params(
        sa_builder_params_t *const sa_params_p,
        sa_builder_state_t * const sa_state_p,
        u32 * const sa_buffer_p)
{
    sa_builder_params_basic_t *sa_params_basic_p;
    sa_params_basic_p = (sa_builder_params_basic_t *)
        (sa_params_p->protocol_extension_p);

    if (sa_params_basic_p == NULL)
    {
        LOG_CRIT("SABuilder: Basic extension pointer is null\n");
        return SAB_INVALID_PARAMETER;
    }

    if (sa_params_basic_p->direction > 1 ||
        sa_params_basic_p->bearer > 32)
    {
        LOG_CRIT("SABuilder: Illegal value for bearer or direction\n");
        return SAB_INVALID_PARAMETER;
    }

    /* Use one of the classic hash algorithms, possibly with
       encryption */
    if ((sa_params_p->auth_algo == SAB_AUTH_HASH_MD5 ||
         sa_params_p->auth_algo == SAB_AUTH_HASH_SHA1 ||
         sa_params_p->auth_algo == SAB_AUTH_HASH_SHA2_224 ||
         sa_params_p->auth_algo == SAB_AUTH_HASH_SHA2_256 ||
         sa_params_p->auth_algo == SAB_AUTH_HASH_SHA2_384 ||
         sa_params_p->auth_algo == SAB_AUTH_HASH_SHA2_512 ||
         sa_params_p->auth_algo == SAB_AUTH_HASH_SM3) &&
        (sa_params_p->flags & (SAB_FLAG_HASH_LOAD|SAB_FLAG_HASH_SAVE|
                              SAB_FLAG_HASH_INTERMEDIATE)) != 0)
    {
        /* We are doing basic hash (no HMAC) with storing the state.*/
        sa_state_p->cw1 |= SAB_CW1_HASH_STORE | SAB_CW1_DIGEST_CNT;
        if(sa_buffer_p != NULL)
            sa_buffer_p[sa_state_p->current_offset] =
                sa_params_basic_p->digest_block_count;
        sa_state_p->current_offset += 1;
    }
    else if (sa_params_p->auth_algo != SAB_AUTH_NULL &&
             (sa_params_p->flags & (SAB_FLAG_HASH_SAVE)) != 0)
    {
        sa_state_p->cw1 |= SAB_CW1_HASH_STORE;
    }

    if ((sa_params_p->auth_algo == SAB_AUTH_KASUMI_F9 ||
         sa_params_p->auth_algo == SAB_AUTH_SNOW_UIA2 ||
         sa_params_p->auth_algo == SAB_AUTH_ZUC_EIA3) &&
        sa_params_basic_p->direction != 0)
    {
        sa_state_p->cw1 |= SAB_CW1_WIRELESS_DIR;
    }

    if (sa_params_p->crypto_algo != SAB_CRYPTO_NULL)
    {
        /* We are now doing basic encryption/decryption,
           possibly with hash.*/
        if (sa_params_p->direction == SAB_DIRECTION_OUTBOUND)
            if (sa_params_p->auth_algo == SAB_AUTH_NULL)
                sa_state_p->cw0 |= SAB_CW0_TOP_ENCRYPT;
            else if (sa_params_p->auth_algo==SAB_AUTH_AES_CCM ||
                     sa_params_p->auth_algo==SAB_AUTH_AES_GMAC)
                sa_state_p->cw0 |= SAB_CW0_TOP_HASH_ENCRYPT;
            else if ( (sa_params_basic_p->basic_flags & SAB_BASIC_FLAG_ENCRYPT_AFTER_HASH) != 0)
                sa_state_p->cw0 |= SAB_CW0_TOP_HASH_ENCRYPT;
            else
                sa_state_p->cw0 |= SAB_CW0_TOP_ENCRYPT_HASH;
        else
            if (sa_params_p->auth_algo == SAB_AUTH_NULL)
                sa_state_p->cw0 |= SAB_CW0_TOP_DECRYPT;
            else if (sa_params_p->auth_algo==SAB_AUTH_AES_CCM)
                sa_state_p->cw0 |= SAB_CW0_TOP_DECRYPT_HASH;
            else if ( (sa_params_basic_p->basic_flags & SAB_BASIC_FLAG_ENCRYPT_AFTER_HASH) != 0)
            {
                sa_state_p->cw0 |= SAB_CW0_TOP_DECRYPT_HASH;
                sa_state_p->cw1 |= SAB_CW1_PREPKT_OP;
                sa_state_p->cw1 |= SAB_CW1_PAD_TLS;
            }
            else
                sa_state_p->cw0 |= SAB_CW0_TOP_HASH_DECRYPT;

        /* Check for prohibited algorithms and crypto modes. */
        if (sa_params_p->crypto_mode == SAB_CRYPTO_MODE_CFB1 ||
            sa_params_p->crypto_mode == SAB_CRYPTO_MODE_CFB8)
        {
            LOG_CRIT("SABuilder: crypto algorithm/mode not supported\n");
            return SAB_INVALID_PARAMETER;
        }

        /* Reject crypto modes other than CBC for ENCRYPT_AFTER_HASH */
        if ( (sa_params_basic_p->basic_flags & SAB_BASIC_FLAG_ENCRYPT_AFTER_HASH) != 0 &&
            sa_params_p->crypto_mode != SAB_CRYPTO_MODE_CBC)
        {
            LOG_CRIT("SABuilder: crypto algorithm/mode not supported for encrypt after hash\n");
            return SAB_INVALID_PARAMETER;
        }

        if (sa_params_p->crypto_mode == SAB_CRYPTO_MODE_XTS ||
            sa_params_p->crypto_mode == SAB_CRYPTO_MODE_XTS_STATEFUL)
        { /* For AES-XTS, extract Key2 from nonce_p parameter,
           same size as the primary AES key. */
#ifdef SAB_STRICT_ARGS_CHECK
            if (sa_params_p->nonce_p == NULL)
            {
                LOG_CRIT("SABuilder: NULL pointer Key 2.\n");
                return SAB_INVALID_PARAMETER;
            }
#endif
            sa_builder_lib_copy_key_mat(sa_buffer_p,
                                    sa_state_p->current_offset,
                                    sa_params_p->nonce_p,
                                    sa_params_p->key_byte_count);
            sa_state_p->current_offset+=sa_state_p->cipher_key_words;
        }

        if (sa_params_p->crypto_mode == SAB_CRYPTO_MODE_F8 ||
            sa_params_p->crypto_mode == SAB_CRYPTO_MODE_UEA2 ||
            sa_params_p->crypto_mode == SAB_CRYPTO_MODE_EEA2 ||
            sa_params_p->crypto_mode == SAB_CRYPTO_MODE_EEA3)
        {
            sa_params_p->iv_src = SAB_IV_SRC_TOKEN;
        }
        else if (sa_params_p->crypto_algo != SAB_CRYPTO_ARCFOUR &&
                 sa_params_p->crypto_mode != SAB_CRYPTO_MODE_ECB &&
                 !(sa_params_p->crypto_algo==SAB_CRYPTO_KASUMI &&
                   sa_params_p->crypto_mode==SAB_CRYPTO_MODE_BASIC))
        {
            /* For ARCFOUR and block ciphers in ECB mode and
               Basic Kasumi we do not have an iv */
            if (sa_params_p->iv_src == SAB_IV_SRC_DEFAULT)
                sa_params_p->iv_src = SAB_IV_SRC_INPUT;

            if (sa_params_p->crypto_mode == SAB_CRYPTO_MODE_GCM &&
                (sa_params_p->flags & SAB_FLAG_HASH_INTERMEDIATE) != 0)
            {
                /* AES/SM4 GCM mode with intermediat4e state.
                 */
                sa_params_p->iv_src = SAB_IV_SRC_SA;
                sa_state_p->cw1 &= ~0x7; // Clear crypto mode (CTR);
                sa_state_p->cw1 |= SAB_CW1_CRYPTO_MODE_CTR_LOAD;
                sa_state_p->cw1 |= SAB_CW1_IV0 |SAB_CW1_IV1 | SAB_CW1_IV2 |
                     SAB_CW1_IV3 |
                    SAB_CW1_CRYPTO_STORE|SAB_CW1_ENCRYPT_HASHRES|
                    SAB_CW1_HASH_STORE |SAB_CW1_DIGEST_CNT;
#ifdef SAB_STRICT_ARGS_CHECK
                    if (sa_params_p->iv_p == NULL)
                    {
                        LOG_CRIT("SABuilder: NULL pointer iv.\n");
                        return SAB_INVALID_PARAMETER;
                    }
#endif
                    sa_builder_lib_zero_fill(sa_buffer_p,
                                          sa_state_p->current_offset,
                                          12 * sizeof(u32));
                    sa_state_p->current_offset+=12;
                    sa_builder_lib_copy_key_mat(sa_buffer_p,
                                     sa_state_p->current_offset,
                                     sa_params_p->iv_p,
                                     3 * sizeof(u32));
                    if (sa_buffer_p != NULL)
                    {
                        sa_buffer_p[sa_state_p->current_offset+3] = 0x01000000;
                    }
                    sa_state_p->current_offset+=4;
            }
            else if (sa_params_p->crypto_mode == SAB_CRYPTO_MODE_CTR ||
                sa_params_p->crypto_mode == SAB_CRYPTO_MODE_CCM ||
                sa_params_p->crypto_mode == SAB_CRYPTO_MODE_GMAC ||
                sa_params_p->crypto_mode == SAB_CRYPTO_MODE_GCM)
            {
                if (sa_params_p->iv_src == SAB_IV_SRC_TOKEN)
                {
                    sa_state_p->cw1 &= ~0x7; // Clear crypto mode (CTR);
                    sa_state_p->cw1 |= SAB_CW1_CRYPTO_MODE_CTR_LOAD;
                    /* When the CTR mode iv is loaded from token, then
                       load all four iv words, including block counter */
                }
                else
                {   /* else add nonce to SA */
                    sa_state_p->cw1 |= SAB_CW1_IV0;

#ifdef SAB_STRICT_ARGS_CHECK
                    if (sa_params_p->nonce_p == NULL)
                    {
                        LOG_CRIT("SABuilder: NULL pointer nonce.\n");
                        return SAB_INVALID_PARAMETER;
                    }
#endif
                    if (sa_params_p->crypto_mode == SAB_CRYPTO_MODE_CCM)
                    {
                        if (sa_buffer_p != NULL)
                            sa_buffer_p[sa_state_p->current_offset] =
                              (sa_params_p->nonce_p[0] << 8)  |
                              (sa_params_p->nonce_p[1] << 16) |
                              (sa_params_p->nonce_p[2] << 24) | SAB_CCM_FLAG_L4;
                    }
                    else
                    {
                        sa_builder_lib_copy_key_mat(sa_buffer_p,
                                                sa_state_p->current_offset,
                                                sa_params_p->nonce_p, sizeof(u32));
                    }
                    sa_state_p->current_offset +=1;
                }

                if (sa_params_p->iv_src == SAB_IV_SRC_SA)
                {
                    sa_state_p->cw1 |= SAB_CW1_IV_CTR |
                                      SAB_CW1_IV1    |
                                      SAB_CW1_IV2;
#ifdef SAB_STRICT_ARGS_CHECK
                    if (sa_params_p->iv_p == NULL)
                    {
                        LOG_CRIT("SABuilder: NULL pointer iv.\n");
                        return SAB_INVALID_PARAMETER;
                    }
#endif
                    sa_params_p->offset_iv = sa_state_p->current_offset;
                    sa_params_p->iv_word32_count = 2;

                    sa_builder_lib_copy_key_mat(sa_buffer_p,
                                            sa_state_p->current_offset,
                                            sa_params_p->iv_p, 8);
                    sa_state_p->current_offset += 2;
                }
                else
                {
                    sa_state_p->cw1 |= SAB_CW1_IV_CTR;
                }

                if (sa_params_p->iv_src != SAB_IV_SRC_TOKEN &&
                    sa_params_p->crypto_mode == SAB_CRYPTO_MODE_CCM)
                {
                    /* Add 0 counter field (IV3) */
                    sa_state_p->cw1 |= SAB_CW1_IV3;
                    if(sa_buffer_p != NULL)
                        sa_buffer_p[sa_state_p->current_offset] = 0;
                    sa_state_p->current_offset+=1;
                }
            }
            else
            {
                if (sa_params_p->crypto_mode == SAB_CRYPTO_MODE_ICM)
                {
                    sa_state_p->cw1 &= ~0x7; // Clear crypto mode (CTR or ICM);
                    sa_state_p->cw1 |= SAB_CW1_CRYPTO_MODE_CTR_LOAD;
                    /* When the CTR mode iv is loaded from token, then
                       load all four iv words, including block counter */
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
                    if (sa_params_p->crypto_algo == SAB_CRYPTO_DES ||
                        sa_params_p->crypto_algo == SAB_CRYPTO_3DES ||
                        sa_params_p->crypto_algo == SAB_CRYPTO_AES ||
                        sa_params_p->crypto_algo == SAB_CRYPTO_SM4 ||
                        sa_params_p->crypto_algo == SAB_CRYPTO_BC0)
                      sa_state_p->cw1 |= SAB_CW1_CRYPTO_STORE;
                }
            }
        }
    }
    else
    {
        /* Bypass operation or authenticate-only,
           use inbound direction when verifying */
        if (sa_params_p->auth_algo == SAB_AUTH_NULL)
            sa_state_p->cw0 |= SAB_CW0_TOP_NULL_OUT;
        else if ((sa_params_basic_p->basic_flags & SAB_BASIC_FLAG_EXTRACT_ICV) !=0)
            sa_state_p->cw0 |= SAB_CW0_TOP_HASH_IN;
        else
            sa_state_p->cw0 |= SAB_CW0_TOP_HASH_OUT;
    }

    return SAB_STATUS_OK;
}

#endif /* SAB_ENABLE_PROTO_BASIC */

/* end of file sa_builder_basic.c */
