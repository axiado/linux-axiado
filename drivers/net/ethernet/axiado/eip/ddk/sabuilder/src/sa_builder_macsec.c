/* sa_builder_macsec.c
 *
 * MACsec specific functions (for initialization of sa_builder_params_t
 * structures and for building the MACsec specifc part of an SA.).
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
#include "sa_builder_macsec.h"
#include "sa_builder_internal.h" /* sa_builder_set_macsec_params */

/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */
#include "c_sa_builder.h"
#include "basic_defs.h"
#include "log.h"

#ifdef SAB_ENABLE_PROTO_MACSEC

/*----------------------------------------------------------------------------
 * Definitions and macros
 */


/*----------------------------------------------------------------------------
 * Local variables
 */

/*----------------------------------------------------------------------------
 * sa_builder_init_macsec
 *
 * This function initializes the sa_builder_params_t data structure and its
 * sa_builder_params_macsec_t extension with sensible defaults for MACsec
 * processing.
 *
 * sa_params_p (output)
 *   Pointer to SA parameter structure to be filled in.
 * sa_params_macsec_p (output)
 *   Pointer to MACsec parameter extension to be filled in
 * sci_p (input)
 *   Pointer to Secure Channel identifier, 8 bytes.
 * an (input)
 *   Association number, a number for 0 to 3.
 * direction (input)
 *   Must be one of SAB_DIRECTION_INBOUND or SAB_DIRECTION_OUTBOUND.
 *
 * Both the crypto and the authentication algorithm are initialized to
 * NULL. The crypto algorithm (which may remain NULL) must be set to
 * one of the algorithms supported by the protocol. The authentication
 * algorithm must also be set to one of the algorithms supported by
 * the protocol..Any required keys have to be specified as well.
 *
 * Both the sa_params_p and sa_params_macsec_p input parameters must point
 * to valid storage where variables of the appropriate type can be
 * stored. This function initializes the link from sa_params_p to
 * sa_params_macsec_p.
 *
 * Return:
 * SAB_STATUS_OK on success
 * SAB_INVALID_PARAMETER when one of the pointer parameters is NULL
 *   or the remaining parameters have illegal values.
 */
sa_builder_status_t
sa_builder_init_macsec(
    sa_builder_params_t * const sa_params_p,
    sa_builder_params_macsec_t * const sa_params_macsec_p,
    const u8 *sci_p,
    const u8 an,
    const sa_builder_direction_t direction)
{
#ifdef SAB_STRICT_ARGS_CHECK
    if (sa_params_p == NULL || sa_params_macsec_p == NULL || sci_p == NULL)
    {
        LOG_CRIT("sa_builder_init_macsec: NULL pointer parameter supplied.\n");
        return SAB_INVALID_PARAMETER;
    }

    if (an > 3)
    {
        LOG_CRIT("sa_builder_init_macsec: Invalid Association number.\n");
        return SAB_INVALID_PARAMETER;
    }

    if (direction != SAB_DIRECTION_OUTBOUND &&
        direction != SAB_DIRECTION_INBOUND)
    {
        LOG_CRIT("sa_builder_init_esp: Invalid direction.\n");
        return SAB_INVALID_PARAMETER;
    }
#endif

    sa_params_p->protocol = SAB_PROTO_MACSEC;
    sa_params_p->direction = direction;
    sa_params_p->protocol_extension_p = (void*)sa_params_macsec_p;
    sa_params_p->flags = 0;
    sa_params_p->redirect_interface = 0;

    sa_params_p->crypto_algo = SAB_CRYPTO_NULL;
    sa_params_p->crypto_mode = SAB_CRYPTO_MODE_CBC;
    sa_params_p->iv_src = SAB_IV_SRC_DEFAULT;
    sa_params_p->crypto_parameter = 0;
    sa_params_p->key_byte_count = 0;
    sa_params_p->key_p = NULL;
    sa_params_p->iv_p = NULL;
    sa_params_p->nonce_p = NULL;

    sa_params_p->auth_algo = SAB_AUTH_NULL;
    sa_params_p->auth_key1_p = NULL;
    sa_params_p->auth_key2_p = NULL;
    sa_params_p->auth_key3_p = NULL;
    sa_params_p->auth_key_byte_count = 0;

    sa_params_p->offset_arc4_state_record = 0;
    sa_params_p->cw0 = 0;
    sa_params_p->cw1 = 0;
    sa_params_p->offset_digest0 = 0;
    sa_params_p->offset_digest1 = 0;
    sa_params_p->offset_seq_num = 0;
    sa_params_p->offset_seq_mask = 0;
    sa_params_p->offset_iv = 0;
    sa_params_p->offset_ij_ptr = 0;
    sa_params_p->offset_arc4_state = 0;
    sa_params_p->seq_num_word32_count = 0;
    sa_params_p->seq_mask_word32_count = 0;
    sa_params_p->iv_word32_count = 0;

    sa_params_macsec_p->macsec_flags = 0;
    sa_params_macsec_p->sci_p = sci_p;
    sa_params_macsec_p->an = an;
    sa_params_macsec_p->seq_num = 0;
    sa_params_macsec_p->replay_window = 0;
    sa_params_macsec_p->conf_offset = 0;
    sa_params_macsec_p->context_ref = 0;

    return SAB_STATUS_OK;
}

