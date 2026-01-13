/* adapter_global_eip97.c
 *
 * Security-IP-97 Global Control Adapter
 */

/*****************************************************************************
* Copyright (c) 2011-2023 by Rambus, Inc. and/or its subsidiaries.
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
#include "api_global_eip97.h"
#include "adapter_global_internal.h"

/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Default Adapter configuration
#include "c_adapter_global.h"

#ifndef GLOBALCONTROL_BUILD
#include "adapter_ring_eip202.h" // Ring EIP-202 interface to pass
                                 // config params from Global Control
#endif

// Driver Framework Basic Definitions API
#include "basic_defs.h"         // u8, u32, bool, IDENTIFIER_NOT_USED

// Driver Framework C Library API
#include "clib.h"               // memcpy, zeroinit

// EIP-97 Driver Library Global Control API
#include "eip97_global_event.h" // Event Management
#include "eip97_global_init.h"  // Init/Uninit
#include "eip97_global_prng.h"  // PRNG Control

#include "device_types.h"       // device_handle_t
#include "device_mgmt.h"        // Device_find

// Logging API
#include "log.h"                // Log_*, LOG_*

#ifdef GLOBALCONTROL_BUILD
#include "shdevxs_init.h"       // SHDevXS_Global_init()
#endif

// Runtime power Management device Macros API
#include "rpm_device_macros.h"  // RPM_*


/*----------------------------------------------------------------------------
 * Definitions and macros
 */

typedef struct
{
    bool f_cached;

    global_control97_ring_pe_map_t ring_pe_map;

} global_control97_ring_pe_map_cache_t;

typedef struct
{
    bool f_cached;

    global_control97_prng_reseed_t   reseed_data;

} global_control97_prng_cache_t;


/*----------------------------------------------------------------------------
 * Local variables
 */
static eip97_global_io_area_t global_io_area;
static bool global_is_initialized;
static bool global_prng_present;

static const  global_control97_capabilities_t global_capabilities_string =
{
  "EIP-97 v_._p_  with EIP-202 v_._p_ and EIP-96 v_._p_, "
  "#PE=__ #rings=__ central-prng=_"// sz_text_description
};

static unsigned int global_control97_nof_p_es;
static unsigned int global_control97_nof_rings;
static unsigned int global_control97_nof_la_interfaces;
static unsigned int global_control97_nof_inline_interfaces;

// Cached values for RPM resume callback
static global_control97_ring_pe_map_cache_t global_control97_ring_pe_map [ADAPTER_GLOBAL_EIP97_NOF_PES];
static global_control97_prng_cache_t global_control97_prng [ADAPTER_GLOBAL_EIP97_NOF_PES];


/*----------------------------------------------------------------------------
 * yes_no
 */
static const char *
yes_no(
        const bool b)
{
    if (b)
        return "Yes";
    else
        return "No";
}


/*----------------------------------------------------------------------------
 * global_control97_lib_resume
 *
 */
static int
global_control97_lib_resume(void * p)
{
    unsigned int i;
    eip97_global_error_t rc;
    eip97_global_capabilities_t capabilities;

    IDENTIFIER_NOT_USED(p);

    LOG_INFO("\n\t\t\t\t eip97_global_init \n");

    rc = eip97_global_init(&global_io_area,
                           eip_device_find(ADAPTER_GLOBAL_DEVICE_NAME));
    if (rc != EIP97_GLOBAL_NO_ERROR)
    {
        LOG_CRIT("%s: eip97_global_init() returned error %d\n", __func__, rc);
        return -1; // error
    }

    rc = eip97_global_hw_revision_get(&global_io_area, &capabilities);
    if (rc != EIP97_GLOBAL_NO_ERROR)
    {
        LOG_CRIT("%s: eip97_global_hw_revision_get() returned error %d\n", __func__, rc);
        return -1; // error
    }
    global_control97_nof_p_es = MIN(capabilities.eip202_options.nof_pes, ADAPTER_GLOBAL_EIP97_NOF_PES);
    for (i = 0; i < global_control97_nof_p_es; i++)
    {
        if (global_control97_ring_pe_map[i].f_cached)
        {
            LOG_INFO("\n\t\t\t\t eip97_global_configure \n");

            rc = eip97_global_configure(&global_io_area,
                                        i,
                                        &global_control97_ring_pe_map[i].ring_pe_map);
            if (rc != EIP97_GLOBAL_NO_ERROR)
            {
                LOG_CRIT("%s: eip97_global_configure() error %d for PE %d\n",
                         __func__,
                         rc,
                         i);
                return -2; // error
            }
        }

        if (global_control97_prng[i].f_cached && global_prng_present)
        {
            LOG_INFO("\n\t\t\t\t eip97_global_prng_reseed \n");

            rc = eip97_global_prng_reseed(&global_io_area,
                                          i,
                                          &global_control97_prng[i].reseed_data);
            if (rc != EIP97_GLOBAL_NO_ERROR)
            {
                LOG_CRIT("%s: eip97_global_prng_reseed() error %d for PE %d\n",
                         __func__,
                         rc,
                         i);
                return -3; // error
            }
        }
    } // for

    return 0; // success
}


