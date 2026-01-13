/* token_builder.h
 *
 * Token Builder API.
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


#ifndef TOKEN_BUILDER_H_
#define TOKEN_BUILDER_H_


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

#include "sa_builder_params.h"

// Driver Framework Basic Definitions API
#include "basic_defs.h"


/*----------------------------------------------------------------------------
 * Definitions and macros
 */
typedef enum
{
    TKB_STATUS_OK,
    TKB_INVALID_PARAMETER,
    TKB_BAD_PACKET,
    TKB_BAD_PROTOCOL,
    TKB_BAD_FIELD_SIZE,
    TKB_BAD_CRYPTO,
    TKB_ERROR,
} token_builder_status_t;

/* The Token Context Record is an opaque data structure that holds
   per-SA information needed by the Token Builder. Its contents are
   never used outside the Token Builder. The application refers to it
   via a void pointer and the application shall call
   TokenBuilder_GetContextSIze() to determine its size and allocate a
   sufficiently large buffer for it.

   The Token Context Record does not in any way refer to the
   SABuilder_SA_Params_t record from which it is derived and the
   latter may safely be discarded after the call to
   token_builder_build_context().

   The Token Context Record is produced by token_builder_build_context()
   and will be used by token_builder_build_token(). The application has
   to make sure that this data structure is not modified and remains
   allocated as long as it is used. After its last use in a call to
   token_builder_build_token(), it may safely be discarded.

   The Packet Token is a data structure used by some types of packet
   engine and is required to process the packet correctly. It must be
   generated separately for every single packet processed.

   The Packet Token is an opaque data structure as far as the application is
   concerned.. The application refers to it via a void pointer and the
   application shall call TokenBuilder_GettSIze() to determine its
   size and allocate a sufficiently large buffer for it..

   A Packet Token has a 32-bit header word, which is returned separately from
   the main Packet Token data structure.

   The Packet Token must remain allocated until it has been used by
   the hardware to process that packet.
*/

/* Per-packet flags */

/* For a basic hash operation, possibly spanning multiple packets,
 * indicate that this packet is the final one of the hash message
*/
#define TKB_PACKET_FLAG_HASHFINAL BIT_0

/* For a basic hash operation, possibly spanning multiple packets,
 * indicate that this packet is the first one of the hash message
*/
#define TKB_PACKET_FLAG_HASHFIRST BIT_1

/* For a basic hash operation, when the message consists of a single
 * packet, set both TKB_PACKET_FLAG_HASHFIRST and
 * TKB_PACKET_FLAG_HASHFINAL. For protocols such as IPsec, that always
 * hash per packet, those flags are not used and should not be set.
*/

/* Specify that the arc4 state must be loaded */
#define TKB_PACKET_FLAG_ARC4_LOAD BIT_2

/* Specify that the arc4 state must be saved */
#define TKB_PACKET_FLAG_ARC4_SAVE BIT_3

/* Specify that the initial arc4 state must be derived from the key.
   Use this for the first packet encrypted or decrypted by each key, unless
   the state was loaded by SA Builder.*/
#define TKB_PARCKET_FLAG_ARC4_INIT BIT_4
#define TKB_PACKET_FLAG_ARC4_INIT BIT_4

/* Specify that the counter (for counter mode) must be reinitialized */
#define TKB_PACKET_FLAG_CTR_INIT BIT_5

/* Specify that hash must be appended to output packet */
#define TKB_PACKET_FLAG_HASHAPPEND BIT_6

/* Specify for AES-XTS (stateful) that the initial iv must be computed from
   Key2. */
#define TKB_PACKET_FLAG_XTS_INIT BIT_7

/* Specify for inbound ESP tunnel operations to keep outer tunnel header */
#define TKB_PACKET_FLAG_KEEP_OUTER BIT_8

/* Specify for outbound ESP IPv6 tunnel operations that we copy the flow label*/
#define TKB_PACKET_FLAG_COPY_FLOWLABEL BIT_9

/* Specify for outbound ESP transport operations that we always encapsulate the
   last extension header is it is Destination options. */
#define TKB_PACKET_FLAG_ENC_LAST_DEST BIT_10

/* Structure to represent optional per-packet parameters */
typedef struct
{
    u32 packet_flags; /* Per packet flags. zero if none apply, bitwise
                              or of TKB_PACKET_FLAG_* constants. */
    u32 bypass_byte_count; /* The number of bytes of the packet
                                that must be bypassed (copied
                                unmodified) */
    u8 pad_byte;   /* A single-byte value that can be specified
                          per packet.
                          - Next Header for outbound IPsec.
                          - Record type for outbound SSL/TLS
                          - number of valid bits in last byte for SNOW/ZUC
                          - Pad alignment minus 1 for outbound basic
                            hash-encrypt.
                          - Record header alignment for inbound DTLS with
                            header processing.
                       */
    u8 *iv_p;      /* Only applies when IvSrc is SAB_IV_SRC_TOKEN */

    u32 additional_value; /* Required for protocol-specific applications
                                 - Pad alignment for outbound IPSec/SSL/TLS
                                 - Rollover counter for outbound SRTP
                                 - SRTCP index for outbound SRTCP
                                 - AAD length for basic crypto/hash.
                                 - count value for wireless algorithms
                                 - J value for AES-XTS
                                 - Packet ID for outbound IPsec IPv4 tunnel
                                 - Reduced window size for inbound IPsec */
    u8 *aad_p;     /* Addtional authentication data, for basic crypto and
                           hash operations and also for combined
                           crypto-authentication algorithms such as GCM.
                           If this is NULL, any additional authentication
                           data is at the start of the packet instead.

                           For inbound SSL/TLS/DTLS: if nonzero, pointer
                           to the last 32-byte block of the packet.
                           This can be required if the packet data is not
                           in a contiguous buffer.
                         */

    /* The following fields are filled in by the Token Builder (are outputs)
     */

    u16 prev_nh_offset; /* offset where Next Header should be patched */
    /* As input, if nonzero, specify the number of bytes in the packet that
       are accessible for header parsing (if the packet is not in a contiguous
       buffer) */
    u16 TOS_TC_DF;  /* TOS_TC byte (and df bit at bit 9) in
                            tunnel header. */
    u8 cle; /* Classification errors */
    u8 result_flags; /* flags to pass to result token */
} token_builder_params_t;


