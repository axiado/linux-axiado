/* token_builder_macros.h
 *
 * Macros to be used inside the Token Builder. Main purpose is to evaluate
 * variables that are used inside generated code.
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


#ifndef TOKEN_BUILDER_MACROS_H_
#define TOKEN_BUILDER_MACROS_H_

#include "log.h"

/*----------------------------------------------------------------------------
 * Definitions and macros
 */


/* Values to be added to Context Control Word 0 for per-packet options */
#define TKB_PERPKT_HASH_FIRST     0x00000010
#define TKB_PERPKT_HASH_NO_FINAL  0x00000020
#define TKB_PERPKT_CTR_INIT       0x00000040
#define TKB_PERPKT_ARC4_INIT      0x00000080
#define TKB_PERPKT_XTS_INIT       0x00000080

#define TKB_PERPKT_HASH_STORE     0x00000040
#define TKB_PERPKT_HASH_CMPRKEY   0x00000080

#define TKB_RESFLAG_INIPV6        BIT_0
#define TKB_RESFLAG_FROMETHER     BIT_1
#define TKB_RESFLAG_OUTIPV6       BIT_2
#define TKB_RESFLAG_INBTUNNEL     BIT_3

#define eval_token_header_word() (eval_packetsize() | \
             token_context_internal_p->token_header_word | \
             (per_packet_options != 0?TKB_HEADER_C:0) | \
             (eval_u_word() != 0 ? TKB_HEADER_U:0))

#define eval_per_packet_options() packet_options(token_context_internal_p,\
                                                tkb_params_p->packet_flags, packet_byte_count - tkb_params_p->bypass_byte_count)

#define eval_u_word() tkb_u_word(token_context_internal_p, tkb_params_p)

#define eval_appendhash() ((unsigned)((tkb_params_p->packet_flags &   \
                                       TKB_PACKET_FLAG_HASHAPPEND) != 0 || \
                               token_context_internal_p->seq_offset==0))

/* The following values are each a parameter, either in the Token Context,
   or in the Packet Parameters or a parameter to the token builder function */
#define eval_cw0() ((unsigned)token_context_internal_p->u.generic.cw0)
#define eval_cw1() ((unsigned)token_context_internal_p->u.generic.cw1)
#define eval_proto() ((unsigned)token_context_internal_p->protocol)
#define eval_hproto() ((unsigned)token_context_internal_p->hproto)
#define eval_ivlen() ((unsigned)token_context_internal_p->iv_byte_count)
#define eval_icvlen() ((unsigned)token_context_internal_p->icv_byte_count)
#define eval_hstatelen() ((unsigned)token_context_internal_p->digest_word_count)
#define eval_hstatelen_bytes() ((unsigned)token_context_internal_p->digest_word_count*4)
#define eval_cipher_is_aes() ((unsigned)(token_context_internal_p->pad_block_byte_count==16))
#define eval_pad_blocksize()                               \
    pad_block_size(token_context_internal_p->pad_block_byte_count, 0)
#define eval_pad_blocksize_out()                               \
    pad_block_size(token_context_internal_p->pad_block_byte_count,  \
                 token_context_internal_p->hproto==0?tkb_params_p->additional_value:0)
#define eval_seq_offset() ((unsigned)token_context_internal_p->seq_offset)
#define eval_iv_offset() ((unsigned)token_context_internal_p->iv_offset)
#define eval_digest_offset() ((unsigned)token_context_internal_p->digest_offset)
#define eval_capwap_out() ((token_context_internal_p->u.generic.esp_flags & TKB_DTLS_FLAG_CAPWAP)?4:0)
#define eval_capwap_in() ((token_context_internal_p->u.generic.esp_flags & TKB_DTLS_FLAG_CAPWAP) && is_capwap(packet_p+bypass+hdrlen)?4:0)
#define eval_packetsize() ((unsigned)packet_byte_count)
#define eval_aadlen_pkt() (eval_aad()==NULL?(unsigned)tkb_params_p->additional_value:0)
#define eval_aadlen_tkn() (eval_aad()==NULL?0:(unsigned)tkb_params_p->additional_value)
#define eval_aadlen_out() (eval_extseq()==0?0:(unsigned)tkb_params_p->additional_value)
#define eval_swap_j() byte_swap32(tkb_params_p->additional_value)
#define eval_paylen_ccm() (eval_packetsize()-eval_bypass() - \
                                        eval_ivlen()-eval_aadlen_pkt()- \
                   (eval_proto()==TKB_PROTO_BASIC_CCM_IN || \
                    eval_proto()==TKB_PROTO_BASIC_CHACHAPOLY_IN?eval_icvlen():0))
#define eval_basic_swaplen() byte_swap32(eval_paylen_ccm())
#define eval_aadlen_swap() byte_swap16(tkb_params_p->additional_value)
#define eval_aadlen_swap32() byte_swap32(tkb_params_p->additional_value)
#define eval_aadpad() (padded_size(tkb_params_p->additional_value + 18 ,16) - \
                       tkb_params_p->additional_value - 18)
#define eval_aadpadpoly() (padded_size(tkb_params_p->additional_value, 16) -  \
                           tkb_params_p->additional_value)
#define eval_basic_hashpad() (padded_size( eval_paylen_ccm(), 16) - eval_paylen_ccm())

#define eval_bypass() ((unsigned)tkb_params_p->bypass_byte_count)
#define eval_ivhandling() ((unsigned)token_context_internal_p->iv_handling)
#define eval_upd_handling() ((unsigned)token_context_internal_p->u.generic.update_handling)
#define eval_extseq() ((unsigned)token_context_internal_p->ext_seq)
#define eval_antireplay() ((unsigned)token_context_internal_p->anti_replay)
#define eval_salt() ((unsigned)token_context_internal_p->u.generic.ccm_salt)
#define eval_basic_salt() (eval_salt() - (tkb_params_p->additional_value==0?0x40:0))
#define eval_swaplen() (eval_proto()==TKB_PROTO_SSLTLS_OUT||    \
                        eval_proto()==TKB_PROTO_SSLTLS_IN||     \
                        eval_proto()==TKB_PROTO_SSLTLS_GCM_OUT||\
                        eval_proto()==TKB_PROTO_SSLTLS_GCM_IN||  \
                        eval_proto()==TKB_PROTO_SSLTLS_CCM_OUT||\
                        eval_proto()==TKB_PROTO_SSLTLS_CCM_IN||  \
                        eval_proto()==TKB_PROTO_SSLTLS_CHACHAPOLY_OUT||\
                        eval_proto()==TKB_PROTO_SSLTLS_CHACHAPOLY_IN?  \
                         byte_swap16(eval_paylen()):             \
                         byte_swap32(eval_paylen()))
#define eval_swap_fraglen() byte_swap16(eval_packetsize() - eval_bypass() + \
       eval_ivlen() + eval_icvlen() - hdrlen + \
           (unsigned int)(eval_upd_handling() >= TKB_UPD_IV2) * \
                                       eval_pad_bytes())
#define eval_swap_fraglen_tls13() byte_swap16(eval_packetsize() - eval_bypass() + \
                                             eval_icvlen() + 1 + eval_count())
