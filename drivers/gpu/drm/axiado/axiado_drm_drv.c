// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022-2025 Axiado Corporation.
 *
 * Based on rockchip_drm_drv.c,
 *   originally (C) Fuzhou Rockchip Electronics Co.Ltd
 *   Author: Mark Yao <mark.yao@rock-chips.com>
 * which itself is based on exynos_drm_drv.c.
 */

#include <linux/dma-mapping.h>
#include <linux/pm_runtime.h>
#include <linux/module.h>
#include <linux/of_graph.h>
#include <linux/of_platform.h>
#include <linux/component.h>
#include <linux/console.h>
#include <linux/iommu.h>
#include <drm/drm_aperture.h>
#include <drm/drm_drv.h>
#include <drm/drm_fbdev_generic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_gem_dma_helper.h>
#include <drm/drm_of.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_vblank.h>

#include "axiado_drm_drv.h"

#define AXIADO_FB_MAX_WIDTH 4095
#define AXIADO_FB_MAX_HEIGHT 4095

#define MAX_AXIADO_SUB_DRIVERS 3

static struct platform_driver *axiado_sub_drivers[MAX_AXIADO_SUB_DRIVERS];
static int num_axiado_sub_drivers;

DEFINE_DRM_GEM_DMA_FOPS(axiado_driver_fops);
static struct drm_driver axiado_drm_driver = {
	.driver_features = DRIVER_MODESET | DRIVER_GEM | DRIVER_ATOMIC,
	.name = "drm-axiado",
	.desc = "Axiado DRM Driver",
	.date = "20230411",
	.major = 1,
	.minor = 0,
	.patchlevel = 0,
	.fops = &axiado_driver_fops,
	.dumb_create = drm_gem_dma_dumb_create_internal,
	.prime_handle_to_fd = drm_gem_prime_handle_to_fd,
	.prime_fd_to_handle = drm_gem_prime_fd_to_handle,
	DRM_GEM_DMA_DRIVER_OPS,
};

static int compare_dev(struct device *dev, void *data);

static const struct drm_mode_config_funcs axiado_mode_config_funcs = {
	.fb_create = drm_gem_fb_create,
	.atomic_check = drm_atomic_helper_check,
	.atomic_commit = drm_atomic_helper_commit,
};

static int axiado_drm_bind(struct device *dev)
{
	struct drm_device *drm_dev;
	int ret;

	drm_dev = drm_dev_alloc(&axiado_drm_driver, dev);
	if (IS_ERR(drm_dev))
		return PTR_ERR(drm_dev);
	dev_set_drvdata(dev, drm_dev);
	ret = drmm_mode_config_init(drm_dev);
	if (ret)
		goto err_free;
	drm_dev->mode_config.min_width = 0;
	drm_dev->mode_config.min_height = 0;
	drm_dev->mode_config.max_width = AXIADO_FB_MAX_WIDTH;
	drm_dev->mode_config.max_height = AXIADO_FB_MAX_HEIGHT;
	drm_dev->mode_config.funcs = &axiado_mode_config_funcs;
	ret = component_bind_all(dev, drm_dev);
	if (ret)
		goto err_mode_config_cleanup;
	drm_mode_config_reset(drm_dev);
	drm_kms_helper_poll_init(drm_dev);
	ret = drm_dev_register(drm_dev, 0);
	if (ret)
		goto err_mode_config_cleanup;
	drm_fbdev_generic_setup(drm_dev, 24);
	return 0;

err_mode_config_cleanup:
	drm_mode_config_cleanup(drm_dev);
err_free:
	dev_set_drvdata(dev, NULL);
	drm_dev_put(drm_dev);
	return ret;
}

static void axiado_drm_unbind(struct device *dev)
{
	struct drm_device *drm_dev = dev_get_drvdata(dev);

	drm_dev_unregister(drm_dev);
	dev_set_drvdata(dev, NULL);
	drm_dev_put(drm_dev);
}
static int compare_dev(struct device *dev, void *data)
{
	if (dev == NULL)
		return -EINVAL;
	return dev == (struct device *)data;
}

static void axiado_drm_match_remove(struct device *dev)
{
	struct device_link *link;

	list_for_each_entry(link, &dev->links.consumers, s_node)
		device_link_del(link);
}

static struct component_match *axiado_drm_match_add(struct device *dev)
{
	struct component_match *match = NULL;
	int i;

	if (dev == NULL)
		return NULL;

