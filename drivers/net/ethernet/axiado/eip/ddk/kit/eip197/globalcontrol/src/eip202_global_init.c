/* eip202_global_init.c
 *
 * EIP-202 Global Control Driver Library
 * Initialization Module
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

#include "eip202_global_init.h"

/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Default configuration
#include "c_eip97_global.h"

// Driver Framework Basic Definitions API
#include "basic_defs.h"             // u32

// Driver Framework C Run-time Library API
#include "clib.h"                   // zeroinit

// Driver Framework device API
#include "device_types.h"           // device_handle_t

#include "eip202_global_level0.h"   // EIP-202 Level 0 macros

// eip97_interfaces_get
#include "eip97_global_internal.h"

/*----------------------------------------------------------------------------
 * Definitions and macros
 */


/*----------------------------------------------------------------------------
 * Local variables
 */


/*----------------------------------------------------------------------------
 * eip202_global_detect
 */
bool
eip202_global_detect(
        const device_handle_t device)
{
    u32 value;

    value = eip202_read32(device, EIP202_G_REG_VERSION);
    if (!EIP202_REV_SIGNATURE_MATCH( value ))
        return false;

    return true;
}




/*----------------------------------------------------------------------------
 * eip202_global_hw_revision_get
 */
void
eip202_global_hw_revision_get(
        const device_handle_t device,
        eip202_capabilities_t * const capabilities_p)
{
    EIP202_EIP_REV_RD(device,
                      &capabilities_p->eip_number,
                      &capabilities_p->complmt_eip_number,
                      &capabilities_p->hw_patch_level,
                      &capabilities_p->min_hw_revision,
                      &capabilities_p->maj_hw_revision);

    EIP202_OPTIONS_RD(device,
                      &capabilities_p->nof_rings,
                      &capabilities_p->nof_pes,
                      &capabilities_p->f_exp_plf,
                      &capabilities_p->cf_size,
                      &capabilities_p->rf_size,
                      &capabilities_p->host_ifc,
                      &capabilities_p->dma_len,
                      &capabilities_p->hdw,
                      &capabilities_p->tgt_align,
                      &capabilities_p->f_addr64);

    EIP202_OPTIONS2_RD(device,
                       &capabilities_p->nof_la_ifs,
                       &capabilities_p->nof_in_ifs,
                       &capabilities_p->nof_axi_wr_chs,
                       &capabilities_p->nof_axi_rd_clusters,
                       &capabilities_p->nof_axi_rd_cpc);
}


/*----------------------------------------------------------------------------
 * eip202_global_endianness_slave_configure
 *
 * Configure Endianness Conversion method
 * for the EIP-202 slave (MMIO) interface
 *
 */
bool
eip202_global_endianness_slave_configure(
        const device_handle_t device)
{
#ifdef EIP97_GLOBAL_ENABLE_SWAP_REG_DATA
    u32 value;

    // Read and check the revision register
    value = eip202_read32(device, EIP202_G_REG_VERSION);
    if (!EIP202_REV_SIGNATURE_MATCH( value ))
    {
        // No match, try to enable the Slave interface byte swap
        // Must be done via EIP-202 HIA GLobal
        EIP202_MST_CTRL_BYTE_SWAP_UPDATE(device, true);

        // Read and check the revision register again
        value = eip202_read32(device, EIP202_G_REG_VERSION);
        if (!EIP202_REV_SIGNATURE_MATCH( value ))
            // Bail out if still not OK
            return false;
    }

    return true;
#else
    IDENTIFIER_NOT_USED(device);
    return true;
#endif // EIP97_GLOBAL_ENABLE_SWAP_REG_DATA
}


/*----------------------------------------------------------------------------
 * eip202_global_init
 */
