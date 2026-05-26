// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021-2025 Axiado Corporation (or its affiliates). All rights reserved.
 *
 * Axiado HCP (Header and Crypto Processing Engine) Platform Driver
 *
 * This is the unified platform driver for the entire HCP subsystem which includes:
 * - SHIM (packet FIFO interface)
 * - 5 Ethernet MACs (4x1G + 1x10G)
 * - PHY/SerDes (SGMII and RXAUI)
 * - EIP-197 crypto engine
 *
 * This driver consolidates what were previously separate platform drivers
 * for shim, eip, and the vendor DDK into a single coherent driver that
 * matches the hardware architecture.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/string.h>

#include "hcp.h"

/* Vendor DDK internal interface for bypassing lkm platform driver */
#include "device_internal.h"

#define HCP_DRIVER_NAME		"axiado-hcp"

/* Device Tree IRQ indices */
#define HCP_IRQ_EIP197_INDEX		0
#define HCP_IRQ_RING_BASE_INDEX		1
#define HCP_IRQ_RING_BASE_FIRST_CNT 4
#define HCP_IRQ_MAC_BASE_INDEX \
	(HCP_IRQ_RING_BASE_INDEX + HCP_IRQ_RING_BASE_FIRST_CNT)

/**
 * hcp_get_pkt_path - Parse DT for packet path EIP/Host-FIFO
 * @pdev: Platform device
 * @hcp: HCP device structure
 *
 * Parse the Device Tree of hcp-node for packet (tx/rx) path
 *
 * Return: 0 on success, negative error code on failure
 */
