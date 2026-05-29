// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2023-25 Axiado Corporation.
 *
 * Axiado PLDA PCIe Controller Driver
 *
 * Based on pci-tegra.c
 *
 * Copyright (c) 2010, CompuLab, Ltd.
 * Author: Mike Rapoport <mike@compulab.co.il>
 *
 * Based on NVIDIA PCIe driver
 * Copyright (c) 2008-2009, NVIDIA Corporation.
 *
 * Bits taken from arch/arm/mach-dove/pcie.c
 *
 * Author: Thierry Reding <treding@nvidia.com>
 */

#include <linux/platform_device.h>
#include <linux/of_address.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_irq.h>

#include "../pci.h"
#include "pcie-axiado.h"
#include <linux/soc/axiado/pcie-axiado.h>

#define EP_LINK_POLL_MS 1000
enum ax_dev_type {
    DEV_TYPE_SCM3000,
    DEV_TYPE_SCM3005,
};

static const struct of_device_id axiado_pcie_of_match[] = {
        {
                .compatible = "axiado,ax3000-pcie",.data = (void *)DEV_TYPE_SCM3000
        },
        {
                .compatible = "axiado,ax3005-pcie",.data = (void *)DEV_TYPE_SCM3005
        },
};

int axiado_pcie_bar_init(struct platform_device *pdev,
			 enum axiado_pcie_bars bar_num, u64 base_address,
			 u32 aperture_size);
static u32 axiado_pcie_range_to_bar_type(u32 flags);

inline u32 axiado_pcie_ioread(void __iomem *base, u32 offset)
{
	void __iomem *mmio_addr = NULL;

	mmio_addr = ((char *)(base)) + offset;
	return ioread32(mmio_addr);
}

inline void axiado_pcie_iowrite(void __iomem *base, u32 offset, u32 val)
{
	void __iomem *mmio_addr = NULL;

	mmio_addr = ((char *)(base)) + offset;
	iowrite32(val, mmio_addr);
}

int axiado_pcie_pll_wait(struct axiado_pcie *pcie, unsigned long timeout)
{
	u32 temp, offset;

	if (!pcie) {
		pr_err("Invalid Arguments passed for PLL check\n");
		return -EINVAL; /* The axiado_pcie structure passed for PLL check is invalid. */
	}

	timeout = jiffies + msecs_to_jiffies(timeout);
	if (pcie->pcie_x1)
		offset = PCIE_X1_PHY_PLL_LOCK_REG;
	else
		offset = PCIE_X2_PHY_PLL_LOCK_REG;

	while (time_before(jiffies, timeout)) {
		temp = axiado_pcie_ioread(pcie->phy, offset);
		if (temp != 0x0) {
			dev_info(pcie->dev, "PLL Locked for port x%d\n",
				 pcie->lanes);
			return 0;
		}
	}

	dev_info(pcie->dev, "PLL is not locked for port x%d. -EAGAIN\n",
		 pcie->lanes);
	return -EAGAIN;
}

/*
 * If there are no PCIe cards attached, then calling this function
 * can result in the increase of the bootup time as there are big timeout
 * loops.
 */
int axiado_pcie_port_check_link(struct axiado_pcie_port *port, unsigned long timeout)
{
	u32 temp, temp1;

	if (!port) {
		pr_err("Invalid Arguments passed for Link status check\n");
		return -EINVAL;
	}

	/* wait for LinkUp(at Gen1) */
	timeout = jiffies + msecs_to_jiffies(timeout);

	while (time_before(jiffies, timeout)) {
		temp = axiado_pcie_ioread(port->pcie->csr,
				      LINK_STATUS_LOW_POWER_ADDR);
		temp1 = (temp >> 8) & 0x1F;
		if (temp1 == 0x10) {
			dev_info(port->pcie->dev, "Link %u UP\n", port->index);
			return 0;
		}
	}

	dev_err(port->pcie->dev, "Link %u DOWN, ignoring\n", port->index);
	return -ENODEV;
}

static int axiado_pcie_get_resources(struct axiado_pcie *pcie)
{
	struct device *dev = pcie->dev;
	struct platform_device *pdev = to_platform_device(dev);
	struct resource *res;
	int err;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "csr");
	if (!res) {
		err = -EADDRNOTAVAIL;
		pr_err("axiado_pcie: Could not find csr resource\n");
		goto phys_put;
	}
	pcie->csr_phys = res->start;
	pcie->csr = devm_ioremap_resource(dev, res);
	if (IS_ERR(pcie->csr)) {
		err = PTR_ERR(pcie->csr);
		pr_err("axiado_pcie: Could not map csr config space address\n");
		goto phys_put;
	}

	pcie->phy = devm_platform_ioremap_resource_byname(pdev, "phy");
	if (IS_ERR(pcie->phy)) {
		err = PTR_ERR(pcie->phy);
		pr_err("axiado_pcie: Could not map phy config space address\n");
		goto phys_put;
	}

	/* request configuration space, but remap later, on demand */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "cfg");
	if (!res) {
		err = -EADDRNOTAVAIL;
		pr_err("axiado_pcie: Could not find int config space address\n");
		goto phys_put;
	}

	pcie->cs = *res;
	pcie->cfg_phys = res->start;

	/* constrain configuration space to 4 KiB */
	pcie->cs.end = pcie->cs.start + SZ_4K - 1;

	pcie->cfg = devm_ioremap_resource(dev, &pcie->cs);
	if (IS_ERR(pcie->cfg)) {
		err = PTR_ERR(pcie->cfg);
		pr_err("axiado_pcie: Could not map pcie config space address\n");
		goto phys_put;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "bridge");
	if (!res) {
		err = -EADDRNOTAVAIL;
		pr_err("axiado_pcie: Could not find bridge register resource\n");
		goto phys_put;
	}
	pcie->bridge_phys = res->start;
	pcie->bridge = devm_ioremap_resource(dev, res);
	if (IS_ERR(pcie->bridge)) {
		err = PTR_ERR(pcie->bridge);
		pr_err("axiado_pcie: Could not map bridge register space\n");
		goto phys_put;
	}

	pcie->mbx = devm_platform_ioremap_resource_byname(pdev, "mbx");
	if (IS_ERR(pcie->mbx)) {
		err = PTR_ERR(pcie->mbx);
		pr_err("axiado_pcie: Could not map mailbox address space\n");
		goto phys_put;
	}
	if (pcie->pcie_x1) {
		pcie->ioctl =
			devm_platform_ioremap_resource_byname(pdev, "ioctl");
		if (IS_ERR(pcie->ioctl)) {
			err = PTR_ERR(pcie->ioctl);
			pr_err("axiado_pcie: Could not map ioctl address space\n");
		}
	}
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ext");
	if (!res) {
		err = -EADDRNOTAVAIL;
		pr_err("axiado_pcie: Could not find ext resource\n");
		goto phys_put;
	}
	pcie->ext_phys = res->start;
	pcie->ext = devm_ioremap_resource(dev, res);
	if (IS_ERR(pcie->ext)) {
		err = PTR_ERR(pcie->ext);
		pr_err("axiado_pcie: Could not map int external config space address\n");
		goto phys_put;
	}

	return 0;

