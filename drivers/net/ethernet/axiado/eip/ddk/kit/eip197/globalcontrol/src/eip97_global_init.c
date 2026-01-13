/* eip97_global_init.c
 *
 * EIP-97 Global Control Driver Library
 * Initialization Module
 */

/* -------------------------------------------------------------------------- */
/*                                                                            */
/*   Module        : ddk197                                                   */
/*   Version       : 5.7.2                                                    */
/*   configuration : DDK-197-BSD                                              */
/*                                                                            */
/*   Date          : 2025-Nov-13                                              */
/*                                                                            */
/* Copyright (c) 2008-2025 by Rambus, Inc. and/or its subsidiaries.           */
/*                                                                            */
/* Redistribution and use in source and binary forms, with or without         */
/* modification, are permitted provided that the following conditions are     */
/* met:                                                                       */
/*                                                                            */
/* 1. Redistributions of source code must retain the above copyright          */
/* notice, this list of conditions and the following disclaimer.              */
/*                                                                            */
/* 2. Redistributions in binary form must reproduce the above copyright       */
/* notice, this list of conditions and the following disclaimer in the        */
/* documentation and/or other materials provided with the distribution.       */
/*                                                                            */
/* 3. Neither the name of the copyright holder nor the names of its           */
/* contributors may be used to endorse or promote products derived from       */
/* this software without specific prior written permission.                   */
/*                                                                            */
/* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS        */
/* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT          */
/* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR      */
/* A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT       */
/* HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,     */
/* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT           */
/* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,      */
/* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY      */
/* THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT        */
/* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE      */
/* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.       */
/* -------------------------------------------------------------------------- */

/*----------------------------------------------------------------------------
 * This module implements (provides) the following interface(s):
 */

#include "eip97_global_init.h"

/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */


// Default configuration
#include "c_eip97_global.h"

// Driver Framework Basic Definitions API
#include "basic_defs.h"             // u32

// Driver Framework C Run-time Library API
#include "clib.h"                       // zeroinit

// Driver Framework device API
#include "device_types.h"           // device_handle_t

// EIP-97 Global Control Driver Library internal interfaces
#include "eip97_global_internal.h"
#include "eip97_global_level0.h"       // EIP-97 Level 0 macros
#include "eip202_global_init.h"        // EIP-202 Initialization code
#include "eip206_level0.h"             // EIP-206 Level 0 macros
#include "eip96_level0.h"              // EIP-96 Level 0 macros
#include "eip97_global_fsm.h"          // state machine

/*----------------------------------------------------------------------------
 * Definitions and macros
 */

// Maximum number of Packet Engines that should be used
// Driver Library will check the maximum number supported run-time
#ifndef EIP97_GLOBAL_MAX_NOF_PE_TO_USE
#error "EIP97_GLOBAL_MAX_NOF_PE_TO_USE is not defined"
#endif

// number of Ring interfaces
// Maximum number of Ring interfaces that should be used
// Driver Library will check the maximum number supported run-time
#ifndef EIP97_GLOBAL_MAX_NOF_RING_TO_USE
#error "EIP97_GLOBAL_MAX_NOF_RING_TO_USE is not defined"
#endif


/*----------------------------------------------------------------------------
 * Local variables
 */
static unsigned int global97_nof_p_es;
static unsigned int global97_nof_rings;
static unsigned int global97_nof_la;
static unsigned int global97_nof_in;
static unsigned int global97_dfedse_offset;
static unsigned int global97_supported_funcs;

/*----------------------------------------------------------------------------
 * eip206_lib_detect
 *
 * Checks the presence of EIP-206 PE hardware. Returns true when found.
 */
