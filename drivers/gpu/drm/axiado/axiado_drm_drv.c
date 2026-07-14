// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022-2026 Axiado Corporation.
 */

#include <linux/aperture.h>
#include <linux/component.h>
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/of_graph.h>
#include <linux/of_platform.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>

#include <drm/drm_atomic_helper.h>
#include <drm/clients/drm_client_setup.h>
#include <drm/drm_drv.h>
#include <drm/drm_fbdev_dma.h>
#include <drm/drm_gem_dma_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_of.h>
#include <drm/drm_probe_helper.h>

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
	.major = 1,
	.minor = 0,
	.patchlevel = 0,
	.fops = &axiado_driver_fops,
	DRM_GEM_DMA_DRIVER_OPS_WITH_DUMB_CREATE(drm_gem_dma_dumb_create_internal),
	DRM_FBDEV_DMA_DRIVER_OPS,
};

static const struct drm_mode_config_funcs axiado_mode_config_funcs = {
	.fb_create = drm_gem_fb_create,
	.atomic_check = drm_atomic_helper_check,
	.atomic_commit = drm_atomic_helper_commit,
};

static int axiado_drm_bind(struct device *dev)
{
	struct drm_device *drm_dev;
	int ret;

	/*
	 * Evict any firmware framebuffer (simplefb / efifb) that may have
	 * claimed our display aperture before this driver loaded. Must
	 * happen before drm_dev_register() — once we are live, fbdev
	 * helpers will try to attach a framebuffer to the same region.
	 */
	ret = aperture_remove_all_conflicting_devices(axiado_drm_driver.name);
	if (ret)
		return ret;

	drm_dev = drm_dev_alloc(&axiado_drm_driver, dev);
	if (IS_ERR(drm_dev))
		return PTR_ERR(drm_dev);

	dev_set_drvdata(dev, drm_dev);

	/*
	 * drmm_mode_config_init() registers drm_mode_config_cleanup() as a
	 * drm-managed action so it fires automatically on drm_dev_put().
	 * Do not call drm_mode_config_cleanup() manually — it would run
	 * twice and walk freed list heads.
	 */
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
		goto err_free;

	drm_mode_config_reset(drm_dev);
	drm_kms_helper_poll_init(drm_dev);

	ret = drm_dev_register(drm_dev, 0);
	if (ret)
		goto err_poll_fini;

	/*
	 * bpp = 0 lets fbdev pick the preferred depth from the connector's
	 * preferred mode. Hard-coding 24 forced XRGB888 even when the
	 * pipeline prefers XRGB8888 / RGB565.
	 */
	drm_client_setup(drm_dev, NULL);
	return 0;

err_poll_fini:
	drm_kms_helper_poll_fini(drm_dev);
	component_unbind_all(dev, drm_dev);
err_free:
	dev_set_drvdata(dev, NULL);
	drm_dev_put(drm_dev);
	return ret;
}

static void axiado_drm_unbind(struct device *dev)
{
	struct drm_device *drm_dev = dev_get_drvdata(dev);

	if (!drm_dev)
		return;

	drm_dev_unregister(drm_dev);
	drm_kms_helper_poll_fini(drm_dev);
	drm_atomic_helper_shutdown(drm_dev);
	component_unbind_all(dev, drm_dev);
	dev_set_drvdata(dev, NULL);
	drm_dev_put(drm_dev);
}

static int compare_of(struct device *dev, void *data)
{
	return dev->of_node == data;
}

static void axiado_drm_match_remove(struct device *dev)
{
	struct device_link *link, *tmp;

	list_for_each_entry_safe(link, tmp, &dev->links.consumers, s_node)
		device_link_del(link);
}

static void axiado_drm_match_add_endpoints(struct device *dev,
					   struct component_match **match,
					   struct device_node *port)
{
	struct device_node *ep, *remote;

	for_each_child_of_node(port, ep) {
		if (!of_node_name_eq(ep, "endpoint"))
			continue;

		remote = of_graph_get_remote_port_parent(ep);
		if (!remote || !of_device_is_available(remote)) {
			of_node_put(remote);
			continue;
		}

		component_match_add(dev, match, compare_of, remote);
		of_node_put(remote);
	}
}

static struct component_match *axiado_drm_match_add(struct device *dev)
{
	struct component_match *match = NULL;
	struct device_node *np = dev->of_node;
	struct device_node *port;
	int i;

	if (!np)
		return NULL;

	/* Parse the "ports" property from the virtual display-subsystem node */
	for (i = 0;; i++) {
		port = of_parse_phandle(np, "ports", i);
		if (!port)
			break;

		if (!of_device_is_available(port->parent)) {
			of_node_put(port);
			continue;
		}

		/* 1. Add the display controller component (disp_ctrl) */
		component_match_add(dev, &match, compare_of, port->parent);

		/* 2. Traverse the graph and add connected endpoints (hdmi) */
		axiado_drm_match_add_endpoints(dev, &match, port);

		of_node_put(port);
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
		dev_err(dev, "missing 'ports' property\n");
		return -ENODEV;
	}
	if (!found) {
		dev_err(dev, "no available display controller found\n");
		return -ENODEV;
	}
	return 0;
}

static int axiado_drm_platform_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct component_match *match = NULL;
	int ret;

	ret = axiado_drm_platform_of_probe(dev);
	if (ret)
		return ret;

	/*
	 * Initialize the reserved memory pool for GEM DMA allocations.
	 * -ENODEV means "no memory-region phandle, fall back to the
	 * platform default" — that is fine. Any other error must be
	 * surfaced and the resource (if partially taken) released on the
	 * error paths below.
	 */
	ret = of_reserved_mem_device_init(dev);
	if (ret && ret != -ENODEV) {
		dev_err(dev, "of_reserved_mem_device_init failed: %d\n", ret);
		return ret;
	}

	ret = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(32));
	if (ret) {
		dev_err(dev, "dma_set_mask_and_coherent failed: %d\n", ret);
		goto err_release_rmem;
	}

	match = axiado_drm_match_add(dev);
	if (IS_ERR(match)) {
		ret = PTR_ERR(match);
		goto err_release_rmem;
	}

	ret = component_master_add_with_match(dev, &axiado_drm_ops, match);
	if (ret < 0)
		goto err_match_remove;

	return 0;

err_match_remove:
	axiado_drm_match_remove(dev);
err_release_rmem:
	of_reserved_mem_device_release(dev);
	return ret;
}

static void axiado_drm_platform_remove(struct platform_device *pdev)
{
	component_master_del(&pdev->dev, &axiado_drm_ops);
	axiado_drm_match_remove(&pdev->dev);
	of_reserved_mem_device_release(&pdev->dev);
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

#define ADD_AXIADO_SUB_DRIVER(drv, cfg)                                      \
	{                                                                    \
		if (IS_ENABLED(CONFIG_##cfg) &&                              \
		    !WARN_ON(num_axiado_sub_drivers >=                       \
			     MAX_AXIADO_SUB_DRIVERS))                        \
			axiado_sub_drivers[num_axiado_sub_drivers++] = &drv; \
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

MODULE_AUTHOR("Axiado Corporation");
MODULE_DESCRIPTION("Axiado DRM Driver");
MODULE_LICENSE("GPL");