/*----------------------------------------------------------------------------
 * global_control97_capabilities_get
 */
global_control97_error_t
global_control97_capabilities_get(
        global_control97_capabilities_t * const capabilities_p)
{
    u8 versions[14];

    LOG_INFO("\n\t\t\t global_control97_capabilities_get \n");

    memcpy(capabilities_p, &global_capabilities_string,
           sizeof(global_capabilities_string));

    {
        eip97_global_error_t rc;
        eip97_global_capabilities_t capabilities;

        zeroinit(capabilities);

        if (RPM_DEVICE_IO_START_MACRO(ADAPTER_GLOBAL_RPM_EIP97_DEVICE_ID,
                                RPM_FLAG_SYNC) != RPM_SUCCESS)
            return GLOBAL_CONTROL_ERROR_INTERNAL;

        LOG_INFO("\n\t\t\t\t eip97_global_hw_revision_get \n");

        rc = eip97_global_hw_revision_get(&global_io_area, &capabilities);

        (void)RPM_DEVICE_IO_STOP_MACRO(ADAPTER_GLOBAL_RPM_EIP97_DEVICE_ID,
                                       RPM_FLAG_ASYNC);

        if (rc != EIP97_GLOBAL_NO_ERROR)
        {
            LOG_CRIT("global_control97_capabilities_get: returned error");
            return GLOBAL_CONTROL_ERROR_INTERNAL;
        }

        // Show those capabilities not propagated to higher layer.
        LOG_CRIT("global_control97_capabilities_get\n");
        LOG_CRIT("EIP202: PEs=%d rings=%d 64-bit=%s, fill level extension=%s\n"
                 "CF size=%d RF size=%d DMA len = %d "
                 "Align=%d hdw=%d host_ifc=%d\n",
                 capabilities.eip202_options.nof_pes,
                 capabilities.eip202_options.nof_rings,
                 yes_no(capabilities.eip202_options.f_addr64),
                 yes_no(capabilities.eip202_options.f_exp_plf),
                 capabilities.eip202_options.cf_size,
                 capabilities.eip202_options.rf_size,
                 capabilities.eip202_options.dma_len,
                 capabilities.eip202_options.tgt_align,
                 capabilities.eip202_options.hdw,
                 capabilities.eip202_options.host_ifc);
        LOG_CRIT("EIP96 options:\n"
                 "AES: %s with CFB/OFB: %s Fast: %s\n"
                 "DES: %s with CFB/OFB: %s Fast: %s\n"
                 "ARCFOUR level: %d\n"
                 "AES-XTS: %s Wireless crypto: %s\n"
                 "MD5: %s SHA1: %s Fast: %s SHA256: %s SHA512: %s\n"
                 "(X)CBC-MAC: %s Fast: %s All key sizes: %s GHASH %s\n",
                 yes_no(capabilities.eip96_options.f_aes),
                 yes_no(capabilities.eip96_options.f_ae_sfb),
                 yes_no(capabilities.eip96_options.f_ae_sspeed),
                 yes_no(capabilities.eip96_options.f_des),
                 yes_no(capabilities.eip96_options.f_de_sfb),
                 yes_no(capabilities.eip96_options.f_de_sspeed),
                 capabilities.eip96_options.arc4,
                 yes_no(capabilities.eip96_options.f_aes_xts),
                 yes_no(capabilities.eip96_options.f_wireless),
                 yes_no(capabilities.eip96_options.f_md5),
                 yes_no(capabilities.eip96_options.f_sha1),
                 yes_no(capabilities.eip96_options.f_sha1speed),
                 yes_no(capabilities.eip96_options.f_sha224_256),
                 yes_no(capabilities.eip96_options.f_sha384_512),
                 yes_no(capabilities.eip96_options.f_xcbc_mac),
                 yes_no(capabilities.eip96_options.f_cbc_ma_cspeed),
                 yes_no(capabilities.eip96_options.f_cbc_ma_ckeylens),
                 yes_no(capabilities.eip96_options.f_ghash));
        LOG_CRIT("EIP97 options: PEs=%d, In Dbuf size=%d In Tbuf size=%d,"
                 " Out Dbuf size=%d, Out Tbuf size=%d, Central PRNG: %s\n"
                 "Token Generator: %s, Transform Record Cache: %s\n",
                 capabilities.eip97_options.nof_pes,
                 capabilities.eip97_options.in_dbuf_size,
                 capabilities.eip97_options.in_tbuf_size,
                 capabilities.eip97_options.out_dbuf_size,
                 capabilities.eip97_options.out_tbuf_size,
                 yes_no(capabilities.eip97_options.central_prng),
                 yes_no(capabilities.eip97_options.tg),
                 yes_no(capabilities.eip97_options.trc));
        LOG_CRIT("EIP206 options: PE type=%d in_classifier=%d out_classifier=%d "
                 "MAC chans=%d \n"
                 "InDBuf=%dkB InTBuf=%dkB OutDBuf=%dkB OutTBuf=%dkB\n",
                 capabilities.eip206_options.pe_type,
                 capabilities.eip206_options.in_classifier,
                 capabilities.eip206_options.out_classifier,
                 capabilities.eip206_options.nof_mac_channels,
                 capabilities.eip206_options.in_dbuf_size_kb,
                 capabilities.eip206_options.in_tbuf_size_kb,
                 capabilities.eip206_options.out_dbuf_size_kb,
                 capabilities.eip206_options.out_tbuf_size_kb);

        versions[0] = capabilities.eip97_version.maj_hw_revision;
        versions[1] = capabilities.eip97_version.min_hw_revision;
        versions[2] = capabilities.eip97_version.hw_patch_level;

        versions[3] = capabilities.eip202_version.maj_hw_revision;
        versions[4] = capabilities.eip202_version.min_hw_revision;
        versions[5] = capabilities.eip202_version.hw_patch_level;

        versions[6] = capabilities.eip96_version.maj_hw_revision;
        versions[7] = capabilities.eip96_version.min_hw_revision;
        versions[8] = capabilities.eip96_version.hw_patch_level;

        versions[9]  = capabilities.eip202_options.nof_pes / 10;
        versions[10] = capabilities.eip202_options.nof_pes % 10;
        versions[11] = capabilities.eip202_options.nof_rings / 10;
        versions[12] = capabilities.eip202_options.nof_rings % 10;
        versions[13] = (u8)capabilities.eip97_options.central_prng;
    }

    {
        char * p = capabilities_p->sz_text_description;
        int ver_index = 0;
        int i = 0;

        while(p[i])
        {
            if (p[i] == '_')
            {
                if (versions[ver_index] > 9)
                    p[i] = '?';
                else
                    p[i] = '0' + versions[ver_index];

                ver_index++;
            }

            i++;
        }
    }

    return GLOBAL_CONTROL_NO_ERROR;
}


