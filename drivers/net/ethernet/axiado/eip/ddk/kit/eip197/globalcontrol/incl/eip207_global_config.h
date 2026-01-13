/* eip207_global_config.h
 *
 * EIP-207 Global Control Driver Library
 * configuration Module
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

#ifndef EIP207_GLOBAL_CONFIG_H_
#define EIP207_GLOBAL_CONFIG_H_


/*----------------------------------------------------------------------------
 * This module implements (provides) the following interface(s):
 */


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Driver Framework Basic Definitions API
#include "basic_defs.h"                 // bool

// Driver Framework device API
#include "device_types.h"               // device_handle_t

// data type for firmware config
#include "eip207_global_init.h"

// EIP-207 Global Control Driver Library internal interfaces
#include "eip207_level0.h"              // EIP207_ICE_REG_SCRATCH_RAM

#include "eip207_hw_interface_ext.h"    // EIP207_OCE_REG_SCRATCH_RAM

// EIP-207c Firmware Classification API
#include "firmware_eip207_api_cs.h"     // FIRMWARE_EIP207_CS_*


/*----------------------------------------------------------------------------
 * Definitions and macros
 */

/*----------------------------------------------------------------------------
 * Local variables
 */


/*--------------------- -------------------------------------------------------
 * eip207_global_firmware_configure
 */
