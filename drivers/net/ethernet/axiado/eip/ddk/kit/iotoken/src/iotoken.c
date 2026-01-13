/* iotoken.c
 *
 * IOToken API implementation for the EIP-197 Server with ice and optional oce
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

/*----------------------------------------------------------------------------
 * This module implements (provides) the following interface(s):
 */

// Extended IOToken API
#include "iotoken_ext.h"


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Default configuration
#include "c_iotoken.h"              // IOTOKEN_STRICT_ARGS

// Driver Framework Basic Definitions API
#include "basic_defs.h"             // IDENTIFIER_NOT_USED, bool, u32

// Firmware packet flow codes
#include "firmware_eip207_api_cmd.h"

// zeroinit
#include "clib.h"

#include "log.h"

// Packet buffer access
#include "adapter_pec_pktbuf.h"

/*----------------------------------------------------------------------------
 * Definitions and macros
 */

#define IOTOKEN_ARGUMENT_ERROR      -1
#define IOTOKEN_INTERNAL_ERROR      -2

#ifdef IOTOKEN_STRICT_ARGS
#define IOTOKEN_CHECK_POINTER(_p) \
    if (NULL == (_p)) \
        return IOTOKEN_ARGUMENT_ERROR;
#define IOTOKEN_CHECK_INT_INRANGE(_i, _min, _max) \
    if ((_i) < (_min) || (_i) > (_max)) \
        return IOTOKEN_ARGUMENT_ERROR;
#define IOTOKEN_CHECK_INT_ATLEAST(_i, _min) \
    if ((_i) < (_min)) \
        return IOTOKEN_ARGUMENT_ERROR;
#define IOTOKEN_CHECK_INT_ATMOST(_i, _max) \
    if ((_i) > (_max)) \
        return IOTOKEN_ARGUMENT_ERROR;
#else
/* IOTOKEN_STRICT_ARGS undefined */
#define IOTOKEN_CHECK_POINTER(_p)
#define IOTOKEN_CHECK_INT_INRANGE(_i, _min, _max)
#define IOTOKEN_CHECK_INT_ATLEAST(_i, _min)
#define IOTOKEN_CHECK_INT_ATMOST(_i, _max)
#endif /*end of IOTOKEN_STRICT_ARGS */

// Input Token words offsets
#define IOTOKEN_HDR_IN_WORD_OFFS           0
#define IOTOKEN_APP_ID_IN_WORD_OFFS        1
#define IOTOKEN_SA_ADDR_LO_IN_WORD_OFFS    2
#define IOTOKEN_SA_ADDR_HI_IN_WORD_OFFS    3
#define IOTOKEN_HW_SERVICES_IN_WORD_OFFS   4
#define IOTOKEN_NH_OFFSET_IN_WORD_OFFS     5
#define IOTOKEN_BP_DATA_IN_WORD_OFFS       6

// Output Token words offsets
#define IOTOKEN_HDR_OUT_WORD_OFFS          0
#define IOTOKEN_BP_LEN_OUT_WORD_OFFS       1
#define IOTOKEN_APP_ID_OUT_WORD_OFFS       2
#define IOTOKEN_PAD_NH_OUT_WORD_OFFS       3
#define IOTOKEN_SA_ADDR_LO_OUT_WORD_OFFS   4
#define IOTOKEN_SA_ADDR_HI_OUT_WORD_OFFS   5
#define IOTOKEN_NPH_CTX_OUT_WORD_OFFS      6
#define IOTOKEN_PREV_NH_OFFS_OUT_WORD_OFFS 7
#define IOTOKEN_BP_DATA_OUT_WORD_OFFS      8

#define IOTOKEN_MARK                       0xEC00


/*----------------------------------------------------------------------------
 * Local variables
 */


/*----------------------------------------------------------------------------
 * io_token_in_word_count_get
 */
unsigned int
io_token_in_word_count_get(void)
{
    return IOTOKEN_IN_WORD_COUNT - 4 + IOTOKEN_BYPASS_WORD_COUNT;
}