/*----------------------------------------------------------------------------
 * global_control97_init
 */
global_control97_error_t
global_control97_init(
        const bool f_hw_reset_done)
{
    eip97_global_error_t rc;
    global_control97_error_t gc97_rc = GLOBAL_CONTROL_ERROR_INTERNAL;
    device_handle_t dev;
    eip97_global_capabilities_t capabilities;

    LOG_INFO("\n\t\t\t global_control97_init \n");

    if (global_is_initialized)
    {
        LOG_CRIT("global_control97_init: called while already initialized\n");
        return GLOBAL_CONTROL_ERROR_BAD_USE_ORDER;
    }

    dev = eip_device_find(ADAPTER_GLOBAL_DEVICE_NAME);
    if (dev == NULL)
    {
        LOG_CRIT("global_control97_init: Could not find device\n");
        return GLOBAL_CONTROL_ERROR_INTERNAL;
    }

    zeroinit(global_control97_ring_pe_map);
    zeroinit(global_control97_prng);

    if (RPM_DEVICE_INIT_START_MACRO(ADAPTER_GLOBAL_RPM_EIP97_DEVICE_ID,
                                    NULL, // Suspend callback not used
                                    global_control97_lib_resume) != RPM_SUCCESS)
        return GLOBAL_CONTROL_ERROR_INTERNAL;

    if (!f_hw_reset_done)
    {
        LOG_INFO("\n\t\t\t\t eip97_global_reset \n");

        // Need to do a software reset first.
        rc = eip97_global_reset(&global_io_area, dev);
        if (rc == EIP97_GLOBAL_BUSY_RETRY_LATER)
        {
            unsigned int tries = 0;
            do {
                LOG_INFO("\n\t\t\t\t eip97_global_reset_is_done \n");

                rc = eip97_global_reset_is_done(&global_io_area);
                if (rc != EIP97_GLOBAL_NO_ERROR &&
                    rc != EIP97_GLOBAL_BUSY_RETRY_LATER)
                {
                    LOG_CRIT("global_control97_init:"
                             " error from eip97_global_reset_is_done\n");
                    goto exit; // error
                }
                tries ++;
                if (tries > ADAPTER_GLOBAL_RESET_MAX_RETRIES)
                {
                    LOG_CRIT("global_control97_init: Reset timeout\n");
                    goto exit; // error
                }
            } while (rc == EIP97_GLOBAL_BUSY_RETRY_LATER);
        }
        else if (rc != EIP97_GLOBAL_NO_ERROR)
        {
            LOG_CRIT("global_control97_init: error from eip97_global_reset\n");
            goto exit; // error
        }
    }

    zeroinit(capabilities);

    rc = eip97_global_hw_revision_get(&global_io_area, &capabilities);
    if (rc != EIP97_GLOBAL_NO_ERROR)
    {
        LOG_CRIT("global_control97_init: returned error");
        goto exit; // error
    }
    global_prng_present = !capabilities.eip97_options.central_prng;

    if (global_control97_lib_resume(NULL) != 0)
        goto exit; // error
    else
    {
        global_is_initialized = true;

        global_control97_nof_rings = capabilities.eip202_options.nof_rings;
        global_control97_nof_la_interfaces = capabilities.eip202_options2.nof_la_ifs;
        global_control97_nof_inline_interfaces = capabilities.eip202_options2.nof_in_ifs;

#ifdef GLOBALCONTROL_BUILD
        if (SHDevXS_Global_Init() != 0)
        {
            LOG_CRIT(
                "global_control97_init: SHDevXS_Global_Init() returned error");
            goto exit; // error
        }
#else
        // pass HW default configuration parameters obtained via
        // the Global Control interface to the Ring Control
        // for its automatic configuration
        adapter_ring_eip202_configure(capabilities.eip202_options.hdw,
                                      capabilities.eip202_options.cf_size,
                                      capabilities.eip202_options.rf_size,
                                      capabilities.eip202_options.nof_pes,
                                      capabilities.eip202_version.maj_hw_revision,
                                      capabilities.eip202_version.min_hw_revision,
                                      capabilities.eip202_version.hw_patch_level);
#endif

        gc97_rc = GLOBAL_CONTROL_NO_ERROR; // success
    }

exit:
    (void)RPM_DEVICE_INIT_STOP_MACRO(ADAPTER_GLOBAL_RPM_EIP97_DEVICE_ID);

    return gc97_rc;
}


