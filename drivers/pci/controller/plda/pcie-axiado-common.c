// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2023-26 Axiado Corporation.
 *
 * Common resource and DT parsing for Axiado PCIe host and endpoint drivers.
 */
#include <linux/platform_device.h>
#include <linux/of_address.h>
#include <linux/kernel.h>
#include <linux/of_irq.h>
#include <linux/of_pci.h>

#include "pcie-axiado.h"

int axiado_pcie_get_resources(struct axiado_pcie *pcie)
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

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "cfg");
	if (!res) {
		err = -EADDRNOTAVAIL;
		pr_err("axiado_pcie: Could not find int_pcie config space address\n");
		goto phys_put;
	}

	pcie->cs = *res;
	pcie->cfg_phys = res->start;
	pcie->cs.end = pcie->cs.start + SZ_4K - 1;

	pcie->cfg = devm_ioremap_resource(dev, &pcie->cs);
	if (IS_ERR(pcie->cfg)) {
		err = PTR_ERR(pcie->cfg);
		pr_err("axiado_pcie: Could not map int config space address\n");
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

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
			"ioctl");
	if (res) {
		pcie->ioctl = devm_ioremap(dev, res->start,
				resource_size(res));
		if (!pcie->ioctl)
			dev_warn(dev, "Could not map ioctl (shared), PERST# control unavailable\n");
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

int axiado_pcie_parse_dt(struct axiado_pcie *pcie)
{
	struct device *dev = pcie->dev;
	struct device_node *np = dev->of_node, *port = NULL;
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
	}
	pcie->lanes = lanes;

	if (pcie->lanes == 1) {
		pcie->pcie_x1 = true;
		pcie->pcie_x2 = false;
	} else if (pcie->lanes == 2) {
		pcie->pcie_x1 = false;
		pcie->pcie_x2 = true;
	}

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

		for_each_child_of_node(np, port) {
			struct axiado_pcie_port *rp;
			unsigned int index;

			if (!of_node_is_type(port, "pci"))
				continue;

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

void axiado_pcie_config_eq_gen3_4(struct axiado_pcie *pcie)
{
	u32 temp;

	/* PEX_SPC: enable EQ tuning / preset programming */
	temp = axiado_pcie_ioread(pcie->bridge,
				  REG_PCIE_X1_PCIE_PEX_SPC_ADRS_OFFSET);
	temp |= 0x3000;
	axiado_pcie_iowrite(pcie->bridge,
			    REG_PCIE_X1_PCIE_PEX_SPC_ADRS_OFFSET, temp);

	/* Gen4 (16G) RX/TX preset, lane 0 (bits [15:0]) */
	temp = axiado_pcie_ioread(pcie->bridge,
				  REG_PCIE_EQ_PRESET_16G_31_0_ADRS_OFFSET);
	temp = (temp & ~PCIE_EQ_PRESET16_MASK) | PCIE_EQ_PRESET16_VAL;
	axiado_pcie_iowrite(pcie->bridge,
			    REG_PCIE_EQ_PRESET_16G_31_0_ADRS_OFFSET, temp);

	/* EQ tuning [31:0]: preset tuning settings + continuous fine tune */
	temp = axiado_pcie_ioread(pcie->bridge,
				  REG_PCIE_EQ_TUNING_31_0_ADRS_OFFSET);
	temp = (temp & ~PCIE_EQ_TUNNING_31_0_MASK) | PCIE_EQ_TUNNING_31_0_VAL;
	axiado_pcie_iowrite(pcie->bridge,
			    REG_PCIE_EQ_TUNING_31_0_ADRS_OFFSET, temp);

	/* EQ tuning [63:32]: Gen3/Gen4 preset vectors + max tuning iterations */
	temp = axiado_pcie_ioread(pcie->bridge,
				  REG_PCIE_EQ_TUNING_63_32_ADRS_OFFSET);
	temp = (temp & ~PCIE_EQ_TUNNING_63_32_MASK) | PCIE_EQ_TUNNING_63_32_VAL;
	axiado_pcie_iowrite(pcie->bridge,
			    REG_PCIE_EQ_TUNING_63_32_ADRS_OFFSET, temp);
}
