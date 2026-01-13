/* eip207_global_init.h
 *
 * EIP-207 Global Control API:
 * Record Cache, flue, optional FLUEC, ice and optional oce initialization
 * as well as ice and optional oce firmware download.
 *
 * This API is not re-entrant.
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

#ifndef EIP207_GLOBAL_INIT_H_
#define EIP207_GLOBAL_INIT_H_


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Default configuration
#include "c_eip207_global.h"

// Driver Framework Basic Definitions API
#include "basic_defs.h"         // u8, u32, bool

// Driver Framework device API
#include "device_types.h"       // device_handle_t


/*----------------------------------------------------------------------------
 * Definitions and macros
 */

#define EIP207_GLOBAL_IOAREA_REQUIRED_SIZE       (2 * sizeof(void*))

// place holder for device specific internal data
typedef struct
{
    u32 placeholder[EIP207_GLOBAL_IOAREA_REQUIRED_SIZE];
} eip207_global_io_area_t;

/*----------------------------------------------------------------------------
 * eip207_global_error_t
 *
 * status (error) code type returned by these API functions
 * See each function "Return value" for details.
 *
 * EIP207_GLOBAL_NO_ERROR : successful completion of the call.
 * EIP207_GLOBAL_UNSUPPORTED_FEATURE_ERROR : not supported by the device.
 * EIP207_GLOBAL_ARGUMENT_ERROR :  invalid argument for a function parameter.
 * EIP207_GLOBAL_ILLEGAL_IN_STATE : illegal state transition
 * EIP207_GLOBAL_INTERNAL_ERROR : failure due to internal error
 */
typedef enum
{
    EIP207_GLOBAL_NO_ERROR = 0,
    EIP207_GLOBAL_UNSUPPORTED_FEATURE_ERROR,
    EIP207_GLOBAL_ARGUMENT_ERROR,
    EIP207_GLOBAL_ILLEGAL_IN_STATE,
    EIP207_GLOBAL_INTERNAL_ERROR
} eip207_global_error_t;

// 64-bit data structure
typedef struct
{
    u32 value64_lo; // Lowest 32 bits of the 64-bit value
    u32 value64_hi; // Highest 32 bits of the 64-bit value
} eip207_global_value64_t;

typedef struct
{
    // Version information
    unsigned int major;
    unsigned int minor;
    unsigned int patch_level;

    // Pointer to the memory location where the firmware image resides
    const u32 * image_p;

    // Firmware image size in 32-bit words
    unsigned int image_word_count;

} eip207_firmware_t;

// EIP-207 HW version
typedef struct
{
    // The basic EIP number.
    u8 eip_number;

    // The complement of the basic EIP number.
    u8 complmt_eip_number;

    // Hardware Patch Level.
    u8 hw_patch_level;

    // minor Hardware revision.
    u8 min_hw_revision;

    // major Hardware revision.
    u8 maj_hw_revision;
} eip207_version_t;

// EIP-207 HW options
typedef struct
{
    // number of cache sets implemented, in range 1...3
    u8 nof_cache_sets;

    // number of external (non-Host) clients on the Flow Record Cache
    // (for each of the cache sets if more than one of those are present)
    u8 nof_frc_clients;

    // number of external (non-Host) clients on the Transform Record Cache
    // (for each of the cache sets if more than one of those are present)
    u8 nof_trc_clients;

    // True when Transform Record Cache logic is combined with the Flow Record
    // Cache logic (i.e. they share the processing pipeline, administration
    // control logic and RAM, but have separate client states and Host client
    // access interfaces).
    bool f_combined_frc_trc;

    // number of external (non-Host) clients on the arc4 state Record Cache
    // (for each of the cache sets if more than one of those are present)
    // Set to 0 when the arc4 state Record Cache is not present
    u8 nof_arc4_clients;

    // True when the arc4 Record Cache is present
    bool f_arc4_present;

    // True when arc4 state Record Cache logic is combined with the Flow Record
    // Cache logic (i.e. they share the processing pipeline, administration
    // control logic and RAM, but have separate client states and Host client
    // access interfaces).
    // Only valid when nof_arc4_clients is not 0.
    bool f_combined_frc_arc4;

    // True when arc4 state Record Cache logic is combined with the Transform
    // Record Cache logic (i.e. they share the processing pipeline,
    // administration control logic and RAM, but have separate client states
    // and Host client access interfaces).
    // Only valid when nof_arc4_clients is not 0.
    bool f_combined_trc_arc4;

    // number of clients to the flow hash and lookup logic, independent from
    // the number of cache sets.
    u8 nof_lookup_clients;

    // True when the Flow lookup logic is fitted with a cache RAM.
    bool f_lookup_cached;

    // number of lookup tables supported by the flow lookup logic
    u8 nof_lookup_tables;

} eip207_options_t;