/*----------------------------------------------------------------------------
 * global_control97_uninit
 */
global_control97_error_t
global_control97_uninit(void)
{
    eip97_global_error_t rc;
    global_control97_error_t gc97_rc = GLOBAL_CONTROL_ERROR_INTERNAL;
    device_handle_t dev;

    LOG_INFO("\n\t\t\t global_control97_uninit \n");

    if (!global_is_initialized)
    {
        LOG_CRIT("global_control97_uninit: called while not initialized\n");
        return GLOBAL_CONTROL_ERROR_BAD_USE_ORDER;
    }

    dev = eip_device_find(ADAPTER_GLOBAL_DEVICE_NAME);
    if (dev == NULL)
    {
        LOG_CRIT("global_control97_uninit: Could not find device\n");
        return GLOBAL_CONTROL_ERROR_INTERNAL;
    }

    if (RPM_DEVICE_UNINIT_START_MACRO(ADAPTER_GLOBAL_RPM_EIP97_DEVICE_ID,
                                      true) != RPM_SUCCESS)
        return GLOBAL_CONTROL_ERROR_INTERNAL;

    LOG_INFO("\n\t\t\t\t eip97_global_reset \n");

    rc = eip97_global_reset(&global_io_area, dev);
    if (rc == EIP97_GLOBAL_BUSY_RETRY_LATER)
    {
        unsigned int tries = 0;
        do {
            LOG_INFO("\n\t\t\t\t eip97_global_reset_is_done \n");

            rc = eip97_global_reset_is_done(&global_io_area);
            if (rc != EIP97_GLOBAL_NO_ERROR &&
                rc != EIP97_GLOBAL_BUSY_RETRY_LATER)
            {
                LOG_CRIT("global_control97_uninit:"
                         " error from eip97_global_reset_is_done\n");
                goto exit; // error
            }
                tries ++;
                if (tries > ADAPTER_GLOBAL_RESET_MAX_RETRIES)
                {
                    LOG_CRIT("global_control97_uninit: Reset timeout\n");
                    goto exit; // error
                }
        } while (rc == EIP97_GLOBAL_BUSY_RETRY_LATER);
    }
    else if (rc != EIP97_GLOBAL_NO_ERROR)
    {
        LOG_CRIT("global_control97_init: error from eip97_global_reset\n");
        goto exit; // error
    }

#ifdef GLOBALCONTROL_BUILD
    SHDevXS_Global_UnInit();
#endif

    global_is_initialized = false;

    gc97_rc = GLOBAL_CONTROL_NO_ERROR; // success

exit:
    (void)RPM_DEVICE_UNINIT_STOP_MACRO(ADAPTER_GLOBAL_RPM_EIP97_DEVICE_ID);

    return gc97_rc;
}