static bool
eip206_lib_detect(
        const device_handle_t device,
        const unsigned int p_enr)
{
    u32 value;

    // No revision register for this HW version

    // read-write test one of the registers

    // Set MASK_8_BITS bits of the EIP206_OUT_REG_DBUF_TRESH register
    eip206_write32(device,
                   EIP206_OUT_REG_DBUF_TRESH(p_enr),
                   MASK_8_BITS);
    value = eip206_read32(device,
                          EIP206_OUT_REG_DBUF_TRESH(p_enr));
    if ((value & MASK_8_BITS) != MASK_8_BITS)
        return false;

    // Clear MASK_8_BITS bits of the EIP206_OUT_REG_DBUF_TRESH(p_enr) register
    eip206_write32(device, EIP206_OUT_REG_DBUF_TRESH(p_enr), 0);
    value = eip206_read32(device, EIP206_OUT_REG_DBUF_TRESH(p_enr));
    if ((value & MASK_8_BITS) != 0)
       return false;

    return true;
}


/*----------------------------------------------------------------------------
 * eip96_lib_detect
 *
 * Checks the presence of EIP-96 Engine hardware. Returns true when found.
 */
static bool
eip96_lib_detect(
        const device_handle_t device,
        const unsigned int p_enr)
{
    u32 value, default_value;
    bool f_success = true;

    // No revision register for this HW version

    // Save the default register value
    default_value = eip96_read32(device,
                                EIP96_REG_CONTEXT_CTRL(p_enr));

    // read-write test one of the registers

    // Set MASK_6_BITS bits of the EIP96_REG_CONTEXT_CTRL register
    eip96_write32(device,
                  EIP96_REG_CONTEXT_CTRL(p_enr),
                  MASK_6_BITS );
    value = eip96_read32(device, EIP96_REG_CONTEXT_CTRL(p_enr));
    if ((value & MASK_6_BITS) != MASK_6_BITS)
        f_success = false;

    if( f_success )
    {
        // Clear MASK_6_BITS bits of the EIP96_REG_CONTEXT_CTRL register
        eip96_write32(device, EIP96_REG_CONTEXT_CTRL(p_enr), 0);
        value = eip96_read32(device, EIP96_REG_CONTEXT_CTRL(p_enr));
        if ((value & MASK_6_BITS) != 0)
            f_success = false;
    }

    // Restore the default register value
    eip96_write32(device,
            EIP96_REG_CONTEXT_CTRL(p_enr),
                  default_value );
    return f_success;
}


/*----------------------------------------------------------------------------
 * eip97_lib_detect
 *
 * Checks the presence of EIP-97 Engine hardware. Returns true when found.
 */
static bool
eip97_lib_detect(
        const device_handle_t device)
{
#ifdef EIP97_GLOBAL_VERSION_CHECK_ENABLE
    u32 value;

    // read and check the revision register
    value = eip97_read32(device, EIP97_REG_VERSION);
    if (!EIP97_REV_SIGNATURE_MATCH( value ))
        return false;
#else
    IDENTIFIER_NOT_USED(device);
#endif // EIP97_GLOBAL_VERSION_CHECK_ENABLE

    return true;
}


/*----------------------------------------------------------------------------
 * eip202_lib_hw_revision_get
 */
