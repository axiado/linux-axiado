/* sa_builder_srtp.c
 *
 * SRTP specific functions (for initialization of
 * sa_builder_params_t structures and for building the SRTP
 * specifc part of an SA.).
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
 * This module implements (provides) the following interface(s):
 */
#include "sa_builder_srtp.h"
#include "sa_builder_internal.h" /* sa_builder_set_ssltls_params */

/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */
#include "c_sa_builder.h"
#include "basic_defs.h"
#include "log.h"

#ifdef SAB_ENABLE_PROTO_SRTP
/*----------------------------------------------------------------------------
 * Definitions and macros
 */


/*----------------------------------------------------------------------------
 * Local variables
 */

/*----------------------------------------------------------------------------
 * sa_builder_init_srtp
 *
 * This function initializes the sa_builder_params_t data structure and
 * its sa_builder_params_srtp_t extension with sensible defaults for
 * SRTP processing..
 *
 * sa_params_p (output)
 *   Pointer to SA parameter structure to be filled in.
 * sa_params_srtp_p (output)
 *   Pointer to SRTP parameter extension to be filled in
 * is_srtcp (input)
 *   true if the SA is for SRTCP.
 * direction (input)
 *   Must be one of SAB_DIRECTION_INBOUND or SAB_DIRECTION_OUTBOUND.
 *
 * Tis function initializes the authentication algorithm to HMAC_SHA1.
 * The application has to fill in the appropriate keys. The crypto algorithm
 * is initialized to NULL. It can be changed to AES ICM and then a crypto
 * key has to be added as well.
 *
 * Both the sa_params_p and sa_params_srtp_p input parameters must point
 * to valid storage where variables of the appropriate type can be
 * stored. This function initializes the link from sa_params_p to
 * sa_params_srtp_p.
 *
 * Return:
 * SAB_STATUS_OK on success
 * SAB_INVALID_PARAMETER when one of the pointer parameters is NULL
 *   or the remaining parameters have illegal values.
 */
sa_builder_status_t
sa_builder_init_srtp(
        sa_builder_params_t * const sa_params_p,
        sa_builder_params_srtp_t * const sa_params_srtp_p,
        const bool is_srtcp,
        const sa_builder_direction_t direction)
{
#ifdef SAB_STRICT_ARGS_CHECK
    if (sa_params_p == NULL || sa_params_srtp_p == NULL)
    {
        LOG_CRIT("sa_builder_init_ssltls: NULL pointer parameter supplied.\n");
        return SAB_INVALID_PARAMETER;
    }

    if (direction != SAB_DIRECTION_OUTBOUND &&
        direction != SAB_DIRECTION_INBOUND)
    {
        LOG_CRIT("sa_builder_init_esp: Invalid direction.\n");
        return SAB_INVALID_PARAMETER;
    }
#endif

    sa_params_p->protocol = SAB_PROTO_SRTP;
    sa_params_p->direction = direction;
    sa_params_p->protocol_extension_p = (void*)sa_params_srtp_p;
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

    sa_params_p->auth_algo = SAB_AUTH_HMAC_SHA1;
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

    sa_params_srtp_p->srtp_flags = 0;
    if (is_srtcp)
        sa_params_srtp_p->srtp_flags |= SAB_SRTP_FLAG_SRTCP;
    sa_params_srtp_p->mki = 0;
    sa_params_srtp_p->icv_byte_count = 10;

    return SAB_STATUS_OK;
}

/*----------------------------------------------------------------------------
 * sa_builder_set_srtp_params
 *
 * Fill in SRTP-specific extensions into the SA.
 *
 * sa_params_p (input)
 *   The SA parameters structure from which the SA is derived.
 * sa_state_p (input, output)
 *   Variables containing information about the SA being generated.
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
sa_builder_set_srtp_params(sa_builder_params_t *const sa_params_p,
                        sa_builder_state_t * const sa_state_p,
                        u32 * const sa_buffer_p)
{
    sa_builder_params_srtp_t *sa_params_srtp_p;
    sa_params_srtp_p = (sa_builder_params_srtp_t *)
        (sa_params_p->protocol_extension_p);
    if (sa_params_srtp_p == NULL)
    {
        LOG_CRIT("SABuilder: SRTP extension pointer is null\n");
        return SAB_INVALID_PARAMETER;
    }

    if (sa_params_p->crypto_algo == SAB_CRYPTO_AES)
    {
        sa_state_p->cw1 &= ~0x7; // Clear crypto mode (CTR or ICM);
        sa_state_p->cw1 |= SAB_CW1_CRYPTO_MODE_CTR_LOAD | SAB_CW1_IV_CTR;
    }
    else if (sa_params_p->crypto_algo != SAB_CRYPTO_NULL)
    {
        LOG_CRIT("SABuilder: I: crypto algorithm not supported\n");
        return SAB_INVALID_PARAMETER;
    }

    if (sa_params_p->auth_algo != SAB_AUTH_HMAC_SHA1)
    {
        LOG_CRIT("SABuilder: I: authentication algorithm not supported\n");
        return SAB_INVALID_PARAMETER;
    }

    /* Add mki (as the spi field) */
    if ((sa_params_srtp_p->srtp_flags & SAB_SRTP_FLAG_INCLUDE_MKI) != 0)
    {
        sa_state_p->cw0 |= SAB_CW0_SPI;
        if (sa_buffer_p != NULL)
            sa_buffer_p[sa_state_p->current_offset] = sa_params_srtp_p->mki;
        sa_state_p->current_offset += 1;
    }

    if (sa_params_p->direction == SAB_DIRECTION_OUTBOUND)
    {
        if (sa_params_p->crypto_algo==SAB_CRYPTO_NULL)
            sa_state_p->cw0 |= SAB_CW0_TOP_HASH_OUT;
        else
            sa_state_p->cw0 |= SAB_CW0_TOP_ENCRYPT_HASH;
    }
    else
    {
        if (sa_params_p->crypto_algo==SAB_CRYPTO_NULL)
            sa_state_p->cw0 |= SAB_CW0_TOP_HASH_IN;
        else
            sa_state_p->cw0 |= SAB_CW0_TOP_HASH_DECRYPT;
    }

    return SAB_STATUS_OK;
}

#endif /* SAB_ENABLE_PROTO_SRTP */


/* end of file sa_builder_srtp.c */
