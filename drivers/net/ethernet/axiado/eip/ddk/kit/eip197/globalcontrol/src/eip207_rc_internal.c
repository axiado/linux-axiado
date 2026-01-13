/* eip207_rc_internal.c
 *
 * EIP-207 Record Cache (RC) interface implementation
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

// Default configuration
#include "c_eip207_global.h"

// EIP-207 Record Cache (RC) internal interface
#include "eip207_rc_internal.h"

// eip97_interfaces_get()
#include "eip97_global_internal.h"


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Driver Framework Basic Definitions API
#include "basic_defs.h"                 // u32, bool

// EIP-207 Global Control Driver Library internal interfaces
#include "eip207_level0.h"              // EIP-207 Level 0 macros

// EIP-206 Global Control Driver Library internal interfaces
#include "eip206_level0.h"              // EIP-206 Level 0 macros

// Driver Framework device API
#include "device_types.h"               // device_handle_t

// EIP-207 Firmware Classification API
#include "firmware_eip207_api_cs.h"     // Classification API: General


/*----------------------------------------------------------------------------
 * Definitions and macros
 */

#if FIRMWARE_EIP207_CS_ARC4RC_RECORD_WORD_COUNT <= \
                FIRMWARE_EIP207_CS_TRC_RECORD_WORD_COUNT
#define EIP207_RC_ARC4_SIZE                             0
#elif FIRMWARE_EIP207_CS_ARC4RC_RECORD_WORD_COUNT <= \
                FIRMWARE_EIP207_CS_TRC_RECORD_WORD_COUNT_LARGE
#define EIP207_RC_ARC4_SIZE                             1
#else
#error "error: arc4 state Record size too big"
#endif

// Minimum number of entries in the Record Cache
#define EIP207_RC_MIN_ENTRY_COUNT                       32

// Maximum number of entries in the Record Cache
#define EIP207_RC_MAX_ENTRY_COUNT                       4096

// Maximum number of records in cachs
#define EIP207_RC_MAX_RECORD_COUNT                      1023

// number of header words (32-bits) in a cache record
#define EIP207_RC_HEADER_WORD_COUNT                     4

// number of 32-bit words in one administration memory word
#define EIP207_RC_ADMIN_MEMWORD_WORD_COUNT              4

// number of hash table entries in one administration memory word
#define EIP207_RC_ADMIN_MEMWORD_ENTRY_COUNT             8

// Null value used in Record Caches
#define EIP207_RC_NULL_VALUE                            0x3FF

// Checks for the required configuration parameters
#ifndef EIP207_FRC_ADMIN_RAM_WORD_COUNT
#error "EIP207_FRC_ADMIN_RAM_WORD_COUNT not defined"
#endif // EIP207_FRC_ADMIN_RAM_WORD_COUNT

#ifndef EIP207_TRC_ADMIN_RAM_WORD_COUNT
#error "EIP207_TRC_ADMIN_RAM_WORD_COUNT not defined"
#endif // EIP207_TRC_ADMIN_RAM_WORD_COUNT

#ifndef EIP207_ARC4RC_ADMIN_RAM_WORD_COUNT
#error "EIP207_ARC4RC_ADMIN_RAM_WORD_COUNT not defined"
#endif // EIP207_ARC4RC_ADMIN_RAM_WORD_COUNT

// Minimum required Record Cache Admin RAM size
#define EIP207_RC_MIN_ADMIN_RAM_WORD_COUNT \
      (((EIP207_RC_MIN_ENTRY_COUNT / EIP207_RC_ADMIN_MEMWORD_ENTRY_COUNT) * \
          EIP207_RC_ADMIN_MEMWORD_WORD_COUNT) + \
             EIP207_RC_HEADER_WORD_COUNT * EIP207_RC_MIN_ENTRY_COUNT)

// Check if the configured frc Admin RAM size is large enough to contain
// the minimum required number of record headers with their hash table
#if EIP207_FRC_ADMIN_RAM_WORD_COUNT > 0 && \
    EIP207_FRC_ADMIN_RAM_WORD_COUNT < EIP207_RC_MIN_ADMIN_RAM_WORD_COUNT
#error "Configured frc Admin RAM size is too small"
#endif