static void
eip202_lib_hw_revision_get(
        const device_handle_t device,
        eip202_options_t * const options_p,
        eip202_options2_t * const options2_p,
        eip_version_t * const version_p)
{
    eip202_capabilities_t eip202_capabilities;
    eip202_global_hw_revision_get(device, &eip202_capabilities);

    version_p->eip_number = eip202_capabilities.eip_number;
    version_p->complmt_eip_number = eip202_capabilities.complmt_eip_number;
    version_p->hw_patch_level = eip202_capabilities.hw_patch_level;
    version_p->min_hw_revision = eip202_capabilities.min_hw_revision;
    version_p->maj_hw_revision = eip202_capabilities.maj_hw_revision;

    options_p->nof_rings = eip202_capabilities.nof_rings;
    options_p->nof_pes = eip202_capabilities.nof_pes;
    options_p->f_exp_plf = eip202_capabilities.f_exp_plf;
    options_p->cf_size = eip202_capabilities.cf_size;
    options_p->rf_size = eip202_capabilities.rf_size;
    options_p->host_ifc = eip202_capabilities.host_ifc;
    options_p->dma_len = eip202_capabilities.dma_len;
    options_p->hdw = eip202_capabilities.hdw;
    options_p->tgt_align = eip202_capabilities.tgt_align;
    options_p->f_addr64 = eip202_capabilities.f_addr64;

    options2_p->nof_la_ifs = eip202_capabilities.nof_la_ifs;
    options2_p->nof_in_ifs = eip202_capabilities.nof_in_ifs;
    options2_p->nof_axi_wr_chs = eip202_capabilities.nof_axi_wr_chs;
    options2_p->nof_axi_rd_clusters = eip202_capabilities.nof_axi_rd_clusters;
    options2_p->nof_axi_rd_cpc = eip202_capabilities.nof_axi_rd_cpc;

}


/*----------------------------------------------------------------------------
 * eip96_lib_hw_revision_get
 */
static void
eip96_lib_hw_revision_get(
        const device_handle_t device,
        const unsigned int p_enr,
        eip96_options_t * const options_p,
        eip_version_t * const version_p)
{
    u32 options_val;
    EIP96_EIP_REV_RD(device,
                     p_enr,
                     &version_p->eip_number,
                     &version_p->complmt_eip_number,
                     &version_p->hw_patch_level,
                     &version_p->min_hw_revision,
                     &version_p->maj_hw_revision);

    EIP96_OPTIONS_RD(device,
                     p_enr,
                     &options_p->f_aes,
                     &options_p->f_ae_sfb,
                     &options_p->f_ae_sspeed,
                     &options_p->f_des,
                     &options_p->f_de_sfb,
                     &options_p->f_de_sspeed,
                     &options_p->arc4,
                     &options_p->f_aes_xts,
                     &options_p->f_wireless,
                     &options_p->f_md5,
                     &options_p->f_sha1,
                     &options_p->f_sha1speed,
                     &options_p->f_sha224_256,
                     &options_p->f_sha384_512,
                     &options_p->f_xcbc_mac,
                     &options_p->f_cbc_ma_cspeed,
                     &options_p->f_cbc_ma_ckeylens,
                     &options_p->f_ghash);
    global97_supported_funcs = device_read32(device, EIP96_REG_OPTIONS(0)) & ~(BIT_14|BIT_17);
    // Read supported cryptographic algorithms.
    options_val = device_read32(device, EIP97_REG_OPTIONS);
    // Add some EIP-97/196/197 option bits into unused bits of the value.
    // Only the EIP-197 can have an frc or oce.
    if ((options_val & BIT_30) != 0)
    {
        global97_supported_funcs |= BIT_17;  // Do we have a flow record cache?
    }
    if ((options_val & BIT_24) != 0)
    {
        global97_supported_funcs |= BIT_14;  // Do we have an oce?
    }
}


/*----------------------------------------------------------------------------
 * eip97_lib_hw_revision_get
 */
static void
eip97_lib_hw_revision_get(
        const device_handle_t device,
        eip97_options_t * const options_p,
        eip_version_t * const version_p)
{
    EIP97_EIP_REV_RD(device,
                     &version_p->eip_number,
                     &version_p->complmt_eip_number,
                     &version_p->hw_patch_level,
                     &version_p->min_hw_revision,
                     &version_p->maj_hw_revision);

    EIP97_OPTIONS_RD(device,
                     &options_p->nof_pes,
                     &options_p->in_tbuf_size,
                     &options_p->in_dbuf_size,
                     &options_p->out_tbuf_size,
                     &options_p->out_dbuf_size,
                     &options_p->central_prng,
                     &options_p->tg,
                     &options_p->trc);
}


/*----------------------------------------------------------------------------
 * eip97_lib_reset_is_done
 */