// capabilities structure for EIP-207 HW
typedef struct
{
    // EIP-207 HW shell
    eip207_options_t     eip207_options;
    eip207_version_t     eip207_version;

} eip207_global_capabilities_t;

// Classification Engine clocks per one tick for
// the blocking next command logic
typedef enum
{
    EIP207_BLOCK_CLOCKS_16 = 0,
    EIP207_BLOCK_CLOCKS_32,
    EIP207_BLOCK_CLOCKS_64,
    EIP207_BLOCK_CLOCKS_128,
    EIP207_BLOCK_CLOCKS_256,
    EIP207_BLOCK_CLOCKS_512,
    EIP207_BLOCK_CLOCKS_1024,
    EIP207_BLOCK_CLOCKS_2048,
} eip207_block_clocks_t;

// EIP-207s frc/trc/arc4_rc configuration
typedef struct
{
    // True when the record cache must be enabled
    bool f_enable;

    // True when the block next command logic is disabled
    bool f_non_block;

    // A blocked command will be released automatically after 3 ticks of
    // a free-running timer whose speed is set by this field.
    // This parameter configures the number of the Classification Engine
    // clocks per one tick, see eip207_block_clocks_t values
    u8 block_clock_count;

    // Record base address
    eip207_global_value64_t rec_base_addr;

    // number of words in Admin RAM.
    unsigned int admin_word_count;

    // number of words in data RAM.
    unsigned int data_word_count;

} eip207_global_cache_params_t;

// EIP-207s frc/trc/arc4_rc status information
typedef struct
{
    // True when a Host bus master read access from this cache
    // resulted in an error.
    bool f_dma_read_error;

    // True when a Host bus master write access from this cache
    // resulted in an error.
    bool f_dma_write_error;

    // True when a non-correctable ECC error is detected in the Record Cache
    // Administration RAM. In case of this error the Record Cache will enter the
    // software reset state.
    bool f_admin_ecc_err;

    // True when a non-correctable ECC error is detected in the Record Cache
    // data RAM. In case of this error the Record Cache will remain
    // operational, only the packets for the corrupted record will be flushed.
    bool f_data_ecc_err;

    // True when a non-correctable ECC error is detected in the Record Cache
    // data RAM. In case of this error the Record Cache will enter the
    // software reset state.
    bool f_data_ecc_oflo;

} eip207_global_cache_status_t;

// EIP207 Cache statistics.
typedef struct
{
    // number of prefetches executed
    u64 prefetch_exec;
    // number of prefetches blocked
    u64 prefetch_block;
    // number of pretetches with DMA
    u64 prefetch_dma;
    // number of select operations
    u64 select_ops;
    // number of select operations with DMA
    u64 select_dma;
    // number of internal DMA writes
    u64 int_dma_write;
    // number of external DMA writes
    u64 ext_dma_write;
    // number of invalidate operations
    u64 invalidate_ops;
    // Read DMA error flags
    u32 read_dma_err_flags;
    // number of read DMA errors
    u32 read_dma_errors;
    // number of write DMA errors
    u32 write_dma_errors;
    // number of invalidates caused by ECC errors
    u32 invalidate_ecc;
    // number of correctable ECC errors in data RAM
    u32 data_ecc_corr;
    // number of correctable ECC errors in Admin RAM
    u32 admin_ecc_corr;
} eip207_global_cache_debug_statistics_t;

