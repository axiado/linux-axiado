// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022-2025 Axiado Corporation.
 *
 * Based on ltdc.c
 * Copyright (C) STMicroelectronics SA 2017
 *   Authors: Philippe Cornu <philippe.cornu@st.com>
 *          Yannick Fertre <yannick.fertre@st.com>
 *          Fabien Dessenne <fabien.dessenne@st.com>
 *          Mickael Reulier <mickael.reulier@st.com>
 */

#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_device.h>
#include <drm/drm_drv.h>
#include <drm/drm_fb_dma_helper.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_gem_dma_helper.h>
#include <drm/drm_managed.h>
#include <drm/drm_of.h>
#include <drm/drm_panel.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_simple_kms_helper.h>
#include <linux/backlight.h>
#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_plane_helper.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_vblank.h>
#include <linux/component.h>
#include <video/display_timing.h>
#include "axiado_lcdc.h"

static const u32 db9000_fmts[] = {
	DRM_FORMAT_RGB888, DRM_FORMAT_RGB565, DRM_FORMAT_XRGB8888,
	DRM_FORMAT_BGR888, DRM_FORMAT_BGR565,
};

static const u64 db9000_format_modifiers[] = { DRM_FORMAT_MOD_LINEAR,
					       DRM_FORMAT_MOD_INVALID };

static u32 reg_read(struct db9000 *db9000, u32 offset)
{
	return readl_relaxed(db9000->regs + (offset));
}

static void reg_write(struct db9000 *db9000, u32 reg, u32 val)
{
	writel_relaxed(val, db9000->regs + reg);
}

static struct db9000 *plane_to_db9000(struct drm_plane *plane)
{
	return (struct db9000 *)plane->dev->dev_private;
}

static inline struct db9000 *crtc_to_db9000(struct drm_crtc *crtc)
{
	return (struct db9000 *)crtc->dev->dev_private;
}

void db9000_bpp_setup(struct db9000 *db9000, int bpp, int bus_width,
		      bool pixel_select)
{
	u32 format;
	u32 reg_cr1 = reg_read(db9000, DB9000_CR1);
	/* reset the BPP bits */
	reg_cr1 &= ~DB9000_CR1_BPP(7);
	reg_cr1 &= ~DB9000_CR1_OPS(5);
	reg_cr1 &= ~DB9000_CR1_OPS(1);
	db9000->bpp = bpp;

	switch (bpp) {
	case 16:
		if (pixel_select) {
			reg_cr1 |= DB9000_CR1_OPS(5);
			reg_cr1 |= DB9000_CR1_OPS(1);
		}

		format = DB9000_CR1_BPP(DB9000_CR1_BPP_16bpp);
		break;
	case 24:
	case 32:
	default:
		format = DB9000_CR1_BPP(DB9000_CR1_BPP_24bpp);
	}
	if (bpp <= DB9000_BPP_16 && bus_width == 24)
		reg_cr1 |= DB9000_CR1_OPS(2);
	else
		reg_cr1 &= ~DB9000_CR1_OPS(2);
	if (bpp == DB9000_BPP_24)
		reg_cr1 |= DB9000_CR1_FBP;
	else
		reg_cr1 &= ~DB9000_CR1_FBP;

	reg_cr1 |= format;
	reg_write(db9000, DB9000_CR1, reg_cr1);
}


void db9000_reset(struct db9000 *db9000)
{
	int i = 0, j = 0x0;

	for (i = 0; i < 13; i++) {
		reg_write(db9000, j, DB9000_DEF_VAL);
		j = j + DB9000_REG_OFFSET;
	}
}

void db9000_controller_on(struct db9000 *db9000)
{
	unsigned long flags;
	/* Release pixel clock domain reset */
	reg_write(db9000, DB9000_PCTR, DB9000_PCTR_PCR | DB9000_PCTR_PCI);
	/* Enable BAU event for IRQ */
	spin_lock_irqsave(&db9000->lock, flags);
	reg_write(db9000, DB9000_ISR, DB9000_ISR_VAL);
	spin_unlock_irqrestore(&db9000->lock, flags);
}

