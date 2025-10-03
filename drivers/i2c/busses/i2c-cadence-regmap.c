// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * I2C bus driver for the Cadence I2C controller accessed via regmap.
 *
 * Copyright (c) 2021-25 Axiado Corporation (or its affiliates). All rights reserved.
 *
 * This is a wrapper driver for Cadence I2C cores that are part of a
 * Multi-Function Device (MFD) and do not have their own MMIO range.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/of.h>

#include "i2c-cadence.h"

static int cdns_i2c_regmap_probe(struct platform_device *pdev)
{
	struct cdns_i2c *id;
	u32 reg_base_offset;
	int ret;

	id = devm_kzalloc(&pdev->dev, sizeof(*id), GFP_KERNEL);
	if (!id)
		return -ENOMEM;

	id->dev = &pdev->dev;
	platform_set_drvdata(pdev, id);

	/* Get the regmap handle from the parent MFD device */
	id->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!id->regmap) {
		dev_err(&pdev->dev, "failed to get parent regmap\n");
		return -ENODEV;
	}

	/* The 'reg' property in our DT node is the offset within the MFD */
	ret = of_property_read_u32(pdev->dev.of_node, "reg", &reg_base_offset);
	if (ret) {
		dev_err(&pdev->dev, "failed to read reg property: %d\n", ret);
		return ret;
	}
	id->regmap_base_offset = reg_base_offset;

	/* Set the adapter name for the regmap case */
	snprintf(id->adap.name, sizeof(id->adap.name),
		 "Cadence I2C via regmap");
	/*
	 * Now that register access is configured, call the common probe
	 * function from the main driver to do the rest of the setup.
	 */
	return cdns_i2c_probe_common(pdev, id);
}

static void cdns_i2c_regmap_remove(struct platform_device *pdev)
{
	struct cdns_i2c *id = platform_get_drvdata(pdev);

	/* Call the common remove function to do all the cleanup */
	cdns_i2c_remove_common(id);
}

static const struct of_device_id cdns_i2c_regmap_of_match[] = {
	{ .compatible = "cdns,i2c-r1p14-regmap" },
	{ .compatible = "axiado,ax3000-i2c" },
	{ /* end of table */ }
};
MODULE_DEVICE_TABLE(of, cdns_i2c_regmap_of_match);

static struct platform_driver cdns_i2c_regmap_driver = {
	.driver = {
		.name  = "i2c-cadence-regmap",
		.of_match_table = cdns_i2c_regmap_of_match,
	},
	.probe  = cdns_i2c_regmap_probe,
	.remove_new = cdns_i2c_regmap_remove,
};

module_platform_driver(cdns_i2c_regmap_driver);

MODULE_AUTHOR("AXIADO CORPORATION");
MODULE_DESCRIPTION("Cadence I2C regmap driver");
MODULE_LICENSE("GPL");