typedef struct
{
    eip207_global_cache_params_t frc [EIP207_GLOBAL_MAX_NOF_CACHE_SETS_TO_USE];
    eip207_global_cache_params_t trc [EIP207_GLOBAL_MAX_NOF_CACHE_SETS_TO_USE];
    eip207_global_cache_params_t arc4[EIP207_GLOBAL_MAX_NOF_CACHE_SETS_TO_USE];
} eip207_global_cache_config_t;

// Flow Hash iv
// Although there are as many flow hash engines in the Classification Support
// module (EIP-207s) as there are Pull-up Micro-engines in the Classification
// Engines (EIP-207c), they all use the same iv value to start their
// calculations from.
typedef struct
{
    u32 iv_word32[4]; // must be written with a true random value
} eip207_hash_iv_t;

// Flow hash table configuration parameters,
// there can be multiple (f) flow hash tables
typedef struct
{
    // True when transform record pre-fetch triggering must be done in the Flow
    // Record Cache for flow lookups done via this hash table.
    bool f_prefetch_xform;

    // True when arc4 state record pre-fetch triggering must be done
    // in the Flow Record Cache for flow lookups done via this hash table.
    bool f_prefetch_arc4_state;

    // True when caching of the flow lookups is enabled for this hash table.
    bool f_lookup_cached;

} eip207_global_flow_hash_table_t;

// Flow Look-Up Engine (flue) configuration
typedef struct
{
    // Flow hash calculation iv data
    eip207_hash_iv_t iv;

    // When true the lookup in Host memory is only started on a failed flow
    // lookup cache access. When false the read from the hash table in Host
    // memory is started in parallel with the flow lookup cache access.
    bool f_delay_mem_xs;

    // number of intermediate entries cached when performing a flow lookup
    u8 cache_chain;

    // number of configured tables in the hash_table parameter,
    // may not exceed EIP207_MAX_NOF_FLUE_HASH_TABLES
    unsigned int hash_tables_count;

    eip207_global_flow_hash_table_t
                hash_table[EIP207_MAX_NOF_FLOW_HASH_TABLES_TO_USE];

    // number of packet interfaces in the interface_index parameter.
    // may not exceed EIP207_FLUE_MAX_NOF_FLOW_HASH_TABLES_TO_USE
    unsigned int interfaces_count;

    unsigned int interface_index[EIP207_FLUE_MAX_NOF_INTERFACES_TO_USE];

} eip207_global_flue_config_t;

// This data structure represents EIP-207s Input Classification Engine (ice)
// global statistics.
typedef struct
{
    eip207_global_value64_t dropped_packets_counter;

    u32                outbound_packet_counter;

    eip207_global_value64_t outbound_octets_counter;

    u32                inbound_packets_counter;

    eip207_global_value64_t inbound_octets_counter;

} eip207_global_ice_global_stats_t;

// This data structure represents EIP-207s Output Classification Engine (oce)
// global statistics.
typedef struct
{
    // For future extensions:
    //   to be filled in with the actual oce global statistics
    eip207_global_value64_t dropped_packets_counter;

} eip207_global_oce_global_stats_t;

// This data structure represents global statistics.
typedef struct
{
    // EIP-207c ice Global Statistics
    eip207_global_ice_global_stats_t ice;

    // EIP-207c oce Global Statistics
    // Note: Not used in some EIP-207 HW versions and
    //       for these versions these counters will not be updated.
    eip207_global_oce_global_stats_t oce;

} eip207_global_global_stats_t;

// This data structure represents the clock count data as generated by
// the Classification Engine timer which speed is configured by
// the timer_prescaler parameter in the eip207_global_firmware_load() function.
// The clock count is updated when a packet is processed and represents
// the timestamp of the last packet.
typedef struct
{
    // EIP-207c ice clock count
    eip207_global_value64_t ice;

    // EIP-207c oce clock count
    // Note: Not used in some EIP-207c HW versions and
    //       for these versions the oce timestamp will not be updated.
    eip207_global_value64_t oce;

} eip207_global_clock_t;