/*----------------------------------------------------------------------------
 * global_control97_configure
 */
global_control97_error_t
global_control97_configure(
        const unsigned int pe_number,
        const global_control97_ring_pe_map_t * const ring_pe_map_p)
{
    eip97_global_error_t rc;

    LOG_INFO("\n\t\t\t global_control97_configure \n");

    if (RPM_DEVICE_IO_START_MACRO(ADAPTER_GLOBAL_RPM_EIP97_DEVICE_ID,
                            RPM_FLAG_SYNC) != RPM_SUCCESS)
        return GLOBAL_CONTROL_ERROR_INTERNAL;

    LOG_INFO("\n\t\t\t\t eip97_global_configure \n");

    rc = eip97_global_configure(&global_io_area, pe_number, ring_pe_map_p);

    (void)RPM_DEVICE_IO_STOP_MACRO(ADAPTER_GLOBAL_RPM_EIP97_DEVICE_ID,
                                   RPM_FLAG_ASYNC);

    if (rc == EIP97_GLOBAL_NO_ERROR)
    {
        // Fall back on eip97_global_configure() for pe_number bounds check
        global_control97_ring_pe_map[pe_number].f_cached   = true;
        global_control97_ring_pe_map[pe_number].ring_pe_map = *ring_pe_map_p;
        return GLOBAL_CONTROL_NO_ERROR;
    }
    else if (rc == EIP97_GLOBAL_ARGUMENT_ERROR)
        return GLOBAL_CONTROL_ERROR_BAD_PARAMETER;
    else
        return GLOBAL_CONTROL_ERROR_INTERNAL;
}


/*----------------------------------------------------------------------------
 * global_control97_prng_reseed
 */