phys_put:
	return err;
}

static int axiado_pcie_parse_dt(struct axiado_pcie *pcie)
{
	struct device *dev = pcie->dev;
	struct device_node *np = dev->of_node, *port;
	u32 lanes;
	int err = -EINVAL;

	if (np == NULL)
		goto err_node_put;

	pcie->irq = irq_of_parse_and_map(np, 0);
	if (!pcie->irq) {
		err = -ENXIO;
		goto err_node_put;
	}

	err = of_property_read_u32(np, "num-lanes", &lanes);
	if (err < 0) {
		dev_err(dev, "failed to parse # of lanes: %d\n", err);
		goto err_node_put;
	}

	if (lanes > 2) {
		dev_err(dev, "invalid # of lanes: %d\n", lanes);
		err = -EINVAL;
		goto err_node_put;
	} else
		pcie->lanes = lanes;

	if (pcie->lanes == 1) {
		pcie->pcie_x1 = true;
		pcie->pcie_x2 = false;
	} else if (pcie->lanes == 2) {
		pcie->pcie_x1 = false;
		pcie->pcie_x2 = true;
	}

	/* Checks for the device class */
	err = of_property_match_string(np, "device_role", "host");
	if (err < 0) {
		err = of_property_match_string(np, "device_role", "vga");
		if (err < 0) {
			err = of_property_match_string(np, "device_role",
						       "eth");
			if (err < 0) {
				dev_err(dev,
					"Invalid device class entry in Device tree\n");
				goto err_node_put;
			} else {
				dev_info(dev,
					 "Initialising as a Network Card\n");
				pcie->is_eth = true;
			}
		} else {
			dev_info(dev, "Initialising as a Display Card\n");
			pcie->is_vga = true;
		}

	} else {
		dev_info(dev, "Initialising as a Host Bridge\n");
		pcie->is_root_port = true;

		/* Parse root ports */
		for_each_child_of_node(np, port) {
			struct axiado_pcie_port *rp;
			unsigned int index;

			err = of_pci_get_devfn(port);
			if (err < 0) {
				dev_err(dev,
					"failed to parse address for port: %d\n",
					err);
				goto err_node_put;
			}

			index = PCI_SLOT(err);
			if (!of_device_is_available(port))
				continue;

			rp = devm_kzalloc(dev, sizeof(*rp), GFP_KERNEL);
			if (!rp) {
				err = -ENOMEM;
				goto err_node_put;
			}

			err = of_address_to_resource(port, 0, &rp->regs);
			if (err < 0) {
				dev_err(dev, "failed to parse address: %d\n",
					err);
				goto err_node_put;
			}

			INIT_LIST_HEAD(&rp->list);
			rp->index = index;
			rp->pcie = pcie;
			rp->np = port;

			rp->base = devm_pci_remap_cfg_resource(dev, &rp->regs);
			if (IS_ERR(rp->base)) {
				err = PTR_ERR(rp->base);
				goto err_node_put;
			}
			pcie->base = rp->base;
			list_add_tail(&rp->list, &pcie->ports);
		}
	}

	return 0;

err_node_put:
	of_node_put(port);
	return err;
}

static unsigned int axiado_pcie_conf_offset(u8 bus, unsigned int devfn,
					unsigned int where)
{
	return (bus << 20) | (PCI_DEV(devfn) << 15) | (PCI_FUNC(devfn) << 12) |
	       where;
}

static void __iomem *axiado_pcie_map_bus(struct pci_bus *bus, unsigned int devfn,
				     int where)
{
	struct axiado_pcie *pcie = bus->sysdata;
	void __iomem *addr = NULL;

	if (bus->number == 0) {
		unsigned int slot = PCI_SLOT(devfn);
		struct axiado_pcie_port *port;

		list_for_each_entry(port, &pcie->ports, list) {
			if (port->index + 1 == slot) {
				addr = port->base + (where & ~3);
				break;
			}
		}
	} else {
		unsigned int offset;

		offset = axiado_pcie_conf_offset(bus->number, devfn, where);
		/* move to correct offset within the 4 KiB page */
		addr = pcie->base + (PCIE_WITH_BUS_OFFSET * bus->number) +
		       (offset & (SZ_4K - 1));
	}
	return addr;
}

static int axiado_pcie_config_read(struct pci_bus *bus, unsigned int devfn,
			       int where, int size, u32 *value)
{
	if (bus->number == 0)
		return pci_generic_config_read32(bus, devfn, where, size,
						 value);

	return pci_generic_config_read(bus, devfn, where, size, value);
}

static int axiado_pcie_config_write(struct pci_bus *bus, unsigned int devfn,
				int where, int size, u32 value)
{
	if (bus->number == 0)
		return pci_generic_config_write32(bus, devfn, where, size,
						  value);

	return pci_generic_config_write(bus, devfn, where, size, value);
}

void ax_set_atr_entry(struct axiado_pcie *pcie, phys_addr_t src_addr,
		      phys_addr_t trsl_addr, size_t window_size, int trsl_param)
{
	u32 offset = PCIE_ATR_AXI4_SLV0;

	if (pcie->atr_table_num >= PCIE_ATR_MAX_TABLE_NUM)
		pcie->atr_table_num = PCIE_ATR_MAX_TABLE_NUM - 1;
	offset +=  PCIE_ATR_TABLE_OFFSET * pcie->atr_table_num;
	pcie->atr_table_num++;

	/* PCIE_ATR_SRC_ADDR_LOW:
	 *   - bit 0: enable entry,
	 *   - bits 1-6: ATR window size: total size in bytes: 2^(ATR_WSIZE + 1)
	 *   - bits 7-11: reserved
	 *   - bits 12-31: start of source address
	 */
	axiado_pcie_iowrite(pcie->bridge, (offset+PCIE_ATR_SRC_ADDR_LOW),
			(lower_32_bits(src_addr) & PCIE_ATR_SRC_ADDR_MASK) |
			(ilog2(window_size) - 1) << PCIE_ATR_SRC_WIN_SIZE_SHIFT |
			1);
	axiado_pcie_iowrite(pcie->bridge, (offset+PCIE_ATR_SRC_ADDR_HIGH),
			(upper_32_bits(src_addr)));

	axiado_pcie_iowrite(pcie->bridge, (offset+PCIE_ATR_TRSL_ADDR_LOW),
			(lower_32_bits(trsl_addr) & PCIE_ATR_TRSL_ADDR_MASK));

	axiado_pcie_iowrite(pcie->bridge, (offset+PCIE_ATR_TRSL_ADDR_HIGH),
			(upper_32_bits(trsl_addr)));

	axiado_pcie_iowrite(pcie->bridge, (offset+PCIE_ATR_TRSL_PARAM), trsl_param);

	pr_info("ATR entry: 0x%010llx %s 0x%010llx [0x%010llx] (param: 0x%06x)\n",
		src_addr, (trsl_param & PCIE_ATR_TRSL_DIR) ? "<-" : "->",
		trsl_addr, (u64)window_size, trsl_param);
}

