// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021-2026 Axiado Corporation (or its affiliates). All rights reserved.
 *
 * Axiado HCP (Header and Crypto Processing Engine) Platform Driver
 *
 * This is the unified platform driver for the HCP subsystem which includes:
 * - SHIM (packet FIFO interface)
 * - 5 Ethernet MACs (4x1G + 1x10G)
 * - PHY/SerDes (SGMII and RXAUI)
 * - Host FIFO network device
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/io.h>

#include "hcp.h"

#define HCP_DRIVER_NAME		"axiado-hcp"

/**
 * hcp_hfifo_init - Parse DT for Host Packet FIFO binding
 * @pdev: Platform device
 * @hcp: HCP device structure
 *
 * Parses the "hfifo" child of the HCP DT node to identify which MAC port
 * should be exposed as a Host FIFO netdev
 *
 * Return: 0 on success, negative error code on failure
 */
static int hcp_hfifo_init(struct platform_device *pdev,
			    struct hcp_device *hcp)
{
	struct device *dev = &pdev->dev;
	struct device_node *child;
	struct hfifo_priv *hpriv = NULL;
	u32 mac_idx = 0;
	int ret = 0;

	child = of_get_child_by_name(dev->of_node, "hfifo");
	if (child) {
		of_node_put(child);
		if (of_device_is_available(child)) {
			ret = of_property_read_u32(child, "mac", &mac_idx);
			if (ret) {
				dev_warn(dev,
					 "Port 'mac' to bind is missing for Host Packet FIFO\n");
				ret = -EINVAL;
				goto end;
			}
			if (mac_idx >= MAX_MAC_CNT) {
				dev_err(dev, "Invalid mac index %u, max %d\n",
					mac_idx, MAX_MAC_CNT - 1);
				ret = -EINVAL;
				goto end;
			}
			hpriv = devm_kzalloc(dev, sizeof(*hpriv), GFP_KERNEL);
			if (!hpriv) {
				ret = -ENOMEM;
				goto end;
			}
			hpriv->mac_idx = mac_idx;
			dev_info(dev,
				 "Host Packet FIFO Port is set to MAC-%u\n",
				 mac_idx);
		}
	}

end:
	hcp->hfifo_priv = hpriv;
	return ret;
}

/**
 * hcp_map_resources - Map all memory regions from Device Tree
 * @pdev: Platform device
 * @hcp: HCP device structure
 *
 * Maps SHIM and PHY CSR regions from the consolidated HCP DT node.
 *
 * Return: 0 on success, negative error code on failure
 */
static int hcp_map_resources(struct platform_device *pdev,
			     struct hcp_device *hcp)
{
	struct device *dev = &pdev->dev;
	struct resource *res;

	dev_info(dev, "Mapping HCP memory resources\n");

	/* Map SHIM registers - REQUIRED */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "shim");
	if (!res) {
		dev_err(dev, "Missing shim resource in Device Tree\n");
		return -EINVAL;
	}
	hcp->shim_base = devm_ioremap_resource(dev, res);
	if (IS_ERR(hcp->shim_base))
		return PTR_ERR(hcp->shim_base);
	hcp->shim_size = resource_size(res);
	dev_info(dev, "SHIM: phys=0x%llx size=0x%llx\n",
		 (unsigned long long)res->start,
		 (unsigned long long)hcp->shim_size);

	/* Map PHY CSR - REQUIRED */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "phy-csr");
	if (!res) {
		dev_err(dev, "Missing phy-csr resource in Device Tree\n");
		return -EINVAL;
	}
	hcp->phy_csr_base = devm_ioremap_resource(dev, res);
	if (IS_ERR(hcp->phy_csr_base))
		return PTR_ERR(hcp->phy_csr_base);

	dev_info(dev, "PHY CSR: phys=0x%llx size=0x%llx\n",
		 (unsigned long long)res->start,
		 (unsigned long long)resource_size(res));

	return 0;
}

