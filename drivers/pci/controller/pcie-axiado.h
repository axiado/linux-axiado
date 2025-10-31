/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2023-2025 Axiado Corporation.
 */

#ifndef __PCIE_AXIADO_H__
#define __PCIE_AXIADO_H__

#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/iopoll.h>
#include <linux/irq.h>
#include <linux/irqchip/chained_irq.h>
#include <linux/irqdomain.h>
#include <linux/msi.h>
#include <linux/of_pci.h>
#include <linux/pci.h>
#include <linux/pm_runtime.h>
#include <linux/soc/axiado/pcie-axiado.h>

#define REG_PCIE_X1_RESET_CTRL_ADRS_OFFSET 0x09c
#define REG_PCIE_X1_MBX_INTR_MASK_ADRS_OFFSET 0x054
#define REG_PCIE_X1_PCIE_CFGCTRL_ADRS_OFFSET 0x0084
#define REG_PCIE_PCIE_CFGCTRL_ADRS_OFFSET 0x0084

#define REG_PCIE_X2_RESET_CTRL_ADRS_OFFSET 0x0fc
#define REG_PCIE_X2_PF_MBX_INTR_MASK_ADRS_OFFSET 0x0a4
#define REG_PCIE_X2_PCIE_CFGCTRL_ADRS_OFFSET 0x0084

#define REG_PAD_CFG_REG_25_ADRS_OFFSET 0x070
#define REG_PCIE_X1_IP_CTRL_ADRS_OFFSET 0x058
#define REG_PCIE_X1_GEN_SETTINGS_ADRS_OFFSET (0x0000 + 0x0080)
#define REG_PCIE_X1_PCIE_PCI_IDS_63_32_ADRS_OFFSET (0x0000 + 0x009C)
#define REG_PCIE_PCIE_PCI_IDS_63_32_ADRS_OFFSET (0x0000 + 0x009C)

#define REG_PAD_CFG_REG_26_ADRS_OFFSET 0x074
#define REG_PCIE_X2_IP_CTRL_ADRS_OFFSET 0x0b8
#define REG_PCIE_X2_PCIE_BASIC_CONF_ADRS_OFFSET (0x0000 + 0x0014)
#define REG_PCIE_X2_GEN_SETTINGS_ADRS_OFFSET (0x0000 + 0x0080)
#define REG_PCIE_X2_PCIE_PCI_IDS_63_32_ADRS_OFFSET (0x0000 + 0x009C)
#define REG_PCIE_X1_ATR_PCIE_WIN0_TAB2_TRSL_ADDR_31_0_ADRS_OFFSET \
	(0x600 + 0x0048)
#define REG_PCIE_X1_ATR_PCIE_WIN_TAB2_TRSL_ADDR_31_0_ADRS_OFFSET \
	(0x600 + 0x0008)
#define REG_PCIE_X1_ATR_PCIE_WIN0_TAB6_TRSL_ADDR_31_0_ADRS_OFFSET \
	(0x600 + 0x00C8)
#define REG_PCIE_X1_COMMAND_STATUS_ADRS_OFFSET (0x0000 + 0x4)
#define REG_PCIE_X2_ATR_PCIE_WIN0_TAB2_TRSL_ADDR_31_0_ADRS_OFFSET \
	(0x600 + 0x0048)