static eip97_global_error_t
eip97_lib_reset_is_done(
        const device_handle_t device,
        volatile u32 * state_p,
        const unsigned int p_enr)
{
    bool f_reset_done = eip202_global_reset_is_done(device, p_enr);

    if(f_reset_done)
    {
        // Transit to a new state
#ifdef EIP97_GLOBAL_DEBUG_FSM
        {
            eip97_global_error_t rv;

            rv = eip97_global_state_set((volatile eip97_global_state_t*)state_p,
                                        EIP97_GLOBAL_STATE_SW_RESET_DONE);
            if(rv != EIP97_GLOBAL_NO_ERROR)
                return EIP97_GLOBAL_ILLEGAL_IN_STATE;
        }
#endif // EIP97_GLOBAL_DEBUG_FSM
    }
    else
    {
#ifdef EIP97_GLOBAL_DEBUG_FSM
        {
            eip97_global_error_t rv;

            // SW Reset is ongoing, retry later
            rv = eip97_global_state_set((volatile eip97_global_state_t*)state_p,
                                        EIP97_GLOBAL_STATE_SW_RESET_START);
            if(rv != EIP97_GLOBAL_NO_ERROR)
                return EIP97_GLOBAL_ILLEGAL_IN_STATE;
        }
#endif // EIP97_GLOBAL_DEBUG_FSM

        return EIP97_GLOBAL_BUSY_RETRY_LATER;
    }

    return EIP97_GLOBAL_NO_ERROR;
}


/*----------------------------------------------------------------------------
 * eip97_global_init
 */
