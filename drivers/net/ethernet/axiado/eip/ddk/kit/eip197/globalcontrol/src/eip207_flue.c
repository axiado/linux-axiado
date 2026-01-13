/* eip207_ice.c
 *
 * EIP-207s Flow Look-Up Engine (flue) interface implementation
 *
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

// EIP-207s Flow Look-Up Engine (flue) interface
#include "eip207_flue.h"


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Default configuration
#include "c_eip207_global.h"

// Driver Framework Basic Definitions API
#include "basic_defs.h"                 // u32, bool

// EIP-207 Global Control Driver Library internal interfaces
#include "eip207_level0.h"              // EIP-207 Level 0 macros

// EIP-207 HW interface
#include "eip207_hw_interface.h"

// Driver Framework device API
#include "device_types.h"               // device_handle_t

// EIP-207 Firmware Classification API
#include "firmware_eip207_api_cs.h"     // Classification API: General


/*----------------------------------------------------------------------------
 * Definitions and macros
 */

#define EIP207_FLUE_3ENTRY_LOOKUP_MODE          1


/*----------------------------------------------------------------------------
 * eip207_flue_init
 */
void
eip207_flue_init(
        const device_handle_t device,
        const unsigned int hash_table_id,
        const eip207_global_flue_config_t * const flue_conf_p,
        const bool f_arc4_present,
        const bool f_lookup_cache_present)
{
    // Configure hash tables
    EIP207_FLUE_CONFIG_WR(device,
                          hash_table_id, // Hash Table ID
                          hash_table_id, // function,
                                       // set it equal to hash table id
                          0,           // generation
                          true,        // Table enable
                          true);       // Access enable

    // Use Hash table 0 parameters, they must be the same for all the tables!
    // Initialize flue with EIP-207 Firmware Classification API parameters
    EIP207_FLUE_OFFSET_WR(device,
                          flue_conf_p->hash_table[hash_table_id].f_prefetch_xform,
                          f_lookup_cache_present ?
                              flue_conf_p->hash_table[hash_table_id].f_lookup_cached :
                              false,
                          FIRMWARE_EIP207_CS_XFORM_RECORD_WORD_OFFSET);

    // Use Hash table 0 parameters, they must be the same for all the tables!
    // Check if arc4 Record Cache is available
    if ( f_arc4_present )
    {
        EIP207_FLUE_ARC4_OFFSET_WR(
                          device,
                          flue_conf_p->hash_table[hash_table_id].f_prefetch_arc4_state,
                          EIP207_GLOBAL_FLUE_LOOKUP_MODE,
                          FIRMWARE_EIP207_CS_ARC4_RECORD_WORD_OFFSET);
    }
#if EIP207_GLOBAL_FLUE_LOOKUP_MODE == EIP207_FLUE_3ENTRY_LOOKUP_MODE
    else
    {
        EIP207_FLUE_ARC4_OFFSET_WR(device,
                                   hash_table_id,
                                   EIP207_GLOBAL_FLUE_LOOKUP_MODE,
                                   0);
    }
#endif // EIP207_GLOBAL_FLUE_LOOKUP_MODE == EIP207_FLUE_1ENTRY_LOOKUP_MODE

#ifdef EIP207_FLUE_HAVE_VIRTUALIZATION
    // Virtualisation support present, initialize the lookup table.
    // All interfaces refer to Table # 0.
    {
        unsigned int i;
        unsigned int c;
        unsigned int idx0,idx1,idx2,idx3;

        c = flue_conf_p->interfaces_count;
        if (c > EIP207_FLUE_MAX_NOF_INTERFACES_TO_USE)
            c = EIP207_FLUE_MAX_NOF_INTERFACES_TO_USE;

        for (i = 0; i < c; i += 4)
        {
            idx0 = flue_conf_p->interface_index[i];

            if (i + 1 < c)
                idx1 = flue_conf_p->interface_index[i + 1];
            else
                idx1 = 0;

            if (i + 2 < c)
                idx2 = flue_conf_p->interface_index[i + 2];
            else
                idx2 = 0;

            if (i + 3 < c)
                idx3 = flue_conf_p->interface_index[i + 3];
            else
                idx3 = 0;

            EIP207_FLUE_IFC_LUT_WR(device,
                                   i / 4,
                                   idx0, idx1, idx2, idx3);
        }
    }
#endif

    return;
}


/*----------------------------------------------------------------------------
 * eip207_flue_status_get
 */
void
eip207_flue_status_get(
        const device_handle_t device,
        eip207_global_flue_status_t * const flue_status_p)
{
    IDENTIFIER_NOT_USED(device);

    flue_status_p->error1 = 0;
    flue_status_p->error2 = 0;

    return;
}


/* end of file eip207_flue.c */
