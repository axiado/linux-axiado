/* sa_builder_ipsec.c
 *
 * IPsec specific functions (for initialization of sa_builder_params_t
 * structures and for building the IPSec specifc part of an SA.).
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
#include "sa_builder_ipsec.h"
#include "sa_builder_internal.h" /* SABuilder_SetIpsecParams */

/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */
#include "c_sa_builder.h"
#include "basic_defs.h"
#include "log.h"

#ifdef SAB_ENABLE_PROTO_IPSEC


/*----------------------------------------------------------------------------
 * Definitions and macros
 */
#ifdef SAB_ENABLE_1024BIT_SEQMASK
#define SAB_SEQUENCE_MAXBITS 1024
#elif defined(SAB_ENABLE_384BIT_SEQMASK)
#define SAB_SEQUENCE_MAXBITS 384
#else
#define SAB_SEQUENCE_MAXBITS 128
#endif

/*----------------------------------------------------------------------------
 * Local variables
 */

/*----------------------------------------------------------------------------
 * sa_builder_init_esp
 *
 * This function initializes the sa_builder_params_t data structure and its
 * sa_builder_params_ipsec_t extension with sensible defaults for ESP
 * processing.
 *
 * sa_params_p (output)
 *   Pointer to SA parameter structure to be filled in.
 * sa_params_ipsec_p (output)
 *   Pointer to IPsec parameter extension to be filled in
 * spi (input)
 *   spi of the newly created parameter structure (must not be zero).
 * tunnel_transport (input)
 *   Must be one of SAB_IPSEC_TUNNEL or SAB_IPSEC_TRANSPORT.
 * ip_mode (input)
 *   Must be one of SAB_IPSEC_IPV4 or SAB_IPSEC_IPV6.
 * direction (input)
 *   Must be one of SAB_DIRECTION_INBOUND or SAB_DIRECTION_OUTBOUND.
 *
 * Both the crypto and the authentication algorithm are initialized to
 * NULL, which is illegal according to the IPsec standards, but it is
 * possible to use this setting for debug purposes.
 *
 * Both the sa_params_p and sa_params_ipsec_p input parameters must point
 * to valid storage where variables of the appropriate type can be
 * stored. This function initializes the link from sa_params_p to
 * sa_params_ipsec_p.
 *
 * Return:
 * SAB_STATUS_OK on success
 * SAB_INVALID_PARAMETER when one of the pointer parameters is NULL
 *   or the remaining parameters have illegal values.
 */
sa_builder_status_t
sa_builder_init_esp(
    sa_builder_params_t * const sa_params_p,
    sa_builder_params_ipsec_t * const sa_params_ipsec_p,
    const u32 spi,
    const u32 tunnel_transport,
    const u32 ip_mode,
    const sa_builder_direction_t direction)
{
    int i;
#ifdef SAB_STRICT_ARGS_CHECK
    if (sa_params_p == NULL || sa_params_ipsec_p == NULL)
    {
        LOG_CRIT("sa_builder_init_esp: NULL pointer parameter supplied.\n");
        return SAB_INVALID_PARAMETER;
    }

    if (spi == 0)
    {
        LOG_CRIT("sa_builder_init_esp: spi may not be 0.\n");
        return SAB_INVALID_PARAMETER;
    }

    if (tunnel_transport != SAB_IPSEC_TUNNEL &&
        tunnel_transport != SAB_IPSEC_TRANSPORT)
    {
        LOG_CRIT("sa_builder_init_esp: Invalid tunnel_transport.\n");
        return SAB_INVALID_PARAMETER;
    }

    if (ip_mode != SAB_IPSEC_IPV4 && ip_mode != SAB_IPSEC_IPV6)
    {
        LOG_CRIT("sa_builder_init_esp: Invalid ip_mode.\n");
        return SAB_INVALID_PARAMETER;
    }

    if (direction != SAB_DIRECTION_OUTBOUND &&
        direction != SAB_DIRECTION_INBOUND)
    {
        LOG_CRIT("sa_builder_init_esp: Invalid direction.\n");
        return SAB_INVALID_PARAMETER;
    }
#endif

    sa_params_p->protocol = SAB_PROTO_IPSEC;
    sa_params_p->direction = direction;
    sa_params_p->protocol_extension_p = (void*)sa_params_ipsec_p;
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

    sa_params_ipsec_p->spi = spi;
    sa_params_ipsec_p->ipsec_flags = SAB_IPSEC_ESP | tunnel_transport | ip_mode;
    sa_params_ipsec_p->seq_num = 0;
    sa_params_ipsec_p->seq_num_hi = 0;
    sa_params_ipsec_p->seq_mask[0] = 1;
    for (i=1; i<SA_SEQ_MASK_WORD_COUNT; i++)
        sa_params_ipsec_p->seq_mask[i] = 0;
    sa_params_ipsec_p->pad_alignment = 0;
    sa_params_ipsec_p->icv_byte_count = 0;
    sa_params_ipsec_p->src_ip_addr_p = NULL;
    sa_params_ipsec_p->dest_ip_addr_p = NULL;
    sa_params_ipsec_p->orig_src_ip_addr_p = NULL;
    sa_params_ipsec_p->orig_dest_ip_addr_p = NULL;
    sa_params_ipsec_p->natt_src_port = 4500;
    sa_params_ipsec_p->natt_dest_port = 4500;
    sa_params_ipsec_p->context_ref = 0;
    sa_params_ipsec_p->ttl = 240;
    sa_params_ipsec_p->dscp = 0;
    sa_params_ipsec_p->sequence_mask_bit_count = 0;
    return SAB_STATUS_OK;
}