/*----------------------------------------------------------------------------
 * io_token_out_word_count_get
 */
unsigned int
io_token_out_word_count_get(void)
{
    return IOTOKEN_OUT_WORD_COUNT - 4 + IOTOKEN_BYPASS_WORD_COUNT;
}


/*----------------------------------------------------------------------------
 * io_token_create
 */
int
io_token_create(
        const io_token_input_dscr_t * const dscr_p,
        u32 * data_p)
{
    io_token_input_dscr_ext_t * dscr_ext_p;
#if IOTOKEN_BYPASS_WORD_COUNT > 0
    unsigned int bypass_gap = 0;
#endif
    unsigned int i;

    IOTOKEN_CHECK_POINTER(dscr_p);
    IOTOKEN_CHECK_POINTER(dscr_p->ext_p);
    IOTOKEN_CHECK_POINTER(data_p);

    dscr_ext_p = (io_token_input_dscr_ext_t *)dscr_p->ext_p;

    // Input Token word: EIP-96 Token Header Word
    {
        i = IOTOKEN_HDR_IN_WORD_OFFS;
	LOG_DEBG("i(IOTOKEN_HDR_IN_WORD_OFFS)=%d\n", i);

        // Set initialization value in the Token Header Word excluding packet
        // size field
        data_p[i] = dscr_p->tkn_hdr_word_init & ~MASK_16_BITS;

        // Input packet size
        data_p[i] |= dscr_p->in_packet_byte_count & MASK_16_BITS;

        if (dscr_ext_p->hw_services == IOTOKEN_CMD_PKT_LAC)
        {
            // options, arc4 pre-fetch
            if (dscr_ext_p->f_arc4_prefetch)
                data_p[i] |= BIT_16;

            data_p[i] |= BIT_17; // Set token header format to EIP-(1)97

            data_p[i] |= BIT_18; // Only 64-bit Context (SA) pointer is supported

            // Enable Context Reuse auto detect if no new SA
            if (dscr_p->options.f_reuse_sa)
                data_p[i] |= BIT_21;
        }
        // mask size for inbound ESP
        if (dscr_ext_p->sequence_mask_bit_count != 0)
        {
            switch(dscr_ext_p->sequence_mask_bit_count)
            {
            case 64:
                data_p[i] |= 0x00200000;
                break;
            case 128:
                data_p[i] |= 0x00400000;
                break;
            case 256:
                data_p[i] |= 0x00600000;
                break;
            case 512:
                data_p[i] |= 0x00800000;
                break;
            }
        }
        // Extended options for LIP
        if (dscr_ext_p->f_enc_last_dest)
            data_p[i] |= BIT_25;
        // Extended options for in-line DTLS packet flow
        {
            if (dscr_ext_p->options.f_capwap)
                data_p[i] |= BIT_26;

            if (dscr_ext_p->options.f_inbound)
                data_p[i] |= BIT_27;

            data_p[i] |= dscr_ext_p->options.content_type << 28;
        }

        data_p[i] |= IOTOKEN_FLOW_TYPE << 30; // Implemented flow type
    }

    // Input Token word: Application ID
    {
        i = IOTOKEN_APP_ID_IN_WORD_OFFS;

	LOG_DEBG("i(IOTOKEN_APP_ID_IN_WORD_OFFS)=%d\n", i);
        data_p[i] = (dscr_p->app_id & MASK_7_BITS) << 9;
        data_p[i] |= (dscr_ext_p->aad_byte_count & MASK_8_BITS);
        if (dscr_ext_p->f_inline)
            data_p[i] |= (32 + IOTOKEN_BYPASS_WORD_COUNT*4) << 16;
    }

    // Input Token word: Context (SA) low 32 bits physical address
    {
        i = IOTOKEN_SA_ADDR_LO_IN_WORD_OFFS;
        data_p[i] = dscr_p->sa_phys_addr.lo;
    }

    // Input Token word: Context (SA) high 32 bits physical address
    {
        i = IOTOKEN_SA_ADDR_HI_IN_WORD_OFFS;
        //if (dscr_ext_p->options.f64bit_sa) // Only 64-bit SA address supported
            data_p[i] = dscr_p->sa_phys_addr.hi;
    }

    // Input Token word: HW services, e,g, packet flow selection
    {
        i = IOTOKEN_HW_SERVICES_IN_WORD_OFFS;
        data_p[i] = ((dscr_ext_p->hw_services & MASK_8_BITS) << 24) |
            (dscr_ext_p->user_def & MASK_16_BITS);
        if (dscr_ext_p->f_strip_padding != IOTOKEN_PADDING_DEFAULT_ON)
            data_p[i] |= BIT_22;
        if (dscr_ext_p->f_allow_padding != IOTOKEN_PADDING_DEFAULT_ON)
            data_p[i] |= BIT_23;
        if (dscr_ext_p->f_redirect_enable) {
            data_p[i] |= BIT_20 | (dscr_ext_p->redirect_ifc & MASK_4_BITS) << 16;
        if (dscr_ext_p->f_l4_chksum_outbound)
            data_p[i] |= BIT_21;
        }
    }

    // Input Token word: offset and Next Header
    {
        i = IOTOKEN_NH_OFFSET_IN_WORD_OFFS;
	LOG_DEBG("i(IOTOKEN_NH_OFFSET_IN_WORD_OFFS)=%d\n", i);
        data_p[i] = (dscr_ext_p->offset_byte_count & MASK_8_BITS) << 8;
        data_p[i] |= (dscr_ext_p->next_header & MASK_8_BITS) << 16;
        if (dscr_ext_p->f_fl)
            data_p[i] |= BIT_24;
        if (dscr_ext_p->f_ipv4_chksum)
            data_p[i] |= BIT_25;
        if (dscr_ext_p->f_l4_chksum)
            data_p[i] |= BIT_26;
        if (dscr_ext_p->f_parse_ether)
            data_p[i] |= BIT_27;
        if (dscr_ext_p->f_keep_outer)
            data_p[i] |= BIT_28;

    }

    if (dscr_ext_p->f_inline)
    {
        data_p[IOTOKEN_BP_DATA_IN_WORD_OFFS]=0;
        data_p[IOTOKEN_BP_DATA_IN_WORD_OFFS+1]=0;
#if IOTOKEN_BYPASS_WORD_COUNT > 0
        bypass_gap = 2;
#endif
    }

#if IOTOKEN_BYPASS_WORD_COUNT > 0
    // Input Token word: Bypass data
    {
        unsigned int j;

        if (dscr_ext_p->bypass_data_p)
        {
            for (j = 0; j < IOTOKEN_BYPASS_WORD_COUNT; j++)
                data_p[j+IOTOKEN_BP_DATA_IN_WORD_OFFS + bypass_gap] = dscr_ext_p->bypass_data_p[j];
        }
        else
        {
            for (j = 0; j < IOTOKEN_BYPASS_WORD_COUNT; j++)
                data_p[j+IOTOKEN_BP_DATA_IN_WORD_OFFS + bypass_gap] = 0xf0f0f0f0;
        }
	LOG_DEBG("i(IOTOKEN_BYPASS_WORD_COUNT)=%d\n", i);
        i += bypass_gap + j;
    }
#endif

    if (i  >= IOTOKEN_IN_WORD_COUNT_IL){

	LOG_DEBG("i(at return)=%d\n", i);
        return IOTOKEN_INTERNAL_ERROR;
    }else{
	LOG_DEBG("i(at return)=%d\n", i);
        return i+1;
	}
}


