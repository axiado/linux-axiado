/* eip207_global_init.c
 *
 * EIP-207 Global Control Driver Library
 * Initialization and status retrieval Module
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

// EIP-207 Global Control API
#include "eip207_global_init.h"


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Default configuration
#include "c_eip207_global.h"

// Driver Framework Basic Definitions API
#include "basic_defs.h"                 // u32

// Driver Framework C Run-time Library API
#include "clib.h"                       // zeroinit

// Driver Framework device API
#include "device_types.h"               // device_handle_t

// EIP-207s Record Cache (RC) internal interface
#include "eip207_rc_internal.h"

// EIP-207c Input Classification Engine (ice) interface
#include "eip207_ice.h"

// EIP-207c Output Classification Engine (oce) interface
#include "eip207_oce.h"

// EIP-207s Flow Look-Up Engine (flue) interface
#include "eip207_flue.h"

// EIP-207 Global Control Driver Library internal interfaces
#include "eip207_level0.h"              // EIP-207 Level 0 macros

// EIP-207c Firmware Classification API
#include "firmware_eip207_api_cs.h"     // Classification API: General

// EIP-207c Firmware Download API
#include "firmware_eip207_api_dwld.h"   // Classification API: FW download

// EIP97 Global init API
#include "eip97_global_init.h"

/*----------------------------------------------------------------------------
 * Definitions and macros
 */

// Maximum number of EIP-207c Classification Engines that should be used
// Should not exceed the number of engines physically available
#ifndef EIP207_GLOBAL_MAX_NOF_CE_TO_USE
#error "EIP207_GLOBAL_MAX_NOF_CE_TO_USE is not defined"
#endif

// Maximum number of flow hash tables that should be used
#ifndef EIP207_MAX_NOF_FLOW_HASH_TABLES_TO_USE
#error "EIP207_MAX_NOF_FLOW_HASH_TABLES_TO_USE is not defined"
#endif

// Default Classification Engine number
#define CE_DEFAULT_NR                                   0

#if (CE_DEFAULT_NR >= EIP207_GLOBAL_MAX_NOF_CE_TO_USE)
#error "error: CE_DEFAULT_NR must be less than EIP207_GLOBAL_MAX_NOF_CE_TO_USE"
#endif

// size of the arc4 state Record in 32-bit words
#define EIP207_CS_ARC4RC_RECORD_WORD_COUNT              64

#ifndef FIRMWARE_EIP207_CS_ARC4RC_RECORD_WORD_COUNT
#define FIRMWARE_EIP207_CS_ARC4RC_RECORD_WORD_COUNT     64
#endif

// I/O Area, used internally
typedef struct
{
    device_handle_t device;
    u32 state;
} eip207_true_io_area_t;

#define ioarea(_p) ((volatile eip207_true_io_area_t *)_p)

#ifdef EIP207_GLOBAL_STRICT_ARGS
#define EIP207_GLOBAL_CHECK_POINTER(_p) \
    if (NULL == (_p)) \
        return EIP207_GLOBAL_ARGUMENT_ERROR;
#define EIP207_GLOBAL_CHECK_INT_INRANGE(_i, _min, _max) \
    if ((_i) < (_min) || (_i) > (_max)) \
        return EIP207_GLOBAL_ARGUMENT_ERROR;
#define EIP207_GLOBAL_CHECK_INT_ATLEAST(_i, _min) \
    if ((_i) < (_min)) \
        return EIP207_GLOBAL_ARGUMENT_ERROR;
#define EIP207_GLOBAL_CHECK_INT_ATMOST(_i, _max) \
    if ((_i) > (_max)) \
        return EIP207_GLOBAL_ARGUMENT_ERROR;
#else
/* EIP207_GLOBAL_STRICT_ARGS undefined */
#define EIP207_GLOBAL_CHECK_POINTER(_p)
#define EIP207_GLOBAL_CHECK_INT_INRANGE(_i, _min, _max)
#define EIP207_GLOBAL_CHECK_INT_ATLEAST(_i, _min)
#define EIP207_GLOBAL_CHECK_INT_ATMOST(_i, _max)
#endif /*end of EIP207_GLOBAL_STRICT_ARGS */

