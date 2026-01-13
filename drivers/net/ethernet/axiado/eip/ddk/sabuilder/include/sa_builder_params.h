/* sa_builder_params.h
 *
 * type definitions for the parameter structure for the SA Builder.
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


#ifndef SA_BUILDER_PARAMS_H_
#define SA_BUILDER_PARAMS_H_


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Driver Framework Basic Definitions API
#include "basic_defs.h"


/*----------------------------------------------------------------------------
 * Definitions and macros
 */

/* This specifies one of the supported protocols. For each of the
 * supported protocols, the sa_builder_params_t record contains a
 * protocol-specific extension, defined in a separate header file.
 */
typedef enum
{
    SAB_PROTO_BASIC,
    SAB_PROTO_IPSEC,
    SAB_PROTO_SSLTLS,
    SAB_PROTO_MACSEC,
    SAB_PROTO_SRTP
} sa_builder_protocol_t;

/* Specify direction: outbound (encrypt) or inbound (decrypt) */
typedef enum
{
    SAB_DIRECTION_OUTBOUND,
    SAB_DIRECTION_INBOUND
} sa_builder_direction_t;

/* Values for the flags field. Combine any values using a bitwise or.
   If no flags apply, use the value zero. */
#define SAB_FLAG_GATHER            BIT_0
#define SAB_FLAG_SCATTER           BIT_1
#define SAB_FLAG_SUPPRESS_HDRPROC  BIT_2
#define SAB_FLAG_SUPPRESS_HEADER   BIT_3
#define SAB_FLAG_SUPPRESS_PAYLOAD  BIT_4

#define SAB_FLAG_IV_SAVE           BIT_5 /* Save iv back into SA */
#define SAB_FLAG_ARC4_STATE_SAVE   BIT_6 /* Save ARCFOUR state back */
#define SAB_FLAG_ARC4_STATE_LOAD   BIT_7 /* Load ARCFOUR state */
#define SAB_FLAG_HASH_LOAD         BIT_8 /* Load digest value from SA */
#define SAB_FLAG_HASH_SAVE         BIT_9 /* Save digest value into SA */
#define SAB_FLAG_HASH_INTERMEDIATE BIT_10 /* hash message crosses multiple
                                   packets: load/store intermediate digest */
#define SAB_FLAG_COPY_IV           BIT_11 /* Insert the iv into the output packet (basic crypto) */
#define SAB_FLAG_REDIRECT          BIT_12 /* Redirect packets to other interface*/


/* Specify one of the crypto algorithms */
typedef enum
{
    SAB_CRYPTO_NULL,
    SAB_CRYPTO_DES,
    SAB_CRYPTO_3DES,
    SAB_CRYPTO_AES,
    SAB_CRYPTO_ARCFOUR,
    SAB_CRYPTO_KASUMI,
    SAB_CRYPTO_SNOW,
    SAB_CRYPTO_ZUC,
    SAB_CRYPTO_CHACHA20,
    SAB_CRYPTO_SM4,
    SAB_CRYPTO_BC0, /* Auxiliary block cipher algorithm */
} sa_builder_crypto_t;

/* Specify one of the crypto modes */
typedef enum
{
    SAB_CRYPTO_MODE_ECB,
    SAB_CRYPTO_MODE_CBC,
    SAB_CRYPTO_MODE_OFB,
    SAB_CRYPTO_MODE_CFB,
    SAB_CRYPTO_MODE_CFB1,
    SAB_CRYPTO_MODE_CFB8,
    SAB_CRYPTO_MODE_CTR,
    SAB_CRYPTO_MODE_ICM,
    SAB_CRYPTO_MODE_CCM,       /* Only use with AES, set SAB_AUTH_AES_CCM */
    SAB_CRYPTO_MODE_GCM,       /* Only use with AES, set SAB_AUTH_AES_GCM */
    SAB_CRYPTO_MODE_GMAC,      /* Only use with AES, set SAB_AUTH_AES_GMAC */
    SAB_CRYPTO_MODE_STATELESS, /* For arc4 */
    SAB_CRYPTO_MODE_STATEFUL, /* For arc4 */
    SAB_CRYPTO_MODE_XTS,      /* Only use with AES, no authentication */
    SAB_CRYPTO_MODE_XTS_STATEFUL, /* Only use with AES, no authentication.
                                   Stateful operation, allow multiple
                                   operations on the same 'sector' */
    SAB_CRYPTO_MODE_BASIC,
    SAB_CRYPTO_MODE_F8,       /* Only with Kasumi */
    SAB_CRYPTO_MODE_UEA2,     /* Only with SNOW */
    SAB_CRYPTO_MODE_EEA2,     /* Only with AES */
    SAB_CRYPTO_MODE_EEA3,     /* Only with ZUC */
    SAB_CRYPTO_MODE_CHACHA_CTR32, /* Only with Chacha20 */
    SAB_CRYPTO_MODE_CHACHA_CTR64, /* Only with Chacha20 */w
} sa_builder_crypto_mode_t;