static irqreturn_t db9000_irq_thread(int irq, void *arg)
{
	struct drm_device *ddev = arg;
	struct db9000 *db9000 = ddev->dev_private;
	struct drm_crtc *crtc = drm_crtc_from_index(ddev, 0);
	u32 status = db9000->irqstatus;

	if (status == 0)
		status = reg_read(db9000, DB9000_ISR);
	/* Read ISR to check authoritative status */

	/* Check vertical-compare trigger -> treat as vblank */
	if (status & DB9000_ISR_VCT) {
		/* Acknowledge/clear the VCT bit in the hardware ISR */
		reg_write(db9000, DB9000_ISR, status & DB9000_ISR_VCT);

		/* Inform DRM core of vblank */
		drm_crtc_handle_vblank(crtc);
	}
	return IRQ_HANDLED;
}

static irqreturn_t db9000_irq(int irq, void *arg)
{
	struct drm_device *ddev = arg;
	struct db9000 *db9000 = ddev->dev_private;
	/* Read ISR – hardware status */
	db9000->irqstatus = reg_read(db9000, DB9000_ISR);

	/* Acknowledge/clear interrupt bits in ISR */
	reg_write(db9000, DB9000_ISR, db9000->irqstatus);

	return IRQ_WAKE_THREAD;
}

/* CRTC Functions */
static int db9000_plane_atomic_check(struct drm_plane *plane,
				     struct drm_atomic_state *state)

{
	struct drm_plane_state *new_plane_state =
		drm_atomic_get_new_plane_state(state, plane);
	struct drm_framebuffer *fb = new_plane_state->fb;
	u32 src_w, src_h;

	if (!fb)
		return 0;
	/* convert src_ from 16:16 format */
	src_w = new_plane_state->src_w >> 16;
	src_h = new_plane_state->src_h >> 16;
	/* Reject scaling */
	if (src_w != new_plane_state->crtc_w ||
	    src_h != new_plane_state->crtc_h) {
		DRM_ERROR("Scaling is not supported");
		return -EINVAL;
	}
	return 0;
}

static void db9000_plane_atomic_disable(struct drm_plane *plane,
					struct drm_atomic_state *oldstate)
{
	struct db9000 *db9000 = plane_to_db9000(plane);
	u32 imr;
	/* disable IRQ */
	imr = reg_read(db9000, DB9000_IMR);
	reg_write(db9000, DB9000_IMR, imr & ~DB9000_IMR_BAUM);
}

static void db9000_plane_atomic_update(struct drm_plane *plane,
				       struct drm_atomic_state *oldstate)
{
	struct db9000 *db9000 = plane_to_db9000(plane);
	struct drm_plane_state *state = plane->state;
	struct drm_plane_state *newstate =
		drm_atomic_get_new_plane_state(oldstate, plane);
	struct drm_framebuffer *fb = state->fb;
	u32 paddr, dear_offset;
	u32 format;

	if (!state->crtc || !fb)
		return;

	if (!newstate->crtc || !fb)
		return;

	format = fb->format->format;
	/* Check for format changes */
	if (format == DRM_FORMAT_RGB565 || format == DRM_FORMAT_BGR565) {
		db9000_bpp_setup(db9000, 16, db9000->bus_width, false);
	} else if (format == DRM_FORMAT_XRGB1555 ||
		   format == DRM_FORMAT_XBGR1555) {
		db9000_bpp_setup(db9000, 16, db9000->bus_width, true);
	} else if (format == DRM_FORMAT_RGB888 || format == DRM_FORMAT_BGR888) {
		db9000_bpp_setup(db9000, 32, db9000->bus_width, false);
	} else if (format == DRM_FORMAT_XRGB8888 ||
		   format == DRM_FORMAT_XBGR8888) {
		db9000_bpp_setup(db9000, 32, db9000->bus_width, false);
	} else {
		dev_err(db9000->dev, "No supported format\n");
		return;
	}
	dear_offset = (db9000->frame_size * db9000->bpp) / 8 + 8;
	/* If the frame buffer has changed get the new FB address */
	if (newstate->fb) {
		paddr = db9000->paddr_disp; /* PCIe Display DDR Address */
		reg_write(db9000, DB9000_DBAR, paddr);
		reg_write(db9000, DB9000_DEAR, paddr + dear_offset);
	}
}