/*----------------------------------------------------------------------------
 * sa_builder_set_ipsec_params
 *
 * Fill in IPsec-specific extensions into the SA.
 *
 * sa_params_p (input, updated)
 *   The SA parameters structure from which the SA is derived.
 * sa_state_p (input, output)
 *   Variables containing information about the SA being generated/
 * sa_buffer_p (output).
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
                         u32 * const sa_buffer_p)
{
    unsigned int iv_offset = 0;
    sa_builder_params_ipsec_t *sa_params_ipsec_p;
    bool f_fixed_seq_offset = false;
    sa_params_ipsec_p = (sa_builder_params_ipsec_t *)
        (sa_params_p->protocol_extension_p);
    if (sa_params_ipsec_p == NULL)
    {
        LOG_CRIT("SABuilder: IPsec extension pointer is null\n");
        return SAB_INVALID_PARAMETER;
    }

    /* First check whether AH or ESP flags are correct */

    if ( (sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_AH) != 0)
    {
#ifdef SAB_ENABLE_IPSEC_AH
        if (sa_params_p->crypto_algo != SAB_CRYPTO_NULL ||
            (sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_ESP) != 0)
        {
            LOG_CRIT("SABuilder: AH does not support crypto.\n");
            return SAB_INVALID_PARAMETER;
        }
#else
        LOG_CRIT("SABuilder: AH unsupported..\n");
        return SAB_INVALID_PARAMETER;
#endif
    }

#ifndef SAB_ENABLE_IPSEC_ESP
    if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_ESP) != 0)
    {
        LOG_CRIT("SABuilder: ESP unsupported.\n");
        return SAB_INVALID_PARAMETER;
    }