/**
 * hcp_get_interrupts - Get MAC IRQs from Device Tree
 * @pdev: Platform device
 * @hcp: HCP device structure
 *
 * Reads MAC interrupts (indices 0..MAX_MAC_CNT-1) from the DT node.
 *
 * Return: 0 on success, negative error code on failure
 */
static int hcp_get_interrupts(struct platform_device *pdev,
			      struct hcp_device *hcp)
{
	struct device *dev = &pdev->dev;
	int i, irq;

	for (i = 0; i < MAX_MAC_CNT; i++) {
		irq = platform_get_irq(pdev, i);
		if (irq < 0) {
			dev_warn(dev, "MAC %d IRQ not found, will use polling\n", i);
			hcp->mac_irqs[i] = 0;
		} else {
			hcp->mac_irqs[i] = irq;
			dev_dbg(dev, "MAC %d IRQ: %d\n", i, hcp->mac_irqs[i]);
		}
	}

	return 0;
}

/**
 * hcp_probe - HCP platform driver probe function
 * @pdev: Platform device
 *
 * 1. Parse Host FIFO DT binding (optional)
 * 2. Map all memory resources from single DT node
 * 3. Get all IRQs
 * 4. Initialize SHIM subsystem (MAC/PHY layer)
 * 5. If a Host FIFO port is configured, initialize the HFIFO subsystem
 *
 * Return: 0 on success, negative error code on failure
 */
static int hcp_probe(struct platform_device *pdev)
{
	struct hcp_device *hcp;
	struct device *dev = &pdev->dev;
	int ret;

	dev_info(dev, "Axiado HCP probing\n");

	if (!dev->of_node) {
		dev_err(dev, "Missing device tree node\n");
		return -EINVAL;
	}

	hcp = devm_kzalloc(dev, sizeof(*hcp), GFP_KERNEL);
	if (!hcp)
		return -ENOMEM;

	hcp->dev = dev;
	hcp->pdev = pdev;
	platform_set_drvdata(pdev, hcp);

	ret = hcp_hfifo_init(pdev, hcp);
	if (ret) {
		dev_err(dev, "Failed to get Packet Path: %d\n", ret);
		return ret;
	}

	ret = hcp_map_resources(pdev, hcp);
	if (ret) {
		dev_err(dev, "Failed to map resources: %d\n", ret);
		return ret;
	}

	ret = hcp_get_interrupts(pdev, hcp);
	if (ret) {
		dev_err(dev, "Failed to get interrupts: %d\n", ret);
		return ret;
	}

	ret = shim_subsystem_init(hcp);
	if (ret) {
		dev_err(dev, "SHIM subsystem init failed: %d\n", ret);
		return ret;
	}

	ret = hfifo_subsystem_init(hcp);
	if (ret) {
		dev_err(dev, "HFIFO subsystem init failed: %d\n", ret);
		goto err_hfifo;
	}

	dev_info(dev, "Axiado HCP initialized successfully\n");
	return 0;

err_hfifo:
	shim_subsystem_exit(hcp);
	return ret;
}

/**
 * hcp_remove - HCP platform driver remove function
 * @pdev: Platform device
 *
 * Return: 0 on success
 */
static int hcp_remove(struct platform_device *pdev)
{
	struct hcp_device *hcp = platform_get_drvdata(pdev);
	struct device *dev = &pdev->dev;

	dev_info(dev, "Removing Axiado HCP driver\n");

	if (!hcp)
		return 0;

	hfifo_subsystem_exit(hcp);

	shim_subsystem_exit(hcp);

	dev_info(dev, "Axiado HCP driver removed\n");
	return 0;
}

/* Device tree compatible string */
static const struct of_device_id hcp_of_match[] = {
	{ .compatible = "axiado,ax3000-hcp" },
	{ }
};
MODULE_DEVICE_TABLE(of, hcp_of_match);

static struct platform_driver hcp_driver = {
	.probe = hcp_probe,
	.remove = hcp_remove,
	.driver = {
		.name = HCP_DRIVER_NAME,
		.of_match_table = hcp_of_match,
	},
};

module_platform_driver(hcp_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Axiado HCP Ethernet Subsystem Driver");
MODULE_AUTHOR("Axiado Corporation");