eip97_global_error_t
eip97_global_init(
        eip97_global_io_area_t * const io_area_p,
        const device_handle_t device)
{
    unsigned int i;
    eip97_global_capabilities_t capabilities;
    volatile eip97_true_io_area_t * const true_io_area_p = ioarea(io_area_p);

    zeroinit(capabilities);

    EIP97_GLOBAL_CHECK_POINTER(io_area_p);

    // Attempt to initialize slave byte swapping.
    if (!eip202_global_endianness_slave_configure(device))
        return EIP97_GLOBAL_UNSUPPORTED_FEATURE_ERROR;

    // Detect presence of the EIP-97 HW hardware
    if (!eip202_global_detect(device) ||
        !eip206_lib_detect(device, PE_DEFAULT_NR) ||
        !eip96_lib_detect(device, PE_DEFAULT_NR) ||
        !eip97_lib_detect(device))
        return EIP97_GLOBAL_UNSUPPORTED_FEATURE_ERROR;

    // Initialize the IO Area
    true_io_area_p->device = device;
    // Can also be EIP97_GLOBAL_STATE_HW_RESET_DONE
    true_io_area_p->state = (u32)EIP97_GLOBAL_STATE_SW_RESET_DONE;

    eip97_lib_hw_revision_get(device,
                            &capabilities.eip97_options,
                            &capabilities.eip97_version);

    eip202_lib_hw_revision_get(device,
                             &capabilities.eip202_options,
                             &capabilities.eip202_options2,
                             &capabilities.eip202_version);

    eip96_lib_hw_revision_get(device,
                            PE_DEFAULT_NR,
                            &capabilities.eip96_options,
                            &capabilities.eip96_version);

    // Check actual configuration HW against capabilities
    // number of configured PE's
    global97_nof_p_es = MIN(capabilities.eip202_options.nof_pes,
                          EIP97_GLOBAL_MAX_NOF_PE_TO_USE);

    // number of configure ring interfaces
    global97_nof_rings = MIN(capabilities.eip202_options.nof_rings,
                            EIP97_GLOBAL_MAX_NOF_RING_TO_USE);

    // number of configured Look-aside fifo interfaces
#if EIP97_GLOBAL_MAX_NOF_LAFIFO_TO_USE==0
    global97_nof_la = 0;
#else
    global97_nof_la = MIN(capabilities.eip202_options2.nof_la_ifs,
                         EIP97_GLOBAL_MAX_NOF_LAFIFO_TO_USE);
#endif
#if EIP97_GLOBAL_MAX_NOF_INFIFO_TO_USE==0
    global97_nof_in = 0;
#else
    // number of configured Inline fifo interfaces
    global97_nof_in = MIN(capabilities.eip202_options2.nof_in_ifs,
                         EIP97_GLOBAL_MAX_NOF_INFIFO_TO_USE);
#endif

    // EIP197 devices with more than 12 rings move the
    // DFE and DSE register addresses up by 2*4kB to make room for 2 extra
    // sets of ring control registers.
    if (capabilities.eip202_options.nof_rings > 12)
    {
        global97_dfedse_offset = 0x2000;
    }

#ifdef EIP97_CDR_COUNTER_UPDATE_WA
    // Force the CDR clock on to work around issues with descriptor counter
    // updates in EIP-202 hardware versions before 2.8.
    eip97_write32(device, EIP97_REG_FORCE_CLOCK_ON, 0x8);
#endif

#ifdef EIP97_GLOBAL_DEBUG_FSM
    {
        eip97_global_error_t rv;

        // Transit to a new state
        rv = eip97_global_state_set(
            (volatile eip97_global_state_t*)&true_io_area_p->state,
            EIP97_GLOBAL_STATE_INITIALIZED);
        if(rv != EIP97_GLOBAL_NO_ERROR)
            return EIP97_GLOBAL_ILLEGAL_IN_STATE;
    }
#endif // EIP97_GLOBAL_DEBUG_FSM




    // Configure Endianness Conversion method for master (DMA) interface
    for (i = 0; i < global97_nof_p_es; i++)
        EIP97_MST_CTRL_WR(device,
                          i,
                          EIP97_GLOBAL_RD_CACHE_VALUE,
                          EIP97_GLOBAL_WR_CACHE_VALUE,
                          EIP97_GLOBAL_BYTE_SWAP_METHOD,
                          EIP97_GLOBAL_SUPPORT_PROTECT_VALUE,
                          0,  // Disable cache-aligned context writes.
                          false);
    {
        u8 ipbuf_max, ipbuf_min,
                itbuf_max, itbuf_min,
                opbuf_max, opbuf_min;

#ifdef EIP97_GLOBAL_THRESH_CONFIG_AUTO
        // Calculate the EIP-202 and EIP-206 Global Control thresholds
        u8 dmalen;

        // Convert to powers of byte counts from powers of 32-bit word counts
        ipbuf_max = capabilities.eip97_options.in_dbuf_size + 2;
        itbuf_max = capabilities.eip97_options.in_tbuf_size + 2;
        opbuf_max = capabilities.eip97_options.out_dbuf_size + 2;

        // EIP-96 token cannot be larger than 2^7 bytes
        if( itbuf_max > EIP97_GLOBAL_MAX_TOKEN_SIZE )
            itbuf_max = EIP97_GLOBAL_MAX_TOKEN_SIZE;

        // dma_len is power of byte count
        if (capabilities.eip202_options.dma_len >= 1)
            dmalen = capabilities.eip202_options.dma_len - 1;
        else
            dmalen = capabilities.eip202_options.dma_len;

        ipbuf_max = MIN(ipbuf_max, dmalen);
        itbuf_max = MIN(itbuf_max, dmalen);
        opbuf_max = MIN(opbuf_max, dmalen);

        ipbuf_min = ipbuf_max - 1;
        itbuf_min = itbuf_max - 1;
        opbuf_min = opbuf_max - 1;
#else
        // Use configured statically
        // the EIP-202 and EIP-206 Global Control thresholds
        ipbuf_min = EIP97_GLOBAL_DFE_MIN_DATA_XFER_SIZE;
        ipbuf_max = EIP97_GLOBAL_DFE_MAX_DATA_XFER_SIZE;

        itbuf_min = EIP97_GLOBAL_DFE_MIN_TOKEN_XFER_SIZE;
        itbuf_max = EIP97_GLOBAL_DFE_MAX_TOKEN_XFER_SIZE;

        opbuf_min = EIP97_GLOBAL_DSE_MIN_DATA_XFER_SIZE;
        opbuf_max = EIP97_GLOBAL_DSE_MAX_DATA_XFER_SIZE;
#endif

        eip202_global_init(device,
                           global97_nof_p_es,
                           capabilities.eip202_options2.nof_la_ifs,
                           ipbuf_min,
                           ipbuf_max,
                           itbuf_min,
                           itbuf_max,
                           opbuf_min,
                           opbuf_max);
        for (i = 0; i < global97_nof_p_es; i++)
        {

            // Configure EIP-206 Processing Engine
            EIP206_IN_DBUF_THRESH_WR(
                            device,
                            i,
                            0,
                            ipbuf_min,
                            ipbuf_max); // ... or use 0xF for maximum, autoconf

            EIP206_IN_TBUF_THRESH_WR(
                            device,
                            i,
                            EIP97_GLOBAL_IN_TBUF_PKT_THR,
                            itbuf_min,
                            itbuf_max); // ... or use 0xF for maximum, autoconf

            EIP206_OUT_DBUF_THRESH_WR(
                            device,
                            i,
                            opbuf_min,
                            opbuf_max); // ... or use 0xF for maximum, autoconf

            // Configure EIP-96 Packet Engine
            EIP96_TOKEN_CTRL_STAT_WR(
                    device,
                    i,
                    true, /* optimal context update */
                    EIP97_GLOBAL_EIP96_NO_TOKEN_WAIT, /* CT No token wait */
                    false, /* Absulute arc4 pointer */
                    false, /* Allow reuse cached context */
                    false, /* Allow postponed reuse */
                    false, /* zero length result */
                    (EIP97_GLOBAL_EIP96_TIMEOUT_CNTR_FLAG == 0) ? false : true,
                    (EIP97_GLOBAL_EIP96_EXTENDED_ERRORS_ENABLE == 0) ? false : true
                    );
            EIP96_TOKEN_CTRL2_WR(
                    device,
                    i,
                    true,
                    true,
                    false,
                    EIP97_GLOBAL_EIP96_CTX_DONE_PULSE);
            EIP96_OUT_BUF_CTRL_WR(
                    device,
                    i,
                    EIP97_GLOBAL_EIP96_PE_HOLD_OUTPUT_DATA,
                    (EIP97_GLOBAL_EIP96_BLOCK_UPDATE_APPEND == 0) ? false : true,
                    (EIP97_GLOBAL_EIP96_LEN_DELTA_ENABLE == 0) ? false : true);
            EIP96_CONTEXT_CTRL_WR(
                device,
                i,
                EIP97_EIP96_CTX_SIZE,
                false,
                true);
            EIP96_CTX_NUM32_THR_WR(
                device,
                i,
                EIP97_GLOBAL_EIP96_NUM32_THR);
            EIP96_CTX_NUM64_THR_L_WR(
                device,
                i,
                EIP97_GLOBAL_EIP96_NUM64_THR_L);
            EIP96_CTX_NUM64_THR_H_WR(
                device,
                i,
                EIP97_GLOBAL_EIP96_NUM64_THR_H);
#ifdef EIP97_GLOBAL_HAVE_ECN_FIXUP
            EIP96_ECN_TABLE_WR(device, i, 0,
                               0, 0,
                               1, 0,
                               2, 0,
                               3, 0);
            EIP96_ECN_TABLE_WR(device, i, 1,
                               0, EIP96_ECN_CLE0,
                               1, 0,
                               1, 0,
                               3, EIP96_ECN_CLE1);
            EIP96_ECN_TABLE_WR(device, i, 2,
                               0, EIP96_ECN_CLE2,
                               1, EIP96_ECN_CLE3,
                               2, 0,
                               3, 0);
            EIP96_ECN_TABLE_WR(device, i, 3,
                               0, EIP96_ECN_CLE4,
                               3, 0,
                               3, 0,
                               3, 0);
#endif
        } // for

    } // EIP-202 HIA Global is configured


    return EIP97_GLOBAL_NO_ERROR;
}


