// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022-2026 Axiado Corporation.
 */

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>

#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_device.h>
#include <drm/drm_drv.h>
#include <drm/drm_fb_dma_helper.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_gem_dma_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_managed.h>
#include <drm/drm_of.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_vblank.h>

#include "axiado_lcdc.h"

static const u32 db9000_fmts[] = {
	DRM_FORMAT_RGB888, DRM_FORMAT_RGB565, DRM_FORMAT_XRGB8888,
	DRM_FORMAT_BGR888, DRM_FORMAT_BGR565,
};

static const u64 db9000_format_modifiers[] = { DRM_FORMAT_MOD_LINEAR,
					       DRM_FORMAT_MOD_INVALID };

static struct db9000 *plane_to_db9000(struct drm_plane *plane)
{
	return container_of(plane, struct db9000, plane);
}

static inline struct db9000 *crtc_to_db9000(struct drm_crtc *crtc)
{
	return container_of(crtc, struct db9000, crtc);
}

static void db9000_bpp_setup(struct db9000 *db9000, int bpp, int bus_width,
			     bool pixel_select)
{
	u32 format;
	u32 reg_cr1 = readl_relaxed(db9000->regs + DB9000_CR1);

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
		format = DB9000_CR1_BPP(DB9000_CR1_BPP_16);
		break;
	case 24:
	case 32:
	default:
		format = DB9000_CR1_BPP(DB9000_CR1_BPP_24);
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
	writel_relaxed(reg_cr1, db9000->regs + DB9000_CR1);
}

static void db9000_reset(struct db9000 *db9000)
{
	int i = 0, j = 0x0;

	for (i = 0; i < 13; i++) {
		writel_relaxed(DB9000_DEF_VAL, db9000->regs + j);
		j = j + DB9000_REG_OFFSET;
	}
}

static void db9000_controller_on(struct db9000 *db9000)
{
	unsigned long flags;

	writel_relaxed(DB9000_PCTR_PCR | DB9000_PCTR_PCI, db9000->regs + DB9000_PCTR);

	spin_lock_irqsave(&db9000->lock, flags);
	writel_relaxed(DB9000_ISR_VAL, db9000->regs + DB9000_ISR);
	spin_unlock_irqrestore(&db9000->lock, flags);
}

static irqreturn_t db9000_irq(int irq, void *arg)
{
	struct db9000 *db9000 = arg;
	u32 status;

	status = readl_relaxed(db9000->regs + DB9000_ISR);
	if (!status)
		return IRQ_NONE;

	if (status & DB9000_ISR_VCT) {
		drm_crtc_handle_vblank(&db9000->crtc);
		status &= ~DB9000_ISR_VCT;
		writel_relaxed(DB9000_ISR_VCT, db9000->regs + DB9000_ISR);
	}

	/*
	 * Acknowledge any remaining bits with a single write. The
	 * previous code wrote `status` (which still had VCT set) here
	 * too, racing a fresh vblank that arrived between the two writes
	 * and silently dropping it. Clear VCT from the local copy first
	 * so this second write only ACKs error/status bits.
	 */
	if (status)
		writel_relaxed(status, db9000->regs + DB9000_ISR);

	return IRQ_HANDLED;
}

static int db9000_plane_atomic_check(struct drm_plane *plane,
				     struct drm_atomic_state *state)
{
	struct drm_plane_state *new_plane_state =
		drm_atomic_get_new_plane_state(state, plane);
	struct drm_framebuffer *fb = new_plane_state->fb;
	u32 src_w, src_h;

	if (!fb)
		return 0;

	src_w = new_plane_state->src_w >> 16;
	src_h = new_plane_state->src_h >> 16;

	if (src_w != new_plane_state->crtc_w ||
	    src_h != new_plane_state->crtc_h) {
		drm_dbg_kms(plane->dev, "Scaling is not supported\n");
		return -EINVAL;
	}
	return 0;
}

