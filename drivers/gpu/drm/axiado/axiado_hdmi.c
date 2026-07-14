// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022-2026 Axiado Corporation.
 *
 * Based on inno_hdmi.c
 * Copyright (c) Fuzhou Rockchip Electronics Co. Ltd.
 */

#include <linux/bitfield.h>
#include <linux/clk.h>
#include <linux/component.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/gpio/consumer.h>
#include <linux/hdmi.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>

#include <drm/drm_atomic_helper.h>
#include <drm/drm_edid.h>
#include <drm/drm_of.h>
#include <drm/drm_print.h>
#include <drm/drm_probe_helper.h>

#include "axiado_hdmi.h"

#define encoder_to_axiado_hdmi(e) container_of(e, struct axiado_hdmi, encoder)
#define connector_to_axiado_hdmi(c) \
	container_of(c, struct axiado_hdmi, connector)

/*
 * Per-mode PLL programming overrides.
 *
 * The chip's reset defaults already produce the 148.5 MHz TMDS clock
 * for 1080p60 against a 74.25 MHz clk_pclk reference, so the BMC's
 * `hdmi --bist` only powers the PLLs up without rewriting the
 * dividers. For other resolutions we need to override the dividers
 * explicitly. Add new entries here as they are verified against a
 * matching BMC BIST dump.
 *
 * Field meanings (each register lives at byte offset = spec_off * 4):
 *   pll1  (0x1a1): pre-PLL pre_div
 *   pll2  (0x1a2): SSC mode / fractional / fb_div[11:8]
 *   pll3  (0x1a3): pre-PLL fb_div[7:0]
 *   pll4  (0x1a4): linktmds / link / tmds dividers
 *   pll5  (0x1a5): pre-PLL main / aux dividers
 *   pll6  (0x1a6): pre-PLL repeat / pixel dividers
 *   pll10 (0x1aa): post-PLL config — bit[0] is power-down, the rest
 *                  is fb_div[8] / post-div_en / ref_clk select
 *   pll11 (0x1ab): post-PLL pre_div
 *   pll12 (0x1ac): post-PLL fb_div[7:0]
 *   pll14 (0x1ad): post-PLL output divider / source select
 */
struct axiado_hdmi_pll_config {
	unsigned long pclk;
	u8 pll1, pll2, pll3, pll4, pll5, pll6;
	u8 pll10, pll11, pll12, pll14;
};

static const struct axiado_hdmi_pll_config axiado_pll_configs[] = {
	{
		.pclk  = 148500000, /* 1080p60 — verified against BMC BIST dump */
		.pll1  = 0x01, .pll2  = 0xf0, .pll3  = 0x63,
		.pll4  = 0x15, .pll5  = 0x41, .pll6  = 0x42,
		.pll10 = 0x0e, .pll11 = 0x01, .pll12 = 0x14, .pll14 = 0x09,
	},
	/*
	 * Add other resolutions here as their PLL settings are verified
	 * against a working BMC BIST capture at that rate.
	 */
};

static const struct axiado_hdmi_pll_config *
axiado_find_pll_config(unsigned long pclk)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(axiado_pll_configs); i++)
		if (axiado_pll_configs[i].pclk == pclk)
			return &axiado_pll_configs[i];
	return NULL;
}

static inline void ax_hdmi_write8(struct axiado_hdmi *hdmi, u32 offset, u8 val)
{
	if (hdmi->data_reg)
		writeb(val, hdmi->data_reg + offset);
}

static inline void hdmi_modl(struct axiado_hdmi *hdmi, u32 offset, u32 msk,
			     u32 val)
{
	u32 temp = readl_relaxed(hdmi->regs + offset);

	temp = (temp & ~msk) | (val & msk);
	writel_relaxed(temp, hdmi->regs + offset);
}

static void ax_hdmi_write_edid(struct axiado_hdmi *hdmi,
			       const struct edid *edid_data)
{
	int i;
	const u8 *data;
	size_t edid_length;

	if (!edid_data || !hdmi->data_reg)
		return;

	if (!of_property_read_bool(hdmi->dev->of_node,
				   "axiado,hypervisor-edid-passthrough"))
		return;

	edid_length = (edid_data->extensions + 1) * EDID_LENGTH;
	data = (const u8 *)edid_data;

	for (i = AX_HDMI_EDID_REG_OFFSET; i < edid_length; i++)
		ax_hdmi_write8(hdmi, i, data[i]);
}

/*
 * Poll for PLL lock. The lock-status registers are RO bit[0]; per spec
 * the PLLs lock in ~1ms but we give some slack. Returns 0 on lock.
 */
static int axiado_hdmi_wait_pll_lock(struct axiado_hdmi *hdmi, u32 reg,
				     const char *tag)
{
	unsigned int i;
	u32 val;

	for (i = 0; i < 200; i++) {
		val = readl_relaxed(hdmi->regs + reg);
		if (val & m_PLL_LOCK)
			return 0;
		usleep_range(50, 100);
	}
	dev_warn(hdmi->dev, "%s did not lock within 10ms (reg=0x%03x val=0x%02x)\n",
		 tag, reg, val);
	return -ETIMEDOUT;
}