/*----------------------------------------------------------------------------
 * io_token_fixup
 */
int
io_token_fixup(
        u32 * const data_p,
        const dma_buf_handle_t packet_handle)
{
    u32 append_flags;
    if (data_p == NULL)
        return 0;

    append_flags = data_p[IOTOKEN_BP_LEN_OUT_WORD_OFFS] & (BIT_31|BIT_30|BIT_29);
    if (append_flags != 0)
    {
        u8 append_data[12];
        u8 *append_p;
        unsigned int offset;
        unsigned int prev_nh_offset;
        unsigned int update_offset;
        unsigned int append_len = 0;
        if ((append_flags & BIT_31) != 0)
            append_len+=4;
        if ((append_flags & BIT_30) != 0)
            append_len+=4;
        if ((append_flags & BIT_29) != 0)
            append_len+=4;
        update_offset = data_p[IOTOKEN_HDR_OUT_WORD_OFFS] & MASK_17_BITS;
        update_offset = 4*((update_offset + 3)/4); // Align to word boundary.
        offset = data_p[IOTOKEN_PAD_NH_OUT_WORD_OFFS] >> 16 & MASK_8_BITS;
        // Get first byte of header
        append_p = adapter_pec_pkt_data_get(packet_handle, append_data, update_offset, append_len);
        if (append_flags == (BIT_31|BIT_30))
        { // IPv6 header update, NH plus length
            prev_nh_offset = data_p[IOTOKEN_PREV_NH_OFFS_OUT_WORD_OFFS] & MASK_16_BITS;
            adapter_pec_pkt_byte_put(packet_handle, prev_nh_offset, append_p[4]);
            adapter_pec_pkt_byte_put(packet_handle, offset + 4, append_p[0]);
            adapter_pec_pkt_byte_put(packet_handle, offset + 5, append_p[1]);
            LOG_INFO("io_token_fixup: IPv6 inbound transport\n");
        }
        else if (append_flags == BIT_29)
        { // IPv4 checksum only update
            adapter_pec_pkt_byte_put(packet_handle, offset + 10, append_p[0]);
            adapter_pec_pkt_byte_put(packet_handle, offset + 11, append_p[1]);
            LOG_INFO("io_token_fixup: IPv4 inbound tunnel\n");
        }
        else if (append_flags == (BIT_31|BIT_30|BIT_29))
        { // IPv4 header update, proto + length + checksum.
            adapter_pec_pkt_byte_put(packet_handle, offset + 2, append_p[0]);
            adapter_pec_pkt_byte_put(packet_handle, offset + 3, append_p[1]);
            adapter_pec_pkt_byte_put(packet_handle, offset + 9, append_p[5]);
            adapter_pec_pkt_byte_put(packet_handle, offset + 10, append_p[8]);
            adapter_pec_pkt_byte_put(packet_handle, offset + 11, append_p[9]);
            LOG_INFO("io_token_fixup: IPv4 inbound transport\n");
        }
        else
        {
            LOG_CRIT("io_token_fixup: Unexpected flags combination 0x%08x\n",
                     append_flags);
            return -1;
        }
    }
    return 0;
}