/*----------------------------------------------------------------------------
 * token_builder_get_context_size
 *
 * Determine the size of the token context record in 32-bit words, which may
 * depend on the SA parameters.
 *
 * sa_params_p (input)
 *   Points to the SA parameters data structure
 * token_context_word32_count_p (output)
 *   Required size of the Token Context Record in 32-bit words.
 *
 * Return:
 * TKB_STATUS_OK if successful.
 * TKB_INVALID_PARAMETER if any of the pointer parameters is NULL or if
 *                      the contents of the input data structures are invalid.
 */
token_builder_status_t
token_builder_get_context_size(
    const sa_builder_params_t * const sa_params_p,
    unsigned int * const token_context_word32_count_p);


/*----------------------------------------------------------------------------
 * token_builder_build_context
 *
 * Create a Token Context Record.
 *
 * sa_params_p (input)
 *   Points to the SA parameters data structure. It is important that
 *   SABuilder_GetSIze() and sa_builder_build_sa() have already been called
 *   for this SA.
 * token_context_p (output)
 *   Buffer to hold the Token Context Record. It must point to a valid
 *   storage buffer that is large enough to hold the Token
 *   Context. Before calling this function, the application must call
 *   TokeBuilder_GetContextSize() with the same SA parameters and
 *   allocate a buffer of that size.
 *
 * Return:
 * TKB_STATUS_OK if successful.
 * TKB_INVALID_PARAMETER if any of the pointer parameters is NULL or if
 *                      the contents of the SA parameters are invalid.
 */
token_builder_status_t
token_builder_build_context(
    const sa_builder_params_t * const sa_params_p,
    void * const token_context_p);


/*----------------------------------------------------------------------------
 * token_builder_next_pkt_id
 *
 * Increment the pkt_id if the Token Context describes an IPv4 ESP Tunnel
 * operation
 *
 */
unsigned int
token_builder_next_pkt_id(
    const void * const token_context_p,
    unsigned int pkt_id);


/*----------------------------------------------------------------------------
 * token_builder_get_size
 *
 * Determine the size of a packet token.
 *
 * token_context_p (input)
 *   Points to the Token Context Record.
 * token_word32_count_p (output)
 *   Expected size of the packet token in 32-bit words, depending on the
 *   contents of the Token Context Record. For some protocols, the
 *   actual token size depends on the packet contents as well, but in
 *   this case the maximum token size for that context is returned.
 *
 * Return:
 * TKB_STATUS_OK if successful.
 * TKB_INVALID_PARAMETER if any of the pointer parameters is NULL or if
 *                      the contents of the input data structures are invalid.
 */
token_builder_status_t
token_builder_get_size(
    const void * const token_context_p,
    unsigned int * const token_word32_count_p);


/*----------------------------------------------------------------------------
 * token_builder_build_token
 *
 * Create a packet token, which is required by the packet engine to
 * process the packet correctly.
 *
 * token_context_p (input/output)
 *   Pointer to an initialized token context record, as
 *   produced by token_builder_build_context(). Can be updated by this function.
 * packet_p (input)
 *   Pointer to the packet to be processed.
 * packet_byte_count (input)
 *   size of the packet in bytes (including any  bypass).
 * tkb_params_p (input, output)
 *   Additional per-packet parameters of the packet. Not all are defined for
 *   all protocols.
 * token_p (output)
 *   Buffer to store the token. It must point to a valid storage
 *   buffer large enough to hold the packet token. Before calling this
 *   function, the application must call token_builder_get_size() with the
 *   same token context and allocate a buffer of that size.
 * token_word32_count_p (output)
 *   Actual size of the packet token in 32-bit words, which may be less
 *   than the size returned by TokebBuilder_GetSize().
 * token_header_word_p (output)
 *   Token Header Word. The Reuse Context field (bits 21..20) is always
 *   set to 1 1 (Force update before reload), which ensures correct
 *   operation. For performance reasons, the application could modify this
 *   field as follows, after calling this function and before submitting the
 *   token to the packet engine:
 *   - If the token is generated for a packet operation using the same SA
 *     as the previous operation on the same packet engine, change the field to
 *     0 1 (clear bit 21).
 *   - Else change the field to 0 0 (clear both bits 21 and 20).
 *
 * Return:
 * TKB_STATUS_OK if successful.
 * TKB_INVALID_PARAMETER if any of the pointer parameters is NULL or if
 *                      the contents of the token context are invalid.
 * TKB_BAD_PACKET if the packet has an invalid header or its size is out of
 *                range or its size is wrongly aligned for a block cipher.
 */
token_builder_status_t
token_builder_build_token(
    void * const token_context_p,
    const u8 *const packet_p,
    const u32 packet_byte_count,
    token_builder_params_t * const tkb_params_p,
    void * const token_p,
    u32 * const token_word32_count_p,
    u32 * const token_header_word_p);


#endif /* TOKEN_BUILDER_H_ */


/* end of file token_builder.h */
