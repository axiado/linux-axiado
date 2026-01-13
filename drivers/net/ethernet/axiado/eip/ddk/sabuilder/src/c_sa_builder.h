/* c_sa_builder.h
 *
 * Default configuration file for the SA Builder
 */

/*****************************************************************************
* Copyright (c) 2011-2020 by Rambus, Inc. and/or its subsidiaries.
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
 * Import the product specific configuration.
 */
#include "cs_sa_builder.h"


/* Which protocol families are enabled? Enable the desired protocols.*/
//#define SAB_ENABLE_PROTO_BASIC
//#define SAB_ENABLE_PROTO_IPSEC
//#define SAB_ENABLE_PROTO_SSLTLS
//#define SAB_ENABLE_PROTO_MACSEC
//#define SAB_ENABLE_PROTO_SRTP

/* Which protocol-specific options are enabled? */
//#define SAB_ENABLE_IPSEC_ESP
//#define SAB_ENABLE_IPSEC_AH

/* Algorithm selection, depending on what the hardware supports */

/* Collectively enable all standard algorithms */
//#define SAB_ENABLE_CRYPTO_STANDARD

/* Enable SHA2-512 algorithms */
//#define SAB_ENABLE_AUTH_SHA2_512

/* Collectively enable all wireless algorithms */
//#define SAB_ENABLE_CRYPTO_WIRELESS

/* Collectively enable SM4, SM3 and related algorithms */
//#define SAB_ENABLE_CRYPTO_SM4_SM3

/* Enable AES-XTS */
//#define SAB_ENABLE_CRYPTO_XTS

/* Enable ARCFOUR */
//#define SAB_ENABLE_CRYPTO_ARCFOUR

/* Enable ChaCha20 and Poly1305 */
//#define SAB_ENABLE_CRYPTO_CHACHAPOLY

/* Enable SHA3 */
//#define SAB_ENABLE_AUTH_SHA3

/* Enable if the SA Builder must support extended use case for IPsec
   processing */
//#define SAB_ENABLE_IPSEC_EXTENDED

/* Enable if the SA Builder must support extended use case for DTLS
   processing */
//#define SAB_ENABLE_DTLS_EXTENDED

/* Enable if the SA Builder must support extended use case for MACsec
   processing */
//#define SAB_ENABLE_MACSEC_EXTENDED

/* Enable if the SA Builder must support extended use case for Basic
   processing */
//#define SAB_ENABLE_BASIC_EXTENDED

/* Enable if the SA Builder must support an engine with fixed SA records
   (e.g. for a record cache) */
//#define SAB_ENABLE_FIXED_RECORD_SIZE

#if defined(SAB_ENABLE_IPSEC_EXTENDED) || defined(SAB_ENABLE_DTLS_EXTENDED) || defined(SAB_ENABLE_MACSEC_EXTENDED) || defined(SAB_ENABLE_BASIC_EXTENDED)

#ifdef SAB_ENABLE_IPSEC_EXTENDED
#if !defined(SAB_ENABLE_PROTO_IPSEC) || !defined(SAB_ENABLE_IPSEC_ESP)
#error "IPsec extended use case requires IPSEC and ESP protocols."
#endif
#endif

#ifdef SAB_ENABLE_DTLS_EXTENDED
#ifndef SAB_ENABLE_PROTO_SSLTLS
#error "DTLS extended use case requires SSL/TLS protocols."
#endif
#endif

#ifdef SAB_ENABLE_MACSEC_EXTENDED
#ifndef SAB_ENABLE_PROTO_MACSEC
#error "MACSEC extended use case requires MACSEC protocols"
#endif
#endif

// In the extended use case, always use a fixed record size, but do
// not provide a value of that size here.
#define SAB_ENABLE_FIXED_RECORD_SIZE

// Set this if tunnel header fields are to be copied into the transform.
// for extended use case.
//#define SAB_ENABLE_EXTENDED_TUNNEL_HEADER

#else
// Look-aside use case.
#ifdef SAB_ENABLE_FIXED_RECORD_SIZE
#ifndef SAB_RECORD_WORD_COUNT
// Specify the number of words of an SA record when the record size is fixed.
#define SAB_RECORD_WORD_COUNT 64
#endif
#endif
#endif