	for (i = 0; i < num_axiado_sub_drivers; i++) {
		struct platform_driver *drv = axiado_sub_drivers[i];
		struct device *p = NULL, *d;

		do {
			d = platform_find_device_by_driver(p, &drv->driver);
			put_device(p);
			p = d;
			if (!d)
				break;
			device_link_add(dev, d, DL_FLAG_STATELESS);
			component_match_add(dev, &match, compare_dev, d);
		} while (true);
	}

	if (IS_ERR(match))
		axiado_drm_match_remove(dev);
	return match ?: ERR_PTR(-ENODEV);
}

static const struct component_master_ops axiado_drm_ops = {
	.bind = axiado_drm_bind,
	.unbind = axiado_drm_unbind,
};

static int axiado_drm_platform_of_probe(struct device *dev)
{
	struct device_node *np = dev->of_node;
	struct device_node *port;
	bool found = false;
	int i;

	if (!np)
		return -ENODEV;
	for (i = 0;; i++) {
		port = of_parse_phandle(np, "ports", i);
		if (!port)
			break;
		if (!of_device_is_available(port->parent)) {
			of_node_put(port);
			continue;
		}
		found = true;
		of_node_put(port);
	}
	if (i == 0) {
		DRM_DEV_ERROR(dev, "axiado-drm: missing 'ports' property\n");
		return -ENODEV;
	}
	if (!found) {
		DRM_DEV_ERROR(dev, "axiado-drm: No available display controller found\n");
		return -ENODEV;
	}
	return 0;
}

static int axiado_drm_platform_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct component_match *match = NULL;
	int ret;

	if (pdev == NULL)
		return -EINVAL;
	ret = axiado_drm_platform_of_probe(dev);
	if (ret)
		return ret;
	match = axiado_drm_match_add(dev);
	if (IS_ERR(match))
		return PTR_ERR(match);
	if (dma_set_coherent_mask(dev, DMA_BIT_MASK(32))) {
		pr_err("axiado-drm: Bad mask\n");
		return -EINVAL;
	}
	ret = component_master_add_with_match(dev, &axiado_drm_ops, match);
	if (ret < 0) {
		axiado_drm_match_remove(dev);
		return ret;
	}
	return 0;
}

static int axiado_drm_platform_remove(struct platform_device *pdev)
{
	if (pdev == NULL)
		return -EINVAL;
	component_master_del(&pdev->dev, &axiado_drm_ops);

	axiado_drm_match_remove(&pdev->dev);

	return 0;
}

static void axiado_drm_platform_shutdown(struct platform_device *pdev)
{
	struct drm_device *drm = platform_get_drvdata(pdev);

	if (drm)
		drm_atomic_helper_shutdown(drm);
}

static const struct of_device_id axiado_drm_dt_ids[] = {
	{
		.compatible = "axiado,display-subsystem",
	},
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, axiado_drm_dt_ids);

static struct platform_driver axiado_drm_platform_driver = {
	.probe = axiado_drm_platform_probe,
	.remove = axiado_drm_platform_remove,
	.shutdown = axiado_drm_platform_shutdown,
	.driver = {
		.name = "axiado-drm",
		.of_match_table = axiado_drm_dt_ids,
	},
};

#define ADD_AXIADO_SUB_DRIVER(drv, cfg)                                 \
	{                                                                   \
		if (IS_ENABLED(CONFIG_##cfg) &&                                 \
		    !WARN_ON(num_axiado_sub_drivers >= MAX_AXIADO_SUB_DRIVERS)) \
			axiado_sub_drivers[num_axiado_sub_drivers++] = &drv;        \
	}

static int __init axiado_drm_init(void)
{
	int ret;

	num_axiado_sub_drivers = 0;

	ADD_AXIADO_SUB_DRIVER(db9000_drm_platform_driver, AXIADO_LCDC);
	ADD_AXIADO_SUB_DRIVER(axiado_hdmi_driver, AXIADO_HDMI);
	ret = platform_register_drivers(axiado_sub_drivers,
					num_axiado_sub_drivers);
	if (ret)
		return ret;
	ret = platform_driver_register(&axiado_drm_platform_driver);
	if (ret)
		goto err_unreg_drivers;
	return 0;

err_unreg_drivers:
	platform_unregister_drivers(axiado_sub_drivers, num_axiado_sub_drivers);
	return ret;
}

static void __exit axiado_drm_exit(void)
{
	platform_driver_unregister(&axiado_drm_platform_driver);
	platform_unregister_drivers(axiado_sub_drivers, num_axiado_sub_drivers);
}

module_init(axiado_drm_init);
module_exit(axiado_drm_exit);

MODULE_AUTHOR("AXIADO CORPORATION");
MODULE_DESCRIPTION("Axiado DRM Driver");
MODULE_LICENSE("GPL");
