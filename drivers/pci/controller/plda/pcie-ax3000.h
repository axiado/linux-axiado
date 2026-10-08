/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Axiado AX3000/AX3005 PCIe controller — shared definitions for
 * the EPC (endpoint controller) and host-bridge drivers.
 *
 * Copyright (C) 2023-2026 Axiado Corporation.
 */

#ifndef __PCIE_AX3000_H__
#define __PCIE_AX3000_H__

#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/delay.h>
#include <linux/iopoll.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/mutex.h>
#include <linux/of_pci.h>
#include <linux/pci.h>

#include "pcie-plda.h"

/* ----------------------------------------------------------------
 * Bridge register offsets
 * ----------------------------------------------------------------
 */

#define PF_MSI_CAP_DISABLE			BIT(3)
#define PLDA_MSI_CAP_OFFSET			0xE0

/* PCI IDS DWORDs (0x98-0xA3); DW1 (0x9C) is defined in pcie-plda.h */
#define PCIE_PCI_IDS_DW0			0x98
#define PCIE_PCI_IDS_DW2			0xA0
#define REG_PCIE_PCIE_CFGCTRL_ADRS_OFFSET	0x0084
#define REG_PCIE_X1_GEN_SETTINGS_ADRS_OFFSET	0x0080
#define REG_PCIE_X2_GEN_SETTINGS_ADRS_OFFSET	0x0080
#define REG_PCIE_PHYMAC_CFG_ADRS_OFFSET	(0x0300 + 0x003C)
#define REG_PCIE_X1_PCIE_PEX_SPC_ADRS_OFFSET	0x00D4
#define REG_PCIE_EQ_TUNING_31_0_ADRS_OFFSET	(0x0300 + 0x005C)
#define REG_PCIE_EQ_TUNING_63_32_ADRS_OFFSET	(0x0300 + 0x0060)
#define REG_PCIE_EQ_PRESET_8G_31_0_ADRS_OFFSET	 0x0100
#define REG_PCIE_EQ_PRESET_16G_31_0_ADRS_OFFSET 0x0150
#define REG_PCIE_ISTATUS_HOST_ADRS_OFFSET	0x018C
#define REG_PCIE_X1_COMMAND_STATUS_ADRS_OFFSET	0x0004

/* BAR sizing registers (bridge internal) */
#define PCIE_BAR_01_OFFSET			0xE4
#define PCIE_BAR_23_OFFSET			0xEC
#define PCIE_BAR_45_OFFSET			0xF4
#define PCIE_BAR_EXP_ROM_OFFSET			0xFC
/* ----------------------------------------------------------------
 * EXT register offsets (per-instance)
 * ----------------------------------------------------------------
 */

#define REG_PCIE_X1_RESET_CTRL_ADRS_OFFSET	0x15C
#define REG_PCIE_X2_RESET_CTRL_ADRS_OFFSET	0x1CC
#define REG_PCIE_X1_IP_CTRL_ADRS_OFFSET	0x118
#define REG_PCIE_X2_IP_CTRL_ADRS_OFFSET	0x0B8
#define REG_PCIE_X1_GEN_OFF			0x10C
#define REG_PCIE_X1_GEN_VAL			0xFE
#define AX_PCIE_TL_CLOCK_FREQ_MHZ		500
#define REG_PCIE_X1_PF_CTRL_STATUS_ADRS_OFFSET	0x104
#define REG_PCIE_X2_PF_CTRL_STATUS_ADRS_OFFSET	0x154

/* PERST# pullup via IOCTL pad */
#define REG_PAD_CFG_REG_25_ADRS_OFFSET		0x070
#define REG_PAD_CFG_REG_26_ADRS_OFFSET		0x074
#define PCIE_PERST_PULLUP_SET			(0x1 << 22)

/* ----------------------------------------------------------------
 * CSR / ATR
 * ----------------------------------------------------------------
 */

#define LINK_STATUS_LOW_POWER_ADDR		0x000003DC