static void db9000_plane_atomic_print_state(struct drm_printer *p,
					    const struct drm_plane_state *state)
{
	struct drm_plane *plane = state->plane;
	struct db9000 *db9000 = plane_to_db9000(plane);
	struct fps_info *fpsi = db9000->plane_fpsi;
	int ms_since_last;
	ktime_t now;

	now = ktime_get();
	ms_since_last = ktime_to_ms(ktime_sub(now, fpsi->last_timestamp));

	drm_printf(p, "\tuser_updates=%dfps\n",
		   DIV_ROUND_CLOSEST(fpsi->counter * 1000, ms_since_last));

	fpsi->last_timestamp = now;
	fpsi->counter = 0;
}

static bool db9000_plane_format_mod_supported(struct drm_plane *plane,
					      u32 format, u64 modifier)
{
	if (!plane)
		return false;
	if (modifier == DRM_FORMAT_MOD_LINEAR)
		return true;
	return false;
}

static const struct drm_plane_funcs db9000_plane_funcs = {
	.update_plane = drm_atomic_helper_update_plane,
	.disable_plane = drm_atomic_helper_disable_plane,
	.destroy = drm_plane_cleanup,
	.reset = drm_atomic_helper_plane_reset,
	.atomic_duplicate_state = drm_atomic_helper_plane_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_plane_destroy_state,
	.atomic_print_state = db9000_plane_atomic_print_state,
	.format_mod_supported = db9000_plane_format_mod_supported,
};

static int db9000_crtc_enable_vblank(struct drm_crtc *crtc)
{
	struct db9000 *db9000 = crtc_to_db9000(crtc);
	struct drm_crtc_state *state = crtc->state;
	u32 imr;

	if (!state->enable)
		return -EPERM;

	/* Set ISCR if required by controller */
	reg_write(db9000, DB9000_ISCR, DB9000_ISCR_VAL);

	/* UNMASK VCT interrupt -> enable vblank IRQ */
	imr = reg_read(db9000, DB9000_IMR);
	imr &= ~DB9000_IMR_VCTM;
	reg_write(db9000, DB9000_IMR, imr);
	drm_crtc_vblank_on(crtc);

	return 0;
}

static void db9000_crtc_disable_vblank(struct drm_crtc *crtc)
{
	struct db9000 *db9000 = crtc_to_db9000(crtc);
	u32 imr;

	drm_crtc_vblank_off(crtc);
	/* MASK VCT interrupt -> disable vblank IRQ */
	imr = reg_read(db9000, DB9000_IMR);
	imr |= DB9000_IMR_VCTM;
	reg_write(db9000, DB9000_IMR, imr);
}