#define TEST_SIZEOF(type, size) \
    extern int size##_must_bigger[1 - 2*((int)(sizeof(type) > size))]

// validate the size of the fake and real io_area structures
TEST_SIZEOF(eip207_true_io_area_t, EIP207_GLOBAL_IOAREA_REQUIRED_SIZE);

#ifdef EIP207_GLOBAL_DEBUG_FSM
// EIP-207 Global Control API States
typedef enum
{
    EIP207_GLOBAL_STATE_INITIALIZED  = 5,
    EIP207_GLOBAL_STATE_FATAL_ERROR  = 7,
    EIP207_GLOBAL_STATE_RC_ENABLED   = 8,
    EIP207_GLOBAL_STATE_FW_LOADED    = 9
} eip207_global_state_t;
#endif // EIP207_GLOBAL_DEBUG_FSM


/*----------------------------------------------------------------------------
 * Local variables
 */


/*----------------------------------------------------------------------------
 * eip207_lib_detect
 *
 * Checks the presence of EIP-207 hardware. Returns true when found.
 */
static bool
eip207_lib_detect(
        const device_handle_t device,
        const unsigned int c_enr)
{
    u32 value;

    IDENTIFIER_NOT_USED(c_enr);

    value = eip207_read32(device, EIP207_CS_REG_VERSION);
    if (!EIP207_CS_SIGNATURE_MATCH( value ))
        return false;

    return true;
}


/*----------------------------------------------------------------------------
 * eip207_lib_hw_revision_get
 */
static void
eip207_lib_hw_revision_get(
        const device_handle_t device,
        eip207_options_t * const options_p,
        eip207_version_t * const version_p)
{
    EIP207_CS_VERSION_RD(
                      device,
                      &version_p->eip_number,
                      &version_p->complmt_eip_number,
                      &version_p->hw_patch_level,
                      &version_p->min_hw_revision,
                      &version_p->maj_hw_revision);

    EIP207_CS_OPTIONS_RD(device,
                         &options_p->nof_lookup_tables,
                         &options_p->f_lookup_cached,
                         &options_p->nof_lookup_clients,
                         &options_p->f_combined_trc_arc4,
                         &options_p->f_combined_frc_arc4,
                         &options_p->f_arc4_present,
                         &options_p->nof_arc4_clients,
                         &options_p->f_combined_frc_trc,
                         &options_p->nof_trc_clients,
                         &options_p->nof_frc_clients,
                         &options_p->nof_cache_sets);
}


#ifdef EIP207_GLOBAL_DEBUG_FSM
/*----------------------------------------------------------------------------
 * eip207_lib_global_state_set
 *
 */
static eip207_global_error_t
eip207_lib_global_state_set(
        eip207_global_state_t * const current_state,
        const eip207_global_state_t new_state)
{
    switch(*current_state)
    {
        case EIP207_GLOBAL_STATE_INITIALIZED:
            switch(new_state)
            {
                case EIP207_GLOBAL_STATE_RC_ENABLED:
                    *current_state = new_state;
                    break;
                case EIP207_GLOBAL_STATE_FATAL_ERROR:
                    *current_state = new_state;
                    break;
                default:
                    return EIP207_GLOBAL_ILLEGAL_IN_STATE;
            }
            break;

         case EIP207_GLOBAL_STATE_RC_ENABLED:
            switch(new_state)
            {
                case EIP207_GLOBAL_STATE_FW_LOADED:
                   *current_state = new_state;
                   break;
                case EIP207_GLOBAL_STATE_FATAL_ERROR:
                    *current_state = new_state;
                    break;
                default:
                    return EIP207_GLOBAL_ILLEGAL_IN_STATE;
            }
            break;

        default:
            return EIP207_GLOBAL_ILLEGAL_IN_STATE;
    }

    return EIP207_GLOBAL_NO_ERROR;
}
#endif // EIP207_GLOBAL_DEBUG_FSM


/*----------------------------------------------------------------------------
 * eip207_global_init
 */