/*----------------------------------------------------------------------------
 * eip97_global_reset
 */
eip97_global_error_t
eip97_global_reset(
        eip97_global_io_area_t * const io_area_p,
        const device_handle_t device)
{
    volatile eip97_true_io_area_t * const true_io_area_p = ioarea(io_area_p);
    unsigned int i;

    EIP97_GLOBAL_CHECK_POINTER(io_area_p);

    // Attempt to initialize slave byte swapping.
    if (!eip202_global_endianness_slave_configure(device))
        return EIP97_GLOBAL_UNSUPPORTED_FEATURE_ERROR;

    // Initialize the IO Area
    true_io_area_p->device = device;
    // Assume this function is called in the Unknown state but
    // this may not always be true
    true_io_area_p->state = (u32)EIP97_GLOBAL_STATE_UNKNOWN;

    if (!eip202_global_reset(device, global97_nof_p_es))
        return EIP97_GLOBAL_UNSUPPORTED_FEATURE_ERROR;

    for (i = 0; i < global97_nof_p_es; i++)
    {
        // Restore the EIP-206 default configuration
        EIP206_IN_DBUF_THRESH_DEFAULT_WR(device, i);
        EIP206_IN_TBUF_THRESH_DEFAULT_WR(device, i);
        EIP206_OUT_DBUF_THRESH_DEFAULT_WR(device, i);
        EIP206_OUT_TBUF_THRESH_DEFAULT_WR(device, i);

        // Restore the EIP-96 default configuration
        EIP96_TOKEN_CTRL_STAT_DEFAULT_WR(device, i);
    }

    // Check if Global SW Reset is done
    for (i = 0; i < global97_nof_p_es; i++)
    {
        eip97_global_error_t eip97_rc;

        eip97_rc =
            eip97_lib_reset_is_done(device, &true_io_area_p->state, i);

        if (eip97_rc != EIP97_GLOBAL_NO_ERROR)
            return eip97_rc;
    }

    return EIP97_GLOBAL_NO_ERROR;
}


