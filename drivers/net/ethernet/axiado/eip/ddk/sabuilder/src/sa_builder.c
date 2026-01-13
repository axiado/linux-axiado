/* sa_builder.c
 *
 * Main implementation file of the EIP-96 SA builder.
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
#include "sa_builder.h"

/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */
#include "c_sa_builder.h"
#include "basic_defs.h"
#include "log.h"
#include "sa_builder_internal.h"

#ifdef SAB_AUTO_TOKEN_CONTEXT_GENERATION
#include "token_builder.h"
#endif

#if defined(SAB_ENABLE_IPSEC_EXTENDED) || defined(SAB_ENABLE_BASIC_EXTENDED)
#include "sa_builder_extended_internal.h"
#include "firmware_eip207_api_flow_cs.h"
#endif


/*----------------------------------------------------------------------------
 * Local variables
 */
#if defined(SAB_ENABLE_IPSEC_EXTENDED) || defined(SAB_ENABLE_SSLTLS_EXTENDED)
unsigned int large_transform_offset = SAB_LARGE_TRANSFORM_OFFSET;
#else
#define large_transform_offset 16
#endif

/*----------------------------------------------------------------------------
 * sa_builder_lib_copy_key_mat
 *
 * Copy a key into the SA.
 *
 * destination_p (input)
 *   Destination (word-aligned) of the SA record.
 *
 * offset (input)
 *   Word offset of the key in the SA record where it must be stored.
 *
 * source_p (input)
 *   source (byte aligned) of the data.
 *
 * key_byte_count (input)
 *   size of the key in bytes.
 *
 * destination_p is allowed to be a null pointer, in which case no key
 * will be written.
 */
void
sa_builder_lib_copy_key_mat(
        u32 * const destination_p,
        const unsigned int offset,
        const u8 * const source_p,
        const unsigned int key_byte_count)
{
    u32 *dst = destination_p + offset;
    const u8 *src = source_p;
    unsigned int i,j;
    u32 w;
    if (destination_p == NULL)
        return;
    for(i=0; i < key_byte_count / sizeof(u32); i++)
    {
        w=0;
        for(j=0; j<sizeof(u32); j++)
            w=(w>>8)|(*src++ << 24);
        *dst++ = w;
    }
    if ((key_byte_count % sizeof(u32)) != 0)
    {
        w=0;
        for(j=0; j<key_byte_count % sizeof(u32); j++)
        {
            w = w | (*src++ << (j*8));
        }
        *dst++ = w;
    }
}


/*----------------------------------------------------------------------------
 * sa_builder_lib_copy_key_mat_swap
 *
 * Copy a key into the SA with the words byte-swapped.
 *
 * destination_p (input)
 *   Destination (word-aligned) to store the data.
 *
 * offset (input)
 *   Word offset of the key in the SA record where it must be stored.
 *
 * source_p (input)
 *   source (byte aligned) of the data.
 *
 * key_byte_count (input)
 *   size of the key in bytes.
 *
 * destination_p is allowed to be a null pointer, in which case no key
 * will be written.
 */
void
sa_builder_lib_copy_key_mat_swap(
        u32 * const destination_p,
        const unsigned int offset,
        const u8 * const source_p,
        const unsigned int key_byte_count)
{
    u32 *dst = destination_p + offset;
    const u8 *src = source_p;
    unsigned int i,j;
    u32 w;

    if (destination_p == NULL)
        return;

    for(i=0; i < key_byte_count  / sizeof(u32); i++)
    {
        w=0;

        for(j=0; j<sizeof(u32); j++)
            w=(w<<8)|(*src++);

        *dst++ = w;
    }
    if ((key_byte_count % sizeof(u32)) != 0)
    {
        w=0;
        for(j=0; j<key_byte_count % sizeof(u32); j++)
        {
            w = w | (*src++ << (24 - j*8));
        }
        *dst++ = w;
    }
}


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
        const unsigned int byte_count)
{
    u32 *dst = destination_p + offset;
    unsigned int i;
    if (destination_p == NULL)
        return;
    for(i=0; i < (byte_count + sizeof(u32) - 1) / sizeof(u32); i++)
    {
        *dst++ = 0;
    }
}

#ifdef SAB_ARC4_STATE_IN_SA
/*----------------------------------------------------------------------------
 * sa_builder_align_for_a_rc4_state
 *
 * Align current_offset to the alignment as specified by
 * SAB_ARC4_STATE_ALIGN_BYTE_COUNT
 */
static unsigned int
sa_builder_align_for_a_rc4_state(
        unsigned int current_offset)
{
#if SAB_ARC4_STATE_ALIGN_BYTE_COUNT <= 4
    return current_offset;
#else
    u32 AlignMask = (SAB_ARC4_STATE_ALIGN_BYTE_COUNT >> 2) - 1;
    return (current_offset + AlignMask) & ~AlignMask;
#endif
}
#endif


/*----------------------------------------------------------------------------
 * sa_builder_set_cipher_keys
 *
 * Fill in cipher keys and associated command word fields in SA.
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
static sa_builder_status_t
sa_builder_set_cipher_keys(
        sa_builder_params_t *const sa_params_p,
        sa_builder_state_t * const sa_state_p,
        u32 * const sa_buffer_p)
{
    /* Fill in crypto-algorithm specific parameters */
    switch (sa_params_p->crypto_algo)
    {
    case SAB_CRYPTO_NULL: /* Null crypto, do nothing */
        break;
#ifdef SAB_ENABLE_CRYPTO_3DES
    case SAB_CRYPTO_DES:
        sa_state_p->cipher_key_words = 2;
        sa_state_p->iv_words = 2;
        sa_state_p->cw0 |= SAB_CW0_CRYPTO_DES;
        break;
    case SAB_CRYPTO_3DES:
        sa_state_p->cipher_key_words = 6;
        sa_state_p->iv_words = 2;
        sa_state_p->cw0 |= SAB_CW0_CRYPTO_3DES;
        break;
#endif
#ifdef SAB_ENABLE_CRYPTO_AES
    case SAB_CRYPTO_AES:
        switch (sa_params_p->key_byte_count)
        {
        case 16:
            sa_state_p->cw0 |= SAB_CW0_CRYPTO_AES_128;
            sa_state_p->cipher_key_words = 4;
            break;
        case 24:
            sa_state_p->cw0 |= SAB_CW0_CRYPTO_AES_192;
            sa_state_p->cipher_key_words = 6;
            break;
        case 32:
            sa_state_p->cw0 |= SAB_CW0_CRYPTO_AES_256;
            sa_state_p->cipher_key_words = 8;
            break;
        default:
            LOG_CRIT("SABuilder: Bad key size for AES.\n");
            return SAB_INVALID_PARAMETER;
        }
        sa_state_p->iv_words = 4;
        break;
#endif
#ifdef SAB_ENABLE_CRYPTO_ARCFOUR
    case SAB_CRYPTO_ARCFOUR:
        sa_state_p->cw0 |= SAB_CW0_CRYPTO_ARC4;
        if (sa_params_p->key_byte_count < 5 ||
            sa_params_p->key_byte_count > 16)
        {
            LOG_CRIT("SABuilder: Bad key size for ARCFOUR.\n");
            return SAB_INVALID_PARAMETER;
        }
        sa_state_p->cw1 |= sa_params_p->key_byte_count;
        sa_state_p->cipher_key_words =
            ((unsigned int)sa_params_p->key_byte_count + sizeof(u32) - 1) /
            sizeof(u32);

        if (sa_params_p->crypto_mode != SAB_CRYPTO_MODE_STATELESS)
        {
            sa_state_p->arc4_state = true;
            sa_state_p->cw1 |= SAB_CW1_ARC4_IJ_PTR | SAB_CW1_ARC4_STATE_SEL |
                SAB_CW1_CRYPTO_STORE;
        }
        break;
#endif
#ifdef SAB_ENABLE_CRYPTO_SM4
    case SAB_CRYPTO_SM4:
        sa_state_p->cipher_key_words = 4;
        sa_state_p->iv_words = 4;
        sa_state_p->cw0 |= SAB_CW0_CRYPTO_SM4;
        break;
#endif
#ifdef SAB_ENABLE_CRYPTO_BC0
    case SAB_CRYPTO_BC0:
        sa_state_p->cipher_key_words = 8;
        sa_state_p->iv_words = 4;
        sa_state_p->cw0 |= SAB_CW0_CRYPTO_BC0 +
            ((sa_params_p->crypto_parameter & 0x3) << 17) ;
        sa_state_p->cw1 |= SAB_CW1_EXT_CIPHER_SET;
        break;
#endif
#ifdef SAB_ENABLE_CRYPTO_KASUMI
    case SAB_CRYPTO_KASUMI:
        sa_state_p->cipher_key_words = 4;
        sa_state_p->iv_words = 0;
        sa_state_p->cw0 |= SAB_CW0_CRYPTO_KASUMI;
        break;
#endif
#ifdef SAB_ENABLE_CRYPTO_SNOW
    case SAB_CRYPTO_SNOW:
        sa_state_p->cipher_key_words = 4;
        sa_state_p->iv_words = 4;
        sa_state_p->cw0 |= SAB_CW0_CRYPTO_SNOW;
        break;
#endif
#ifdef SAB_ENABLE_CRYPTO_ZUC
    case SAB_CRYPTO_ZUC:
        sa_state_p->cipher_key_words = 4;
        sa_state_p->iv_words = 4;
        sa_state_p->cw0 |= SAB_CW0_CRYPTO_ZUC;
        break;
#endif
#ifdef SAB_ENABLE_CRYPTO_CHACHAPOLY
    case SAB_CRYPTO_CHACHA20:
        sa_state_p->cw0 |= SAB_CW0_CRYPTO_CHACHA20;
        switch (sa_params_p->key_byte_count)
        {
        case 16:
            sa_state_p->cw1 |= SAB_CW1_CRYPTO_MODE_CHACHA128;
            sa_state_p->cipher_key_words = 4;
            break;
        case 32:
            sa_state_p->cw1 |= SAB_CW1_CRYPTO_MODE_CHACHA256;
            sa_state_p->cipher_key_words = 8;
            break;
        default:
            LOG_CRIT("SABuilder: Bad key size for Chacha20.\n");
            return SAB_INVALID_PARAMETER;
        }
        break;
#endif
    default:
        LOG_CRIT("SABuilder: Unsupported crypto algorithm\n");
        return SAB_UNSUPPORTED_FEATURE;
    }

    /* Check block cipher length against provided key  */
    if (sa_params_p->crypto_algo != SAB_CRYPTO_ARCFOUR &&
        sa_state_p->cipher_key_words*sizeof(u32) != sa_params_p->key_byte_count)
    {
        LOG_CRIT("SABuilder: Bad cipher key size..\n");
        return SAB_INVALID_PARAMETER;
    }