/*
 * axiado_pll_config - bring the HDMI PLLs up for the requested mode.
 *
 * If axiado_pll_configs[] has a matching entry for the pixel clock
 * we power the PLLs down, write the per-mode dividers, then power
 * them back up. If no entry exists we leave the dividers at their
 * reset defaults and just power up — this mirrors the BMC's
 * `hdmi --bist` sequence and works for 1080p60 against a 74.25 MHz
 * clk_pclk reference.
 */
static void axiado_pll_config(struct axiado_hdmi *hdmi,
			      struct drm_display_mode *mode)
{
	const struct axiado_hdmi_pll_config *cfg;

	cfg = axiado_find_pll_config((unsigned long)mode->clock * 1000);
	if (cfg) {
		/* Power down both PLLs while reprogramming. */
		writel_relaxed(0x01, hdmi->regs + HDMI_PLL0);
		writel_relaxed(cfg->pll10 | 0x01, hdmi->regs + HDMI_PLL10);

		/* Pre-PLL divider chain (0x1a1..0x1a6). */
		writel_relaxed(cfg->pll1, hdmi->regs + HDMI_PLL1);
		writel_relaxed(cfg->pll2, hdmi->regs + HDMI_PLL2);
		writel_relaxed(cfg->pll3, hdmi->regs + HDMI_PLL3);
		writel_relaxed(cfg->pll4, hdmi->regs + HDMI_PLL4);
		writel_relaxed(cfg->pll5, hdmi->regs + HDMI_PLL5);
		writel_relaxed(cfg->pll6, hdmi->regs + HDMI_PLL6);

		/* Post-PLL divider chain (0x1ab..0x1ad). */
		writel_relaxed(cfg->pll11, hdmi->regs + HDMI_PLL11);
		writel_relaxed(cfg->pll12, hdmi->regs + HDMI_PLL12);
		writel_relaxed(cfg->pll14, hdmi->regs + HDMI_PLL14);

		/* Power up pre-PLL, then post-PLL. */
		writel_relaxed(0x00, hdmi->regs + HDMI_PLL0);
		writel_relaxed(cfg->pll10, hdmi->regs + HDMI_PLL10);
	} else {
		dev_dbg(hdmi->dev,
			"no PLL override for pclk=%u kHz; using chip defaults\n",
			mode->clock);
		writel_relaxed(0x00, hdmi->regs + HDMI_PLL0);
		writel_relaxed(0x0e, hdmi->regs + HDMI_PLL10);
	}

	axiado_hdmi_wait_pll_lock(hdmi, HDMI_PRE_LOCK, "pre-PLL");
	axiado_hdmi_wait_pll_lock(hdmi, HDMI_POST_LOCK, "post-PLL");

	/*
	 * Enable the per-data-channel LDOs (0x1b4[2:0]) and serializers
	 * (0x1be[6:4]). The BMC sequence writes full bytes here rather
	 * than RMW masking — the chip only latches the enable bits on a
	 * full-byte write.
	 */
	writel_relaxed(0x07, hdmi->regs + HDMI_LDO);
	writel_relaxed(0x71, hdmi->regs + HDMI_SERIAL);
}

static void axiado_hdmi_tmds_sync(struct axiado_hdmi *hdmi)
{
	/* Step 17: sync enable — write 0, wait, write 1 to 0x0ce[0]. */
	writel_relaxed(0x00, hdmi->regs + HDMI_TMDS_SYNC);
	usleep_range(10, 20);
	writel_relaxed(0x01, hdmi->regs + HDMI_TMDS_SYNC);

	/*
	 * Step 18: turn on the four TMDS drivers (clock + data 0/1/2)
	 * via 0x1b2[3:0] = 4'b1111. Use a read-modify-write so we keep
	 * bit [7] (clock channel control — "select TMDS clock output",
	 * reset value 1). Writing the byte as 0x0f drops bit [7] and the
	 * clock pad goes dark while the data pads stay up, which silently
	 * defeats sink detection.
	 */
	hdmi_modl(hdmi, HDMI_TMDS_SYS_CTL, 0x0f, 0x0f);
}

static const struct {
	const char *name;
	u32 off;
} axiado_hdmi_dump_table[] = {
	{ "BIAS_CIRCUIT",     HDMI_BIAS_CIRCUIT },
	{ "RX_SENSE",         HDMI_RX_SENSE },
	{ "RX_SENSE_STATUS",  HDMI_RX_SENSE_STATUS },
	{ "PLL0(pre_pd)",     HDMI_PLL0 },
	{ "PLL1",             HDMI_PLL1 },
	{ "PLL2",             HDMI_PLL2 },
	{ "PLL3",             HDMI_PLL3 },
	{ "PLL4",             HDMI_PLL4 },
	{ "PLL5",             HDMI_PLL5 },
	{ "PLL6",             HDMI_PLL6 },
	{ "PRE_LOCK",         HDMI_PRE_LOCK },
	{ "PLL10(post_pd)",   HDMI_PLL10 },
	{ "PLL11(post_pre)",  HDMI_PLL11 },
	{ "PLL12(post_fb)",   HDMI_PLL12 },
	{ "PLL14(post_out)",  HDMI_PLL14 },
	{ "POST_LOCK",        HDMI_POST_LOCK },
	{ "LDO",              HDMI_LDO },
	{ "SERIAL",           HDMI_SERIAL },
	{ "VBIST",            HDMI_VBIST },
	{ "SYS_CTRL",         HDMI_SYS_CTRL },
	{ "TMDS_SYNC",        HDMI_TMDS_SYNC },
	{ "TMDS_SYS_CTL",     HDMI_TMDS_SYS_CTL },
	{ "STATUS",           HDMI_STATUS },
};