static int axiado_pcie_setup_windows(struct axiado_pcie *pcie)
{
	struct pci_host_bridge *bridge = pci_host_bridge_from_priv(pcie);
	struct resource_entry *entry;
	u64 pci_addr;

	resource_list_for_each_entry(entry, &bridge->windows) {
		if (resource_type(entry->res) != IORESOURCE_MEM)
			continue;
		pci_addr = entry->res->start - entry->offset;
		ax_set_atr_entry(pcie, entry->res->start, pci_addr,
				 resource_size(entry->res),
				 PCIE_ATR_TRSLID_PCIE_MEMORY);
	}

	return 0;
}


static void axiado_pcie_ep_setup_p2a_atr(struct axiado_pcie *pcie,
					  u64 bar0_pci, u64 bar2_pci)
{
	u32 atr_base;

	/*
	 * bootloader style: SRC addr=0, just enable + window size.
	 * In EP mode the PLDA IP routes BAR hits to TAB entries
	 * based on BAR number, not SRC address matching.
	 * We only need to set TRSL_ADDR (AXI target) and TRSL_PARAM.
	 */

	/* TAB0: BAR0 1MB → 0x89300000 */
	atr_base = PCIE_ATR_PCIE_WIN0;
	axiado_pcie_iowrite(pcie->bridge, atr_base + PCIE_ATR_SRC_ADDR_LOW,
			    ((ilog2(pcie->ep_bar0_size) - 1) << PCIE_ATR_SRC_WIN_SIZE_SHIFT) | 1);
	axiado_pcie_iowrite(pcie->bridge, atr_base + PCIE_ATR_SRC_ADDR_HIGH, 0);
	axiado_pcie_iowrite(pcie->bridge, atr_base + PCIE_ATR_TRSL_ADDR_LOW,
			    (u32)pcie->ep_bar0_addr & PCIE_ATR_TRSL_ADDR_MASK);
	axiado_pcie_iowrite(pcie->bridge, atr_base + PCIE_ATR_TRSL_ADDR_HIGH, 0);
	axiado_pcie_iowrite(pcie->bridge, atr_base + PCIE_ATR_TRSL_PARAM,
			    PCIE_ATR_TRSLID_AXI4_MASTER_0);

	/* TAB2: BAR2 64MB → 0x8C000000 */
	atr_base = PCIE_ATR_PCIE_WIN0 + 2 * PCIE_ATR_TABLE_OFFSET;
	axiado_pcie_iowrite(pcie->bridge, atr_base + PCIE_ATR_SRC_ADDR_LOW,
			    ((ilog2(pcie->ep_bar1_size) - 1) << PCIE_ATR_SRC_WIN_SIZE_SHIFT) | 1);
	axiado_pcie_iowrite(pcie->bridge, atr_base + PCIE_ATR_SRC_ADDR_HIGH, 0);
	axiado_pcie_iowrite(pcie->bridge, atr_base + PCIE_ATR_TRSL_ADDR_LOW,
			    (u32)pcie->ep_bar1_addr & PCIE_ATR_TRSL_ADDR_MASK);
	axiado_pcie_iowrite(pcie->bridge, atr_base + PCIE_ATR_TRSL_ADDR_HIGH, 0);
	axiado_pcie_iowrite(pcie->bridge, atr_base + PCIE_ATR_TRSL_PARAM,
			    PCIE_ATR_TRSLID_AXI4_MASTER_0);

	dev_info(pcie->dev,
		 "P2A ATR: TAB0 src=0 trsl=0x%llx (%dMB) TAB2 src=0 trsl=0x%llx (%dMB)\n",
		 pcie->ep_bar0_addr, (int)(pcie->ep_bar0_size >> 20),
		 pcie->ep_bar1_addr, (int)(pcie->ep_bar1_size >> 20));
}

static void axiado_pcie_ep_link_work(struct work_struct *work)
{
	struct axiado_pcie *pcie =
		container_of(work, struct axiado_pcie, ep_link_work.work);
	u32 reg, ltssm;

	reg = axiado_pcie_ioread(pcie->csr, LINK_STATUS_LOW_POWER_ADDR);
	ltssm = (reg >> 8) & 0x1F;

	if (ltssm == 0x10) {
		if (!pcie->ep_link_up) {
			pcie->ep_link_up = true;
			dev_info(pcie->dev, "EP link UP LTSSM=0x%x\n", ltssm);
		}
	} else {
		if (pcie->ep_link_up) {
			pcie->ep_link_up = false;
			dev_info(pcie->dev, "EP link DOWN LTSSM=0x%x\n", ltssm);
		}
		schedule_delayed_work(&pcie->ep_link_work,
				msecs_to_jiffies(EP_LINK_POLL_MS));
	}

}