// Check if the configured trc Admin RAM size is large enough to contain
// the minimum required number of record headers with their hash table
#if EIP207_TRC_ADMIN_RAM_WORD_COUNT > 0 && \
    EIP207_TRC_ADMIN_RAM_WORD_COUNT < EIP207_RC_MIN_ADMIN_RAM_WORD_COUNT
#error "Configured trc Admin RAM size is too small"
#endif

// Check if the (optional) configured arc4_rc Admin RAM size is large enough
// to contain the minimum required number of record headers
// with their hash table
#if EIP207_ARC4RC_ADMIN_RAM_WORD_COUNT > 0 && \
    EIP207_ARC4RC_ADMIN_RAM_WORD_COUNT < EIP207_RC_MIN_ADMIN_RAM_WORD_COUNT
#error "Configured arc4_rc Admin RAM size is too small"
#endif

// Hash Table size calculation for *RC_p_PARAMS registers
// Hash Table entry count =
//      2 ^ (Hash Table size + EIP207_RC_HASH_TABLE_SIZE_POWER_FACTOR)
#define EIP207_RC_HASH_TABLE_SIZE_POWER_FACTOR      5


/*----------------------------------------------------------------------------
 * eip207_lib_rc_internal_lower_power_of_two
 *
 * Rounds down a value to the lower or equal power of two value.
 */
static unsigned int
eip207_lib_rc_internal_lower_power_of_two(const unsigned int value,
                                      unsigned int * const power)
{
    unsigned int v = value;
    unsigned int i = 0;

    if (v == 0)
        return v;

    while (v)
    {
        v = v >> 1;
        i++;
    }

    v = 1 << (i - 1);

    *power = i - 1;

    return v;
}


#define EIP207_RC_DATA_WORDCOUNT_MAX 131072
#define EIP207_RC_DATA_WORDCOUNT_MIN 256
#define EIP207_RC_ADMIN_WORDCOUNT_MAX 16384
#define EIP207_RC_ADMIN_WORDCOUNT_MIN 64

/*----------------------------------------------------------------------------
 * eip207_lib_bank_set
 *
 * Set the bank for the CS RAM to the given address.
 *
 * device (input)
 *     device to use.
 *
 * bank_nr (input)
 *     bank number from 0 to 7.
 *
 */
static void
eip207_lib_bank_set(
    const device_handle_t device,
    u32 bank_nr)
{
    u32 old_value = device_read32(device, EIP207_CS_REG_RAM_CTRL);

    u32 new_value = (old_value & 0xffff8fff) | ((bank_nr&0x7) << 12);

    device_write32(device, EIP207_CS_REG_RAM_CTRL, new_value);
}


static void
eip207_lib_ram_write32(
        const device_handle_t device,
        u32 address,
        u32 value)
{
    device_write32(device,
                   EIP207_CS_RAM_XS_SPACE_BASE + address,
                   value);
}

static u32
eip207_lib_ram_read32(
        const device_handle_t device,
        u32 address)
{
    return device_read32(device,
                         EIP207_CS_RAM_XS_SPACE_BASE + address);
}



/*----------------------------------------------------------------------------
 * eip207_lib_ram_size_probe
 *
 * Probe the size of the accessible RAM, do not access more memory than
 * indicated by max_size.
 *
 * device (input)
 *     device to use.
 *
 * max_size (input)
 *     Maximum size of RAM
 */