#define eval_hashpad() (padded_size( eval_paylen(), 16) - eval_paylen())

#define eval_paylen() payload_size(token_context_internal_p, \
                                  eval_packetsize() - eval_bypass() - hdrlen, \
                                  tkb_params_p->additional_value)
#define eval_iv() tkb_params_p->iv_p
#define eval_aad() tkb_params_p->aad_p

#define eval_paylen_tls13_ccm_out() (eval_packetsize() - eval_bypass() + \
                                     1 + eval_count())
#define eval_paylen_tls13_ccm_in() (eval_packetsize() - eval_bypass() - \
                                    eval_icvlen() - 5)
#define eval_swaplen3() byte_swap24(eval_paylen())
#define eval_swaplen3_tls13_out() byte_swap24(eval_paylen_tls13_ccm_out())
#define eval_swaplen3_tls13_in() byte_swap24(eval_paylen_tls13_ccm_in())
#define eval_hashpad_tls13_out() (padded_size( eval_paylen_tls13_ccm_out(), 16) - eval_paylen_tls13_ccm_out())
#define eval_hashpad_tls13_in() (padded_size( eval_paylen_tls13_ccm_in(), 16) - eval_paylen_tls13_ccm_in())


/* eval_pad_remainder() is used to check whether the packet payload
   on inbound packets is a multiple of the pad block size. If not,
   the packet is invalid and shall not be processed.
 */
#define eval_pad_remainder() tkb_pad_remainder(eval_packetsize() - 8 - \
                                           eval_ivlen() - eval_icvlen() - \
                                           eval_bypass() - hdrlen,     \
                                           eval_pad_blocksize())

/* eval_pad_bytes() is used to compute the number of bytes that must
   be added to outbound packets.

   For IPsec, two bytes (next header and the number of padded bytes)
   must be added in any case. It is the difference between the padded
   payload size and the unpadded payload size.

   For SSL and TLS, one byte (number of padded bytes) must be added in
   any case.  Padding has to be applied to the payload plus MAC.
*/
#define eval_pad_bytes() compute_pad_bytes(token_context_internal_p,\
                            eval_packetsize()-eval_bypass()-hdrlen,     \
                            eval_pad_blocksize_out())
#define eval_pad_bytes_basic() (padded_size(eval_packetsize() - eval_bypass() -eval_antireplay() - eval_aadlen_pkt(), \
                  token_context_internal_p->pad_block_byte_count) -  \
                                eval_packetsize()+eval_bypass()+eval_antireplay()+eval_aadlen_pkt())
#define eval_pad_bytes_hashenc() compute_pad_bytes(token_context_internal_p, \
            eval_packetsize() - eval_bypass() + \
             token_context_internal_p->pad_block_byte_count - eval_aadlen_pkt(), \
            pad_block_size(token_context_internal_p->pad_block_byte_count,tkb_params_p->pad_byte + 1))

#define eval_hash_prev_bytes() (token_context_internal_p->tunnel_ip)
#define eval_hash_prev_len()   (token_context_internal_p->msg_len)
#define eval_hash_cur_len()    (token_context_internal_p->u.generic.ccm_salt)
#define eval_hash_is_final()   ((per_packet_options & TKB_PERPKT_HASH_NO_FINAL)==0)
#define eval_hash_next_len()   compute_next_block(token_context_internal_p, \
                                packet_p + tkb_params_p->bypass_byte_count, \
                                packet_byte_count - tkb_params_p->bypass_byte_count);
#define eval_gcm_state()       compute_gcm_state(token_context_internal_p, \
                                               packet_byte_count -        \
                                               tkb_params_p->bypass_byte_count - \
                                               tkb_params_p->additional_value, \
                                               eval_hash_is_final());

#define eval_srtp_iv0() (token_context_internal_p->u.srtp.salt_key0)
#define eval_srtp_iv1() tkb_srtp_iv1(token_context_internal_p,\
                                 packet_p + eval_bypass())
#define eval_srtp_iv2() tkb_srtp_iv2(token_context_internal_p, \
                                 packet_p + eval_bypass(), \
                                 packet_byte_count - eval_bypass(), \
                                 tkb_params_p->additional_value)
#define eval_srtp_iv3() tkb_srtp_iv3(token_context_internal_p, \
                                 packet_p + eval_bypass(), \
                                 packet_byte_count - eval_bypass(), \
                                 tkb_params_p->additional_value)
#define eval_srtp_offset() srtp_offset(token_context_internal_p, \
                                       packet_p + eval_bypass(),  \
                                       packet_byte_count - eval_bypass(), \
                                       tkb_params_p->additional_value)
#define eval_srtp_swaproc() byte_swap32(tkb_params_p->additional_value)


#define eval_ssltls_lastblock() (packet_p + eval_packetsize() - 16 - 16*eval_cipher_is_aes())

#define eval_ssltls_lastword() (packet_p + eval_packetsize() - 4)

#define eval_count() tkb_params_p->additional_value
#define eval_bearer_dir_fresh() token_context_internal_p->u.generic.ccm_salt

#if TKB_HAVE_PROTO_IPSEC==1|TKB_HAVE_PROTO_SSLTLS==1
/*----------------------------------------------------------------------------
 * tkb_pad_remainder
 *
 * Compute the remainder of byte_count when devided by block_byte_count..
 * block_byte_count must be a power of 2.
 */
static unsigned int
tkb_pad_remainder(unsigned int byte_count,
             unsigned int block_byte_count)
{
    unsigned int val;
    val = byte_count & (block_byte_count - 1);
    LOG_INFO("token_builder_build_token: Input message %u pad align=%u "
             "remainder=%u\n",
             byte_count, block_byte_count,val);
    return val;
}
#endif

#if TKB_HAVE_PROTO_IPSEC==1||TKB_HAVE_PROTO_SSLTLS==1||TKB_HAVE_PROTO_BASIC==1
/*----------------------------------------------------------------------------
 * padded_size
 *
 * Compute the size of a packet of length byte_count when padded to a multiple
 * of block_byte_count.
 *
 * block_byte_count must be a power of 2.
 */
static unsigned int
padded_size(unsigned int byte_count,
           unsigned int block_byte_count)
{
    return (byte_count + block_byte_count - 1) & ~(block_byte_count - 1);
}
#endif

#if TKB_HAVE_PROTO_IPSEC==1||TKB_HAVE_PROTO_SSLTLS==1||TKB_HAVE_PROTO_BASIC==1
/*----------------------------------------------------------------------------
 * pad_block_size
 *
 * Select the pad block size: select the override value if it is appropriate,
 * else select the default value.
 *
 * The override value is appropriate if it is a power of two and if it is
 * greater than the default value and at most 256.
 */
static unsigned int
pad_block_size(unsigned int default_val,
             unsigned int override_val)
{
    if (override_val > default_val  &&
        override_val <= 256 &&
        (override_val  & (override_val - 1)) == 0) // Check power of two.
    {
        LOG_INFO("TokenBuilder_Buildtoken: Override pad alignment from %u to %u\n",
                 default_val, override_val);
        return override_val;
    }
    else
    {
        return default_val;
    }
}
#endif