static int axiado_pcie_init(struct axiado_pcie *pcie)
{
	u32 temp = 0;
	u32 speed = 0x1;
	struct axiado_pcie_port *port;
	u32 offset = 0x0;

	/*
	 * TBD: Reset the PCIe controller, only in x1 case. Both x1 and x2
	 * are having the same region for IOCTL. The same address cannot
	 * be accessed by both x1 and x2 at the same time.
	 **/
	if (pcie->pcie_x1)
		offset = REG_PAD_CFG_REG_25_ADRS_OFFSET;
	else if (pcie->pcie_x2)
		offset = REG_PAD_CFG_REG_26_ADRS_OFFSET;

	temp = axiado_pcie_ioread(pcie->ioctl, offset);
	temp = temp | PCIE_PERST_PULLUP_SET;
	axiado_pcie_iowrite(pcie->ioctl, offset, temp);
	/* 1)
	 * For x1:
	 *	14'h10b8 address, bit[11:0] write 12'h032
	 * For x2:
	 *	14'h20b8 address, bit[11:0] write 12'h032
	 **/

	if (pcie->scm_version == DEV_TYPE_SCM3000) {
		if (pcie->pcie_x1)
			offset = PCIE_X1_PHY_PLL_CTRL1_REG;
		else if (pcie->pcie_x2)
			offset = PCIE_X2_PHY_PLL_CTRL1_REG;

		temp = axiado_pcie_ioread(pcie->phy, offset);
		temp = (temp & PCIE_PHY_PLL_CTRL1_MASK) | 0x32;
		axiado_pcie_iowrite(pcie->phy, offset, temp);
		temp = axiado_pcie_ioread(pcie->phy, offset);

		/* 2)
		 * For x1:
		 *	14'h10c4 address, bit[12:8] write 5'h03
		 * For x2:
		 *	14'h20c4 address, bit[12:8] write 5'h03
		 **/
		if (pcie->pcie_x1)
			offset = PCIE_X1_PHY_PLL_CTRL2_REG;
		else if (pcie->pcie_x2)
			offset = PCIE_X2_PHY_PLL_CTRL2_REG;

		temp = axiado_pcie_ioread(pcie->phy, offset);
		temp = (temp & PCIE_PHY_CTRL_CONF1_MASK) | PCIE_PHY_PLL_CTRL1_SET;
		axiado_pcie_iowrite(pcie->phy, offset, temp);
		temp = axiado_pcie_ioread(pcie->phy, offset);

		/* 3) 14'h0024/14'h1024 address, bit[18:16] write 3'b100 for
		 * lane0/lane1 respectively
		 **/
		temp = axiado_pcie_ioread(pcie->phy, PCIE_PHY_LANE0_CTRL_REG);
		temp = (temp & PCIE_PHY_LANE_CTRL_MASK) | PCIE_PHY_PLL_CTRL2_SET;
		axiado_pcie_iowrite(pcie->phy, PCIE_PHY_LANE0_CTRL_REG, temp);
		temp = axiado_pcie_ioread(pcie->phy, PCIE_PHY_LANE0_CTRL_REG);

		if (pcie->pcie_x2) { /* For x2 only */
			temp = axiado_pcie_ioread(pcie->phy, PCIE_PHY_LANE1_CTRL_REG);
			temp = (temp & PCIE_PHY_LANE_CTRL_MASK) |
				PCIE_PHY_PLL_CTRL2_SET;
			axiado_pcie_iowrite(pcie->phy, PCIE_PHY_LANE1_CTRL_REG, temp);
			temp = axiado_pcie_ioread(pcie->phy, PCIE_PHY_LANE1_CTRL_REG);
		}

		/* 4) To adjust vga gain of PCIe x2 PHY, write 5'b11111 to bit[12:8]
		 * of address 14'h0804/14'h1804 for lane0/1 respectively
		 */
		temp = axiado_pcie_ioread(pcie->phy, PCIE_PHY_VGA0_GAIN_REG);
		temp = (temp & PCIE_PHY_CTRL_CONF1_MASK) | PCIE_PHY_PLL_VGA_SET;
		axiado_pcie_iowrite(pcie->phy, PCIE_PHY_VGA0_GAIN_REG, temp);
		temp = axiado_pcie_ioread(pcie->phy, PCIE_PHY_VGA0_GAIN_REG);

		if (pcie->pcie_x2) { /* For x2 only */
			temp = axiado_pcie_ioread(pcie->phy, PCIE_PHY_VGA1_GAIN_REG);
			temp = (temp & PCIE_PHY_CTRL_CONF1_MASK) | PCIE_PHY_PLL_VGA_SET;
			axiado_pcie_iowrite(pcie->phy, PCIE_PHY_VGA1_GAIN_REG, temp);
			temp = axiado_pcie_ioread(pcie->phy, PCIE_PHY_VGA1_GAIN_REG);
		}

		/*
		 * For x1:
		 *	13'h0804 address bit[7:4], value = 0x4 -- CTLE setting
		 * For x2:
		 *	13'h1804 address bit[7:4], value = 0x4 -- CTLE setting
		 **/
		temp = axiado_pcie_ioread(pcie->phy, PCIE_PHY_VGA0_GAIN_REG);
		temp = (temp & PCIE_PHY_CTRL_CONF2_MASK) | PCIE_PHY_PLL_CTLE_SET;
		axiado_pcie_iowrite(pcie->phy, PCIE_PHY_VGA0_GAIN_REG, temp);
		temp = axiado_pcie_ioread(pcie->phy, PCIE_PHY_VGA0_GAIN_REG);

		if (pcie->pcie_x2) { /* For x2 only */
			temp = axiado_pcie_ioread(pcie->phy, PCIE_PHY_VGA1_GAIN_REG);
			temp = (temp & PCIE_PHY_CTRL_CONF2_MASK) |
				PCIE_PHY_PLL_CTLE_SET;
			axiado_pcie_iowrite(pcie->phy, PCIE_PHY_VGA1_GAIN_REG, temp);
			temp = axiado_pcie_ioread(pcie->phy, PCIE_PHY_VGA1_GAIN_REG);
		}
		/* POR reset */
		if (pcie->pcie_x1)
			axiado_pcie_iowrite(pcie->ext, REG_PCIE_X1_RESET_CTRL_ADRS_OFFSET,
					PCIE_PHY_POR_RESET);
		else if (pcie->pcie_x2)
			axiado_pcie_iowrite(pcie->ext, REG_PCIE_X2_RESET_CTRL_ADRS_OFFSET,
					PCIE_PHY_POR_RESET);
	}

	if (pcie->scm_version == DEV_TYPE_SCM3005) {
		axiado_pcie_iowrite(pcie->ext, REG_PCIE_X1_GEN_OFF, REG_PCIE_X1_GEN_VAL);
	}
	/* Set supported speeds and RP/EP in k_gen settings */
	if (pcie->pcie_x1)
		temp = axiado_pcie_ioread(pcie->ext, REG_PCIE_X1_IP_CTRL_ADRS_OFFSET);
	else if (pcie->pcie_x2)
		temp = axiado_pcie_ioread(pcie->ext, REG_PCIE_X2_IP_CTRL_ADRS_OFFSET);

	if (pcie->is_root_port) {
		/* set GEN_SETTINGS BIT0 to set RP mode */
		temp = temp | 0x1; // set RP
	}
	if (speed == 0x1)
		temp = (temp & PCIE_PHY_SPEED_MASK);
	else if (speed == 0x2)
		temp = (temp & PCIE_PHY_SPEED_MASK) | PCIE_PHY_GEN1;
	else if (speed == 0x3)
		temp = (temp & PCIE_PHY_SPEED_MASK) | PCIE_PHY_GEN2;
	else
		temp = temp | PCIE_PHY_GEN3;

	if (pcie->pcie_x1)
		axiado_pcie_iowrite(pcie->ext, REG_PCIE_X1_IP_CTRL_ADRS_OFFSET,
				temp);
	else if (pcie->pcie_x2)
		axiado_pcie_iowrite(pcie->ext, REG_PCIE_X2_GEN_SETTINGS_ADRS_OFFSET,
				temp);

	if (pcie->is_root_port) {
		/* Change class to PCIe Bridge */
		axiado_pcie_iowrite(pcie->bridge,
				REG_PCIE_PCIE_PCI_IDS_63_32_ADRS_OFFSET,
				PCIE_BRIDGE_CLASS_CODE);
	} else if (pcie->is_vga) {
		/* Change class to VGA (PCIe End-Point) */
		axiado_pcie_iowrite(pcie->bridge,
				REG_PCIE_PCIE_PCI_IDS_63_32_ADRS_OFFSET,
				PCIE_VGA_CLASS_CODE);
	} else if (pcie->is_eth) {
		/* Change class to network (PCIe End-Point) */
		axiado_pcie_iowrite(pcie->bridge,
				REG_PCIE_PCIE_PCI_IDS_63_32_ADRS_OFFSET,
				PCIE_ETHERNET_CLASS_CODE);
	}

	/* enable EQ PH2,3 */
	temp = axiado_pcie_ioread(pcie->bridge, REG_PCIE_PHYMAC_CFG_ADRS_OFFSET);
	axiado_pcie_iowrite(pcie->bridge, REG_PCIE_PHYMAC_CFG_ADRS_OFFSET,
			temp | PCIE_PHY_EQ_ENABLE);
	temp = axiado_pcie_ioread(pcie->bridge, REG_PCIE_PHYMAC_CFG_ADRS_OFFSET);
	/* set max fine tuning attempts to 0 and presets to try 1,3
	 * //@!@ may need updates in actual bringup
	 */
	if (pcie->scm_version == DEV_TYPE_SCM3005) {
		temp = axiado_pcie_ioread(pcie->bridge, REG_PCIE_X1_PCIE_PEX_SPC_ADRS_OFFSET);
		temp = temp | 0x3000;
		axiado_pcie_iowrite(pcie->bridge, REG_PCIE_X1_PCIE_PEX_SPC_ADRS_OFFSET, temp);
	} else if (pcie->scm_version == DEV_TYPE_SCM3000) {
		temp = axiado_pcie_ioread(pcie->bridge,
				REG_PCIE_EQ_TUNING_63_32_ADRS_OFFSET);
		temp = temp &
			PCIE_PHY_EQ_TUNING_MASK; /* 0 out bits 53:48(fine tuning) */
		temp = temp & PCIE_PHY_PRESET_MASK; /* presets to test 46:36 */
		axiado_pcie_iowrite(pcie->bridge, REG_PCIE_EQ_TUNING_63_32_ADRS_OFFSET,
				temp | PCIE_PHY_EQ_TUNING_SET);
	} else {
		dev_err(pcie->dev, "Unsupported device matched, not ax3xxx device!");
	}

	if (pcie->is_root_port) {
		/* wait for PHY PLL lock -- Inno PCIe PHY doc says
		 * PLL lock status is at offset 14'h23C0? but paddr
		 * for PHY model is 12:0
		 */
		temp = axiado_pcie_pll_wait(pcie, PCIE_LINKUP_TIMEOUT);
		if (temp == -EAGAIN) {
			dev_dbg(pcie->dev,
					"PLL is not locked for port x%d, ignoring\n",
					pcie->lanes);
			/*
			 * If PLL is not locked yet, it is not to be considered
			 * as a fatal error Please check AXBUGS-1250
			 */
		} else if (temp == -EINVAL)
			return temp;
		/* The axiado_pcie structure passed for PLL
		 * check is invalid.
		 */
	}

	if (pcie->is_vga) {
		u32 bar_val;

		/*
		 * BAR registers in bridge space encode both size mask
		 * and type.  Size mask = ~(size - 1) & 0xFFFFFFF0,
		 * type goes in bits [3:0].  BAR1/BAR3 are upper 32
		 * bits for 64-bit BARs.
		 *
		 * Must be configured while IP is in reset.
		 */

		/* BAR0: size + type from DTS ranges flags */
		bar_val = (~(pcie->ep_bar0_size - 1) & 0xFFFFFFF0) |
			  axiado_pcie_range_to_bar_type(pcie->ep_bar0_flags);
		axiado_pcie_iowrite(pcie->bridge, PCIE_BAR_01_OFFSET, bar_val);
		/* BAR1: upper 32 bits for 64-bit BAR0 */
		axiado_pcie_iowrite(pcie->bridge, PCIE_BAR_01_OFFSET + 4, 0xFFFFFFFF);

		/* BAR2: size + type */
		bar_val = (~(pcie->ep_bar1_size - 1) & 0xFFFFFFF0) |
			  axiado_pcie_range_to_bar_type(pcie->ep_bar1_flags);
		axiado_pcie_iowrite(pcie->bridge, PCIE_BAR_23_OFFSET, bar_val);
		/* BAR3: upper 32 bits for 64-bit BAR2 */
		axiado_pcie_iowrite(pcie->bridge, PCIE_BAR_23_OFFSET + 4,
				    0xFFFFFFFF);

		/* Disable BAR4/5 — not used, clear default 4KB */
		axiado_pcie_iowrite(pcie->bridge, PCIE_BAR_45_OFFSET, 0x0);
		axiado_pcie_iowrite(pcie->bridge, PCIE_BAR_45_OFFSET + 4, 0x0);
	}

	/* release PIPE_RST_N for PCIe */
	if (pcie->pcie_x1)
		axiado_pcie_iowrite(pcie->ext, REG_PCIE_X1_RESET_CTRL_ADRS_OFFSET,
				0x5);
	else if (pcie->pcie_x2) {
		if (pcie->scm_version == DEV_TYPE_SCM3000) {
			axiado_pcie_iowrite(pcie->ext, REG_PCIE_X2_RESET_CTRL_ADRS_OFFSET,
					    0x1);
		} else {
			axiado_pcie_iowrite(pcie->ext, REG_PCIE_X2_RESET_CTRL_ADRS_OFFSET,
					    0x5);
		}

	}
	usleep_range(1000, 2000);

	if (pcie->is_vga) {
		dev_info(pcie->dev,
			 "BAR01[%#llx]=%#x/%#x BAR23[%#llx]=%#x/%#x BAR45[%#llx]=%#x/%#x\n",
			 (u64)pcie->bridge_phys + PCIE_BAR_01_OFFSET,
			 axiado_pcie_ioread(pcie->bridge, PCIE_BAR_01_OFFSET),
			 axiado_pcie_ioread(pcie->bridge, PCIE_BAR_01_OFFSET + 4),
			 (u64)pcie->bridge_phys + PCIE_BAR_23_OFFSET,
			 axiado_pcie_ioread(pcie->bridge, PCIE_BAR_23_OFFSET),
			 axiado_pcie_ioread(pcie->bridge, PCIE_BAR_23_OFFSET + 4),
			 (u64)pcie->bridge_phys + PCIE_BAR_45_OFFSET,
			 axiado_pcie_ioread(pcie->bridge, PCIE_BAR_45_OFFSET),
			 axiado_pcie_ioread(pcie->bridge, PCIE_BAR_45_OFFSET + 4));
		dev_info(pcie->dev,
			 "CFGCTRL[%#llx]=%#x GEN[%#llx]=%#x\n",
			 (u64)pcie->bridge_phys + 0x84,
			 axiado_pcie_ioread(pcie->bridge, 0x84),
			 (u64)pcie->bridge_phys + 0x80,
			 axiado_pcie_ioread(pcie->bridge, 0x80));
	}

	if (pcie->scm_version == DEV_TYPE_SCM3000) {
		/* set RP mode(in ext regs, a soft strap to the IP) */
		if (pcie->pcie_x1)
			offset = REG_PCIE_X1_IP_CTRL_ADRS_OFFSET;
		else if (pcie->pcie_x2)
			offset = REG_PCIE_X2_IP_CTRL_ADRS_OFFSET;

		temp = axiado_pcie_ioread(pcie->ext, offset);
		temp = (temp & PCIE_PHY_FREQ_MASK) |
			PCIE_PHY_FREQ_SET; /* set tl_clk freq to 250MHz from default */
		if (pcie->is_root_port)
			axiado_pcie_iowrite(pcie->ext, offset, temp | 0x1);
		else
			axiado_pcie_iowrite(pcie->ext, offset, temp);
	}
	if (pcie->is_root_port) {
		/* Bus master enable */
		usleep_range(10, 20); /* 10uS sleep for the BME. */
		temp = axiado_pcie_ioread(pcie->base,
				REG_PCIE_X1_COMMAND_STATUS_ADRS_OFFSET);
		axiado_pcie_iowrite(pcie->base,
				REG_PCIE_X1_COMMAND_STATUS_ADRS_OFFSET,
				temp | PCIE_CFG_BUS_MASTER_EN);
		/* Setup A2P address translation table */
		axiado_pcie_setup_windows(pcie);
	} else {
		/* Bus master + memory space enable in EP config space */
		usleep_range(10, 20);
		temp = axiado_pcie_ioread(pcie->cfg,
				REG_PCIE_X1_COMMAND_STATUS_ADRS_OFFSET);
		axiado_pcie_iowrite(pcie->cfg,
				    REG_PCIE_X1_COMMAND_STATUS_ADRS_OFFSET,
				    temp | PCIE_CFG_BUS_MASTER_EN);
		dev_info(pcie->dev, "EP CMD [0x%llx]: 0x%x -> 0x%x\n",
			 (u64)pcie->cfg_phys + REG_PCIE_X1_COMMAND_STATUS_ADRS_OFFSET,
			 temp, temp | PCIE_CFG_BUS_MASTER_EN);

		/* P2A ATR will be configured by EP link monitor after
		 * host enumerates and assigns BAR addresses.
		 * Pre-program translation targets now, source addresses
		 * will be set once we read host-assigned BARs.
		 */
		axiado_pcie_ep_setup_p2a_atr(pcie, 0, 0);
		pcie->ep_bar0_pci = 0;
		pcie->ep_bar2_pci = 0;
		dev_info(pcie->dev,
			 "P2A ATR pre-configured (src=0, will update after host enum)\n");
	}

	/* enable LTSSM — clear DISABLE_LTSSM (bit 2) in CFGCTRL */
	temp = axiado_pcie_ioread(pcie->bridge, REG_PCIE_PCIE_CFGCTRL_ADRS_OFFSET);
	dev_info(pcie->dev, "LTSSM enable [0x%llx]: 0x%x -> 0x%x\n",
		 (u64)pcie->bridge_phys + REG_PCIE_PCIE_CFGCTRL_ADRS_OFFSET,
		 temp, temp & PCIE_PHY_CONF_CTRL_LTSSM);
	axiado_pcie_iowrite(pcie->bridge, REG_PCIE_PCIE_CFGCTRL_ADRS_OFFSET,
			temp & PCIE_PHY_CONF_CTRL_LTSSM);

	if (pcie->is_root_port) {
		/* Check the link status for Ports in RP mode */
		list_for_each_entry(port, &pcie->ports, list) {
			temp = axiado_pcie_port_check_link(port,
						       PCIE_LINKUP_TIMEOUT);
			if (temp == -EINVAL)
				return temp;
		}
	} else {
		/* Start deferred EP link monitor — host may not be ready yet */
		pcie->ep_link_up = false;
		INIT_DELAYED_WORK(&pcie->ep_link_work,
				  axiado_pcie_ep_link_work);
		schedule_delayed_work(&pcie->ep_link_work,
				      msecs_to_jiffies(EP_LINK_POLL_MS));
		dev_info(pcie->dev, "EP link monitor started (poll %dms)\n",
			 EP_LINK_POLL_MS);
	}

	return 0;
}