static void db9000_plane_atomic_disable(struct drm_plane *plane,
					struct drm_atomic_state *oldstate)
{
	struct db9000 *db9000 = plane_to_db9000(plane);
	u32 imr;

	imr = readl_relaxed(db9000->regs + DB9000_IMR);
	writel_relaxed(imr & ~DB9000_IMR_BAUM, db9000->regs + DB9000_IMR);
}

static void db9000_plane_atomic_update(struct drm_plane *plane,
				       struct drm_atomic_state *oldstate)
{
	struct db9000 *db9000 = plane_to_db9000(plane);
	struct drm_plane_state *state =
		drm_atomic_get_new_plane_state(oldstate, plane);
	struct drm_framebuffer *fb = state->fb;
	struct drm_gem_dma_object *gem;
	dma_addr_t paddr;
	u32 dear_offset;
	u32 format;

	if (!state->crtc || !fb)
		return;

	format = fb->format->format;

	/*
	 * Only formats listed in db9000_fmts[] can reach this point —
	 * drm_universal_plane_init() filters everything else. Map the
	 * accepted set to the controller's bpp/pixel-select setup.
	 */
	switch (format) {
	case DRM_FORMAT_RGB565:
	case DRM_FORMAT_BGR565:
		db9000_bpp_setup(db9000, 16, db9000->bus_width, false);
		break;
	case DRM_FORMAT_RGB888:
	case DRM_FORMAT_BGR888:
	case DRM_FORMAT_XRGB8888:
		db9000_bpp_setup(db9000, 32, db9000->bus_width, false);
		break;
	default:
		dev_err(db9000->dev, "unexpected fb format 0x%x\n", format);
		return;
	}

	/*
	 * DEAR is the end address of the scanout window. Use the
	 * framebuffer's pitch * height — fb->pitches[0] reflects whatever
	 * stride the allocator chose (often 64- or 4096-byte aligned),
	 * which can exceed hdisplay * bpp / 8. The previous formula used
	 * frame_size = hdisplay * vdisplay and silently underran the
	 * actual buffer end on stride-aligned allocations.
	 */
	dear_offset = (u32)fb->pitches[0] * fb->height;

	gem = drm_fb_dma_get_gem_obj(fb, 0);
	if (gem) {
		u32 mrr_val;

		paddr = gem->dma_addr + fb->offsets[0];
		writel_relaxed(paddr, db9000->regs + DB9000_DBAR);
		writel_relaxed(paddr + dear_offset, db9000->regs + DB9000_DEAR);

		/*
		 * Enable 4 outstanding AXI reads. DEAR_MRR sits three
		 * bursts before DEAR — when the DMA's current address
		 * crosses DEAR_MRR the controller stops issuing new
		 * read requests, so the three reads still in flight
		 * have room to complete up to DEAR without overrunning
		 * the framebuffer. Writing MRR without DEAR_MRR (the
		 * naive devmem case) leaves the lookahead at 0 and
		 * starves the FIFO immediately → black screen.
		 *
		 * The guard handles the (vanishingly unlikely)
		 * sub-burst-size framebuffer by falling back to a
		 * single-read DMA configuration.
		 */
		if (dear_offset > 3 * db9000->dma_burst_bytes) {
			mrr_val = (paddr + dear_offset -
				   3 * db9000->dma_burst_bytes) &
				  DB9000_MRR_DEAR_MASK;
			mrr_val |= DB9000_MRR_4_READS;
		} else {
			mrr_val = DB9000_MRR_1_READ;
		}
		writel_relaxed(mrr_val, db9000->regs + DB9000_MRR);
	}
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
	.format_mod_supported = db9000_plane_format_mod_supported,
};

/*
 * .enable_vblank / .disable_vblank are invoked by drm_vblank.c with
 * drm_device->vbl_lock held. They must only program the controller's
 * vblank-interrupt mask — drm_crtc_vblank_{on,off} would attempt to
 * re-acquire vbl_lock as a non-recursive spinlock (self-deadlock).
 * The on/off calls live in .atomic_enable / .atomic_disable instead.
 */
