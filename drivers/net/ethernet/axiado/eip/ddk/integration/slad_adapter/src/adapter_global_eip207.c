/* adapter_global_eip207.c
 *
 * Security-IP-207 Global Control Adapter
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

/*----------------------------------------------------------------------------
 * This module implements (provides) the following interface(s):
 */

// Classification (EIP-207) Global Control Initialization API
#include "api_global_eip207.h"

// Classification (EIP-207) Global Control status API
#include "api_global_status_eip207.h"


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */
#include "c_adapter_cs.h"
#include "c_adapter_global.h"   // ADAPTER_CS_GLOBAL_REDIR_RING

#ifndef GLOBALCONTROL_BUILD
#include "adapter_rc_eip207.h"  // Record Cache EIP-207 interface to pass
                                // config params from Global Control
#endif

// Global Control API
#include "api_global_eip97.h"

// Driver Framework Basic Definitions API
#include "basic_defs.h"          // u8, u32, bool

#include <linux/compiler.h>

// Driver Framework C Library API
#include "clib.h"                // memcpy, zeroinit

// EIP-207 Driver Library Global Control API
#include "eip207_global_init.h"  // Init/Uninit/status/FW download

// EIP-207 Driver Library Global Control API: configuration
#include "eip207_global_config.h" // EIP207_Global_MetaData_Configure

#include "device_types.h"        // device_handle_t
#include "device_mgmt.h"         // Device_find

// Logging API
#include "log.h"                 // Log_*, LOG_*

// Firmware load API.
#include "adapter_firmware.h"
#include "firmware_eip207_api_dwld.h"

// Runtime power Management device Macros API
#include "rpm_device_macros.h"  // RPM_*

// EIP97_Supported_Funcs_Get()
#include "eip97_global_init.h"


/*----------------------------------------------------------------------------
 * Definitions and macros
 */

/* Support legacy firmware packages without name parameters in download API */
#ifndef FIRMWARE_EIP207_IPUE_NAME
#define FIRMWARE_EIP207_IPUE_NAME "firmware_eip207_ipue.bin"
#define FIRMWARE_EIP207_IFPP_NAME "firmware_eip207_ifpp.bin"
#endif
#ifndef FIRMWARE_EIP207_OPUE_NAME
#define FIRMWARE_EIP207_OPUE_NAME "firmware_eip207_opue.bin"
#define FIRMWARE_EIP207_OFPP_NAME "firmware_eip207_ofpp.bin"
#endif

/*----------------------------------------------------------------------------
 * Local variables
 */

static eip207_global_io_area_t global_io_area;
static bool global_is_initialized;

// Cached values during initialization will be used for RPM device resume
static eip207_global_cache_config_t rc_conf;
static eip207_global_flue_config_t flue_conf;

static const  global_control207_capabilities_t global_capabilities_string =
{
    "EIP-207 v_._p_  #cache sets=__ #lookup tables=__" // sz_text_description
};


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
 * global_control207_lib_init
 *
 */
static int
global_control207_lib_init(void)
{
    eip207_global_error_t rc;
    unsigned int i;

    LOG_INFO("\n\t\t\t\t eip207_global_init \n");

    rc = eip207_global_init(&global_io_area,
                            eip_device_find(ADAPTER_CS_GLOBAL_DEVICE_NAME),
                            &rc_conf,
                            &flue_conf);

    for (i = 0; i < EIP207_GLOBAL_MAX_NOF_CACHE_SETS_TO_USE; i++)
    {
        log_formatted_message("GlobalControl_EIP207_Init cache set %d:\n"
                             "\t\tFRC  AdminWords=%5d DataWords=%5d\n"
                             "\t\tTRC  AdminWords=%5d DataWords=%5d\n"
                             "\t\tARC4 AdminWords=%5d DataWords=%5d\n",
                             i,
                             rc_conf.frc[i].admin_word_count,
                             rc_conf.frc[i].data_word_count,
                             rc_conf.trc[i].admin_word_count,
                             rc_conf.trc[i].data_word_count,
                             rc_conf.arc4[i].admin_word_count,
                             rc_conf.arc4[i].data_word_count);
    }

    if (rc != EIP207_GLOBAL_NO_ERROR)
    {
        LOG_CRIT("%s: eip207_global_init() returned error %d\n", __func__, rc);
        return -1; // error
    }

    return 0; // success
}


