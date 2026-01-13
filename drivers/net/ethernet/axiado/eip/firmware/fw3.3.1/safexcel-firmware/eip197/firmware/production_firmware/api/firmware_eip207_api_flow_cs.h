/* firmware_eip207_api_flow_cs.h
 *
 * EIP-207 Firmware Classification API:
 * Flow Control functionality,
 *
 * This API is defined by the EIP-207 Classification Firmware
 *
 */

/* -------------------------------------------------------------------------- */
/*                                                                            */
/*   Module        : firmware_eip197                                          */
/*   Version       : 3.3.1                                                    */
/*   configuration : FIRMWARE-GENERIC                                         */
/*                                                                            */
/* Date          : 2021-Jun-11                                                */
/*                                                                            */
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


#ifndef FIRMWARE_EIP207_API_FLOW_CS_H_
#define FIRMWARE_EIP207_API_FLOW_CS_H_


/*----------------------------------------------------------------------------
 * This module implements (provides) the following interface(s):
 */



/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

#include "basic_defs.h"     // u32

/*----------------------------------------------------------------------------
 * Definitions and macros
 */

// size of the Flow Record in 32-bit words
#define FIRMWARE_EIP207_CS_FLOW_FRC_RECORD_WORD_COUNT                       16

// size of the Transform Record in 32-bit words
#define FIRMWARE_EIP207_CS_FLOW_TRC_RECORD_WORD_COUNT                       64
#define FIRMWARE_EIP207_CS_FLOW_TRC_RECORD_WORD_COUNT_LARGE                 80

// size of the arc4 Record in 32-bit words
#define FIRMWARE_EIP207_CS_FLOW_ARC4RC_RECORD_WORD_COUNT                    64
#define FIRMWARE_EIP207_CS_FLOW_ARC4RC_RECORD_WORD_COUNT_LARGE              64

// Word offset for arc4 state record physical address
#define FIRMWARE_EIP207_CS_FLOW_FR_ARC4_ADDR_WORD_OFFSET                    6

// Flow ID field length in 32-bit words
#define FIRMWARE_EIP207_CS_FLOW_HASH_ID_FIELD_WORD_COUNT                    4

/*
 * Flow Record field offsets
 */

 // Word offset for transform record physical address offset relative to cache base address
#define FIRMWARE_EIP207_CS_FLOW_FR_XFORM_OFFS_WORD_OFFSET                   4

// Word offset for transform record physical address
#define FIRMWARE_EIP207_CS_FLOW_FR_XFORM_ADDR_WORD_OFFSET                   5

// Word offset for transform record physical address
#define FIRMWARE_EIP207_CS_FLOW_FR_XFORM_ADDR_HI_WORD_OFFSET                6

// Word offset for software flow record host (virtual) address
#define FIRMWARE_EIP207_CS_FLOW_FR_SW_ADDR_WORD_OFFSET                      8

// Word offset for flags field
#define FIRMWARE_EIP207_CS_FLOW_FR_FLAGS_WORD_OFFSET                        9

// Word offset for MTU/Interface ID field
#define FIRMWARE_EIP207_CS_FLOW_FR_MTU_IFC_WORD_OFFSET                      3

// Word offset for Next Hop MAC field (size of field is 3 words).
#define FIRMWARE_EIP207_CS_FLOW_FR_NEXTHOP_MAC_WORD_OFFSET                  7

// Word offset for NAT ports
#define FIRMWARE_EIP207_CS_FLOW_FR_NAT_PORTS_WORD_OFFSET                    8

// Word offset for NAT source address
#define FIRMWARE_EIP207_CS_FLOW_FR_NAT_SRC_WORD_OFFSET                      9

// Word offset for NAT destination address
#define FIRMWARE_EIP207_CS_FLOW_FR_NAT_DST_WORD_OFFSET                      3

// Word offset for time stamp 64-bit value, low 32-bits
#define FIRMWARE_EIP207_CS_FLOW_FR_TIME_STAMP_LO_WORD_OFFSET                12

// Word offset for time stamp 64-bit value, high 32-bits
#define FIRMWARE_EIP207_CS_FLOW_FR_TIME_STAMP_HI_WORD_OFFSET                \
                          (FIRMWARE_EIP207_CS_FLOW_FR_TIME_STAMP_LO_WORD_OFFSET + 1)