#ifdef SAB_STRICT_ARGS_CHECK
    if ( sa_state_p->cipher_key_words > 0 && sa_params_p->key_p == NULL)
    {
        LOG_CRIT("SABuilder: NULL pointer for key_p.\n");
        return SAB_INVALID_PARAMETER;
    }
#endif

#ifdef SAB_ENABLE_CRYPTO_CHACHAPOLY
    if (sa_params_p->crypto_algo == SAB_CRYPTO_CHACHA20 &&
        sa_params_p->key_byte_count == 16)
    {
        /* Copy the key twice for 128-bit Chacha20 */
        sa_builder_lib_copy_key_mat(sa_buffer_p, sa_state_p->current_offset,
                                sa_params_p->key_p, sa_params_p->key_byte_count);
        sa_state_p->current_offset += sa_state_p->cipher_key_words;
    }
#endif
    /* Copy the cipher key */
    sa_builder_lib_copy_key_mat(sa_buffer_p, sa_state_p->current_offset,
                            sa_params_p->key_p, sa_params_p->key_byte_count);

    if (sa_params_p->crypto_algo == SAB_CRYPTO_ARCFOUR)
        sa_state_p->current_offset += 4; /* Always use 4 words for key in arc4*/
    else
        sa_state_p->current_offset += sa_state_p->cipher_key_words;

    /* Check that wireless algorithms are not used with authentication */
    if ((sa_params_p->crypto_algo == SAB_CRYPTO_KASUMI ||
         sa_params_p->crypto_algo == SAB_CRYPTO_SNOW ||
         sa_params_p->crypto_algo == SAB_CRYPTO_ZUC) &&
        sa_params_p->auth_algo != SAB_AUTH_NULL)
    {
        LOG_CRIT("SABuilder: "
                 "Crypto algorithm cannot be combined with authentication.\n");
        return SAB_INVALID_PARAMETER;
    }

    /* handle feedback modes */
    if (sa_params_p->crypto_algo == SAB_CRYPTO_DES ||
        sa_params_p->crypto_algo == SAB_CRYPTO_3DES ||
        sa_params_p->crypto_algo == SAB_CRYPTO_AES ||
        sa_params_p->crypto_algo == SAB_CRYPTO_SM4 ||
        sa_params_p->crypto_algo == SAB_CRYPTO_BC0)
    {
        switch (sa_params_p->crypto_mode)
        {
        case SAB_CRYPTO_MODE_ECB:
            sa_state_p->cw1 |= SAB_CW1_CRYPTO_MODE_ECB;
            sa_state_p->iv_words = 0;
            break;
        case SAB_CRYPTO_MODE_CBC:
            sa_state_p->cw1 |= SAB_CW1_CRYPTO_MODE_CBC;
            break;
        case SAB_CRYPTO_MODE_CFB:
            sa_state_p->cw1 |= SAB_CW1_CRYPTO_MODE_CFB;
            break;
        case SAB_CRYPTO_MODE_OFB:
            sa_state_p->cw1 |= SAB_CW1_CRYPTO_MODE_OFB;
            break;
        case SAB_CRYPTO_MODE_CTR:
        case SAB_CRYPTO_MODE_EEA2:
            sa_state_p->cw1 |= SAB_CW1_CRYPTO_MODE_CTR;
            break;
        case SAB_CRYPTO_MODE_ICM:
            sa_state_p->cw1 |= SAB_CW1_CRYPTO_MODE_ICM;
            break;
        case SAB_CRYPTO_MODE_CCM:
            sa_state_p->cw1 |= SAB_CW1_CRYPTO_MODE_CTR_LOAD;
            if (sa_params_p->auth_algo != SAB_AUTH_AES_CCM)
            {
                LOG_CRIT("SABuilder: crypto CCM requires auth CCM.\n");
                return SAB_INVALID_PARAMETER;
            }
            break;
        case SAB_CRYPTO_MODE_GCM:
            sa_state_p->cw1 |= SAB_CW1_CRYPTO_MODE_CTR;
            if (sa_params_p->auth_algo != SAB_AUTH_GCM)
            {
                LOG_CRIT("SABuilder: crypto GCM requires auth GCM.\n");
                return SAB_INVALID_PARAMETER;
            }
            break;
        case SAB_CRYPTO_MODE_GMAC:
            sa_state_p->cw1 |= SAB_CW1_CRYPTO_MODE_CTR;
            if (sa_params_p->auth_algo != SAB_AUTH_GMAC)
            {
                LOG_CRIT("SABuilder: crypto GMAC requires auth GMAC.\n");
                return SAB_INVALID_PARAMETER;
            }
            break;
#ifdef SAB_ENABLE_CRYPTO_XTS
        case SAB_CRYPTO_MODE_XTS:
            sa_state_p->cw1 |= SAB_CW1_CRYPTO_MODE_XTS;
            if (sa_params_p->auth_algo != SAB_AUTH_NULL)
            {
                LOG_CRIT("SABuilder: crypto AES-XTS requires auth NULL.\n");
                return SAB_INVALID_PARAMETER;
            }
            break;
        case SAB_CRYPTO_MODE_XTS_STATEFUL:
            sa_state_p->cw1 |= SAB_CW1_CRYPTO_MODE_XTS | SAB_CW1_XTS_STATEFUL;
            if (sa_params_p->auth_algo != SAB_AUTH_NULL)
            {
                LOG_CRIT("SABuilder: crypto AES-XTS requires auth NULL.\n");
                return SAB_INVALID_PARAMETER;
            }
            break;
#endif
        default:
            LOG_CRIT("SABuilder: Invalid crypto mode.\n");
            return SAB_INVALID_PARAMETER;
        }
    }
    else if (sa_params_p->crypto_algo == SAB_CRYPTO_KASUMI ||
             sa_params_p->crypto_algo == SAB_CRYPTO_SNOW ||
             sa_params_p->crypto_algo == SAB_CRYPTO_ZUC)
    {
        if (sa_params_p->crypto_mode == SAB_CRYPTO_MODE_F8 ||
            sa_params_p->crypto_mode == SAB_CRYPTO_MODE_UEA2 ||
            sa_params_p->crypto_mode == SAB_CRYPTO_MODE_EEA3)

        {
            sa_state_p->cw1 |= SAB_CW1_CRYPTO_MODE_F8_UEA;
        }
        else if (sa_params_p->crypto_mode == SAB_CRYPTO_MODE_BASIC ||
                 sa_params_p->crypto_mode == SAB_CRYPTO_MODE_ECB)
        {
            // Do nothing.
        }
        else
        {
            LOG_CRIT("SABuilder: Invalid crypto mode for wireless .\n");
            return SAB_INVALID_PARAMETER;
        }
    }

