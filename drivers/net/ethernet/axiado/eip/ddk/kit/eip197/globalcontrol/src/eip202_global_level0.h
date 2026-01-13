/* eip202_global_level0.h
 *
 * EIP-202 HIA Global Control Level0 internal interface
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

#ifndef EIP202_GLOBAL_LEVEL0_H_
#define EIP202_GLOBAL_LEVEL0_H_


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Default configuration
#include "c_eip97_global.h"

// EIP-292 Global Control HW interface
#include "eip202_global_hw_interface.h"

// Driver Framework Basic Definitions API
#include "basic_defs.h"         // BIT definitions, bool, u32

// Driver Framework device API
#include "device_types.h"       // device_handle_t
#include "device_rw.h"          // Read32, Write32
#include "device_swap.h"        // device_swap_endian32


/*----------------------------------------------------------------------------
 * Definitions and macros
 */


/*----------------------------------------------------------------------------
 * Local variables
 */


/*----------------------------------------------------------------------------
 * eip202_set_slave_byte_order_swap_word32
 *
 * Helper function that can be used to swap words in the body of the
 * EIP202_Set_Slave_Byte_Order() functions as well as in the other
 * functions where words require byte swap before this can be done
 * by the Packet Engine Slave interface
  */
static inline void
eip202_set_slave_byte_order_swap_word32(u32* const word32)
{
#ifndef EIP97_GLOBAL_DISABLE_HOST_SWAP_INIT

    u32 tmp = device_swap_endian32(*word32);

    *word32 = tmp;

#else
    IDENTIFIER_NOT_USED(word32);
#endif // !EIP97_GLOBAL_DISABLE_HOST_SWAP_INIT
}


/*----------------------------------------------------------------------------
 * EIP202 Global Functions
 *
 */

/*----------------------------------------------------------------------------
 * eip202_read32
 *
 * This routine writes to a Register location in the EIP-202.
 */
static inline u32
eip202_read32(
        device_handle_t device,
        const unsigned int offset)
{
    return device_read32(device, offset);
}


/*----------------------------------------------------------------------------
 * eip202_write32
 *
 * This routine writes to a Register location in the EIP-202.
 */
static inline void
eip202_write32(
        device_handle_t device,
        const unsigned int offset,
        const u32 value)
{
    device_write32(device, offset, value);
}


static inline bool
EIP202_REV_SIGNATURE_MATCH(
        const u32 rev)
{
    return (((u16)rev) == EIP202_SIGNATURE);
}


static inline void
EIP202_EIP_REV_RD(
        device_handle_t device,
        u8 * const eip_number,
        u8 * const complmt_eip_number,
        u8 * const hw_patch_level,
        u8 * const min_hw_revision,
        u8 * const maj_hw_revision)
{
    u32 rev_reg_val;

    rev_reg_val = eip202_read32(device, EIP202_G_REG_VERSION);

    *maj_hw_revision     = (u8)((rev_reg_val >> 24) & MASK_4_BITS);
    *min_hw_revision     = (u8)((rev_reg_val >> 20) & MASK_4_BITS);
    *hw_patch_level      = (u8)((rev_reg_val >> 16) & MASK_4_BITS);
    *complmt_eip_number  = (u8)((rev_reg_val >> 8)  & MASK_8_BITS);
    *eip_number         = (u8)((rev_reg_val)       & MASK_8_BITS);
}


static inline void
EIP202_OPTIONS_RD(
        device_handle_t device,
        u8 * const nof_rings,
        u8 * const nof_pes,
        bool * const f_exp_plf,
        u8 * const cf_size,
        u8 * const rf_size,
        u8 * const host_ifc,
        u8 * const dma_len,
        u8 * const hdw,
        u8 * const tgt_align,
        bool * const f_addr64)
{
    u32 rev_reg_val;

    rev_reg_val = eip202_read32(device, EIP202_G_REG_OPTIONS);

    *f_addr64   = ((rev_reg_val & BIT_31) != 0);
    *tgt_align  = (u8)((rev_reg_val >> 28) & MASK_3_BITS);
    *hdw       = (u8)((rev_reg_val >> 25) & MASK_3_BITS);
    *dma_len   = (u8)((rev_reg_val >> 20) & MASK_5_BITS);
    *host_ifc   = (u8)((rev_reg_val >> 16) & MASK_4_BITS);
    *f_exp_plf   = ((rev_reg_val & BIT_15) != 0);
    // Make RD fifo size = 2^rf_size in hdw words
    *rf_size   = (u8)((rev_reg_val >> 12) & MASK_3_BITS) + 4;
    // Make CD fifo size = 2^cf_size in hdw words
    *cf_size   = (u8)((rev_reg_val >> 9)  & MASK_3_BITS) + 4;
    *nof_pes    = (u8)((rev_reg_val >> 4)  & MASK_5_BITS);
    *nof_rings  = (u8)((rev_reg_val)       & MASK_4_BITS);
}