static int axiado_pcie_intx_map(struct irq_domain *domain, unsigned int irq,
			    irq_hw_number_t hwirq)
{
	irq_set_chip_and_handler(irq, &dummy_irq_chip, handle_simple_irq);
	irq_set_chip_data(irq, domain->host_data);

	return 0;
}

/* INTx IRQ Domain operations */
static const struct irq_domain_ops ax_intx_domain_ops = {
	.map = axiado_pcie_intx_map,
	.xlate = pci_irqd_intx_xlate,
};

static void axiado_pcie_handle_intx_irq(struct axiado_pcie *pcie, unsigned long status)
{
	u32 bit;

	for_each_set_bit(bit, &status, PCI_NUM_INTX) {
		axiado_pcie_iowrite(pcie->bridge, ISTATUS_LOCAL, 1 << (bit + INTA_OFFSET));
		generic_handle_domain_irq(pcie->irq_domain, bit);
	}
}

static void axiado_pcie_handle_msi_irq(struct axiado_pcie *pcie)
{
	unsigned long msi_status;
	u32 bit;
	u32 virq;

	msi_status = axiado_pcie_ioread(pcie->bridge, ISTATUS_MSI);
	for_each_set_bit(bit, &msi_status, INT_PCI_MSI_NR) {
		/* Clear interrupts */
		axiado_pcie_iowrite(pcie->bridge, ISTATUS_MSI, 1 << bit);
		virq = irq_find_mapping(pcie->dev_domain, bit);
		if (virq) {
			if (test_bit(bit, pcie->msi_irq_in_use))
				generic_handle_irq(virq);
			else
				dev_err(pcie->dev,
					"Unhandled MSI, MSI%d virq %d\n", bit,
					virq);
		} else
			dev_err(pcie->dev, "Unexpected MSI, MSI%d\n", bit);
	}
	axiado_pcie_iowrite(pcie->bridge, ISTATUS_LOCAL, INT_MSI);
}