static unsigned int
eip207_lib_ram_size_probe(
    const device_handle_t device,
    const unsigned int max_size)
{
    unsigned int max_bank, max_page, max_offs, i, ram_size;

    if (max_size <= 16384)
    {
        // All RAM is in a single bank.
        max_bank = 0;
    }
    else
    {
        // Probe the maximum bank number that has (distinct) RAM.
        for (i=0; i<8; i++)
        {
            eip207_lib_bank_set(device, 7 - i);
            eip207_lib_ram_write32(device, 0, 7 - i);
            eip207_lib_ram_write32(device, 4, 0);
            eip207_lib_ram_write32(device, 8, 0);
            eip207_lib_ram_write32(device, 12, 0);
        }
        max_bank=0;
        for (i=0; i<7; i++)
        {
            eip207_lib_bank_set(device, i);
            if (eip207_lib_ram_read32(device, 0) != i)
            {
                break;
            }
            max_bank = i;
        }
    }

    eip207_lib_bank_set(device, max_bank);

    for (i=0; i<0x10000; i+=0x100)
    {
        eip207_lib_ram_write32(device, 0xff00-i, 0xff00-i);
        eip207_lib_ram_write32(device, 0xff00-i+4, 0);
        eip207_lib_ram_write32(device, 0xff00-i+8, 0);
        eip207_lib_ram_write32(device, 0xff00-i+12, 0);
    }

    max_page = 0;
    for (i=0; i<0x10000; i+=0x100)
    {
        if (eip207_lib_ram_read32(device, i) != i)
        {
            break;
        }
        max_page = i;
    }

    for (i=0; i<0x100; i+= 4)
    {
        eip207_lib_ram_write32(device, max_page + 0xfc - i, max_page + 0xfc - i);
    }

    max_offs = 0;

    for (i=0; i<0x100; i+=4)
    {
        if (eip207_lib_ram_read32(device, max_page + i) != max_page + i)
        {
            break;
        }
        max_offs = i;
    }

    eip207_lib_bank_set(device, 0);
    ram_size = ((max_bank<<16) + max_page + max_offs + 4) >> 2;

    if (ram_size > max_size)
        ram_size = max_size;

    return ram_size;
}

/*----------------------------------------------------------------------------
 * eip207_rc_internal_init
 */