/*----------------------------------------------------------------------------
 * io_token_sa_addr_update
 */
int
io_token_sa_addr_update(
        const io_token_phys_addr_t * const sa_phys_addr_p,
        u32 * in_token_data_p)
{
    IOTOKEN_CHECK_POINTER(sa_phys_addr_p);
    IOTOKEN_CHECK_POINTER(in_token_data_p);

    // Input Token word: Context (SA) low 32 bits physical address
    in_token_data_p[IOTOKEN_SA_ADDR_LO_IN_WORD_OFFS] = sa_phys_addr_p->lo;

    // Input Token word: Context (SA) high 32 bits physical address
    in_token_data_p[IOTOKEN_SA_ADDR_HI_IN_WORD_OFFS] = sa_phys_addr_p->hi;

    return 2; // updated 32-bit token words
}


/*----------------------------------------------------------------------------
 * io_token_sa_reuse_update
 */
int
io_token_sa_reuse_update(
        const bool f_reuse_sa,
        u32 * in_token_data_p)
{
    IOTOKEN_CHECK_POINTER(in_token_data_p);

    // Enable Context Reuse auto detect if no new SA
    if (f_reuse_sa)
        in_token_data_p[IOTOKEN_HDR_IN_WORD_OFFS] |= BIT_21;
    else
        in_token_data_p[IOTOKEN_HDR_IN_WORD_OFFS] &= ~BIT_21;

    return 1;
}


/*----------------------------------------------------------------------------
 * io_token_mark_set
 */