/*----------------------------------------------------------------------------
 * eip97_global_reset_is_done
 */
eip97_global_error_t
eip97_global_reset_is_done(
        eip97_global_io_area_t * const io_area_p)
{
    device_handle_t device;
    volatile eip97_true_io_area_t * const true_io_area_p = ioarea(io_area_p);
    unsigned int i;

    EIP97_GLOBAL_CHECK_POINTER(io_area_p);

    device = true_io_area_p->device;

    // Check if Global SW Reset is done
    for (i = 0; i < global97_nof_p_es; i++)
    {
        eip97_global_error_t eip97_rc;

        eip97_rc =
            eip97_lib_reset_is_done(device, &true_io_area_p->state, i);

        if (eip97_rc != EIP97_GLOBAL_NO_ERROR)
            return eip97_rc;
    }

    return EIP97_GLOBAL_NO_ERROR;
}


/*----------------------------------------------------------------------------
 * eip97_global_hw_revision_get
 */
eip97_global_error_t
eip97_global_hw_revision_get(
        eip97_global_io_area_t * const io_area_p,
        eip97_global_capabilities_t * const capabilities_p)
{
    device_handle_t device;
    volatile eip97_true_io_area_t * const true_io_area_p = ioarea(io_area_p);

    EIP97_GLOBAL_CHECK_POINTER(io_area_p);
    EIP97_GLOBAL_CHECK_POINTER(capabilities_p);

    device = true_io_area_p->device;

    eip202_lib_hw_revision_get(device,
                             &capabilities_p->eip202_options,
                             &capabilities_p->eip202_options2,
                             &capabilities_p->eip202_version);

    eip96_lib_hw_revision_get(device,
                            PE_DEFAULT_NR,
                            &capabilities_p->eip96_options,
                            &capabilities_p->eip96_version);

    eip97_lib_hw_revision_get(device,
                            &capabilities_p->eip97_options,
                            &capabilities_p->eip97_version);

    return EIP97_GLOBAL_NO_ERROR;
}


