/* eip207_oce.c
 *
 * EIP-207c Output Classification Engine (oce) interface implementation
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

// EIP-207c Output Classification Engine (oce) interface
#include "eip207_oce.h"

/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Default configuration
#include "c_eip207_global.h"

// Driver Framework Basic Definitions API
#include "basic_defs.h"                 // u32, bool

// Driver Framework C Run-Time Library Abstraction API
#include "clib.h"                 // zeroinit

// EIP-207 Global Control Driver Library internal interfaces
#include "eip207_level0.h"              // EIP-207 Level 0 macros

// Driver Framework device API
#include "device_types.h"               // device_handle_t
#include "device_rw.h"                  // Read32, Write32

// EIP-207 Firmware Classification API
#include "firmware_eip207_api_cs.h"     // Classification API: General

// EIP-207 Firmware Download API
#include "firmware_eip207_api_dwld.h"   // Classification API: FW download

// eip97_interfaces_get()
#include "eip97_global_internal.h"

// EIP97 Global init API
#include "eip97_global_init.h"

/*----------------------------------------------------------------------------
 * Definitions and macros
 */

// Allow legacy firmware to be used, which does not have these constants.
#ifndef FIRMWARE_EIP207_DWLD_ADMIN_RAM_INPUT_LAST_BYTE_COUNT
#define FIRMWARE_EIP207_DWLD_ADMIN_RAM_INPUT_LAST_BYTE_COUNT FIRMWARE_EIP207_DWLD_ADMIN_RAM_LAST_BYTE_COUNT
#endif
#ifndef FIRMWARE_EIP207_CS_ADMIN_RAM_INPUT_LAST_BYTE_COUNT
#define FIRMWARE_EIP207_CS_ADMIN_RAM_INPUT_LAST_BYTE_COUNT FIRMWARE_EIP207_DWLD_ADMIN_RAM_INPUT_LAST_BYTE_COUNT
#endif

// number fo words to check when reading firmware back.
#define EIP207_FW_CHECK_WORD_COUNT 16


/*----------------------------------------------------------------------------
 * eip207_oce_firmware_load
 */