static inline void
EIP202_OPTIONS2_RD(
        device_handle_t device,
        u8 * const nof_la_ifs,
        u8 * const nof_in_ifs,
        u8 * const nof_axi_wr_chs,
        u8 * const nof_axi_rd_clusters,
        u8 * const nof_axi_rd_cpc)
{
    u32 rev_reg_val;

    rev_reg_val = eip202_read32(device, EIP202_G_REG_OPTIONS2);

    *nof_axi_rd_cpc      = (u8)((rev_reg_val >> 28) & MASK_4_BITS);
    *nof_axi_rd_clusters = (u8)((rev_reg_val >> 20) & MASK_8_BITS);
    *nof_axi_wr_chs      = (u8)((rev_reg_val >> 16) & MASK_4_BITS);
    *nof_in_ifs         = (u8)((rev_reg_val >> 4)  & MASK_4_BITS);
    *nof_la_ifs         = (u8)((rev_reg_val)       & MASK_4_BITS);
}


static inline void
EIP202_MST_CTRL_BUS_BURST_SIZE_GET(
        device_handle_t device,
        u8 * const bus_burst_size)
{
    u32 reg_val;

    reg_val = eip202_read32(device, EIP202_G_REG_MST_CTRL);

    *bus_burst_size  = (u8)((reg_val >> 4  ) & MASK_4_BITS);
}


static inline void
EIP202_MST_CTRL_BUS_BURST_SIZE_UPDATE(
        device_handle_t device,
        const u8 bus_burst_size,
        const u8 rx_bus_burst_size)
{
    u32 reg_val;

    // Preserve other settings
    reg_val = eip202_read32(device, EIP202_G_REG_MST_CTRL);

    reg_val &= ~MASK_8_BITS;

    reg_val |= (u32)((((u32)bus_burst_size)   & MASK_4_BITS) << 4);
    reg_val |= (u32)((((u32)rx_bus_burst_size) & MASK_4_BITS));

    eip202_write32(device, EIP202_G_REG_MST_CTRL, reg_val);
}


static inline void
EIP202_MST_CTRL_BUS_TIMEOUT_UPDATE(
        device_handle_t device,
        const u8 timeout)
{
    u32 reg_val;

    // Preserve other settings
    reg_val = eip202_read32(device, EIP202_G_REG_MST_CTRL);

    reg_val &= ~(MASK_6_BITS << 26);

    reg_val |= (u32)((((u32)timeout) & MASK_6_BITS) << 26);

    eip202_write32(device, EIP202_G_REG_MST_CTRL, reg_val);
}


static inline void
EIP202_MST_CTRL_BYTE_SWAP_UPDATE(
        const device_handle_t device,
        const bool f_byte_swap)
{
    u32 slave_cfg;

    slave_cfg = eip202_read32(device, EIP202_G_REG_MST_CTRL);

    // Swap the bytes for the Host endianness format (must be Big Endian)
    eip202_set_slave_byte_order_swap_word32(&slave_cfg);

    // Enable the Slave Endian byte Swap in HW
    slave_cfg &= ~(BIT_25 | BIT_24);
    slave_cfg |= (f_byte_swap ? BIT_24 : BIT_25);

    // Swap the bytes back for the device endianness format (Little Endian)
    eip202_set_slave_byte_order_swap_word32(&slave_cfg);

    // Set byte order in the EIP202_G_REG_MST_CTRL register
    eip202_write32(device, EIP202_G_REG_MST_CTRL, slave_cfg);
}


/*----------------------------------------------------------------------------
 * EIP202 Ring Arbiter Functions
 *
 */