eip207_global_error_t
eip207_global_init(
        eip207_global_io_area_t * const io_area_p,
        const device_handle_t device,
        eip207_global_cache_config_t * const cache_conf_p,
        const eip207_global_flue_config_t * const flue_conf_p)
{
    unsigned int flue_nof_lookup_tables;
    eip207_global_capabilities_t capabilities;
    volatile eip207_true_io_area_t * const true_io_area_p = ioarea(io_area_p);
    unsigned int i;

    EIP207_GLOBAL_CHECK_POINTER(io_area_p);
    EIP207_GLOBAL_CHECK_POINTER(cache_conf_p);
    EIP207_GLOBAL_CHECK_POINTER(flue_conf_p);

    // Detect presence of EIP-207 HW hardware
    if (!eip207_lib_detect(device, CE_DEFAULT_NR))
        return EIP207_GLOBAL_UNSUPPORTED_FEATURE_ERROR;

    // Initialize the IO Area
    true_io_area_p->device = device;

#ifdef EIP207_GLOBAL_DEBUG_FSM
    true_io_area_p->state = (u32)EIP207_GLOBAL_STATE_INITIALIZED;
#endif // EIP207_GLOBAL_DEBUG_FSM

    zeroinit(capabilities);

    eip207_lib_hw_revision_get(device,
                             &capabilities.eip207_options,
                             &capabilities.eip207_version);

    flue_nof_lookup_tables = capabilities.eip207_options.nof_lookup_tables;

    // 0 hash tables has a special meaning for the EIP-207 flue HW
    if (flue_nof_lookup_tables == 0)
        flue_nof_lookup_tables = EIP207_GLOBAL_MAX_HW_NOF_FLOW_HASH_TABLES;

    // Check actual configuration HW against capabilities
    // number of configured cache sets and hash tables
    if ((capabilities.eip207_options.nof_cache_sets <
            EIP207_GLOBAL_MAX_NOF_CACHE_SETS_TO_USE) ||
        (flue_nof_lookup_tables <
            EIP207_MAX_NOF_FLOW_HASH_TABLES_TO_USE)  ||
        (flue_conf_p->hash_tables_count >
            EIP207_MAX_NOF_FLOW_HASH_TABLES_TO_USE))
        return EIP207_GLOBAL_UNSUPPORTED_FEATURE_ERROR;

    // Configure EIP-207 Classification Engine

    // Initialize Record Caches
    {
        eip207_global_error_t rv;

        // Initialize Flow Record Cache
#ifndef EIP207_GLOBAL_FRC_DISABLE
        if ((eip97_supported_funcs_get() & BIT_17) != 0)
        {
            for (i=0; i < EIP207_GLOBAL_MAX_NOF_CACHE_SETS_TO_USE; i++)
            {
                cache_conf_p->frc[i].data_word_count = EIP207_FRC_RAM_WORD_COUNT;
                cache_conf_p->frc[i].admin_word_count =
                    EIP207_FRC_ADMIN_RAM_WORD_COUNT;
            }

            rv = eip207_rc_internal_init(
                device,
                EIP207_RC_INTERNAL_NOT_COMBINED,
                EIP207_FRC_REG_BASE,
                cache_conf_p->frc,
                FIRMWARE_EIP207_CS_FRC_RECORD_WORD_COUNT);
            if (rv != EIP207_GLOBAL_NO_ERROR)
                return rv;
        }
#endif // EIP207_GLOBAL_FRC_DISABLE
        for (i=0; i < EIP207_GLOBAL_MAX_NOF_CACHE_SETS_TO_USE; i++)
        {
            cache_conf_p->trc[i].data_word_count = EIP207_TRC_RAM_WORD_COUNT;
            cache_conf_p->trc[i].admin_word_count =
                EIP207_TRC_ADMIN_RAM_WORD_COUNT;
        }

        // Initialize Transform Record Cache
        rv = eip207_rc_internal_init(
                 device,
                 capabilities.eip207_options.f_combined_frc_trc ?
                         EIP207_RC_INTERNAL_FRC_TRC_COMBINED :
                                EIP207_RC_INTERNAL_NOT_COMBINED,
                 EIP207_TRC_REG_BASE,
                 cache_conf_p->trc,
                 FIRMWARE_EIP207_CS_TRC_RECORD_WORD_COUNT);
        if (rv != EIP207_GLOBAL_NO_ERROR)
            return rv;

        // Check if arc4 Record Cache is available
        if ( capabilities.eip207_options.f_arc4_present )
        {
            for (i=0; i < EIP207_GLOBAL_MAX_NOF_CACHE_SETS_TO_USE; i++)
            {
                cache_conf_p->arc4[i].data_word_count =
                    EIP207_ARC4RC_RAM_WORD_COUNT;
                cache_conf_p->arc4[i].admin_word_count =
                    EIP207_ARC4RC_ADMIN_RAM_WORD_COUNT;
            }
            // Initialize arc4 Record Cache
            rv = eip207_rc_internal_init(
                    device,
                    capabilities.eip207_options.f_combined_frc_arc4 ?
                        EIP207_RC_INTERNAL_FRC_ARC4_COMBINED :
                    (capabilities.eip207_options.f_combined_trc_arc4 ?
                          EIP207_RC_INTERNAL_TRC_ARC4_COMBINED :
                                    EIP207_RC_INTERNAL_NOT_COMBINED),
                    EIP207_ARC4RC_REG_BASE,
                    cache_conf_p->arc4,
                    FIRMWARE_EIP207_CS_ARC4RC_RECORD_WORD_COUNT);
            if (rv != EIP207_GLOBAL_NO_ERROR)
                return rv;
        }
    }

    // Initialize Flow Hash Engine, set iv values
    EIP207_FHASH_IV_WR(device,
                       flue_conf_p->iv.iv_word32[0],
                       flue_conf_p->iv.iv_word32[1],
                       flue_conf_p->iv.iv_word32[2],
                       flue_conf_p->iv.iv_word32[3]);

    // Initialize flue Hash Tables
    {
        unsigned int i;

        for (i = 0; i < flue_conf_p->hash_tables_count; i++)
            eip207_flue_init(device,
                             i,
                             flue_conf_p,
                             capabilities.eip207_options.f_arc4_present,
                             capabilities.eip207_options.f_lookup_cached);
    }

#ifdef EIP207_GLOBAL_DEBUG_FSM
        {
            eip207_global_error_t rv;
            u32 state = true_io_area_p->state;

            // Transit to a new state
            rv = eip207_lib_global_state_set(
                    (eip207_global_state_t* const)&state,
                    EIP207_GLOBAL_STATE_RC_ENABLED);

            true_io_area_p->state = state;

            if (rv != EIP207_GLOBAL_NO_ERROR)
                return EIP207_GLOBAL_ILLEGAL_IN_STATE;
        }
#endif // EIP207_GLOBAL_DEBUG_FSM

    return EIP207_GLOBAL_NO_ERROR;
}