// Set this if there are two fixed record sizes:
// SAB_RECORD_WORD_COUNT and SAB_RECORD_WORD_COUNT_LARGE,
// select the large one if the context size exceeds
// SAB_LARGE_RECORD_TRHESHOLD_WORD_COUNT.
//#define SAB_ENABLE_TWO_FIXED_RECORD_SIZES

#ifndef SAB_LARGE_TRANSFORM_OFFSET
#define SAB_LARGE_TRANSFORM_OFFSET 16
#endif

/* Enable specific crypto and authentication algorithms. The following
   are standard algorithms, normally always selected.*/
//#define SAB_ENABLE_CRYPTO_AES
//#define SAB_ENABLE_CRYPTO_GCM
//#define SAB_ENABLE_CRYPTO_3DES
//#define SAB_ENABLE_AUTH_MD5
//#define SAB_ENABLE_AUTH_SHA1
//#define SAB_ENABLE_AUTH_SHA2_256
#ifdef SAB_ENABLE_CRYPTO_STANDARD
#define SAB_ENABLE_CRYPTO_AES
#define SAB_ENABLE_CRYPTO_GCM
#define SAB_ENABLE_CRYPTO_3DES
#define SAB_ENABLE_AUTH_MD5
#define SAB_ENABLE_AUTH_SHA1
#define SAB_ENABLE_AUTH_SHA2_256
#endif

/* Wireless algorihms */
//#define SAB_ENABLE_CRYPTO_KASUMI
//#define SAB_ENABLE_CRYPTO_SNOW
//#define SAB_ENABLE_CRYPTO_ZUC
#ifdef SAB_ENABLE_CRYPTO_WIRELESS
#define SAB_ENABLE_CRYPTO_KASUMI
#define SAB_ENABLE_CRYPTO_SNOW
#define SAB_ENABLE_CRYPTO_ZUC
#endif

/* SM4-SM3 algorithms */
//#define SAB_ENABLE_CRYPTO_SM4
//#define SAB_ENABLE_CRYPTO_BC0
//#define SAB_ENABLE_AUTH_SM3
#ifdef SAB_ENABLE_CRYPTO_SM4_SM3
#define SAB_ENABLE_CRYPTO_SM4
#define SAB_ENABLE_CRYPTO_BC0
#define SAB_ENABLE_AUTH_SM3
#endif

/* Enable this if the hardware supports 384-bitsequence number
   masks. If SAB_ENABLE_256BIT_SEQMASK not also set, support 256-bit
   masks via a work-around.*/
//#define SAB_ENABLE_384BIT_SEQMASK

/* Enable this if the hardware supports 256-bit sequence
   number masks. */
//#define SAB_ENABLE_256BIT_SEQMASK

/* Enable this to use fixed offsets for sequence number masks regardless of
   size. This only works on hardware that supports 384-bit masks. */
//#define SAB_ENABLE_DEFAULT_FIXED_OFFSETS

/* Enable this to support building an SA for LTE firmware*/
//#define SAB_ENABLE_LTE_FIRMWARE

/* Enable this if you desire to include the arc4 state in the SA
   record. This requires the hardware to be configured for relative
   arc4 state offsets */
//#define SAB_ARC4_STATE_IN_SA


/* When the arc4 state is included in the SA record, specify the
   alignment requirements for this state.
 */
#ifndef SAB_ARC4_STATE_ALIGN_BYTE_COUNT
#define SAB_ARC4_STATE_ALIGN_BYTE_COUNT 4
#endif


/* Strict checking of function arguments if enabled */
//#define SAB_STRICT_ARGS_CHECK

/* Call Token Context generation in SA Builder */
//#define SAB_AUTO_TOKEN_CONTEXT_GENERATION

/* log level for the SA builder.
   choose from LOG_SEVERITY_INFO, LOG_SEVERITY_WARN, LOG_SEVERITY_CRIT */
#ifndef LOG_SEVERITY_MAX
#define LOG_SEVERITY_MAX LOG_SEVERITY_CRIT
#endif


/* end of file c_sa_builder.h */