static inline void
EIP202_RA_PRIO_0_VALUE32_WR(
        device_handle_t device,
        const u32 value)
{
    eip202_write32(device, EIP202_RA_REG_PRIO_0, value);
}


static inline u32
EIP202_RA_PRIO_0_VALUE32_RD(
        device_handle_t device)
{
    return eip202_read32(device, EIP202_RA_REG_PRIO_0);
}


static inline void
EIP202_RA_PRIO_0_WR(
        device_handle_t device,
        const bool f_cdr0_high,
        const u8 cdr0_slots,
        const bool f_cdr1_high,
        const u8 cdr1_slots,
        const bool f_cdr2_high,
        const u8 cdr2_slots,
        const bool f_cdr3_high,
        const u8 cdr3_slots)
{
    u32 reg_val = EIP202_RA_REG_PRIO_0_DEFAULT;

    if(f_cdr0_high)
        reg_val |= BIT_7;

    if(f_cdr1_high)
        reg_val |= BIT_15;

    if(f_cdr2_high)
        reg_val |= BIT_23;

    if(f_cdr3_high)
        reg_val |= BIT_31;

    reg_val |= (u32)((((u32)cdr3_slots) & MASK_4_BITS) << 24);
    reg_val |= (u32)((((u32)cdr2_slots) & MASK_4_BITS) << 16);
    reg_val |= (u32)((((u32)cdr1_slots) & MASK_4_BITS) << 8);
    reg_val |= (u32)((((u32)cdr0_slots) & MASK_4_BITS));

    eip202_write32(device, EIP202_RA_REG_PRIO_0, reg_val);
}


static inline void
EIP202_RA_PRIO_1_WR(
        device_handle_t device,
        const bool f_cdr4_high,
        const u8 cdr4_slots,
        const bool f_cdr5_high,
        const u8 cdr5_slots,
        const bool f_cdr6_high,
        const u8 cdr6_slots,
        const bool f_cdr7_high,
        const u8 cdr7_slots)
{
    u32 reg_val = EIP202_RA_REG_PRIO_1_DEFAULT;

    if(f_cdr4_high)
        reg_val |= BIT_7;

    if(f_cdr5_high)
        reg_val |= BIT_15;

    if(f_cdr6_high)
        reg_val |= BIT_23;

    if(f_cdr7_high)
        reg_val |= BIT_31;

    reg_val |= (u32)((((u32)cdr7_slots) & MASK_4_BITS) << 24);
    reg_val |= (u32)((((u32)cdr6_slots) & MASK_4_BITS) << 16);
    reg_val |= (u32)((((u32)cdr5_slots) & MASK_4_BITS) << 8);
    reg_val |= (u32)((((u32)cdr4_slots) & MASK_4_BITS));

    eip202_write32(device, EIP202_RA_REG_PRIO_1, reg_val);
}


static inline void
EIP202_RA_PRIO_2_WR(
        device_handle_t device,
        const bool f_cdr8_high,
        const u8 cdr8_slots,
        const bool f_cdr9_high,
        const u8 cdr9_slots,
        const bool f_cdr10_high,
        const u8 cdr10_slots,
        const bool f_cdr11_high,
        const u8 cdr11_slots)
{
    u32 reg_val = EIP202_RA_REG_PRIO_2_DEFAULT;

    if(f_cdr8_high)
        reg_val |= BIT_7;

    if(f_cdr9_high)
        reg_val |= BIT_15;

    if(f_cdr10_high)
        reg_val |= BIT_23;

    if(f_cdr11_high)
        reg_val |= BIT_31;

    reg_val |= (u32)((((u32)cdr11_slots) & MASK_4_BITS) << 24);
    reg_val |= (u32)((((u32)cdr10_slots) & MASK_4_BITS) << 16);
    reg_val |= (u32)((((u32)cdr9_slots)  & MASK_4_BITS) << 8);
    reg_val |= (u32)((((u32)cdr8_slots)  & MASK_4_BITS));

    eip202_write32(device, EIP202_RA_REG_PRIO_2, reg_val);
}