#endif

    /* Check for supported algorithms and crypto modes in IPsec */
    if ((sa_params_p->crypto_algo != SAB_CRYPTO_NULL &&
         sa_params_p->crypto_algo != SAB_CRYPTO_DES &&
         sa_params_p->crypto_algo != SAB_CRYPTO_3DES &&
         sa_params_p->crypto_algo != SAB_CRYPTO_AES &&
         sa_params_p->crypto_algo != SAB_CRYPTO_CHACHA20 &&
         sa_params_p->crypto_algo != SAB_CRYPTO_SM4 &&
         sa_params_p->crypto_algo != SAB_CRYPTO_BC0) ||
        (sa_params_p->crypto_algo != SAB_CRYPTO_NULL && (
            sa_params_p->crypto_mode != SAB_CRYPTO_MODE_CBC &&
            sa_params_p->crypto_mode != SAB_CRYPTO_MODE_CTR &&
            sa_params_p->crypto_mode != SAB_CRYPTO_MODE_GCM &&
            sa_params_p->crypto_mode != SAB_CRYPTO_MODE_GMAC &&
            sa_params_p->crypto_mode != SAB_CRYPTO_MODE_CCM &&
            sa_params_p->crypto_mode != SAB_CRYPTO_MODE_CHACHA_CTR32)))
    {
        LOG_CRIT("SABuilder: IPsec: crypto algorithm/mode not supported\n");
        return SAB_INVALID_PARAMETER;
    }

    /* Check for supported authentication algorithms in IPsec */
    if (sa_params_p->auth_algo != SAB_AUTH_NULL &&
        sa_params_p->auth_algo != SAB_AUTH_HMAC_MD5 &&
        sa_params_p->auth_algo != SAB_AUTH_HMAC_SHA1 &&
        sa_params_p->auth_algo != SAB_AUTH_HMAC_SHA2_256 &&
        sa_params_p->auth_algo != SAB_AUTH_HMAC_SHA2_384 &&
        sa_params_p->auth_algo != SAB_AUTH_HMAC_SHA2_512 &&
        sa_params_p->auth_algo != SAB_AUTH_AES_XCBC_MAC &&
        sa_params_p->auth_algo != SAB_AUTH_AES_CMAC_128 &&
        sa_params_p->auth_algo != SAB_AUTH_AES_GCM &&
        sa_params_p->auth_algo != SAB_AUTH_AES_GMAC &&
        sa_params_p->auth_algo != SAB_AUTH_AES_CCM &&
        sa_params_p->auth_algo != SAB_AUTH_POLY1305 &&
        sa_params_p->auth_algo != SAB_AUTH_HMAC_SM3)
    {
        LOG_CRIT("SABuilder: IPsec: auth algorithm not supported\n");
        return SAB_INVALID_PARAMETER;
    }

    /* Add spi to SA record */
    if (sa_buffer_p != NULL)
        sa_buffer_p[sa_state_p->current_offset] = sa_params_ipsec_p->spi;
    sa_state_p->current_offset += 1;

    /* Determine whether we will have a fixed sequence number offset */
    if (sa_params_p->direction == SAB_DIRECTION_INBOUND)
    {
        /* Determine size of sequence number mask in bits */
        if (sa_params_ipsec_p->sequence_mask_bit_count == 0)
        {
            /* Some flags indicate specific mask sizes */
            if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_MASK_384) != 0)
            {
                sa_params_ipsec_p->sequence_mask_bit_count = 384;
            }
            else if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_MASK_256) != 0)
            {
                sa_params_ipsec_p->sequence_mask_bit_count = 256;
            }
            else if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_MASK_128) != 0)
            {
                sa_params_ipsec_p->sequence_mask_bit_count = 128;
            }
            else if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_MASK_32) != 0)
            {
                sa_params_ipsec_p->sequence_mask_bit_count = 32;
            }
            else
            {
                sa_params_ipsec_p->sequence_mask_bit_count = 64;
            }
        }
        if (sa_params_ipsec_p->sequence_mask_bit_count > SAB_SEQUENCE_MAXBITS ||
            (sa_params_ipsec_p->sequence_mask_bit_count & 0x1f) != 0)
        {
            LOG_CRIT("SABuilder: Illegal sequence mask size.\n");
            return SAB_INVALID_PARAMETER;
        }
#ifdef SAB_ENABLE_DEFAULT_FIXED_OFFSETS
        f_fixed_seq_offset = true;
#else
        if (sa_params_ipsec_p->sequence_mask_bit_count > 128 ||
            (sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_FIXED_SEQ_OFFSET) != 0)
        {
            f_fixed_seq_offset = true;
        }
