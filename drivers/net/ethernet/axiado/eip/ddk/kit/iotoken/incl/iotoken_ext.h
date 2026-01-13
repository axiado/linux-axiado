/* iotoken_ext.h
 *
 * IOToken API for Extended Server with Input Classification Engine (ice)
 * and optional Output Classification Engine (oce)
 *
 * All the functions in this API are re-entrant for different tokens.
 * All the functions in this API can be used concurrently for different tokens.
 *
 */

/*****************************************************************************
* Copyright (c) 2016-2024 by Rambus, Inc. and/or its subsidiaries.
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

#ifndef IOTOKEN_EXT_H_
#define IOTOKEN_EXT_H_

/*----------------------------------------------------------------------------
 * This module implements (provides) the following interface(s):
 */


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Standard IOToken API
#include "iotoken.h"


/*----------------------------------------------------------------------------
 * Definitions and macros
 */

// Length of the EIP-197 Server extended command token (in 32-bit words)
// Note: Based on 64-bit Context (SA) address only!
// Take the maximum possible value, including 4 optional bypass data words.
#define IOTOKEN_IN_WORD_COUNT        (6 + 4)
// The inline token may be larger.
#define IOTOKEN_IN_WORD_COUNT_IL     (8 + 4)

// Length of the EIP-197 Server extended result token (in 32-bit words)
// Take the maximum possible value, including 4 optional bypass data words.
#define IOTOKEN_OUT_WORD_COUNT       (8 + 4)
// The inline token may be larger.
#define IOTOKEN_OUT_WORD_COUNT_IL    (8 + 4)

#define IOTOKEN_FLOW_TYPE            3 // Extended packet flow

// DTLS content type
typedef enum
{
    IOTOKEN_DTLS_CTYPE_CHANGE_CIPHER,
    IOTOKEN_DTLS_CTYPE_ALERT,
    IOTOKEN_DTLS_CTYPE_HANDSHAKE,
    IOTOKEN_DTLS_CTYPE_APP_DATA
} io_token_dtls_content_type_t;

// Input token header word extended options
typedef struct
{
    // true - DTLS CAPWAP header is present in the input DTLS packet.
    //        Used only for inbound (Direction = 1) and ignored for outbound
    //        processing.
    bool f_capwap;

    // Direction for the packet flow. true - inbound, false - outbound
    bool f_inbound;

    // DTLS content type
    io_token_dtls_content_type_t content_type;

} io_token_options_ext_t;

// Input Token descriptor extension
typedef struct
{
    // arc4 state record pre-fetch control
    // true - arc4 state pre-fetch is used, otherwise (false) not
    bool f_arc4_prefetch;

    // Input token header extended options
    io_token_options_ext_t options;

    // Packet flow selection (see the EIP-197 firmware API specification)
    unsigned int hw_services;

    // offset to the end of the bypass data (start of packet data) or
    // length (in bytes) of bypass data
    unsigned int offset_byte_count;

    // Next header to be inserted with pad data during the ESP Outbound
    // encapsulation, otherwise zero
    unsigned int next_header;

    // number of bytes for additional authentication data in combined
    // encyrpt-authenticate operations.
    unsigned int aad_byte_count;

    // User defined field for flow lookup.
    u16 user_def;

    // Will we redirect the packet based on command descriptor?
    bool f_redirect_enable;

    // Interface to which the packet must be redirected.
    u8 redirect_ifc;

    // Strip padding after IP datagrams
    bool f_strip_padding;

    // Allow padding after IP datagrams
    bool f_allow_padding;

    // Copy Flow Label
    bool f_fl;

    // Verify (or generate) IPv4 checksum
    bool f_ipv4_chksum;

    // Verify (or generate) L4 checksum
    bool f_l4_chksum;

    // Generate L4 checksum, use together with fL4Checksum for outbound IPsec
    bool f_l4_chksum_outbound;

    // Parse Ethernet header
    bool f_parse_ether;

    // Keep outer header fter inbound tunnel operation
    bool f_keep_outer;

    // Always encapsulate last destination header (IPv6 ESP outbound transport).
    bool f_enc_last_dest;

    // Set to specify inline interface.
    bool f_inline;

    // Reduced mask size for inbound SA with 1024-bit mask.
    unsigned int sequence_mask_bit_count;

    // Bypass data which will be passed unmodified from Input Token
    // to Output Token
    // Buffer size must IOTOKEN_BYPASS_WORD_COUNT (32-bit words) at least
    // If not used set to NULL (IOTOKEN_BYPASS_WORD_COUNT must be set to 0)
    unsigned int * bypass_data_p;

} io_token_input_dscr_ext_t;

