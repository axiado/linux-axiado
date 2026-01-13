/* eip207_ice.h
 *
 * EIP-207c Input Classification Engine (ice) interface
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

#ifndef EIP207_ICE_H_
#define EIP207_ICE_H_


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// EIP-207 Global Control Init API
#include "eip207_global_init.h"         // EIP207_*


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Driver Framework Basic Definitions API
#include "basic_defs.h"                 // bool, u32

// Driver Framework device API
#include "device_types.h"               // device_handle_t


/*----------------------------------------------------------------------------
 * Definitions and macros
 */


/*----------------------------------------------------------------------------
 * eip207_ice_firmware_load
 */
eip207_global_error_t
eip207_ice_firmware_load(
        const device_handle_t device,
        const unsigned int timer_prescaler,
        eip207_firmware_t * const pue_firmware_p,
        eip207_firmware_t * const fpp_firmware_p);


/*----------------------------------------------------------------------------
 * eip207_ice_global_stats_get
 */
eip207_global_error_t
eip207_ice_global_stats_get(
        const device_handle_t device,
        const unsigned int ce_number,
        eip207_global_ice_global_stats_t * const ice_global_stats_p);


/*----------------------------------------------------------------------------
 * eip207_ice_clock_count_get
 */
eip207_global_error_t
eip207_ice_clock_count_get(
        const device_handle_t device,
        const unsigned int ce_number,
        eip207_global_value64_t * const ice_clock_p);


/*-----------------------------------------------------------------------------
 * eip207_ice_status_get
 */
eip207_global_error_t
eip207_ice_status_get(
        const device_handle_t device,
        const unsigned int ce_number,
        eip207_global_ce_status_t * const ice_status_p);


/* end of file eip207_ice.h */


#endif /* EIP207_ICE_H_ */