#if TKB_HAVE_PROTO_SSLTLS==1
/*----------------------------------------------------------------------------
 * is_capwap
 *
 * Check if there is a CAPWAP header at the given location in the packet.
 */
static inline bool
is_capwap(const u8 * const packet_p)
{
    return packet_p[0]==1;
}

#endif
/*----------------------------------------------------------------------------
 * byte_swap32
 *
 * Return a 32-bit value byte-swapped.
 */
static inline unsigned int
byte_swap32(unsigned int val)
{
    return ((val << 24) & 0xff000000) |
           ((val << 8)  & 0x00ff0000) |
           ((val >> 8)  & 0x0000ff00) |
           ((val >> 24) & 0x000000ff);
}

#if TKB_HAVE_PROTO_IPSEC==1||TKB_HAVE_PROTO_SSLTLS==1||TKB_HAVE_PROTO_BASIC==1
/*----------------------------------------------------------------------------
 * byte_swap16
 *
 * Return a 16-bit value byte-swapped.
 */
static inline unsigned int
byte_swap16(unsigned int val)
{
    return ((val << 8) & 0xff00) |
           ((val >> 8) & 0x00ff);
}


/*----------------------------------------------------------------------------
 * byte_swap24
 *
 * Return a 24-bit value byte-swapped.
 */
static inline unsigned int
byte_swap24(unsigned int val)
{
    return ((val << 16) & 0x00ff0000) |
           ( val        & 0x0000ff00) |
           ((val >> 16) & 0x000000ff);
}

#endif


/*----------------------------------------------------------------------------
 * packet_options
 *
 * Return the per-packet options depending on the packet flags and the protocol.
 */
static inline unsigned int
packet_options(token_builder_context_t * token_context_p,
              unsigned int flags, unsigned int packet_byte_count)
{
    unsigned int val = 0;
    unsigned int protocol = token_context_p->protocol;

    if ((protocol == TKB_PROTO_SSLTLS_OUT || protocol == TKB_PROTO_SSLTLS_IN ||
            protocol == TKB_PROTO_BASIC_CRYPTO) &&
        (flags & TKB_PACKET_FLAG_ARC4_INIT) != 0)
        val |= TKB_PERPKT_ARC4_INIT;

    if (protocol == TKB_PROTO_BASIC_XTS_CRYPTO &&
        (flags & TKB_PACKET_FLAG_XTS_INIT) != 0)
        val |= TKB_PERPKT_XTS_INIT;


    if ((protocol == TKB_PROTO_BASIC_HASH &&
        ((token_context_p->seq_offset != 0 &&
         token_context_p->digest_word_count > 16) ||
         (token_context_p->u.generic.cw0 & 0x07800000) == 0x07800000)) ||
         protocol == TKB_PROTO_BASIC_GCM_OUT_CONT ||
         protocol == TKB_PROTO_BASIC_GCM_IN_CONT)
    {
        if ((flags & TKB_PACKET_FLAG_HASHFIRST) != 0)
            val |= TKB_PERPKT_HASH_FIRST;
        if ((flags & TKB_PACKET_FLAG_HASHFINAL) == 0)
            val |= TKB_PERPKT_HASH_NO_FINAL;
    }

    if (protocol == TKB_PROTO_BASIC_HASH_UNALIGNED)
    {
        if ((flags & TKB_PACKET_FLAG_HASHFIRST) != 0)
        {
            token_context_p->msg_len = 0;
            token_context_p->f_first = true;
        }
        if (token_context_p->msg_len + packet_byte_count > token_context_p->hash_block_size)
        {
            token_context_p->u.generic.ccm_salt =
                (token_context_p->msg_len + packet_byte_count -1) /
                token_context_p->hash_block_size *
                token_context_p->hash_block_size;
            if (token_context_p->f_first) {
                val |= TKB_PERPKT_HASH_FIRST;
            }
        }
        else
        {
            token_context_p->u.generic.ccm_salt = 0;
        }
        if ((flags & TKB_PACKET_FLAG_HASHFINAL) == 0) {
            val |= TKB_PERPKT_HASH_NO_FINAL;
        } else if (token_context_p->f_first) {
            val |= TKB_PERPKT_HASH_FIRST;
        }
    }

    if (protocol == TKB_PROTO_BASIC_SHAKE)
    {
        if ((flags & TKB_PACKET_FLAG_HASHFIRST) != 0)
        {
            val |= TKB_PERPKT_HASH_FIRST;
            token_context_p->msg_len = 0;
            token_context_p->f_first = false;
        }
        if (token_context_p->f_first)
        {
            token_context_p->msg_len = 255;
        }
        else if ((flags & TKB_PACKET_FLAG_HASHFINAL) != 0)
        {
            token_context_p->msg_len = packet_byte_count / 136 * 136 + 136 - packet_byte_count;
            /* Create the SHAKE/cSHAKE padding */
            token_context_p->tunnel_ip[0] = token_context_p->icv_byte_count;
            if (token_context_p->msg_len > 1)
            {
                memset(token_context_p->tunnel_ip + 1, 0, token_context_p->msg_len - 1);
            }
            token_context_p->tunnel_ip[token_context_p->msg_len-1] |= 0x80;
            token_context_p->f_first = true;
        }
        val |= TKB_PERPKT_HASH_NO_FINAL; /* Never finalize to the SHA3 core*/
    }

    if (protocol == TKB_PROTO_BASIC_HMAC_PRECOMPUTE ||
        protocol == TKB_PROTO_BASIC_HMAC_CTXPREPARE)
    {
        unsigned int key_limit = 64;
        if (token_context_p->digest_word_count==16)
            key_limit = 128;

        val |= TKB_PERPKT_HASH_FIRST;
        if (packet_byte_count > key_limit)
            val |= TKB_PERPKT_HASH_CMPRKEY;

        if (protocol == TKB_PROTO_BASIC_HMAC_CTXPREPARE)
            val |= TKB_PERPKT_HASH_STORE;
    }

    if (val != 0)
    {
        LOG_INFO("token_builder_build_token: non-zero per-packet options: %x\n",
                 val);
    }

    return val;
}

/*----------------------------------------------------------------------------
 * u_word
 *
 * Return the U-word that is optionally present in the Token. It can be used
 * to specify non-byte aligned packet sizes in SNOW and ZUC authentication
 * algorithms or to specify the per-packet window size for inbound IPSsec ESP.
 */
static inline unsigned int
tkb_u_word(const token_builder_context_t * token_context_p,
      const token_builder_params_t *tkb_params_p)
{
    switch (token_context_p->protocol)
    {
    case TKB_PROTO_ESP_IN:
    case TKB_PROTO_ESP_CCM_IN:
    case TKB_PROTO_ESP_GCM_IN:
    case TKB_PROTO_ESP_GMAC_IN:
    case TKB_PROTO_ESP_CHACHAPOLY_IN:
        if (token_context_p->anti_replay < 32)
            return 0;
        switch(tkb_params_p->additional_value)
        {
        case 64:
            return 0x00200000;
        case 128:
            return 0x00400000;
        case 256:
            return 0x00600000;
        case 512:
            return 0x00800000;
        default:
            return 0;
        }
    case TKB_PROTO_BASIC_SNOW_HASH:
    case TKB_PROTO_BASIC_ZUC_HASH:
        return tkb_params_p->pad_byte;
    default:
        return 0;
    }
}