void
eip202_global_init(
        const device_handle_t device,
        unsigned int nof_pe,
        unsigned int nof_la,
        u8 ipbuf_min,
        u8 ipbuf_max,
        u8 itbuf_min,
        u8 itbuf_max,
        u8 opbuf_min,
        u8 opbuf_max)
{
    unsigned int i;
    u8 buffer_ctrl;
    unsigned int nof_p_es,nof_rings,nof_l_as,nof_in,dfedse_offset;
    eip97_interfaces_get(&nof_p_es,&nof_rings,&nof_l_as,&nof_in);
    dfedse_offset = eip97_dfedse_offset_get();

    // Configure EIP-202 HIA Global
    EIP202_MST_CTRL_BUS_BURST_SIZE_UPDATE(device,
                                          EIP97_GLOBAL_BUS_BURST_SIZE,
                                          EIP97_GLOBAL_RX_BUS_BURST_SIZE);
    EIP202_MST_CTRL_BUS_TIMEOUT_UPDATE(device,
                                       EIP97_GLOBAL_TIMEOUT_VALUE);
    if (nof_la)
        // User-configured value
        buffer_ctrl = EIP97_GLOBAL_DSE_BUFFER_CTRL;
    else
        // Default register reset value
        buffer_ctrl = (u8)EIP202_DSE_BUFFER_CTRL;

    for (i = 0; i < nof_pe; i++)
    {
        // Configure EIP-202 HIA DFE Global
        EIP202_DFE_CFG_WR(device,
                          dfedse_offset,
                          i,
                          ipbuf_min,
                          EIP97_GLOBAL_DFE_DATA_CACHE_CTRL,
                          ipbuf_max,
                          itbuf_min,
                          EIP97_GLOBAL_DFE_CTRL_CACHE_CTRL,
                          itbuf_max,
                          (EIP97_GLOBAL_DFE_ADV_THRESH_MODE_FLAG == 1),
                          (EIP97_GLOBAL_DFE_AGGRESSIVE_DMA_FLAG == 1));

        // Configure EIP-202 HIA DSE Global
        EIP202_DSE_CFG_WR(device,
                          dfedse_offset,
                          i,
                          opbuf_min,
                          EIP97_GLOBAL_DSE_DATA_CACHE_CTRL,
                          opbuf_max,
                          buffer_ctrl,
                          (EIP97_GLOBAL_DSE_ENABLE_SINGLE_WR_FLAG == 1),
                          (EIP97_GLOBAL_DSE_AGGRESSIVE_DMA_FLAG == 1));

    }

    // Configure HIA Look-aside fifo
    EIP202_LASIDE_BASE_ADDR_LO_WR(device,
                                  EIP202_LASIDE_DSCR_BYTE_SWAP_METHOD);

    for (i = EIP97_GLOBAL_LAFIFO_RING_ID;
         i < nof_l_as +
             EIP97_GLOBAL_LAFIFO_RING_ID;
         i++)
    {
        EIP202_LASIDE_SLAVE_CTRL_WR(device,
                                    i,
                                    EIP202_LASIDE_IN_PKT_BYTE_SWAP_METHOD,
                                    EIP202_LASIDE_IN_PKT_PROTO,
                                    EIP202_LASIDE_TOKEN_BYTE_SWAP_METHOD,
                                    EIP202_LASIDE_TOKEN_PROTO,
                                    true); // Clear cmd descriptor error

        EIP202_LASIDE_MASTER_CTRL_WR(device,
                                     i,
                                     EIP202_LASIDE_OUT_PKT_BYTE_SWAP_METHOD,
                                     EIP202_LASIDE_OUT_PKT_PROTO,
                                     true); // Clear res descriptor error
    }
    // Configure HIA Inline fifo
    for (i = 0; i < nof_in; i++)
        EIP202_INLINE_CTRL_WR(device,
                              i,
                              EIP202_INLINE_IN_PKT_BYTE_SWAP_METHOD,
                              false, // Clear protocol error
                              EIP202_INLINE_OUT_PKT_BYTE_SWAP_METHOD,
                              opbuf_min,
                              opbuf_max,
                              EIP202_INLINE_BURST_SIZE,
                              EIP202_INLINE_FORCE_INORDER);

}


/*----------------------------------------------------------------------------
 * eip202_global_reset
 */
