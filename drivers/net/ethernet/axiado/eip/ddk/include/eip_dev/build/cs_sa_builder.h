/* cs_sa_builder.h
 *
 * Product-specific configuration file for the SA Builder.
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

/* Collectively enable all standard algorithms */
#define SAB_ENABLE_CRYPTO_STANDARD

/* Enable SHA2-512 algorithms */
#define SAB_ENABLE_AUTH_SHA2_512

/* Collectively enable all wireless algorithms */
#define SAB_ENABLE_CRYPTO_WIRELESS

/* Collectively enable SM4, SM3 and related algorithms */
#define SAB_ENABLE_CRYPTO_SM4_SM3

/* Enable AES-XTS */
#define SAB_ENABLE_CRYPTO_XTS

/* Enable ARCFOUR */
#define SAB_ENABLE_CRYPTO_ARCFOUR

/* Enable ChaCha20 and Poly1305 */
#define SAB_ENABLE_CRYPTO_CHACHAPOLY

/* Enable SHA3 */
#define SAB_ENABLE_AUTH_SHA3

// Require this even for basic loookaside, as cache invalidate command will
// write out entire record (64 word).
#define SAB_ENABLE_FIXED_RECORD_SIZE
#define SAB_RECORD_WORD_COUNT 64

/* Enable this if the hardware supports 384-bitsequence number
   masks. If SAB_ENABLE_256BIT_SEQMASK not also set, support 256-bit
   masks via a work-around.*/
#define SAB_ENABLE_384BIT_SEQMASK

/* Enable this if the hardware supports 256-bit sequence
   number masks. */
//#define SAB_ENABLE_256BIT_SEQMASK

/* Enable this to use fixed offsets for sequence number masks regardless of
   size. This only works on hardware that supports 384-bit masks. */
//#define SAB_ENABLE_DEFAULT_FIXED_OFFSETS
#define SAB_ENABLE_1024BIT_SEQMASK

/* Which protcol families are enabled? */
#define SAB_ENABLE_PROTO_BASIC
#define SAB_ENABLE_PROTO_IPSEC
#define SAB_ENABLE_PROTO_SSLTLS
//#define SAB_ENABLE_PROTO_MACSEC
//#define SAB_ENABLE_PROTO_SRTP

/* Which protcol-specific options are enabled? */
#define SAB_ENABLE_IPSEC_ESP
//#define SAB_ENABLE_IPSEC_AH

/* Enable this if you desire to include the arc4 state in the SA
   record. This requires the hardware to be configured for relative
   arc4 state offsets */
#define SAB_ARC4_STATE_IN_SA

/* Strict checking of function arguments if enabled */
#define SAB_STRICT_ARGS_CHECK

/* log level for the token builder.
   choose from LOG_SEVERITY_INFO, LOG_SEVERITY_WARN, LOG_SEVERITY_CRIT */
#define LOG_SEVERITY_MAX LOG_SEVERITY_WARN

#define SAB_ENABLE_1024BIT_SEQMASK

/* end of file cs_sa_builder.h */