static int db9000_crtc_enable_vblank(struct drm_crtc *crtc)
{
	struct db9000 *db9000 = crtc_to_db9000(crtc);
	u32 imr;

	writel_relaxed(DB9000_ISCR_VAL, db9000->regs + DB9000_ISCR);

	/*
	 * Same level-triggered IRQ caveat as bind-time pre-clear: ACK
	 * any latched VCT before unmasking VCTM, so the GIC observes a
	 * clean rising edge on the next vblank rather than a line that
	 * was already high from a vblank that fired during the masked
	 * window. Without this, modeset transitions (atomic_disable →
	 * atomic_enable) leave wait_for_vblanks blocking for 50ms until
	 * timeout.
	 */
	writel_relaxed(DB9000_ISR_VCT, db9000->regs + DB9000_ISR);

	imr = readl_relaxed(db9000->regs + DB9000_IMR);
	imr &= ~DB9000_IMR_VCTM;
	writel_relaxed(imr, db9000->regs + DB9000_IMR);

	return 0;
}

static void db9000_crtc_disable_vblank(struct drm_crtc *crtc)
{
	struct db9000 *db9000 = crtc_to_db9000(crtc);
	u32 imr;

	imr = readl_relaxed(db9000->regs + DB9000_IMR);
	imr |= DB9000_IMR_VCTM;
	writel_relaxed(imr, db9000->regs + DB9000_IMR);
}

static void db9000_crtc_mode_set_nofb(struct drm_crtc *crtc)
{
	struct db9000 *db9000 = crtc_to_db9000(crtc);
	struct drm_display_mode *mode = &crtc->state->adjusted_mode;
	unsigned long target_rate = mode->clock * 1000UL;
	unsigned long actual_rate;
	u32 reg_cr1 = readl_relaxed(db9000->regs + DB9000_CR1);
	u32 vsw, vfp, vbp, lpp;
	u32 hsw, hfp, hbp, ppl;
	int ret;

	/*
	 * Drive clk_pclk at the mode's pixel clock so the HDMI PLL
	 * downstream sees the rate it was programmed for. Previously the
	 * clock stayed at the DT-fixed boot rate regardless of mode, and
	 * any mismatch broke the HDMI PHY PLL lock.
	 */
	ret = clk_set_rate(db9000->lcd_eclk, target_rate);
	if (ret)
		dev_warn(db9000->dev,
			 "clk_set_rate(%lu Hz) failed: %d, keeping %lu Hz\n",
			 target_rate, ret, clk_get_rate(db9000->lcd_eclk));
	actual_rate = clk_get_rate(db9000->lcd_eclk);
	dev_dbg(db9000->dev, "%s: pclk requested %lu Hz, got %lu Hz\n",
		mode->name, target_rate, actual_rate);

	/*
	 * drm_display_mode.flags carries DRM_MODE_FLAG_PHSYNC /
	 * DRM_MODE_FLAG_PVSYNC (active-high syncs). The previous code
	 * tested DISPLAY_FLAGS_HSYNC_HIGH from <video/display_timing.h>,
	 * whose numeric value collides with DRM_MODE_FLAG_NHSYNC — the
	 * polarity logic was inverted for both H and V, producing
	 * active-low sync where active-high was required. 1080p60 carries
	 * PHSYNC|PVSYNC, so this regression made every common mode wrong.
	 */
	if (mode->flags & DRM_MODE_FLAG_PHSYNC)
		reg_cr1 |= DB9000_CR1_HSP;
	else
		reg_cr1 &= ~DB9000_CR1_HSP;

	if (mode->flags & DRM_MODE_FLAG_PVSYNC)
		reg_cr1 |= DB9000_CR1_VSP;
	else
		reg_cr1 &= ~DB9000_CR1_VSP;

	reg_cr1 |= DB9000_CR1_DEP;

	db9000->frame_size = mode->hdisplay * mode->vdisplay;

	/* Translate drm_display_mode timing fields into the DB9000 HTR/VTR layout. */
	hsw = mode->hsync_end - mode->hsync_start;
	hbp = mode->htotal - mode->hsync_end;
	hfp = mode->hsync_start - mode->hdisplay;
	ppl = mode->hdisplay / 16;

	vsw = mode->vsync_end - mode->vsync_start;
	vbp = mode->vtotal - mode->vsync_end;
	vfp = mode->vsync_start - mode->vdisplay;
	lpp = mode->vdisplay;

	writel_relaxed(DB9000_HTR_HSW(hsw) | DB9000_HTR_HBP(hbp) |
		       DB9000_HTR_PPL(ppl) | DB9000_HTR_HFP(hfp),
		       db9000->regs + DB9000_HTR);

	writel_relaxed(DB9000_VTR1_VSW(vsw) | DB9000_VTR1_VBP(vbp) |
		       DB9000_VTR1_VFP(vfp),
		       db9000->regs + DB9000_VTR1);

	writel_relaxed(DB9000_VTR2_LPP(lpp), db9000->regs + DB9000_VTR2);

	/*
	 * FDW=2 selects 16-beat AXI bursts (TRM 8.9). FDW=1 (8-beat)
	 * leaves the output FIFO empty more often when the AXI master
	 * bus is contended — e.g. while fbcon is scrolling rapidly —
	 * which shows up as transient blanking / scanline corruption.
	 * 16-beat is the largest the controller supports.
	 */
	writel_relaxed(reg_cr1 | DB9000_CR1_HSS(1) | DB9000_CR1_FDW(2) |
		       DB9000_CR1_LPE | DB9000_CR1_LCE,
		       db9000->regs + DB9000_CR1);
}