#ifdef SAB_ENABLE_CRYPTO_CHACHAPOLY
    else if (sa_params_p->crypto_algo == SAB_CRYPTO_CHACHA20)
    {
        sa_state_p->iv_words = 4;
        if (sa_params_p->auth_algo == SAB_AUTH_POLY1305 ||
            sa_params_p->auth_algo == SAB_AUTH_KEYED_HASH_POLY1305)
        {
            sa_state_p->cw1 |= SAB_CW1_CRYPTO_MODE_CHACHA_CTR32 |
                SAB_CW1_CRYPTO_AEAD;
        }
        else if (sa_params_p->auth_algo == SAB_AUTH_NULL)
        {
            if (sa_params_p->crypto_mode == SAB_CRYPTO_MODE_CHACHA_CTR64)
            {
                sa_state_p->cw1 |= SAB_CW1_CRYPTO_MODE_CHACHA_CTR64;
            }
            else
            {
                sa_state_p->cw1 |= SAB_CW1_CRYPTO_MODE_CHACHA_CTR32;
            }
        }
        else
        {
            LOG_CRIT("SABuilder: Invalid authentication mode for Chacha20 .\n");
            return SAB_INVALID_PARAMETER;
        }
    }
#endif

    /* The following crypto modes can only be used with AES */
    if ( (sa_params_p->crypto_mode==SAB_CRYPTO_MODE_CCM) &&
          sa_params_p->crypto_algo != SAB_CRYPTO_AES)
    {
        LOG_CRIT("SABuilder: crypto mode requires AES.\n");
        return SAB_INVALID_PARAMETER;
    }
    /* The following crypto modes can only be used with AES or SM4 */
    if ( (sa_params_p->crypto_mode==SAB_CRYPTO_MODE_GCM ||
          sa_params_p->crypto_mode==SAB_CRYPTO_MODE_GMAC ||
          sa_params_p->crypto_mode==SAB_CRYPTO_MODE_XTS ||
          sa_params_p->crypto_mode==SAB_CRYPTO_MODE_XTS_STATEFUL) &&
          sa_params_p->crypto_algo != SAB_CRYPTO_AES &&
          sa_params_p->crypto_algo != SAB_CRYPTO_SM4)
    {
        LOG_CRIT("SABuilder: crypto mode requires AES or SM4.\n");
        return SAB_INVALID_PARAMETER;
    }
    /* The following crypto modes can only be used with AES, SM4 or BC0 */
    if ( (sa_params_p->crypto_mode==SAB_CRYPTO_MODE_CTR ||
          sa_params_p->crypto_mode==SAB_CRYPTO_MODE_ICM) &&
         sa_params_p->crypto_algo != SAB_CRYPTO_AES &&
         sa_params_p->crypto_algo != SAB_CRYPTO_SM4 &&
         sa_params_p->crypto_algo != SAB_CRYPTO_BC0)
    {
        LOG_CRIT("SABuilder: crypto mode requires AES or SM4.\n");
        return SAB_INVALID_PARAMETER;
    }

    return SAB_STATUS_OK;
}