bool
eip202_global_reset(
        const device_handle_t device,
        const unsigned int nof_pe)
{
    unsigned int i;
    unsigned int dfedse_offset;
    dfedse_offset = eip97_dfedse_offset_get();

    // Restore the EIP-202 default configuration
    // Resets DFE thread and clears ring assignment
    for (i = 0; i < nof_pe; i++)
        EIP202_DFE_TRD_CTRL_WR(device, dfedse_offset, i, 0, false, true);

    // HIA DFE defaults
    for (i = 0; i < nof_pe; i++)
        EIP202_DFE_CFG_DEFAULT_WR(device, dfedse_offset, i);

#ifndef EIP202_RA_DISABLE
    EIP202_RA_PRIO_0_DEFAULT_WR(device);
    EIP202_RA_PRIO_1_DEFAULT_WR(device);
    EIP202_RA_PRIO_2_DEFAULT_WR(device);
    EIP202_RA_PRIO_3_DEFAULT_WR(device);

    // Resets ring assignment
    for (i = 0; i < nof_pe; i++)
        EIP202_RA_PE_CTRL_WR(device, i, 0, false, true);
#endif // #ifndef EIP202_RA_DISABLE

    // Resets DSE thread and clears ring assignment
    for (i = 0; i < nof_pe; i++)
        EIP202_DSE_TRD_CTRL_WR(device, dfedse_offset, i, 0, false, true);

    // HIA DSE defaults
    for (i = 0; i < nof_pe; i++)
        EIP202_DSE_CFG_DEFAULT_WR(device, dfedse_offset, i);


    // HIA LASIDE defaults
    EIP202_LASIDE_BASE_ADDR_LO_DEFAULT_WR(device);
    EIP202_LASIDE_BASE_ADDR_HI_DEFAULT_WR(device);
    for (i = EIP97_GLOBAL_LAFIFO_RING_ID;
         i < EIP97_GLOBAL_MAX_NOF_LAFIFO_TO_USE + EIP97_GLOBAL_LAFIFO_RING_ID;
         i++)
    {
        EIP202_LASIDE_MASTER_CTRL_DEFAULT_WR(device, i);
        EIP202_LASIDE_SLAVE_CTRL_DEFAULT_WR(device, i);
    }

    // HIA INLINE defaults
    for (i = 0; i < nof_pe; i++)
        EIP202_INLINE_CTRL_DEFAULT_WR(device, i);
    return true;
}


/*----------------------------------------------------------------------------
 * eip202_global_reset_is_done
 */
bool
eip202_global_reset_is_done(
        const device_handle_t device,
        const unsigned int p_enr)
{
    u16 fifo_word_count, dma_byte_count;
    unsigned int dfedse_offset;
    bool f_token_dma_busy, f_data_dma_busy, f_dma_error;
    dfedse_offset = eip97_dfedse_offset_get();

    // Check for completion of all DMA transfers
    EIP202_DFE_TRD_STAT_RD(device,
                           dfedse_offset,
                           p_enr,
                           &fifo_word_count,
                           &dma_byte_count,
                           &f_token_dma_busy,
                           &f_data_dma_busy,
                           &f_dma_error);
    if(!f_token_dma_busy && !f_data_dma_busy)
    {
        EIP202_DSE_TRD_STAT_RD(device,
                               dfedse_offset,
                               p_enr,
                               &fifo_word_count,
                               &dma_byte_count,
                               &f_token_dma_busy,
                               &f_data_dma_busy,
                               &f_dma_error);
        if(!f_token_dma_busy && !f_data_dma_busy)
        {
            // Take DFE thread out of reset
            EIP202_DFE_TRD_CTRL_DEFAULT_WR(device, dfedse_offset, p_enr);

            // Take DSE thread out of reset
            EIP202_DSE_TRD_CTRL_DEFAULT_WR(device, dfedse_offset, p_enr);

            // Do not restore the EIP-202 Master Control default configuration
            // so this will not change the endianness conversion configuration
            // for the Slave interface
            return true;

        }
        else
        {
            return false;
        }
    }
    else
    {
        return false;
    }
}


/*----------------------------------------------------------------------------
 * eip202_global_configure
 */
