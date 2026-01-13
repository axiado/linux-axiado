/* sa_builder_params_ipsec.h
 *
 * IPsec specific extension to the sa_builder_params_t type.
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


#ifndef SA_BUILDER_PARAMS_IPSEC_H_
#define SA_BUILDER_PARAMS_IPSEC_H_


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

#include "sa_builder_params.h"

// Driver Framework Basic Definitions API
#include "basic_defs.h"

#define SA_SEQ_MASK_WORD_COUNT 12 /* Maximum number of words in sequence mask */

/*----------------------------------------------------------------------------
 * Definitions and macros
 */

/* Flag bits for the ipsec_flags field. Combine any values using a
   bitwise or.
   Of SAB_IPSEC_ESP and SAB_AH, exactly one must be set.
   Of SAB_IPSEC_TUNNEL and SAB_IPSEC_TRANSPORT, exactly one must be set.
   Of SAB_IPSEC_IPV4 and SAB_IPSEC_IPV6, exactly one must be set.
*/

#define SAB_IPSEC_ESP             BIT_0
#define SAB_IPSEC_AH              BIT_1

#define SAB_IPSEC_TUNNEL          BIT_2
#define SAB_IPSEC_TRANSPORT       BIT_3

#define SAB_IPSEC_IPV4            BIT_4
#define SAB_IPSEC_IPV6            BIT_5

#define SAB_IPSEC_LONG_SEQ        BIT_6 /* Use 64-bit extended seq. number */
#define SAB_IPSEC_NO_ANTI_REPLAY  BIT_7 /* Disable anti-replay protection */
#define SAB_IPSEC_MASK_128        BIT_8 /* Use 128-bit anti-replay mask instead of 64-bit (for downward compatibility) */
#define SAB_IPSEC_MASK_32         BIT_9 /* Use 32-bit anti-replay mask instead of 64-bit (for downward compatibility) */

#define SAB_IPSEC_PROCESS_IP_HEADERS BIT_10 /* Perform header processing */
#define SAB_IPSEC_CLEAR_DF        BIT_11 /* Clear df on outer tunnel header */
#define SAB_IPSEC_SET_DF          BIT_12 /* Set df on outer tunnel header */
#define SAB_IPSEC_NATT            BIT_13 /* Encapsulate ESP in UDP for nat traversal */
#define SAB_IPSEC_REPLACE_DSCP    BIT_14 /* Copy dscp from transform record */
#define SAB_IPSEC_MASK_384        BIT_15 /* Use 384-bit anti-replay mask instead of 64-bit (for downward compatibility) */
#define SAB_IPSEC_APPEND_SEQNUM   BIT_16 /* Cause the hardware to append sequence number to output */
#define SAB_IPSEC_CLEAR_ECN       BIT_17 /* Clear ECN bits in tunnel header for compatibility mode */
#define SAB_IPSEC_MASK_256        BIT_18 /* Use 256-bit anti-replay mask instead of 64-bit (for downward compatibility) */
#define SAB_IPSEC_FIXED_SEQ_OFFSET BIT_19 /* Use fixed sequence number offset
                                             for 64 or 128 bit masks. */
#define SAB_IPSEC_EXT_PROCESSING  BIT_20 /* Extended processing for IPsec
                                            in stand-alone token builder */
#define SAB_IPSEC_TRANSPORT_NAT     BIT_21 /* Use additional NAT with transport NATT */
#define SAB_IPSEC_CHECKSUM_FIX      BIT_22 /* Fix checksum for inbound transport NAT-T */
#define SAB_IPSEC_DEC_TTL           BIT_23 /* Decrement ttl/hop limit field */

#define SAB_IPSEC_XFRM_API          BIT_24 /* Use this transform with Linux kernel XFRM API */

/* Extension record for SAParams_t. Protocol_Extension_p must point
   to this structure when the IPsec protocol is used.

   SABuilder_Iinit_ESP() will fill all fields in this structure  with
   sensible defaults.
 */
typedef struct
{
    u32 spi;
    u32 ipsec_flags;    /* See SAB_IPSEC_* flag bits above */

    u32 seq_num;       /* Initialize with zero */
    u32 seq_num_hi;     /* Only valid if SAB_IPSEC_LONG_SEQ is set */
    u32 seq_mask[SA_SEQ_MASK_WORD_COUNT];
                           /* mask window Only used with inbound operations.
                              By default, set first word to 1, all others to 0.

                              The mask can be programmed to resume an existing
                              SA operation, This field is unused for
                              mask sizes that cannot be accommodated in
                              this array. For these, only the default mask is
                              possible,
                           */
    u32 pad_alignment; /* Align padding to specified multiple of bytes.
                              This must be a power of two between 4 and 256.
                              If zero, default pad alignment is used.*/
    u32 icv_byte_count; /* Length of ICV in bytes. If left zero, a default
                              value is used, compatible with the authentication
                              algorithm, */
    u8 *src_ip_addr_p;  /* source IP address for tunnel header.
                                 4 bytes for IPv4, 16 bytes for IPv6
                              Also used as the translate-to address for NAT.*/
    u8 *dest_ip_addr_p; /* Destination IP address for tunnel header.
                              4 bytes for IPv4, 16 bytes for IPv6
                              Also used as the translate-to address for NAT.*/
    u8 *orig_src_ip_addr_p; /* Original NAT source address (translate-from)
                                 used in checksum delta calculations */
    u8 *orig_dest_ip_addr_p; /* Original NAT destination address (translate-from)
                                 used in checksum delta calculations */
    u16 natt_src_port;  /* UDP source port when using NAT-T */
    u16 natt_dest_port; /* UDP destination port when using NAT-T */
    u32 context_ref; /* Reference to application context */
    u8  ttl;        /* Time-to-live/Hop limit in outer header */
    u8  dscp;       /* dscp/traffic class field in outer header
                            if copied from SA */
    u16 sequence_mask_bit_count; /* number of bits in sequence number mask.
                                      Default is 64 */
} sa_builder_params_ipsec_t;


#endif /* SA_BUILDER_PARAMS_IPSEC_H_ */


/* end of file sa_builder_params_ipsec.h */