static inline void
EIP202_RA_PRIO_3_WR(
        device_handle_t device,
        const bool f_cdr12_high,
        const u8 cdr12_slots,
        const bool f_cdr13_high,
        const u8 cdr13_slots,
        const bool f_cdr14_high,
        const u8 cdr14_slots)
{
    u32 reg_val = EIP202_RA_REG_PRIO_3_DEFAULT;

    if(f_cdr12_high)
        reg_val |= BIT_7;

    if(f_cdr13_high)
        reg_val |= BIT_15;

    if(f_cdr14_high)
        reg_val |= BIT_23;

    reg_val |= (u32)((((u32)cdr14_slots) & MASK_4_BITS) << 16);
    reg_val |= (u32)((((u32)cdr13_slots) & MASK_4_BITS) << 8);
    reg_val |= (u32)((((u32)cdr12_slots) & MASK_4_BITS));

    eip202_write32(device, EIP202_RA_REG_PRIO_3, reg_val);
}


static inline void
EIP202_RA_PRIO_0_DEFAULT_WR(
        device_handle_t device)
{
    eip202_write32(device,
                   EIP202_RA_REG_PRIO_0,
                   EIP202_RA_REG_PRIO_0_DEFAULT);
}


static inline void
EIP202_RA_PRIO_1_DEFAULT_WR(
        device_handle_t device)
{
    eip202_write32(device,
                   EIP202_RA_REG_PRIO_1,
                   EIP202_RA_REG_PRIO_1_DEFAULT);
}


static inline void
EIP202_RA_PRIO_2_DEFAULT_WR(
        device_handle_t device)
{
    eip202_write32(device,
                   EIP202_RA_REG_PRIO_2,
                   EIP202_RA_REG_PRIO_2_DEFAULT);
}


static inline void
EIP202_RA_PRIO_3_DEFAULT_WR(
        device_handle_t device)
{
    eip202_write32(device,
                   EIP202_RA_REG_PRIO_3,
                   EIP202_RA_REG_PRIO_3_DEFAULT);
}

static inline void
EIP202_RA_PE_CTRL_DEFAULT_WR(
        device_handle_t device,
        const unsigned int p_enr)
{
    eip202_write32(device,
                   EIP202_RA_PE_REG_CTRL(p_enr),
                   EIP202_RA_PE_REG_CTRL_DEFAULT);
}


static inline void
EIP202_RA_PE_CTRL_WR(
        device_handle_t device,
        const unsigned int p_enr,
        const u16 ring_p_emap,
        const bool f_enable,
        const bool f_reset)
{
    u32 reg_val = EIP202_RA_PE_REG_CTRL_DEFAULT;

    if(ring_p_emap > 0)
        reg_val |= (((u32)ring_p_emap) & MASK_15_BITS);

    if(f_enable)
        reg_val |= BIT_30;

    if(f_reset)
        reg_val |= BIT_31;

    eip202_write32(device, EIP202_RA_PE_REG_CTRL(p_enr), reg_val);
}
/*----------------------------------------------------------------------------
 * EIP202 DFE Thread Functions
 *
 */

static inline void
EIP202_DFE_CFG_DEFAULT_WR(
        device_handle_t device,
        const unsigned int offset,
        const unsigned int p_enr)
{
    eip202_write32(device,
                   EIP202_DFE_REG_CFG(p_enr) + offset,
                   EIP202_DFE_REG_CFG_DEFAULT);
}


static inline void
EIP202_DFE_CFG_WR(
        device_handle_t device,
        const unsigned int offset,
        const unsigned int p_enr,
        const u8 min_data_size,
        const u8 data_cache_ctrl,
        const u8 max_data_size,
        const u8 min_ctrl_size,
        const u8 ctrl_cache_ctrl,
        const u8 max_ctrl_size,
        const bool f_adv_thresh_mode,
        const bool f_aggressive)
{
    u32 reg_val = EIP202_DFE_REG_CFG_DEFAULT;

    if(f_aggressive)
        reg_val |= BIT_31;

    if(f_adv_thresh_mode)
        reg_val |= BIT_29;

    reg_val |= (u32)((((u32)max_ctrl_size)   & MASK_4_BITS) << 24);
    reg_val |= (u32)((((u32)ctrl_cache_ctrl) & MASK_3_BITS) << 20);
    reg_val |= (u32)((((u32)min_ctrl_size)   & MASK_4_BITS) << 16);
    reg_val |= (u32)((((u32)max_data_size)   & MASK_4_BITS) << 8);
    reg_val |= (u32)((((u32)data_cache_ctrl) & MASK_3_BITS) << 4);
    reg_val |= (u32)((((u32)min_data_size)   & MASK_4_BITS));

    eip202_write32(device, EIP202_DFE_REG_CFG(p_enr) + offset, reg_val);
}


