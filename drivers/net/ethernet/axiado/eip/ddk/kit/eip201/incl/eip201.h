/* eip201.h
 *
 * EIP201 Driver Library API
 *
 * Security-IP-201 is the Advanced Interrupt Controller (aic)
 */

/*****************************************************************************
* Copyright (c) 2007-2020 by Rambus, Inc. and/or its subsidiaries.
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

#ifndef INCLUDE_GUARD_EIP201_H
#define INCLUDE_GUARD_EIP201_H

// Basic definitions
#include "basic_defs.h"          // u32, bool, static inline, BIT_* etc.
// device API definitions
#include "device_types.h"        // device_handle_t

// The API's table of contents:
//
//  eip201_status_t - error codes data type
//  eip201_source_t - interrupt source (irq) data type
//  EIP201_Config API - to control individual configurations
//  EIP201_SourceMask API - to control individual interrupt masks (masking)
//  EIP201_SourceStatus API - to retrieve individual interrupt statuses
//  eip201_initialize API - to initialize the EIP201 device by providing
//                          initial interrupt polarities and masks
//                          just in one call
//  eip201_acknowledge - to acknowledge individual interrupts


/*----------------------------------------------------------------------------
 * list of EIP-201 error codes that API functions can return.
 * EIP201_STATUS_UNSUPPORTED_IRQ can be returned if a concrete EIP device
 * does not support an interrupt source number provided in a bitmap
 * as a function argument.
 *
 * Any other integer value: error return from device read or write function.
 */
enum
{
    EIP201_STATUS_SUCCESS = 0,
    EIP201_STATUS_UNSUPPORTED_IRQ,
    EIP201_STATUS_UNSUPPORTED_HARDWARE_VERSION,
};
typedef int eip201_status_t;


/*----------------------------------------------------------------------------
 * list of EIP-201 interrupt sources
 * Maximum number of sources is 32.
 */

// a single EIP201 interrupt source, typically BIT_xx
typedef u32 eip201_source_t;

// An OR-ed combination of EIP201 interrupt sources
typedef u32 eip201_source_bitmap_t;

// A handy macro for all interrupt sources mask
#define EIP201_SOURCE_ALL  (eip201_source_bitmap_t)(~0)

//Note1:
//  In API functions to follow the first parameter often is
//  device_handle_t device.
//  This is a context object for the Driver Framework device API
//  implementation.
//  This context must be unique for each instance of each driver
//  to allow selection of a specific EIP device (instance).
//  It is important that the device_handle_t instance describes a
//  valid EIP-201 device (HW block).

//Note2:
//  In API functions to follow the second parameter sometimes is
//  const eip201_source_bitmap_t sources.
//  This is always a set of interrupt sources, for which some operation
//  has to be performed.
//  If an interrupt source is not included in the eip201_source_bitmap_t
//  instance, then the operation will not be performed for this source
//  (corresponding bit is not changed in a HW register).

//Note3:
//  If not stated otherwise, all API functions are re-entrant and can be
//  called concurrently with other API functions.

/*----------------------------------------------------------------------------
 * EIP201_Config API
 *
 * Controls configuration of individual interrupt sources:
 * - Falling Edge or Rising Edge (edge based)
 * - Active High or Active Low (level based)
 *
 * Usage:
 *     eip201_config_t Source0Config;
 *
 *     eip201_config_change(
 *             device,
 *             BIT_0,
 *             EIP201_CONFIG_ACTIVE_LOW);
 *
 *     eip201_config_change(
 *             device,
 *             BIT_1 | BIT_2,
 *             EIP201_CONFIG_RISING_EDGE);
 *
 *     Source0Config = eip201_config_read(device, BIT_0);
 */
typedef enum
{
    EIP201_CONFIG_ACTIVE_LOW = 0,
    EIP201_CONFIG_ACTIVE_HIGH,
    EIP201_CONFIG_FALLING_EDGE,
    EIP201_CONFIG_RISING_EDGE
} eip201_config_t;

/* eip201_config_change function is not re-entrant and
   cannot be called concurrently with API functions:
        eip201_initialize
*/
eip201_status_t
eip201_config_change(
        device_handle_t device,
        const eip201_source_bitmap_t sources,
        const eip201_config_t new_config);

// source can only have one bit set
eip201_config_t
eip201_config_read(
        device_handle_t device,
        const eip201_source_t source);


/*----------------------------------------------------------------------------
 * EIP201_SourceMask API
 *
 * Allows masking/unmasking individual interrupt sources.
 *
 * Usage:
 *     bool Source0IsEnabled;
 *     eip201_source_bitmap_t AllEnabledSources;
 *     eip201_source_mask_enable_source(device, BIT_1 + BIT_0);
 *     eip201_source_mask_disable_source(device, BIT_5 + BIT_2);
 *     Source0IsEnabled = eip201_source_mask_source_is_enabled(device, BIT_0);
 *     AllEnabledSources = eip201_source_mask_read_all(device);
 */