static void db9000_crtc_mode_set_nofb(struct drm_crtc *crtc)
{
	struct db9000 *db9000 = crtc_to_db9000(crtc);
	struct drm_device *ddev = crtc->dev;
	struct drm_connector_list_iter iter;
	struct drm_connector *connector = NULL;
	struct drm_encoder *encoder = NULL, *en_iter;
	struct drm_display_mode *mode = &crtc->state->adjusted_mode;
	u32 bus_flags = 0, reg_cr1;
	u32 vfront_porch, vback_porch, hfront_porch, hback_porch;
	/* The horizontal and vertical timings are defined per the following diagram.
	 *
	 * ::
	 *
	 *
	 *               Active                 Front           Sync           Back
	 *              Region                 Porch                          Porch
	 *     <-----------------------><----------------><-------------><-------------->
	 *       //////////////////////|
	 *      ////////////////////// |
	 *     //////////////////////  |..................               ................
	 *                                                _______________
	 *     <----- [hv]display ----->
	 *     <------------- [hv]sync_start ------------>
	 *     <--------------------- [hv]sync_end --------------------->
	 *     <-------------------------------- [hv]total ----------------------------->*
	 */
	reg_cr1 = reg_read(db9000, DB9000_CR1);
	vfront_porch = mode->vsync_start - mode->vdisplay;
	vback_porch = mode->vtotal - mode->vsync_end;
	hfront_porch = mode->hsync_start - mode->hdisplay;
	hback_porch = mode->htotal - mode->hsync_end;

	/* get encoder from crtc */
	drm_for_each_encoder(en_iter, ddev) {
		if (en_iter->crtc == crtc) {
			encoder = en_iter;
			break;
		}
	}
	if (encoder) {
		/* Get the connector from encoder */
		drm_connector_list_iter_begin(ddev, &iter);
		drm_for_each_connector_iter(connector, &iter) {
			if (connector->encoder == encoder)
				break;
		}
		drm_connector_list_iter_end(&iter);
	}
	if (connector)
		bus_flags = connector->display_info.bus_flags;

	if (mode->flags & DISPLAY_FLAGS_HSYNC_HIGH)
		reg_cr1 |= DB9000_CR1_HSP;
	else
		reg_cr1 &= ~DB9000_CR1_HSP;

	if (mode->flags & DISPLAY_FLAGS_VSYNC_HIGH)
		reg_cr1 |= DB9000_CR1_VSP;
	else
		reg_cr1 &= ~DB9000_CR1_VSP;

	reg_cr1 |= DB9000_CR1_DEP;

	db9000->frame_size = mode->hdisplay * mode->vdisplay;

	/* DEAR register must be configured to the block end + 8 */
	if (mode->hdisplay == 1280 && mode->vdisplay == 720) {
		reg_write(db9000, DB9000_CR1, CR1_TEST_SET_A_1280);
		reg_write(db9000, DB9000_HTR, SET_HTR_PARM_1280);
		reg_write(db9000, DB9000_VTR1, SET_VTR1_PARM_1280);
		reg_write(db9000, DB9000_VTR2, SET_VTR2_PARM_1280);
	} else if (mode->hdisplay == 640 && mode->vdisplay == 480) {
		reg_write(db9000, DB9000_CR1, CR1_TEST_SET_A);
		reg_write(db9000, DB9000_HTR, SET_HTR_PARM);
		reg_write(db9000, DB9000_VTR1, SET_VTR1_PARM);
		reg_write(db9000, DB9000_VTR2, SET_VTR2_PARM);
	} else {
		dev_err(db9000->dev, "drm-db9000: No matching resolution -%s\n", mode->name);
	}
	/* Pixel Control and Control Register Enabled for early fbcon access */
	reg_write(db9000, DB9000_CR1, DB9000_CR1_VAL3);
}

static enum drm_mode_status
db9000_crtc_mode_valid(struct drm_crtc *crtc,
		       const struct drm_display_mode *mode)
{
	if (!crtc || !mode)
		return MODE_ERROR;

	return MODE_OK;
}

static bool db9000_crtc_mode_fixup(struct drm_crtc *crtc,
				   const struct drm_display_mode *mode,
				   struct drm_display_mode *adjusted_mode)
{
	if (!crtc || !mode || !adjusted_mode)
		return false;

	return true;
}

static void db9000_crtc_atomic_flush(struct drm_crtc *crtc,
				     struct drm_atomic_state *state)
{
	struct drm_device *ddev = crtc->dev;
	struct drm_pending_vblank_event *event = crtc->state->event;

	if (event) {
		crtc->state->event = NULL;
		spin_lock_irq(&ddev->event_lock);
		if (drm_crtc_vblank_get(crtc) == 0)
			drm_crtc_arm_vblank_event(crtc, event);
		else
			drm_crtc_send_vblank_event(crtc, event);
		spin_unlock_irq(&ddev->event_lock);
	}
}

static void db9000_crtc_atomic_enable(struct drm_crtc *crtc,
				      struct drm_atomic_state *state)
{
	struct db9000 *db9000 = crtc_to_db9000(crtc);
	struct drm_device *ddev = crtc->dev;
	u32 iscr, mask;