// Word offset for octets statistics 64-bit value, low 32-bits
#define FIRMWARE_EIP207_CS_FLOW_FR_STAT_OCT_LO_WORD_OFFSET                  14

// Word offset for octets statistics 64-bit value, high 32-bits
#define FIRMWARE_EIP207_CS_FLOW_FR_STAT_OCT_HI_WORD_OFFSET                  \
                      (FIRMWARE_EIP207_CS_FLOW_FR_STAT_OCT_LO_WORD_OFFSET + 1)

// Word offset for packet statistics 32-bit value
#define FIRMWARE_EIP207_CS_FLOW_FR_STAT_PKT_WORD_OFFSET                     10

#define FIRMWARE_EIP207_CS_FLOW_FR_LAST_WORD_OFFSET                         \
                       (FIRMWARE_EIP207_CS_FLOW_FR_STAT_OCT_HI_WORD_OFFSET)

#if (FIRMWARE_EIP207_CS_FLOW_FR_LAST_WORD_OFFSET + 1) != \
                        FIRMWARE_EIP207_CS_FLOW_FRC_RECORD_WORD_COUNT
#error "error: Firmware EIP-207 flow record offsets do not match its size"
#endif

/*
 * Transform Record field offsets
 */
// Maximum offset after storing all EIP96 context information, beyond which the
// large record size and associated offsets must be used.
#define FIRMWARE_EIP207_CS_FLOW_TR_LARGE_THRESHOLD_OFFSET                  56

// offset of extension data from start of transform record.
#define FIRMWARE_EIP207_CS_FLOW_TR_EXTENSION_WORD_OFFSET                   56

// Word offset for CCM salt value
#define FIRMWARE_EIP207_CS_FLOW_TR_CCM_SALT_WORD_OFFSET                    56

// Word offset for pad aligment.
#define FIRMWARE_EIP207_CS_FLOW_TR_PAD_ALIGN_WORD_OFFSET                   50

// Word offset for transform flags.
#define FIRMWARE_EIP207_CS_FLOW_TR_FLAGS_WORD_OFFSET                       49

// Word offset for Token Verify Instruction field, 32-bit word
#define FIRMWARE_EIP207_CS_FLOW_TR_TK_VFY_INST_WORD_OFFSET                 54

// Word offset for Token Context Instruction field, 32-bit word
#define FIRMWARE_EIP207_CS_FLOW_TR_TK_CTX_INST_WORD_OFFSET                 55

// Word offset for NAT-T ports
#define FIMRWARE_EIP207_CS_FLOW_TR_NATT_PORTS_WORD_OFFSET                  53

// Word offset for time stamp 64-bit value, low 32-bits
#define FIRMWARE_EIP207_CS_FLOW_TR_TIME_STAMP_LO_WORD_OFFSET               58

// Word offset for time stamp 64-bit value, high 32-bits
#define FIRMWARE_EIP207_CS_FLOW_TR_TIME_STAMP_HI_WORD_OFFSET                \
                      (FIRMWARE_EIP207_CS_FLOW_TR_TIME_STAMP_LO_WORD_OFFSET + 1)

// Word offset for octets statistics 64-bit value, low 32-bits
#define FIRMWARE_EIP207_CS_FLOW_TR_STAT_OCT_LO_WORD_OFFSET                 62

// Word offset for octets statistics 64-bit value, high 32-bits
#define FIRMWARE_EIP207_CS_FLOW_TR_STAT_OCT_HI_WORD_OFFSET                  \
                      (FIRMWARE_EIP207_CS_FLOW_TR_STAT_OCT_LO_WORD_OFFSET + 1)

// Word offset for packets statistics 32-bit value
#define FIRMWARE_EIP207_CS_FLOW_TR_STAT_PKT_WORD_OFFSET                    60

// Word offset for token header word.
#define FIRMWARE_EIP207_CS_FLOW_TR_TK_HDR_WORD_OFFSET                      48

// Word offset for various byte-sized parameters.
#define FIRMWARE_EIP207_CS_FLOW_TR_BYTE_PARAM_WORD_OFFSET                  52

// Word offset for header proc context pointer.
#define FIRMWARE_EIP207_CS_FLOW_TR_HDRPROC_CTX_WORD_OFFSET                 51

// Word offset for Tunnel IP source address
#define FIRMWARE_EIP207_CS_FLOW_TR_TUNNEL_SRC_WORD_OFFSET                  40