static void axiado_pcie_handle_errors_irq(struct axiado_pcie *pcie, u32 status)
{
	if (status & INT_AXI_POST_ERROR)
		dev_err(pcie->dev, "AXI post error\n");
	if (status & INT_AXI_FETCH_ERROR)
		dev_err(pcie->dev, "AXI fetch error\n");
	if (status & INT_AXI_DISCARD_ERROR)
		dev_err(pcie->dev, "AXI discard error\n");
	if (status & INT_PCIE_POST_ERROR)
		dev_err(pcie->dev, "PCIe post error\n");
	if (status & INT_PCIE_FETCH_ERROR)
		dev_err(pcie->dev, "PCIe fetch error\n");
	if (status & INT_PCIE_DISCARD_ERROR)
		dev_err(pcie->dev, "PCIe discard error\n");

	axiado_pcie_iowrite(pcie->bridge, ISTATUS_LOCAL, INT_ERRORS);
}
static void axiado_pcie_intr_handler(struct irq_desc *desc)
{
	struct axiado_pcie *pcie = irq_desc_get_handler_data(desc);
	struct irq_chip *irqchip = irq_desc_get_chip(desc);
	unsigned long status = 0;

	chained_irq_enter(irqchip, desc);

	status = axiado_pcie_ioread(pcie->bridge, ISTATUS_LOCAL) & INT_MASK;

	if (status & INT_INTX_MASK)
		axiado_pcie_handle_intx_irq(pcie, status);

	if (IS_ENABLED(CONFIG_PCI_MSI)) {
		if (status & INT_MSI)
			axiado_pcie_handle_msi_irq(pcie);
	}

	if (status & INT_ERRORS)
		axiado_pcie_handle_errors_irq(pcie, status);

	chained_irq_exit(irqchip, desc);
}