eip207_global_error_t
eip207_rc_internal_init(
        const device_handle_t device,
        const eip207_rc_internal_combination_type_t combination_type,
        const u32 cache_base,
        eip207_global_cache_params_t * rc_params_p,
        const unsigned int record_word_count)
{
    unsigned int i;
    u16 rc_record2_word_count = 0;
    u8 clocks_per_tick;
    bool f_frc = false, f_trc = false, f_arc4 = false;
    eip207_global_cache_params_t * rc_p = rc_params_p;
    unsigned int null_val = EIP207_RC_NULL_VALUE;

    switch (cache_base)
    {
        case EIP207_FRC_REG_BASE:
            f_frc = true;
            rc_record2_word_count = 0;
            break;

        case EIP207_TRC_REG_BASE:
            f_trc = true;
            rc_record2_word_count =
                    FIRMWARE_EIP207_CS_TRC_RECORD_WORD_COUNT_LARGE;
            break;

        case EIP207_ARC4RC_REG_BASE:
            f_arc4 = true;
            rc_record2_word_count =
                    FIRMWARE_EIP207_CS_ARC4RC_RECORD_WORD_COUNT_LARGE;
            break;

        default:
            return EIP207_GLOBAL_ARGUMENT_ERROR;
    }

    if (combination_type != EIP207_RC_INTERNAL_NOT_COMBINED)
    {
        if( f_frc ) // frc cannot be combined
            return EIP207_GLOBAL_ARGUMENT_ERROR;

        // Initialize all the configured for use Record Cache sets
        for (i = 0; i < EIP207_GLOBAL_MAX_NOF_CACHE_SETS_TO_USE; i++)
            EIP207_RC_PARAMS_WR(device,
                                cache_base,
                                i,     // Cache Set number
                                false, // Disable cache RAM access
                                false, // Block Next command is reserved
                                false, // Enable access cache administration RAM
                                0,
                                0,     // Block Time Base is reserved
                                false, // Not used for this HW
                                rc_record2_word_count); // Large record size

        return EIP207_GLOBAL_NO_ERROR;
    }
    // Indicate if arc4 state records are considered 'large' transform records.
    // Only relevent inin case the trc is used to store arc4 state records, but
    // without a combined cache.
    if (f_trc)
    {
        unsigned int nof_c_es;
        eip97_interfaces_get(&nof_c_es,NULL,NULL,NULL);
        for (i = 0; i < nof_c_es; i++)
            EIP206_ARC4_SIZE_WR(device, i, EIP207_RC_ARC4_SIZE);
    }

    // Initialize all the configured for use Record Cache sets
    for (i = 0; i < EIP207_GLOBAL_MAX_NOF_CACHE_SETS_TO_USE; i++)
    {
        unsigned int rc_ram_word_count = rc_p->data_word_count;
        u8 rc_hash_table_size;
        unsigned int j, power,
                     rc_record_count,
                     rc_record_word_count,
                     rc_hash_table_entry_count,
                     rc_hash_table_word_count,
                     rc_admin_ram_word_count,
                     rc_admin_ram_entry_count,
                     rc_hash_table_byte_offset;

        if (rc_p->f_enable == false)
            continue; // Cache is not enabled

        // Enable Record Cache RAM access
        EIP207_CS_RAM_CTRL_WR(
            device,
            f_frc && i==0,
            f_frc && i==1,
            f_frc && i==2,
            f_trc && i==0,
            f_trc && i==1,
            f_trc && i==2,
            f_arc4 && i==0,
            f_arc4 && i==1,
            f_arc4 && i==2,
            false);               // No FLUEC cache RAM access

        // Take Record Cache into reset
        // Make cache data RAM accessible
        EIP207_RC_PARAMS_WR(device,
                            cache_base,
                            i,     // Cache Set number
                            true,  // Enable cache RAM access
                            false,
                            true, // Enable access cache data RAM
                            0,
                            0,
                            false, // Not used here for this HW
                            0);

        if (rc_ram_word_count == 0 ||
            rc_ram_word_count > EIP207_RC_DATA_WORDCOUNT_MAX)
            rc_ram_word_count = EIP207_RC_DATA_WORDCOUNT_MAX;
        // Check the size of the data RAM.
        rc_ram_word_count = eip207_lib_ram_size_probe(
            device,
            rc_ram_word_count);

        // data RAM may be inaccessible on some hardware configurations,
        // so RAM size probing may not work. Assume that provided word count
        // input actually reflects RAM size.
        if (rc_ram_word_count < EIP207_RC_DATA_WORDCOUNT_MIN)
            rc_ram_word_count = MIN(rc_p->data_word_count, EIP207_RC_DATA_WORDCOUNT_MAX);
        if (rc_ram_word_count < EIP207_RC_DATA_WORDCOUNT_MIN)
            return EIP207_GLOBAL_INTERNAL_ERROR;

        rc_p->data_word_count = rc_ram_word_count;

        // Take Record Cache into reset
        // Make cache administration RAM accessible
        EIP207_RC_PARAMS_WR(device,
                            cache_base,
                            i,     // Cache Set number
                            true,  // Enable cache RAM access
                            false,
                            false, // Enable access cache administration RAM
                            0,
                            0,
                            false, // Not used here for this HW
                            0);

        // Get the configured RC Admin RAM size
        rc_admin_ram_word_count = rc_p->admin_word_count;
        if (rc_admin_ram_word_count == 0 ||
            rc_admin_ram_word_count > EIP207_RC_ADMIN_WORDCOUNT_MAX)
            rc_admin_ram_word_count = EIP207_RC_ADMIN_WORDCOUNT_MAX;

        rc_admin_ram_word_count = eip207_lib_ram_size_probe(
            device,
            rc_admin_ram_word_count);

        rc_p->admin_word_count = rc_admin_ram_word_count;

        if (rc_admin_ram_word_count < EIP207_RC_ADMIN_WORDCOUNT_MIN)
            return EIP207_GLOBAL_INTERNAL_ERROR;

       // Check the size of the Admin RAM

        // Determine the RC record size to use
        if (rc_record2_word_count > record_word_count)
            rc_record_word_count = rc_record2_word_count;
        else
            rc_record_word_count = record_word_count;

        // Calculate the maximum possible record count that
        // the Record Cache data RAM can contain
        rc_record_count = rc_ram_word_count / rc_record_word_count;
        if (rc_record_count > EIP207_RC_MAX_RECORD_COUNT)
            rc_record_count = EIP207_RC_MAX_RECORD_COUNT;

        // rc_record_count is calculated using the configured RC data RAM size.

        // rc_admin_ram_entry_count is calculated using
        // the configured RC Admin RAM size.

        // Calculate the maximum possible record count that
        // the RC Hash Table (in Record Cache Administration RAM) can contain
        rc_admin_ram_entry_count = EIP207_RC_ADMIN_MEMWORD_ENTRY_COUNT *
                                    rc_admin_ram_word_count /
                                (EIP207_RC_ADMIN_MEMWORD_WORD_COUNT +
                                        EIP207_RC_ADMIN_MEMWORD_ENTRY_COUNT *
                                           EIP207_RC_HEADER_WORD_COUNT);

        // Try to extend the Hash Table in the RC Admin RAM
        if (rc_record_count < rc_admin_ram_entry_count)
        {
            unsigned int ht_space_word_count;

            // Calculate the size of space available for the Hash Table
            ht_space_word_count = rc_admin_ram_word_count -
                                  rc_record_count * EIP207_RC_HEADER_WORD_COUNT;

            // Calculate maximum possible Hash Table entry count
            rc_hash_table_entry_count = (ht_space_word_count /
                                        EIP207_RC_ADMIN_MEMWORD_WORD_COUNT) *
                                           EIP207_RC_ADMIN_MEMWORD_ENTRY_COUNT;
        }
        else // Extension impossible
            rc_hash_table_entry_count = rc_admin_ram_entry_count;

        // Check minimum number of entries in the record cache
        rc_hash_table_entry_count = MAX(EIP207_RC_MIN_ENTRY_COUNT,
                                      rc_hash_table_entry_count);

        // Check maximum number of entries in the record cache
        rc_hash_table_entry_count = MIN(EIP207_RC_MAX_ENTRY_COUNT,
                                      rc_hash_table_entry_count);

        // Round down to power of two
        power = 0;
        rc_hash_table_entry_count =
             eip207_lib_rc_internal_lower_power_of_two(rc_hash_table_entry_count,
                                                   &power);

        // Hash Table mask that determines the hash table size
        if (power >= EIP207_RC_HASH_TABLE_SIZE_POWER_FACTOR)
            rc_hash_table_size =
                    (u8)(power - EIP207_RC_HASH_TABLE_SIZE_POWER_FACTOR);
        else
            // Insufficient memory for Hash Table in the RC Admin RAM
            return EIP207_GLOBAL_INTERNAL_ERROR;

        // Calculate the Hash Table size in 32-bit words
        rc_hash_table_word_count = rc_hash_table_entry_count /
                                  EIP207_RC_ADMIN_MEMWORD_ENTRY_COUNT *
                                       EIP207_RC_ADMIN_MEMWORD_WORD_COUNT;

        // Recalculate the record count that fits the RC Admin RAM space
        // without the Hash Table, restricting for the maximum records
        // which fit the RC data RAM
        {
            // Adjusted record count which fits the RC Admin RAM
            unsigned int rc_admin_ram_adjusted_entry_count =
                             (rc_admin_ram_word_count -
                                rc_hash_table_word_count) /
                                   EIP207_RC_HEADER_WORD_COUNT;

            // Maximum record count which fits the RC data RAM - rc_record_count
            // use the minimum of the two
            rc_record_count = MIN(rc_record_count,
                                 rc_admin_ram_adjusted_entry_count);
        }

        // Clear all ECC errors
        EIP207_RC_ECCCTRL_WR(device, cache_base, i, false, false, false);

        // Clear all record administration words
        // in Record Cache administration RAM
        for (j = 0; j < rc_record_count; j++)
        {
            // Calculate byte offset for the current record
            unsigned int byte_offset = EIP207_CS_RAM_XS_SPACE_BASE +
                                      j *
                                        EIP207_RC_HEADER_WORD_COUNT *
                                          sizeof(u32);

            // Write word 0
            device_write32(device,
                           byte_offset,
                           (null_val << 20) | // Hash_Collision_Prev
                           (null_val << 10)); // Hash_Collision_Next

            // Write word 1
            byte_offset += sizeof(u32);

            if (j == rc_record_count - 1)
            {
                // Last record
                device_write32(device,
                               byte_offset,
                               ((j - 1) << 10) |   // Free_List_Prev
                               null_val);           // Free_List_Next
            }
            else if (j == 0)
            {
                // First record
                device_write32(device,
                               byte_offset,
                               (null_val << 10) | // Free_List_Prev
                               (j + 1));         // Free_List_Next
            }
            else
            {
                // All other records
                device_write32(device,
                               byte_offset,
                               ((j - 1) << 10) | // Free_List_Prev
                               (j + 1));         // Free_List_Next
            }

            // Write word 2
            byte_offset += sizeof(u32);

            device_write32(device,
                           byte_offset,
                           0); // Address_Key, low bits

            // Write word 3
            byte_offset += sizeof(u32);

            device_write32(device,
                           byte_offset,
                           0); // Address_Key, high bits
        } // for (records)

        // Calculate byte offset for the Hash Table
        rc_hash_table_byte_offset = EIP207_CS_RAM_XS_SPACE_BASE +
                            rc_record_count *
                                EIP207_RC_HEADER_WORD_COUNT *
                                    sizeof(u32);

        // Clear all hash table words
        for (j = 0; j < rc_hash_table_word_count; j++)
            device_write32(device,
                           rc_hash_table_byte_offset + j * sizeof(u32),
                           0x3FFFFFFF);

        // Disable Record Cache RAM access
        EIP207_CS_RAM_CTRL_DEFAULT_WR(device);

        // Write head and tail pointers to the RC Free Chain
        EIP207_RC_FREECHAIN_WR(
                  device,
                  cache_base,
                  i,                               // Cache Set number
                  0,                               // head pointer
                  (u16)(rc_record_count - 1)); // tail pointer

        // Set Hash Table start
        // This is an offset from EIP207_CS_RAM_XS_SPACE_BASE
        // in record administration memory words
        EIP207_RC_PARAMS2_WR(device,
                             cache_base,
                             i,
                             (u16)rc_record_count,
    /* Small Record size */  f_arc4 ? 0 : (u16)record_word_count,
                             FIRMWARE_EIP207_RC_DMA_WR_COMB_DLY);

        // Select the highest clock count as specified by
        // the Host and the Firmware for the frc
#ifdef FIRMWARE_EIP207_CS_BLOCK_NEXT_COMMAND_LOGIC_DISABLE
        clocks_per_tick = rc_p->block_clock_count;
#else
        {
            u8 tmp;

            tmp = (u8)FIRMWARE_EIP207_CS_FRC_BLOCK_TIMEBASE;
            clocks_per_tick = rc_p->block_clock_count >= tmp ?
                                       cache_conf_p->frc.block_clock_count : tmp;
        }
#endif

        // Take Record Cache out of reset
        EIP207_RC_PARAMS_WR(device,
                            cache_base,
                            i,     // Cache Set number
                            false, // Disable cache RAM access
                            rc_p->f_non_block,
                            false, // Disable access cache administration RAM
                            rc_hash_table_size,
                            clocks_per_tick,
                            false, // Not used here for this HW
                            rc_record2_word_count); // Large record size

        rc_p++;
    } // for, i cache sets

    return EIP207_GLOBAL_NO_ERROR;
}