#if TKB_HAVE_PROTO_BASIC==1
/*----------------------------------------------------------------------------
 * compute_next_block
 *
 * For the unaligned hash operation, prepare the left-over bytes from the
 * current packet that must be carried over to the next hash operation.
 */
static unsigned int
compute_next_block(token_builder_context_t *token_context_p,
                 const u8 *packet_p,
                 const unsigned int packet_byte_count)
{
    if (token_context_p->u.generic.ccm_salt != 0) {
        token_context_p->f_first = false;
        /* data was hashed during current operation */
        token_context_p->msg_len = packet_byte_count - token_context_p->u.generic.ccm_salt + token_context_p->msg_len;
        /* Replace buffer with tail of current packet  */
        memcpy(token_context_p->tunnel_ip,
               packet_p+packet_byte_count-token_context_p->msg_len,
               token_context_p->msg_len);
        return token_context_p->msg_len;
    }
    else
    {
        /* No data was hashed during current operation */
        /* Append current packet to buffer */
        memcpy(token_context_p->tunnel_ip + token_context_p->msg_len,
               packet_p+packet_byte_count-packet_byte_count,
               packet_byte_count);
        token_context_p->msg_len += packet_byte_count;
        return packet_byte_count;
    }
}

/*----------------------------------------------------------------------------
 * compute_next_block
 *
 * For the GCM continuation function, return the state of the current packet:
 * 0 conttains AAD data and no payload.
 * 1 contains the first data to be encrypted, need to prepare Y0.
 * 2 bloxk in message after the first block with encrypted data, allow no AAD,
 *   no need to prepare Y0.
 */
static unsigned int
compute_gcm_state(token_builder_context_t *token_context_p,
                unsigned int payload_byte_count,
                unsigned int is_final)
{
    if (token_context_p->u.generic.ccm_salt) {
        return 2;
    } else if (payload_byte_count > 0 || is_final != 0) {
        token_context_p->u.generic.ccm_salt=1;
        return 1;
    } else {
        return 0;
    }
}

#endif

#if TKB_HAVE_PROTO_IPSEC==1||TKB_HAVE_PROTO_SSLTLS==1|TKB_HAVE_PROTO_BASIC==1
/*----------------------------------------------------------------------------
 * compute_pad_bytes
 *
 * token_context_p (input)
 *     Token context.
 * payload_byte_count (input)
 *    size of message to be padded in bytes.
 * pad_block_byte_count (input)
 *    Block size to which the payload must be padded.
 */
static unsigned int
compute_pad_bytes(const token_builder_context_t *token_context_p,
                unsigned int payload_byte_count,
                unsigned int pad_block_byte_count)
{
    unsigned int pad_byte_count;
    if(token_context_p->protocol == TKB_PROTO_SSLTLS_OUT ||
        token_context_p->protocol == TKB_PROTO_BASIC_HASHENC)
    {
        pad_byte_count = padded_size(payload_byte_count + 1 +
                                  token_context_p->iv_byte_count +
                                  token_context_p->icv_byte_count,
                                  pad_block_byte_count) -
          payload_byte_count - token_context_p->icv_byte_count -
          token_context_p->iv_byte_count;
        LOG_INFO("token_builder_build_token: SSL/TLS padding message %u\n"
                 "  with mac %u pad bytes=%u align=%u\n",
                 payload_byte_count,
                 token_context_p->icv_byte_count,
                 pad_byte_count, pad_block_byte_count);
    }
    else
    {
        pad_byte_count =  padded_size(payload_byte_count + 2, pad_block_byte_count) -
            payload_byte_count;
        LOG_INFO("token_builder_build_token: IPsec padding message %u "
                 "pad bytes=%u align=%u\n",
                 payload_byte_count, pad_byte_count, pad_block_byte_count);
    }
    return pad_byte_count;
}
#endif



#if TKB_HAVE_PROTO_IPSEC==1||TKB_HAVE_PROTO_SSLTLS==1
/*----------------------------------------------------------------------------
 * payload_size
 *
 * token_context_p (input)
 *     Token context.
 * message_byte_count (input)
 *     size of the input packet, excluding bypass.
 * additional_value
 *     Additional value supplied in pacekt parameters. This may be a pad
 *     block size or the number of pad bytes.
 *
 * Compute the payload length in bytes for all SSL/TLS and for ESP with AES-CCM.
 * This is not used for other ESP modes.
 */
static unsigned int
payload_size(const token_builder_context_t *token_context_p,
            unsigned int message_byte_count,
            unsigned int additional_value)
{
    int size;
    switch(token_context_p->protocol)
    {
    case TKB_PROTO_ESP_CCM_OUT:
        /* Need paddded payload size, pad message */
        return padded_size(message_byte_count + 2,
           pad_block_size(token_context_p->pad_block_byte_count, additional_value));
    case TKB_PROTO_ESP_CCM_IN:
        /* Need paddded payload size, derive from message length, subtract
           headers and trailers.*/
        return message_byte_count - token_context_p->iv_byte_count -
            token_context_p->icv_byte_count - 8;
    case TKB_PROTO_SSLTLS_OUT:
    case TKB_PROTO_SSLTLS_GCM_OUT:
    case TKB_PROTO_SSLTLS_CCM_OUT:
    case TKB_PROTO_SSLTLS_CHACHAPOLY_OUT:
        /* Need fragment length */
        return message_byte_count;
    case TKB_PROTO_SSLTLS_IN:
    case TKB_PROTO_SSLTLS_GCM_IN:
    case TKB_PROTO_SSLTLS_CCM_IN:
    case TKB_PROTO_SSLTLS_CHACHAPOLY_IN:
        /* Need fragment length. Must remove any headers, padding and trailers
         */
        size = (int)message_byte_count;
        if ((token_context_p->u.generic.esp_flags & TKB_DTLS_FLAG_CAPWAP) != 0)
            size -= 4;
        // Deduce CAPWAP/DTLS header.
        size -= token_context_p->icv_byte_count;
        size -= token_context_p->iv_byte_count;
        if (token_context_p->ext_seq != 0)
            size -= 8; /* Deduce epoch and sequence number for DTLS */
        size -= 5;   /* Deduce type, version and fragment length. */
        if (size < 0)
            return 0xffffffffu;
        if(token_context_p->u.generic.update_handling != TKB_UPD_NULL &&
           token_context_p->u.generic.update_handling != TKB_UPD_ARC4)
        {
            if (tkb_pad_remainder((unsigned int)size + token_context_p->icv_byte_count,
                             token_context_p->pad_block_byte_count) != 0)
                /* Padded payload + ICV must be a multiple of block size */
                return 0xffffffffu;
/* Report negative payload size due to subtraction of pad bytes as 0
 * rather than 0xffffffff, so the the operation will still be
 * execcuted. This to avoid timing attacks. See RFC5246, section
 * 6.2.3.2. */
        }
        return (unsigned int)size;
    default:
        LOG_CRIT("Token Builder: payload_size used with unsupported protocol\n");
        return 0;
    }
}
#endif