int
io_token_mark_set(
        u32 * in_token_data_p)
{
    IOTOKEN_CHECK_POINTER(in_token_data_p);

    in_token_data_p[IOTOKEN_APP_ID_IN_WORD_OFFS] &= ~(MASK_7_BITS << 9);
    in_token_data_p[IOTOKEN_APP_ID_IN_WORD_OFFS] |= IOTOKEN_MARK;

    return 1;
}


/*----------------------------------------------------------------------------
 * IOToken_Mark_Offset_Get
 */
int
io_token_out_mark_offset_get(void)
{
    return IOTOKEN_APP_ID_OUT_WORD_OFFS;
}


/*----------------------------------------------------------------------------
 * io_token_mark_check
 */
int
io_token_mark_check(
        u32 * out_token_data_p)
{
    u32 mark;

    IOTOKEN_CHECK_POINTER(out_token_data_p);

    mark = out_token_data_p[IOTOKEN_APP_ID_OUT_WORD_OFFS] & IOTOKEN_MARK;

    if (mark == IOTOKEN_MARK)
        return 0;
    else
        return 1;
}


/*----------------------------------------------------------------------------
 * io_token_parse
 */
int
io_token_parse(
        const u32 * data_p,
        io_token_output_dscr_t * const dscr_p)
{
    unsigned int i, j = 0;
    io_token_output_dscr_ext_t * ext_p;

    IOTOKEN_CHECK_POINTER(data_p);
    IOTOKEN_CHECK_POINTER(dscr_p);

    ext_p = (io_token_output_dscr_ext_t *)dscr_p->ext_p;

    // Output Token word: EIP-96 Output Token Header Word
    {
        i = IOTOKEN_HDR_OUT_WORD_OFFS;
        dscr_p->out_packet_byte_count = data_p[i]       & MASK_17_BITS;
        dscr_p->error_code           = data_p[i] >> 17 & MASK_15_BITS;
    }

    // Output Token word: EIP-96 Output Token Bypass data Length Word
    {
        i = IOTOKEN_BP_LEN_OUT_WORD_OFFS;
        dscr_p->bypass_data_byte_count =  data_p[i] & MASK_4_BITS;
        dscr_p->f_hash_appended        = (data_p[i] & BIT_21) != 0;

        dscr_p->hash_byte_count       = data_p[i] >> 22 & MASK_6_BITS;

        dscr_p->f_bytes_appended       = (data_p[i] & BIT_28) != 0;
        dscr_p->f_checksum_appended    = (data_p[i] & BIT_29) != 0;
        dscr_p->f_next_header_appended  = (data_p[i] & BIT_30) != 0;
        dscr_p->f_length_appended      = (data_p[i] & BIT_31) != 0;
    }

    // Output Token word: EIP-96 Output Token Application ID Word
    {
        i = IOTOKEN_APP_ID_OUT_WORD_OFFS;
        dscr_p->app_id = data_p[i] >> 9 & MASK_7_BITS;
    }

    // Output Token word: Pad Length and Next Header
    {
        i = IOTOKEN_PAD_NH_OUT_WORD_OFFS;
        dscr_p->next_header    = data_p[i]      & MASK_8_BITS;
        dscr_p->pad_byte_count = data_p[i] >> 8 & MASK_8_BITS;
    }

    // Parse Output Token descriptor extension if requested
    if (ext_p)
    {
        // Output Token word: ToS/TC, df, Firmware errors
        {
            j = IOTOKEN_BP_LEN_OUT_WORD_OFFS;
            ext_p->TOS_TC    = data_p[j] >> 5  & MASK_8_BITS;

            ext_p->f_df       = (data_p[j] & BIT_13) != 0;

            ext_p->cfy_errors = data_p[j] >> 16 & MASK_5_BITS;
        }

#ifdef IOTOKEN_EXTENDED_ERRORS_ENABLE
        if ((data_p[IOTOKEN_HDR_OUT_WORD_OFFS] & BIT_31) != 0)
        {
            ext_p->ext_errors = (data_p[IOTOKEN_HDR_OUT_WORD_OFFS] >> 17) & MASK_8_BITS;
        }
        else
#endif
        {
            ext_p->ext_errors = 0;
        }

        // Output Token word: IP Length Delta and offset
        {
            j = IOTOKEN_PAD_NH_OUT_WORD_OFFS;
            ext_p->offset_byte_count  = data_p[j] >> 16 & MASK_8_BITS;
            ext_p->ip_delta_byte_count = data_p[j] >> 24 & MASK_8_BITS;
        }

        // Output Token word: SA physical address low/high words
        {
            j = IOTOKEN_SA_ADDR_LO_OUT_WORD_OFFS;
            ext_p->sa_phys_addr.lo = data_p[j];

            j = IOTOKEN_SA_ADDR_HI_OUT_WORD_OFFS;
            ext_p->sa_phys_addr.hi = data_p[j];
        }

        // Output Token word: Application header processing context
        {
            j = IOTOKEN_NPH_CTX_OUT_WORD_OFFS;
            ext_p->nph_context = data_p[j];
        }

        // Output Token word: Previous Next Header offset
        {
            j = IOTOKEN_PREV_NH_OFFS_OUT_WORD_OFFS;
            ext_p->next_header_offset = data_p[j] & MASK_16_BITS;
            ext_p->f_in_ipv6 = ((data_p[j] & BIT_16) != 0);
            ext_p->f_from_ether = ((data_p[j] & BIT_17) != 0);
            ext_p->f_out_ipv6 = ((data_p[j] & BIT_22) != 0);
            ext_p->f_inb_tunnel = ((data_p[j] & BIT_23) != 0);
        }

#if IOTOKEN_BYPASS_WORD_COUNT > 0
        // Output Token word: Bypass data
        {
            unsigned int k = 0;
            if (ext_p->bypass_data_p)
            {
                for (k = 0; k < IOTOKEN_BYPASS_WORD_COUNT; k++)
                    ext_p->bypass_data_p[k] =
                        data_p[k+IOTOKEN_BP_DATA_OUT_WORD_OFFS];
            }
            j += k;
        }
#endif
    }

    // Output Token word: reserved word not used
    // i = IOTOKEN_RSVD_OUT_WORD_OFFS;

    return MAX(i, j);
}


