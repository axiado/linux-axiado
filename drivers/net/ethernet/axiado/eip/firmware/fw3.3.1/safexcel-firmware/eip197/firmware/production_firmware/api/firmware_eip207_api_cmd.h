/* firmware_eip207_api_cmd.h
 *
 * EIP-207 Firmware Packet Flow ID API:
 *
 * Defines codes for packet flows and special-case commands.
 * These codes are included in command descriptors (input tokens) supplied
 * to the Classification Engine.
 *
 * This API is defined by the EIP-207 Classification Firmware.
 *
 */

/* -------------------------------------------------------------------------- */
/*                                                                            */
/*   Module        : firmware_eip197                                          */
/*   Version       : 3.3.1                                                    */
/*   configuration : FIRMWARE-GENERIC                                         */
/*                                                                            */
/* Date          : 2021-Jun-11                                                */
/*                                                                            */
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

#ifndef FIRMWARE_EIP207_API_CMD_H_
#define FIRMWARE_EIP207_API_CMD_H_


/*----------------------------------------------------------------------------
 * This module implements (provides) the following interface(s):
 */


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */


/*----------------------------------------------------------------------------
 * Definitions and macros
 */

// Lookaside Crypto packet Flow
#define FIRMWARE_EIP207_CMD_PKT_LAC       0x04

// Lookaside IPsec packet flow
#define FIRMWARE_EIP207_CMD_PKT_LIP       0x02

// Inline IPsec packet flow
#define FIRMWARE_EIP207_CMD_PKT_IIP       0x02

// Lookaside IPsec packet flow, custom classification
#define FIRMWARE_EIP207_CMD_PKT_LIP_CUST  0x03

// Inline IPsec packet flow, custom classification
#define FIRMWARE_EIP207_CMD_PKT_IIP_CUST  0x03

// Lookaside IPsec with Token Builder offload packet flow
#define FIRMWARE_EIP207_CMD_PKT_LAIP_TB   0x18

// Lookaside Basic with Token Builder offload packet flow
#define FIRMWARE_EIP207_CMD_PKT_LAB_TB    0x20

// Inline Basic with Token Builder offload packet flow
#define FIRMWARE_EIP207_CMD_PKT_IB_TB    0x20

// Lookaside DTLS (including CAPWAP)
#define FIRMWARE_EIP207_CMD_PKT_LDT       0x28

// Inline DTLS (including CAPWAP)
#define FIRMWARE_EIP207_CMD_PKT_IDT       0x28

// Inline MACsec
#define FIRMWARE_EIP207_CMD_PKT_IMA       0x38

// Invalidate Transform Record command
#define FIRMWARE_EIP207_CMD_INV_TR        0x06

// Invalidate Flow Record command
#define FIRMWARE_EIP207_CMD_INV_FR        0x05

// Bit mask that enables the flow lookup. Otherwise the transform lookup is performed.
#define FIRMWARE_EIP207_CMD_FLOW_LOOKUP   0x80

#endif /* FIRMWARE_EIP207_API_CMD_H_ */


/* end of file firmware_eip207_api_cmd.h */