/* tkb_srtp_iv1

   Form second word of iv.  (Exclusive or of salt key and SSRC).
*/
static unsigned int
tkb_srtp_iv1(
        const token_builder_context_t *token_context_p,
        const u8 *packet_p)
{
    unsigned int ssrc_offset;
    if (token_context_p->ext_seq != 0)
    { // SRTCP
        ssrc_offset = 4;
    }
    else
    { // SRTP
        ssrc_offset = 8;
    }

    if (packet_p == NULL)
        return 0;

    return token_context_p->u.srtp.salt_key1 ^ ((packet_p[ssrc_offset]) |
                                              (packet_p[ssrc_offset+1]<<8) |
                                              (packet_p[ssrc_offset+2]<<16) |
                                              (packet_p[ssrc_offset+3]<<24));
}

/* tkb_srtp_iv2

   Form third word of iv (SRTCP: exclusive or with MSB of SRTCP index.
   SRTP: Exclusive or with ROC).
*/
static unsigned int
tkb_srtp_iv2(
        const token_builder_context_t *token_context_p,
        const u8 *packet_p,
        unsigned int message_byte_count,
        unsigned int additional_value)
{
    unsigned int byte_offset;

    if (packet_p == NULL)
        return 0;

    if (token_context_p->ext_seq != 0)
    { // SRTCP
        u32 srtcp_index;
        if (token_context_p->protocol == TKB_PROTO_SRTP_OUT)
        {   // For outbound packet, SRTCP index is supplied parameter.
            srtcp_index = additional_value & 0x7fffffff;
        }
        else
        {   // For inbound, extract it from the packet.
            byte_offset = message_byte_count - 4 - token_context_p->anti_replay -
                token_context_p->icv_byte_count;
            srtcp_index = ((packet_p[byte_offset]&0x7f)<<24) |
                (packet_p[byte_offset+1]<<16) ;
        }
        return token_context_p->u.srtp.salt_key2 ^ byte_swap32(srtcp_index>>16);
    }
    else
    { // SRTP
        return token_context_p->u.srtp.salt_key2 ^ byte_swap32(additional_value);
    }
}



/* tkb_srtp_iv3

   Form fourth word of iv (SRTCP: exclusive or with LSB of SRTCP index.
   SRTP: Exclusive or with seq no in packet).
*/
static unsigned int
tkb_srtp_iv3(
        const token_builder_context_t *token_context_p,
        const u8 *packet_p,
        unsigned int message_byte_count,
        unsigned int additional_value)
{
    unsigned int byte_offset;

    if (packet_p == NULL)
        return 0;

    if (token_context_p->ext_seq != 0)
    { // SRTCP
        u32 srtcp_index;
        if (token_context_p->protocol == TKB_PROTO_SRTP_OUT)
        {   // For outbound packet, SRTCP index is supplied parameter.
            srtcp_index = additional_value;
        }
        else
        {   // For inbound, extract it from the packet.
            byte_offset = message_byte_count - 4 - token_context_p->anti_replay -
                token_context_p->icv_byte_count;
            srtcp_index = (packet_p[byte_offset+2]<<8) |
                (packet_p[byte_offset+3]) ;
        }
        return token_context_p->u.srtp.salt_key3 ^ byte_swap32(srtcp_index<<16);
    }
    else
    {
        return token_context_p->u.srtp.salt_key3 ^ (packet_p[2] |
                                                  (packet_p[3]<<8));
    }
}

#if TKB_HAVE_PROTO_SRTP==1
/* srtp_offset

   Compute the crypto offset (in bytes) of an SRTP packet.

   If no crypto used: return 0.
   If packet is invalid: return offset larger than message size.
*/
static unsigned int
srtp_offset(
        const token_builder_context_t *token_context_p,
        const u8 *packet_p,
        unsigned int message_byte_count,
        unsigned int additional_value)
{
    unsigned int byte_offset;
    if (token_context_p->iv_byte_count == 0)
        return 0;

    if (packet_p == NULL)
        return 0;

    if (token_context_p->ext_seq != 0)
    { // SRTCP
        u32 srtcp_index;
        if (token_context_p->protocol == TKB_PROTO_SRTP_OUT)
        {   // For outbound packet, SRTCP index is supplied parameter.
            srtcp_index = additional_value;
        }
        else
        {   // For inbound, extract it from the packet.
            byte_offset = message_byte_count - 4 - token_context_p->anti_replay -
                token_context_p->icv_byte_count;
            if (byte_offset > message_byte_count)
            {
                LOG_INFO("token_builder_build_token: Short packet\n");
                return message_byte_count + 1;

            }
            srtcp_index = packet_p[byte_offset]<<24; // Only get MSB.
        }
        if ((srtcp_index & BIT_31) != 0) // Test the E bit.
            return 8; // SRTCP always has crypto offset 8.
        else
            return 0; // Return 0 if no encryption is used.
    }
    else
    { // SRTP
        unsigned int ncsrc = packet_p[0] & 0xf; // number of CSRC fields.
        byte_offset = 12 + 4*ncsrc;
        if ( (packet_p[0] & BIT_4) != 0) // Extension header present.
        {
            if (byte_offset + 4 > message_byte_count)
            {
                LOG_INFO("token_builder_build_token: Short packet\n");
                return message_byte_count + 1;
            }
            byte_offset += 4 + 4*(packet_p[byte_offset+2]<<8) +
                4*packet_p[byte_offset+3]; // Add length field from extension.
        }
        return byte_offset;
    }
}
#endif


#if TKB_HAVE_EXTENDED_IPSEC==1 || TKB_HAVE_EXTENDED_DTLS==1


/*-----------------------------------------------------------------------------
 * token_builder_parse_ip_header
 *
 * Parse ths IP header and provide values for the following variables
 * - hdrlen (returned). number of bytes to skip in input packet before
 *                      inserting/removing ESP header.
 * - ohdrlen            number of bytes in output packet before
 *                      ESP header or decrypted payload.
 * - outlen             Length of output IP packet (outbound only).
 * - nh                 Next header byte in IPv6 header (if applicable)
 *                      For tunnel outbound modes (IPv4 or IPv6), it is the
 *                      dscp+ECN byte instead.
 * - nextheader         Next header byte to insert in ESP trailer.
 * - prev_nhoffset      offset in packet where next header byte is replaced.
 *                      For IPv6 tunnel mode, it is the flow label instead.
 * - ecn_fixup_instr    ECN fixup instruction for inbound tunnel mode.
 */