/*----------------------------------------------------------------------------
 * sa_builder_set_auth_size_and_mode
 *
 * Determine the size of the authentication keys and set the required mode
 * bits in the control words.
 *
 * sa_params_p (input)
 *   The SA parameters structure from which the SA is derived.
 *
 * sa_state_p (input, output)
 *   Variables containing information about the SA being generated.
 *
 * auth1_words_p (output)
 *   The number of 32-bit words of the first authentication key.
 *
 * auth2_words_p (output)
 *   The number of 32-bit words of the second authentication key.
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
static sa_builder_status_t
sa_builder_set_auth_size_and_mode(
        sa_builder_params_t *const sa_params_p,
        sa_builder_state_t * const sa_state_p,
        u32 * const auth1_words_p,
        u32 * const auth2_words_p,
        u32 * const block_count_p)
{
    unsigned int auth1_words = 0;
    unsigned int auth2_words = 0;
    unsigned int block_count = 0;
    switch (sa_params_p->auth_algo)
    {
    case SAB_AUTH_NULL:
        break;
#ifdef SAB_ENABLE_AUTH_MD5
    case SAB_AUTH_HASH_MD5:
        sa_state_p->cw0 |= SAB_CW0_AUTH_HASH_MD5;
        if ((sa_params_p->flags & (SAB_FLAG_HASH_LOAD|SAB_FLAG_HASH_SAVE|
                                  SAB_FLAG_HASH_INTERMEDIATE)) != 0)
        {
            sa_state_p->cw0 |= SAB_CW0_HASH_LOAD_DIGEST;
            auth1_words = 4;
        }
        break;
#endif
#ifdef SAB_ENABLE_AUTH_SHA1
    case SAB_AUTH_HASH_SHA1:
        sa_state_p->cw0 |= SAB_CW0_AUTH_HASH_SHA1;
        if ((sa_params_p->flags & (SAB_FLAG_HASH_LOAD|SAB_FLAG_HASH_SAVE|
                                  SAB_FLAG_HASH_INTERMEDIATE)) != 0)
        {
            sa_state_p->cw0 |= SAB_CW0_HASH_LOAD_DIGEST;
            auth1_words = 5;
        }
        break;
#endif
#ifdef SAB_ENABLE_AUTH_SHA2_256
    case SAB_AUTH_HASH_SHA2_224:
        sa_state_p->cw0 |= SAB_CW0_AUTH_HASH_SHA2_224;
        if ((sa_params_p->flags & (SAB_FLAG_HASH_LOAD|SAB_FLAG_HASH_SAVE|
                                  SAB_FLAG_HASH_INTERMEDIATE)) != 0)
        {
            sa_state_p->cw0 |= SAB_CW0_HASH_LOAD_DIGEST;
            auth1_words = 8;
        }
        break;
    case SAB_AUTH_HASH_SHA2_256:
        sa_state_p->cw0 |= SAB_CW0_AUTH_HASH_SHA2_256;
        if ((sa_params_p->flags & (SAB_FLAG_HASH_LOAD|SAB_FLAG_HASH_SAVE|
                                  SAB_FLAG_HASH_INTERMEDIATE)) != 0)
        {
            sa_state_p->cw0 |= SAB_CW0_HASH_LOAD_DIGEST;
            auth1_words = 8;
        }
        break;
#endif
#ifdef SAB_ENABLE_AUTH_SHA2_512
    case SAB_AUTH_HASH_SHA2_384:
        sa_state_p->cw0 |= SAB_CW0_AUTH_HASH_SHA2_384;
        if ((sa_params_p->flags & (SAB_FLAG_HASH_LOAD|SAB_FLAG_HASH_SAVE|
                                  SAB_FLAG_HASH_INTERMEDIATE)) != 0)
        {
            sa_state_p->cw0 |= SAB_CW0_HASH_LOAD_DIGEST;
            auth1_words = 16;
        }
        break;
    case SAB_AUTH_HASH_SHA2_512:
        sa_state_p->cw0 |= SAB_CW0_AUTH_HASH_SHA2_512;
        if ((sa_params_p->flags & (SAB_FLAG_HASH_LOAD|SAB_FLAG_HASH_SAVE|
                                  SAB_FLAG_HASH_INTERMEDIATE)) != 0)
        {
            sa_state_p->cw0 |= SAB_CW0_HASH_LOAD_DIGEST;
            auth1_words = 16;
        }
        break;
#endif
#ifdef SAB_ENABLE_AUTH_SHA3
    case SAB_AUTH_HASH_SHA3_224:
        if ((sa_params_p->flags & SAB_FLAG_HASH_INTERMEDIATE) != 0)
        {
            sa_state_p->cw1 |= SAB_CW1_HASH_STORE;
            auth1_words = 50;
            sa_state_p->cw0 |= SAB_CW0_AUTH_HASH_SAVE_SHA3_224;
        }
        else if ((sa_params_p->flags & SAB_FLAG_HASH_SAVE) != 0)
        {
            sa_state_p->cw1 |= SAB_CW1_HASH_STORE;
            auth1_words = 7;
            sa_state_p->cw0 |= SAB_CW0_AUTH_HASH_SHA3_224;
        }
        else
        {
            sa_state_p->cw0 |= SAB_CW0_AUTH_HASH_SHA3_224;
        }
        break;
    case SAB_AUTH_HASH_SHA3_256:
        if ((sa_params_p->flags & (SAB_FLAG_HASH_SAVE|
                                  SAB_FLAG_HASH_INTERMEDIATE)) != 0)
        {
            sa_state_p->cw1 |= SAB_CW1_HASH_STORE;
            auth1_words = 50;
            sa_state_p->cw0 |= SAB_CW0_AUTH_HASH_SAVE_SHA3_256;
        }
        else if ((sa_params_p->flags & SAB_FLAG_HASH_SAVE) != 0)
        {
            sa_state_p->cw1 |= SAB_CW1_HASH_STORE;
            auth1_words = 8;
            sa_state_p->cw0 |= SAB_CW0_AUTH_HASH_SHA3_256;
        }
        else
        {
            sa_state_p->cw0 |= SAB_CW0_AUTH_HASH_SHA3_256;
        }
        break;
    case SAB_AUTH_HASH_SHA3_384:
        if ((sa_params_p->flags & (SAB_FLAG_HASH_SAVE|
                                  SAB_FLAG_HASH_INTERMEDIATE)) != 0)
        {
            sa_state_p->cw1 |= SAB_CW1_HASH_STORE;
            auth1_words = 50;
            sa_state_p->cw0 |= SAB_CW0_AUTH_HASH_SAVE_SHA3_384;
        }
        else if ((sa_params_p->flags & SAB_FLAG_HASH_SAVE) != 0)
        {
            sa_state_p->cw1 |= SAB_CW1_HASH_STORE;
            auth1_words = 12;
            sa_state_p->cw0 |= SAB_CW0_AUTH_HASH_SHA3_384;
        }
        else
        {
            sa_state_p->cw0 |= SAB_CW0_AUTH_HASH_SHA3_384;
        }
        break;
    case SAB_AUTH_HASH_SHA3_512:
        if ((sa_params_p->flags & (SAB_FLAG_HASH_SAVE|
                                  SAB_FLAG_HASH_INTERMEDIATE)) != 0)
        {
            sa_state_p->cw1 |= SAB_CW1_HASH_STORE;
            auth1_words = 50;
            sa_state_p->cw0 |= SAB_CW0_AUTH_HASH_SAVE_SHA3_512;
        }
        else if ((sa_params_p->flags & SAB_FLAG_HASH_SAVE) != 0)
        {
            sa_state_p->cw1 |= SAB_CW1_HASH_STORE;
            auth1_words = 16;
            sa_state_p->cw0 |= SAB_CW0_AUTH_HASH_SHA3_512;
        }
        else
        {
            sa_state_p->cw0 |= SAB_CW0_AUTH_HASH_SHA3_512;
        }
        break;
    case SAB_AUTH_SHAKE_256:
    case SAB_AUTH_CSHAKE_256:
        sa_state_p->cw1 |= SAB_CW1_HASH_STORE;
        auth1_words = 50;
        sa_state_p->cw0 |= SAB_CW0_AUTH_HASH_SAVE_SHA3_256;
        break;
#endif
#ifdef SAB_ENABLE_AUTH_SM3
    case SAB_AUTH_HASH_SM3:
        sa_state_p->cw0 |= SAB_CW0_AUTH_HASH_SM3;
        if ((sa_params_p->flags & (SAB_FLAG_HASH_LOAD|SAB_FLAG_HASH_SAVE|
                                  SAB_FLAG_HASH_INTERMEDIATE)) != 0)
        {
            sa_state_p->cw0 |= SAB_CW0_HASH_LOAD_DIGEST;
            auth1_words = 8;
        }
        break;
#endif
#ifdef SAB_ENABLE_AUTH_MD5
    case SAB_AUTH_SSLMAC_MD5:
    case SAB_AUTH_HMAC_MD5:
        sa_state_p->cw0 |= SAB_CW0_AUTH_HMAC_MD5;
        auth1_words = 4;
        block_count = 1;
        auth2_words = 4;
        break;
#endif
#ifdef SAB_ENABLE_AUTH_SHA1
    case SAB_AUTH_SSLMAC_SHA1:
        sa_state_p->cw0 |= SAB_CW0_AUTH_SSLMAC_SHA1;
        auth1_words = 5;
        break;
    case SAB_AUTH_HMAC_SHA1:
        sa_state_p->cw0 |= SAB_CW0_AUTH_HMAC_SHA1;
        auth1_words = 5;
        block_count = 1;
        auth2_words = 5;
        break;
#endif
#ifdef SAB_ENABLE_AUTH_SHA2_256
    case SAB_AUTH_HMAC_SHA2_224:
        sa_state_p->cw0 |= SAB_CW0_AUTH_HMAC_SHA2_224;
        auth1_words = 8;
        block_count = 1;
        auth2_words = 8;
        break;
    case SAB_AUTH_HMAC_SHA2_256:
        sa_state_p->cw0 |= SAB_CW0_AUTH_HMAC_SHA2_256;
        auth1_words = 8;
        block_count = 1;
        auth2_words = 8;
        break;
#endif
#ifdef SAB_ENABLE_AUTH_SHA2_512
    case SAB_AUTH_HMAC_SHA2_384:
        sa_state_p->cw0 |= SAB_CW0_AUTH_HMAC_SHA2_384;
        auth1_words = 16;
        block_count = 2;
        auth2_words = 16;
        break;
    case SAB_AUTH_HMAC_SHA2_512:
        sa_state_p->cw0 |= SAB_CW0_AUTH_HMAC_SHA2_512;
        auth1_words = 16;
        block_count = 2;
        auth2_words = 16;
        break;
#endif
#ifdef SAB_ENABLE_AUTH_SHA3
    case SAB_AUTH_KEYED_HASH_SHA3_224:
        sa_state_p->cw0 |= SAB_CW0_AUTH_KEYED_HASH_SHA3_224;
        sa_state_p->cw0 |= SAB_CW0_HASH_LOAD_DIGEST;
        auth1_words = 36;
        break;
    case SAB_AUTH_KEYED_HASH_SHA3_256:
        sa_state_p->cw0 |= SAB_CW0_AUTH_KEYED_HASH_SHA3_256;
        sa_state_p->cw0 |= SAB_CW0_HASH_LOAD_DIGEST;
        auth1_words = 34;
        break;
    case SAB_AUTH_KEYED_HASH_SHA3_384:
        sa_state_p->cw0 |= SAB_CW0_AUTH_KEYED_HASH_SHA3_384;
        sa_state_p->cw0 |= SAB_CW0_HASH_LOAD_DIGEST;
        auth1_words = 26;
        break;
    case SAB_AUTH_KEYED_HASH_SHA3_512:
        sa_state_p->cw0 |= SAB_CW0_AUTH_KEYED_HASH_SHA3_512;
        sa_state_p->cw0 |= SAB_CW0_HASH_LOAD_DIGEST;
        auth1_words = 18;
        break;
    case SAB_AUTH_HMAC_SHA3_224:
        sa_state_p->cw0 |= SAB_CW0_AUTH_HMAC_SHA3_224;
        auth1_words = 36;
        break;
    case SAB_AUTH_HMAC_SHA3_256:
        sa_state_p->cw0 |= SAB_CW0_AUTH_HMAC_SHA3_256;
        auth1_words = 34;
        break;
    case SAB_AUTH_HMAC_SHA3_384:
        sa_state_p->cw0 |= SAB_CW0_AUTH_HMAC_SHA3_384;
        auth1_words = 26;
        break;
    case SAB_AUTH_HMAC_SHA3_512:
        sa_state_p->cw0 |= SAB_CW0_AUTH_HMAC_SHA3_512;
        auth1_words = 18;
        break;
#endif
#ifdef SAB_ENABLE_AUTH_SM3
    case SAB_AUTH_HMAC_SM3:
        sa_state_p->cw0 |= SAB_CW0_AUTH_HMAC_SM3;
        auth1_words = 8;
        block_count = 1;
        auth2_words = 8;
        break;
#endif
    case SAB_AUTH_AES_XCBC_MAC:
    case SAB_AUTH_AES_CMAC_128:
        sa_state_p->cw0 |= SAB_CW0_AUTH_CMAC_128;
        auth1_words = 4;
        auth2_words = 4;
        break;
    case SAB_AUTH_AES_CMAC_192:
        sa_state_p->cw0 |= SAB_CW0_AUTH_CMAC_192;
        auth1_words = 6;
        auth2_words = 4;
        break;
    case SAB_AUTH_AES_CMAC_256:
        sa_state_p->cw0 |= SAB_CW0_AUTH_CMAC_256;
        auth1_words = 8;
        auth2_words = 4;
        break;
    case SAB_AUTH_AES_CCM:
        if (sa_params_p->crypto_mode != SAB_CRYPTO_MODE_CCM)
        {
            LOG_CRIT("SABuilder: auth CCM requires crypto CCM.\n");
            return SAB_INVALID_PARAMETER;
        }
        switch (sa_params_p->key_byte_count)
        {
        case 16:
            auth1_words = 4;
            sa_state_p->cw0 |= SAB_CW0_AUTH_CMAC_128;
            break;
        case 24:
            auth1_words = 6;
            sa_state_p->cw0 |= SAB_CW0_AUTH_CMAC_192;
            break;
        case 32:
            auth1_words = 8;
            sa_state_p->cw0 |= SAB_CW0_AUTH_CMAC_256;
            break;
        }
        auth2_words = 4;
        sa_state_p->cw1 |= SAB_CW1_ENCRYPT_HASHRES;
        break;
#ifdef SAB_ENABLE_CRYPTO_GCM
    case SAB_AUTH_GCM:
        if (sa_params_p->crypto_mode != SAB_CRYPTO_MODE_GCM)
        {
            LOG_CRIT("SABuilder: auth GCM requires crypto GCM.\n");
            return SAB_INVALID_PARAMETER;
        }
        sa_state_p->cw0 |= SAB_CW0_AUTH_GHASH;
        auth1_words = 4;
        sa_state_p->cw1 |= SAB_CW1_ENCRYPT_HASHRES;
        break;
    case SAB_AUTH_GMAC:
        if (sa_params_p->crypto_mode != SAB_CRYPTO_MODE_GMAC)
        {
            LOG_CRIT("SABuilder: auth GMAC requires crypto GMAC.\n");
            return SAB_INVALID_PARAMETER;
        }
        sa_state_p->cw0 |= SAB_CW0_AUTH_GHASH;
        auth1_words = 4;
        sa_state_p->cw1 |= SAB_CW1_ENCRYPT_HASHRES;
        break;
#endif
#ifdef SAB_ENABLE_CRYPTO_KASUMI
    case SAB_AUTH_KASUMI_F9:
        if (sa_params_p->crypto_algo != SAB_CRYPTO_NULL)
        {
            LOG_CRIT("SABuilder: auth KASUMI requires crypto NULL.\n");
            return SAB_INVALID_PARAMETER;
        }
        auth1_words = 4;
        sa_state_p->cw0 |= SAB_CW0_AUTH_KASUMI_F9;
        break;
#endif
#ifdef SAB_ENABLE_CRYPTO_SNOW
    case SAB_AUTH_SNOW_UIA2:
        if (sa_params_p->crypto_algo != SAB_CRYPTO_NULL)
        {
            LOG_CRIT("SABuilder: auth SNOW requires crypto NULL.\n");
            return SAB_INVALID_PARAMETER;
        }
        auth1_words = 4;
        sa_state_p->cw0 |= SAB_CW0_AUTH_SNOW_UIA2;
        break;
#endif
#ifdef SAB_ENABLE_CRYPTO_ZUC
    case SAB_AUTH_ZUC_EIA3:
        if (sa_params_p->crypto_algo != SAB_CRYPTO_NULL)
        {
            LOG_CRIT("SABuilder: auth ZUC requires crypto NULL.\n");
            return SAB_INVALID_PARAMETER;
        }
        auth1_words = 4;
        sa_state_p->cw0 |= SAB_CW0_AUTH_ZUC_EIA3;
        break;
#endif
    case SAB_AUTH_AES_EIA2:
        if (sa_params_p->crypto_algo != SAB_CRYPTO_NULL)
        {
            LOG_CRIT("SABuilder: auth AES-EIA2 requires crypto NULL.\n");
            return SAB_INVALID_PARAMETER;
        }
        sa_state_p->cw0 |= SAB_CW0_AUTH_CMAC_128;
        auth1_words = 4;
        auth2_words = 8;
        break;
#ifdef SAB_ENABLE_CRYPTO_CHACHAPOLY
    case SAB_AUTH_KEYED_HASH_POLY1305:
        if (sa_params_p->crypto_algo != SAB_CRYPTO_NULL)
        {
            LOG_CRIT("SABuilder: auth keyed Poly1305 requires crypto NULL.\n");
            return SAB_INVALID_PARAMETER;
        }
        if ((sa_params_p->flags & (SAB_FLAG_HASH_LOAD|SAB_FLAG_HASH_SAVE|
                                  SAB_FLAG_HASH_INTERMEDIATE)) != 0)
        {
            sa_state_p->cw0 |= SAB_CW0_HASH_LOAD_DIGEST;
            auth1_words = 4;
            auth2_words = 8;
        }
        else
        {
            auth1_words = 8;
        }
        sa_state_p->cw0 |= SAB_CW0_AUTH_KEYED_HASH_POLY1305;
        break;
    case SAB_AUTH_POLY1305:
        if (sa_params_p->crypto_algo != SAB_CRYPTO_CHACHA20)
        {
            LOG_CRIT("SABuilder: auth Poly1305 requires crypto Chacha20.\n");
            return SAB_INVALID_PARAMETER;
        }
        sa_state_p->cw0 |= SAB_CW0_AUTH_POLY1305;
        sa_state_p->cw1 |= SAB_CW1_IV_CTR|SAB_CW1_CRYPTO_MODE_CHACHA_POLY_OTK;
        sa_state_p->cw1 |= SAB_CW1_ENCRYPT_HASHRES;
        break;
#endif
    default:
        LOG_CRIT("SABuilder: Unsupported authentication algorithm\n");
        return SAB_UNSUPPORTED_FEATURE;
    }

    /* Use block count only for Basc operation HMAC. only when intermediate
       hash is desired */
    if (sa_params_p->protocol != SAB_PROTO_BASIC ||
        (sa_params_p->flags & SAB_FLAG_HASH_INTERMEDIATE) == 0)
    {
        block_count = 0;
    }

    *auth1_words_p = auth1_words;
    *auth2_words_p = auth2_words;
    *block_count_p = block_count;
    return SAB_STATUS_OK;
}