#define REG_PCIE_PHYMAC_CFG_ADRS_OFFSET (0x0300 + 0x003C)
#define REG_PCIE_EQ_TUNING_63_32_ADRS_OFFSET (0x0300 + 0x0060)
#define REG_PCIE_DMA_BASE  (0x400)
#define REG_PCIE_DMA0_CONFIG_SPACE_DMA_SRCPARAM_ADRS_OFFSET       (0x00)
#define REG_PCIE_DMA0_CONFIG_SPACE_DMA_DESTPARAM_ADRS_OFFSET      (0x04)
#define REG_PCIE_DMA0_CONFIG_SPACE_DMA_SRCADDR_31_0_ADRS_OFFSET   (0x08)
#define REG_PCIE_DMA0_CONFIG_SPACE_DMA_SRCADDR_63_32_ADRS_OFFSET  (0x0C)
#define REG_PCIE_DMA0_CONFIG_SPACE_DMA_DESTADDR_31_0_ADRS_OFFSET  (0x10)
#define REG_PCIE_DMA0_CONFIG_SPACE_DMA_DESTADDR_63_32_ADRS_OFFSET (0x14)
#define REG_PCIE_DMA0_CONFIG_SPACE_DMA_LENGTH_ADRS_OFFSET         (0x18)
#define REG_PCIE_DMA0_CONFIG_SPACE_DMA_CONTROL_ADRS_OFFSET        (0x1C)
#define REG_PCIE_DMA0_CONFIG_SPACE_DMA_STATUS_ADRS_OFFSET         (0x20)
#define REG_PCIE_PCIE_VC_CRED_31_0_ADRS_OFFSET 0x0090
#define REG_PCIE_PCIE_VC_CRED_63_32_ADRS_OFFSET 0x0094
#define REG_PCIE_PCIE_SRIOV_31_0_ADRS_OFFSET 0x0120
#define REG_PCIE_PCIE_SRIOV_63_32_ADRS_OFFSET 0x0124
#define REG_PCIE_AXI_MST0_VF_APER_LO_LSBS_N3_ADRS_OFFSET 0x064
#define REG_PCIE_AXI_MST0_VF_APER_LO_MSBS_N3_ADRS_OFFSET 0x070
#define REG_PCIE_AXI_MST0_VF_APER_HI_LSBS_N3_ADRS_OFFSET 0x07c
#define REG_PCIE_AXI_MST0_VF_APER_HI_MSBS_N3_ADRS_OFFSET 0x0
#define REG_PCIE_AXI_MST0_VF_APER_LO_MSBS_N3_FLD_REMAP_DIS_LSB 27
#define REG_PCIE_AXI_MST0_VF_APER_LO_MSBS_N3_FLD_APER_SIZE_LSB 16
#define REG_PCIE_AXI_MST0_VF_APER_LO_MSBS_N3_FLD_ADRS_LO_LSB 0
#define REG_PCIE_X1_PF_CTRL_STATUS_ADRS_OFFSET 0x044
#define REG_PCIE_X2_PF_CTRL_STATUS_ADRS_OFFSET 0x094
#define REG_PCIE_X2_VF_MSI_TRG_N2_ADRS_OFFSET 0x054
#define REG_PCIE_ISTATUS_HOST_ADRS_OFFSET 0x018c
#define REG_PCIE_X2_CSR_INT_OCM_OFFSET 0x03f8

#define LINK_STATUS_LOW_POWER_ADDR 0x000003dc
#define PCIE_ATR_TRSLID_PCIE_MEMORY 0x0
#define PCIE_ATR_TRSL_DIR BIT(22)
#define PCIE_ATR_AXI4_SLV0 0x800
#define PCIE_ATR_MAX_TABLE_NUM 8
#define PCIE_ATR_SRC_WIN_SIZE_SHIFT 1
#define PCIE_ATR_SRC_ADDR_MASK 0x0ffff000
#define PCIE_ATR_TRSL_ADDR_MASK 0xfffff000
#define PCIE_ATR_SRC_ADDR_LOW 0x0
#define PCIE_ATR_SRC_ADDR_HIGH 0x4
#define PCIE_ATR_TRSL_ADDR_LOW 0x8
#define PCIE_ATR_TRSL_ADDR_HIGH 0xc
#define PCIE_ATR_TRSL_PARAM 0x10
#define PCIE_ATR_TABLE_OFFSET 0x20

#define INT_PCI_MSI_NR (1 * 32)
#define PCIE_PERST_PULLUP_EN 1 /* PCIe RESET Enabling Settings */
#define PCIE_SERIAL_MODE 1 /* Enabling the MAC and Equalization Settings */
#define PCIE_LINKUP_TIMEOUT 2000 /* Timeout for Link status check */
#define PCIE_RP_DISABLE 0x0 /* DTS value to disable RP Mode */