	 pm_runtime_get_sync(ddev->dev);
	iscr = reg_read(db9000, DB9000_ISCR);
	mask = iscr | DB9000_ISCR_OFO | DB9000_ISCR_IFU;
	reg_write(db9000, DB9000_ISCR, mask);
}

static void db9000_crtc_atomic_disable(struct drm_crtc *crtc,
				       struct drm_atomic_state *state)
{

	pr_debug("Atomic CRTC Disable\n");
}

static const struct drm_crtc_helper_funcs db9000_crtc_helper_funcs = {
	.mode_valid = db9000_crtc_mode_valid,
	.mode_fixup = db9000_crtc_mode_fixup,
	.mode_set_nofb = db9000_crtc_mode_set_nofb,
	.atomic_flush = db9000_crtc_atomic_flush,
	.atomic_enable = db9000_crtc_atomic_enable,
	.atomic_disable = db9000_crtc_atomic_disable,
};

static const struct drm_crtc_funcs db9000_crtc_funcs = {
	.destroy = drm_crtc_cleanup,
	.set_config = drm_atomic_helper_set_config,
	.page_flip = drm_atomic_helper_page_flip,
	.reset = drm_atomic_helper_crtc_reset,
	.atomic_duplicate_state = drm_atomic_helper_crtc_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_crtc_destroy_state,
	.enable_vblank = db9000_crtc_enable_vblank,
	.disable_vblank = db9000_crtc_disable_vblank,
};

static const struct drm_plane_helper_funcs db9000_plane_helper_funcs = {
	.atomic_check = db9000_plane_atomic_check,
	.atomic_update = db9000_plane_atomic_update,
	.atomic_disable = db9000_plane_atomic_disable,
};

static struct drm_plane *db9000_plane_create(struct drm_device *ddev,
					     enum drm_plane_type type)
{
	unsigned long possible_crtcs = CRTC_MASK;
	struct device *dev = ddev->dev;
	struct drm_plane *plane;
	int ret;

	plane = devm_kzalloc(dev, sizeof(*plane), GFP_KERNEL);
	if (!plane)
		return NULL;
	ret = drm_universal_plane_init(ddev, plane, possible_crtcs,
				       &db9000_plane_funcs, db9000_fmts,
				       ARRAY_SIZE(db9000_fmts), NULL, type,
				       NULL);
	if (ret < 0)
		return NULL;
	drm_plane_helper_add(plane, &db9000_plane_helper_funcs);
	return plane;
}

static int db9000_crtc_init(struct drm_device *ddev, struct drm_crtc *crtc)
{
	struct drm_plane *primary, *overlay;
	int ret;

	if (!crtc)
		return -EINVAL;
	primary = db9000_plane_create(ddev, DRM_PLANE_TYPE_PRIMARY);
	if (!primary) {
		DRM_ERROR("Can not create primary plane\n");
		return -EINVAL;
	}
	ret = drm_crtc_init_with_planes(ddev, crtc, primary, NULL,
			&db9000_crtc_funcs, NULL);
	if (ret) {
		DRM_ERROR("Can not initialize CRTC\n");
		goto err;
	}
	drm_crtc_helper_add(crtc, &db9000_crtc_helper_funcs);
	drm_mode_crtc_set_gamma_size(crtc, CLUT_SIZE);
	drm_crtc_enable_color_mgmt(crtc, 0, false, CLUT_SIZE);

	DRM_DEBUG("CRTC:%d created\n", crtc->base.id);
	overlay = db9000_plane_create(ddev, DRM_PLANE_TYPE_OVERLAY);
	if (!overlay) {
		ret = -ENOMEM;
		DRM_ERROR("Can not create overlay plane\n");
		goto err;
	}
	return 0;

err:
	return ret;
}

