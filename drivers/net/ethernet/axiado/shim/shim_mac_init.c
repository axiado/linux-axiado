// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * shim_mac_init.c - Initialization routines for the Axiado SHIM driver.
 * Configures MACs, Flow Control, Inline Command Descriptors, and TDEST masks.
 *
 * Copyright (c) 2022-2026 Axiado Corporation
 */

#include <linux/device.h>
#include <linux/kernel.h>

#include "mac_config.h"
#include "shim_eip_config.h"
#include "shim_mac.h"
#include "shim_platform.h"

/**
 * shim_mac_init - Master function for initializing all enabled MACs and SHIM.
 * @dev: The device structure for logging.
 *
 * This function orchestrates the initialization sequence:
 * 1. Basic initialization of all enabled MACs.
 * 2. Flow control configuration.
 * 3. Inline Command Descriptor (CD) and Transform Record (TR) configuration.
 * 4. Traffic Destination (TDEST) masking.
 *
 * Return: SHIM_STATUS_SUCCESS on success, or a negative error code on failure.
 */
enum AX_SHIM_STATUS shim_mac_init(struct device *dev)
{
	enum AX_SHIM_STATUS ret;

	dev_dbg(dev, "Initializing shim and mac configuration\n");

	/* 1. Initialization of all MACs */
	ret = mac_basic_init(dev);
	if (ret != SHIM_STATUS_SUCCESS) {
		dev_err(dev, "MAC initialization failed. ret: %d\n", ret);
		return ret;
	}
	dev_dbg(dev, "MAC basic init done\n");

	/* 2. MAC flow config */
	ret = mac_flow_config();
	if (ret != SHIM_STATUS_SUCCESS) {
		dev_err(dev, "MAC flow config failed. ret: %d\n", ret);
		return ret;
	}
	dev_dbg(dev, "Flow config done\n");

	/* 3. Inline CD and TR config */
	ret = shim_inline_cd_config(dev);
	if (ret != SHIM_STATUS_SUCCESS) {
		dev_err(dev, "Inline CD config failed. ret: %d\n", ret);
		return ret;
	}
	dev_dbg(dev, "Inline CD config done\n");

	/* 4. TDEST masking */
	dev_dbg(dev, "Configuring Tdest masking\n");
	ret = tdest_mask_config(dev);
	if (ret != SHIM_STATUS_SUCCESS) {
		/* Note: TDEST mask failure is logged but treated as non-fatal
		 * to allow partial driver functionality.
		 */
		dev_err(dev, "Tdest mask config failed. ret: %d\n", ret);
	}

	dev_info(dev, "SHIM and MACs initialized successfully\n");

	return SHIM_STATUS_SUCCESS;
}