static enum drm_mode_status
db9000_crtc_mode_valid(struct drm_crtc *crtc,
		       const struct drm_display_mode *mode)
{
	/*
	 * The DB9000 timing registers (HTR / VTR1) are 8-bit fields, so
	 * any single h/v parameter wider than 255 needs the HVTER
	 * extension bits — which this driver does not yet program.
	 * Reject those modes outright rather than truncate silently.
	 *
	 * The controller also has no half-line / field-toggle support
	 * for interlaced modes, so reject them too.
	 */
	if (mode->flags & DRM_MODE_FLAG_INTERLACE)
		return MODE_NO_INTERLACE;

	if (mode->hsync_end - mode->hsync_start > 255 ||
	    mode->htotal - mode->hsync_end > 255 ||
	    mode->hsync_start - mode->hdisplay > 255 ||
	    mode->hdisplay / 16 > 255)
		return MODE_H_ILLEGAL;

	if (mode->vsync_end - mode->vsync_start > 255 ||
	    mode->vtotal - mode->vsync_end > 255 ||
	    mode->vsync_start - mode->vdisplay > 255 ||
	    mode->vdisplay > 4095)
		return MODE_V_ILLEGAL;

	return MODE_OK;
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
	struct drm_device *ddev = crtc->dev;

	pm_runtime_get_sync(ddev->dev);
	drm_crtc_vblank_on(crtc);
}

static void db9000_crtc_atomic_disable(struct drm_crtc *crtc,
				       struct drm_atomic_state *state)
{
	struct drm_device *ddev = crtc->dev;

	drm_crtc_vblank_off(crtc);

	/*
	 * Deliver any pending page-flip event synchronously now that the
	 * CRTC is going away — drm_crtc_handle_vblank() will not fire
	 * again to do it for us.
	 */
	spin_lock_irq(&ddev->event_lock);
	if (crtc->state->event) {
		drm_crtc_send_vblank_event(crtc, crtc->state->event);
		crtc->state->event = NULL;
	}
	spin_unlock_irq(&ddev->event_lock);

	pm_runtime_put_sync(ddev->dev);
}