static void ax_msi_compose_msi_msg(struct irq_data *data, struct msi_msg *msg)
{
	struct axiado_pcie *pcie = irq_data_get_irq_chip_data(data);
	phys_addr_t msi_addr = axiado_pcie_ioread(pcie->bridge, IMSI_ADDR);

	msg->address_lo = lower_32_bits(msi_addr);
	msg->address_hi = upper_32_bits(msi_addr);
	msg->data = data->hwirq;
}

static int ax_msi_set_affinity(struct irq_data *irq_data,
			       const struct cpumask *mask, bool force)
{
	return -EINVAL;
}

static struct irq_chip ax_msi_bottom_irq_chip = {
	.name = "AXIADO MSI",
	.irq_compose_msi_msg = ax_msi_compose_msi_msg,
	.irq_set_affinity = ax_msi_set_affinity,
};

static int ax_irq_msi_domain_alloc(struct irq_domain *domain, unsigned int virq,
				   unsigned int nr_irqs, void *args)
{
	struct axiado_pcie *pcie = domain->host_data;
	int bit;
	int i;

	mutex_lock(&pcie->lock);
	bit = bitmap_find_free_region(pcie->msi_irq_in_use, INT_PCI_MSI_NR,
				      get_count_order(nr_irqs));
	if (bit < 0)
		return -ENOSPC;

	for (i = 0; i < nr_irqs; i++) {
		irq_domain_set_info(domain, virq + i, bit + i,
				    &ax_msi_bottom_irq_chip, domain->host_data,
				    handle_simple_irq, NULL, NULL);
	}
	mutex_unlock(&pcie->lock);

	return 0;
}

static void ax_irq_msi_domain_free(struct irq_domain *domain, unsigned int virq,
				   unsigned int nr_irqs)
{
	struct irq_data *data = irq_domain_get_irq_data(domain, virq);
	struct axiado_pcie *pcie = irq_data_get_irq_chip_data(data);

	mutex_lock(&pcie->lock);
	bitmap_release_region(pcie->msi_irq_in_use, data->hwirq,
			      get_count_order(nr_irqs));
	mutex_unlock(&pcie->lock);
}

static const struct irq_domain_ops ax_msi_domain_ops = {
	.alloc = ax_irq_msi_domain_alloc,
	.free = ax_irq_msi_domain_free,
};

static void axiado_pcie_msi_enable(struct axiado_pcie *pcie)
{
	axiado_pcie_iowrite(pcie->bridge, IMASK_LOCAL, PCIE_INT_VAL_MASK);
	axiado_pcie_iowrite(pcie->bridge, ISTATUS_LOCAL, PCIE_INT_VAL_MASK);
}

static struct irq_chip ax_msi_irq_chip = {
	.name = "PCIe MSI",
	.irq_enable = pci_msi_unmask_irq,
	.irq_disable = pci_msi_mask_irq,
	.irq_mask = pci_msi_mask_irq,
	.irq_unmask = pci_msi_unmask_irq,
};

static struct msi_domain_info ax_msi_domain_info = {
	.flags = (MSI_FLAG_USE_DEF_DOM_OPS | MSI_FLAG_USE_DEF_CHIP_OPS),
	.chip = &ax_msi_irq_chip,
};

static int axiado_pcie_init_irq_domain(struct axiado_pcie *pcie)
{
	struct device *dev = pcie->dev;
	struct device_node *node = dev->of_node;
	struct device_node *pcie_intc_node;
#ifdef CONFIG_PCI_MSI
	struct fwnode_handle *fwnode = dev_fwnode(pcie->dev);
#endif

	/* Setup INTx */
	pcie_intc_node = of_get_next_child(node, NULL);
	if (!pcie_intc_node) {
		dev_err(dev, "No PCIe Intc node found\n");
		return -ENODEV;
	}
	pcie->irq_domain = irq_domain_add_linear(pcie_intc_node, PCI_NUM_INTX,
						 &ax_intx_domain_ops, pcie);

	if (!pcie->irq_domain) {
		dev_err(dev, "failed to get an INTx IRQ domain\n");
		return -ENOMEM;
	}

	of_node_put(pcie_intc_node);
#ifdef CONFIG_PCI_MSI
	pcie->dev_domain = irq_domain_add_linear(NULL, INT_PCI_MSI_NR,
						 &ax_msi_domain_ops, pcie);
	if (!pcie->dev_domain) {
		dev_err(pcie->dev, "failed to create IRQ domain\n");
		return -ENOMEM;
	}

	pcie->msi_domain = pci_msi_create_irq_domain(
		fwnode, &ax_msi_domain_info, pcie->dev_domain);
	if (!pcie->msi_domain) {
		dev_err(pcie->dev, "failed to create MSI domain\n");
		irq_domain_remove(pcie->dev_domain);
		return -ENOMEM;
	}
	axiado_pcie_msi_enable(pcie);
#endif

	return 0;
}

static void axiado_pcie_port_free(struct axiado_pcie_port *port)
{
	struct axiado_pcie *pcie = port->pcie;
	struct device *dev = pcie->dev;

	devm_iounmap(dev, port->base);
	devm_release_mem_region(dev, port->regs.start,
				resource_size(&port->regs));
	list_del(&port->list);
	devm_kfree(dev, port);
}

static struct pci_ops axiado_pcie_ops = {
	.map_bus = axiado_pcie_map_bus,
	.read = axiado_pcie_config_read,
	.write = axiado_pcie_config_write,
};