/*----------------------------------------------------------------------------
 * global_control207_lib_firmware_load
 *
 */
static int
global_control207_lib_firmware_load(bool f_verbose)
{
    adapter_firmware_t ipue_handle, ifpp_handle,opue_handle, ofpp_handle;
    eip207_firmware_t ipue_firmware, ifpp_firmware;
    eip207_firmware_t opue_firmware, ofpp_firmware;
    eip207_global_error_t rc;

    zeroinit(ipue_firmware);
    zeroinit(ifpp_firmware);
    zeroinit(opue_firmware);
    zeroinit(ofpp_firmware);

#ifdef FIRMWARE_EIP207_VERSION_MAJOR
    // If version numbers are provided, then fill them in, so they
    // will be checked against the actual firmware, else leave them
    // at zero.
    ipue_firmware.major = FIRMWARE_EIP207_VERSION_MAJOR;
    ipue_firmware.minor = FIRMWARE_EIP207_VERSION_MINOR;
    ipue_firmware.patch_level = FIRMWARE_EIP207_VERSION_PATCH;
    ifpp_firmware.major = FIRMWARE_EIP207_VERSION_MAJOR;
    ifpp_firmware.minor = FIRMWARE_EIP207_VERSION_MINOR;
    ifpp_firmware.patch_level = FIRMWARE_EIP207_VERSION_PATCH;
    opue_firmware.major = FIRMWARE_EIP207_VERSION_MAJOR;
    opue_firmware.minor = FIRMWARE_EIP207_VERSION_MINOR;
    opue_firmware.patch_level = FIRMWARE_EIP207_VERSION_PATCH;
    ofpp_firmware.major = FIRMWARE_EIP207_VERSION_MAJOR;
    ofpp_firmware.minor = FIRMWARE_EIP207_VERSION_MINOR;
    ofpp_firmware.patch_level = FIRMWARE_EIP207_VERSION_PATCH;
#endif
    ipue_handle = adapter_firmware_acquire(FIRMWARE_EIP207_IPUE_NAME,
                                           &ipue_firmware.image_p,
                                           &ipue_firmware.image_word_count);
    ifpp_handle = adapter_firmware_acquire(FIRMWARE_EIP207_IFPP_NAME,
                                           &ifpp_firmware.image_p,
                                           &ifpp_firmware.image_word_count);
    if ((eip97_supported_funcs_get() & BIT_14) != 0)
    {
        opue_handle = adapter_firmware_acquire(FIRMWARE_EIP207_OPUE_NAME,
                                               &opue_firmware.image_p,
                                               &opue_firmware.image_word_count);
        ofpp_handle = adapter_firmware_acquire(FIRMWARE_EIP207_OFPP_NAME,
                                               &ofpp_firmware.image_p,
                                               &ofpp_firmware.image_word_count);
    }
    else
    {
        opue_handle = adapter_firmware_null;
        ofpp_handle = adapter_firmware_null;
    }

    LOG_INFO("\n\t\t\t\t eip207_global_firmware_load \n");

    rc = eip207_global_firmware_load(&global_io_area,
                                     ADAPTER_CS_TIMER_PRESCALER,
                                     &ipue_firmware,
                                     &ifpp_firmware,
                                     &opue_firmware,
                                     &ofpp_firmware);
    if (ipue_handle != adapter_firmware_null)
        adapter_firmware_release(ipue_handle);
    if (ifpp_handle != adapter_firmware_null)
        adapter_firmware_release(ifpp_handle);
    if (opue_handle != adapter_firmware_null)
        adapter_firmware_release(opue_handle);
    if (ofpp_handle != adapter_firmware_null)
        adapter_firmware_release(ofpp_handle);
    if (rc != EIP207_GLOBAL_NO_ERROR)
    {
        LOG_CRIT("global_control207_init: "
                 "eip207_global_firmware_load() failed\n");
        return -3; // error
    }
    else if (f_verbose)
    {
        LOG_CRIT("global_control207_init: firmware "
                 "downloaded successfully\n");

        LOG_CRIT("\tIPUE firmware v%d.%d.%d, image byte count %d\n",
                 ipue_firmware.major,
                 ipue_firmware.minor,
                 ipue_firmware.patch_level,
                 (int)(ipue_firmware.image_word_count * sizeof(u32)));

        LOG_CRIT("\tIFPP firmware v%d.%d.%d, image byte count %d\n\n",
                 ifpp_firmware.major,
                     ifpp_firmware.minor,
                 ifpp_firmware.patch_level,
                 (int)(ifpp_firmware.image_word_count * sizeof(u32)));

        LOG_CRIT("\tOPUE firmware v%d.%d.%d, image byte count %d\n",
                 opue_firmware.major,
                 opue_firmware.minor,
                 opue_firmware.patch_level,
                     (int)(opue_firmware.image_word_count * sizeof(u32)));

        LOG_CRIT("\tOFPP firmware v%d.%d.%d, image byte count %d\n\n",
                 ofpp_firmware.major,
                 ofpp_firmware.minor,
                 ofpp_firmware.patch_level,
                 (int)(ofpp_firmware.image_word_count * sizeof(u32)));
    }
    return 0;
}