static inline u32
token_builder_parse_ip_header(
        const u8 *packet_p,
        const unsigned int packet_byte_count,
        const token_builder_context_t *token_context_internal_p,
        token_builder_params_t * tkb_params_p,
        u32 *ohdrlen_p,
        u32 *outlen_p,
        u32 *nh_p,
        u32 *nextheader_p,
        u32 *prev_nhoffset_p,
        u32 *ecn_fixup_instr_p)
{
    u32 nh = 0;
    u32 ohdrlen = 0;
    u32 outlen = 0;
    u8 nextheader = tkb_params_p->pad_byte;
    u32 prev_nhoffset = 0;
    u32 hdrlen = 0;
    u32 ecn_fixup_instr = 0x20000000;
    u8 hproto=token_context_internal_p->hproto;
    bool f_ipv6;
    u32 max_hdr_len = tkb_params_p->prev_nh_offset;

    tkb_params_p->cle = 0;
    tkb_params_p->prev_nh_offset = 0;
    tkb_params_p->TOS_TC_DF = 0;
    tkb_params_p->result_flags = 0;

    if (hproto)
    {
        /* Check whether the packet is IPv6. For outbound tunnel modes
           this is the only form of header parsing done. For non-protocol
           modes, do not look at the header at all

           Also collect the TOS bytes (dscp+ECN) and the IPv6 flow label.*/
        if (packet_byte_count < 20)
        {
            tkb_params_p->cle = 3;
            return 0xffffffff;
        }
        f_ipv6 = (packet_p[0] & 0xf0) == 0x60;
        tkb_params_p->result_flags |= f_ipv6;
        if ((f_ipv6 && packet_byte_count < 40) ||
            (!f_ipv6 && (packet_p[0] & 0xf0) != 0x40))
        {
            tkb_params_p->cle = 3;
            return 0xffffffff;
        }
        if (!f_ipv6)
        {
            nh = packet_p[1]; /* dscp+ECN byte */
            tkb_params_p->TOS_TC_DF = nh | ((packet_p[6]&0x40)?0x100:0);
        }
        else
        {
            u32 w = (packet_p[0]<<24) | (packet_p[1]<<16) |
                (packet_p[2]<<8) |  packet_p[3];
            nh = (w>>20) & 0xff; /* dscp+ECN byte */
            tkb_params_p->TOS_TC_DF = nh;
            if (tkb_params_p->packet_flags & TKB_PACKET_FLAG_COPY_FLOWLABEL)
                prev_nhoffset = w & 0xfffff; /* Flow label */
        }
    }

    switch (hproto)
    {
    case 0:
        break;
    case TKB_HDR_IPV4_OUT_TUNNEL:
        ohdrlen = 20;
        nextheader = f_ipv6 ? 41 : 4;
        break;
    case TKB_HDR_IPV6_OUT_TUNNEL:
        ohdrlen = 40;
        nextheader = f_ipv6 ? 41 : 4;
        break;
    case TKB_HDR_IPV4_OUT_TUNNEL_NATT:
        ohdrlen = 28;
        nextheader = f_ipv6 ? 41 : 4;
        nh = packet_p[1]; /* dscp+ECN byte */
        break;
    case TKB_HDR_IPV6_OUT_TUNNEL_NATT:
        ohdrlen = 48;
        nextheader = f_ipv6 ? 41 : 4;
        break;
    default:
        /* Parse the IP header, sufficiently to find the location of the
           ESP header or where it should be inserted. */
        if (f_ipv6)
        {
            int chain_state = 0;
            // 0 no specifics encountered.
            // 1 routing header encountered, immediately exit when
            //   destination opts header found (before parsing that header).

            if (max_hdr_len == 0 || max_hdr_len > packet_byte_count)
            {
                max_hdr_len = packet_byte_count;
            }
            nh = packet_p[6];
            prev_nhoffset = 6;
            hdrlen = 40;
            nextheader = nh;
            while (nextheader == 0 || nextheader == 43 ||
                   nextheader == 44 || nextheader == 60)
            {
                if (max_hdr_len < hdrlen + 8) {
                    tkb_params_p->cle = 3;
                    return 0xffffffff;
                }
                /* Assume we want to encapsulate any destination options
                   after the routine header */
                if (nextheader == 43 &&
                    (hproto == TKB_HDR_IPV6_OUT_TRANSP ||
                     hproto == TKB_HDR_IPV6_OUT_TRANSP_HDRBYPASS ||
                     hproto == TKB_HDR_IPV6_OUT_TRANSP_NATT ||
                     hproto == TKB_HDR_IPV6_OUT_TRANSP_HDRBYPASS_NATT))
                {
                    chain_state=1;
                }
                /* Fragment header encountered, these packets cannot be
                   processed.*/
                if (nextheader == 44)
                {
                    tkb_params_p->cle = 1;
                    return 0xffffffff;
                }
                nextheader = packet_p[hdrlen];
                prev_nhoffset = hdrlen;
                hdrlen += packet_p[hdrlen+1] * 8 + 8;
                if ((nextheader == 60 && chain_state == 1) ||
                    chain_state == 2)
                {
                    break;
                }
            }
        }
        else
        {
            hdrlen = (packet_p[0] & 0xf) * 4;
            nextheader = packet_p[9];
            /* Special protocol 254 for Firmware tests, encode the
               desired nextheader byte in pkt_id field */
            if (nextheader == 254)
                nextheader = packet_p[5];
            /* Check for fragment, reject fragments  */
            if ((packet_p[6] & ~0xc0) != 0 || packet_p[7] != 0)
            {
                    tkb_params_p->cle = 1;
                    return 0xffffffff;
            }
        }

        switch (hproto)
        {
        case TKB_HDR_IPV4_OUT_TRANSP:
        case TKB_HDR_IPV4_OUT_TRANSP_HDRBYPASS:
            if (f_ipv6)
            {
                tkb_params_p->cle = 3;
                return 0xffffffff;
            }
            ohdrlen = hdrlen;
            break;
        case TKB_HDR_IPV6_OUT_TRANSP:
        case TKB_HDR_IPV6_OUT_TRANSP_HDRBYPASS:
            if (!f_ipv6)
            {
                tkb_params_p->cle = 3;
                return 0xffffffff;
            }
            tkb_params_p->result_flags |= TKB_RESFLAG_OUTIPV6;
            ohdrlen = hdrlen;
            break;
        case TKB_HDR_IPV4_IN_TRANSP:
        case TKB_HDR_IPV4_IN_TRANSP_HDRBYPASS:
            if (nextheader != 50 || f_ipv6)
            {

                tkb_params_p->cle = 3;
                return 0xffffffff;
            }
            ohdrlen = hdrlen;
            break;
        case TKB_HDR_IPV6_IN_TRANSP:
        case TKB_HDR_IPV6_IN_TRANSP_HDRBYPASS:
            if (nextheader != 50 || !f_ipv6)
            {

                tkb_params_p->cle = 3;
                return 0xffffffff;
            }
            tkb_params_p->result_flags |= TKB_RESFLAG_OUTIPV6;
            tkb_params_p->prev_nh_offset = prev_nhoffset +
                tkb_params_p->bypass_byte_count ;
            ohdrlen = hdrlen;
            break;
        case TKB_HDR_IPV4_OUT_TRANSP_NATT:
        case TKB_HDR_IPV4_OUT_TRANSP_HDRBYPASS_NATT:
            if (f_ipv6)
            {
                tkb_params_p->cle = 3;
                return 0xffffffff;
            }
            ohdrlen = hdrlen + 8;
            break;
        case TKB_HDR_IPV6_OUT_TRANSP_NATT:
        case TKB_HDR_IPV6_OUT_TRANSP_HDRBYPASS_NATT:
            if (!f_ipv6)
            {
                tkb_params_p->cle = 3;
                return 0xffffffff;
            }
            tkb_params_p->result_flags |= TKB_RESFLAG_OUTIPV6;
            ohdrlen = hdrlen + 8;
            break;
        case TKB_HDR_IPV4_IN_TRANSP_NATT:
        case TKB_HDR_IPV4_IN_TRANSP_HDRBYPASS_NATT:
            if (nextheader != 17 || f_ipv6)
            {

                tkb_params_p->cle = 3;
                return 0xffffffff;
            }
            ohdrlen = hdrlen;
            hdrlen += 8;
            break;
        case TKB_HDR_IPV6_IN_TRANSP_NATT:
        case TKB_HDR_IPV6_IN_TRANSP_HDRBYPASS_NATT:
            if (nextheader != 17 || !f_ipv6)
            {

                tkb_params_p->cle = 3;
                return 0xffffffff;
            }
            tkb_params_p->prev_nh_offset = prev_nhoffset  +
                tkb_params_p->bypass_byte_count;
            tkb_params_p->result_flags |= TKB_RESFLAG_OUTIPV6;
            ohdrlen = hdrlen;
            hdrlen += 8;
            break;
        case TKB_HDR_IPV4_IN_TUNNEL:
        case TKB_HDR_IPV6_IN_TUNNEL:
            if (nextheader != 50)
            {

                tkb_params_p->cle = 3;
                return 0xffffffff;
            }
            if ((tkb_params_p->packet_flags & TKB_PACKET_FLAG_KEEP_OUTER) != 0)
                ohdrlen = hdrlen;
            tkb_params_p->result_flags |= TKB_RESFLAG_INBTUNNEL;
            ecn_fixup_instr = 0xa6000000 +
                ((tkb_params_p->TOS_TC_DF & 0x3)<<19)  +
                ohdrlen + tkb_params_p->bypass_byte_count;
            break;
        case TKB_HDR_IPV4_IN_TUNNEL_NATT:
        case TKB_HDR_IPV6_IN_TUNNEL_NATT:
            if (nextheader != 17)
            {

                tkb_params_p->cle = 3;
                return 0xffffffff;
            }
            if ((tkb_params_p->packet_flags & TKB_PACKET_FLAG_KEEP_OUTER) != 0)
                ohdrlen = hdrlen;
            hdrlen += 8;
            tkb_params_p->result_flags |= TKB_RESFLAG_INBTUNNEL;
            ecn_fixup_instr = 0xa6000000 +
                ((tkb_params_p->TOS_TC_DF & 0x3)<<19)  +
                ohdrlen + tkb_params_p->bypass_byte_count;
            break;
        case TKB_HDR_IPV4_OUT_DTLS:
        case TKB_HDR_IPV6_OUT_DTLS:
            hdrlen += 8;
            ohdrlen = hdrlen;
            /* Must be UDP or UDPlite */
            if (nextheader != 17 && nextheader != 136)
            {

                tkb_params_p->cle = 3;
                return 0xffffffff;
            }
            nh = nextheader;
            prev_nhoffset = packet_p[6];
            nextheader = tkb_params_p->pad_byte;
            if (f_ipv6) tkb_params_p->result_flags |= TKB_RESFLAG_OUTIPV6;
            break;
        case TKB_HDR_DTLS_UDP_IN:
            hdrlen += 8;
            ohdrlen = hdrlen;
            /* Must be UDP or UDPlite */
            if (nextheader != 17 && nextheader != 136)
            {

                tkb_params_p->cle = 3;
                return 0xffffffff;
            }
            if ((token_context_internal_p->u.generic.esp_flags &
                 TKB_DTLS_FLAG_PLAINTEXT_HDR) != 0)
            {
                ohdrlen += 5;
                if (tkb_params_p->pad_byte >= 8)
                {
                    ohdrlen += (tkb_params_p->pad_byte - 5);
                }
            }
            if (f_ipv6) tkb_params_p->result_flags |= TKB_RESFLAG_OUTIPV6;
            break;
        case TKB_HDR_IPV6_OUT_TUNNEL:
        case TKB_HDR_IPV6_OUT_TUNNEL_NATT:
            tkb_params_p->result_flags |= TKB_RESFLAG_OUTIPV6;
            break;
        }
    }

    if (hproto == TKB_HDR_IPV4_OUT_TRANSP ||
        hproto == TKB_HDR_IPV6_OUT_TRANSP ||
        hproto == TKB_HDR_IPV4_OUT_TRANSP_NATT ||
        hproto == TKB_HDR_IPV6_OUT_TRANSP_NATT ||
        hproto == TKB_HDR_IPV4_OUT_TUNNEL ||
        hproto == TKB_HDR_IPV6_OUT_TUNNEL ||
        hproto == TKB_HDR_IPV4_OUT_TUNNEL_NATT ||
        hproto == TKB_HDR_IPV6_OUT_TUNNEL_NATT)
    {
        outlen = ohdrlen + 8 +
            token_context_internal_p->iv_byte_count +
            token_context_internal_p->icv_byte_count +
            padded_size(packet_byte_count - hdrlen + 2,
                       token_context_internal_p->pad_block_byte_count);
    }
    else if (hproto == TKB_HDR_IPV4_OUT_DTLS ||
             hproto == TKB_HDR_IPV6_OUT_DTLS)
    {
        if (token_context_internal_p->protocol == TKB_PROTO_SSLTLS_OUT &&
            token_context_internal_p->iv_byte_count > 0)
            outlen = 13 + token_context_internal_p->iv_byte_count +
                padded_size(packet_byte_count - hdrlen + 1 + token_context_internal_p->icv_byte_count,
                           token_context_internal_p->pad_block_byte_count);
        else
            outlen = packet_byte_count - hdrlen + 13 + token_context_internal_p->iv_byte_count +
                token_context_internal_p->icv_byte_count;
        if ((token_context_internal_p->u.generic.esp_flags & TKB_DTLS_FLAG_CAPWAP) != 0)
            outlen += 4;
    }

    *nh_p = nh;
    *ohdrlen_p = ohdrlen;
    *outlen_p = outlen;
    *nextheader_p = nextheader;
    *prev_nhoffset_p = prev_nhoffset;
    *ecn_fixup_instr_p = ecn_fixup_instr;
    return hdrlen;
}