#define PCIE_ATR_PCIE_WIN0			0x600
#define PCIE_ATR_MAX_TABLE_NUM			8
#define PCIE_ATR_SRC_WIN_SIZE_SHIFT		1
#define PCIE_ATR_TRSL_ADDR_MASK			0xFFFFF000
#define PCIE_ATR_SRC_ADDR_LOW			0x0
#define PCIE_ATR_SRC_ADDR_HIGH			0x4
#define PCIE_ATR_TRSL_ADDR_LOW			0x8
#define PCIE_ATR_TRSL_ADDR_HIGH			0xC
#define PCIE_ATR_TRSL_PARAM			0x10
#define PCIE_ATR_TABLE_OFFSET			0x20
#define PCIE_ATR_TRSLID_AXI4_MASTER_0		0x4

/* ----------------------------------------------------------------
 * Class codes / constants
 * ----------------------------------------------------------------
 */

#define PCIE_ETHERNET_CLASS_CODE		0x02000000
#define PCIE_VGA_CLASS_CODE			0x03000000
#define PCIE_PHY_EQ_ENABLE			0x1
#define PCIE_PHY_CONF_CTRL_LTSSM		0xFFFFFFFB
#define PCIE_CFG_BUS_MASTER_EN			0x6

/* ----------------------------------------------------------------
 * EQ preset/tuning
 * ----------------------------------------------------------------
 */

#define PCIE_GEN3_MAX_TUNING_ITER		40
#define PCIE_GEN3_TX_PRESET			0x1
#define PCIE_GEN3_RX_PRESET			0x1
#define PCIE_GEN3_PRESET_VECTOR			0x9F
#define PCIE_GEN4_MAX_TUNING_ITER		40
#define PCIE_GEN4_TX_PRESET			0x6
#define PCIE_GEN4_RX_PRESET			0x6
#define PCIE_GEN4_PRESET_VECTOR			0x7FF

#define PCIE_GEN3_RX_TX_PRESET_GET(rx, tx) \
	({ \
		u8 __byte = ((rx) << 4) | (tx); \
		(u32)(__byte | (__byte << 16)); \
	})
#define PCIE_GEN4_RX_TX_PRESET_GET(rx, tx) \
	({ \
		u8 __byte = ((rx) << 4) | (tx); \
		(u32)(__byte | (__byte << 8) | (__byte << 16) | (__byte << 24)); \
	})

/* ----------------------------------------------------------------
 * Register field helpers
 * ----------------------------------------------------------------
 */

#define REG_PCIE_IP_CTRL_RP_NEP_SET(dst, val) \
	(((u32)(dst) & ~0x1) | ((u32)(val) & 0x1))
#define REG_PCIE_IP_CTRL_FREQ_SET(dst, val) \
	(((u32)(dst) & ~0x7FFFFE) | (((u32)(val) << 1) & 0x7FFFFE))
#define REG_PCIE_IP_CTRL_TL_CLK_GATE_EN_SET(dst, val) \
	(((u32)(dst) & ~0x800000) | (((u32)(val) << 23) & 0x800000))

#define REG_PCIE_RESET_CTRL_PCIE_RSTN_SET(dst, val) \
	(((u32)(dst) & ~0x1) | ((u32)(val) & 0x1))
#define REG_PCIE_RESET_CTRL_HOLD_PIPE_RST_SET(dst, val) \
	(((u32)(dst) & ~0x2) | (((u32)(val) << 1) & 0x2))

#define PCIE_K_SET_PCIE_PORT_TYPE_SET(dst, val) \
	(((u32)(dst) & ~0x1) | ((u32)(val) & 0x1))
#define PCIE_K_SET_LANE_REVERSAL_EN_SET(dst, val) \
	(((u32)(dst) & ~0x80) | (((u32)(val) << 7) & 0x80))
#define PCIE_K_SET_LINK_WIDTH_X2_SET(dst, val) \
	(((u32)(dst) & ~0x100) | (((u32)(val) << 8) & 0x100))