/*----------------------------------------------------------------------------
 * io_token_packet_legth_get
 */
int
io_token_packet_legth_get(
        const u32 * data_p,
        unsigned int * pkt_byte_count_p)
{
    IOTOKEN_CHECK_POINTER(data_p);
    IOTOKEN_CHECK_POINTER(pkt_byte_count_p);

    *pkt_byte_count_p = data_p[IOTOKEN_HDR_OUT_WORD_OFFS] & MASK_17_BITS;

    return 1;
}


/*----------------------------------------------------------------------------
 * io_token_bypass_legth_get
 */
int
io_token_bypass_legth_get(
        const u32 * data_p,
        unsigned int * bd_byte_count_p)
{
    IOTOKEN_CHECK_POINTER(data_p);
    IOTOKEN_CHECK_POINTER(bd_byte_count_p);

    *bd_byte_count_p = data_p[IOTOKEN_BP_LEN_OUT_WORD_OFFS] & MASK_4_BITS;

    return 1;
}


/*----------------------------------------------------------------------------
 * io_token_error_code_get
 */
int
io_token_error_code_get(
        const u32 * data_p,
        unsigned int * error_code_p)
{
    IOTOKEN_CHECK_POINTER(data_p);
    IOTOKEN_CHECK_POINTER(error_code_p);

    *error_code_p = (data_p[IOTOKEN_HDR_OUT_WORD_OFFS] >> 17) & MASK_15_BITS;
#ifdef IOTOKEN_EXTENDED_ERRORS_ENABLE
    if ((data_p[IOTOKEN_HDR_OUT_WORD_OFFS] & BIT_31) != 0)
    {
        *error_code_p &= ~0x40ff; // Remove the detailed error code and related bits.
    }
#endif

    return 1;
}


/* end of file iotoken.c */