/*----------------------------------------------------------------------------
 * sa_builder_set_auth_keys
 *
 * Fill in authentication keys and associated command word fields in SA.
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
static sa_builder_status_t
sa_builder_set_auth_keys(
        sa_builder_params_t *const sa_params_p,
        sa_builder_state_t * const sa_state_p,
        u32 * const sa_buffer_p)
{
    unsigned int auth1_words = 0;
    unsigned int auth2_words = 0;
    unsigned int block_count = 0;
    sa_builder_status_t rc;

    rc = sa_builder_set_auth_size_and_mode(sa_params_p,
                                      sa_state_p,
                                      &auth1_words,
                                      &auth2_words,
                                      &block_count);
    if (rc != SAB_STATUS_OK)
    {
        return  rc;
    }

    /* Now copy the authentication keys, if applicable */
    if (sa_params_p->auth_algo == SAB_AUTH_AES_CCM)
    {
        sa_params_p->offset_digest0 = sa_state_p->current_offset;
        sa_builder_lib_zero_fill(sa_buffer_p, sa_state_p->current_offset, 32);
        /* Fill zero blocks for the XCBC MAC subkeys */
        sa_state_p->current_offset += 8;
        sa_builder_lib_copy_key_mat_swap(sa_buffer_p, sa_state_p->current_offset,
                                    sa_params_p->key_p,
                                    sa_params_p->key_byte_count);
        sa_state_p->current_offset += sa_state_p->cipher_key_words;

        if (sa_state_p->cipher_key_words == 6)
        {
            sa_builder_lib_zero_fill(sa_buffer_p, sa_state_p->current_offset, 8);
            sa_state_p->current_offset += 2; /* Pad key to 256 bits for CCM-192*/
        }
    }
    else if (sa_params_p->auth_algo == SAB_AUTH_AES_XCBC_MAC ||
             sa_params_p->auth_algo == SAB_AUTH_AES_EIA2 ||
             sa_params_p->auth_algo == SAB_AUTH_AES_CMAC_128 ||
             sa_params_p->auth_algo == SAB_AUTH_AES_CMAC_192 ||
             sa_params_p->auth_algo == SAB_AUTH_AES_CMAC_256)
    {
#ifdef SAB_STRICT_ARGS_CHECK
        if (sa_params_p->auth_key1_p == NULL ||
            sa_params_p->auth_key2_p == NULL ||
            sa_params_p->auth_key3_p == NULL)
        {
            LOG_CRIT("SABuilder: NULL pointer AuthKey supplied\n");
            return SAB_INVALID_PARAMETER;
        }
#endif
        sa_params_p->offset_digest0 = sa_state_p->current_offset;
        sa_builder_lib_copy_key_mat_swap(sa_buffer_p, sa_state_p->current_offset,
                                    sa_params_p->auth_key2_p,
                                    4 * sizeof(u32));
        sa_builder_lib_copy_key_mat_swap(sa_buffer_p, sa_state_p->current_offset + 4,
                                    sa_params_p->auth_key3_p,
                                    4 * sizeof(u32));
        sa_builder_lib_copy_key_mat_swap(sa_buffer_p, sa_state_p->current_offset + 8,
                                    sa_params_p->auth_key1_p,
                                    auth1_words * sizeof(u32));
        sa_state_p->current_offset += 8 + auth1_words;
        if (auth1_words == 6)
        {
            sa_builder_lib_zero_fill(sa_buffer_p, sa_state_p->current_offset, 8);
            sa_state_p->current_offset += 2; /* Pad key to 256 bits for CMAC-192*/
        }
    }