static void
eip207_rc_internal_debug_statistics_single_get(
        const device_handle_t device,
        const unsigned int cache_set_id,
        u32 reg_base,
        eip207_global_cache_debug_statistics_t * const stats_p)
{
    EIP207_RC_LONGCTR_RD(device, EIP207_RC_REG_PREFEXEC(reg_base, cache_set_id),
                         &stats_p->prefetch_exec);
    EIP207_RC_LONGCTR_RD(device, EIP207_RC_REG_PREFBLCK(reg_base, cache_set_id),
                         &stats_p->prefetch_block);
    EIP207_RC_LONGCTR_RD(device, EIP207_RC_REG_PREFDMA(reg_base, cache_set_id),
                         &stats_p->prefetch_dma);
    EIP207_RC_LONGCTR_RD(device, EIP207_RC_REG_SELOPS(reg_base, cache_set_id),
                         &stats_p->select_ops);
    EIP207_RC_LONGCTR_RD(device, EIP207_RC_REG_SELDMA(reg_base, cache_set_id),
                         &stats_p->select_dma);
    EIP207_RC_LONGCTR_RD(device, EIP207_RC_REG_IDMAWR(reg_base, cache_set_id),
                         &stats_p->int_dma_write);
    EIP207_RC_LONGCTR_RD(device, EIP207_RC_REG_XDMAWR(reg_base, cache_set_id),
                         &stats_p->ext_dma_write);
    EIP207_RC_LONGCTR_RD(device, EIP207_RC_REG_INVCMD(reg_base, cache_set_id),
                         &stats_p->invalidate_ops);
    EIP207_RC_RDMAERRFLGS_RD(device, reg_base, cache_set_id,
                             &stats_p->read_dma_err_flags);
    EIP207_RC_SHORTCTR_RD(device, EIP207_RC_REG_RDMAERR(reg_base, cache_set_id),
                          &stats_p->read_dma_errors);
    EIP207_RC_SHORTCTR_RD(device, EIP207_RC_REG_WDMAERR(reg_base, cache_set_id),
                          &stats_p->write_dma_errors);
    EIP207_RC_SHORTCTR_RD(device, EIP207_RC_REG_INVECC(reg_base, cache_set_id),
                          &stats_p->invalidate_ecc);
    EIP207_RC_SHORTCTR_RD(device, EIP207_RC_REG_DATECC_CORR(reg_base, cache_set_id),
                          &stats_p->data_ecc_corr);
    EIP207_RC_SHORTCTR_RD(device, EIP207_RC_REG_ADMECC_CORR(reg_base, cache_set_id),
                          &stats_p->admin_ecc_corr);

}