#ifdef ADAPTER_GLOBAL_RPM_EIP207_DEVICE_ID
/*----------------------------------------------------------------------------
 * global_control207_lib_resume
 *
 */
static int
__maybe_unused global_control207_lib_resume(void * p)
{
    IDENTIFIER_NOT_USED(p);

    if (global_control207_lib_init() != 0)
        return -1; // error

    if (global_control207_lib_firmware_load(false) != 0)
        return -2; // error

    return 0; // success
}
#endif


/*----------------------------------------------------------------------------
 * global_control207_capabilities_get
 */
void
global_control207_capabilities_get(
        global_control207_capabilities_t * const capabilities_p)
{
    u8 versions[7];

    LOG_INFO("\n\t\t\t %s \n", __func__);

    memcpy(capabilities_p, &global_capabilities_string,
           sizeof(global_capabilities_string));

    {
        eip207_global_error_t rc;
        eip207_global_capabilities_t capabilities;

        if (RPM_DEVICE_IO_START_MACRO(ADAPTER_GLOBAL_RPM_EIP207_DEVICE_ID,
                                RPM_FLAG_SYNC) != RPM_SUCCESS)
            return;

        LOG_INFO("\n\t\t\t\t eip207_global_hw_revision_get \n");

        rc = eip207_global_hw_revision_get(&global_io_area, &capabilities);

        (void)RPM_DEVICE_IO_STOP_MACRO(ADAPTER_GLOBAL_RPM_EIP207_DEVICE_ID,
                                       RPM_FLAG_ASYNC);

        if (rc != EIP207_GLOBAL_NO_ERROR)
        {
            LOG_CRIT("global_control207_capabilities_get: "
                     "eip207_global_hw_revision_get() failed\n");
            return;
        }

        // Show those capabilities not propagated to higher layer.
        LOG_CRIT("EIP-207 capabilities\n");
        LOG_CRIT("\tLookup cached:            %s\n"
                 "\tFRC combined with trc:    %s\n"
                 "\tARC4RC present:           %s\n"
                 "\tFRC combined with arc4_rc: %s\n"
                 "\tTRC combined with arc4_rc: %s\n"
                 "\tFRC clients:              %d\n"
                 "\tTRC clients:              %d\n"
                 "\tARC4RC clients:           %d\n"
                 "\tLookup clients:           %d\n\n",
                 yes_no(capabilities.eip207_options.f_lookup_cached),
                 yes_no(capabilities.eip207_options.f_combined_frc_trc),
                 yes_no(capabilities.eip207_options.f_arc4_present),
                 yes_no(capabilities.eip207_options.f_combined_frc_arc4),
                 yes_no(capabilities.eip207_options.f_combined_trc_arc4),
                 capabilities.eip207_options.nof_frc_clients,
                 capabilities.eip207_options.nof_trc_clients,
                 capabilities.eip207_options.nof_arc4_clients,
                 capabilities.eip207_options.nof_lookup_clients);

        versions[0] = capabilities.eip207_version.maj_hw_revision;
        versions[1] = capabilities.eip207_version.min_hw_revision;
        versions[2] = capabilities.eip207_version.hw_patch_level;

        versions[3] = capabilities.eip207_options.nof_cache_sets / 10;
        versions[4] = capabilities.eip207_options.nof_cache_sets % 10;

        versions[5] = capabilities.eip207_options.nof_lookup_tables / 10;
        versions[6] = capabilities.eip207_options.nof_lookup_tables % 10;
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
                    p[i] = '0' + versions[ver_index++];

                if (ver_index >= 7)
                    break;
            }

            i++;
        }
    }

    return;
}