/*----------------------------------------------------------------------------
 * eip97_global_configure
 */
eip97_global_error_t
eip97_global_configure(
        eip97_global_io_area_t * const io_area_p,
        const unsigned int pe_number,
        const eip97_global_ring_pe_map_t * const ring_pe_map_p)
{
    device_handle_t device;
    volatile eip97_true_io_area_t * const true_io_area_p = ioarea(io_area_p);
    eip202_global_ring_pe_map_t eip202_ring_pe_map;

    EIP97_GLOBAL_CHECK_POINTER(io_area_p);
    EIP97_GLOBAL_CHECK_POINTER(ring_pe_map_p);

    if(pe_number >= global97_nof_p_es)
        return EIP97_GLOBAL_ARGUMENT_ERROR;

    device = true_io_area_p->device;

    // Figure out which rings must be assigned to
    // DFE and DSE threads for pe_number
    eip202_ring_pe_map.ring_pe_mask = (u16)(ring_pe_map_p->ring_pe_mask &
                                             ((1 << (global97_nof_rings + global97_nof_la + global97_nof_in))-1));
    eip202_ring_pe_map.ring_prio_mask = ring_pe_map_p->ring_prio_mask;
    eip202_ring_pe_map.ring_slots0 = ring_pe_map_p->ring_slots0;
    eip202_ring_pe_map.ring_slots1 = ring_pe_map_p->ring_slots1;

#ifdef EIP97_GLOBAL_DEBUG_FSM
    {
        eip97_global_error_t rv;
        eip97_global_state_t new_state;

        // Transit to a new state
        if(eip202_ring_pe_map.ring_pe_mask != 0)
            new_state = EIP97_GLOBAL_STATE_ENABLED;
        else
            // Engines without rings not allowed!
            new_state = EIP97_GLOBAL_STATE_INITIALIZED;

        rv = eip97_global_state_set(
                (volatile eip97_global_state_t*)&true_io_area_p->state, new_state);
        if(rv != EIP97_GLOBAL_NO_ERROR)
            return EIP97_GLOBAL_ILLEGAL_IN_STATE;
    }
#endif // EIP97_GLOBAL_DEBUG_FSM

    eip202_global_configure(device, pe_number, &eip202_ring_pe_map);

    return EIP97_GLOBAL_NO_ERROR;
}


/*----------------------------------------------------------------------------
 * eip97_interfaces_get
 */
void
eip97_interfaces_get(
    unsigned int * const nof_p_es_p,
    unsigned int * const nof_rings_p,
    unsigned int * const nof_la_p,
    unsigned int * const nof_in_p)
{
    if (nof_p_es_p)
        *nof_p_es_p = global97_nof_p_es;
    if (nof_rings_p)
        *nof_rings_p = global97_nof_rings;
    if (nof_la_p)
        *nof_la_p = global97_nof_la;
    if (nof_in_p)
        *nof_in_p = global97_nof_in;
}

/*----------------------------------------------------------------------------
 * eip97_dfedse_offset_get
 */
unsigned int
eip97_dfedse_offset_get(void)
{
    return global97_dfedse_offset;
}


unsigned int
eip97_supported_funcs_get(void)
{
    return global97_supported_funcs;
}


/* end of file eip97_global_init.c */