static const struct drm_crtc_helper_funcs db9000_crtc_helper_funcs = {
	.mode_valid = db9000_crtc_mode_valid,
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

static int db9000_crtc_init(struct drm_device *ddev, struct db9000 *db9000)
{
	int ret;

	ret = drm_universal_plane_init(ddev, &db9000->plane, CRTC_MASK,
				       &db9000_plane_funcs, db9000_fmts,
				       ARRAY_SIZE(db9000_fmts),
				       db9000_format_modifiers,
				       DRM_PLANE_TYPE_PRIMARY, NULL);
	if (ret) {
		drm_err(ddev, "Can not initialize primary plane\n");
		return ret;
	}
	drm_plane_helper_add(&db9000->plane, &db9000_plane_helper_funcs);

	ret = drm_crtc_init_with_planes(ddev, &db9000->crtc, &db9000->plane,
					NULL, &db9000_crtc_funcs, NULL);
	if (ret) {
		drm_err(ddev, "Can not initialize CRTC\n");
		drm_plane_cleanup(&db9000->plane);
		return ret;
	}

	drm_crtc_helper_add(&db9000->crtc, &db9000_crtc_helper_funcs);
	drm_mode_crtc_set_gamma_size(&db9000->crtc, CLUT_SIZE);
	drm_crtc_enable_color_mgmt(&db9000->crtc, 0, false, CLUT_SIZE);

	return 0;
}

static int db9000_regs_show(struct seq_file *s, void *data)
{
	struct db9000 *db9000 = s->private;
	u32 cr1, isr, imr;

	if (!db9000)
		return -EINVAL;

	cr1 = readl_relaxed(db9000->regs + DB9000_CR1);
	isr = readl_relaxed(db9000->regs + DB9000_ISR);
	imr = readl_relaxed(db9000->regs + DB9000_IMR);

	seq_printf(s, "CR1:   0x%08x  (LCE=%d LPE=%d HSP=%d VSP=%d DEP=%d)\n",
		   cr1,
		   !!(cr1 & DB9000_CR1_LCE),
		   !!(cr1 & DB9000_CR1_LPE),
		   !!(cr1 & DB9000_CR1_HSP),
		   !!(cr1 & DB9000_CR1_VSP),
		   !!(cr1 & DB9000_CR1_DEP));
	seq_printf(s, "HTR:   0x%08x\n", readl_relaxed(db9000->regs + DB9000_HTR));
	seq_printf(s, "VTR1:  0x%08x\n", readl_relaxed(db9000->regs + DB9000_VTR1));
	seq_printf(s, "VTR2:  0x%08x\n", readl_relaxed(db9000->regs + DB9000_VTR2));
	seq_printf(s, "PCTR:  0x%08x\n", readl_relaxed(db9000->regs + DB9000_PCTR));
	seq_printf(s, "ISR:   0x%08x  (VCT bit %s)\n",
		   isr, (isr & DB9000_ISR_VCT) ? "PENDING" : "clear");
	seq_printf(s, "IMR:   0x%08x  (vblank %s)\n",
		   imr,
		   (imr & DB9000_IMR_VCTM) ? "MASKED" : "unmasked");
	seq_printf(s, "ISCR:  0x%08x\n", readl_relaxed(db9000->regs + DB9000_ISCR));
	seq_printf(s, "DBAR:  0x%08x\n", readl_relaxed(db9000->regs + DB9000_DBAR));
	seq_printf(s, "DEAR:  0x%08x\n", readl_relaxed(db9000->regs + DB9000_DEAR));
	seq_printf(s, "DCAR:  0x%08x  (scan position)\n",
		   readl_relaxed(db9000->regs + DB9000_DCAR));
	{
		static const char * const mrr_outs[] = {
			"1", "2", "4", "RSVD"
		};
		u32 mrr = readl_relaxed(db9000->regs + DB9000_MRR);

		seq_printf(s,
			   "MRR:   0x%08x  (DEAR_MRR=0x%08x outstanding=%s burst=%u B)\n",
			   mrr, mrr & DB9000_MRR_DEAR_MASK,
			   mrr_outs[mrr & DB9000_MRR_MRR_MASK],
			   db9000->dma_burst_bytes);
	}

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(db9000_regs);

static void ax_db9000_debugfs_exit(struct db9000 *db9000)
{
	debugfs_remove_recursive(db9000->debugfs);
	db9000->debugfs = NULL;
}

static void ax_db9000_debugfs_init(struct db9000 *db9000)
{
	db9000->debugfs = debugfs_create_dir("axiado-lcdc", NULL);
	debugfs_create_file("db9000_regs", 0444, db9000->debugfs, db9000,
			    &db9000_regs_fops);
}

static int axiado_db9000_bind(struct device *dev, struct device *master,
			      void *data)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct drm_device *ddev = data;
	struct db9000 *db9000;
	struct device_node *np = dev->of_node;
	struct device_node *port;

	u32 bpp;
	u32 bus_width;
	int ret, irq;

	db9000 = devm_kzalloc(dev, sizeof(*db9000), GFP_KERNEL);
	if (!db9000)
		return -ENOMEM;

	db9000->dev = dev;

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

	spin_lock_init(&db9000->lock);

	db9000->regs = devm_platform_get_and_ioremap_resource(pdev, 0, NULL);
	if (IS_ERR(db9000->regs))
		return PTR_ERR(db9000->regs);

	db9000->lcd_eclk = devm_clk_get_enabled(dev, "clk_pclk");
	if (IS_ERR(db9000->lcd_eclk)) {
		dev_err(dev, "Unable to get and enable pixel clock: %ld\n",
			PTR_ERR(db9000->lcd_eclk));
		return PTR_ERR(db9000->lcd_eclk);
	}

	db9000_reset(db9000);
	db9000_controller_on(db9000);
	db9000_bpp_setup(db9000, db9000->bpp, db9000->bus_width, false);

	/*
	 * Derive the AXI burst size from CIR1's bus-width field. CIR1
	 * only exists on cores at CIR_REV ≥ 1.16 — older revisions
	 * expose GPIOR at the same offset, so guard the read on the
	 * DB9000 model-number signature (0x90 in CIR_MN) and otherwise
	 * fall back to 64-bit (the AX3000 AXI width). FDW=2 = 16 beats.
	 */
	{
		u32 cir1 = readl_relaxed(db9000->regs + DB9000_CIR1);
		u32 mn = (cir1 & DB9000_CIR1_MN_MASK) >> DB9000_CIR1_MN_SHIFT;
		u32 bw = (cir1 & DB9000_CIR1_BW_MASK) >> DB9000_CIR1_BW_SHIFT;
		u32 bytes_per_beat = 8;

		if (mn == DB9000_CIR1_MN_DB9000) {
			switch (bw) {
			case DB9000_CIR1_BW_32:
				bytes_per_beat = 4;
				break;
			case DB9000_CIR1_BW_64:
				bytes_per_beat = 8;
				break;
			case DB9000_CIR1_BW_128:
				bytes_per_beat = 16;
				break;
			case DB9000_CIR1_BW_256:
				bytes_per_beat = 32;
				break;
			}
		}
		db9000->dma_burst_bytes = bytes_per_beat * 16;
		dev_dbg(dev, "CIR1=0x%08x master-bus=%u bits burst=%u bytes\n",
			cir1, bytes_per_beat * 8, db9000->dma_burst_bytes);
	}

	/*
	 * Quiesce the controller's IRQ state before Linux owns the line.
	 *
	 * The DB9000 IRQ is a "write 1 to clear" level-triggered SPI on
	 * the GIC. If the BMC was already driving scanout before Linux
	 * came up (e.g. the user ran `hdmi --bist` from syscli), ISR can
	 * have VCT (and other bits) already latched and the IRQ line
	 * left high. When devm_request_irq() → enable_irq() then runs,
	 * the GIC will not redeliver until it sees the line drop and
	 * rise again — and the controller will not drop the line until
	 * software ACKs the pending bits — but the kernel never gets the
	 * IRQ to do that ACK from. Deadlock at the GIC.
	 *
	 * Break the cycle: mask everything, then write 1s into ISR to
	 * clear any latched bits, then unmask. By the time the IRQ
	 * handler is wired up the line is guaranteed to be low.
	 */
	writel_relaxed(0xffffffff, db9000->regs + DB9000_IMR);
	writel_relaxed(0xffffffff, db9000->regs + DB9000_ISR);
	writel_relaxed(DB9000_IMR_VAL2, db9000->regs + DB9000_IMR);

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		ret = irq;
		goto err;
	}

	ret = db9000_crtc_init(ddev, db9000);
	if (ret) {
		drm_err(ddev, "Failed to init crtc\n");
		goto err;
	}

	port = of_get_child_by_name(dev->of_node, "port");
	if (!port) {
		drm_err(ddev, "no port node found in %pOF\n", dev->of_node);
		ret = -ENOENT;
		goto err;
	}
	db9000->crtc.port = port;

	/*
	 * drm_vblank_init() must precede devm_request_irq(): the handler
	 * calls drm_crtc_handle_vblank(), which dereferences the
	 * dev->vblank[] entry that drm_vblank_init populates. The earlier
	 * pre-clear of ISR/IMR makes a pre-existing interrupt edge unlikely
	 * to fire the moment request_irq enables the GIC line, but the
	 * ordering contract still has to hold.
	 */
	ret = drm_vblank_init(ddev, NB_CRTC);
	if (ret) {
		drm_err(ddev, "Failed calling drm_vblank_init()\n");
		goto err;
	}

	ret = devm_request_irq(dev, irq, db9000_irq, 0, dev_name(dev), db9000);
	if (ret) {
		drm_err(ddev, "Failed to register DB9000 interrupt\n");
		goto err;
	}

	dev_set_drvdata(dev, db9000);

	if (IS_ENABLED(CONFIG_DEBUG_FS))
		ax_db9000_debugfs_init(db9000);

	return 0;

err:
	return ret;
}