/*----------------------------------------------------------------------------
 * global_control207_init
 */
global_control207_error_t
global_control207_init(
        const bool f_load_firmware,
        const global_control207_iv_t * const iv_p)
{
    unsigned int i;
    global_control207_error_t gc207_rc = EIP207_GLOBAL_CONTROL_ERROR_INTERNAL;
    eip207_global_error_t rc;
    device_handle_t device;
    unsigned int nof_c_es,nof_rings,nof_la_interfaces,nof_inline_interfaces;

    LOG_INFO("\n\t\t\t %s \n", __func__);

    if (global_is_initialized)
    {
        LOG_CRIT("global_control207_init: called while already initialized\n");
        return EIP207_GLOBAL_CONTROL_ERROR_BAD_USE_ORDER;
    }

    device = eip_device_find(ADAPTER_CS_GLOBAL_DEVICE_NAME);
    if (device == NULL)
    {
        LOG_CRIT("global_control207_init: Could not find device\n");
        return EIP207_GLOBAL_CONTROL_ERROR_INTERNAL;
    }

    zeroinit(rc_conf);
    zeroinit(flue_conf);

    // Record Caches initialization parameters
    for (i = 0; i < EIP207_GLOBAL_MAX_NOF_CACHE_SETS_TO_USE; i++)
    {
        rc_conf.frc[i].f_enable = (ADAPTER_CS_FRC_ENABLED != 0);
        rc_conf.frc[i].f_non_block = false;
        rc_conf.frc[i].block_clock_count = ADAPTER_CS_RC_BLOCK_CLOCK_COUNT;
        rc_conf.frc[i].rec_base_addr.value64_lo = 0;
        rc_conf.frc[i].rec_base_addr.value64_hi = 0;

        rc_conf.trc[i].f_enable = (ADAPTER_CS_TRC_ENABLED != 0);
        rc_conf.trc[i].f_non_block = false;
        rc_conf.trc[i].block_clock_count = ADAPTER_CS_RC_BLOCK_CLOCK_COUNT;
        rc_conf.trc[i].rec_base_addr.value64_lo = 0;
        rc_conf.trc[i].rec_base_addr.value64_hi = 0;

        rc_conf.arc4[i].f_enable = (ADAPTER_CS_ARC4RC_ENABLED != 0);
        rc_conf.arc4[i].f_non_block = false;
        rc_conf.arc4[i].block_clock_count = ADAPTER_CS_RC_BLOCK_CLOCK_COUNT;
        rc_conf.arc4[i].rec_base_addr.value64_lo = 0;
        rc_conf.arc4[i].rec_base_addr.value64_hi = 0;
    }

    // Flow Look-Up Engine initialization parameters
    flue_conf.cache_chain = ADAPTER_CS_FLUE_CACHE_CHAIN;
    flue_conf.f_delay_mem_xs = (ADAPTER_CS_FLUE_MEMXS_DELAY != 0);
    flue_conf.iv.iv_word32[0] = iv_p->iv[0];
    flue_conf.iv.iv_word32[1] = iv_p->iv[1];
    flue_conf.iv.iv_word32[2] = iv_p->iv[2];
    flue_conf.iv.iv_word32[3] = iv_p->iv[3];

    // Hash table initialization parameters
    flue_conf.hash_tables_count = ADAPTER_CS_MAX_NOF_FLOW_HASH_TABLES_TO_USE;
    for (i = 0; i < flue_conf.hash_tables_count; i++)
    {
        flue_conf.hash_table[i].f_lookup_cached =
                                    (ADAPTER_CS_FLUE_LOOKUP_CACHED != 0);
        flue_conf.hash_table[i].f_prefetch_xform =
                                    (ADAPTER_CS_FLUE_PREFETCH_XFORM != 0);
        flue_conf.hash_table[i].f_prefetch_arc4_state =
                                    (ADAPTER_CS_FLUE_PREFETCH_ARC4 != 0);
    }

    global_control97_interfaces_get(&nof_c_es, &nof_rings, &nof_la_interfaces, &nof_inline_interfaces);
    LOG_CRIT("GlobalControl_EIP207_Init:\n"
             "number of Rings: %u, LA Interfaces: %u, Inline interfaces: %u\n",
             nof_rings,nof_la_interfaces,nof_inline_interfaces);

    flue_conf.interfaces_count = nof_rings + nof_la_interfaces +
                                                    nof_inline_interfaces;
    if (flue_conf.interfaces_count == 0)
    {
        LOG_CRIT("global_control207_init: device not initialized\n");
        return EIP207_GLOBAL_CONTROL_ERROR_INTERNAL;
    }

    for (i = 0; i < flue_conf.interfaces_count; i++)
        flue_conf.interface_index[i] = 0;

    if (RPM_DEVICE_INIT_START_MACRO(ADAPTER_GLOBAL_RPM_EIP207_DEVICE_ID,
                                    NULL, // Suspend callback not used
                                    global_control207_lib_resume) != RPM_SUCCESS)
        return EIP207_GLOBAL_CONTROL_ERROR_INTERNAL;

    if (global_control207_lib_init() != 0)
        goto exit; // error

    // Configure the Record Cache functionality at the Ring Control
    {
        eip207_global_capabilities_t capabilities;

        LOG_INFO("\n\t\t\t\t eip207_global_hw_revision_get \n");

        rc = eip207_global_hw_revision_get(&global_io_area, &capabilities);
        if (rc != EIP207_GLOBAL_NO_ERROR)
        {
            LOG_CRIT("global_control207_init: "
                     "eip207_global_hw_revision_get() failed\n");
            goto exit; // error
        }

#ifndef GLOBALCONTROL_BUILD
#ifdef ADAPTER_CS_RC_SUPPORT
        adapter_rc_eip207_configure(
                (ADAPTER_CS_TRC_ENABLED != 0),
                (ADAPTER_CS_ARC4RC_ENABLED != 0) &&
                                 capabilities.eip207_options.f_arc4_present,
                capabilities.eip207_options.f_combined_trc_arc4);
#endif // ADAPTER_CS_RC_SUPPORT
#endif // GLOBALCONTROL_BUILD
    }

    if (f_load_firmware)
    {
        if (global_control207_lib_firmware_load(true) != 0)
            goto exit;
    }

    {
        eip207_firmware_config_t fw_config;
        zeroinit(fw_config);
#if defined(ADAPTER_CS_GLOBAL_IOTOKEN_METADATA_ENABLE) || \
    defined(ADAPTER_CS_GLOBAL_CFH_ENABLE)
        fw_config.f_token_extensions_enable = true;
#else
        fw_config.f_token_extensions_enable = false;
#endif
#ifdef ADAPTER_CS_GLOBAL_INCREMENT_PKTID
        fw_config.f_increment_pkt_id = true;
#else
        fw_config.f_increment_pkt_id = false;
#endif
#ifdef ADAPTER_CS_GLOBAL_ECN_CONTROL
        fw_config.ecn_control = ADAPTER_CS_GLOBAL_ECN_CONTROL;
#endif
#ifdef ADAPTER_CS_GLOBAL_DTLS_DEFER_CCS
        fw_config.f_dtls_defer_ccs = true;
#endif
#ifdef ADAPTER_CS_GLOBAL_DTLS_DEFER_ALERT
        fw_config.f_dtls_defer_alert = true;
#endif
#ifdef ADAPTER_CS_GLOBAL_DTLS_DEFER_HANDSHAKE
        fw_config.f_dtls_defer_handshake = true;
#endif
#ifdef ADAPTER_CS_GLOBAL_DTLS_DEFER_APPDATA
        fw_config.f_dtls_defer_app_data = true;
#endif
#ifdef ADAPTER_CS_GLOBAL_DTLS_DEFER_CAPWAP
        fw_config.f_dtls_defer_capwap = true;
#endif
#ifdef ADAPTER_CS_GLOBAL_DTLS_HDR_ALIGN
        fw_config.dtls_record_header_align = ADAPTER_CS_GLOBAL_DTLS_HDR_ALIGN;
#endif
/* This is for ice-redirection enable for Transform records. eip_ring_cfg(i) doesn't need to be called. */
        fw_config.transform_redir_enable = 0xfffe; //exclude r-0(this is right thing to do..)
       //fw_config.transform_redir_enable = 0xffff; //this works but why do what isn't needed

#ifdef ADAPTER_CS_GLOBAL_REDIR_RING
        fw_config.f_redir_ring_enable = true;
        fw_config.redir_ring = ADAPTER_CS_GLOBAL_REDIR_RING;
//TODO also take help from rambus. Ask what could be over-writing default redirect-setting that is
//done via Firmware_COnfigure(). Could it be ownership-words and shit
//TODO: also ask rambus, why so many error-message (or analyze yourself).. on PEC_packet_Get() on ring-0, its
//though, just interrupts landing on r-0.
#endif
        // Configure the EIP-207 Firmware meta-data or CFH presence
        // in the ice scratch-path RAM
        for (i = 0; i < nof_c_es; i++)
        {
            eip207_global_firmware_configure(device, i, &fw_config);
            if (fw_config.f_increment_pkt_id)
            {   /* Give each engine its own range of pkt_id values */
                fw_config.pkt_id += 4096;
            }
        }
    }
    global_is_initialized = true;
    gc207_rc = EIP207_GLOBAL_CONTROL_NO_ERROR; // success

exit:
    (void)RPM_DEVICE_INIT_STOP_MACRO(ADAPTER_GLOBAL_RPM_EIP207_DEVICE_ID);

    return gc207_rc;
}