void
eip207_rc_internal_debug_statistics_get(
        const device_handle_t device,
        const unsigned int cache_set_id,
        eip207_global_cache_debug_statistics_t * const frc_stats_p,
        eip207_global_cache_debug_statistics_t * const trc_stats_p)
{
    eip207_rc_internal_debug_statistics_single_get(
        device,
        cache_set_id,
        EIP207_FRC_REG_BASE,
        frc_stats_p);
    eip207_rc_internal_debug_statistics_single_get(
        device,
        cache_set_id,
        EIP207_TRC_REG_BASE,
        trc_stats_p);
}

/*----------------------------------------------------------------------------
 * eip207_rc_internal_status_get
 */
void
eip207_rc_internal_status_get(
        const device_handle_t device,
        const unsigned int cache_set_id,
        eip207_global_cache_status_t * const frc_status_p,
        eip207_global_cache_status_t * const trc_status_p,
        eip207_global_cache_status_t * const arc4_rc_status_p)
{
    // Read frc status
    EIP207_RC_PARAMS_RD(device,
                        EIP207_FRC_REG_BASE,
                        cache_set_id,
                        &frc_status_p->f_dma_read_error,
                        &frc_status_p->f_dma_write_error);

    EIP207_RC_ECCCTRL_RD_CLEAR(device,
                               EIP207_FRC_REG_BASE,
                               cache_set_id,
                               &frc_status_p->f_data_ecc_oflo,
                               &frc_status_p->f_data_ecc_err,
                               &frc_status_p->f_admin_ecc_err);

    // Read trc status
    EIP207_RC_PARAMS_RD(device,
                        EIP207_TRC_REG_BASE,
                        cache_set_id,
                        &trc_status_p->f_dma_read_error,
                        &trc_status_p->f_dma_write_error);

    EIP207_RC_ECCCTRL_RD_CLEAR(device,
                               EIP207_TRC_REG_BASE,
                               cache_set_id,
                               &trc_status_p->f_data_ecc_oflo,
                               &trc_status_p->f_data_ecc_err,
                               &trc_status_p->f_admin_ecc_err);

    // Read arc4_rc status
    EIP207_RC_PARAMS_RD(device,
                        EIP207_ARC4RC_REG_BASE,
                        cache_set_id,
                        &arc4_rc_status_p->f_dma_read_error,
                        &arc4_rc_status_p->f_dma_write_error);

    EIP207_RC_ECCCTRL_RD_CLEAR(device,
                               EIP207_ARC4RC_REG_BASE,
                               cache_set_id,
                               &arc4_rc_status_p->f_data_ecc_oflo,
                               &arc4_rc_status_p->f_data_ecc_err,
                               &arc4_rc_status_p->f_admin_ecc_err);
}


/* end of file eip207_rc_internal.c */