/*-----------------------------------------------------------------------------
 * form_dscp_ecn
 *
 * Form the dscp/ECN byte in the tunnel header. Use the value provided in the
 * input packet, but optionally replace the dscp field or clear the ECN field.
 */
static u8
form_dscp_ecn(
        const token_builder_context_t *token_context_internal_p,
        const u8 dscp_ecn_in)
{
    u8 dscp_ecn_out = dscp_ecn_in;
    if (token_context_internal_p->u.generic.esp_flags & TKB_ESP_FLAG_REPLACE_DSCP)
    {
        dscp_ecn_out = (dscp_ecn_out & 0x3) | \
            ((token_context_internal_p->u.generic.dscp)<<2);
    }
    if (token_context_internal_p->u.generic.esp_flags & TKB_ESP_FLAG_CLEAR_ECN)
    {
        dscp_ecn_out &= 0xfc;
    }
    return dscp_ecn_out;
}

/*-----------------------------------------------------------------------------
 * form_w0_ip4
 *
 * Form the first word of the IPv4 tunnel header.
 */
static u32
form_w0_ip4(
    const token_builder_context_t *token_context_internal_p,
    const u16 pkt_len,
    const u8 dscp_ecn_in)
{
    return 0x45 |
        (form_dscp_ecn(token_context_internal_p, dscp_ecn_in) << 8) |
        (byte_swap16(pkt_len) << 16);
}