#ifdef SAB_ENABLE_AUTH_SHA3
    else if (sa_params_p->auth_algo == SAB_AUTH_HASH_SHA3_224 ||
             sa_params_p->auth_algo == SAB_AUTH_HASH_SHA3_256 ||
             sa_params_p->auth_algo == SAB_AUTH_HASH_SHA3_384 ||
             sa_params_p->auth_algo == SAB_AUTH_HASH_SHA3_512 ||
             sa_params_p->auth_algo == SAB_AUTH_KEYED_HASH_SHA3_224 ||
             sa_params_p->auth_algo == SAB_AUTH_KEYED_HASH_SHA3_256 ||
             sa_params_p->auth_algo == SAB_AUTH_KEYED_HASH_SHA3_384 ||
             sa_params_p->auth_algo == SAB_AUTH_KEYED_HASH_SHA3_512 ||
             sa_params_p->auth_algo == SAB_AUTH_HMAC_SHA3_224 ||
             sa_params_p->auth_algo == SAB_AUTH_HMAC_SHA3_256 ||
             sa_params_p->auth_algo == SAB_AUTH_HMAC_SHA3_384 ||
             sa_params_p->auth_algo == SAB_AUTH_HMAC_SHA3_512)
    {
#ifdef SAB_STRICT_ARGS_CHECK
        if (auth1_words > 0)
        {
            if (sa_params_p->auth_key1_p == NULL &&
                sa_params_p->auth_key_byte_count > 0)
            {
                LOG_CRIT("SABuilder: NULL pointer AuthKey supplied\n");
                return SAB_INVALID_PARAMETER;
            }
            if (sa_params_p->auth_key_byte_count > auth1_words * sizeof(u32))
            {
                LOG_CRIT("SABuilder: Authentication key too long\n");
                return SAB_INVALID_PARAMETER;
            }
#endif
            sa_params_p->offset_digest0 = sa_state_p->current_offset;
            sa_builder_lib_zero_fill(sa_buffer_p, sa_state_p->current_offset,
                                  auth1_words * sizeof(u32));

            if (sa_params_p->auth_key_byte_count > 0)
            {
                sa_builder_lib_copy_key_mat(sa_buffer_p, sa_state_p->current_offset,
                                        sa_params_p->auth_key1_p,
                                        sa_params_p->auth_key_byte_count);
            }

            sa_state_p->current_offset += auth1_words;

            if (sa_params_p->auth_algo == SAB_AUTH_KEYED_HASH_SHA3_224 ||
                sa_params_p->auth_algo == SAB_AUTH_KEYED_HASH_SHA3_256 ||
                sa_params_p->auth_algo == SAB_AUTH_KEYED_HASH_SHA3_384 ||
                sa_params_p->auth_algo == SAB_AUTH_KEYED_HASH_SHA3_512)
            {
                if (sa_params_p->auth_key_byte_count <
                    auth1_words * sizeof(u32))
                {
                    // Put the key length in last byte of key if it is smaller
                    // than the maximum.
                    sa_state_p->cw1 |= SAB_CW1_DIGEST_CNT;
                    if (sa_buffer_p != NULL)
                    {
                        sa_buffer_p[sa_state_p->current_offset - 1] |=
                            (sa_params_p->auth_key_byte_count << 24);
                    }
                }
            }
        }
    }
#endif
    else if(auth1_words > 0 && auth2_words > 0 &&
            sa_params_p->auth_key1_p == NULL && sa_params_p->auth_key2_p == NULL)
    {
        /* HMAC precomputes not given, allow later computation of
           precomputes. */
        sa_params_p->offset_digest0 = sa_state_p->current_offset;
        sa_params_p->offset_digest1 = sa_state_p->current_offset + auth1_words;
        sa_builder_lib_zero_fill(sa_buffer_p, sa_state_p->current_offset,
                              (auth1_words + auth2_words) * sizeof(u32));
        sa_state_p->current_offset += auth1_words + auth2_words;
        if (block_count > 0)
        {
            /* Only with HMAC multi-part hashing */
            sa_state_p->cw1 |= SAB_CW1_HASH_STORE | SAB_CW1_DIGEST_CNT;
            if (sa_buffer_p != NULL)
                sa_buffer_p[sa_state_p->current_offset] = block_count;
            sa_state_p->current_offset += 1;
        }
    }
    else
    {
        if (auth1_words > 0)
        {
#ifdef SAB_STRICT_ARGS_CHECK
            if (sa_params_p->auth_key1_p == NULL)
            {
                LOG_CRIT("SABuilder: NULL pointer AuthKey supplied\n");
                return SAB_INVALID_PARAMETER;
            }
#endif
            sa_params_p->offset_digest0 = sa_state_p->current_offset;
            sa_builder_lib_copy_key_mat(sa_buffer_p, sa_state_p->current_offset,
                                    sa_params_p->auth_key1_p,
                                    auth1_words * sizeof(u32));
            sa_state_p->current_offset += auth1_words;

            if (sa_params_p->auth_algo == SAB_AUTH_SSLMAC_SHA1)
            { /* both inner and outer digest fields must be set, even though
                 only one is used. */
                sa_builder_lib_zero_fill(sa_buffer_p, sa_state_p->current_offset,
                                        auth1_words * sizeof(u32));
                sa_state_p->current_offset += auth1_words;
            }
        }
        if (auth2_words > 0)
        {
#ifdef SAB_STRICT_ARGS_CHECK
            if (sa_params_p->auth_key2_p == NULL)
            {
                LOG_CRIT("SABuilder: NULL pointer AuthKey supplied\n");
                return SAB_INVALID_PARAMETER;
            }
#endif
            sa_params_p->offset_digest1 = sa_state_p->current_offset;
            sa_builder_lib_copy_key_mat(sa_buffer_p, sa_state_p->current_offset,
                                    sa_params_p->auth_key2_p,
                                    auth2_words * sizeof(u32));
            sa_state_p->current_offset += auth2_words;
        }
        if (block_count > 0)
        {
            /* Only with HMAC multi-part hashing */
            sa_state_p->cw1 |= SAB_CW1_HASH_STORE | SAB_CW1_DIGEST_CNT;
            if (sa_buffer_p != NULL)
                sa_buffer_p[sa_state_p->current_offset] = block_count;
            sa_state_p->current_offset += 1;
        }
    }

    return SAB_STATUS_OK;
}



/*----------------------------------------------------------------------------
 * sa_builder_get_sizes
 *
 * Compute the required sizes in 32-bit words of any of up to three memory
 * areas used by the SA.
 *
 * sa_params_p (input)
 *   Pointer to the SA parameters structure.
 *
 * sa_word32_count_p (output)
 *   The size of the normal SA buffer.
 *
 * sa_state_word32_count_p (output)
 *   The size of any SA state record.
 *
 * arc4_state_word32_count_p (output) T
 *   The size of any ARCFOUR state buffer (output).
 *
 * When the SA state record or ARCFOUR state buffer are not required by
 * the packet engine for this transform, the corresponding size outputs
 * are returned as zero. The Security-IP-96 never requires these buffers
 *
 * If any of the output parameters is a null pointer,
 * the corresponding size will not be returned.
 *
 * The sa_params_p structure must be fully filled in: it must have the
 * same contents as it would have when sa_builder_build_sa is called.
 * This function calls the same routines as sa_builder_build_sa, but with
 * null pointers instead of the SA pointer, so no actual SA will be built.
 * These functions are only called to obtain the length of the SA.
 *
 * Return:
 * SAB_STATUS_OK on success
 * SAB_INVALID_PARAMETER when the record referenced by sa_params_p is invalid,
 */