// Word offset for Tunnel IP destination address
#define FIRMWARE_EIP207_CS_FLOW_TR_TUNNEL_DST_WORD_OFFSET                  44

// Word offset for Tunnel IPv4 checksum
#define FIRMWARE_EIP207_CS_FLOW_TR_CHECKSUM_WORD_OFFSET                    41

// Word offset for Path MTU field
#define FIRMWARE_EIP207_CS_FLOW_TR_PATH_MTU_WORD_OFFSET                    57

#define FIRMWARE_EIP207_CS_FLOW_TR_LAST_WORD_OFFSET                        FIRMWARE_EIP207_CS_FLOW_TR_HDRPROC_CTX_WORD_OFFSET

#if (FIRMWARE_EIP207_CS_FLOW_TR_LAST_WORD_OFFSET + 1) > \
                        FIRMWARE_EIP207_CS_FLOW_TRC_RECORD_WORD_COUNT
#error "error: Firmware EIP-207 transform record offsets do not match its size"
#endif

/*
 * Transform Record field offsets for large records
 */

// offset of extension data from start of transform record.
#define FIRMWARE_EIP207_CS_FLOW_TR_EXTENSION_WORD_OFFSET_LARGE          72

// Word offset for CCM salt value
#define FIRMWARE_EIP207_CS_FLOW_TR_CCM_SALT_WORD_OFFSET_LARGE           72

// Word offset for pad aligment.
#define FIRMWARE_EIP207_CS_FLOW_TR_PAD_ALIGN_WORD_OFFSET_LARGE          66

// Word offset for transform flags.
#define FIRMWARE_EIP207_CS_FLOW_TR_FLAGS_WORD_OFFSET_LARGE              65

// Word offset for Token Verify Instruction field, 32-bit word
#define FIRMWARE_EIP207_CS_FLOW_TR_TK_VFY_INST_WORD_OFFSET_LARGE        70

// Word offset for Token Context Instruction field, 32-bit word
#define FIRMWARE_EIP207_CS_FLOW_TR_TK_CTX_INST_WORD_OFFSET_LARGE       71

// Word offset for NAT-T ports
#define FIMRWARE_EIP207_CS_FLOW_TR_NATT_PORTS_WORD_OFFSET_LARGE        69

// Word offset for time stamp 64-bit value, low 32-bits
#define FIRMWARE_EIP207_CS_FLOW_TR_TIME_STAMP_LO_WORD_OFFSET_LARGE     74

// Word offset for time stamp 64-bit value, high 32-bits
#define FIRMWARE_EIP207_CS_FLOW_TR_TIME_STAMP_HI_WORD_OFFSET_LARGE     \
         (FIRMWARE_EIP207_CS_FLOW_TR_TIME_STAMP_LO_WORD_OFFSET_LARGE + 1)

// Word offset for octets statistics 64-bit value, low 32-bits
#define FIRMWARE_EIP207_CS_FLOW_TR_STAT_OCT_LO_WORD_OFFSET_LARGE       78

// Word offset for octets statistics 64-bit value, high 32-bits
#define FIRMWARE_EIP207_CS_FLOW_TR_STAT_OCT_HI_WORD_OFFSET_LARGE       \
         (FIRMWARE_EIP207_CS_FLOW_TR_STAT_OCT_LO_WORD_OFFSET_LARGE + 1)

// Word offset for packets statistics 32-bit value
#define FIRMWARE_EIP207_CS_FLOW_TR_STAT_PKT_WORD_OFFSET_LARGE          76

// Word offset for token header word.
#define FIRMWARE_EIP207_CS_FLOW_TR_TK_HDR_WORD_OFFSET_LARGE            64

// Word offset for various byte-sized parameters.
#define FIRMWARE_EIP207_CS_FLOW_TR_BYTE_PARAM_WORD_OFFSET_LARGE        68

// Word offset for header proc context pointer.
#define FIRMWARE_EIP207_CS_FLOW_TR_HDRPROC_CTX_WORD_OFFSET_LARGE       67

// Word offset for Tunnel IP source address
#define FIRMWARE_EIP207_CS_FLOW_TR_TUNNEL_SRC_WORD_OFFSET_LARGE        56

// Word offset for Tunnel IP destination address
#define FIRMWARE_EIP207_CS_FLOW_TR_TUNNEL_DST_WORD_OFFSET_LARGE        60