/*----------------------------------------------------------------------------
 * sa_builder_set_macsec_params
 *
 * Fill in MACsec-specific extensions into the SA.
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
sa_builder_set_macsec_params(sa_builder_params_t *const sa_params_p,
                          sa_builder_state_t * const sa_state_p,
                          u32 * const sa_buffer_p)
{
    sa_builder_params_macsec_t *sa_params_macsec_p;
    sa_params_macsec_p = (sa_builder_params_macsec_t *)
        (sa_params_p->protocol_extension_p);
    if (sa_params_macsec_p == NULL)
    {
        LOG_CRIT("SABuilder: MACsec extension pointer is null\n");
        return SAB_INVALID_PARAMETER;
    }

    /* Allow only AES-GMAC and AES-GCM */
    if (sa_params_p->auth_algo != SAB_AUTH_AES_GCM &&
        sa_params_p->auth_algo != SAB_AUTH_AES_GMAC)
    {
        LOG_CRIT("SABuilder: Only AES-GCM and GMAC allowed wtih MACsec\n");
        return SAB_INVALID_PARAMETER;
    }

    if ( (sa_params_macsec_p->macsec_flags & SAB_MACSEC_ES) != 0 &&
         (sa_params_macsec_p->macsec_flags & SAB_MACSEC_SC) != 0)
    {
        LOG_CRIT("SABuilder: MACSEC if ES is set, then SC must be zero,\n");
        return SAB_INVALID_PARAMETER;
    }

    /* Add sequence number */
    sa_state_p->cw0 |= SAB_CW0_SEQNUM_32;
    sa_params_p->offset_seq_num = sa_state_p->current_offset;
    sa_params_p->seq_num_word32_count = 1;
    sa_state_p->cw1 |= SAB_CW1_SEQNUM_STORE;

    if (sa_buffer_p != NULL)
        sa_buffer_p[sa_state_p->current_offset] = sa_params_macsec_p->seq_num;
    sa_state_p->current_offset += 1;

    if (sa_params_p->direction == SAB_DIRECTION_OUTBOUND)
    {
        if (sa_params_p->crypto_mode == SAB_CRYPTO_MODE_GCM)
            sa_state_p->cw0 |= SAB_CW0_TOP_ENCRYPT_HASH;
        else
            sa_state_p->cw0 |= SAB_CW0_TOP_HASH_ENCRYPT;
        /* Some versions of the hardware can update the sequence number
           early, so multiple engines can operate in parallel. */
        sa_state_p->cw1 |= SAB_CW1_EARLY_SEQNUM_UPDATE;
        sa_state_p->cw1 |= sa_params_p->offset_seq_num << 24;
    }
    else
    {
        sa_state_p->cw0 |= SAB_CW0_TOP_HASH_DECRYPT;

        /* Add 'sequence number mask' parameter, which is the replay
           window size */
        sa_params_p->offset_seq_mask = sa_state_p->current_offset;
        if(sa_buffer_p != NULL)
        {
            sa_buffer_p[sa_state_p->current_offset] =
                sa_params_macsec_p->replay_window;
            sa_buffer_p[sa_state_p->current_offset+1] = 0; // Add dummy mask word.
        }
        sa_params_p->seq_mask_word32_count = 1;
        sa_state_p->current_offset += 2;
        sa_state_p->cw0 |= SAB_CW0_MASK_64; /* This setting for MACsec */
        sa_state_p->cw1 |= SAB_CW1_MACSEC_SEQCHECK|SAB_CW1_NO_MASK_UPDATE;
    }

    /* Add SCI (IV0 and IV1) */
    sa_state_p->cw1 |= SAB_CW1_IV_CTR | SAB_CW1_IV0 | SAB_CW1_IV1 | SAB_CW1_IV2;
#ifdef SAB_STRICT_ARGS_CHECK
    if (sa_params_macsec_p->sci_p == NULL)
    {
        LOG_CRIT("SABuilder: NULL pointer SCI.\n");
        return SAB_INVALID_PARAMETER;
    }
#endif
    sa_params_p->offset_iv = sa_state_p->current_offset;
    sa_params_p->iv_word32_count = 2;

    if (sa_params_macsec_p->sci_p == NULL)
    {
        return SAB_INVALID_PARAMETER;
    }
    sa_builder_lib_copy_key_mat(sa_buffer_p, sa_state_p->current_offset,
                            sa_params_macsec_p->sci_p, 8);
    sa_state_p->current_offset += 2;

    /* Add sequence number once more (IV2) */
    if (sa_buffer_p != NULL)
        sa_buffer_p[sa_state_p->current_offset] = sa_params_macsec_p->seq_num;
    sa_state_p->current_offset += 1;

    return SAB_STATUS_OK;
}


#endif /* SAB_ENABLE_PROTO_MACSEC */

/* end of file sa_builder_macsec.c */