static u32 axiado_pcie_range_to_bar_type(u32 flags)
{
	u32 bar_type = 0;

	if (flags & IORESOURCE_MEM_64)
		bar_type |= PCI_BASE_ADDRESS_MEM_TYPE_64;
	if (flags & IORESOURCE_PREFETCH)
		bar_type |= PCI_BASE_ADDRESS_MEM_PREFETCH;

	return bar_type;
}

static int axiado_pcie_parse_ep_ranges(struct axiado_pcie *pcie)
{
	struct device_node *np = pcie->dev->of_node;
	struct of_pci_range_parser parser;
	struct of_pci_range range;
	int i = 0;

	if (of_pci_range_parser_init(&parser, np))
		return -EINVAL;

	for_each_of_pci_range(&parser, &range) {
		if (i == 0) {
			pcie->ep_bar0_addr = range.cpu_addr;
			pcie->ep_bar0_size = range.size;
			pcie->ep_bar0_flags = range.flags;
		} else if (i == 1) {
			pcie->ep_bar1_addr = range.cpu_addr;
			pcie->ep_bar1_size = range.size;
			pcie->ep_bar1_flags = range.flags;
		}
		dev_info(pcie->dev, "EP range[%d]: cpu 0x%llx size 0x%llx flags 0x%x\n",
			 i, range.cpu_addr, range.size, range.flags);
		i++;
	}

	if (i < 2) {
		dev_err(pcie->dev, "need 2 ranges for endpoint BARs, found %d\n", i);
		return -EINVAL;
	}

	return 0;
}

static int axiado_pcie_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct axiado_pcie *pcie;
	struct pci_host_bridge *host;
	const struct of_device_id *match;
	int err;
	bool is_endpoint;

	/*
	 * Endpoint mode: use pci_alloc_host_bridge() to skip
	 * devm_of_pci_bridge_init() which registers ranges as
	 * iomem resources — those collide with reserved-memory
	 * backing the PCIe BARs.
	 * Root port mode: use devm_pci_alloc_host_bridge() as normal.
	 */
	is_endpoint = of_property_match_string(dev->of_node,
					       "device_role", "host") < 0;

	if (is_endpoint)
		host = pci_alloc_host_bridge(sizeof(*pcie));
	else
		host = devm_pci_alloc_host_bridge(dev, sizeof(*pcie));
	if (!host)
		return -ENOMEM;

	match = of_match_device(axiado_pcie_of_match, &pdev->dev);
	if (!match)
		return -ENODEV;


	pcie = pci_host_bridge_priv(host);

	pcie->dev = dev;
	pcie->scm_version = (int)(uintptr_t)match->data;  /* 3000 or 3005 */
	platform_set_drvdata(pdev, pcie);
	INIT_LIST_HEAD(&pcie->ports);

	err = axiado_pcie_parse_dt(pcie);
	if (err < 0)
		goto put_resources;

	err = axiado_pcie_get_resources(pcie);
	if (err < 0) {
		dev_err(dev, "failed to request resources: %d\n", err);
		goto put_resources;
	}

	if (is_endpoint) {
		err = axiado_pcie_parse_ep_ranges(pcie);
		if (err < 0) {
			dev_err(dev, "failed to parse endpoint ranges: %d\n", err);
			goto put_resources;
		}
	}

	/* Fix for segregating the PCIe PLDA register configuration needed
	 * for PCIe EP mode as VGA and PCIe x2 Root Port Mode so that
	 * reconfiguration of PCIe EP for x2 should not happen as X2 for NIC
	 * is getting initialized in SBL
	 */
	if ((pcie->pcie_x1) ||
	    (pcie->pcie_x2 && pcie->is_root_port)) {
		err = axiado_pcie_init(pcie);
		if (err < 0) {
			dev_err(dev, "failed to initialize PCIe device: %d\n",
				err);
			goto put_resources;
		}
	}

	if (pcie->is_vga) {
		/* Program BAR apertures so the host can enumerate the BARs */
		err = axiado_pcie_bar_init(pdev, BAR_01,
					   pcie->ep_bar0_addr,
					   (u32)pcie->ep_bar0_size);
		if (err) {
			dev_err(dev, "failed to init BAR0: %d\n", err);
			goto put_resources;
		}
		err = axiado_pcie_bar_init(pdev, BAR_23,
					   pcie->ep_bar1_addr,
					   (u32)pcie->ep_bar1_size);
		if (err) {
			dev_err(dev, "failed to init BAR1: %d\n", err);
			goto put_resources;
		}
	}

	if (pcie->is_root_port) {
		err = axiado_pcie_init_irq_domain(pcie);
		if (err) {
			dev_err(dev, "Failed creating IRQ Domain\n");
			goto put_resources;
		}
		irq_set_chained_handler_and_data(pcie->irq,
						 axiado_pcie_intr_handler, pcie);

		pm_runtime_enable(pcie->dev);
		err = pm_runtime_get_sync(pcie->dev);
		if (err < 0) {
			dev_err(dev, "fail to enable pcie controller: %d\n",
				err);
			goto pm_runtime_put;
		}

		host->ops = &axiado_pcie_ops;
		host->sysdata = pcie;

		err = pci_host_probe(host);
		if (err)
			goto put_resources;
	}

	return 0;

pm_runtime_put:
	pm_runtime_put_sync(pcie->dev);
	pm_runtime_disable(pcie->dev);

put_resources:
	if (is_endpoint)
		pci_free_host_bridge(host);
	return err;
}

/*
 * This function is called to remove the devices when device unregistered.
 * @param	pdev the device structure used to give information
 * on which device remove
 * @return	The function returns 0 on success and -1 on failure.
 */
static int axiado_pcie_remove(struct platform_device *pdev)
{
	struct axiado_pcie *pcie;
	struct pci_host_bridge *host;
	struct axiado_pcie_port *port, *tmp;

	pcie = platform_get_drvdata(pdev);
	cancel_delayed_work_sync(&pcie->ep_link_work);
	if (pcie->is_root_port) {
		host = pci_host_bridge_from_priv(pcie);
		pci_stop_root_bus(host->bus);
		pci_remove_root_bus(host->bus);
		pm_runtime_put_sync(pcie->dev);
		pm_runtime_disable(pcie->dev);

		list_for_each_entry_safe(port, tmp, &pcie->ports, list)
			axiado_pcie_port_free(port);
	} else {
		host = pci_host_bridge_from_priv(pcie);
		pci_free_host_bridge(host);
	}

	return 0;
}

static struct platform_driver axiado_pcie_driver = {
	.driver	= {
		.name = "axiado-pcie",
		.owner = THIS_MODULE,
		.of_match_table = axiado_pcie_of_match,
	},
	.probe = axiado_pcie_probe,
	.remove = axiado_pcie_remove,
};

module_platform_driver(axiado_pcie_driver);

MODULE_AUTHOR("AXIADO CORPORATION");
MODULE_DESCRIPTION("Axiado PCIe Controller driver");
MODULE_LICENSE("GPL");