/*----------------------------------------------------------------------------
 * global_control207_uninit
 */
global_control207_error_t
global_control207_uninit(void)
{
    LOG_INFO("\n\t\t\t %s \n", __func__);

    if (!global_is_initialized)
    {
        LOG_CRIT("global_control207_uninit: called while not initialized\n");
        return EIP207_GLOBAL_CONTROL_ERROR_BAD_USE_ORDER;
    }

    (void)RPM_DEVICE_UNINIT_START_MACRO(ADAPTER_GLOBAL_RPM_EIP207_DEVICE_ID, false);
    (void)RPM_DEVICE_UNINIT_STOP_MACRO(ADAPTER_GLOBAL_RPM_EIP207_DEVICE_ID);

    global_is_initialized = false;

    return EIP207_GLOBAL_CONTROL_NO_ERROR;
}


/*----------------------------------------------------------------------------
 * global_control207_status_get
 */
global_control207_error_t
global_control207_status_get(
        const unsigned int ce_number,
        global_control207_status_t * const status_p)
{
    eip207_global_error_t rc;
    bool f_fatal_error;

    LOG_INFO("\n\t\t\t %s \n", __func__);

    if (RPM_DEVICE_IO_START_MACRO(ADAPTER_GLOBAL_RPM_EIP207_DEVICE_ID,
                            RPM_FLAG_SYNC) != RPM_SUCCESS)
        return EIP207_GLOBAL_CONTROL_ERROR_INTERNAL;

    LOG_INFO("\n\t\t\t\t eip207_global_status_get \n");

    rc = eip207_global_status_get(&global_io_area,
                                  ce_number,
                                  status_p,
                                  &f_fatal_error);

    (void)RPM_DEVICE_IO_STOP_MACRO(ADAPTER_GLOBAL_RPM_EIP207_DEVICE_ID,
                                   RPM_FLAG_ASYNC);

    if (rc == EIP207_GLOBAL_NO_ERROR)
    {
        if (f_fatal_error)
            LOG_CRIT("global_control207_status_get: Fatal error detected, "
                     "reset required!\n");

        return EIP207_GLOBAL_CONTROL_NO_ERROR;
    }
    else if (rc == EIP207_GLOBAL_ARGUMENT_ERROR)
        return EIP207_GLOBAL_CONTROL_ERROR_BAD_PARAMETER;
    else
        return EIP207_GLOBAL_CONTROL_ERROR_INTERNAL;
}