static int hdmi_regs_show(struct seq_file *s, void *data)
{
	struct axiado_hdmi *hdmi = s->private;
	int i;

	if (!hdmi) {
		seq_puts(s, "HDMI not bound\n");
		return 0;
	}
	for (i = 0; i < ARRAY_SIZE(axiado_hdmi_dump_table); i++)
		seq_printf(s, "%-22s @0x%03x = 0x%02x\n",
			   axiado_hdmi_dump_table[i].name,
			   axiado_hdmi_dump_table[i].off,
			   readl_relaxed(hdmi->regs + axiado_hdmi_dump_table[i].off));
	return 0;
}

DEFINE_SHOW_ATTRIBUTE(hdmi_regs);

static void axiado_hdmi_i2c_init(struct axiado_hdmi *hdmi)
{
	int ddc_bus_freq;

	ddc_bus_freq = (hdmi->tmds_rate >> 2) / HDMI_SCL_RATE;
	writel_relaxed(ddc_bus_freq & 0xFF, hdmi->regs + DDC_BUS_FREQ_L);
	writel_relaxed((ddc_bus_freq >> 8) & 0xFF, hdmi->regs + DDC_BUS_FREQ_H);
	/* Clear the EDID interrupt flag and mute the interrupt */
	writel_relaxed(0, hdmi->regs + HDMI_INTERRUPT_MASK1);
	writel_relaxed(m_INT_EDID_READY, hdmi->regs + HDMI_INTERRUPT_STATUS1);
}

static void axiado_hdmi_sys_power(struct axiado_hdmi *hdmi, bool enable)
{
	if (enable)
		hdmi_modl(hdmi, HDMI_SYS_CTRL, m_POWER, v_PWR_ON);
	else
		hdmi_modl(hdmi, HDMI_SYS_CTRL, m_POWER, v_PWR_OFF);
}

static void axiado_hdmi_set_pwr_mode(struct axiado_hdmi *hdmi,
				     enum axiado_hdmi_pwr_mode mode)
{
	switch (mode) {
	case AXIADO_HDMI_PWR_NORMAL:
		axiado_hdmi_sys_power(hdmi, false);
		axiado_hdmi_sys_power(hdmi, true);
		break;
	case AXIADO_HDMI_PWR_STANDBY:
		axiado_hdmi_sys_power(hdmi, false);
		break;
	default:
		dev_err(hdmi->dev, "unknown power mode %d\n", mode);
	}
}

static void axiado_hdmi_bias_on(struct axiado_hdmi *hdmi)
{
	unsigned int i;
	u32 status;

	/*
	 * Step 5: Turn on bias circuit. Spec 0x1b0[2] must be set to 1.
	 * The previous code passed HDMI_BIAS_DEF (0x08, bit[3]) as the
	 * val argument with a (1<<2) mask — the mask zeroed the value,
	 * so the bias enable bit was being *cleared* on every modeset.
	 * A working BMC BIST run reads back 0x04 here (only bit[2] set);
	 * match that exactly.
	 */
	writel_relaxed(m_BIAS_CONTROL, hdmi->regs + HDMI_BIAS_CIRCUIT);

	/* Step 6: Turn on rxsense detection circuit (0x1cc[3:0] = 0xF). */
	writel_relaxed(m_HDMI_RX_SENSE, hdmi->regs + HDMI_RX_SENSE);

	/*
	 * Step 7: wait for rxsense detection result. Bit[3:0] of 0x1cd
	 * (HDMI_RX_SENSE_STATUS) goes high once the sink is detected. If
	 * the cable is unplugged this never asserts, so just log and move
	 * on rather than blocking forever.
	 */
	for (i = 0; i < 200; i++) {
		status = readl_relaxed(hdmi->regs + HDMI_RX_SENSE_STATUS);
		if ((status & m_HDMI_RX_SENSE) == m_HDMI_RX_SENSE)
			break;
		usleep_range(50, 100);
	}
}

/*
 * axiado_hdmi_reset - put the HDMI controller into a known state.
 *
 * Mirrors the minimal "reset / un-reset" toggle that the BMC's BIST
 * sequence and the upstream Rockchip inno_hdmi driver both rely on.
 * The earlier version of this function wrote ~30 configuration
 * registers while the analog and digital reset bits were still
 * asserted, which left the controller in an inconsistent state that
 * the rest of bring-up could not recover from — the symptom was that
 * neither external LCDC video nor the internal BIST color bar would
 * reach the sink until SYS_CTRL was manually toggled via devmem.
 *
 * Just write 0x03 (assert both resets, power off) followed by 0x61
 * (deassert both resets, normal power, INT polarity high) — exactly
 * the recovery sequence the user runs by hand.
 */
static void axiado_hdmi_reset(struct axiado_hdmi *hdmi)
{
	writel_relaxed(0x03, hdmi->regs + HDMI_SYS_CTRL);
	usleep_range(100, 200);
	writel_relaxed(HDMI_SYS_PWR_LOW, hdmi->regs + HDMI_SYS_CTRL);
	usleep_range(100, 200);
}