// Word offset for Tunnel IPv4 checksum
#define FIRMWARE_EIP207_CS_FLOW_TR_CHECKSUM_WORD_OFFSET_LARGE          \
                      (FIRMWARE_EIP207_CS_FLOW_TR_TUNNEL_SRC_WORD_OFFSET_LARGE + 1)

// Word offset for Path MTU field
#define FIRMWARE_EIP207_CS_FLOW_TR_PATH_MTU_WORD_OFFSET_LARGE          73

#define FIRMWARE_EIP207_CS_FLOW_TR_LAST_WORD_OFFSET_LARGE                   \
                       (FIRMWARE_EIP207_CS_FLOW_TR_HDRPROC_CTX_WORD_OFFSET_LARGE)

#if (FIRMWARE_EIP207_CS_FLOW_TR_LAST_WORD_OFFSET_LARGE + 1) > \
                        FIRMWARE_EIP207_CS_FLOW_TRC_RECORD_WORD_COUNT_LARGE
#error "error: Firmware EIP-207 large transform record offsets do not match its size"
#endif


/*
 * Flow hash ID calculation
 */

// The maximum size of the 32-bit word array that is used as data input for
// the flow hash ID calculation
#define FIRMWARE_EIP207_CS_FLOW_HASH_ID_INPUT_WORD_COUNT               13

// flags
#define FIRMWARE_EIP207_CS_FLOW_SELECT_IPV4                            BIT_0
#define FIRMWARE_EIP207_CS_FLOW_SELECT_IPV6                            BIT_1
#define FIMRWARE_EIP207_CD_FLOW_SELECT_CUSTOM                          BIT_2
#define FIMRWARE_EIP207_CD_FLOW_ESP_WITH_SRC                           BIT_3

#define FIRMWARE_EIP207_CS_FLOW_DTLS_SUPPORTED                         1
// This data structure represents the packet parameters (such as IP addresses
// and ports) that select a particular flow.
typedef struct
{
    // flags, see FIRMWARE_EIP207_CS_FLOW_SELECT_*
    u32 flags;

    // IP protocol number
    u8 ip_proto;

    // IP source address
    u8 * src_ip_p;

    // IP destination address
    u8 * dst_ip_p;

    // source port for UDP
    u16  src_port;

    // Destination port for UDP
    u16  dst_port;

    // Custom selection ID.
    u16  custom_id;

    // spi in IPsec
    u32  spi;

    // epoch for inbound DTLS
    u16 epoch;

} firmware_eip207_cs_flow_selector_params_t;


/*----------------------------------------------------------------------------
 * Local variables
 */


/*----------------------------------------------------------------------------
 * firmware_eip207_cs_flow_selectors_reorder
 *
 * This function re-orders the selectors for the flow hash ID calculation.
 *
 * selectors_p (input)
 *     Pointer to the data structure that contains the selectors from the
 *     packet header that can be used for the flow hash ID calculation.
 *
 * out_data_p (output)
 *     Pointer to the memory where the data arrays of 32-bit words
 *     will be stored. The buffer for the array must be of size
 *     FW207_CS_FLOW_HASH_ID_INPUT_WORD_COUNT.
 *
 * out_data_word_count_p (output)
 *     Pointer to the memory where the data arrays size in 32-bit words
 *     will be stored.
 *
 * Return value
 *     None
 */