static void axiado_db9000_unbind(struct device *dev, struct device *master,
				 void *data)
{
	struct db9000 *db9000 = dev_get_drvdata(dev);

	if (!db9000)
		return;

	if (IS_ENABLED(CONFIG_DEBUG_FS))
		ax_db9000_debugfs_exit(db9000);

	/*
	 * lcd_eclk was obtained via devm_clk_get_enabled() and is
	 * automatically disabled+unprepared by devm cleanup after this
	 * callback returns — no explicit clk_disable_unprepare() here.
	 */
}

static const struct component_ops axiado_db9000_ops = {
	.bind = axiado_db9000_bind,
	.unbind = axiado_db9000_unbind,
};

static int db9000_drm_platform_probe(struct platform_device *pdev)
{
	return component_add(&pdev->dev, &axiado_db9000_ops);
}

static void db9000_drm_platform_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &axiado_db9000_ops);
}

static void db9000_drm_platform_shutdown(struct platform_device *pdev)
{
	struct db9000 *db9000 = dev_get_drvdata(&pdev->dev);

	if (!db9000)
		return;

	writel_relaxed(DB9000_IMR_VAL2, db9000->regs + DB9000_IMR);
	writel_relaxed(readl_relaxed(db9000->regs + DB9000_ISR),
		       db9000->regs + DB9000_ISR);

	/*
	 * Clock is managed by devm_clk_get_enabled() — devm will tear it
	 * down on driver detach. The shutdown path just quiesces the
	 * controller IRQs.
	 */
}

static const struct of_device_id db9000_dt_ids[] = {
	{ .compatible = "axiado,drm-db9000" },
	{ /* sentinel */ }
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

MODULE_AUTHOR("Axiado Corporation");
MODULE_DESCRIPTION("Axiado LCD Controller Driver");
MODULE_LICENSE("GPL");