/*-----------------------------------------------------------------------------
 * form_w1_ip4
 *
 * Form the second word of the IPv4 tunnel header. Contains df bit.
 */
static u32
form_w1_ip4(
        const token_builder_context_t *token_context_internal_p,
        const u8 flag_byte,
        const u32 pkt_id)
{
    bool df = (flag_byte & 0x40) != 0;
    if (token_context_internal_p->u.generic.esp_flags & TKB_ESP_FLAG_SET_DF)
        df=true;
    else if (token_context_internal_p->u.generic.esp_flags & TKB_ESP_FLAG_CLEAR_DF)
        df=false;
    return (df ? 0x00400000 : 0x00000000) |
        ((pkt_id >> 8) & 0xff) | ((pkt_id & 0xff) << 8);
}

/*-----------------------------------------------------------------------------
 * form_w2_ip4
 *
 * Form the third word of the IPv4 tunnel header. Contains header checksum,
 * which is computed over all other fields of the header.
 */
static u32
form_w2_ip4(
    const token_builder_context_t *token_context_internal_p,
    const u32 w0,
    const u32 w1)
{
    u32 w2;
    u32 sum;
    unsigned int i;
    w2 = token_context_internal_p->u.generic.ttl |
        (token_context_internal_p->hproto==TKB_HDR_IPV4_OUT_TUNNEL_NATT ?
         0x1100 : 0x3200);
    sum = (w0 >> 16)+(w0 & 0xffff) +
        (w1 >> 16)+(w1 & 0xffff) + w2;
    for (i=0; i<4; i++)
    {
        sum = sum + token_context_internal_p->tunnel_ip[2*i] +
            256 * token_context_internal_p->tunnel_ip[2*i + 1];
    }
    sum = (sum >> 16) + (sum & 0xffff);
    sum = (sum >> 16) + (sum & 0xffff);
    return (sum ^ 0xffff) << 16 | w2;
}


/*-----------------------------------------------------------------------------
 * form_w0_ip6
 *
 * Form the first word of the IPv6 tunnel header.
 */
static u32
form_w0_ip6(
    const token_builder_context_t *token_context_internal_p,
    const u8 dscp_ecn_in,
    const u32 flow_label)
{
    return byte_swap32(0x60000000 |
            (form_dscp_ecn(token_context_internal_p,dscp_ecn_in) << 20) |
             flow_label);
}

/*-----------------------------------------------------------------------------
 * form_w1_ip6
 *
 * Form the second word of the IPv6 tunnel header.
 */
static u32
form_w1_ip6(
    const token_builder_context_t *token_context_internal_p,
    const u16 pkt_len)
{
    return byte_swap16(pkt_len - 40) |
        (token_context_internal_p->u.generic.ttl << 24) |
        (token_context_internal_p->hproto==TKB_HDR_IPV6_OUT_TUNNEL_NATT ?
         0x110000 : 0x320000);
}


#define eval_prev_nhoffset() prev_nhoffset
#define eval_hdrlen() token_builder_parse_ip_header(packet_p + bypass, \
                                                 packet_byte_count - bypass, \
                                                 token_context_internal_p, \
                                                 tkb_params_p, \
                                                 &ohdrlen, \
                                                 &outlen, \
                                                 &nh, \
                                                 &nextheader, \
                                                 &prev_nhoffset, \
                                                 &ecn_fixup_instr)
#define eval_ohdrlen() ohdrlen
#define eval_outlen() outlen
#define eval_nh() nh
#define eval_ecn_fixup_instr() ecn_fixup_instr
#define eval_outer_ecn()  (tkb_params_p->TOS_TC_DF & 0x3)
#define eval_tunnel_w0_ip4() form_w0_ip4(token_context_internal_p, \
                                         outlen, nh)
#define eval_tunnel_w1_ip4() form_w1_ip4(token_context_internal_p, \
                                         nextheader==4?packet_p[bypass+6]:0, \
                                         tkb_params_p->additional_value)
#define eval_tunnel_w2_ip4() form_w2_ip4(token_context_internal_p, \
                                         tunnel_w0_ip4, tunnel_w1_ip4)
#define eval_tunnel_w0_ip6() form_w0_ip6(token_context_internal_p, nh,\
                                         prev_nhoffset)
#define eval_tunnel_w1_ip6() form_w1_ip6(token_context_internal_p, outlen)

#define eval_ports_natt() token_context_internal_p->natt_ports
#define eval_nextheader() nextheader
#define eval_tunnel_ip_addr() token_context_internal_p->tunnel_ip
#define eval_dst_ip_addr() (token_context_internal_p->tunnel_ip + 4)
#define eval_is_nat() ((token_context_internal_p->u.generic.esp_flags & TKB_ESP_FLAG_NAT) != 0)
#else
#define eval_hdrlen() 0
#define eval_ohdrlen() 0
#define eval_nextheader() ((unsigned)tkb_params_p->pad_byte)
#define eval_ecn_fixup_instr() 0x20000000
#endif


/*-----------------------------------------------------------------------------
 * token_builder_copy_bytes
 *
 * Copy a number of bytes from src to dst. src is a byte pointer, which is
 * not necessarily byte aligned. Always write a whole number of words,
 * filling the last word with null-bytes if necessary.
 */
static void
token_builder_copy_bytes(u32 *dest,
                       const u8 *src,
                       unsigned int byte_count)
{
    unsigned int i,j;
    u32 w;
    if (src == NULL)
    {
        LOG_CRIT("TokenBuilder trying copy data from NULL pointer\n");
        return;
    }
    for (i=0; i<byte_count/sizeof(u32); i++)
    {
        w=0;
        for (j=0; j<sizeof(u32); j++)
        {
            w = (w>>8) | (*src++ << 24);
        }
        *dest++ = w;
    }
    if (byte_count % sizeof(u32) != 0)
    {
        w=0;
        for (j=0; j<byte_count % sizeof(u32); j++)
        {
            w = w | (*src++ << (8*j));
        }
        *dest++ = w;
    }
}


/*-----------------------------------------------------------------------------
 * switch_proto
 *
 * Switch the Token Context back from HMAC precompute to the original
 * protocol.
 *
 * Return true if protocol was switched.
 */
static inline bool
switch_proto(token_builder_context_t *ctx)
{
    if(ctx->protocol == TKB_PROTO_BASIC_HMAC_CTXPREPARE)
    {
        ctx->protocol = ctx->protocol_next;
        ctx->hproto = ctx->hproto_next;
        ctx->iv_handling = ctx->iv_handling_next;
        ctx->digest_word_count = ctx->digest_word_count_next;
        ctx->token_header_word |= ctx->header_word_fields_next << 22;
        return true;
    }
    else
    {
        return false;
    }
}

#endif /* TOKEN_BUILDER_MACROS_H_ */

/* end of file token_builder_macros.h */