global_control97_error_t
global_control97_prng_reseed(
        const unsigned int pe_number,
        const global_control97_prng_reseed_t * const reseed_data_p)
{
    eip97_global_error_t rc;

    LOG_INFO("\n\t\t\t global_control97_prng_reseed \n");

    if (!global_prng_present)
    {
        LOG_CRIT("%s: PRNG device not present\n",__func__);
        return GLOBAL_CONTROL_ERROR_NOT_IMPLEMENTED;
    }

    if (RPM_DEVICE_IO_START_MACRO(ADAPTER_GLOBAL_RPM_EIP97_DEVICE_ID,
                            RPM_FLAG_SYNC) != RPM_SUCCESS)
        return GLOBAL_CONTROL_ERROR_INTERNAL;

    LOG_INFO("\n\t\t\t\t eip97_global_prng_reseed \n");

    rc = eip97_global_prng_reseed(&global_io_area, pe_number, reseed_data_p);

    (void)RPM_DEVICE_IO_STOP_MACRO(ADAPTER_GLOBAL_RPM_EIP97_DEVICE_ID,
                                   RPM_FLAG_ASYNC);

    if (rc == EIP97_GLOBAL_NO_ERROR)
    {
        global_control97_prng[pe_number].f_cached     = true;
        global_control97_prng[pe_number].reseed_data  = *reseed_data_p;
        return GLOBAL_CONTROL_NO_ERROR;
    }
    else if (rc == EIP97_GLOBAL_ARGUMENT_ERROR)
        return GLOBAL_CONTROL_ERROR_BAD_PARAMETER;
    else
        return GLOBAL_CONTROL_ERROR_INTERNAL;

}


/*----------------------------------------------------------------------------
 * global_control97_dfe_status_get
 */
global_control97_error_t
global_control97_dfe_status_get(
        const unsigned int pe_number,
        global_control97_dfe_status_t * const dfe_status_p)
{
    eip97_global_error_t rc;

    LOG_INFO("\n\t\t\t global_control97_dfe_status_get \n");

    if (RPM_DEVICE_IO_START_MACRO(ADAPTER_GLOBAL_RPM_EIP97_DEVICE_ID,
                            RPM_FLAG_SYNC) != RPM_SUCCESS)
        return GLOBAL_CONTROL_ERROR_INTERNAL;

    LOG_INFO("\n\t\t\t\t eip97_global_dfe_status_get \n");

    rc = eip97_global_dfe_status_get(&global_io_area, pe_number, dfe_status_p);

    (void)RPM_DEVICE_IO_STOP_MACRO(ADAPTER_GLOBAL_RPM_EIP97_DEVICE_ID,
                                   RPM_FLAG_ASYNC);

    if (rc == EIP97_GLOBAL_NO_ERROR)
        return GLOBAL_CONTROL_NO_ERROR;
    else if (rc == EIP97_GLOBAL_ARGUMENT_ERROR)
        return GLOBAL_CONTROL_ERROR_BAD_PARAMETER;
    else
        return GLOBAL_CONTROL_ERROR_INTERNAL;
}


/*----------------------------------------------------------------------------
 * global_control97_dse_status_get
 */
global_control97_error_t
global_control97_dse_status_get(
        const unsigned int pe_number,
        global_control97_dse_status_t * const dse_status_p)
{
    eip97_global_error_t rc;

    LOG_INFO("\n\t\t\t global_control97_dse_status_get \n");

    if (RPM_DEVICE_IO_START_MACRO(ADAPTER_GLOBAL_RPM_EIP97_DEVICE_ID,
                            RPM_FLAG_SYNC) != RPM_SUCCESS)
        return GLOBAL_CONTROL_ERROR_INTERNAL;

    LOG_INFO("\n\t\t\t\t eip97_global_dse_status_get \n");

    rc = eip97_global_dse_status_get(&global_io_area, pe_number, dse_status_p);

    (void)RPM_DEVICE_IO_STOP_MACRO(ADAPTER_GLOBAL_RPM_EIP97_DEVICE_ID, RPM_FLAG_ASYNC);

    if (rc == EIP97_GLOBAL_NO_ERROR)
        return GLOBAL_CONTROL_NO_ERROR;
    else if (rc == EIP97_GLOBAL_ARGUMENT_ERROR)
        return GLOBAL_CONTROL_ERROR_BAD_PARAMETER;
    else
        return GLOBAL_CONTROL_ERROR_INTERNAL;
}


/*----------------------------------------------------------------------------
 * global_control97_token_status_get
 */