static inline void
firmware_eip207_cs_flow_selectors_reorder(
        const firmware_eip207_cs_flow_selector_params_t * const selectors_p,
        u32 * out_data_p,
        unsigned int * const out_data_word_count_p)
{
    unsigned int i = 0;

    // Word 0
    out_data_p[i++] = 0;

    // flags and IP protocol number
    if ( selectors_p->flags & FIRMWARE_EIP207_CS_FLOW_SELECT_IPV6 )
        out_data_p[i++] = (u32)(selectors_p->ip_proto << 8) |
                         (u32)(BIT_25);
    else
        out_data_p[i++] = (u32)(selectors_p->ip_proto << 8);

    // spi
    out_data_p[i++] = selectors_p->spi;

    // epoch
    out_data_p[i++] = selectors_p->epoch;

    if (selectors_p->flags & FIMRWARE_EIP207_CD_FLOW_SELECT_CUSTOM)
    {
        // Custom ID (used instead of port numbers)
        out_data_p[i++] = (u32)selectors_p->custom_id;
    }
    else if (selectors_p->spi == 0)
    {
        // L4 (TCP or UDP) destination and source port numbers
        out_data_p[i++] = (u32)selectors_p->src_port |
        (u32)(selectors_p->dst_port << 16);
    }
    else
    {
        out_data_p[i++] = 0;
    }

    // Destination IP address
    out_data_p[i++] = (u32)selectors_p->dst_ip_p[0]         |
                     (u32)(selectors_p->dst_ip_p[1] << 8)  |
                     (u32)(selectors_p->dst_ip_p[2] << 16) |
                     (u32)(selectors_p->dst_ip_p[3] << 24);

    if ( selectors_p->flags & FIRMWARE_EIP207_CS_FLOW_SELECT_IPV6 )
    {
        out_data_p[i++] = (u32)selectors_p->dst_ip_p[4]         |
                         (u32)(selectors_p->dst_ip_p[5] << 8)  |
                         (u32)(selectors_p->dst_ip_p[6] << 16) |
                         (u32)(selectors_p->dst_ip_p[7] << 24);
        out_data_p[i++] = (u32)selectors_p->dst_ip_p[8]         |
                         (u32)(selectors_p->dst_ip_p[9] << 8)  |
                         (u32)(selectors_p->dst_ip_p[10] << 16)|
                         (u32)(selectors_p->dst_ip_p[11] << 24);
        out_data_p[i++] = (u32)selectors_p->dst_ip_p[12]        |
                         (u32)(selectors_p->dst_ip_p[13] << 8) |
                         (u32)(selectors_p->dst_ip_p[14] << 16)|
                         (u32)(selectors_p->dst_ip_p[15] << 24);
    }
    else
    {
        out_data_p[i++] = 0;
        out_data_p[i++] = 0;
        out_data_p[i++] = 0;
    }

    // source IP address
    if (selectors_p->spi != 0 && (selectors_p->flags & FIMRWARE_EIP207_CD_FLOW_ESP_WITH_SRC) == 0)
    {
        out_data_p[i++] = 0;
        out_data_p[i++] = 0;
        out_data_p[i++] = 0;
        out_data_p[i++] = 0;
    }
    else
    {
        out_data_p[i++] = (u32)selectors_p->src_ip_p[0]         |
                         (u32)(selectors_p->src_ip_p[1] << 8)  |
                         (u32)(selectors_p->src_ip_p[2] << 16) |
                         (u32)(selectors_p->src_ip_p[3] << 24);
        if ( selectors_p->flags & FIRMWARE_EIP207_CS_FLOW_SELECT_IPV6 )
        {
            out_data_p[i++] = (u32)selectors_p->src_ip_p[4]         |
                             (u32)(selectors_p->src_ip_p[5] << 8)  |
                             (u32)(selectors_p->src_ip_p[6] << 16) |
                             (u32)(selectors_p->src_ip_p[7] << 24);
            out_data_p[i++] = (u32)selectors_p->src_ip_p[8]         |
                             (u32)(selectors_p->src_ip_p[9] << 8)  |
                             (u32)(selectors_p->src_ip_p[10] << 16)|
                             (u32)(selectors_p->src_ip_p[11] << 24);
            out_data_p[i++] = (u32)selectors_p->src_ip_p[12]        |
                             (u32)(selectors_p->src_ip_p[13] << 8) |
                             (u32)(selectors_p->src_ip_p[14] << 16)|
                             (u32)(selectors_p->src_ip_p[15] << 24);
        }
        else
        {
            out_data_p[i++] = 0;
            out_data_p[i++] = 0;
            out_data_p[i++] = 0;
        }
    }

    *out_data_word_count_p = i;
}


/*----------------------------------------------------------------------------
 * firmware_eip207_cs_flow_seq_num_offset_read
 *
 * This function reads the word offset of the 32-bit Sequence number field
 * from the provided Token Context Instruction 32-bit word.
 *
 * value (input)
 *     Token Context Instruction 32-bit word.
 *
 * word_offset_p (output)
 *     Pointer to the memory where the word offset of the 32-bit Sequence number
 *     field will be stored.
 *
 * Return value
 *     None
 */
static inline void
firmware_eip207_cs_flow_seq_num_offset_read(
        const u32 value,
        unsigned int * const word_offset_p)
{
    *word_offset_p = (unsigned int)(value & MASK_8_BITS);
}


#endif /* FIRMWARE_EIP207_API_FLOW_CS_H_ */


/* end of file firmware_eip207_api_flow_cs.h */