static inline void
EIP202_DFE_TRD_CTRL_DEFAULT_WR(
        device_handle_t device,
        const unsigned int offset,
        const unsigned int p_enr)
{
    eip202_write32(device,
                   EIP202_DFE_TRD_REG_CTRL(p_enr) + offset,
                   EIP202_DFE_TRD_REG_CTRL_DEFAULT);
}


static inline void
EIP202_DFE_TRD_CTRL_WR(
        device_handle_t device,
        const unsigned int offset,
        const unsigned int p_enr,
        const u16 ring_p_emap,
        const bool f_enable,
        const bool f_reset)
{
    u32 reg_val = EIP202_DFE_TRD_REG_CTRL_DEFAULT;

    IDENTIFIER_NOT_USED(ring_p_emap);
    IDENTIFIER_NOT_USED(f_enable);
    if(f_reset)
        reg_val |= BIT_31;

    eip202_write32(device, EIP202_DFE_TRD_REG_CTRL(p_enr) + offset, reg_val);
}


static inline void
EIP202_DFE_TRD_CTRL_UPDATE(
        const device_handle_t device,
        const unsigned int offset,
        const unsigned int p_enr,
        const u16 ring_p_emap,
        const bool f_enable,
        const bool f_reset)
{
    u32 reg_val;

    // Preserve other settings
    reg_val = eip202_read32(device, EIP202_DFE_TRD_REG_CTRL(p_enr) + offset);

    reg_val &= (~MASK_15_BITS);
    reg_val |= (((u32)ring_p_emap) & MASK_15_BITS);

    if(f_enable)
        reg_val |= BIT_30;
    else
        reg_val &= (~BIT_30);

    if(f_reset)
        reg_val |= BIT_31;
    else
        reg_val &= (~BIT_31);

    eip202_write32(device, EIP202_DFE_TRD_REG_CTRL(p_enr) + offset, reg_val);
}


static inline void
EIP202_DFE_TRD_STAT_RD(
        device_handle_t device,
        const unsigned int offset,
        const unsigned int p_enr,
        u16 * const cdfifo_word_count,
        u16 * const dma_byte_count,
        bool * const f_token_dma_busy,
        bool * const f_data_dma_busy,
        bool * const f_dma_error)
{
    u32 reg_val;

    reg_val = eip202_read32(device, EIP202_DFE_TRD_REG_STAT(p_enr) + offset);

    *f_dma_error         = ((reg_val & BIT_31) != 0);
    *f_data_dma_busy      = ((reg_val & BIT_29) != 0);
    *f_token_dma_busy     = ((reg_val & BIT_28) != 0);
    *dma_byte_count      = (u16)((reg_val >> 16) & MASK_12_BITS);
    *cdfifo_word_count   = (u16)((reg_val)       & MASK_12_BITS);
}


/*----------------------------------------------------------------------------
 * EIP202 DSE Thread Functions
 *
 */

static inline void
EIP202_DSE_CFG_DEFAULT_WR(
        device_handle_t device,
        const unsigned int offset,
        const unsigned int p_enr)
{
    eip202_write32(device,
                   EIP202_DSE_REG_CFG(p_enr) + offset,
                   EIP202_DSE_REG_CFG_DEFAULT);
}


static inline void
EIP202_DSE_CFG_WR(
        device_handle_t device,
        const unsigned int offset,
        const unsigned int p_enr,
        const u8 min_data_size,
        const u8 data_cache_ctrl,
        const u8 max_data_size,
        const u8 buffer_ctrl,
        const bool f_enable_single_wr,
        const bool f_aggressive)
{
    u32 reg_val = EIP202_DSE_REG_CFG_DEFAULT;

    reg_val &= (~BIT_31);
    if(f_aggressive)
        reg_val |= BIT_31;

    if(f_enable_single_wr)
        reg_val |= BIT_29;

    reg_val &= (u32)(~(MASK_2_BITS << 14)); // Clear buffer_ctrl field
    reg_val |= (u32)((((u32)buffer_ctrl)    & MASK_2_BITS) << 14);
    reg_val |= (u32)((((u32)max_data_size)   & MASK_4_BITS) << 8);
    reg_val |= (u32)((((u32)data_cache_ctrl) & MASK_3_BITS) << 4);
    reg_val |= (u32)((((u32)min_data_size)   & MASK_4_BITS));

    eip202_write32(device, EIP202_DSE_REG_CFG(p_enr) + offset, reg_val);
}