/*----------------------------------------------------------------------------
 * global_control207_global_stats_get
 */
global_control207_error_t
global_control207_global_stats_get(
        const unsigned int ce_number,
        global_control207_global_stats_t * const global_stats_p)
{
    eip207_global_error_t rc;

    LOG_INFO("\n\t\t\t %s \n", __func__);

    if (RPM_DEVICE_IO_START_MACRO(ADAPTER_GLOBAL_RPM_EIP207_DEVICE_ID,
                            RPM_FLAG_SYNC) != RPM_SUCCESS)
        return EIP207_GLOBAL_CONTROL_ERROR_INTERNAL;

    LOG_INFO("\n\t\t\t\t eip207_global_global_stats_get \n");

    rc = eip207_global_global_stats_get(&global_io_area,
                                       ce_number,
                                       global_stats_p);

    (void)RPM_DEVICE_IO_STOP_MACRO(ADAPTER_GLOBAL_RPM_EIP207_DEVICE_ID,
                                   RPM_FLAG_ASYNC);

    if (rc == EIP207_GLOBAL_NO_ERROR)
        return EIP207_GLOBAL_CONTROL_NO_ERROR;
    else if (rc == EIP207_GLOBAL_ARGUMENT_ERROR)
        return EIP207_GLOBAL_CONTROL_ERROR_BAD_PARAMETER;
    else
        return EIP207_GLOBAL_CONTROL_ERROR_INTERNAL;
}