/*----------------------------------------------------------------------------
 * eip207_global_firmware_load
 */
eip207_global_error_t
eip207_global_firmware_load(
        eip207_global_io_area_t * const io_area_p,
        const unsigned int timer_prescaler,
        eip207_firmware_t * const ipue_firmware_p,
        eip207_firmware_t * const ifpp_firmware_p,
        eip207_firmware_t * const opue_firmware_p,
        eip207_firmware_t * const ofpp_firmware_p)
{
    eip207_global_error_t eip207_rc;
    device_handle_t device;
    volatile eip207_true_io_area_t * const true_io_area_p = ioarea(io_area_p);

    EIP207_GLOBAL_CHECK_POINTER(io_area_p);

    device = true_io_area_p->device;

    // Download EIP-207c Input Classification Engine (ice) firmware,
    // use the same images for all the instances of the engine
    eip207_rc = eip207_ice_firmware_load(device,
                                         timer_prescaler,
                                         ipue_firmware_p,
                                         ifpp_firmware_p);
    if (eip207_rc != EIP207_GLOBAL_NO_ERROR)
        return eip207_rc;

    // Download EIP-207c Output Classification Engine (oce) firmware,
    // use the same images for all the instances of the engine
    // Download Input Classification Engine (oce) firmware
    eip207_rc = eip207_oce_firmware_load(device,
                                             timer_prescaler,
                                             opue_firmware_p,
                                             ofpp_firmware_p);
        // oce is not supported by this EIP-207 HW version
    if (eip207_rc != EIP207_GLOBAL_UNSUPPORTED_FEATURE_ERROR &&
        eip207_rc != EIP207_GLOBAL_NO_ERROR) // oce is supported
        return eip207_rc; // oce FW download error, abort!

#ifdef EIP207_GLOBAL_DEBUG_FSM
        {
            eip207_global_error_t rv;
            u32 state = true_io_area_p->state;

            // Transit to a new state
            rv = eip207_lib_global_state_set(
                    (eip207_global_state_t* const)&state,
                    EIP207_GLOBAL_STATE_FW_LOADED);

            true_io_area_p->state = state;

            if (rv != EIP207_GLOBAL_NO_ERROR)
                return EIP207_GLOBAL_ILLEGAL_IN_STATE;
        }
#endif // EIP207_GLOBAL_DEBUG_FSM

    return EIP207_GLOBAL_NO_ERROR;
}