static inline void
EIP202_DSE_TRD_CTRL_DEFAULT_WR(
        device_handle_t device,
        const unsigned int offset,
        const unsigned int p_enr)
{
    eip202_write32(device,
                   EIP202_DSE_TRD_REG_CTRL(p_enr) + offset,
                   EIP202_DSE_TRD_REG_CTRL_DEFAULT);
}


static inline void
EIP202_DSE_TRD_CTRL_WR(
        device_handle_t device,
        const unsigned int offset,
        const unsigned int p_enr,
        const u16 ring_p_emap,
        const bool f_enable,
        const bool f_reset)
{
    u32 reg_val = EIP202_DSE_TRD_REG_CTRL_DEFAULT;
    IDENTIFIER_NOT_USED(ring_p_emap);

    if(f_enable)
        reg_val |= BIT_30;

    if(f_reset)
        reg_val |= BIT_31;

    eip202_write32(device, EIP202_DSE_TRD_REG_CTRL(p_enr) + offset, reg_val);
}


static inline void
EIP202_DSE_TRD_CTRL_UPDATE(
        const device_handle_t device,
        const unsigned int offset,
        const unsigned int p_enr,
        const u16 ring_p_emap,
        const bool f_enable,
        const bool f_reset)
{
    u32 reg_val;

    // Preserve other settings
    reg_val = eip202_read32(device, EIP202_DSE_TRD_REG_CTRL(p_enr) + offset);

    reg_val &= (~MASK_15_BITS);
    reg_val |= (((u32)ring_p_emap) & MASK_15_BITS);

    if(f_enable)
        reg_val |= BIT_30;
    else
        reg_val &= (~BIT_30);

    if(f_reset)
        reg_val |= BIT_31;
    else
        reg_val &= (~BIT_31);

    eip202_write32(device, EIP202_DSE_TRD_REG_CTRL(p_enr) + offset, reg_val);
}


static inline void
EIP202_DSE_TRD_STAT_RD(
        device_handle_t device,
        const unsigned int offset,
        const unsigned int p_enr,
        u16 * const rdfifo_word_count,
        u16 * const dma_byte_count,
        bool * const f_data_flush_busy,
        bool * const f_data_dma_busy,
        bool * const f_dma_error)
{
    u32 reg_val;

    reg_val = eip202_read32(device, EIP202_DSE_TRD_REG_STAT(p_enr) + offset);

    *f_dma_error         = ((reg_val & BIT_31) != 0);
    *f_data_dma_busy      = ((reg_val & BIT_29) != 0);
    *f_data_flush_busy    = ((reg_val & BIT_28) != 0);
    *dma_byte_count      = (u16)((reg_val >> 16) & MASK_12_BITS);
    *rdfifo_word_count   = (u16)((reg_val)       & MASK_12_BITS);
}



static inline void
EIP202_LASIDE_BASE_ADDR_LO_DEFAULT_WR(
        device_handle_t device)
{
    eip202_write32(device,
                   EIP202_REG_LASIDE_BASE_ADDR_LO,
                   EIP202_REG_LASIDE_BASE_ADDR_LO_DEFAULT);
}


static inline void
EIP202_LASIDE_BASE_ADDR_LO_WR(
        device_handle_t device,
        const u8 dscr_data_swap)
{
    u32 reg_val = EIP202_REG_LASIDE_BASE_ADDR_LO_DEFAULT;

    reg_val |= (u32)((((u32)dscr_data_swap)   & MASK_4_BITS));

    eip202_write32(device, EIP202_REG_LASIDE_BASE_ADDR_LO, reg_val);
}


static inline void
EIP202_LASIDE_BASE_ADDR_HI_DEFAULT_WR(
        device_handle_t device)
{
    eip202_write32(device,
                   EIP202_REG_LASIDE_BASE_ADDR_HI,
                   EIP202_REG_LASIDE_BASE_ADDR_HI_DEFAULT);
}