/* Specify one of the iv sources. Not all methods are supported with all
 protocols.*/
typedef enum
{
    SAB_IV_SRC_DEFAULT, /* Default mode for the protocol */
    SAB_IV_SRC_SA,      /* iv is loaded from SA */
    SAB_IV_SRC_PRNG,    /* iv is derived from PRNG */
    SAB_IV_SRC_INPUT,   /* iv is prepended to the input packet. */
    SAB_IV_SRC_TOKEN,   /* iv is included in the packet token. */
    SAB_IV_SRC_SEQ,     /* iv is derived from packet sequence number. */
    SAB_IV_SRC_XORSEQ,  /* iv is derived from packet sequence number XOR-ed
                           with fixed value. */
    SAB_IV_SRC_IMPLICIT, /* iv is derived from packet sequence number,
                            not included in packet data (RFC8750 for ESP) */
} sa_builder_iv_src_t;

/* Specify one of the hash or authentication methods */
typedef enum
{
    SAB_AUTH_NULL,
    SAB_AUTH_HASH_MD5,
    SAB_AUTH_HASH_SHA1,
    SAB_AUTH_HASH_SHA2_224,
    SAB_AUTH_HASH_SHA2_256,
    SAB_AUTH_HASH_SHA2_384,
    SAB_AUTH_HASH_SHA2_512,
    SAB_AUTH_SSLMAC_MD5,
    SAB_AUTH_SSLMAC_SHA1,
    SAB_AUTH_HMAC_MD5,
    SAB_AUTH_HMAC_SHA1,
    SAB_AUTH_HMAC_SHA2_224,
    SAB_AUTH_HMAC_SHA2_256,
    SAB_AUTH_HMAC_SHA2_384,
    SAB_AUTH_HMAC_SHA2_512,
    SAB_AUTH_AES_XCBC_MAC,
    SAB_AUTH_AES_CMAC_128,  /* Identical to AES_XCBC_MAC */
    SAB_AUTH_AES_CMAC_192,
    SAB_AUTH_AES_CMAC_256,
    SAB_AUTH_AES_CCM,      /* Set matching crypto algorithm and mode */
    SAB_AUTH_GCM,      /* Set matching crypto algorithm (AES or SM4) and mode */
    SAB_AUTH_GMAC,     /* Set matching crypto algorithm (AES or SM4) and mode */
    SAB_AUTH_KASUMI_F9,
    SAB_AUTH_SNOW_UIA2,
    SAB_AUTH_AES_EIA2,
    SAB_AUTH_ZUC_EIA3,
    SAB_AUTH_HASH_SHA3_224,
    SAB_AUTH_HASH_SHA3_256,
    SAB_AUTH_HASH_SHA3_384,
    SAB_AUTH_HASH_SHA3_512,
    SAB_AUTH_KEYED_HASH_SHA3_224,
    SAB_AUTH_KEYED_HASH_SHA3_256,
    SAB_AUTH_KEYED_HASH_SHA3_384,
    SAB_AUTH_KEYED_HASH_SHA3_512,
    SAB_AUTH_HMAC_SHA3_224,
    SAB_AUTH_HMAC_SHA3_256,
    SAB_AUTH_HMAC_SHA3_384,
    SAB_AUTH_HMAC_SHA3_512,
    SAB_AUTH_POLY1305,
    SAB_AUTH_KEYED_HASH_POLY1305,
    SAB_AUTH_HASH_SM3,
    SAB_AUTH_HMAC_SM3,
    SAB_AUTH_SHAKE_256,
    SAB_AUTH_CSHAKE_256,
   /* Backward comptaible definitions for these names.
      Note: GCM and GMAC are not tied specifically to the AES block cipher.
   */
    SAB_AUTH_AES_GCM = SAB_AUTH_GCM,
    SAB_AUTH_AES_GMAC = SAB_AUTH_GMAC,
} sa_builder_auth_t;



/* This is the main SA parameter structure.
 *
 * This contains the common fields for all protocol families.
 * Each protocol has a special extension.
 *
 * The entire data structure (including protocol-specific extension) must be
 * prepared before calling sa_builder_get_sizes() and SABuilder_Build_SA().
 *
 * See the SABuilder_Init* functions in the protocol specific headers
 * (sa_builder_ipsec.h etc.) for helper functions to prepare it.
 * All these initialization functions will provide sensible defaults for all
 * fields, but both the crypto algorithm and the authentication algorithm
 * are set to NULL.
 *
 * For a practical use, at least the following fields must be filled in
 * after calling the initialization functions:
 * - crypto_algo, key_p and key_byte_count if encryption is required.
 *   - crypto_mode if anything other than CBC is required.
 *   - nonce_p if counter mode is used.
 * - auth_algo if authentication is desired and (depending on the
 *   authentication algorithm), one or more of
 *   auth_key1_p, auth_key2_p or auth_key3_p.
 */
