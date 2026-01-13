/* sa_builder_extended_macsec.c
 *
 * MACsec specific functions (for initialization of sa_builder_params_t
 * structures and for building the MACsec specific part of an SA) in the
 * Extended use case.
 */

/*****************************************************************************
* Copyright (c) 2013-2025 by Rambus, Inc. and/or its subsidiaries.
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
#include "c_sa_builder.h"
#ifdef SAB_ENABLE_MACSEC_EXTENDED
#include "sa_builder_extended_internal.h"

/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */
#include "basic_defs.h"
#include "log.h"
#include "sa_builder_internal.h" /* sa_builder_set_macsec_params */
#include "sa_builder_macsec.h"

/*----------------------------------------------------------------------------
 * Definitions and macros
 */
#define SAB_MACSEC_ETHER_TYPE 0x88e5
/* Various bits in the tci byte */
#define SAB_MACSEC_TCI_ES  BIT_6
#define SAB_MACSEC_TCI_SC  BIT_5
#define SAB_MACSEC_TCI_SCB BIT_4
#define SAB_MACSEC_TCI_E   BIT_3
#define SAB_MACSEC_TCI_C   BIT_2

/*----------------------------------------------------------------------------
 * Local variables
 */


/*----------------------------------------------------------------------------
 * sa_builder_set_extended_macsec_params
 *
 * Fill in MACsec-specific extensions into the SA.for Extended.
 *
 * sa_params_p (input)
 *   The SA parameters structure from which the SA is derived.
 * sa_state_p (input, output)
 *   Variables containing information about the SA being generated/
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
sa_builder_status_t
sa_builder_set_extended_macsec_params(sa_builder_params_t *const sa_params_p,
                         sa_builder_state_t * const sa_state_p,
                         u32 * const sa_buffer_p)
{
    sa_builder_params_macsec_t *sa_params_macsec_p =
        (sa_builder_params_macsec_t *)(sa_params_p->protocol_extension_p);
    u32 token_header_word = SAB_HEADER_DEFAULT;
    sa_builder_esp_protocol_t esp_proto;
    sa_builder_header_protocol_t header_proto;
    u8 iv_byte_count;
    u8 icv_byte_count;
    u8 seq_offset;
    u8 tci; /* tci byte in SECtag */
    u32 flags = 0;
    u32 verify_instruction_word, ctx_instruction_word;

    IDENTIFIER_NOT_USED(sa_state_p);

    if (sa_params_macsec_p == NULL)
    {
        LOG_CRIT("SABuilder: MACsec extension pointer is null\n");
        return SAB_INVALID_PARAMETER;
    }

    seq_offset = sa_params_p->offset_seq_num;
    icv_byte_count = 16;
    tci = sa_params_macsec_p->an;
    if ((sa_params_macsec_p->macsec_flags & SAB_MACSEC_ES) != 0)
    {
        tci |= SAB_MACSEC_TCI_ES;
    }
    if ((sa_params_macsec_p->macsec_flags & SAB_MACSEC_SC) != 0)
    {
        iv_byte_count = 8;
        tci |= SAB_MACSEC_TCI_SC;
    }
    else
    {
        iv_byte_count = 0;
    }
    if ((sa_params_macsec_p->macsec_flags & SAB_MACSEC_SCB) != 0)
    {
        tci |= SAB_MACSEC_TCI_SCB;
    }

    if (sa_params_p->auth_algo == SAB_AUTH_AES_GCM)
        tci |= SAB_MACSEC_TCI_E | SAB_MACSEC_TCI_C;

    if (sa_params_p->direction == SAB_DIRECTION_OUTBOUND)
    {
        header_proto = SAB_HDR_MACSEC_OUT;
        if (sa_params_p->auth_algo == SAB_AUTH_AES_GCM)
            esp_proto = SAB_MACSEC_PROTO_OUT_GCM;
        else
            esp_proto = SAB_MACSEC_PROTO_OUT_GMAC;
        verify_instruction_word = SAB_VERIFY_NONE;
        ctx_instruction_word = SAB_CTX_OUT_SEQNUM +
            ((unsigned int)(1<<24)) + seq_offset;
    }
    else
    {
        header_proto = SAB_HDR_MACSEC_IN;
        if (sa_params_p->auth_algo == SAB_AUTH_AES_GCM)
            esp_proto = SAB_MACSEC_PROTO_IN_GCM;
        else
            esp_proto = SAB_MACSEC_PROTO_IN_GMAC;
        verify_instruction_word = SAB_VERIFY_NONE + SAB_VERIFY_BIT_H +
            SAB_VERIFY_BIT_SEQ + icv_byte_count;
        ctx_instruction_word = SAB_CTX_SEQNUM +
            ((unsigned int)(1<<24)) + seq_offset;
    }

    /* Write all parameters to their respective offsets */
    if (sa_buffer_p != NULL)
    {
        /* Do not support large transform records as Macsec will never
           use HMAC-SHA512 */
        sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_FLAGS_WORD_OFFSET] = flags;
        sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_HDRPROC_CTX_WORD_OFFSET] =
            sa_params_macsec_p->context_ref;
        sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_BYTE_PARAM_WORD_OFFSET] =
            SAB_PACKBYTES(iv_byte_count,icv_byte_count,header_proto,esp_proto);
        sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_TK_HDR_WORD_OFFSET] = token_header_word;
        sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_PAD_ALIGN_WORD_OFFSET] =
            SAB_PACKBYTES(SAB_MACSEC_ETHER_TYPE>>8,
                          SAB_MACSEC_ETHER_TYPE &0xff, tci, 0);
        sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_CCM_SALT_WORD_OFFSET] =
            sa_params_macsec_p->conf_offset;
        sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_TK_VFY_INST_WORD_OFFSET] =
                verify_instruction_word;
        sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_TK_CTX_INST_WORD_OFFSET] =
            ctx_instruction_word;
        sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_TIME_STAMP_LO_WORD_OFFSET] = 0;
        sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_TIME_STAMP_HI_WORD_OFFSET] = 0;
        sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_STAT_OCT_LO_WORD_OFFSET] = 0;
        sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_STAT_OCT_HI_WORD_OFFSET] = 0;
        sa_buffer_p[FIRMWARE_EIP207_CS_FLOW_TR_STAT_PKT_WORD_OFFSET] = 0;

        if (sa_params_macsec_p->sci_p == NULL)
        {
            return SAB_INVALID_PARAMETER;
        }
        sa_builder_lib_copy_key_mat(sa_buffer_p,
                                FIRMWARE_EIP207_CS_FLOW_TR_TUNNEL_SRC_WORD_OFFSET,
                                sa_params_macsec_p->sci_p, 8);
    }
    return SAB_STATUS_OK;
}

#endif /* SAB_ENABLE_MACSEC_EXTENDED */


/* end of file sa_builder_extended_dtls.c */