static int axiado_hdmi_upload_frame(struct axiado_hdmi *hdmi, int setup_rc,
				    union hdmi_infoframe *frame,
				    u32 frame_index, u32 mask, u32 disable,
				    u32 enable)
{
	writel_relaxed(frame_index, hdmi->regs + HDMI_CONTROL_PACKET_BUF_INDEX);

	if (setup_rc >= 0) {
		u8 packed_frame[HDMI_MAXIMUM_INFO_FRAME_SIZE];
		ssize_t rc, i;

		rc = hdmi_infoframe_pack(frame, packed_frame,
					 sizeof(packed_frame));
		if (rc < 0)
			return rc;

		for (i = 0; i < rc; i++)
			writel_relaxed(packed_frame[i], hdmi->regs + HDMI_CONTROL_PACKET_ADDR + (i * 4));
	}
	return setup_rc;
}

static int axiado_hdmi_config_video_avi(struct axiado_hdmi *hdmi,
					struct drm_display_mode *mode)
{
	union hdmi_infoframe frame;
	int rc;

	rc = drm_hdmi_avi_infoframe_from_display_mode(&frame.avi, NULL, mode);
	if (hdmi->hdmi_data.enc_out_format == HDMI_COLORSPACE_YUV444)
		frame.avi.colorspace = HDMI_COLORSPACE_YUV444;
	else if (hdmi->hdmi_data.enc_out_format == HDMI_COLORSPACE_YUV422)
		frame.avi.colorspace = HDMI_COLORSPACE_YUV422;
	else
		frame.avi.colorspace = HDMI_COLORSPACE_RGB;

	return axiado_hdmi_upload_frame(hdmi, rc, &frame, INFOFRAME_AVI, 0, 0,
					0);
}

static void axiado_hdmi_video_config(struct axiado_hdmi *hdmi,
				     struct drm_display_mode *mode)
{
	u32 val;

	/* 1. Clear AV Mute (un-black the screen) */
	writel_relaxed(v_AVMUTE_CLEAR(1) | v_AVMUTE_ENABLE(0) | v_AUDIO_MUTE(0) |
		       v_VIDEO_MUTE(0), hdmi->regs + HDMI_AV_MUTE);

	/* 2. Configure Timing Control to use External Video (from LCDC) and match polarity */
	val = v_EXTERANL_VIDEO(1);

	if (mode->flags & DRM_MODE_FLAG_PHSYNC)
		val |= v_HSYNC_POLARITY(1);
	else
		val |= v_HSYNC_POLARITY(0);

	if (mode->flags & DRM_MODE_FLAG_PVSYNC)
		val |= v_VSYNC_POLARITY(1);
	else
		val |= v_VSYNC_POLARITY(0);

	if (mode->flags & DRM_MODE_FLAG_INTERLACE)
		val |= v_INETLACE(1);

	writel_relaxed(val, hdmi->regs + HDMI_VIDEO_TIMING_CTL);

	/*
	 * Program the external video timing reference registers so the
	 * controller's encoder/AVI infoframe path knows what timing the
	 * LCDC is feeding it. With these at the reset default of 0 the
	 * sink shows the wrong picture size even though TMDS is up.
	 *
	 * Each register is one byte of an h/v timing field, written to the
	 * LSB of a 4-byte slot. Field semantics match the upstream
	 * inno_hdmi driver.
	 */
	val = mode->htotal;
	writel_relaxed(val & 0xff, hdmi->regs + HDMI_VIDEO_EXT_HTOTAL_L);
	writel_relaxed((val >> 8) & 0xff, hdmi->regs + HDMI_VIDEO_EXT_HTOTAL_H);

	val = mode->htotal - mode->hdisplay;
	writel_relaxed(val & 0xff, hdmi->regs + HDMI_VIDEO_EXT_HBLANK_L);
	writel_relaxed((val >> 8) & 0xff, hdmi->regs + HDMI_VIDEO_EXT_HBLANK_H);

	val = mode->hsync_start - mode->hdisplay;
	writel_relaxed(val & 0xff, hdmi->regs + HDMI_VIDEO_EXT_HDELAY_L);
	writel_relaxed((val >> 8) & 0xff, hdmi->regs + HDMI_VIDEO_EXT_HDELAY_H);

	val = mode->hsync_end - mode->hsync_start;
	writel_relaxed(val & 0xff, hdmi->regs + HDMI_VIDEO_EXT_HDURATION_L);
	writel_relaxed((val >> 8) & 0xff, hdmi->regs + HDMI_VIDEO_EXT_HDURATION_H);

	val = mode->vtotal;
	writel_relaxed(val & 0xff, hdmi->regs + HDMI_VIDEO_EXT_VTOTAL_L);
	writel_relaxed((val >> 8) & 0xff, hdmi->regs + HDMI_VIDEO_EXT_VTOTAL_H);

	val = mode->vtotal - mode->vdisplay;
	writel_relaxed(val & 0xff, hdmi->regs + HDMI_VIDEO_EXT_VBLANK);

	val = mode->vsync_start - mode->vdisplay;
	writel_relaxed(val & 0xff, hdmi->regs + HDMI_VIDEO_EXT_VDELAY);

	val = mode->vsync_end - mode->vsync_start;
	writel_relaxed(val & 0xff, hdmi->regs + HDMI_VIDEO_EXT_VDURATION);