// EIP-207c Classification Engine (CE) status information
typedef struct
{
    // True when a correctable ECC error has been detected
    // by the Pull-Up micro-engine
    bool f_pue_ecc_corr;

    // True when a non-correctable ECC error has been detected
    // by the Pull-Up micro-engine
    bool f_pue_ecc_derr;

    // True when a correctable ECC error has been detected
    // by the PostProcessor micro-engine
    bool f_fpp_ecc_corr;

    // True when a non-correctable ECC error has been detected
    // by the PostProcessor micro-engine
    bool f_fpp_ecc_derr;

    // The Classification Engine timer overflow
    bool f_timer_overflow;

} eip207_global_ce_status_t;

// Flow Look-Up Engine (flue) status information
typedef struct
{
    // error bit mask 1, bits definition is implementation-specific
    u32 error1;

    // error bit mask 2, bits definition is implementation-specific
    u32 error2;

} eip207_global_flue_status_t;

// Classification Engine status information
typedef struct
{
    // The EIP-207c Input Classification Engine (ice)
    eip207_global_ce_status_t ice;

    // The EIP-207c Output Classification Engine (oce)
    // Note: oce is not used in some EIP-207 HW versions and
    //        these flags are set to false in this case.
    eip207_global_ce_status_t oce;

    // Debug statistics counters.
    eip207_global_cache_debug_statistics_t frc_stats[EIP207_GLOBAL_MAX_NOF_CACHE_SETS_TO_USE];
    eip207_global_cache_debug_statistics_t trc_stats[EIP207_GLOBAL_MAX_NOF_CACHE_SETS_TO_USE];

    // Fatal DMA errors reported by the Record Caches (frc/trc/arc4_rc)
    // The engine must be recovered by means of the Global SW Reset or
    // the HW Reset
    eip207_global_cache_status_t frc [EIP207_GLOBAL_MAX_NOF_CACHE_SETS_TO_USE];
    eip207_global_cache_status_t trc [EIP207_GLOBAL_MAX_NOF_CACHE_SETS_TO_USE];
    eip207_global_cache_status_t arc4_rc [EIP207_GLOBAL_MAX_NOF_CACHE_SETS_TO_USE];

    // Errors reported by the EIP-207s Flow Look-Up Engine (flue).
    eip207_global_flue_status_t flue;

} eip207_global_status_t;

// Firmware configuration data structure.
typedef struct {
    bool f_token_extensions_enable; /* Use extensions to command/result tokens */
    bool f_increment_pkt_id; /* increment packet ID in tunnel headers */
    u8 dtls_record_header_align; /* size of DTLS record headers in decrypted
                                      output */
    u8 ecn_control; /* Bit mask controlling ECN packet drops */
    u16 pkt_id; /* Initial value of packet ID */

    bool f_dtls_defer_ccs; /* Defer DTLS packets of type CCS to slow path */
    bool f_dtls_defer_alert; /* Defer DTLS packets of type Alert to slow path */
    bool f_dtls_defer_handshake; /* Defer DTLS packets of type Handshake to slow path */
    bool f_dtls_defer_app_data; /* Defer DTLS packets of type AppData to slow path */
    bool f_dtls_defer_capwap; /* Defer DTLS packets with CAPWAP to slow path */

    bool f_redir_ring_enable; /* Enable the redirection ring */
    bool f_always_redirect; /* Always redirect inline to redirection ring */
    u8 redir_ring;     /* Ring ID of redirection ring */
    u16 transform_redir_enable; /* Enable per-interface the per-transform redirect. */
    u16 rps_ring_enable; /* Bitmask to select rings for Received Packet Steering. */
} eip207_firmware_config_t;