/*----------------------------------------------------------------------------
 * global_control207_clock_count_get
 */
global_control207_error_t
global_control207_clock_count_get(
        const unsigned int ce_number,
        global_control207_clock_t * const clock_p)
{
    eip207_global_error_t rc;

    LOG_INFO("\n\t\t\t %s \n", __func__);

    if (RPM_DEVICE_IO_START_MACRO(ADAPTER_GLOBAL_RPM_EIP207_DEVICE_ID,
                            RPM_FLAG_SYNC) != RPM_SUCCESS)
        return EIP207_GLOBAL_CONTROL_ERROR_INTERNAL;

    LOG_INFO("\n\t\t\t\t eip207_global_clock_count_get \n");

    rc = eip207_global_clock_count_get(&global_io_area,
                                      ce_number,
                                      clock_p);

    (void)RPM_DEVICE_IO_STOP_MACRO(ADAPTER_GLOBAL_RPM_EIP207_DEVICE_ID,
                                   RPM_FLAG_ASYNC);

    if (rc == EIP207_GLOBAL_NO_ERROR)
        return EIP207_GLOBAL_CONTROL_NO_ERROR;
    else if (rc == EIP207_GLOBAL_ARGUMENT_ERROR)
        return EIP207_GLOBAL_CONTROL_ERROR_BAD_PARAMETER;
    else
        return EIP207_GLOBAL_CONTROL_ERROR_INTERNAL;
}


/*--------------------- -------------------------------------------------------
 * global_control207_firmware_configure
 */
global_control207_error_t
global_control207_firmware_configure(
        global_control_firmware_config_t * const fw_config_p)
{
    unsigned i;
    device_handle_t device;
    unsigned int nof_c_es;
    device = eip_device_find(ADAPTER_CS_GLOBAL_DEVICE_NAME);
    global_control97_interfaces_get(&nof_c_es, NULL, NULL, NULL);
    if (fw_config_p == NULL)
        return EIP207_GLOBAL_CONTROL_ERROR_BAD_PARAMETER;
    fw_config_p->pkt_id = 0;
    // Configure the EIP-207 Firmware meta-data or CFH presence
    // in the ice scratch-path RAM
    for (i = 0; i < nof_c_es; i++)
    {
        eip207_global_firmware_configure(device, i, fw_config_p);
        if (fw_config_p->f_increment_pkt_id)
        {   /* Give each engine its own range of pkt_id values */
            fw_config_p->pkt_id += 4096;
        }
    }

    return EIP207_GLOBAL_CONTROL_NO_ERROR;

}


/* end of file adapter_global_eip207.c */