	/* 3. Configure Input Format mapping (RGB 24-bit, External DE) */
	writel_relaxed(v_VIDEO_INPUT_FORMAT(0) | v_DE_EXTERNAL, hdmi->regs + HDMI_VIDEO_CONTRL1);
	writel_relaxed(HDMI_CTRL2_VAL, hdmi->regs + HDMI_VIDEO_CONTRL2);
	writel_relaxed(v_COLOR_DEPTH_NOT_INDICATED(0) | v_SOF_DISABLE |
		       v_COLOR_RANGE_FULL | v_CSC_DISABLE, hdmi->regs + HDMI_VIDEO_CONTRL3);
}

static int axiado_hdmi_setup(struct axiado_hdmi *hdmi,
			     struct drm_display_mode *mode)
{
	hdmi->hdmi_data.vic = drm_match_cea_mode(mode);
	hdmi->hdmi_data.enc_in_format = HDMI_COLORSPACE_RGB;
	hdmi->hdmi_data.enc_out_format = HDMI_COLORSPACE_RGB;

	axiado_hdmi_bias_on(hdmi);
	axiado_pll_config(hdmi, mode);

	if (hdmi->hdmi_data.vic == VIC_FMT_720x480i4_3 ||
	    hdmi->hdmi_data.vic == VIC_FMT_720x480i16_9 ||
	    hdmi->hdmi_data.vic == VIC_FMT_720x576i4_3 ||
	    hdmi->hdmi_data.vic == VIC_FMT_720x576i16_9 ||
	    hdmi->hdmi_data.vic == VIC_FMT_720x480p4_3 ||
	    hdmi->hdmi_data.vic == VIC_FMT_720x480p16_9 ||
	    hdmi->hdmi_data.vic == VIC_FMT_720x576p4_3 ||
	    hdmi->hdmi_data.vic == VIC_FMT_720x576p16_9)
		hdmi->hdmi_data.colorimetry = HDMI_COLORIMETRY_ITU_601;
	else
		hdmi->hdmi_data.colorimetry = HDMI_COLORIMETRY_ITU_709;

	if (hdmi->hdmi_data.sink_is_hdmi)
		axiado_hdmi_config_video_avi(hdmi, mode);

	hdmi->tmds_rate = mode->clock * 1000;

	axiado_hdmi_i2c_init(hdmi);
	axiado_hdmi_video_config(hdmi, mode);

	/*
	 * SYS_CTRL power-cycle, matching the BMC BIST sequence: write
	 * 0x63 (power off) and then 0x61 (normal power). This is what
	 * actually arms the digital block once all the PHY and video
	 * config registers are programmed.
	 */
	writel_relaxed(HDMI_SYS_PWR_ON, hdmi->regs + HDMI_SYS_CTRL);
	writel_relaxed(HDMI_SYS_PWR_LOW, hdmi->regs + HDMI_SYS_CTRL);

	axiado_hdmi_tmds_sync(hdmi);

	return 0;
}

/* Encoder and connector API implementations */
static void axiado_hdmi_encoder_enable(struct drm_encoder *encoder)
{
	struct axiado_hdmi *hdmi = encoder_to_axiado_hdmi(encoder);

	axiado_hdmi_set_pwr_mode(hdmi, AXIADO_HDMI_PWR_NORMAL);
}

static void axiado_hdmi_encoder_disable(struct drm_encoder *encoder)
{
	struct axiado_hdmi *hdmi = encoder_to_axiado_hdmi(encoder);

	axiado_hdmi_set_pwr_mode(hdmi, AXIADO_HDMI_PWR_STANDBY);
}

static void axiado_hdmi_encoder_mode_set(struct drm_encoder *encoder,
					 struct drm_crtc_state *crtc_state,
					 struct drm_connector_state *conn_state)
{
	struct axiado_hdmi *hdmi = encoder_to_axiado_hdmi(encoder);

	axiado_hdmi_setup(hdmi, &crtc_state->adjusted_mode);
	drm_mode_copy(&hdmi->previous_mode, &crtc_state->adjusted_mode);
}

static const struct drm_encoder_funcs axiado_hdmi_encoder_funcs = {
	.destroy = drm_encoder_cleanup,
};

static const struct drm_encoder_helper_funcs axiado_hdmi_encoder_helper_funcs = {
	.enable = axiado_hdmi_encoder_enable,
	.disable = axiado_hdmi_encoder_disable,
	.atomic_mode_set = axiado_hdmi_encoder_mode_set,
};

static enum drm_connector_status
axiado_hdmi_connector_detect(struct drm_connector *connector, bool force)
{
	struct axiado_hdmi *hdmi = connector_to_axiado_hdmi(connector);
	u32 status = readl_relaxed(hdmi->regs + HDMI_STATUS);

	return (status & m_HOTPLUG) ? connector_status_connected :
				      connector_status_disconnected;
}

static int axiado_hdmi_connector_get_modes(struct drm_connector *connector)
{
	struct axiado_hdmi *hdmi = connector_to_axiado_hdmi(connector);
	const struct drm_edid *drm_edid;
	int ret;

	drm_edid = drm_edid_read(connector);
	drm_edid_connector_update(connector, drm_edid);
	if (!drm_edid)
		return 0;

	/*
	 * connector->display_info.is_hdmi is populated by
	 * drm_edid_connector_update(); prefer it to the deprecated
	 * drm_detect_hdmi_monitor(edid) helper.
	 */
	hdmi->hdmi_data.sink_is_hdmi = connector->display_info.is_hdmi;
	ret = drm_edid_connector_add_modes(connector);
	ax_hdmi_write_edid(hdmi, drm_edid_raw(drm_edid));
	drm_edid_free(drm_edid);

	return ret;
}