/*----------------------------------------------------------------------------
 * eip207_global_hw_revision_get
 */
eip207_global_error_t
eip207_global_hw_revision_get(
        eip207_global_io_area_t * const io_area_p,
        eip207_global_capabilities_t * const capabilities_p)
{
    device_handle_t device;
    volatile eip207_true_io_area_t * const true_io_area_p = ioarea(io_area_p);

    EIP207_GLOBAL_CHECK_POINTER(io_area_p);
    EIP207_GLOBAL_CHECK_POINTER(capabilities_p);

    device = true_io_area_p->device;

    eip207_lib_hw_revision_get(device,
                             &capabilities_p->eip207_options,
                             &capabilities_p->eip207_version);

    return EIP207_GLOBAL_NO_ERROR;
}


/*----------------------------------------------------------------------------
 * eip207_global_global_stats_get
 */
eip207_global_error_t
eip207_global_global_stats_get(
        eip207_global_io_area_t * const io_area_p,
        const unsigned int ce_number,
        eip207_global_global_stats_t * const global_stats_p)
{
    device_handle_t device;
    eip207_global_error_t rv;
    volatile eip207_true_io_area_t * const true_io_area_p = ioarea(io_area_p);

    EIP207_GLOBAL_CHECK_POINTER(io_area_p);
    EIP207_GLOBAL_CHECK_POINTER(global_stats_p);

    if(ce_number >= EIP207_GLOBAL_MAX_NOF_CE_TO_USE)
        return EIP207_GLOBAL_ARGUMENT_ERROR;

    device = true_io_area_p->device;

    rv = eip207_ice_global_stats_get(device, ce_number, &global_stats_p->ice);
    if (rv != EIP207_GLOBAL_NO_ERROR)
        return rv;

    rv = eip207_oce_global_stats_get(device, ce_number, &global_stats_p->oce);
    if (rv == EIP207_GLOBAL_UNSUPPORTED_FEATURE_ERROR)
        return EIP207_GLOBAL_NO_ERROR; // oce is not supported
    else                               // by this EIP-207 HW version
        return rv;
}


/*----------------------------------------------------------------------------
 * eip207_global_clock_count_get
 */
eip207_global_error_t
eip207_global_clock_count_get(
        eip207_global_io_area_t * const io_area_p,
        const unsigned int ce_number,
        eip207_global_clock_t * const clock_p)
{
    device_handle_t device;
    eip207_global_error_t rv;
    volatile eip207_true_io_area_t * const true_io_area_p = ioarea(io_area_p);

    EIP207_GLOBAL_CHECK_POINTER(io_area_p);
    EIP207_GLOBAL_CHECK_POINTER(clock_p);

    if(ce_number >= EIP207_GLOBAL_MAX_NOF_CE_TO_USE)
        return EIP207_GLOBAL_ARGUMENT_ERROR;

    device = true_io_area_p->device;

    rv = eip207_ice_clock_count_get(device, ce_number, &clock_p->ice);
    if (rv != EIP207_GLOBAL_NO_ERROR)
        return rv;

    rv = eip207_oce_clock_count_get(device, ce_number, &clock_p->oce);
    if (rv == EIP207_GLOBAL_UNSUPPORTED_FEATURE_ERROR)
        return EIP207_GLOBAL_NO_ERROR; // oce is not supported
    else                               // by this EIP-207 HW version
        return rv;
}