typedef struct
{
    /* Protocol related fields */
    sa_builder_protocol_t protocol;
    sa_builder_direction_t direction;
    void * protocol_extension_p; /* Pointer to the extension record */
    u32 flags; /* Generic flags */

    /* Crypto related fields */
    sa_builder_crypto_t crypto_algo; /* Cryptographic algorithm */
    sa_builder_crypto_mode_t crypto_mode;
    u8 crypto_parameter; /* Additional algorithm parameter,
                                for example number of rounds */
    sa_builder_iv_src_t iv_src; /* source of iv */
    u8 key_byte_count; /* Key length in bytes */
    u8 *key_p;    /* pointer to crypto key */
    u8 *iv_p;     /* pointer to initialization vector.
                          iv size is implied by crypto algorithm and mode */
    u8 *nonce_p;    /* Cryptographic nonce, e.g. for counter mode */
    /* Note: for ARCFOUR, the stream cipher state will be loaded if the
       SAB_FLAG_ARC4_STATE_LOAD flag is set.
       nonce_p[0] is the I variable, nonce_p[1] is the J variable and
       iv_p points to the 256-byte S-array.

       If this flag is not set, the stream cipher state will not be
       loaded at SA build time and the first packet has to specify
       the ARCFOUR state to be initialized from the key.

       For AES-XTS, the Nonce represents Key2, */

    /* Authentication related fields */
    sa_builder_auth_t auth_algo; /* Authentication algorithm */
    u8 auth_key_byte_count; /* number of bytes in authentication key
                                 (only used for keyed SHA3 and HMAC-SHA3) */
    u8 *auth_key1_p;
    u8 *auth_key2_p;
    u8 *auth_key3_p;
    /* The SA Builder expects authentication keys in their
       preprocessed form. The sizes of the authentication keys are
       implied by auth_algo. For details on the preprocessed
       authentication keys, see the Implementation Notes document.

       Plain hash functions with no digest loading:
                   No authentication keys are used.
       Plain hash functions with digest loading:
                   AuthKey1 is the digest to be loaded.
       Any of the HMAC functions:
                   AuthKey1 is precomputed inner digest.
                   AuthKey2 is precomputed outer digest.
       SSL-MAC-MD$:
                   AuthKey1 is precomputed inner digest.
                   AuthKey2 is precomputed outer digest.
       SSL-HAC-SHA1:
                   AuthKey1 is the authentication key (not pre-processed).
       GCM or GMAC:
                   AuthKey1 is H = E(K,0). This is an all-zero block encrypted
                   with the encryption key.
       XCBC_MAC (RFC3566):
                   AuthKey1 = K1
                   AuthKey2 = K2
                   AuthKey3 = K3
       CMAC (RFC4493,RFC4494):
                   AuthKey1 = K
                   AuthKey2 = K1
                   AuthKey3 = K2
       CCM:
                   No authentication keys required: the SA builder will
                   use the encryption key.

       Unused AuthKey fields may be NULL.
    */
    u8 redirect_interface;
    /* Interface ID to which packets must be redirected */

    u32 offset_arc4_state_record;
    /* offset of the arc4 state record with respect to the start of
       the SA record. The application can set this to specify a
       desired offset for this record. If left 0, the SA Builder will
       put the arc4 state right after the SA.

       This parameter is only used when SAB_ARC4_STATE_IN_SA is defined.
    */

    /* The following values reflect control words. The application shall
       not touch those */
    u32 cw0, cw1;

    /* The following values reflect. offsets of certain fields in the SA
     buffer. The application shall not touch these.*/
    u8 offset_digest0; /* Word-offset of Digest 0 */
    u8 offset_digest1; /* Word-offset of Digest 1 */
    u8 offset_seq_num;  /* Word-offset of Sequence number  */
    u8 offset_seq_mask; /* Word-offset of Sequence number mask */
    u8 offset_iv;      /* Word-offset of iv */
    u8 offset_ij_ptr;   /* Word-offset of IJ Pointer for arc4 */
    u8 offset_arc4_state; /* Word-offset of arc4 state */
    /* The following values reflect the width of certain fields in the SA
       (those that may be updated and may be read back)*/
    u8 seq_num_word32_count; /* Width of the sequence number */
    u8 seq_mask_word32_count;/* Width of the sequence number masks*/
    u8 iv_word32_count;  /* Width of the iv */
} sa_builder_params_t;


#endif /* SA_BUILDER_PARAMS_H_ */


/* end of file sa_builder_params.h */