static const struct drm_connector_funcs axiado_hdmi_connector_funcs = {
	.fill_modes = drm_helper_probe_single_connector_modes,
	.detect = axiado_hdmi_connector_detect,
	.destroy = drm_connector_cleanup,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

static const struct drm_connector_helper_funcs
	axiado_hdmi_connector_helper_funcs = {
		.get_modes = axiado_hdmi_connector_get_modes,
	};

/* IRQ & I2C Handlers */
static irqreturn_t axiado_hdmi_i2c_irq(struct axiado_hdmi *hdmi)
{
	struct axiado_hdmi_i2c *i2c = hdmi->i2c;
	u8 stat;

	stat = readl_relaxed(hdmi->regs + HDMI_INTERRUPT_STATUS1);
	if (!(stat & m_INT_EDID_READY))
		return IRQ_NONE;

	writel_relaxed(m_INT_EDID_READY, hdmi->regs + HDMI_INTERRUPT_STATUS1);
	complete(&i2c->cmp);

	return IRQ_HANDLED;
}

static irqreturn_t axiado_hdmi_hardirq(int irq, void *dev_id)
{
	struct axiado_hdmi *hdmi = dev_id;
	irqreturn_t ret = IRQ_NONE;
	u8 interrupt;

	if (hdmi->i2c)
		ret = axiado_hdmi_i2c_irq(hdmi);

	interrupt = readl_relaxed(hdmi->regs + HDMI_STATUS);
	if (interrupt & m_INT_HOTPLUG) {
		hdmi_modl(hdmi, HDMI_STATUS, m_INT_HOTPLUG, m_INT_HOTPLUG);
		ret = IRQ_WAKE_THREAD;
	}

	return ret;
}

static irqreturn_t axiado_hdmi_irq(int irq, void *dev_id)
{
	struct axiado_hdmi *hdmi = dev_id;

	if (hdmi->connector.dev)
		drm_helper_hpd_irq_event(hdmi->connector.dev);

	return IRQ_HANDLED;
}

/*
 * One-shot post-bind HPD re-probe. On the USB-C / DP-Alt path the
 * downstream chain (PD chip → DP chip → USB-C-to-HDMI adapter →
 * monitor) finishes negotiating after Linux has already enumerated
 * the DRM device, so the first connector .detect can return
 * disconnected even when a monitor is physically plugged in. Without
 * this kick the user sees a black screen until they replug the cable
 * to generate a fresh HPD edge. Firing drm_helper_hpd_irq_event()
 * after a few seconds re-runs .detect, picks up any late HPD edge
 * the IRQ missed, and lets fbdev pick the right mode automatically.
 */
#define AXIADO_HDMI_BOOT_HPD_DELAY_MS 3000

static void axiado_hdmi_hpd_probe_work(struct work_struct *work)
{
	struct axiado_hdmi *hdmi = container_of(to_delayed_work(work),
						struct axiado_hdmi,
						hpd_probe_work);

	if (!hdmi->connector.dev)
		return;

	drm_helper_hpd_irq_event(hdmi->connector.dev);
}

static int axiado_hdmi_i2c_read(struct axiado_hdmi *hdmi, struct i2c_msg *msgs)
{
	int length = msgs->len;
	u8 *buf = msgs->buf;
	int ret;

	ret = wait_for_completion_timeout(&hdmi->i2c->cmp, HZ / 10);
	if (!ret) {
		return -EAGAIN;
	}
	while (length--)
		*buf++ = readl_relaxed(hdmi->regs + HDMI_EDID_FIFO_ADDR);

	return 0;
}

static int axiado_hdmi_i2c_write(struct axiado_hdmi *hdmi, struct i2c_msg *msgs)
{
	if ((msgs->len != 1) ||
	    ((msgs->addr != DDC_ADDR) && (msgs->addr != DDC_SEGMENT_ADDR)))
		return -EINVAL;

	reinit_completion(&hdmi->i2c->cmp);

	if (msgs->addr == DDC_SEGMENT_ADDR)
		hdmi->i2c->segment_addr = msgs->buf[0];
	if (msgs->addr == DDC_ADDR)
		hdmi->i2c->ddc_addr = msgs->buf[0];

	writel_relaxed(0x00, hdmi->regs + HDMI_EDID_FIFO_OFFSET);
	writel_relaxed(hdmi->i2c->ddc_addr, hdmi->regs + HDMI_EDID_WORD_ADDR);
	writel_relaxed(hdmi->i2c->segment_addr, hdmi->regs + HDMI_EDID_SEGMENT_POINTER);

	return 0;
}

static int axiado_hdmi_i2c_xfer(struct i2c_adapter *adap, struct i2c_msg *msgs,
				int num)
{
	struct axiado_hdmi *hdmi = i2c_get_adapdata(adap);
	struct axiado_hdmi_i2c *i2c = hdmi->i2c;
	int i, ret = 0;

	mutex_lock(&i2c->lock);
	writel_relaxed(m_INT_EDID_READY, hdmi->regs + HDMI_INTERRUPT_MASK1);
	writel_relaxed(m_INT_EDID_READY, hdmi->regs + HDMI_INTERRUPT_STATUS1);

	for (i = 0; i < num; i++) {
		dev_dbg(hdmi->dev, "xfer: num: %d/%d, len: %d, flags: %#x\n",
			i + 1, num, msgs[i].len, msgs[i].flags);
		if (msgs[i].flags & I2C_M_RD)
			ret = axiado_hdmi_i2c_read(hdmi, &msgs[i]);
		else
			ret = axiado_hdmi_i2c_write(hdmi, &msgs[i]);

		if (ret < 0)
			break;
	}
	if (!ret)
		ret = num;

	writel_relaxed(0, hdmi->regs + HDMI_INTERRUPT_MASK1);
	mutex_unlock(&i2c->lock);

	return ret;
}

static u32 axiado_hdmi_i2c_func(struct i2c_adapter *adapter)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;
}