/*-----------------------------------------------------------------------------
 * eip207_global_status_get
 */
eip207_global_error_t
eip207_global_status_get(
        eip207_global_io_area_t * const io_area_p,
        const unsigned int ce_number,
        eip207_global_status_t * const status_p,
        bool * const f_fatal_error_p)
{
    unsigned int i;
    device_handle_t device;
    eip207_global_error_t rv;
    bool f_fatal_error = false;
    volatile eip207_true_io_area_t * const true_io_area_p = ioarea(io_area_p);

    EIP207_GLOBAL_CHECK_POINTER(io_area_p);
    EIP207_GLOBAL_CHECK_POINTER(status_p);
    EIP207_GLOBAL_CHECK_POINTER(f_fatal_error_p);

    if(ce_number >= EIP207_GLOBAL_MAX_NOF_CE_TO_USE)
        return EIP207_GLOBAL_ARGUMENT_ERROR;

    device = true_io_area_p->device;

    rv = eip207_ice_status_get(device, ce_number, &status_p->ice);
    if (rv != EIP207_GLOBAL_NO_ERROR)
        return rv;

    rv = eip207_oce_status_get(device, ce_number, &status_p->oce);
    if (rv != EIP207_GLOBAL_NO_ERROR &&
        rv != EIP207_GLOBAL_UNSUPPORTED_FEATURE_ERROR)
        return rv;

    eip207_flue_status_get(device, &status_p->flue);

    if (status_p->ice.f_pue_ecc_derr   ||
        status_p->ice.f_fpp_ecc_derr   ||
        status_p->oce.f_pue_ecc_derr ||
        status_p->oce.f_fpp_ecc_derr ||
        status_p->ice.f_timer_overflow ||
        status_p->oce.f_timer_overflow ||
        status_p->flue.error1 != 0   ||
        status_p->flue.error2 != 0)
        f_fatal_error = true;

    for (i = 0; i < EIP207_GLOBAL_MAX_NOF_CACHE_SETS_TO_USE; i++)
    {
        eip207_rc_internal_status_get(device,
                                      i,
                                      &status_p->frc[i],
                                      &status_p->trc[i],
                                      &status_p->arc4_rc[i]);
        if (status_p->frc[i].f_dma_read_error ||
            status_p->frc[i].f_dma_write_error ||
            status_p->frc[i].f_admin_ecc_err ||
            status_p->frc[i].f_data_ecc_oflo ||
            status_p->trc[i].f_dma_read_error ||
            status_p->trc[i].f_dma_write_error ||
            status_p->trc[i].f_admin_ecc_err ||
            status_p->trc[i].f_data_ecc_oflo ||
            status_p->arc4_rc[i].f_dma_read_error ||
            status_p->arc4_rc[i].f_dma_write_error ||
            status_p->arc4_rc[i].f_admin_ecc_err ||
            status_p->arc4_rc[i].f_data_ecc_oflo)
            f_fatal_error = true;

        eip207_rc_internal_debug_statistics_get(device,
                                               i,
                                               &status_p->frc_stats[i],
                                               &status_p->trc_stats[i]);
    }

    *f_fatal_error_p = f_fatal_error;

#ifdef EIP207_GLOBAL_DEBUG_FSM
    if (f_fatal_error)
    {
        u32 state = true_io_area_p->state;

        // Transit to a new state
        rv = eip207_lib_global_state_set(
                (eip207_global_state_t* const)&state,
                EIP207_GLOBAL_STATE_FATAL_ERROR);

        true_io_area_p->state = state;

        if (rv != EIP207_GLOBAL_NO_ERROR)
            return EIP207_GLOBAL_ILLEGAL_IN_STATE;
    }
#endif // EIP207_GLOBAL_DEBUG_FSM

    return EIP207_GLOBAL_NO_ERROR;
}


/* end of file eip207_global_init.c */