#endif
        if (sa_params_ipsec_p->sequence_mask_bit_count ==32)
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

        if (sa_buffer_p != NULL)
            sa_buffer_p[sa_state_p->current_offset] = sa_params_ipsec_p->seq_num;

        if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_LONG_SEQ) != 0)
        {
            if (sa_buffer_p != NULL)
                sa_buffer_p[sa_state_p->current_offset+1] = sa_params_ipsec_p->seq_num_hi;
            sa_state_p->cw0 |= SAB_CW0_SPI | SAB_CW0_SEQNUM_64_FIX;
            sa_params_p->seq_num_word32_count = 2;
        }
        else
        {
            sa_state_p->cw0 |= SAB_CW0_SPI | SAB_CW0_SEQNUM_32_FIX;
            sa_params_p->seq_num_word32_count = 1;
        }
        // Always reserve 2 words for the sequence number.
        sa_state_p->current_offset += 2;
    }
    else
    {
        /* Add sequence number */
        sa_params_p->offset_seq_num = sa_state_p->current_offset;

        if (sa_buffer_p != NULL)
            sa_buffer_p[sa_state_p->current_offset] = sa_params_ipsec_p->seq_num;
        sa_state_p->current_offset += 1;
        if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_LONG_SEQ) != 0)
        {
            if (sa_buffer_p != NULL)
                sa_buffer_p[sa_state_p->current_offset] = sa_params_ipsec_p->seq_num_hi;
            sa_state_p->cw0 |= SAB_CW0_SPI | SAB_CW0_SEQNUM_64;
            sa_state_p->current_offset += 1;

            sa_params_p->seq_num_word32_count = 2;
        }
        else
        {
            sa_state_p->cw0 |= SAB_CW0_SPI | SAB_CW0_SEQNUM_32;
            sa_params_p->seq_num_word32_count = 1;
        }
    }

    if (sa_params_p->direction == SAB_DIRECTION_OUTBOUND)
    {

        if (sa_params_p->crypto_algo==SAB_CRYPTO_NULL &&
            sa_params_p->auth_algo==SAB_AUTH_NULL)
            sa_state_p->cw0 |= SAB_CW0_TOP_NULL_OUT;
        else if (sa_params_p->crypto_algo==SAB_CRYPTO_NULL)
            sa_state_p->cw0 |= SAB_CW0_TOP_HASH_OUT;
        else if (sa_params_p->auth_algo==SAB_AUTH_NULL)
            sa_state_p->cw0 |= SAB_CW0_TOP_ENCRYPT;
        else if (sa_params_p->auth_algo==SAB_AUTH_AES_CCM ||
                 sa_params_p->auth_algo==SAB_AUTH_AES_GMAC)
            sa_state_p->cw0 |= SAB_CW0_TOP_HASH_ENCRYPT;
        else
            sa_state_p->cw0 |= SAB_CW0_TOP_ENCRYPT_HASH;

        /* Some versions of the hardware can update the sequence number
           early, so multiple engines can operate in parallel. */
        sa_state_p->cw1 |= SAB_CW1_EARLY_SEQNUM_UPDATE;
        sa_state_p->cw1 |= sa_params_p->offset_seq_num << 24;

        sa_state_p->cw1 |= SAB_CW1_SEQNUM_STORE;
        if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_NO_ANTI_REPLAY)!=0 &&
            (sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_LONG_SEQ) == 0)
        {
            /* Disable outbound sequence number rollover checking by putting
               an 64-sequence number in the SA. This will not be
               used in authentication (no ESN) */
            if (sa_buffer_p != NULL)
                sa_buffer_p[sa_state_p->current_offset] = 0;
            sa_state_p->cw0 |= SAB_CW0_SEQNUM_64;
            sa_state_p->current_offset += 1;
        }

        /* Take care of iv and nonce */
        if (sa_params_p->crypto_mode == SAB_CRYPTO_MODE_CTR ||
            sa_params_p->crypto_mode == SAB_CRYPTO_MODE_GCM ||
            sa_params_p->crypto_mode == SAB_CRYPTO_MODE_GMAC ||
            sa_params_p->crypto_mode == SAB_CRYPTO_MODE_CCM ||
            sa_params_p->crypto_mode == SAB_CRYPTO_MODE_CHACHA_CTR32)
        {
            if (sa_params_p->iv_src == SAB_IV_SRC_DEFAULT)
                sa_params_p->iv_src = SAB_IV_SRC_SEQ;

            /* Add nonce, always present */
            sa_state_p->cw1 |= SAB_CW1_IV0;
            if (sa_buffer_p != NULL)
            {
                if (sa_params_p->nonce_p == NULL)
                {
                    return SAB_INVALID_PARAMETER;
                }
                if (sa_params_p->crypto_mode == SAB_CRYPTO_MODE_CCM)
                    sa_buffer_p[sa_state_p->current_offset] =
                        (sa_params_p->nonce_p[0] << 8)  |
                        (sa_params_p->nonce_p[1] << 16) |
                        (sa_params_p->nonce_p[2] << 24) | SAB_CCM_FLAG_L4;
                else
                    sa_builder_lib_copy_key_mat(sa_buffer_p,
                                            sa_state_p->current_offset,
                                            sa_params_p->nonce_p,
                                            sizeof(u32));
            }
            sa_state_p->current_offset +=1;

            if (sa_params_p->iv_src == SAB_IV_SRC_SEQ)
            {
                sa_state_p->cw1 |= SAB_CW1_IV_ORIG_SEQ;
            }
            else if (sa_params_p->iv_src == SAB_IV_SRC_IMPLICIT)
            {
                sa_state_p->cw1 |= SAB_CW1_IV_INCR_SEQ;
            }
            else if (sa_params_p->iv_src == SAB_IV_SRC_XORSEQ)
            {
                if (sa_params_p->iv_p == NULL)
                {
                    return SAB_INVALID_PARAMETER;
                }
                sa_state_p->cw1 |= SAB_CW1_IV_CTR|SAB_CW1_CRYPTO_NONCE_XOR;
                sa_state_p->cw1 |= SAB_CW1_IV0|SAB_CW1_IV1|SAB_CW1_IV2;
                sa_builder_lib_copy_key_mat(sa_buffer_p,
                                        sa_state_p->current_offset,
                                        sa_params_p->iv_p,
                                        2*sizeof(u32));
                sa_state_p->current_offset +=2;
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
            if (sa_params_p->crypto_mode == SAB_CRYPTO_MODE_CCM)
            {
                /* Add 0 counter field (IV3) */
                sa_state_p->cw1 |= SAB_CW1_IV3;
                if(sa_buffer_p != NULL)
                    sa_buffer_p[sa_state_p->current_offset] = 0;
                sa_state_p->current_offset+=1;
            }
        }
        else if (sa_state_p->iv_words > 0)
        { /* CBC mode, non-null */
            if (sa_params_p->iv_src == SAB_IV_SRC_DEFAULT)
                sa_params_p->iv_src = SAB_IV_SRC_PRNG;
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
        unsigned int input_mask_word_count =
            sa_params_ipsec_p->sequence_mask_bit_count / 32;
        unsigned int alloc_mask_word_count;

        if (sa_params_p->crypto_algo==SAB_CRYPTO_NULL &&
            sa_params_p->auth_algo==SAB_AUTH_NULL)
            sa_state_p->cw0 |= SAB_CW0_TOP_NULL_IN;
        else if (sa_params_p->crypto_algo==SAB_CRYPTO_NULL)
            sa_state_p->cw0 |= SAB_CW0_TOP_HASH_IN;
        else if (sa_params_p->auth_algo==SAB_AUTH_NULL)
            sa_state_p->cw0 |= SAB_CW0_TOP_DECRYPT;
        else if (sa_params_p->auth_algo==SAB_AUTH_AES_CCM)
            sa_state_p->cw0 |= SAB_CW0_TOP_DECRYPT_HASH;
        else
            sa_state_p->cw0 |= SAB_CW0_TOP_HASH_DECRYPT;

        sa_state_p->cw1 |= SAB_CW1_PAD_IPSEC;

        /* Add sequence mask  Always add one even with no anti-replay*/
        sa_params_p->offset_seq_mask = sa_state_p->current_offset;

        if ((sa_params_ipsec_p->ipsec_flags & SAB_IPSEC_APPEND_SEQNUM) != 0)
            sa_state_p->cw0 |= SAB_CW0_SEQNUM_APPEND;

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
        else if (input_mask_word_count <= 12)
        {
            alloc_mask_word_count = 12;
            sa_state_p->cw0 |= SAB_CW0_MASK_384_FIX;
        }
        else
        {
            alloc_mask_word_count = 32;
            sa_state_p->cw0 |= SAB_CW0_MASK_1024_FIX;
            sa_state_p->f_large_mask = true;
            sa_state_p->f_large = true;
        }
        if(sa_buffer_p != NULL)
        {
            unsigned int i;
            if (alloc_mask_word_count <= SA_SEQ_MASK_WORD_COUNT)
            {
                for (i = 0; i < input_mask_word_count; i++)
                    sa_buffer_p[sa_state_p->current_offset+i] =
                        sa_params_ipsec_p->seq_mask[i];
                /* If the input mask is smaller than the one picked by the
                   hardware, fill the remaining words with all-one, the
                   hardware will treat these words as invalid.
                */
                for (i= input_mask_word_count; i < alloc_mask_word_count; i++)
                    sa_buffer_p[sa_state_p->current_offset+i] = 0xffffffff;
            }
            else
            {
                /* mask too big to store in parameter structure.
                   Also need to shift the '1' bit to correct position */
                u32 word_idx, bit_mask;
                for (i= 0; i < alloc_mask_word_count; i++)
                    sa_buffer_p[sa_state_p->current_offset+i] = 0;
                word_idx = (sa_params_ipsec_p->seq_num & MASK_10_BITS) >> 5;
                bit_mask = 1 << (sa_params_ipsec_p->seq_num & MASK_5_BITS);
                sa_buffer_p[sa_state_p->current_offset+word_idx] = bit_mask;
            }
        }
        sa_state_p->current_offset += alloc_mask_word_count;
        sa_params_p->seq_mask_word32_count = input_mask_word_count;

        /* Add nonce for CTR and related modes */
        if (sa_params_p->crypto_mode == SAB_CRYPTO_MODE_CTR ||
            sa_params_p->crypto_mode == SAB_CRYPTO_MODE_GCM ||
            sa_params_p->crypto_mode == SAB_CRYPTO_MODE_GMAC ||
            sa_params_p->crypto_mode == SAB_CRYPTO_MODE_CCM||
            sa_params_p->crypto_mode == SAB_CRYPTO_MODE_CHACHA_CTR32)
        {
            if (iv_offset == 0)
                iv_offset = sa_state_p->current_offset;

            sa_state_p->cw1 |= SAB_CW1_IV0;

            /* For Poly/Chacha, we need to run in XOR iv mode with
              delayed OTK in order to make the OTK derivation from the
              extracted iv work */
            if (sa_params_p->crypto_mode == SAB_CRYPTO_MODE_CHACHA_CTR32)
            {
                sa_state_p->cw1 |= SAB_CW1_CRYPTO_NONCE_XOR|
                    SAB_CW1_CRYPTO_MODE_CHACHA_POLY_OTK;
                /* need all 3 iv double words - for iv=seqno these need to be
                   zeroized */
                sa_state_p->cw1 |= SAB_CW1_IV1|SAB_CW1_IV2;
                if (sa_params_p->iv_src == SAB_IV_SRC_IMPLICIT)
                    sa_state_p->cw1 |= SAB_CW1_IV_CTR;
            }
            else if (sa_params_p->iv_src == SAB_IV_SRC_IMPLICIT)
                sa_state_p->cw1 |= SAB_CW1_IV_ORIG_SEQ;

            if (sa_buffer_p != NULL)
            {
                if (sa_params_p->nonce_p == NULL)
                {
                    return SAB_INVALID_PARAMETER;
                }
                if (sa_params_p->crypto_mode == SAB_CRYPTO_MODE_CCM)
                {
                    sa_buffer_p[iv_offset] =
                        (sa_params_p->nonce_p[0] << 8)  |
                        (sa_params_p->nonce_p[1] << 16) |
                        (sa_params_p->nonce_p[2] << 24) | SAB_CCM_FLAG_L4;
                }
                else
                    sa_builder_lib_copy_key_mat(sa_buffer_p,
                                            iv_offset,
                                            sa_params_p->nonce_p,
                                            sizeof(u32));
            }
            iv_offset += 1;
            if (sa_params_p->crypto_mode == SAB_CRYPTO_MODE_CCM)
            {
                /* Add 0 counter field (IV3) */
                sa_state_p->cw1 |= SAB_CW1_IV3;
                if(sa_buffer_p != NULL)
                    sa_buffer_p[iv_offset] = 0;
                iv_offset += 1;
            }
            else if (sa_params_p->crypto_mode == SAB_CRYPTO_MODE_CHACHA_CTR32)
            {
                /* For ChaCha20 the IV1 and IV2 words are required to be 0 */
                sa_builder_lib_zero_fill(sa_buffer_p, iv_offset, 2*sizeof(u32));
                iv_offset +=2;
            }
            if (iv_offset > sa_state_p->current_offset)
            {
                sa_state_p->current_offset = iv_offset;
            }
         }
    }
    return SAB_STATUS_OK;
}


#endif /* SAB_ENABLE_PROTO_IPSEC */

/* end of file sa_builder_ipsec.c */