static inline void
EIP202_LASIDE_SLAVE_CTRL_DEFAULT_WR(
        device_handle_t device,
        const unsigned int fif_onr)
{
    eip202_write32(device,
                   EIP202_REG_LASIDE_SLAVE_CTRL(fif_onr),
                   EIP202_REG_LASIDE_SLAVE_CTRL_DEFAULT);
}


static inline void
EIP202_LASIDE_SLAVE_CTRL_WR(
        device_handle_t device,
        const unsigned int fif_onr,
        const u8 in_pkt_data_swap,
        const u8 pkt_prot,
        const u8 token_data_swap,
        const u8 token_prot,
        const bool f_dscr_error_clear)
{
    u32 reg_val = EIP202_REG_LASIDE_SLAVE_CTRL_DEFAULT;

    if(f_dscr_error_clear)
        reg_val |= BIT_31;

    reg_val |= (u32)((((u32)token_prot)     & MASK_4_BITS) << 12);
    reg_val |= (u32)((((u32)token_data_swap) & MASK_4_BITS) << 8);
    reg_val |= (u32)((((u32)pkt_prot)       & MASK_4_BITS) << 4);
    reg_val |= (u32)((((u32)in_pkt_data_swap) & MASK_4_BITS));

    eip202_write32(device, EIP202_REG_LASIDE_SLAVE_CTRL(fif_onr), reg_val);
}


static inline void
EIP202_LASIDE_MASTER_CTRL_DEFAULT_WR(
        device_handle_t device,
        const unsigned int fif_onr)
{
    eip202_write32(device,
                   EIP202_REG_LASIDE_MASTER_CTRL(fif_onr),
                   EIP202_REG_LASIDE_MASTER_CTRL_DEFAULT);
}


static inline void
EIP202_LASIDE_MASTER_CTRL_WR(
        device_handle_t device,
        const unsigned int fif_onr,
        const u8 out_pkt_data_swap,
        const u8 pkt_prot,
        const bool f_dscr_error_clear)
{
    u32 reg_val = EIP202_REG_LASIDE_MASTER_CTRL_DEFAULT;

    if(f_dscr_error_clear)
        reg_val |= BIT_31;

    reg_val |= (u32)((((u32)pkt_prot)        & MASK_4_BITS) << 4);
    reg_val |= (u32)((((u32)out_pkt_data_swap) & MASK_4_BITS));

    eip202_write32(device, EIP202_REG_LASIDE_MASTER_CTRL(fif_onr), reg_val);
}


static inline void
EIP202_INLINE_CTRL_DEFAULT_WR(
        device_handle_t device,
        const unsigned int fif_onr)
{
    eip202_write32(device,
                   EIP202_REG_INLINE_CTRL(fif_onr),
                   EIP202_REG_INLINE_CTRL_DEFAULT);
}


static inline void
EIP202_INLINE_CTRL_WR(
        device_handle_t device,
        const unsigned int fif_onr,
        const u8 in_pkt_data_swap,
        const bool f_in_proto_error_clear,
        const u8 out_pkt_data_swap,
        const u8 thresh_lo,
        const u8 thresh_hi,
        const u8 burst_size,
        const bool f_force_in_order)
{
    u32 reg_val = EIP202_REG_INLINE_CTRL_DEFAULT;

    if(f_in_proto_error_clear)
        reg_val |= BIT_7;

    if(f_force_in_order)
        reg_val |= BIT_31;

    reg_val |= (u32)((((u32)burst_size)      & MASK_4_BITS) << 20);
    reg_val |= (u32)((((u32)thresh_hi)       & MASK_4_BITS) << 16);
    reg_val |= (u32)((((u32)thresh_lo)       & MASK_4_BITS) << 12);
    reg_val |= (u32)((((u32)out_pkt_data_swap) & MASK_4_BITS) << 8);
    reg_val |= (u32)((((u32)in_pkt_data_swap)  & MASK_4_BITS));

    eip202_write32(device, EIP202_REG_INLINE_CTRL(fif_onr), reg_val);
}

#endif /* EIP202_GLOBAL_LEVEL0_H_ */


/* end of file eip202_global_level0.h */