static const struct i2c_algorithm axiado_hdmi_algorithm = {
	.master_xfer = axiado_hdmi_i2c_xfer,
	.functionality = axiado_hdmi_i2c_func,
};

static struct i2c_adapter *axiado_hdmi_i2c_adapter(struct axiado_hdmi *hdmi)
{
	struct i2c_adapter *adap;
	struct axiado_hdmi_i2c *i2c;
	int ret;

	i2c = devm_kzalloc(hdmi->dev, sizeof(*i2c), GFP_KERNEL);
	if (!i2c)
		return ERR_PTR(-ENOMEM);

	mutex_init(&i2c->lock);
	init_completion(&i2c->cmp);

	adap = &i2c->adap;
	adap->owner = THIS_MODULE;
	adap->dev.parent = hdmi->dev;
	/* Removed adap->dev.of_node assignment to prevent I2C modalias parsing of DRM port nodes */
	adap->algo = &axiado_hdmi_algorithm;
	strscpy(adap->name, "axiado HDMI", sizeof(adap->name));

	i2c_set_adapdata(adap, hdmi);
	ret = i2c_add_adapter(adap);
	if (ret) {
		dev_warn(hdmi->dev, "cannot add %s I2C adapter\n", adap->name);
		devm_kfree(hdmi->dev, i2c);
		return ERR_PTR(ret);
	}

	hdmi->i2c = i2c;
	return adap;
}

static void ax_hdmi_debugfs_exit(struct axiado_hdmi *hdmi)
{
	debugfs_remove_recursive(hdmi->debugfs);
	hdmi->debugfs = NULL;
}

static void ax_hdmi_debugfs_init(struct axiado_hdmi *hdmi)
{
	hdmi->debugfs = debugfs_create_dir("axiado-hdmi", NULL);
	debugfs_create_file("hdmi_regs", 0444, hdmi->debugfs, hdmi,
			    &hdmi_regs_fops);
}