sa_builder_status_t
sa_builder_get_sizes(
        sa_builder_params_t *const sa_params_p,
        unsigned int *const sa_word32_count_p,
        unsigned int *const sa_state_word32_count_p,
        unsigned int *const arc4_state_word32_count_p)
{
    sa_builder_state_t sa_state;
    int rc;

#ifdef SAB_STRICT_ARGS_CHECK
    if (sa_params_p == NULL)
    {
        LOG_CRIT("sa_builder_get_sizes: NULL pointer sa_params_p supplied\n");
        return SAB_INVALID_PARAMETER;
    }
#endif

    sa_state.current_offset = 2; /* count Control words 0 and 1 */
    sa_state.cw0 = 0;
    sa_state.cw1 = 0;
    sa_state.cipher_key_words = 0;
    sa_state.iv_words = 0;
    sa_state.arc4_state = false;
    sa_state.f_large = false;
    sa_state.f_large_mask = false;

    rc = sa_builder_set_cipher_keys(sa_params_p, &sa_state, NULL);
    if (rc != SAB_STATUS_OK)
        return rc;

    rc = sa_builder_set_auth_keys(sa_params_p, &sa_state, NULL);
    if (rc != SAB_STATUS_OK)
        return rc;

    switch ( sa_params_p->protocol)
    {
#ifdef SAB_ENABLE_PROTO_BASIC
    case SAB_PROTO_BASIC:
    {
        rc = sa_builder_set_basic_params(sa_params_p, &sa_state, NULL);
    }
    break;
#endif
#ifdef SAB_ENABLE_PROTO_IPSEC
    case SAB_PROTO_IPSEC:
    {
        rc = sa_builder_set_ipsec_params(sa_params_p, &sa_state, NULL);
    }
    break;
#endif
#ifdef SAB_ENABLE_PROTO_SSLTLS
    case SAB_PROTO_SSLTLS:
    {
        rc = sa_builder_set_ssltls_params(sa_params_p, &sa_state, NULL);
    }
    break;
#endif
#ifdef SAB_ENABLE_PROTO_SRTP
    case SAB_PROTO_SRTP:
    {
        rc = sa_builder_set_srtp_params(sa_params_p, &sa_state, NULL);
    }
    break;
#endif
#ifdef SAB_ENABLE_PROTO_MACSEC
    case SAB_PROTO_MACSEC:
    {
        rc = sa_builder_set_macsec_params(sa_params_p, &sa_state, NULL);
    }
    break;
#endif
    default:
        LOG_CRIT("sa_builder_get_sizes: unsupported protocol\n");
        return SAB_INVALID_PARAMETER;
    }
    if (rc != SAB_STATUS_OK)
        return rc;

    if (sa_state.arc4_state)
    {
        sa_state.current_offset += 2; /* count IJ pointer and arc4 state */
    }

    if (sa_state.current_offset == 2)
    {
        sa_state.current_offset += 1;
        /* Make sure to have at least one non-context word */
    }

#ifdef SAB_ENABLE_FIXED_RECORD_SIZE
#ifdef SAB_ENABLE_TWO_FIXED_RECORD_SIZES
#ifdef SAB_ENABLE_IPSEC_EXTENDED
    /* It is possible that a record will have to be
       large because of options used by firmware, so determine if the record
       needs to be large. */
    if (sa_params_p->protocol == SAB_PROTO_IPSEC)
    {
        sa_builder_status_t res;
        res = sa_builder_set_extended_ipsec_params(sa_params_p,
                                               &sa_state,
                                               NULL);
        if (res != SAB_STATUS_OK)
            return res;
    }
#endif
#ifdef SAB_ENABLE_DTLS_EXTENDED
    if (sa_params_p->protocol == SAB_PROTO_SSLTLS)
    {
        sa_builder_status_t res;
        res = sa_builder_set_extended_dtls_params(sa_params_p, &sa_state, NULL);
        if (res != SAB_STATUS_OK)
            return res;
    }
#endif
#ifdef SAB_ENABLE_MACSEC_EXTENDED
    if (sa_params_p->protocol == SAB_PROTO_MACSEC)
    {
        sa_builder_status_t res;
        res = sa_builder_set_extended_macsec_params(sa_params_p,
                                                &sa_state,
                                                NULL);
        if (res != SAB_STATUS_OK)
            return res;
    }
#endif
#ifdef SAB_ENABLE_BASIC_EXTENDED
    if (sa_params_p->protocol == SAB_PROTO_BASIC)
    {
        sa_builder_status_t res;
        res = sa_builder_set_extended_basic_params(sa_params_p,
                                               &sa_state,
                                               NULL);
        if (res != SAB_STATUS_OK)
            return res;
    }
#endif
    if (sa_params_p->offset_seq_num <= SAB_SEQNUM_LO_FIX_OFFSET &&
        !sa_state.f_large)
    {
        sa_state.current_offset = SAB_RECORD_WORD_COUNT;
    }
    else if (sa_state.current_offset <= SAB_RECORD_WORD_COUNT + large_transform_offset)
    {
        sa_state.current_offset = SAB_RECORD_WORD_COUNT + large_transform_offset;
    }
    else
    {
        LOG_CRIT("sa_builder_get_sizes: SA filled beyond record size.\n");
        return SAB_INVALID_PARAMETER;
    }
#else
    if (sa_state.current_offset > SAB_RECORD_WORD_COUNT)
    {
        LOG_CRIT("sa_builder_get_sizes: SA filled beyond record size.\n");
        return SAB_INVALID_PARAMETER;
    }
    sa_state.current_offset = SAB_RECORD_WORD_COUNT;
    /* Make the SA record a fixed size for engines that have
       record caches */
#endif
#endif

    if (sa_state_word32_count_p != NULL)
        *sa_state_word32_count_p = 0;

#ifdef SAB_ARC4_STATE_IN_SA
    if (arc4_state_word32_count_p != NULL)
        *arc4_state_word32_count_p = 0;
    if (sa_state.arc4_state)
    {
        if (sa_params_p->offset_arc4_state_record > 0)
        {
            if (sa_params_p->offset_arc4_state_record  < sa_state.current_offset)
            {
                LOG_CRIT("sa_builder_get_sizes: offset_arc4_state_record too low\n");
                return SAB_INVALID_PARAMETER;
            }
            sa_state.current_offset = sa_params_p->offset_arc4_state_record + 64;
        }
        else
        {
            sa_state.current_offset =
                    sa_builder_align_for_a_rc4_state(sa_state.current_offset);
            sa_state.current_offset += 64;
        }
    }
#else
    if (arc4_state_word32_count_p != NULL)
    {
        if (sa_state.arc4_state)
        {
            *arc4_state_word32_count_p = 64;
        }
        else
        {
            *arc4_state_word32_count_p = 0;
        }
    }
#endif
#ifdef SAB_AUTO_TOKEN_CONTEXT_GENERATION
    {
        token_builder_status_t rc;
        unsigned int context_size;
        rc = token_builder_get_context_size(sa_params_p, &context_size);
        if (rc != TKB_STATUS_OK)
        {
            return SAB_ERROR;
        }
        sa_state.current_offset += context_size;
    }
#endif
    if (sa_word32_count_p != NULL)
        *sa_word32_count_p = sa_state.current_offset;

    return SAB_STATUS_OK;
}

/*----------------------------------------------------------------------------
 * sa_builder_build_sa
 *
 * Construct the SA record for the operation described in sa_params_p in
 * up to three memory buffers.
 *
 * sa_params_p (input)
 *    Pointer to the SA parameters structure.
 *
 * sa_buffer_p (output)
 *    Pointer to the the normal SA buffer.
 *
 * sa_state_buffer_p (output)
 *    Pointer to the SA state record buffer.
 *
 * arc4_state_buffer_p (output)
 *    Pointer to the ARCFOUR state buffer.
 *
 * Each of the Buffer arguments must point to a word-aligned
 * memory buffer whose size in words is at least equal to the
 * corresponding size parameter returned by sa_builder_get_sizes().
 *
 * If any of the three buffers is not required for the SA (the
 * corresponding size in sa_builder_get_sizes() is 0), the corresponding
 * Buffer arguments to this function may be a null pointer.
 * The Security-IP-96 never requires these buffers.
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
sa_builder_build_sa(
        sa_builder_params_t * const sa_params_p,
        u32 *const sa_buffer_p,
        u32 *const sa_state_buffer_p,
        u32 *const arc4_state_buffer_p)
{
    sa_builder_state_t sa_state;
    int rc;

    IDENTIFIER_NOT_USED(sa_state_buffer_p);
    IDENTIFIER_NOT_USED(arc4_state_buffer_p);

#ifdef SAB_STRICT_ARGS_CHECK
    if (sa_params_p == NULL || sa_buffer_p == NULL)
    {
        LOG_CRIT("SABuilder: NULL pointer parameter supplied.\n");
        return SAB_INVALID_PARAMETER;
    }
#endif

    sa_state.current_offset = 2; /* count Control words 0 and 1 */
    sa_state.cw0 = 0;
    sa_state.cw1 = 0;
    sa_state.cipher_key_words = 0;
    sa_state.iv_words = 0;
    sa_state.arc4_state = false;
    sa_state.f_large = false;
    sa_state.f_large_mask = false;

    rc = sa_builder_set_cipher_keys(sa_params_p, &sa_state, sa_buffer_p);
    if (rc != SAB_STATUS_OK)
        return rc;

    rc = sa_builder_set_auth_keys(sa_params_p, &sa_state, sa_buffer_p);
    if (rc != SAB_STATUS_OK)
        return rc;

    switch ( sa_params_p->protocol)
    {
#ifdef SAB_ENABLE_PROTO_BASIC
    case SAB_PROTO_BASIC:
    {
        rc = sa_builder_set_basic_params(sa_params_p, &sa_state, sa_buffer_p);
    }
    break;
#endif
#ifdef SAB_ENABLE_PROTO_IPSEC
    case SAB_PROTO_IPSEC:
    {
        rc = sa_builder_set_ipsec_params(sa_params_p, &sa_state, sa_buffer_p);
    }
    break;
#endif
#ifdef SAB_ENABLE_PROTO_SSLTLS
    case SAB_PROTO_SSLTLS:
    {
        rc = sa_builder_set_ssltls_params(sa_params_p, &sa_state, sa_buffer_p);
    }
    break;
#endif
#ifdef SAB_ENABLE_PROTO_SRTP
    case SAB_PROTO_SRTP:
    {
        rc = sa_builder_set_srtp_params(sa_params_p, &sa_state, sa_buffer_p);
    }
    break;
#endif
#ifdef SAB_ENABLE_PROTO_MACSEC
    case SAB_PROTO_MACSEC:
    {
        rc = sa_builder_set_macsec_params(sa_params_p, &sa_state, sa_buffer_p);
    }
    break;
#endif
    default:
        LOG_CRIT("sa_builder_build_sa: unsupported protocol\n");
        return SAB_UNSUPPORTED_FEATURE;
    }
    if (rc != SAB_STATUS_OK)
        return rc;