/* eip201_source_mask_enable_source
*/
eip201_status_t
eip201_source_mask_enable_source(
        device_handle_t device,
        const eip201_source_bitmap_t sources);

/* eip201_source_mask_disable_source
*/
eip201_status_t
eip201_source_mask_disable_source(
        device_handle_t device,
        const eip201_source_bitmap_t sources);

// source can only have one bit set
bool
eip201_source_mask_source_is_enabled(
        device_handle_t device,
        const eip201_source_t source);

// Returns a bitmask for all enabled sources.
// In this bitmask:
//      0 - an interrupt source is disabled
//      1 - an interrupt source is enabled
eip201_source_bitmap_t
eip201_source_mask_read_all(
        device_handle_t device);


/*----------------------------------------------------------------------------
 * EIP201_SourceStatus API
 *
 * Allows reading status of individual interrupt sources.
 * _IsEnabledSourcePending -
 *     reads the status of the source after a source mask is applied.
 * _IsRawSourcePending -
 *     reads the status of the raw source (no source mask is applied).
 *
 * Usage:
 *     bool Source0EnabledStatus;
 *     bool Source0RawStatus;
 *     eip201_source_bitmap_t AllEnabledStatuses;
 *     eip201_source_bitmap_t AllRawStatuses;
 *     Source0EnabledStatus =
 *         eip201_source_status_is_enabled_source_pending(device, BIT_0);
 *     Source0RawStatus =
 *         eip201_source_status_is_raw_source_pending(device, BIT_0);
 *     AllEnabledStatuses = eip201_source_status_read_all_enabled(device);
 *     AllRawStatuses = eip201_source_status_read_all_raw(device);
 */
// source can only have one bit set
bool
eip201_source_status_is_enabled_source_pending(
        device_handle_t device,
        const eip201_source_t source);

// source can only have one bit set
bool
eip201_source_status_is_raw_source_pending(
        device_handle_t device,
        const eip201_source_t source);

// Returns a bitmask for current statuses of all enabled sources.
// (after a source mask is applied)
// In this bitmask:
//      0 - an enabled interrupt source is not pending (inactive).
//      1 - an enabled interrupt source is pending (active).
eip201_source_bitmap_t
eip201_source_status_read_all_enabled(
        device_handle_t device);

// Returns a bitmask for current statuses of all raw sources.
// (no source mask is applied)
// In this bitmask:
//      0 - a raw interrupt source is not pending (inactive).
//      1 - a raw interrupt source is pending (active).
eip201_source_bitmap_t
eip201_source_status_read_all_raw(
        device_handle_t device);


// Returns a bitmask for current statuses of all enabled sources.
// (after a source mask is applied). Also return a status.
// In this bitmask:
//      0 - an enabled interrupt source is not pending (inactive).
//      1 - an enabled interrupt source is pending (active).
eip201_status_t
eip201_source_status_read_all_enabled_check(
        device_handle_t device,
        eip201_source_bitmap_t * const statuses_p);

// Returns a bitmask for current statuses of all raw sources.
// (no source mask is applied). Also return a status.
// In this bitmask:
//      0 - a raw interrupt source is not pending (inactive).
//      1 - a raw interrupt source is pending (active).
eip201_status_t
eip201_source_status_read_all_raw_check(
        device_handle_t device,
        eip201_source_bitmap_t * const statuses_p);


/*----------------------------------------------------------------------------
 * eip201_initialize API
 *
 * Initializes the EIP201 interrupt controller device.
 *
 *     settings_array_p
 *         Initial interrupt settings for a number of interrupt sources.
 *         Can be NULL. If NULL, all settings are device default.
 *
 *     settings_count
 *         number of interrupt sources, for which settings are given.
 *         Can be 0. If 0, all settings are device default.
 *
 * Usage:
 *     eip201_source_settings_t MySettings[] =
 *     {
 *         {BIT_1, EIP201_CONFIG_ACTIVE_LOW,  false},
 *         {BIT_2, EIP201_CONFIG_ACTIVE_HIGH, true}
 *     };
 *
 *     eip201_initialize(device, MySettings, 2);
 */
typedef struct
{
    eip201_source_t source;  // for which interrupt source the settings are
    eip201_config_t config;
    bool f_enable;
} eip201_source_settings_t;

/* eip201_initialize function is not re-entrant and
   cannot be called concurrently with API functions:
        eip201_config_change
*/
eip201_status_t
eip201_initialize(
        device_handle_t device,
        const eip201_source_settings_t * settings_array_p,
        const unsigned int settings_count);


/*----------------------------------------------------------------------------
 * eip201_acknowledge
 *
 * Acknowledges the EIP201 interrupts.
 *
 * Usage:
 *     eip201_acknowledge(device, BIT_0 | BIT_1);
 */
eip201_status_t
eip201_acknowledge(
        device_handle_t device,
        const eip201_source_bitmap_t sources);

#endif /* INCLUDE_GUARD_EIP201_H */

/* end of file eip201.h */