static int axiado_hdmi_bind(struct device *dev, struct device *master,
			    void *data)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct device_node *np = dev->of_node;
	struct device_node *disp_node_entry;
	struct drm_device *drm = data;
	struct axiado_hdmi *hdmi;
	struct resource res;
	int irq;
	int ret;

	hdmi = devm_kzalloc(dev, sizeof(*hdmi), GFP_KERNEL);
	if (!hdmi)
		return -ENOMEM;

	hdmi->dev = dev;
	hdmi->drm_dev = drm;

	hdmi->regs = devm_platform_get_and_ioremap_resource(pdev, 0, NULL);
	if (IS_ERR(hdmi->regs))
		return PTR_ERR(hdmi->regs);

	disp_node_entry = of_parse_phandle(np, "memory-region", 0);
	if (disp_node_entry) {
		ret = of_address_to_resource(disp_node_entry, 0, &res);
		of_node_put(disp_node_entry);
		if (!ret) {
			hdmi->data_reg = devm_memremap(hdmi->dev, res.start,
						       resource_size(&res),
						       MEMREMAP_WB);
			if (!hdmi->data_reg)
				dev_warn(dev,
					 "failed to map reserved buffer\n");
		}
	}

	hdmi->pclk = devm_clk_get_enabled(hdmi->dev, "clk_pclk");
	if (IS_ERR(hdmi->pclk)) {
		dev_err(hdmi->dev, "Unable to get and enable HDMI pixel clock\n");
		return PTR_ERR(hdmi->pclk);
	}

	/* Use get_optional to ignore safely if the GPIO isn't populated in Device Tree */
	hdmi->ax_hdmi_pwren =
		devm_gpiod_get_optional(hdmi->dev, "pwren", GPIOD_OUT_HIGH);
	if (IS_ERR(hdmi->ax_hdmi_pwren))
		dev_warn(hdmi->dev,
			 "Could not get pwren gpio, proceeding without it\n");

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		ret = irq;
		return ret;
	}

	axiado_hdmi_reset(hdmi);

	/*
	 * Power on the PHY bias circuit and rxsense before creating the DDC
	 * adapter.  axiado_hdmi_bias_on() is also called from
	 * axiado_hdmi_setup() at modeset time, but EDID is read by
	 * get_modes() before any modeset happens.  Without the bias on, the
	 * DDC hardware cannot drive the I2C lines and every EDID read times
	 * out.
	 */
	axiado_hdmi_bias_on(hdmi);

	hdmi->ddc = axiado_hdmi_i2c_adapter(hdmi);
	if (IS_ERR(hdmi->ddc)) {
		ret = PTR_ERR(hdmi->ddc);
		hdmi->ddc = NULL;
		return ret;
	}

	hdmi->tmds_rate = clk_get_rate(hdmi->pclk);
	axiado_hdmi_i2c_init(hdmi);

	/* Registration of Encoder and Connector natively to fix pipeline linkage */
	hdmi->encoder.possible_crtcs = 1;

	drm_encoder_init(drm, &hdmi->encoder, &axiado_hdmi_encoder_funcs,
			 DRM_MODE_ENCODER_TMDS, NULL);
	drm_encoder_helper_add(&hdmi->encoder,
			       &axiado_hdmi_encoder_helper_funcs);

	drm_connector_init_with_ddc(drm, &hdmi->connector,
				    &axiado_hdmi_connector_funcs,
				    DRM_MODE_CONNECTOR_HDMIA,
				    hdmi->ddc);
	drm_connector_helper_add(&hdmi->connector,
				 &axiado_hdmi_connector_helper_funcs);

	/*
	 * Enable HPD-driven re-probing. drm_helper_hpd_irq_event() (called
	 * from the HDMI threaded IRQ on every hot-plug edge) walks the
	 * connector list and only re-polls connectors that have
	 * DRM_CONNECTOR_POLL_HPD set in connector.polled. Leaving it at
	 * the default 0 makes the IRQ a no-op from DRM's perspective —
	 * sysfs status stays stale until userspace pokes
	 * `echo detect > status`. With the flag set the IRQ does its job
	 * and a uevent fires automatically.
	 */
	hdmi->connector.polled = DRM_CONNECTOR_POLL_HPD;

	drm_connector_attach_encoder(&hdmi->connector, &hdmi->encoder);

	dev_set_drvdata(dev, hdmi);
	hdmi_modl(hdmi, HDMI_STATUS, m_MASK_INT_HOTPLUG, v_MASK_INT_HOTPLUG(1));

	ret = devm_request_threaded_irq(dev, irq, axiado_hdmi_hardirq,
					axiado_hdmi_irq, IRQF_SHARED,
					dev_name(dev), hdmi);
	if (ret < 0)
		goto err_cleanup_connector;

	if (IS_ENABLED(CONFIG_DEBUG_FS))
		ax_hdmi_debugfs_init(hdmi);

	/*
	 * Kick a delayed HPD re-probe — see the work function comment above
	 * for the USB-C / DP-Alt timing rationale.
	 */
	INIT_DELAYED_WORK(&hdmi->hpd_probe_work, axiado_hdmi_hpd_probe_work);
	schedule_delayed_work(&hdmi->hpd_probe_work,
			      msecs_to_jiffies(AXIADO_HDMI_BOOT_HPD_DELAY_MS));

	return 0;

err_cleanup_connector:
	drm_connector_cleanup(&hdmi->connector);
	drm_encoder_cleanup(&hdmi->encoder);
	i2c_del_adapter(&hdmi->i2c->adap);
	return ret;
}

static void axiado_hdmi_unbind(struct device *dev, struct device *master,
			       void *data)
{
	struct axiado_hdmi *hdmi = dev_get_drvdata(dev);

	if (!hdmi)
		return;

	if (IS_ENABLED(CONFIG_DEBUG_FS))
		ax_hdmi_debugfs_exit(hdmi);

	/*
	 * Synchronously cancel the post-bind HPD re-probe before tearing
	 * the connector down — the work callback dereferences
	 * hdmi->connector.dev.
	 */
	cancel_delayed_work_sync(&hdmi->hpd_probe_work);

	/*
	 * Mask the HPD interrupt before tearing down the connector so a
	 * late IRQ cannot dereference a half-destroyed drm_connector. The
	 * IRQ line itself is freed by devm after this callback returns.
	 */
	hdmi_modl(hdmi, HDMI_STATUS, m_MASK_INT_HOTPLUG, v_MASK_INT_HOTPLUG(0));

	drm_connector_cleanup(&hdmi->connector);
	drm_encoder_cleanup(&hdmi->encoder);
	if (hdmi->i2c)
		i2c_del_adapter(&hdmi->i2c->adap);
}

static const struct component_ops axiado_hdmi_ops = {
	.bind = axiado_hdmi_bind,
	.unbind = axiado_hdmi_unbind,
};

static int axiado_hdmi_probe(struct platform_device *pdev)
{
	return component_add(&pdev->dev, &axiado_hdmi_ops);
}

static void axiado_hdmi_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &axiado_hdmi_ops);
}

static const struct of_device_id axiado_hdmi_dt_ids[] = {
	{
		.compatible = "axiado,ax3000-hdmi",
	},
	{},
};
MODULE_DEVICE_TABLE(of, axiado_hdmi_dt_ids);

struct platform_driver axiado_hdmi_driver = {
	.probe  = axiado_hdmi_probe,
	.remove = axiado_hdmi_remove,
	.driver = {
		.name = "axiado-hdmi",
		.of_match_table = axiado_hdmi_dt_ids,
	},
};

MODULE_AUTHOR("Axiado Corporation");
MODULE_DESCRIPTION("Axiado HDMI TX Driver");
MODULE_LICENSE("GPL");