#ifdef SAB_ENABLE_FIXED_RECORD_SIZE
#ifdef SAB_ENABLE_TWO_FIXED_RECORD_SIZES
    if (sa_params_p->offset_seq_num > SAB_SEQNUM_LO_FIX_OFFSET)
        sa_state.f_large = true;
#endif
#endif

    if (sa_state.arc4_state)
    {
        unsigned int arc4_offset = 0;
#ifdef SAB_ARC4_STATE_IN_SA
        if (sa_params_p->offset_arc4_state_record > 0)
        {
            arc4_offset = sa_params_p->offset_arc4_state_record;
        }
        else
        {
#ifdef SAB_ENABLE_FIXED_RECORD_SIZE
#ifdef SAB_ENABLE_TWO_FIXED_RECORD_SIZES
            if (sa_state.f_large)
            {
                arc4_offset = SAB_RECORD_WORD_COUNT + large_transform_offset;
            }
            else
#else
            {
                arc4_offset = SAB_RECORD_WORD_COUNT;
            }
#endif
#else
            arc4_offset = sa_state.current_offset + 2;
#endif
            arc4_offset = sa_builder_align_for_a_rc4_state(arc4_offset);
        }
#endif
        sa_buffer_p[sa_state.current_offset] = arc4_offset * sizeof(u32);
        sa_buffer_p[sa_state.current_offset + 1] = 0;

        if ( (sa_params_p->flags & SAB_FLAG_ARC4_STATE_LOAD) != 0)
        {
            /* Load the arc4 state when building the SA.
             nonce_p[0] is the 'i' variable and
             nonce_p[1] is the 'j' variable.
             iv_p points to the 256-byte state array.
             The SA Builder will not fill in the arc4 state pointer. */
            if (sa_params_p->nonce_p != NULL)
            {
                sa_buffer_p[sa_state.current_offset + 1] =
                    ((sa_params_p->nonce_p[0] + 1) & 0xff) |
                    (sa_params_p->nonce_p[1]<<8);
            }
            if(sa_params_p->iv_p != NULL)
            {
#ifdef SAB_ARC4_STATE_IN_SA
                sa_builder_lib_copy_key_mat(sa_buffer_p,
                                        (sa_params_p->offset_arc4_state_record >0 ?
                                         sa_params_p->offset_arc4_state_record :
                                          arc4_offset),
                                        sa_params_p->iv_p,
                                        256);

#else
                sa_builder_lib_copy_key_mat(arc4_state_buffer_p,
                                        0,
                                        sa_params_p->iv_p,
                                        256);
#endif
            }

        }

        sa_params_p->offset_ij_ptr = sa_state.current_offset + 1;
        sa_params_p->offset_arc4_state = sa_state.current_offset;

        sa_state.current_offset += 2; /* count IJ pointer and arc4 state */
    }

    if (sa_state.current_offset == 2)
    {
        sa_buffer_p[sa_state.current_offset++] = 0;
        /* Make sure to have at least one non-context word */
    }

    if (!sa_state.f_large_mask)
        sa_state.cw0 |= (sa_state.current_offset - 2) << 8;
    else
        sa_state.cw0 |= (sa_state.current_offset == 66)? 0x0200 : 0x0300;

    sa_buffer_p[0] = sa_state.cw0;
    sa_buffer_p[1] = sa_state.cw1;

    sa_params_p->cw0 = sa_state.cw0;
    sa_params_p->cw1 = sa_state.cw1;

#ifdef SAB_ENABLE_IPSEC_EXTENDED
    if (sa_params_p->protocol == SAB_PROTO_IPSEC)
    {
        sa_builder_status_t res;
        res = sa_builder_set_extended_ipsec_params(sa_params_p,
                                               &sa_state,
                                               sa_buffer_p);
        if (res != SAB_STATUS_OK)
            return res;
    }
#endif
#ifdef SAB_ENABLE_DTLS_EXTENDED
    if (sa_params_p->protocol == SAB_PROTO_SSLTLS)
    {
        sa_builder_status_t res;
        res = sa_builder_set_extended_dtls_params(sa_params_p, &sa_state, sa_buffer_p);
        if (res != SAB_STATUS_OK)
            return res;
    }
#endif
#ifdef SAB_ENABLE_MACSEC_EXTENDED
    if (sa_params_p->protocol == SAB_PROTO_MACSEC)
    {
        sa_builder_status_t res;
        res = sa_builder_set_extended_macsec_params(sa_params_p,
                                                &sa_state,
                                                sa_buffer_p);
        if (res != SAB_STATUS_OK)
            return res;
    }
#endif
#ifdef SAB_ENABLE_BASIC_EXTENDED
    if (sa_params_p->protocol == SAB_PROTO_BASIC)
    {
        sa_builder_status_t res;
        res = sa_builder_set_extended_basic_params(sa_params_p,
                                               &sa_state,
                                               sa_buffer_p);
        if (res != SAB_STATUS_OK)
            return res;
    }
#endif

#ifdef SAB_ENABLE_FIXED_RECORD_SIZE
#ifdef SAB_ENABLE_TWO_FIXED_RECORD_SIZES
    if (sa_state.f_large)
    {
        sa_buffer_p[0] |= SAB_CW0_SW_IS_LARGE;
#ifdef SAB_ENABLE_IPSEC_EXTENDED
        /* Must be set independent of protocol */
        if ((sa_params_p->flags & SAB_FLAG_REDIRECT) != 0)
        {
            sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_FLAGS_WORD_OFFSET +
                       large_transform_offset] |=
                BIT_11 | ((sa_params_p->redirect_interface & MASK_4_BITS) << 12);
        }
#endif
        sa_state.current_offset = SAB_RECORD_WORD_COUNT + large_transform_offset;
    }
    else
    {
#ifdef SAB_ENABLE_IPSEC_EXTENDED
        /* Must be set independent of protocol */
        if ((sa_params_p->flags & SAB_FLAG_REDIRECT) != 0)
        {
            sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_FLAGS_WORD_OFFSET] |=
                BIT_11 | ((sa_params_p->redirect_interface & MASK_4_BITS) << 12);
        }
#endif
        sa_state.current_offset = SAB_RECORD_WORD_COUNT;
    }
#endif
#endif

#ifdef SAB_AUTO_TOKEN_CONTEXT_GENERATION
    {
        token_builder_status_t rc;
        void *token_context_p = (void*)&sa_buffer_p[sa_state.current_offset];
        rc = token_builder_build_context(sa_params_p, token_context_p);
        if (rc != TKB_STATUS_OK)
        {
            return SAB_ERROR;
        }
    }
#endif


    return SAB_STATUS_OK;
}


/*----------------------------------------------------------------------------
 * sa_builder_set_large_transform_offset();
 */
sa_builder_status_t
sa_builder_set_large_transform_offset(
    unsigned int offset)
{
#if defined(SAB_ENABLE_IPSEC_EXTENDED) || defined(SAB_ENABLE_SSLTLS_EXTENDED)
    large_transform_offset = offset;
    return SAB_STATUS_OK;
#else
    IDENTIFIER_NOT_USED(offset);
    return SAB_UNSUPPORTED_FEATURE;
#endif
}

/* end of file sa_builder.c */