static int db9000_reinit_state(struct seq_file *s, void *data)
{
	struct db9000 *db9000 = (struct db9000 *)dev_get_drvdata(s->private);
	u32 err = 0, iscr, mask;
	u32 paddr, dear_offset;

	if (db9000 == NULL) {
		pr_err("DB9000 Info not Obtained\n");
		return err;
	}
	paddr = db9000->paddr_disp;
	dear_offset = (db9000->frame_size * db9000->bpp) / 8 + 8;
	reg_write(db9000, DB9000_CR1, CR1_TEST_SET_A);
	reg_write(db9000, DB9000_HTR, SET_HTR_PARM);
	reg_write(db9000, DB9000_VTR1, SET_VTR1_PARM);
	reg_write(db9000, DB9000_VTR2, SET_VTR2_PARM);
	reg_write(db9000, DB9000_CR1, DB9000_CR1_VAL3);
	db9000_bpp_setup(db9000, 32, db9000->bus_width, false);
	reg_write(db9000, DB9000_DBAR, paddr);
	reg_write(db9000, DB9000_DEAR, paddr + dear_offset);
	iscr = reg_read(db9000, DB9000_ISCR);
	mask = iscr | DB9000_ISCR_OFO | DB9000_ISCR_IFU;
	reg_write(db9000, DB9000_ISCR, mask);

	if (err < 0)
		seq_puts(s, "DB9000 reinit failed\n");
	else
		seq_puts(s, "DB9000 reinit Done\n");

	return err;
}

static void ax_db9000_debugfs_exit(struct db9000 *db9000)
{
	debugfs_remove_recursive(db9000->debugfs);
	db9000->debugfs = NULL;
}

static void ax_db9000_debugfs_init(struct db9000 *db9000)
{
	db9000->debugfs = debugfs_create_dir("db9000", NULL);

	debugfs_create_devm_seqfile(db9000->dev, "db9000_reinit_state", db9000->debugfs,
			db9000_reinit_state);

}

static int axiado_db9000_bind(struct device *dev, struct device *master, void *data)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct drm_device *ddev = data;
	struct db9000 *db9000;
	struct drm_crtc *crtc;
	struct device_node *np = dev->of_node;
	struct device_node *port;
	struct device_node *disp_node_entry;
	struct resource res;

	u32 bpp, addr;
	u32 bus_width;
	int ret, irq;

	db9000 = devm_kzalloc(dev, sizeof(*db9000), GFP_KERNEL);
	if (!db9000)
		return -ENOMEM;
	db9000->dev = dev;
	ddev->dev_private = (void *)db9000; /* for crash resolve to trace back */
	/* Parse the DTB */
	ret = of_property_read_u32(np, "bits-per-pixel", &bpp);
	if (ret)
		bpp = 24;

	if (bpp != 16 && bpp != 24 && bpp != 32)
		bpp = 16;
	ret = of_property_read_u32(np, "bus-width", &bus_width);
	if (ret)
		bus_width = 24;
	db9000->bus_width = bus_width;
	db9000->bpp = bpp;

	ret = of_property_read_u32(np, "paddr-disp", &addr);
	if (ret) {
		dev_err(db9000->dev, "PCIe addr paddr-disp property not found\n");
		return -EINVAL;
	}
	db9000->paddr_disp = addr;
	spin_lock_init(&db9000->lock);
	/* Memory setup */
	db9000->regs = devm_platform_get_and_ioremap_resource(pdev, 0, NULL);
	if (IS_ERR(db9000->regs))
		return PTR_ERR(db9000->regs);

	/* Parsing the memory region for the data control path */
	disp_node_entry = of_parse_phandle(np, "memory-region", 0);
	if (!disp_node_entry) {
		dev_err(db9000->dev, "PCIe addr memory-region property not found\n");
		return -EINVAL;
	}

	if (of_address_to_resource(disp_node_entry, 0, &res)) {
		dev_err(db9000->dev, "Cannot get memory resource from device tree\n");
		return -EINVAL;
	}

	/* Memory mapping for the Control Path */
	db9000->data_reg =
		devm_memremap(db9000->dev, res.start, res.end - res.start, MEMREMAP_WB);
	if (!db9000->data_reg) {
		dev_err(db9000->dev, "failed to map reserved buffer!\n");
		return PTR_ERR(db9000->data_reg);
	}

	/* Clock setup */
	db9000->lcd_eclk = devm_clk_get(dev, "clk_pclk");
	if (IS_ERR(db9000->lcd_eclk)) {
		DRM_ERROR("Unable to get pixel clock\n");
		return -ENODEV;
	}
	if (clk_prepare_enable(db9000->lcd_eclk)) {
		DRM_ERROR("Unable to prepare pixel clock\n");
		return -ENODEV;
	}
	db9000_reset(db9000);
	db9000_controller_on(db9000);
	db9000_bpp_setup(db9000, db9000->bpp, db9000->bus_width, false);

	/* Disable Interrupt */
	reg_write(db9000, DB9000_IMR, DB9000_IMR_VAL2);
	crtc = devm_kzalloc(dev, sizeof(*crtc), GFP_KERNEL);
	if (!crtc) {
		DRM_ERROR("Failed to allocate crtc\n");
		ret = -ENOMEM;
		goto err;
	}
	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		ret = irq;
		goto err;
	}
	ret = devm_request_threaded_irq(dev, irq, db9000_irq, db9000_irq_thread,
			IRQF_ONESHOT, dev_name(dev), ddev);
	if (ret) {
		DRM_ERROR("Failed to register DB9000 interrupt\n");
		goto err;
	}
	ret = db9000_crtc_init(ddev, crtc);
	if (ret) {
		DRM_ERROR("Failed to init crtc\n");
		goto err;
	}
	port = of_get_child_by_name(dev->of_node, "port");
	if (!port) {
		DRM_ERROR("no port node found in %pOF\n", dev->of_node);
		ret = -ENOENT;
		goto err;
	}
	crtc->port = port;
	ret = drm_vblank_init(ddev, NB_CRTC);
	if (ret) {
		DRM_ERROR("Failed calling drm_vblank_init()\n");
		goto err;
	}
	dev_set_drvdata(dev, db9000);
	if (IS_ENABLED(CONFIG_DEBUG_FS))
		ax_db9000_debugfs_init(db9000);
	return 0;