#define PCIE_ETHERNET_CLASS_CODE \
	0x02000000 /* PCIe Class Code for network in EP Mode */
#define PCIE_VGA_CLASS_CODE 0x03000000 /* PCIe Class Code for VGA in EP Mode */
#define PCIE_BRIDGE_CLASS_CODE 0x06040000 /* PCIe Class Code for Host Mode */
#define PCIE_PERST_PULLUP_SET (0x1 << 22)
#define PCIE_PHY_GEN1 (0x1 << 12)
#define PCIE_PHY_GEN2 (0x3 << 12)
#define PCIE_PHY_GEN3 (0x7 << 12)
#define PCIE_PHY_PLL_CTRL1_SET (0x03 << 8)
#define PCIE_PHY_PLL_CTRL2_SET (0x04 << 16)
#define PCIE_PHY_PLL_VGA_SET (0x1f << 8)
#define PCIE_PHY_PLL_CTLE_SET (0x4 << 4)
#define PCIE_PHY_FREQ_SET (0xFA << 1)
#define PCIE_PHY_PLL_CTRL1_MASK 0xFFFFF000
#define PCIE_X1_PHY_PLL_CTRL1_REG 0x10B8
#define PCIE_X2_PHY_PLL_CTRL1_REG 0x20B8
#define PCIE_PHY_CTRL_CONF1_MASK 0xFFFFE0FF
#define PCIE_PHY_CTRL_CONF2_MASK 0xFFFFEF0F
#define PCIE_X1_PHY_PLL_CTRL2_REG 0x10C4
#define PCIE_X2_PHY_PLL_CTRL2_REG 0x20C4
#define PCIE_PHY_LANE_CTRL_MASK 0xFFF8FFFF
#define PCIE_PHY_LANE0_CTRL_REG 0x0024
#define PCIE_PHY_LANE1_CTRL_REG 0x1024
#define PCIE_PHY_VGA0_GAIN_REG 0x0804
#define PCIE_PHY_VGA1_GAIN_REG 0x1804
#define PCIE_X1_PHY_PLL_LOCK_REG 0x13C0
#define PCIE_X2_PHY_PLL_LOCK_REG 0x23C0
#define PCIE_PHY_SPEED_MASK 0xFFFFF0FF /* PHY speed mask */
#define PCIE_PHY_EQ_ENABLE 0x1
#define PCIE_PHY_EQ_TUNING_SET 0x1
#define PCIE_PHY_EQ_TUNING_MASK 0xFFC0FFFF
#define PCIE_PHY_PRESET_MASK 0xFFFF80FF
#define PCIE_PHY_CONF_CTRL_LTSSM 0xFFFFFFFB
#define PCIE_PHY_FREQ_MASK 0xFF800001
#define PCIE_PHY_POR_RESET 0x3
#define PCIE_CFG_BUS_MASTER_EN 0x6
#define PCIE_ATR_AXI4_SLV0 0x800
#define PCIE_ATR_TRSLID_PCIE_MEMORY 0x0
#define IMASK_LOCAL 0x180
#define ISTATUS_LOCAL 0x184
#define IMSI_ADDR 0x190
#define ISTATUS_MSI 0x194
#define PCIE_INT_VAL_MASK 0xffffffff
#define INT_AXI_POST_ERROR BIT(16)
#define INT_AXI_FETCH_ERROR BIT(17)
#define INT_AXI_DISCARD_ERROR BIT(18)
#define INT_PCIE_POST_ERROR BIT(20)
#define INT_PCIE_FETCH_ERROR BIT(21)
#define INT_PCIE_DISCARD_ERROR BIT(22)

#define INT_ERRORS                                                          \
	(INT_AXI_POST_ERROR | INT_AXI_FETCH_ERROR | INT_AXI_DISCARD_ERROR | \
	 INT_PCIE_POST_ERROR | INT_PCIE_FETCH_ERROR | INT_PCIE_DISCARD_ERROR)