#define PCIE_EQ_TUNING_63_0_PRESET_TUNING_SETTINGS_SET(dst, val) \
	((u64)(((u64)(dst) & ~0x3ULL) | ((u64)(val) & 0x3ULL)))
#define PCIE_EQ_TUNING_63_0_CONT_FINE_TUNE_EVEN_IF_NO_COEFF_SET(dst, val) \
	((u64)(((u64)(dst) & ~0x4ULL) | (((u64)(val) << 2) & 0x4ULL)))
#define PCIE_EQ_TUNING_63_0_GEN3_PRESET_SET(dst, val) \
	((u64)(((u64)(dst) & ~0x7FF0000ULL) | \
	       (((u64)(val) << 16) & 0x7FF0000ULL)))
#define PCIE_EQ_TUNING_63_0_GEN3_MAX_TUNING_ITER_SET(dst, val) \
	((u64)(((u64)(dst) & ~0x3F0000000ULL) | \
	       (((u64)(val) << 28) & 0x3F0000000ULL)))
#define PCIE_EQ_TUNING_63_0_GEN4_PRESET_SET(dst, val) \
	((u64)(((u64)(dst) & ~0x7FF000000000ULL) | \
	       (((u64)(val) << 36) & 0x7FF000000000ULL)))
#define PCIE_EQ_TUNING_63_0_GEN4_MAX_TUNING_ITER_SET(dst, val) \
	((u64)(((u64)(dst) & ~0x3F000000000000ULL) | \
	       (((u64)(val) << 48) & 0x3F000000000000ULL)))

/* ----------------------------------------------------------------
 * Shared driver state (parsed by pcie-axiado-common.c)
 * ----------------------------------------------------------------
 */
struct axiado_pcie {
	struct device *dev;
	struct pci_epf *epf;
	int irq;

	bool is_root_port;
	bool is_vga;
	bool is_eth;

	bool pcie_x1;
	bool pcie_x2;
	unsigned int lanes;

	void __iomem *csr;
	void __iomem *phy;
	void __iomem *cfg;
	void __iomem *bridge;
	void __iomem *mbx;
	void __iomem *ioctl;
	void __iomem *ext;
	resource_size_t csr_phys;
	resource_size_t cfg_phys;
	resource_size_t bridge_phys;
	resource_size_t ext_phys;
	struct resource cs;

	/* Host bridge (root port path in common.c) */
	struct list_head ports;
	void __iomem *base;

	/* Endpoint BAR config from DTS ranges (range[0]→BAR0, [1]→BAR2, [2]→BAR4) */
	u64 ep_bar0_addr;
	u64 ep_bar0_size;
	u32 ep_bar0_flags;
	u64 ep_bar1_addr;
	u64 ep_bar1_size;
	u32 ep_bar1_flags;
	u64 ep_bar2_addr;
	u64 ep_bar2_size;
	u32 ep_bar2_flags;
	int ep_num_ranges;

	/* EP link monitor */
	struct delayed_work ep_link_work;
	bool ep_link_up;

	/* EPC framework */
	struct pci_epc *epc;
	struct mutex atr_lock; /* protects atr_table_map/atr_table_addr */
	DECLARE_BITMAP(atr_table_map, PCIE_ATR_MAX_TABLE_NUM);
	phys_addr_t atr_table_addr[PCIE_ATR_MAX_TABLE_NUM];
	u16 msi_cap;
	u32 num_msi_vectors;
};

struct axiado_pcie_port {
	struct axiado_pcie *pcie;
	struct device_node *np;
	struct list_head list;
	struct resource regs;
	void __iomem *base;
	unsigned int index;
};

int axiado_pcie_get_resources(struct axiado_pcie *pcie);
int axiado_pcie_parse_dt(struct axiado_pcie *pcie);
void axiado_pcie_config_eq_gen3_4(struct axiado_pcie *pcie);

#endif /* __PCIE_AX3000_H__ */
