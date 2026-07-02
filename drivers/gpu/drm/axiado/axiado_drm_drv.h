/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2022-2025 Axiado Corporation.
 *
 * Based on rockchip_drm_drv.h,
 *   originally (C) Fuzhou Rockchip Electronics Co.Ltd
 *   Author: Mark Yao <mark.yao@rock-chips.com>
 * which itself is based on exynos_drm_drv.h.
 */

#ifndef _AXIADO_DRM_DRV_H
#define _AXIADO_DRM_DRV_H

#include <drm/drm_fb_helper.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_gem.h>

#include <linux/module.h>
#include <linux/component.h>

struct drm_device;
struct drm_connector;
struct iommu_domain;

extern struct platform_driver axiado_hdmi_driver;
extern struct platform_driver db9000_drm_platform_driver;

#endif /* _AXIADO_DRM_DRV_H_ */