global_control97_error_t
global_control97_token_status_get(
        const unsigned int pe_number,
        global_control97_token_status_t * const token_status_p)
{
    eip97_global_error_t rc;

    LOG_INFO("\n\t\t\t global_control97_token_status_get \n");

    if (RPM_DEVICE_IO_START_MACRO(ADAPTER_GLOBAL_RPM_EIP97_DEVICE_ID,
                            RPM_FLAG_SYNC) != RPM_SUCCESS)
        return GLOBAL_CONTROL_ERROR_INTERNAL;

    LOG_INFO("\n\t\t\t\t eip97_global_eip96_token_status_get \n");

    rc = eip97_global_eip96_token_status_get(&global_io_area,
                                             pe_number,
                                             token_status_p);

    (void)RPM_DEVICE_IO_STOP_MACRO(ADAPTER_GLOBAL_RPM_EIP97_DEVICE_ID,
                                   RPM_FLAG_ASYNC);

    if (rc == EIP97_GLOBAL_NO_ERROR)
        return GLOBAL_CONTROL_NO_ERROR;
    else if (rc == EIP97_GLOBAL_ARGUMENT_ERROR)
        return GLOBAL_CONTROL_ERROR_BAD_PARAMETER;
    else
        return GLOBAL_CONTROL_ERROR_INTERNAL;
}


/*----------------------------------------------------------------------------
 * global_control97_context_status_get
 */
global_control97_error_t
global_control97_context_status_get(
        const unsigned int pe_number,
        global_control97_context_status_t * const context_status_p)
{
    eip97_global_error_t rc;

    LOG_INFO("\n\t\t\t global_control97_context_status_get \n");

    if (RPM_DEVICE_IO_START_MACRO(ADAPTER_GLOBAL_RPM_EIP97_DEVICE_ID,
                            RPM_FLAG_SYNC) != RPM_SUCCESS)
        return GLOBAL_CONTROL_ERROR_INTERNAL;

    LOG_INFO("\n\t\t\t\t eip97_global_eip96_context_status_get \n");

    rc = eip97_global_eip96_context_status_get(&global_io_area,
                                               pe_number,
                                               context_status_p);

    (void)RPM_DEVICE_IO_STOP_MACRO(ADAPTER_GLOBAL_RPM_EIP97_DEVICE_ID,
                                   RPM_FLAG_ASYNC);

    if (rc == EIP97_GLOBAL_NO_ERROR)
        return GLOBAL_CONTROL_NO_ERROR;
    else if (rc == EIP97_GLOBAL_ARGUMENT_ERROR)
        return GLOBAL_CONTROL_ERROR_BAD_PARAMETER;
    else
        return GLOBAL_CONTROL_ERROR_INTERNAL;
}


/*----------------------------------------------------------------------------
 * global_control97_interrupt_status_get
 */
global_control97_error_t
global_control97_interrupt_status_get(
        const unsigned int pe_number,
        global_control97_interrupt_status_t * const interrupt_status_p)
{
    IDENTIFIER_NOT_USED(pe_number);

    LOG_INFO("\n\t\t\t global_control97_interrupt_status_get \n");

    // Not implemented yet, must use EIP-96 aic
    zeroinit(*interrupt_status_p);

    return GLOBAL_CONTROL_NO_ERROR;
}


/*----------------------------------------------------------------------------
 * global_control97_out_xfer_status_get
 */
global_control97_error_t
global_control97_out_xfer_status_get(
        const unsigned int pe_number,
        global_control97_output_transfer_status_t * const out_xfer_status_p)
{
    eip97_global_error_t rc;

    LOG_INFO("\n\t\t\t global_control97_out_xfer_status_get \n");

    if (RPM_DEVICE_IO_START_MACRO(ADAPTER_GLOBAL_RPM_EIP97_DEVICE_ID,
                            RPM_FLAG_SYNC) != RPM_SUCCESS)
        return GLOBAL_CONTROL_ERROR_INTERNAL;

    LOG_INFO("\n\t\t\t\t eip97_global_eip96_out_xfer_status_get \n");

    rc = eip97_global_eip96_out_xfer_status_get(&global_io_area,
                                               pe_number,
                                               out_xfer_status_p);

    (void)RPM_DEVICE_IO_STOP_MACRO(ADAPTER_GLOBAL_RPM_EIP97_DEVICE_ID,
                                   RPM_FLAG_ASYNC);

    if (rc == EIP97_GLOBAL_NO_ERROR)
        return GLOBAL_CONTROL_NO_ERROR;
    else if (rc == EIP97_GLOBAL_ARGUMENT_ERROR)
        return GLOBAL_CONTROL_ERROR_BAD_PARAMETER;
    else
        return GLOBAL_CONTROL_ERROR_INTERNAL;
}