/*----------------------------------------------------------------------------
 * eip207_global_init
 *
 * This function performs the initialization of the EIP-207 Global Control HW
 * interface and transits the API to the "Caches enabled" state.
 *
 * This function returns the EIP207_GLOBAL_UNSUPPORTED_FEATURE_ERROR error code
 * when it detects a mismatch in the Global Control Driver Library configuration
 * and the use EIP-207 HW revision or configuration.
 *
 * Note: This function should be called either after the HW Reset or
 *       after the Global SW Reset.
 *       It must be called before any other functions of this API are called
 *       for this Classification Engine instance.
 *
 * io_area_p (output)
 *     Pointer to the place holder in memory for the IO Area data for
 *     the Classification Engine instance identified by the device parameter.
 *
 * device (input)
 *     handle for the EIP-207 Global Control device instance returned
 *     by eip_device_find(). There can be only one device instance for the
 *     EIP-207 Global HW interface.
 *
 * cache_conf_p (input, output)
 *     EIP-207s frc/trc/arc4_rc configuration parameters
 *
 * flue_conf_p (input)
 *     EIP-207s Flow Hash & Look-Up Engine configuration parameters
 *
 * Return value
 *     EIP207_GLOBAL_NO_ERROR
 *     EIP207_GLOBAL_UNSUPPORTED_FEATURE_ERROR
 *     EIP207_GLOBAL_ARGUMENT_ERROR
 *     EIP207_GLOBAL_ILLEGAL_IN_STATE
 */
eip207_global_error_t
eip207_global_init(
        eip207_global_io_area_t * const io_area_p,
        const device_handle_t device,
        eip207_global_cache_config_t * const cache_conf_p,
        const eip207_global_flue_config_t * const flue_conf_p);


/*----------------------------------------------------------------------------
 * eip207_global_firmware_load
 *
 * This function performs the initialization of the EIP-207 Classification
 * Engine internal RAM, performs the firmware download and transits the API to
 * the "Firmware Loaded" state. The function also clears the global statistics.
 *
 * Note: This function should be called after eip207_global_init()
 *
 * io_area_p (output)
 *     Pointer to the IO Area for the Classification Engine.
 *
 * timer_prescaler (input)
 *     Prescaler setting to let the 64 bit timer increment at a fixed rate
 *
 * ipue_firmware_p (input, output)
 *     Pointer to the firmware data structure for the Input Pull-up
 *     micro-engine.
 *     The adapter has to pass a valid image pointer and size.
 *     The adapter can optionally set the major, minor and patch_level fields
 *     to 0, in which case this function returns the acutal version fields
 *     of the firmware loaded. If the adapter sets one of them to a nonzero
 *     value, they have to match with the actual values of the loaded firmware.
 *
 * ifpp_firmware_p (input, output)
 *     Pointer to the firmware data structure for the Input Flow
 *     The adapter has to pass a valid image pointer and size.
 *     The adapter can optionally set the major, minor and patch_level fields
 *     to 0, in which case this function returns the acutal version fields
 *     of the firmware loaded. If the adapter sets one of them to a nonzero
 *     value, they have to match with the actual values of the loaded firmware.
 *     Post-processor micro-engine.
 *
 * opue_firmware_p (input, output)
 *     Pointer to the firmware data structure for the Output Pull-up
 *     micro-engine.
 *     The adapter has to pass a valid image pointer and size.
 *     The adapter can optionally set the major, minor and patch_level fields
 *     to 0, in which case this function returns the acutal version fields
 *     of the firmware loaded. If the adapter sets one of them to a nonzero
 *     value, they have to match with the actual values of the loaded firmware.
 *     Note: Not used in some EIP-207 HW versions.
 *
 * ofpp_firmware_p (input, output)
 *     Pointer to the firmware data structure for the Output Flow
 *     Post-processor micro-engine.
 *     The adapter has to pass a valid image pointer and size.
 *     The adapter can optionally set the major, minor and patch_level fields
 *     to 0, in which case this function returns the acutal version fields
 *     of the firmware loaded. If the adapter sets one of them to a nonzero
 *     value, they have to match with the actual values of the loaded firmware.
 *     Note: Not used in some EIP-207 HW versions.
 *
 * Return value
 *     EIP207_GLOBAL_NO_ERROR
 *     EIP207_GLOBAL_UNSUPPORTED_FEATURE_ERROR
 *     EIP207_GLOBAL_ARGUMENT_ERROR
 *     EIP207_GLOBAL_ILLEGAL_IN_STATE
 *     EIP207_GLOBAL_INTERNAL_ERROR : firmware download failed
 */