err:
	return ret;
}

static void axiado_db9000_unbind(struct device *dev, struct device *master, void *data)
{
	struct db9000 *db9000 = dev_get_drvdata(dev);

	if (IS_ENABLED(CONFIG_DEBUG_FS))
		ax_db9000_debugfs_exit(db9000);
	clk_disable_unprepare(db9000->lcd_eclk);
}

static const struct component_ops axiado_db9000_ops = {
	.bind = axiado_db9000_bind,
	.unbind = axiado_db9000_unbind,
};

static int db9000_drm_platform_probe(struct platform_device *pdev)
{
	if (!pdev)
		return -EINVAL;
	return component_add(&pdev->dev, &axiado_db9000_ops);
}

static int db9000_drm_platform_remove(struct platform_device *pdev)
{
	if (!pdev)
		return -EINVAL;
	component_del(&pdev->dev, &axiado_db9000_ops);

	return 0;
}

static void db9000_drm_platform_shutdown(struct platform_device *pdev)
{
	struct db9000 *db9000 = dev_get_drvdata(&pdev->dev);

	if (!db9000)
		return;

	/* Mask all interrupts for the controller to prevent stray IRQs */
	reg_write(db9000, DB9000_IMR, DB9000_IMR_VAL2);

	/* try to acknowledge/clear any pending ISR bits (best-effort) */
	reg_write(db9000, DB9000_ISR, reg_read(db9000, DB9000_ISR));

	/* Disable the pixel clock if it's enabled */
	if (db9000->lcd_eclk)
		clk_disable_unprepare(db9000->lcd_eclk);
}

static const struct of_device_id db9000_dt_ids[] = {
	{ .compatible = "axiado,drm-db9000" },
};
MODULE_DEVICE_TABLE(of, db9000_dt_ids);

struct platform_driver db9000_drm_platform_driver = {
	.probe = db9000_drm_platform_probe,
	.remove = db9000_drm_platform_remove,
	.shutdown = db9000_drm_platform_shutdown,
	.driver = {
		.name = "drm-db9000",
		.of_match_table = db9000_dt_ids,
	},
};

MODULE_AUTHOR("AXIADO CORPORATION");
MODULE_DESCRIPTION("Axiado LCD Controller Driver");
MODULE_LICENSE("GPL");