#define INTA_OFFSET 24
#define INTA BIT(24)
#define INTB BIT(25)
#define INTC BIT(26)
#define INTD BIT(27)
#define INT_MSI BIT(28)
#define INT_INTX_MASK (INTA | INTB | INTC | INTD)
#define INT_MASK (INT_INTX_MASK | INT_MSI | INT_ERRORS)

/* Bus offset for root port enumeration */
#define PCIE_WITH_BUS_OFFSET 0x100000
/* PCIe Root Port Adrressing space size */
#define PCIE_X2_MAP 0x10000000
#define PCIE_X1_MAP 0x8000000

#define PCIE_LINK_SPEED_1 0x1 /**< PCIe link speed 1*/
#define PCIE_LINK_SPEED_2 0x2 /**< PCIe link speed 2*/
#define PCIE_LINK_SPEED_3 0x3 /**< PCIe link speed 3*/
#define PCIE_LINK_SPEED_4 0x4 /**< PCIe link speed 4*/

/* PCIe bridge internal Address translation */
#define PCIE_P2A_XLATION_ADDR 0x40000000
#define PCIE_P2A_BAR0_XLATION_ADDR 0x48000000
#define PCIE_P2A_X2_BAR0_XLATION_ADDR 0x60000000

#define PCI_DEV(d) (((d) >> 3) & 0x1f)
#define MAX_DMA_LIST (16)
#define PCIE_INTF 0x00000000
#define AXI4_M0   0x00000004
#define AX_PCIE_DMA_VDM_TRSF_PARAM              (4)

struct axiado_pcie {
	struct device *dev;
	int irq;

	/* Device Class */
	bool is_root_port; /* If set, enables Root Port mode */
	bool is_vga; /* If set, initialises as a VGA device */
	bool is_eth; /* If set, initialises as a Network device */

	bool pcie_x1; /* Used for executing PCIe X1 specific task */
	bool pcie_x2; /* Used for executing PCIe X2 specific task */

	/* Memory Regions to be mapped */
	void __iomem *csr; /* CSR memory region */
	void __iomem *phy; /* PHY memory region */
	void __iomem *cfg; /* INT CFG memory region */
	void __iomem *base; /* RP base address */
	void __iomem *mbx; /* mailbox memory region */
	void __iomem *ioctl; /* IOCTL memory region */
	void __iomem *ext; /* EXT memory region */
	u64 phys_bar2; /* BAR2 region */
	u32 phys_bar2_size; /* BAR2 size */

	DECLARE_BITMAP(msi_irq_in_use, INT_PCI_MSI_NR);
	struct mutex map_lock;
	struct irq_domain *irq_domain;
	struct irq_domain *msi_domain;
	struct irq_domain *dev_domain;
	struct mutex lock;

	struct resource cs;

	struct list_head ports;
	unsigned int lanes; /* Number of lanes */
	int atr_table_num;

	void *sg_buffer_desc_src[MAX_DMA_LIST];
	dma_addr_t sg_buffer_dma_desc_src[MAX_DMA_LIST];
	void *sg_buffer_desc_dest[MAX_DMA_LIST];
	dma_addr_t sg_buffer_dma_desc_dest[MAX_DMA_LIST];
};

struct axiado_pcie_port {
	struct axiado_pcie *pcie;
	struct device_node *np;
	struct list_head list;
	struct resource regs;
	void __iomem *base;
	unsigned int index;
};

/* Common Function Declaration */
inline u32 axiado_pcie_ioread(void __iomem *base, u32 offset);
inline void axiado_pcie_iowrite(void __iomem *base, u32 offset, u32 val);
int axiado_pcie_pll_wait(struct axiado_pcie *pcie, unsigned long timeout);
int axiado_pcie_port_check_link(struct axiado_pcie_port *port, unsigned long timeout);

#endif /** __PCIE_AXIADO_H__ */
