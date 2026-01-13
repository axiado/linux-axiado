/* cs_ddk197.h
 *
 * Top-level DDK-197 product configuration
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

#ifndef CS_DDK197_H_
#define CS_DDK197_H_

// Version string to show at startup
#define DDK_EIP197_VERSION_STRING "DDK-197-BSD v5.7.2"

// Enable if hardware supports 256-bit sequence number masks natively.
#define DDK_EIP197_EIP96_SEQMASK_256

// Enable if hardware supports 1024-bit sequence number masks natively.
//#define DDK_EIP197_EIP96_SEQMASK_1024

// Prevent update information from being appended to the output packet.
// Update information is used for inbound ESP transport: length, protocol,
// checksum.
//#define DDK_EIP197_EIP96_BLOCK_UPDATE_APPEND

// Enable extended error codes from the EIP-96
//#define DDK_EIP197_EXTENDED_ERRORS_ENABLE

// Enable use of meta-data in input and output tokens with the EIP-197 firmware
// Note: The EIP-197 firmware must support this before in order to be enabled
#define DDK_EIP197_FW_IOTOKEN_METADATA_ENABLE

// Ring Control: command Descriptor offset (maximum size), in bytes
//               16 32-bit words, 64 bytes
// Note: consider CPU d-cache line size alignment
//       if the command Ring buffer is in cached memory!
#define DDK_EIP202_CD_OFFSET_BYTE_COUNT     128

// Ring Control: Result Descriptor offset (maximum size), in bytes
//               16 32-bit words, 64 bytes
// Note: consider CPU d-cache line size alignment
//       if the Result Ring buffer is in cached memory!
#define DDK_EIP202_RD_OFFSET_BYTE_COUNT     128


/*----------------------------------------------------------------------------
 * CAUTION: Do not change the parameters below unless
 *          the EIP-197 hardware configuration changes!
 */

// Enable this for specific versions of the firmware that invert the
// f_allow_padding and f_strip_padding bits.

#define IOTOKEN_PADDING_DEFAULT_ON 1

// Enable this for specific versions of the firmware that contain
// TrustSec header parsing.
//#define DDK_EIP197_FW_TRUSTSEC_ENABLE

// EIP-197 Server with ice configuration and optional oce
#define DDK_EIP197_SRV_ICE

// Test Firmware 3.3 features.
#define DDK_EIP197_FW33_FEATURES

// Test Firmware 3.6 features.
//#define DDK_EIP197_FW36_FEATURES

// Firmware supports XFRM API
#define DDK_EIP197_FW_XFRM_API

// Force the CDR clock on to work around issues with descriptor counter
// updates in EIP-202 hardware versions before 2.8.
//#define DDK_EIP197_CDR_COUNTER_UPDATE_WA


#endif // CS_DDK197_H_


/* end of file cs_ddk197.h */