eip207_global_error_t
eip207_oce_firmware_load(
        const device_handle_t device,
        const unsigned int timer_prescaler,
        eip207_firmware_t * const pue_firmware_p,
        eip207_firmware_t * const fpp_firmware_p)
{
    unsigned int nof_c_es;

    if ((eip97_supported_funcs_get() & BIT_14) != 0)
    {
        eip97_interfaces_get(&nof_c_es,NULL,NULL,NULL);
        // Check if the Adapter provides the OPUE firmware image with correct size.
        if (!pue_firmware_p->image_p ||
            pue_firmware_p->image_word_count > EIP207_OPUE_PROG_RAM_WORD_COUNT)
            return EIP207_GLOBAL_ARGUMENT_ERROR;

        // Check if the Adapter provides the OFPP firmware image with correct size.
        if (!fpp_firmware_p->image_p ||
            fpp_firmware_p->image_word_count > EIP207_OPUE_PROG_RAM_WORD_COUNT)
            return EIP207_GLOBAL_ARGUMENT_ERROR;


        // Clear EIP-207c oce Scratchpad RAM where the the firmware
        // administration data will be located
        {
            unsigned int i, ce_count, block_count, required_ram_byte_count;

            required_ram_byte_count =
                MAX(FIRMWARE_EIP207_CS_ADMIN_RAM_INPUT_LAST_BYTE_COUNT,
                    FIRMWARE_EIP207_DWLD_ADMIN_RAM_INPUT_LAST_BYTE_COUNT);

#if (EIP207_OCE_SCRATCH_RAM_128B_BLOCK_COUNT * 128) < FIRMWARE_EIP207_CS_ADMIN_RAM_INPUT_LAST_BYTE_COUNT || \
    (EIP207_OCE_SCRATCH_RAM_128B_BLOCK_COUNT * 128) < FIRMWARE_EIP207_DWLD_ADMIN_RAM_INPUT_LAST_BYTE_COUNT
#error "Required ADMIN RAM specified too large"
#endif

            // Calculate how many 128-byte blocks are required for the firmware
            // administration data
            block_count = (required_ram_byte_count + 127) / 128;

            for (ce_count = 0; ce_count < nof_c_es; ce_count++)
            {
                // Make oce Scratchpad RAM accessible and set the timer
                EIP207_OCE_SCRATCH_CTRL_WR(
                    device,
                    ce_count,
                    true, // Change timer
                    true, // Enable timer
                    (u16)timer_prescaler,
                    EIP207_OCE_SCRATCH_TIMER_OFLO_BIT, // Timer overflow bit
                    true, // Change access
                    (u8)block_count);

#ifdef DEBUG
                // Check if the timer runs
                {
                    u32 value32;
                    unsigned int i;

                    for (i = 0; i < 10; i++)
                    {
                        value32 = device_read32(device,
                                                EIP207_OCE_REG_TIMER_LO(ce_count));

                        if (value32 == 0)
                            return EIP207_GLOBAL_INTERNAL_ERROR;
                    }
                }
#endif

                // Write the oce Scratchpad RAM with 0
                for(i = 0; i < (block_count * 32); i++)
                {
                    device_write32(device,
                                   EIP207_OCE_REG_SCRATCH_RAM(ce_count) +
                                   i * sizeof(u32),
                                   0);
#ifdef DEBUG
                    // Perform read-back check
                    {
                        u32 value32 =
                            device_read32(
                                device,
                                EIP207_OCE_REG_SCRATCH_RAM(ce_count) +
                                i * sizeof(u32));

                        if (value32 != 0)
                            return EIP207_GLOBAL_INTERNAL_ERROR;
                    }
#endif
                }
            }

            // Leave the scratchpad RAM accessible for the Host
        }

        // Download the firmware
        {
            unsigned int ce_count;
#ifdef DEBUG
            unsigned int i;
#endif
            for (ce_count = 0; ce_count < nof_c_es; ce_count++)
            {
                // Reset the Input Flow Post-Processor micro-engine (OFPP) to make its
                // Program RAM accessible
                EIP207_OCE_FPP_CTRL_WR(device,
                                       ce_count,
                                       0,     // No start address for debug mode
                                       1,       // Clear ECC correctable error
                                       1,       // Clear ECC non-correctable error
                                       false, // Debug mode OFF
                                       true); // SW Reset ON

                // Enable access to OFPP Program RAM
                EIP207_OCE_RAM_CTRL_WR(device, ce_count, false, true);
            }

#ifdef DEBUG
            // Write the Input Flow post-Processor micro-Engine firmware
            for(i = 0; i < fpp_firmware_p->image_word_count; i++)
            {
                device_write32(device,
                               EIP207_CS_RAM_XS_SPACE_BASE + i * sizeof(u32),
                               fpp_firmware_p->image_p[i]);
                // Perform read-back check
                {
                    u32 value32 =
                        device_read32(
                            device,
                           EIP207_CS_RAM_XS_SPACE_BASE + i * sizeof(u32));

                    if (value32 != fpp_firmware_p->image_p[i])
                        return EIP207_GLOBAL_INTERNAL_ERROR;
                }
            }
#else
            device_write32_array(device,
                                EIP207_CS_RAM_XS_SPACE_BASE,
                                fpp_firmware_p->image_p,
                                fpp_firmware_p->image_word_count);
            // Perform read-back check of small subset
            {
                static u32 read_buf[EIP207_FW_CHECK_WORD_COUNT];
                device_read32_array(device,
                                   EIP207_CS_RAM_XS_SPACE_BASE,
                                   read_buf,
                                   EIP207_FW_CHECK_WORD_COUNT);
                if (memcmp(read_buf,
                           fpp_firmware_p->image_p,
                           EIP207_FW_CHECK_WORD_COUNT * sizeof(u32)) != 0)
                    return EIP207_GLOBAL_INTERNAL_ERROR;

            }
#endif

            for (ce_count = 0; ce_count < nof_c_es; ce_count++)
            {
                // Disable access to OFPP Program RAM
                // Enable access to OPUE Program RAM
                EIP207_OCE_RAM_CTRL_WR(device, ce_count, true, false);

                // Reset the Input Pull-Up micro-Engine (OPUE) to make its
                // Program RAM accessible
                EIP207_OCE_PUE_CTRL_WR(device,
                                       ce_count,
                                       0,     // No start address for debug mode
                                       1,       // Clear ECC correctable error
                                       1,       // Clear ECC non-correctable error
                                   false, // Debug mode OFF
                                       true); // SW Reset ON
            }

#ifdef DEBUG
            // Write the Input Pull-Up micro-Engine firmware
            for(i = 0; i < pue_firmware_p->image_word_count; i++)
            {
                device_write32(device,
                               EIP207_CS_RAM_XS_SPACE_BASE + i * sizeof(u32),
                               pue_firmware_p->image_p[i]);
                // Perform read-back check
                {
                    u32 value32 =
                        device_read32(
                            device,
                            EIP207_CS_RAM_XS_SPACE_BASE + i * sizeof(u32));

                    if (value32 != pue_firmware_p->image_p[i])
                        return EIP207_GLOBAL_INTERNAL_ERROR;
                }
            }
#else
            device_write32_array(device,
                                EIP207_CS_RAM_XS_SPACE_BASE,
                                pue_firmware_p->image_p,
                                pue_firmware_p->image_word_count);
            // Perform read-back check of small subset
            {
                static u32 read_buf[EIP207_FW_CHECK_WORD_COUNT];
                device_read32_array(device,
                                   EIP207_CS_RAM_XS_SPACE_BASE,
                                   read_buf,
                               EIP207_FW_CHECK_WORD_COUNT);
                if (memcmp(read_buf,
                           pue_firmware_p->image_p,
                           EIP207_FW_CHECK_WORD_COUNT * sizeof(u32)) != 0)
                    return EIP207_GLOBAL_INTERNAL_ERROR;

            }
#endif

            // Disable access to OPUE Program RAM
            for (ce_count = 0; ce_count < EIP207_GLOBAL_MAX_NOF_CE_TO_USE; ce_count++)
                EIP207_OCE_RAM_CTRL_WR(device, ce_count, false, false);

#ifdef EIP207_GLOBAL_FIRMWARE_DOWNLOAD_VERSION_CHECK
            // Check the firmware version and start all the engines
            for (ce_count = 0; ce_count < nof_c_es; ce_count++)
            {
                u32 value32;
                unsigned int ma, mi, pl, i;
                bool f_updated;

                // Start the OFPP in Debug mode for the firmware version check
                EIP207_OCE_FPP_CTRL_WR(
                    device,
                    ce_count,
                    FIRMWARE_EIP207_DWLD_OFPP_VERSION_CHECK_DBG_PROG_CNTR,
                    1,       // Clear ECC correctable error
                    1,       // Clear ECC non-correctable error
                    true,    // Debug mode ON
                    false);  // SW Reset OFF

                // Wait for the OFPP version update
                for (i = 0; i < EIP207_FW_VER_CHECK_MAX_NOF_READ_ATTEMPTS; i++)
                {
                    value32 = device_read32(
                        device,
                        EIP207_OCE_REG_SCRATCH_RAM(ce_count) +
                        FIRMWARE_EIP207_DWLD_ADMIN_RAM_OFPP_CTRL_BYTE_OFFSET);

                    firmware_eip207_dwld_ofpp_version_updated_read(value32,
                                                                  &f_updated);

                    if (f_updated)
                        break;
                }

                if (!f_updated)
                    return EIP207_GLOBAL_INTERNAL_ERROR;

                value32 = device_read32(
                    device,
                    EIP207_OCE_REG_SCRATCH_RAM(ce_count) +
                    FIRMWARE_EIP207_DWLD_ADMIN_RAM_OFPP_VERSION_BYTE_OFFSET);

                firmware_eip207_dwld_version_read(value32, &ma, &mi, &pl);

                if (fpp_firmware_p->major == 0 &&
                    fpp_firmware_p->minor == 0 &&
                    fpp_firmware_p->patch_level == 0)
                {  // Adapter did not provide expected version, return it.
                    fpp_firmware_p->major = ma;
                    fpp_firmware_p->minor = mi;
                    fpp_firmware_p->patch_level = pl;
                }
                else
                {
                    // Adapter provided expected version, check it.
                    if (fpp_firmware_p->major != ma ||
                        fpp_firmware_p->minor != mi ||
                        fpp_firmware_p->patch_level != pl)
                        return EIP207_GLOBAL_INTERNAL_ERROR;
                }

                // Start the OPUE in Debug mode for the firmware version check
                EIP207_OCE_PUE_CTRL_WR(
                    device,
                    ce_count,
                    FIRMWARE_EIP207_DWLD_OPUE_VERSION_CHECK_DBG_PROG_CNTR,
                    1,       // Clear ECC correctable error
                    1,       // Clear ECC non-correctable error
                    true,    // Debug mode ON
                    false);  // SW Reset OFF

                // Wait for the OPUE version update
                for (i = 0; i < EIP207_FW_VER_CHECK_MAX_NOF_READ_ATTEMPTS; i++)
                {
                    value32 = device_read32(
                        device,
                        EIP207_OCE_REG_SCRATCH_RAM(ce_count) +
                        FIRMWARE_EIP207_DWLD_ADMIN_RAM_OPUE_CTRL_BYTE_OFFSET);

                    firmware_eip207_dwld_opue_version_updated_read(value32,
                                                                  &f_updated);

                    if (f_updated)
                        break;
                }

                if (!f_updated)
                    return EIP207_GLOBAL_INTERNAL_ERROR;

                value32 = device_read32(
                    device,
                     EIP207_OCE_REG_SCRATCH_RAM(ce_count) +
                    FIRMWARE_EIP207_DWLD_ADMIN_RAM_OPUE_VERSION_BYTE_OFFSET);

                firmware_eip207_dwld_version_read(value32, &ma, &mi, &pl);

                if (pue_firmware_p->major == 0 &&
                    pue_firmware_p->minor == 0 &&
                pue_firmware_p->patch_level == 0)
                {  // Adapter did not provide expected version, return it.
                    pue_firmware_p->major = ma;
                    pue_firmware_p->minor = mi;
                    pue_firmware_p->patch_level = pl;
                }
                else
                {
                    // Adapter provided expected version, check it.
                    if (pue_firmware_p->major != ma ||
                    pue_firmware_p->minor != mi ||
                        pue_firmware_p->patch_level != pl)
                        return EIP207_GLOBAL_INTERNAL_ERROR;
                }
            } // for
#else
            for (ce_count = 0; ce_count < EIP207_GLOBAL_MAX_NOF_CE_TO_USE; ce_count++)
            {
            // Start the OFPP in Debug mode
                EIP207_OCE_FPP_CTRL_WR(
                    device,
                    ce_count,
                    FIRMWARE_EIP207_DWLD_OFPP_VERSION_CHECK_DBG_PROG_CNTR,
                    1,       // Clear ECC correctable error
                    1,       // Clear ECC non-correctable error
                    true,    // Debug mode ON
                    false);  // SW Reset OFF

                // Start the OPUE in Debug mode
                EIP207_OCE_PUE_CTRL_WR(
                    device,
                    ce_count,
                    FIRMWARE_EIP207_DWLD_OPUE_VERSION_CHECK_DBG_PROG_CNTR,
                    1,       // Clear ECC correctable error
                    1,       // Clear ECC non-correctable error
                            true,    // Debug mode ON
                    false);  // SW Reset OFF
            } // for
#endif // EIP207_GLOBAL_FIRMWARE_DOWNLOAD_VERSION_CHECK
        }
    }
    return EIP207_GLOBAL_NO_ERROR;
}