/*----------------------------------------------------------------------------
 * global_control97_prng_status_get
 */
global_control97_error_t
global_control97_prng_status_get(
        const unsigned int pe_number,
        global_control97_prng_status_t * const prng_status_p)
{
    eip97_global_error_t rc;

    LOG_INFO("\n\t\t\t global_control97_prng_status_get \n");

    if (!global_prng_present)
    {
        return GLOBAL_CONTROL_ERROR_NOT_IMPLEMENTED;
    }

    if (RPM_DEVICE_IO_START_MACRO(ADAPTER_GLOBAL_RPM_EIP97_DEVICE_ID,
                            RPM_FLAG_SYNC) != RPM_SUCCESS)
        return GLOBAL_CONTROL_ERROR_INTERNAL;

    LOG_INFO("\n\t\t\t\t eip97_global_eip96_prng_status_get \n");

    rc = eip97_global_eip96_prng_status_get(&global_io_area,
                                            pe_number,
                                            prng_status_p);

    (void)RPM_DEVICE_IO_STOP_MACRO(ADAPTER_GLOBAL_RPM_EIP97_DEVICE_ID,
                                   RPM_FLAG_ASYNC);

    if (rc == EIP97_GLOBAL_NO_ERROR)
        return GLOBAL_CONTROL_NO_ERROR;
    else if (rc == EIP97_GLOBAL_ARGUMENT_ERROR)
        return GLOBAL_CONTROL_ERROR_BAD_PARAMETER;
    else
        return GLOBAL_CONTROL_ERROR_INTERNAL;
}

#ifdef ADAPTER_GLOBAL_DBG_STATISTICS
/*----------------------------------------------------------------------------
 * global_control97_debug_statistics_get
 */
global_control97_error_t
global_control97_debug_statistics_get(
        global_control97_debug_statistics_t * const debug_statistics_p)
{
    eip97_global_error_t rc;

    LOG_INFO("\n\t\t\t GlobalControl97_DSE_Debug_Statistics_Get \n");

    if (RPM_DEVICE_IO_START_MACRO(ADAPTER_GLOBAL_RPM_EIP97_DEVICE_ID,
                            RPM_FLAG_SYNC) != RPM_SUCCESS)
        return GLOBAL_CONTROL_ERROR_INTERNAL;

    LOG_INFO("\n\t\t\t\t EIP97_Global_Deubg_Statistics_Get \n");

    rc = eip97_global_debug_statistics_get(&global_io_area, debug_statistics_p);

    (void)RPM_DEVICE_IO_STOP_MACRO(ADAPTER_GLOBAL_RPM_EIP97_DEVICE_ID, RPM_FLAG_ASYNC);

    if (rc == EIP97_GLOBAL_NO_ERROR)
        return GLOBAL_CONTROL_NO_ERROR;
    else if (rc == EIP97_GLOBAL_ARGUMENT_ERROR)
        return GLOBAL_CONTROL_ERROR_BAD_PARAMETER;
    else
        return GLOBAL_CONTROL_ERROR_INTERNAL;
}
#endif

/*----------------------------------------------------------------------------
 * global_control97_interfaces_get
 */
void
global_control97_interfaces_get(
    unsigned int * const nof_p_es_p,
    unsigned int * const nof_rings_p,
    unsigned int * const nof_la_interfaces_p,
    unsigned int * const nof_inline_interfaces_p)
{
    if (nof_p_es_p)
        *nof_p_es_p = global_control97_nof_p_es;
    if (nof_rings_p)
        *nof_rings_p = global_control97_nof_rings;
    if (nof_la_interfaces_p)
        *nof_la_interfaces_p = global_control97_nof_la_interfaces;
    if (nof_inline_interfaces_p)
        *nof_inline_interfaces_p = global_control97_nof_inline_interfaces;
}


/* end of file adapter_global_eip97.c */