static inline void
eip207_global_firmware_configure(
        const device_handle_t device,
        const unsigned int ce_number,
        const eip207_firmware_config_t * const fw_config_p)
{
    unsigned int byte_offset;
#ifdef FIRMWARE_EIP207_CS_ADMIN_RAM_DTLS_HDR_SIZE_BYTE_OFFSET
    u32 value;
#endif
    // Set input and output token format (with or without meta-data)
    byte_offset = FIRMWARE_EIP207_CS_ADMIN_RAM_TOKEN_FORMAT_BYTE_OFFSET;

    eip207_write32(device,
                   EIP207_ICE_REG_SCRATCH_RAM(ce_number) + byte_offset,
                   fw_config_p->f_token_extensions_enable ? 1 : 0);
#ifdef FIRMWARE_EIP207_CS_ADMIN_RAM_DTLS_HDR_SIZE_BYTE_OFFSET
    byte_offset = FIRMWARE_EIP207_CS_ADMIN_RAM_DTLS_HDR_SIZE_BYTE_OFFSET;
    value = fw_config_p->dtls_record_header_align;
    if (fw_config_p->f_dtls_defer_ccs)
        value |= BIT_15;
    if (fw_config_p->f_dtls_defer_alert)
        value |= BIT_13;
    if (fw_config_p->f_dtls_defer_handshake)
        value |= BIT_13;
    if (fw_config_p->f_dtls_defer_app_data)
        value |= BIT_12;
    if (fw_config_p->f_dtls_defer_capwap)
        value |= BIT_11;
    eip207_write32(device,
                   EIP207_ICE_REG_SCRATCH_RAM(ce_number) + byte_offset,
                   value);
#endif
//eip_common_cfg() does this as of now..  but neverthless
#ifdef EIP207_OCE_REG_SCRATCH_RAM // Test if we have this scratch RAM., this works with ABC(n) #ifdef ABC ok!
#ifdef FIRMWARE_EIP207_CS_ADMIN_RAM_PKTID_BYTE_OFFSET
        byte_offset = FIRMWARE_EIP207_CS_ADMIN_RAM_PKTID_BYTE_OFFSET;
        eip207_write32(device,
                       EIP207_OCE_REG_SCRATCH_RAM(ce_number) + byte_offset,
                       fw_config_p->f_increment_pkt_id ?
                       fw_config_p->pkt_id | BIT_16 :
                       0);
#endif
#ifdef FIRMWARE_EIP207_CS_ADMIN_RAM_ECN_CONTROL_BYTE_OFFSET
        byte_offset = FIRMWARE_EIP207_CS_ADMIN_RAM_ECN_CONTROL_BYTE_OFFSET;
        eip207_write32(device,
                       EIP207_OCE_REG_SCRATCH_RAM(ce_number) + byte_offset,
                       fw_config_p->ecn_control);
#endif
#ifdef FIRMWARE_EIP207_CS_ADMIN_RAM_REDIR_BYTE_OFFSET
        byte_offset = FIRMWARE_EIP207_CS_ADMIN_RAM_REDIR_BYTE_OFFSET;
        eip207_write32(device,
                       EIP207_OCE_REG_SCRATCH_RAM(ce_number) + byte_offset,
                       fw_config_p->redir_ring |
                       (fw_config_p->f_redir_ring_enable << 16) |
                       (fw_config_p->f_always_redirect << 17));
#endif
#endif

//ice-redirection is enabled, all good for ice 12/10/25
#ifdef FIRMWARE_EIP207_CS_ADMIN_RAM_REDIR_BYTE_OFFSET
        byte_offset = FIRMWARE_EIP207_CS_ADMIN_RAM_REDIR_BYTE_OFFSET;
        eip207_write32(device,
                       EIP207_ICE_REG_SCRATCH_RAM(ce_number) + byte_offset,
                       fw_config_p->transform_redir_enable);//This is okay
#endif
#ifdef FIRMWARE_EIP207_CS_ADMIN_RAM_RPS_RINGS1_BYTE_OFFSET   //this check also false...
        if ((fw_config_p->rps_ring_enable & MASK_16_BITS) == 0)
        {
            byte_offset = FIRMWARE_EIP207_CS_ADMIN_RAM_RPS_RINGS1_BYTE_OFFSET;
            eip207_write32(device,
                           EIP207_OCE_REG_SCRATCH_RAM(ce_number) + byte_offset,
                           0);
        }
        else
        {
            // Corrections to the values when doubling to make them agree
            // with table 11 "Division constants for RPS" in FWRM section 4.5.3
            static const u8 sub_corr[16]={0,0,1,0,3,2,2,0,
                                               1,6,5,5,2,5,7,0};
            unsigned int i,n,ifc_idx;
            u32 rings1,rings2;
            /* count the number of bits in n */
            n=0;
            for (i=0; i<16; i++) {
                if ((fw_config_p->rps_ring_enable & (1<<i)) != 0 ) {
                    n++;
                }
            }
            /* Fill in divison constants, Avoid 64-bit division as ARM kernel
               mode does not have it in its C library. */
            value = 0xffffffff / n + 1;
            for (i=0; i<4; i++) {
                byte_offset = FIRMWARE_EIP207_CS_ADMIN_RAM_RPS_DIVISOR0_BYTE_OFFSET + 4*i;
                eip207_write32(device,
                               EIP207_OCE_REG_SCRATCH_RAM(ce_number) + byte_offset,
                               value);
                if (value & 0x80000000) {
                    value = 0;
                } else {
                    value = 2*value - ((sub_corr[n-1] >> i) & 1);
                }
            }
            rings1=0;
            rings2=0;
            /* Allocate interfaces to slots */
            /* Start at the index given by table 12 "Selection of Ring
               ID Slots for RPS"  of FWRM section 4.5.3 */
            if (n>8) {
                ifc_idx = 16-n;
            } else if (n>4) {
                ifc_idx = 8-n;
            } else if (n>2) {
                ifc_idx = 4-n;
            } else {
                ifc_idx = 0;
            }
            /* Fill in the rings values */
            for (i=0; i<16; i++) {
                if ((fw_config_p->rps_ring_enable & (1<<i)) != 0 ) {
                    if (ifc_idx>=8)
                        rings2 |= i<<(4*(ifc_idx-8));
                    else
                        rings1 |= i<<(4*ifc_idx);
                    ifc_idx++;
                }
            }
            /* Rotate the rings values 4 bits to the right words to put the
               slots in the correct positions. */
            rings1 = (rings1<<28) | (rings1>>4);
            rings2 = (rings2<<28) | (rings2>>4);
            if (rings1 == 0) {
                rings1 |= 1; /* Make sure rings1 value is not equal to 0 */
            }
            byte_offset = FIRMWARE_EIP207_CS_ADMIN_RAM_RPS_RINGS1_BYTE_OFFSET;
            eip207_write32(device,
                           EIP207_OCE_REG_SCRATCH_RAM(ce_number) + byte_offset,
                           rings1);
            byte_offset = FIRMWARE_EIP207_CS_ADMIN_RAM_RPS_RINGS2_BYTE_OFFSET;
            eip207_write32(device,
                           EIP207_OCE_REG_SCRATCH_RAM(ce_number) + byte_offset,
                           rings2);
        }
#endif
}


#endif // EIP207_GLOBAL_CONFIG_H_


/* end of file eip207_global_config.h */