// Output Token descriptor extension
typedef struct
{
    // For tunnel ESP processing
    // IPv4 type of Service or IPv6 Traffic Class field from inner IP header
    unsigned int TOS_TC;

    // For tunnel ESP processing
    // IPv4 DontFragment field from inner IP header
    bool f_df;

    // Classification errors (cle), see firmware specification for details
    unsigned int cfy_errors;

    // Extended errors, see firmware specification for details.
    unsigned int ext_errors;

    // Copy of the offset_byte_count from the Input Token
    unsigned int offset_byte_count;

    // Valid only when f_length_appended is true and both f_checksum_appended and
    // f_next_header_appended are false (see Output Token Descriptor)
    // Delta between the result packet length and the length value that
    // must be inserted in the IP header
    unsigned int ip_delta_byte_count;

    // Context record (SA) physical address
    io_token_phys_addr_t sa_phys_addr;

    // Application-specific reference for Network Packet Header processing
    // Comes to the Engine via the Context (SA) Record
    unsigned int nph_context;

    // offset within the IPv6 header needed to complete the header processing
    unsigned int next_header_offset;

    // Bypass data which will be passed unmodified from Input Token
    // to Output Token

    // Input packet was IPv6
    bool f_in_ipv6;

    // Ethernet parsing performed
    bool f_from_ether;

    // Output packet is IPv6
    bool f_out_ipv6;

    // Inbound tunnel operation
    bool f_inb_tunnel;


    // Buffer size must IOTOKEN_BYPASS_WORD_COUNT (32-bit words) at least
    // If not used set to NULL (IOTOKEN_BYPASS_WORD_COUNT must be set to 0)
    unsigned int * bypass_data_p;

} io_token_output_dscr_ext_t;

// Lookaside Crypto packet Flow
#define IOTOKEN_CMD_PKT_LAC       0x04

// Lookaside IPsec packet flow
#define IOTOKEN_CMD_PKT_LIP       0x02

// Inline IPsec packet flow
#define IOTOKEN_CMD_PKT_IIP       0x02

// Lookaside IPsec packet flow, custom classification
#define IOTOKEN_CMD_PKT_LIP_CUST  0x03

// Inline IPsec packet flow, custom classification
#define IOTOKEN_CMD_PKT_IIP_CUST  0x03

// Lookaside IPsec with Token Builder offload packet flow
#define IOTOKEN_CMD_PKT_LAIP_TB   0x18

// Lookaside Basic with Token Builder offload packet flow
#define IOTOKEN_CMD_PKT_LAB_TB    0x20

// Inline Basic with Token Builder offload packet flow
#define IOTOKEN_CMD_PKT_IB_TB    0x20

// Lookaside DTLS (including CAPWAP)
#define IOTOKEN_CMD_PKT_LDT       0x28

// Inline DTLS (including CAPWAP)
#define IOTOKEN_CMD_PKT_IDT       0x28

// Inline MACsec
#define IOTOKEN_CMD_PKT_IMA       0x38

// Invalidate Transform Record command
#define IOTOKEN_CMD_INV_TR        0x06

// Invalidate Flow Record command
#define IOTOKEN_CMD_INV_FR        0x05

// Bypass operation.
#define IOTOKEN_CMD_BYPASS        0x08

// Bit mask that enables the flow lookup. Otherwise the transform lookup is performed.
#define IOTOKEN_CMD_FLOW_LOOKUP   0x80

#define IOTOKEN_USE_HW_SERVICE
#define IOTOKEN_USE_TRANSFORM_INVALIDATE


/* end of file iotoken_ext.h */


#endif /* IOTOKEN_EXT_H_ */