static int hcp_get_pkt_path(struct platform_device *pdev,
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
				dev_warn(
					dev,
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
				dev_err(dev,
					"Failed to allocate memory for HFIFO\n");
				ret = -ENOMEM;
				goto end;
			}
			hpriv->mac_idx = mac_idx;
			hcp->hfifo_mode = true;
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
 * Maps all memory regions (EIP-197, SHIM, PHY CSR) from the
 * single consolidated HCP device tree node.
 *
 * Return: 0 on success, negative error code on failure
 */
static int hcp_map_resources(struct platform_device *pdev,
			     struct hcp_device *hcp)
{
	struct device *dev = &pdev->dev;
	struct resource *res;

	dev_info(dev, "Mapping HCP memory resources\n");

	if (!hcp->hfifo_mode) {
		/* Map EIP-197 crypto engine registers - REQUIRED */
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						   "eip197");
		if (!res) {
			dev_err(dev,
				"Missing eip197 resource in Device Tree\n");
			return -EINVAL;
		}
		hcp->eip197_base = devm_ioremap_resource(dev, res);
		if (IS_ERR(hcp->eip197_base))
			return PTR_ERR(hcp->eip197_base);

		hcp->eip197_phys = res->start;
		hcp->eip197_size = resource_size(res);
		dev_info(dev, "EIP-197: phys=0x%llx size=0x%llx\n",
			 (unsigned long long)hcp->eip197_phys,
			 (unsigned long long)hcp->eip197_size);
	}

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
 * hcp_get_interrupts - Get all IRQs from Device Tree
 * @pdev: Platform device
 * @hcp: HCP device structure
 *
 * Gets all interrupt numbers from the device tree node.
 * Expected order:
 *   IRQ 0:    EIP-197 crypto
 *   IRQ 1-4:  Rings 0-3
 *   IRQ 5-9:  MACs 0-4
 *   IRQ 10-17: Rings 4-11 (AX3005 only)
 *
 * EIP-197 and ring IRQs are skipped in hfifo mode.
 *
 * Return: 0 on success, negative error code on failure
 */
static int hcp_get_interrupts(struct platform_device *pdev,
			      struct hcp_device *hcp)
{
	struct device *dev = &pdev->dev;
	int i, irq, num;

	if (!hcp->hfifo_mode) {
		/* EIP-197 crypto interrupt */
		irq = platform_get_irq(pdev, HCP_IRQ_EIP197_INDEX);
		if (irq < 0) {
			dev_err(dev, "Failed to get EIP-197 IRQ\n");
			return irq;
		}
		hcp->eip197_irq = irq;
		dev_info(dev, "EIP-197 IRQ: %d\n", hcp->eip197_irq);

		/* EIP Ring interrupts */
		for (i = 0; i < RING_INTERFACE_CNT; i++) {
			num = HCP_IRQ_RING_BASE_INDEX + i;
			if (i >= HCP_IRQ_RING_BASE_FIRST_CNT)
				num += MAX_MAC_CNT;
			irq = platform_get_irq(pdev, num);
			if (irq < 0) {
				dev_err(dev, "Failed to get EIP Ring %d IRQ\n",
					i);
				return irq;
			}
			hcp->ring_irqs[i] = irq;
			dev_dbg(dev, "EIP Ring %d IRQ: %d\n", i,
				hcp->ring_irqs[i]);
		}
	}

	/* MAC interrupts */
	for (i = 0; i < MAX_MAC_CNT; i++) {
		irq = platform_get_irq(pdev, HCP_IRQ_MAC_BASE_INDEX + i);
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
 * hcp_init_vendor_ddk - Bypass vendor DDK platform driver and init manually
 * @hcp: HCP device structure
 *
 * The vendor Rambus EIP-197 DDK normally registers its own platform driver
 * via lkm_init() which expects a separate device tree node. We bypass this
 * by manually initializing the global device structure that the DDK expects
 * AND manually registering the device list from HWPAL_DEVICES.
 *
 * This replaces what would normally happen via:
 * - eip_device_initialize() -> copies static devices to dynamic list
 * - device_internal_initialize() -> calls lkm_init() (we skip this)
 * - lkm_probe() -> populates global (we do this manually)
 *
 * Return: 0 on success, negative error code on failure
 */
static int hcp_init_vendor_ddk(struct hcp_device *hcp)
{
	device_global_admin_t *ddk_global;
	const device_admin_static_t *dev_stat_admin_p;
	device_admin_t **dev_admin_pp;
	unsigned int dev_stat_count, dev_count;
	struct clk *eip_clk;
	unsigned int i;

	dev_info(hcp->dev, "Initializing vendor EIP-197 DDK\n");

	/* Get pointer to vendor DDK's global device structure.
	 * This is normally populated by lkm_init() -> lkm_probe(),
	 * but we bypass that and set it up manually.
	 */
	ddk_global = device_internal_admin_global_get();
	if (!ddk_global) {
		dev_err(hcp->dev, "Failed to get DDK global structure\n");
		return -ENODEV;
	}

	/* Check if already initialized (shouldn't happen, but be safe) */
	if (ddk_global->f_initialized) {
		dev_info(hcp->dev, "DDK already initialized\n");
		return 0;
	}

	/* Populate the platform-specific fields */
	ddk_global->platform.platform_device_p = hcp->pdev;
	ddk_global->platform.mapped_base_addr_p =
		(u32 __iomem *)hcp->eip197_base;
	ddk_global->platform.phys_base_addr = (void *)hcp->eip197_phys;

	dev_info(hcp->dev, "DDK global initialized: phys=0x%llx\n",
		 (unsigned long long)hcp->eip197_phys);

	/* Manually register devices from HWPAL_DEVICES static list.
	 * This is what eip_device_initialize() would do, but we can't
	 * call that because it would try to call lkm_init() which fails.
	 */
	dev_stat_count = device_internal_static_count_get();
	dev_count = device_internal_count_get();
	dev_stat_admin_p = device_internal_admin_static_get();
	dev_admin_pp = device_internal_admin_get();

	if (dev_stat_count > dev_count) {
		dev_err(hcp->dev,
			"Invalid number of static devices (%u), max %u\n",
			dev_stat_count, dev_count);
		return -EINVAL;
	}

	dev_info(hcp->dev, "Registering %u vendor DDK devices\n", dev_stat_count);

	/* Copy static devices to dynamic device list */
	for (i = 0; i < dev_stat_count; i++) {
		size_t name_len = strlen(dev_stat_admin_p[i].dev_name) + 1;

		/* Allocate memory for device administration data */
		dev_admin_pp[i] = device_internal_alloc(sizeof(device_admin_t));
		if (!dev_admin_pp[i]) {
			dev_err(hcp->dev,
				"Failed to allocate device %u (%s)\n",
				i, dev_stat_admin_p[i].dev_name);
			goto err_cleanup;
		}

		/* Allocate and copy device name */
		dev_admin_pp[i]->dev_name = device_internal_alloc(name_len);
		if (!dev_admin_pp[i]->dev_name) {
			dev_err(hcp->dev,
				"Failed to allocate name for device %u (%s)\n",
				i, dev_stat_admin_p[i].dev_name);
			device_internal_free(dev_admin_pp[i]);
			dev_admin_pp[i] = NULL;
			goto err_cleanup;
		}
		strscpy(dev_admin_pp[i]->dev_name, dev_stat_admin_p[i].dev_name,
			name_len);

		/* Copy device properties */
		dev_admin_pp[i]->device_nr = dev_stat_admin_p[i].device_nr;
		dev_admin_pp[i]->first_ofs = dev_stat_admin_p[i].first_ofs;
		dev_admin_pp[i]->last_ofs = dev_stat_admin_p[i].last_ofs;
		dev_admin_pp[i]->flags = dev_stat_admin_p[i].flags;
		dev_admin_pp[i]->device_id = i;

		dev_dbg(hcp->dev,
			"Registered device '%s': offset 0x%x-0x%x\n",
			dev_admin_pp[i]->dev_name,
			dev_admin_pp[i]->first_ofs,
			dev_admin_pp[i]->last_ofs);
	}

	/* Mark as initialized - device_read32/write32 check this */
	ddk_global->f_initialized = true;

	dev_info(hcp->dev, "Vendor DDK initialized with %u devices\n",
		 dev_stat_count);

	/* Optional: Enable EIP-197 clock if present in DT */
	eip_clk = devm_clk_get_optional(hcp->dev, "eip197");
	if (!IS_ERR_OR_NULL(eip_clk)) {
		int ret = clk_prepare_enable(eip_clk);

		if (ret) {
			dev_warn(hcp->dev, "Failed to enable EIP-197 clock: %d\n", ret);
		} else {
			hcp->eip197_clk = eip_clk;
			dev_info(hcp->dev, "EIP-197 clock enabled\n");
		}
	}

	return 0;

err_cleanup:
	/* Free all allocated memory on error */
	for (i = 0; i < dev_stat_count; i++) {
		if (dev_admin_pp[i]) {
			if (dev_admin_pp[i]->dev_name)
				device_internal_free(dev_admin_pp[i]->dev_name);
			device_internal_free(dev_admin_pp[i]);
			dev_admin_pp[i] = NULL;
		}
	}
	return -ENOMEM;
}

/**
 * hcp_probe - HCP platform driver probe function
 * @pdev: Platform device
 *
 * Main probe function that orchestrates initialization of the entire
 * HCP subsystem in the correct order:
 *
 * 1. Map all memory resources from single DT node
 * 2. Get all IRQs
 * 3. Initialize SHIM subsystem (MAC/PHY layer)
 * 4. Bypass vendor DDK platform driver and init manually
 * 5. Initialize EIP subsystem (crypto and network devices)
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

	/* Allocate main HCP device structure */
	hcp = devm_kzalloc(dev, sizeof(*hcp), GFP_KERNEL);
	if (!hcp)
		return -ENOMEM;

	hcp->dev = dev;
	hcp->pdev = pdev;
	platform_set_drvdata(pdev, hcp);

	/* Host Packet FIFO or EIP path for Tx/Rx packet */
	ret = hcp_get_pkt_path(pdev, hcp);
	if (ret) {
		dev_err(dev, "Failed to get Packet Path: %d\n", ret);
		return ret;
	}

	/* Map all memory regions from consolidated DT node */
	ret = hcp_map_resources(pdev, hcp);
	if (ret) {
		dev_err(dev, "Failed to map resources: %d\n", ret);
		return ret;
	}

	/* Get all interrupts */
	ret = hcp_get_interrupts(pdev, hcp);
	if (ret) {
		dev_err(dev, "Failed to get interrupts: %d\n", ret);
		return ret;
	}

	/* Initialize subsystems in dependency order:
	 * SHIM must be first (provides hardware layer for MACs/PHYs)
	 */
	ret = shim_subsystem_init(hcp);
	if (ret) {
		dev_err(dev, "SHIM subsystem init failed: %d\n", ret);
		goto err;
	}

	if (hcp->hfifo_mode) {
		ret = hfifo_subsystem_init(hcp);
		if (ret) {
			dev_err(dev, "HFIFO subsystem init failed: %d\n", ret);
			goto err;
		}
	} else {
		/* Bypass vendor DDK's platform driver registration.
		 * Manually initialize the global state the DDK expects.
		 */
		ret = hcp_init_vendor_ddk(hcp);
		if (ret) {
			dev_err(dev, "Vendor DDK init failed: %d\n", ret);
			goto err;
		}

		/* Initialize EIP subsystem (crypto engine and network devices).
		 * This calls driver197_init() which will use the DDK global
		 * structure we just populated.
		 */
		dev_info(dev, "Initializing EIP subsystem\n");
		ret = eip_subsystem_init(hcp);
		if (ret) {
			dev_err(dev, "EIP subsystem init failed: %d\n", ret);
			goto err;
		}
	}

	dev_info(dev, "Axiado HCP initialized successfully\n");
	return 0;

err:
	dev_err(dev, "Axiado HCP initialization failed\n");
	/* DDK cleanup is handled by eip_subsystem_exit if needed */
	if (hcp->eip197_clk)
		clk_disable_unprepare(hcp->eip197_clk);
	shim_subsystem_exit(hcp);

	return ret;
}

/**
 * hcp_remove - HCP platform driver remove function
 * @pdev: Platform device
 *
 * Cleanup function that tears down all subsystems in reverse order.
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

	/* Cleanup in reverse order of initialization */
	eip_subsystem_exit(hcp);

	if (hcp->eip197_clk)
		clk_disable_unprepare(hcp->eip197_clk);

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

MODULE_FIRMWARE("firmware_eip207_ipue.bin");
MODULE_FIRMWARE("firmware_eip207_ifpp.bin");
MODULE_FIRMWARE("firmware_eip207_opue.bin");
MODULE_FIRMWARE("firmware_eip207_ofpp.bin");

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Axiado HCP Ethernet and Crypto Subsystem Driver");
MODULE_AUTHOR("Axiado Corporation");