/*----------------------------------------------------------------------------
 * eip207_oce_global_stats_get
 */
eip207_global_error_t
eip207_oce_global_stats_get(
        const device_handle_t device,
        const unsigned int ce_number,
        eip207_global_oce_global_stats_t * const oce_global_stats_p)
{
    IDENTIFIER_NOT_USED(device);
    IDENTIFIER_NOT_USED(ce_number);

    // Not used / implemented yet
    zeroinit(*oce_global_stats_p);

    return EIP207_GLOBAL_NO_ERROR;
}


/*----------------------------------------------------------------------------
 * eip207_oce_clock_count_get
 */
eip207_global_error_t
eip207_oce_clock_count_get(
        const device_handle_t device,
        const unsigned int ce_number,
        eip207_global_value64_t * const oce_clock_p)
{
    IDENTIFIER_NOT_USED(device);
    IDENTIFIER_NOT_USED(ce_number);

    // Not used / implemented yet
    zeroinit(*oce_clock_p);

    return EIP207_GLOBAL_NO_ERROR;
}


/*-----------------------------------------------------------------------------
 * eip207_oce_status_get
 */
eip207_global_error_t
eip207_oce_status_get(
        const device_handle_t device,
        const unsigned int ce_number,
        eip207_global_ce_status_t * const oce_status_p)
{
    if ((eip97_supported_funcs_get() & BIT_14) != 0)
    {
        EIP207_OCE_PUE_CTRL_RD_CLEAR(device,
                                     ce_number,
                                     &oce_status_p->f_pue_ecc_corr,
                                     &oce_status_p->f_pue_ecc_derr);

        EIP207_OCE_FPP_CTRL_RD_CLEAR(device,
                                     ce_number,
                                     &oce_status_p->f_fpp_ecc_corr,
                                     &oce_status_p->f_fpp_ecc_derr);

        EIP207_OCE_SCRATCH_CTRL_RD(device,
                                   ce_number,
                                   &oce_status_p->f_timer_overflow);
    }
    return EIP207_GLOBAL_NO_ERROR;
}


/* end of file eip207_oce.c */