void
eip202_global_configure(
        const device_handle_t device,
        const unsigned int pe_number,
        const eip202_global_ring_pe_map_t * const ring_pe_map_p)
{
    unsigned int dfedse_offset;
    dfedse_offset = eip97_dfedse_offset_get();
    // Disable EIP-202 HIA DFE thread(s)
    EIP202_DFE_TRD_CTRL_WR(device,
                           dfedse_offset,
                           pe_number,   // Thread nr
                           0,
                           false,       // Disable thread
                           false);      // Do not reset thread

    // Disable EIP-202 HIA DSE thread(s)
    EIP202_DSE_TRD_CTRL_WR(device,
                           dfedse_offset,
                           pe_number,   // Thread nr
                           0,
                           false,       // Disable thread
                           false);      // Do not reset thread

#ifndef EIP202_RA_DISABLE
    // Configure the HIA Ring Arbiter
    EIP202_RA_PRIO_0_WR(
            device,
            (ring_pe_map_p->ring_prio_mask & BIT_0) == 0 ? false : true,
            (u8)(ring_pe_map_p->ring_slots0 & MASK_4_BITS),
            (ring_pe_map_p->ring_prio_mask & BIT_1) == 0 ? false : true,
            (u8)((ring_pe_map_p->ring_slots0 >> 4) & MASK_4_BITS),
            (ring_pe_map_p->ring_prio_mask & BIT_2) == 0 ? false : true,
            (u8)((ring_pe_map_p->ring_slots0 >> 8) & MASK_4_BITS),
            (ring_pe_map_p->ring_prio_mask & BIT_3) == 0 ? false : true,
            (u8)((ring_pe_map_p->ring_slots0 >> 12) & MASK_4_BITS));

    EIP202_RA_PRIO_1_WR(
            device,
            (ring_pe_map_p->ring_prio_mask & BIT_4) == 0 ? false : true,
            (u8)((ring_pe_map_p->ring_slots0 >> 16) & MASK_4_BITS),
            (ring_pe_map_p->ring_prio_mask & BIT_5) == 0 ? false : true,
            (u8)((ring_pe_map_p->ring_slots0 >> 20) & MASK_4_BITS),
            (ring_pe_map_p->ring_prio_mask & BIT_6) == 0 ? false : true,
            (u8)((ring_pe_map_p->ring_slots0 >> 24) & MASK_4_BITS),
            (ring_pe_map_p->ring_prio_mask & BIT_7) == 0 ? false : true,
            (u8)((ring_pe_map_p->ring_slots0 >> 28) & MASK_4_BITS));

    EIP202_RA_PRIO_2_WR(
            device,
            (ring_pe_map_p->ring_prio_mask & BIT_8) == 0 ? false : true,
            (u8)(ring_pe_map_p->ring_slots1 & MASK_4_BITS),
            (ring_pe_map_p->ring_prio_mask & BIT_9) == 0 ? false : true,
            (u8)((ring_pe_map_p->ring_slots1 >> 4) & MASK_4_BITS),
            (ring_pe_map_p->ring_prio_mask & BIT_10) == 0 ? false : true,
            (u8)((ring_pe_map_p->ring_slots1 >> 8) & MASK_4_BITS),
            (ring_pe_map_p->ring_prio_mask & BIT_11) == 0 ? false : true,
            (u8)((ring_pe_map_p->ring_slots1 >> 12) & MASK_4_BITS));

    EIP202_RA_PRIO_3_WR(
            device,
            (ring_pe_map_p->ring_prio_mask & BIT_12) == 0 ? false : true,
            (u8)((ring_pe_map_p->ring_slots1 >> 16) & MASK_4_BITS),
            (ring_pe_map_p->ring_prio_mask & BIT_13) == 0 ? false : true,
            (u8)((ring_pe_map_p->ring_slots1 >> 20) & MASK_4_BITS),
            (ring_pe_map_p->ring_prio_mask & BIT_14) == 0 ? false : true,
            (u8)((ring_pe_map_p->ring_slots1 >> 24) & MASK_4_BITS));

    // Ring assignment in the Ring Arbiter
    EIP202_RA_PE_CTRL_WR(device,
                         pe_number,
                         ring_pe_map_p->ring_pe_mask,
                         true,
                         false);
#endif // #ifndef EIP202_RA_DISABLE

    {
        // Assign Rings to this DFE thread
        // Enable EIP-202 HIA DFE thread(s)
        EIP202_DFE_TRD_CTRL_WR(device,
                               dfedse_offset,
                               pe_number,   // Thread nr
                               ring_pe_map_p->ring_pe_mask,  // Rings to assign
                               true,        // Enable thread
                               false);      // Do not reset thread

        // Assign Rings to this DSE thread
        // Enable EIP-202 HIA DSE thread(s)
        EIP202_DSE_TRD_CTRL_WR(device,
                               dfedse_offset,
                               pe_number,   // Thread nr
                               ring_pe_map_p->ring_pe_mask,   // Rings to assign
                               true,        // Enable thread
                               false);      // Do not reset thread
    }
}

/* end of file eip202_global_init.c */