eip207_global_error_t
eip207_global_firmware_load(
        eip207_global_io_area_t * const io_area_p,
        const unsigned int timer_prescaler,
        eip207_firmware_t * const ipue_firmware_p,
        eip207_firmware_t * const ifpp_firmware_p,
        eip207_firmware_t * const opue_firmware_p,
        eip207_firmware_t * const ofpp_firmware_p);


/*----------------------------------------------------------------------------
 * eip207_global_hw_revision_get
 *
 * This function returns EIP-207 hardware revision
 * information in the capabilities_p data structure.
 *
 * io_area_p (output)
 *     Pointer to the IO Area for the Classification Engine.
 *
 * capabilities_p (output)
 *     Pointer to the place holder in memory where the device capability
 *     information will be stored.
 *
 * Return value
 *     EIP207_GLOBAL_NO_ERROR
 *     EIP207_GLOBAL_ARGUMENT_ERROR
 *     EIP207_GLOBAL_ILLEGAL_IN_STATE
 */
eip207_global_error_t
eip207_global_hw_revision_get(
        eip207_global_io_area_t * const io_area_p,
        eip207_global_capabilities_t * const capabilities_p);


/*----------------------------------------------------------------------------
 * eip207_global_global_stats_get
 *
 * This function obtains global statistics for the Classification Engine.
 *
 * io_area_p (output)
 *     Pointer to the IO Area for the Classification Engine.
 *
 * ce_number (input)
 *     number of the CE for which the status must be obtained.
 *
 * global_stats_p (output)
 *     Pointer to the data structure where the global statistics will be stored.
 *
 * Return value
 *     EIP207_GLOBAL_NO_ERROR
 *     EIP207_GLOBAL_ARGUMENT_ERROR
 *     EIP207_GLOBAL_ILLEGAL_IN_STATE
 *     EIP207_GLOBAL_INTERNAL_ERROR : failed to read 64-bit counter value
 */
eip207_global_error_t
eip207_global_global_stats_get(
        eip207_global_io_area_t * const io_area_p,
        const unsigned int ce_number,
        eip207_global_global_stats_t * const global_stats_p);


/*--------------------- -------------------------------------------------------
 * eip207_global_clock_count_get
 *
 * Retrieve the current clock count as used by the Classification Engine.
 *
 * io_area_p (output)
 *     Pointer to the IO Area for the Classification Engine.
 *
 * ce_number (input)
 *     number of the CE for which the status must be obtained.
 *
 * clock_p (output)
 *     Current clock count used by the engine (least significant word).
 *
 * Return value
 *     EIP207_GLOBAL_NO_ERROR
 *     EIP207_GLOBAL_ARGUMENT_ERROR
 *     EIP207_GLOBAL_ILLEGAL_IN_STATE
 *     EIP207_GLOBAL_INTERNAL_ERROR : failed to read 64-bit counter value
 */
eip207_global_error_t
eip207_global_clock_count_get(
        eip207_global_io_area_t * const io_area_p,
        const unsigned int ce_number,
        eip207_global_clock_t * const clock_p);


/*--------------------- -------------------------------------------------------
 * eip207_global_status_get
 *
 * Retrieve the Classification Engine status information.
 *
 * io_area_p (output)
 *     Pointer to the IO Area for the Classification Engine.
 *
 * ce_number (input)
 *     number of the CE for which the status must be obtained.
 *
 * status_p (output)
 *     Pointer to the data structure where the Classification Engine
 *     status information will be stored.
 *
 * f_fatal_error_p (output)
 *     Pointer to memory where the boolean flag will be stored. The flag
 *     is set to true when this function reports one or several fatal errors.
 *     A fatal error requires the engine Global SW or HW Reset.
 *
 * Return value
 *     EIP207_GLOBAL_NO_ERROR
 *     EIP207_GLOBAL_ARGUMENT_ERROR
 *     EIP207_GLOBAL_ILLEGAL_IN_STATE
 */
eip207_global_error_t
eip207_global_status_get(
        eip207_global_io_area_t * const io_area_p,
        const unsigned int ce_number,
        eip207_global_status_t * const status_p,
        bool * const f_fatal_error_p);


#endif /* EIP207_GLOBAL_INIT_H_ */


/* end of file eip207_global_init.h */
