// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022-2025 Axiado Corporation.
 *
 * Based on inno_hdmi.c
 *   Copyright (c) Fuzhou Rockchip Electronics Co. Ltd.
 *     Zheng Yang <zhengyang@rock-chips.com>
 *     Yakir Yang <ykk@rock-chips.com>
 */

#include <linux/irq.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/hdmi.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_device.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/of_address.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_edid.h>
#include <drm/drm_of.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_simple_kms_helper.h>
#include <linux/platform_device.h>
#include <drm/drm_crtc.h>
#include <drm/drm_print.h>
#include <linux/component.h>
#include "axiado_hdmi.h"

#define to_axiado_hdmi(x) container_of(x, struct axiado_hdmi, x)

static inline u8 hdmi_readb(struct axiado_hdmi *hdmi, u32 offset)
{
	u8 status = readb(hdmi->regs + (offset));
	return status;
}

static inline void hdmi_writeb(struct axiado_hdmi *hdmi, u32 offset, u32 val)
{
	writel_relaxed(val, hdmi->regs + (offset));
}

static inline void ax_hdmi_write8(struct axiado_hdmi *hdmi, u32 offset, u8 val)
{
	writeb(val, hdmi->data_reg + offset);
}

static inline void hdmi_modb(struct axiado_hdmi *hdmi, u32 offset, u32 msk,
			     u32 val)
{
	u8 temp = hdmi_readb(hdmi, offset) & ~msk;

	temp |= val & msk;
	hdmi_writeb(hdmi, offset, temp);
}

void ax_hdmi_write_edid(struct axiado_hdmi *hdmi, struct edid *edid_data)
{
	int i;
	u8 *data;
	size_t edid_length;

	if (edid_data) {
		edid_length = (edid_data->extensions + 1) * EDID_LENGTH;
		data = (u8 *)edid_data;

		/* Write the EDID to the BAR so that x86 driver can access the same */
		for (i = AX_HDMI_EDID_REG_OFFSET; i < edid_length; i++)
			ax_hdmi_write8(hdmi, i, data[i]);
	}
}

/* TO DO */
/* For now registers are hard coded will change it to dynamic in future */

/**
 * axiado_pll_config - this function is used to set the PLL clocks
 * These resgister settings are for 1280X720 and 640X480
 * Can add registers for other resolutions in future
 */
static void axiado_pll_config(struct axiado_hdmi *hdmi,
			     struct drm_display_mode *mode)
{
	u8 pll;

	hdmi_writeb(hdmi, HDMI_PLL0, HDMI_PLL0_VAL);
	hdmi_writeb(hdmi, HDMI_PLL1, HDMI_PLL1_VAL);
	hdmi_writeb(hdmi, HDMI_PLL2, HDMI_PLL2_VAL);
	if (!(strcmp(mode->name, "1280x720"))) {
		hdmi_writeb(hdmi, HDMI_PLL3, HDMI_PLL3_VAL);
		pll = hdmi_readb(hdmi, HDMI_PLL4);
		hdmi_writeb(hdmi, HDMI_PLL4, pll ^ m_PLL4);
		hdmi_writeb(hdmi, HDMI_PLL5, HDMI_PLL5_VAL);
	} else if (!(strcmp(mode->name, "640x480"))) {
		pll = hdmi_readb(hdmi, HDMI_PLL3);
		hdmi_writeb(hdmi, HDMI_PLL3, pll ^ m_PLL3);
		pll = hdmi_readb(hdmi, HDMI_PLL4);
		hdmi_writeb(hdmi, HDMI_PLL4, pll ^ m_PLL4_640);
		pll = hdmi_readb(hdmi, HDMI_PLL5);
		hdmi_writeb(hdmi, HDMI_PLL5, pll ^ m_PLL5);
	} else {
		dev_dbg(hdmi->dev, "axiado-hdmi: No settings for given resolution ");
	}
	pll = hdmi_readb(hdmi, HDMI_PLL6);
	hdmi_writeb(hdmi, HDMI_PLL6, pll ^ m_PLL6);
	hdmi_writeb(hdmi, HDMI_PLL7, HDMI_PLL7_VAL);
	hdmi_writeb(hdmi, HDMI_PLL8, HDMI_PLL8_VAL);
	hdmi_writeb(hdmi, HDMI_PLL9, HDMI_PLL9_VAL);
	pll = hdmi_readb(hdmi, HDMI_PLL10);
	hdmi_writeb(hdmi, HDMI_PLL10, pll ^ m_PLL10_DIVIDER_CTRL_0);
	pll = hdmi_readb(hdmi, HDMI_PLL11);
	hdmi_writeb(hdmi, HDMI_PLL11, pll ^ m_PLL11_DIVIDER_CTRL_0);
	if (!(strcmp(mode->name, "640x480"))) {
		pll = hdmi_readb(hdmi, HDMI_PLL13);
		hdmi_writeb(hdmi, HDMI_PLL13, pll ^ m_PLL13);
		pll = hdmi_readb(hdmi, HDMI_PLL14);
		hdmi_writeb(hdmi, HDMI_PLL14, pll ^ m_PLL14_DIVIDER_CTRL_2);
	}
	usleep_range(10, 15);
	pll = hdmi_readb(hdmi, HDMI_LDO);
	hdmi_writeb(hdmi, HDMI_LDO, pll ^ m_HDMI_LDO);
	pll = hdmi_readb(hdmi, HDMI_SERIAL);
	hdmi_writeb(hdmi, HDMI_SERIAL, pll ^ m_HDMI_SERIAL);
}

static void axiado_hdmi_tmds_sync(struct axiado_hdmi *hdmi)
{
	u8 tmds, mask;

	hdmi_writeb(hdmi, HDMI_TMDS_SYNC, 0x00);
	hdmi_writeb(hdmi, HDMI_TMDS_SYNC, 0x00);
	hdmi_writeb(hdmi, HDMI_TMDS_SYNC, 0x01);

	tmds = hdmi_readb(hdmi, HDMI_TMDS_SYS_CTL);
	mask = (m_TMDS_CURRENT_PWR | m_BANDGAP_PWR | m_PLL_PWR |
		m_TMDS_CHG_PWR);
	hdmi_writeb(hdmi, HDMI_TMDS_SYS_CTL, tmds ^ mask);
}

static void axiado_hdmi_i2c_init(struct axiado_hdmi *hdmi)
{
	int ddc_bus_freq;

	ddc_bus_freq = (hdmi->tmds_rate >> 2) / HDMI_SCL_RATE;
	hdmi_writeb(hdmi, DDC_BUS_FREQ_L, ddc_bus_freq & 0xFF);
	hdmi_writeb(hdmi, DDC_BUS_FREQ_H, (ddc_bus_freq >> 8) & 0xFF);
	/* Clear the EDID interrupt flag and mute the interrupt */
	hdmi_writeb(hdmi, HDMI_INTERRUPT_MASK1, 0);
	hdmi_writeb(hdmi, HDMI_INTERRUPT_STATUS1, m_INT_EDID_READY);
}

/**
 *axiado_hdmi_sys_power - this function is used to set the power
 *@bool:0,1 to set or reset the power
 */

static void axiado_hdmi_sys_power(struct axiado_hdmi *hdmi, bool enable)
{
	if (enable)
		hdmi_modb(hdmi, HDMI_SYS_CTRL, m_POWER, v_PWR_ON);
	else
		hdmi_modb(hdmi, HDMI_SYS_CTRL, m_POWER, v_PWR_OFF);
}

/**
 *axiado_hdmi_set_pwr_mode - set the power modes
 *@mode - Normal or low power mode
 */
static void axiado_hdmi_set_pwr_mode(struct axiado_hdmi *hdmi, int mode)
{
	switch (mode) {
	case NORMAL:
		axiado_hdmi_sys_power(hdmi, false);
		axiado_hdmi_sys_power(hdmi, true);
		break;

	case LOWER_PWR:
		axiado_hdmi_sys_power(hdmi, false);
		break;

	default:
		DRM_DEV_ERROR(hdmi->dev, "axiado-hdmi: Unknown power mode %d\n",
			      mode);
	}
}

static void axiado_hdmi_bias_on(struct axiado_hdmi *hdmi)
{
	u8 val;

	val = hdmi_readb(hdmi, HDMI_BIAS_CIRCUIT);
	hdmi_writeb(hdmi, HDMI_BIAS_CIRCUIT, val ^ m_BIAS_CONTROL);

	val = hdmi_readb(hdmi, HDMI_RX_SENSE);
	hdmi_writeb(hdmi, HDMI_RX_SENSE, val ^ m_HDMI_RX_SENSE);
}

/**
 *axiado_hdmi_reset - reset the hdmi
 *resetting HDMI using registers
 */
static void axiado_hdmi_reset(struct axiado_hdmi *hdmi)
{
	u32 val, msk, i, j;

	hdmi_writeb(hdmi, HDMI_SYS_CTRL, HDMI_RESET_VAL);
	hdmi_writeb(hdmi, HDMI_BIAS_CIRCUIT, HDMI_BIAS_DEF);
	hdmi_writeb(hdmi, HDMI_PLL0, HDMI_RESET_VAL1);
	hdmi_writeb(hdmi, HDMI_PLL1, HDMI_RESET_VAL1);
	hdmi_writeb(hdmi, HDMI_PLL2, HDMI_PLL2_VAL);
	hdmi_writeb(hdmi, HDMI_PLL3, HDMI_SYS_PWR_ON);
	hdmi_writeb(hdmi, HDMI_PLL4, HDMI_PLL4_VAL);
	hdmi_writeb(hdmi, HDMI_PLL5, HDMI_PLL5_VAL);
	hdmi_writeb(hdmi, HDMI_PLL6, HDMI_PLL6_VAL);
	hdmi_writeb(hdmi, HDMI_RX_SENSE, HDMI_RESET_VAL);
	hdmi_writeb(hdmi, HDMI_PLL7, HDMI_RESET_VAL);
	hdmi_writeb(hdmi, HDMI_PLL8, HDMI_RESET_VAL);
	hdmi_writeb(hdmi, HDMI_PLL9, HDMI_RESET_VAL);
	hdmi_writeb(hdmi, HDMI_PLL10, HDMI_RESET_VAL1);
	hdmi_writeb(hdmi, HDMI_PLL11, HDMI_PLL11_VAL);
	hdmi_writeb(hdmi, HDMI_PLL12, HDMI_RESET_VAL1);
	hdmi_writeb(hdmi, HDMI_PLL13, HDMI_PLL13_VAL);
	hdmi_writeb(hdmi, HDMI_PLL14, HDMI_PLL14_VAL);
	hdmi_writeb(hdmi, HDMI_SYS_RESET1, HDMI_REG_OFFSET);
	j = HDMI_SYS_RESET2;
	for (i = 0; i < 4; i++) {
		hdmi_writeb(hdmi, j, HDMI_RESET_VAL);
		j = j + HDMI_REG_OFFSET;
	}
	hdmi_writeb(hdmi, HDMI_SYS_DEF1, HDMI_RESET_VAL);
	hdmi_writeb(hdmi, HDMI_SYS_DEF2, HDMI_RESET_VAL);
	j = HDMI_SYS_DEF3;
	for (i = 0; i < 4; i++) {
		hdmi_writeb(hdmi, j, HDMI_RESET_VAL2);
		j = j + HDMI_REG_OFFSET;
	}

	hdmi_writeb(hdmi, HDMI_SYS_DEF4, HDMI_RESET_VAL);
	j = HDMI_SYS_DEF5;
	for (i = 0; i < 3; i++) {
		hdmi_writeb(hdmi, j, HDMI_RESET_VAL);
		j = j + HDMI_REG_OFFSET;
	}
	hdmi_writeb(hdmi, HDMI_LDO, HDMI_RESET_VAL);
	hdmi_writeb(hdmi, HDMI_SERIAL, HDMI_RESET_VAL1);
	hdmi_writeb(hdmi, HDMI_SYS_CTRL, HDMI_SYS_PWR_ON);
	hdmi_writeb(hdmi, HDMI_SYS_CTRL1, HDMI_RESET_VAL1);
	hdmi_writeb(hdmi, HDMI_SYS_CTRL2,  HDMI_CTRL2_VAL);
	hdmi_writeb(hdmi, HDMI_CONTROL_PACKET_BUF_INDEX, HDMI_RESET_VAL);
	j =  HDMI_CONTROL_PACKET_ADDR;
	for (i = 0 ; i < 9; i++) {
		hdmi_writeb(hdmi, j, HDMI_RESET_VAL);
		j = j + HDMI_REG_OFFSET;
	}

	hdmi_writeb(hdmi, HDMI_SYS_CTRL, HDMI_SYS_PWR_ON);
	hdmi_writeb(hdmi, HDMI_TMDS_SYNC, HDMI_RESET_VAL1);
	hdmi_writeb(hdmi, HDMI_TMDS_SYS_CTL, TMDS_SYNC_VAL2);

	hdmi_modb(hdmi, HDMI_SYS_CTRL, m_RST_DIGITAL, v_NOT_RST_DIGITAL);
	usleep_range(100, 110);
	hdmi_modb(hdmi, HDMI_SYS_CTRL, m_RST_ANALOG, v_NOT_RST_ANALOG);
	usleep_range(100, 110);
	msk = m_REG_CLK_INV | m_REG_CLK_SOURCE | m_POWER | m_INT_POL;
	val = v_REG_CLK_INV | v_REG_CLK_SOURCE_SYS | v_PWR_ON | v_INT_POL_HIGH;
	hdmi_modb(hdmi, HDMI_SYS_CTRL, msk, val);
	axiado_hdmi_set_pwr_mode(hdmi, NORMAL);
}

/**
 *axiado_hdmi_upload_frame - upload frame to hdmi
 */
static int axiado_hdmi_upload_frame(struct axiado_hdmi *hdmi, int setup_rc,
				   union hdmi_infoframe *frame, u32 frame_index,
				   u32 mask, u32 disable, u32 enable)
{
	hdmi_writeb(hdmi, HDMI_CONTROL_PACKET_BUF_INDEX, frame_index);

	if (setup_rc >= 0) {
		u8 packed_frame[HDMI_MAXIMUM_INFO_FRAME_SIZE];
		ssize_t rc, i;

		rc = hdmi_infoframe_pack(frame, packed_frame,
					 sizeof(packed_frame));
		if (rc < 0)
			return rc;

		for (i = 0; i < HDMI_MAXIMUM_INFO_FRAME_SIZE; i++) {
			hdmi_writeb(hdmi, HDMI_CONTROL_PACKET_ADDR + (i * 4),
				    packed_frame[i]);
		}
	}
	return setup_rc;
}

static int axiado_hdmi_config_video_vsi(struct axiado_hdmi *hdmi,
				       struct drm_display_mode *mode)
{
	union hdmi_infoframe frame;
	int rc = 0;

	return axiado_hdmi_upload_frame(hdmi, rc, &frame, INFOFRAME_VSI,
				       m_PACKET_VSI_EN, v_PACKET_VSI_EN(0),
				       v_PACKET_VSI_EN(1));
}

static int axiado_hdmi_config_video_avi(struct axiado_hdmi *hdmi,
				       struct drm_display_mode *mode)
{
	union hdmi_infoframe frame;
	int rc;

	rc = drm_hdmi_avi_infoframe_from_display_mode(&frame.avi,
						      &hdmi->connector, mode);
	if (hdmi->hdmi_data.enc_out_format == HDMI_COLORSPACE_YUV444)
		frame.avi.colorspace = HDMI_COLORSPACE_YUV444;
	else if (hdmi->hdmi_data.enc_out_format == HDMI_COLORSPACE_YUV422)
		frame.avi.colorspace = HDMI_COLORSPACE_YUV422;
	else
		frame.avi.colorspace = HDMI_COLORSPACE_RGB;

	return axiado_hdmi_upload_frame(hdmi, rc, &frame, INFOFRAME_AVI, 0, 0,
				       0);
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
	if (hdmi->hdmi_data.sink_is_hdmi) {
		axiado_hdmi_config_video_avi(hdmi, mode);
		axiado_hdmi_config_video_vsi(hdmi, mode);
	}
	/*
	 * When IP controller have configured to an accurate video
	 * timing, then the TMDS clock source would be switched to
	 * DCLK_LCDC, so we need to init the TMDS rate to mode pixel
	 * clock rate, and reconfigure the DDC clock.
	 */
	hdmi->tmds_rate = mode->clock * 1000;
	axiado_hdmi_i2c_init(hdmi);
	hdmi_writeb(hdmi, HDMI_SYS_CTRL, HDMI_SYS_PWR_LOW);
	axiado_hdmi_tmds_sync(hdmi);
	return 0;
}

/**
 *axiado_hdmi_encoder_mode_set - update the display mode of an encoder.
 */
static void axiado_hdmi_encoder_mode_set(struct drm_encoder *encoder,
					struct drm_display_mode *mode,
					struct drm_display_mode *adj_mode)
{
	struct axiado_hdmi *hdmi = to_axiado_hdmi(encoder);

	axiado_hdmi_setup(hdmi, adj_mode);
	/* Store the display mode for plugin/DPMS poweron events */
	drm_mode_copy(&hdmi->previous_mode, adj_mode);
}

static void axiado_hdmi_encoder_enable(struct drm_encoder *encoder)
{
	struct axiado_hdmi *hdmi = to_axiado_hdmi(encoder);

	axiado_hdmi_set_pwr_mode(hdmi, NORMAL);
}

static void axiado_hdmi_encoder_disable(struct drm_encoder *encoder)
{
	struct axiado_hdmi *hdmi = to_axiado_hdmi(encoder);

	axiado_hdmi_set_pwr_mode(hdmi, LOWER_PWR);
}

static bool axiado_hdmi_encoder_mode_fixup(struct drm_encoder *encoder,
					  const struct drm_display_mode *mode,
					  struct drm_display_mode *adj_mode)
{
	if (!encoder || !mode || !adj_mode)
		return false;
	return true;
}

/**
 *axiado_hdmi_encoder_atomic_check - update the display mode of an encoder
 *This callback is used by the atomic modeset helpers in place of the
 *@mode_set callback, if set by the driver.
 */
static int
axiado_hdmi_encoder_atomic_check(struct drm_encoder *encoder,
				struct drm_crtc_state *crtc_state,
				struct drm_connector_state *conn_state)
{
	struct axiado_crtc_state *s = to_axiado_crtc_state(crtc_state);

	if (!encoder || !crtc_state || !conn_state)
		return -EINVAL;
	s->output_mode = AXIADO_OUT_MODE_P888;
	s->output_type = DRM_MODE_CONNECTOR_HDMIA;
	return 0;
}

static const struct drm_encoder_helper_funcs axiado_hdmi_encoder_helper_funcs = {
	.enable = axiado_hdmi_encoder_enable,
	.disable = axiado_hdmi_encoder_disable,
	.mode_fixup = axiado_hdmi_encoder_mode_fixup,
	.mode_set = axiado_hdmi_encoder_mode_set,
	.atomic_check = axiado_hdmi_encoder_atomic_check,
};

/**
 *axiado_hdmi_connector_detect - anything is attached to the connector
 *The parameter force is set to false whilst polling, true when checking the
 *connector due to a user request. force can be used by the driver to
 *avoid expensive, destructive operations during automated probing.
 */
static enum drm_connector_status
axiado_hdmi_connector_detect(struct drm_connector *connector, bool force)
{
	struct axiado_hdmi *hdmi = to_axiado_hdmi(connector);

	if (!hdmi)
		return -EINVAL;
	return (hdmi_readb(hdmi, HDMI_STATUS) & m_HOTPLUG) ?
		       connector_status_connected :
		       connector_status_disconnected;
}

/**
 *axiado_hdmi_connector_get_modes -  fill in all modes currently valid for the
 *sink into the &drm_connector.
 *fill in all modes currently valid for the sink
 *into the &drm_connector.
 */
static int axiado_hdmi_connector_get_modes(struct drm_connector *connector)
{
	struct axiado_hdmi *hdmi = to_axiado_hdmi(connector);
	struct edid *edid;
	int ret = 0;

	if (!connector)
		return -EINVAL;
	if (!hdmi->ddc)
		return 0;
	edid = drm_get_edid(connector, hdmi->ddc);
	if (edid) {
		hdmi->hdmi_data.sink_is_hdmi = drm_detect_hdmi_monitor(edid);
		drm_connector_update_edid_property(connector, edid);
		ret = drm_add_edid_modes(connector, edid);

		/* Write the EDID to the On-Chip Memory for PCIe Display */
		ax_hdmi_write_edid(hdmi, edid);
		kfree(edid);
	}
	return ret;
}

/*
 *axiado_hdmi_connector_mode_valid - reset connector hardware and software state to off
 *isn't called by the core directly, only through drm_mode_config_reset().
 */
static enum drm_mode_status
axiado_hdmi_connector_mode_valid(struct drm_connector *connector,
				struct drm_display_mode *mode)
{
	if (!connector || !mode)
		return MODE_ERROR;
	return MODE_OK;
}

/*
 *axiado_hdmi_probe_single_connector_modes -  get complete set of display modes
 *@connector: connector to probe
 *@maxX: max width for modes
 *@maxY: max height for modes
 */
static int
axiado_hdmi_probe_single_connector_modes(struct drm_connector *connector,
					uint32_t maxX, uint32_t maxY)
{
	struct axiado_connector *axiado_connector;
	struct drm_device *dev;
	struct drm_display_mode *mode, *iterator;

	axiado_connector = to_axiado_connector(connector);
	if (!axiado_connector)
		return 0;
	dev = axiado_connector->base.dev;
	list_for_each_entry_safe(mode, iterator, &connector->modes, head) {
		list_del(&mode->head);
		drm_mode_destroy(dev, mode);
	}
	/*
	 * Configured 480p resolution here as the PCIe Display Data Path is
	 * 480p till the time Control Path is Up for PCIe Display
	 */
	return drm_helper_probe_single_connector_modes(connector, 640, 480);
}

/*
 *axiado_hdmi_connector_destroy - clean up connector resources.
 *this is called at driver unload time
 *it can also be called at runtime when a connector is being hot-unplugged
 *for drivers that support
 */
static void axiado_hdmi_connector_destroy(struct drm_connector *connector)
{
	drm_connector_unregister(connector);
	drm_connector_cleanup(connector);
}

static const struct drm_connector_funcs axiado_hdmi_connector_funcs = {
	.fill_modes = axiado_hdmi_probe_single_connector_modes,
	.detect = axiado_hdmi_connector_detect,
	.destroy = axiado_hdmi_connector_destroy,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

static struct drm_connector_helper_funcs axiado_hdmi_connector_helper_funcs = {
	.get_modes = axiado_hdmi_connector_get_modes,
	.mode_valid = axiado_hdmi_connector_mode_valid,
};

/*
 *axiado_hdmi_register - register the hdmi with different components
 */
static int axiado_hdmi_register(struct drm_device *drm, struct axiado_hdmi *hdmi)
{
	struct drm_encoder *encoder = &hdmi->encoder;
	struct device *dev = hdmi->dev;

	encoder->possible_crtcs = drm_of_find_possible_crtcs(drm, dev->of_node);

	/*
	 *If we failed to find the CRTC(s) which this encoder is
	 *supposed to be connected to, it's because the CRTC has
	 *not been registered yet.  Defer probing, and hope that
	 *the required CRTC is added later.
	 */
	if (encoder->possible_crtcs == 0)
		return -EPROBE_DEFER;
	drm_encoder_helper_add(encoder, &axiado_hdmi_encoder_helper_funcs);
	drm_simple_encoder_init(drm, encoder, DRM_MODE_ENCODER_TMDS);
	hdmi->connector.polled = DRM_CONNECTOR_POLL_HPD;
	drm_connector_helper_add(&hdmi->connector,
				 &axiado_hdmi_connector_helper_funcs);
	drm_connector_init_with_ddc(drm, &hdmi->connector,
				    &axiado_hdmi_connector_funcs,
				    DRM_MODE_CONNECTOR_HDMIA, hdmi->ddc);
	drm_connector_attach_encoder(&hdmi->connector, encoder);
	return 0;
}

static irqreturn_t axiado_hdmi_i2c_irq(struct axiado_hdmi *hdmi)
{
	struct axiado_hdmi_i2c *i2c = hdmi->i2c;
	u8 stat;

	stat = hdmi_readb(hdmi, HDMI_INTERRUPT_STATUS1);

	if (!(stat & m_INT_EDID_READY))
		return IRQ_NONE;
	/* Clear HDMI EDID interrupt flag */
	hdmi_writeb(hdmi, HDMI_INTERRUPT_STATUS1, m_INT_EDID_READY);
	complete(&i2c->cmp);
	return IRQ_HANDLED;
}

static irqreturn_t axiado_hdmi_hardirq(int irq, void *dev_id)
{
	struct axiado_hdmi *hdmi = dev_id;
	irqreturn_t ret = IRQ_NONE;
	u8 interrupt;

	if (!dev_id)
		return IRQ_NONE;
	if (hdmi->i2c)
		ret = axiado_hdmi_i2c_irq(hdmi);
	interrupt = hdmi_readb(hdmi, HDMI_STATUS);
	if (interrupt & m_INT_HOTPLUG) {
		hdmi_modb(hdmi, HDMI_STATUS, m_INT_HOTPLUG, m_INT_HOTPLUG);
		ret = IRQ_WAKE_THREAD;
	}
	return ret;
}

static irqreturn_t axiado_hdmi_irq(int irq, void *dev_id)
{
	struct axiado_hdmi *hdmi = dev_id;

	if (!dev_id)
		return IRQ_NONE;
	drm_helper_hpd_irq_event(hdmi->connector.dev);
	return IRQ_HANDLED;
}

static int axiado_hdmi_i2c_read(struct axiado_hdmi *hdmi, struct i2c_msg *msgs)
{
	int length = msgs->len;
	u8 *buf = msgs->buf;
	int ret;

	ret = wait_for_completion_timeout(&hdmi->i2c->cmp, HZ / 10);
	if (!ret)
		return -EAGAIN;
	while (length--)
		*buf++ = hdmi_readb(hdmi, HDMI_EDID_FIFO_ADDR);
	return 0;
}

static int axiado_hdmi_i2c_write(struct axiado_hdmi *hdmi, struct i2c_msg *msgs)
{
	if (!hdmi || !msgs)
		return -EINVAL;
	/*
	 * The DDC module only support read EDID message, so
	 * we assume that each word write to this i2c adapter
	 * should be the offset of EDID word address.
	 */
	if ((msgs->len != 1) ||
	    ((msgs->addr != DDC_ADDR) && (msgs->addr != DDC_SEGMENT_ADDR)))
		return -EINVAL;
	reinit_completion(&hdmi->i2c->cmp);
	if (msgs->addr == DDC_SEGMENT_ADDR)
		hdmi->i2c->segment_addr = msgs->buf[0];
	if (msgs->addr == DDC_ADDR)
		hdmi->i2c->ddc_addr = msgs->buf[0];
	/* Set EDID fifo first addr */
	hdmi_writeb(hdmi, HDMI_EDID_FIFO_OFFSET, 0x00);

	/* Set EDID word address 0x00/0x80 */
	hdmi_writeb(hdmi, HDMI_EDID_WORD_ADDR, hdmi->i2c->ddc_addr);

	/* Set EDID segment pointer */
	hdmi_writeb(hdmi, HDMI_EDID_SEGMENT_POINTER, hdmi->i2c->segment_addr);

	return 0;
}

/*
 *axiado_hdmi_i2c_xfer - to read the data from registers
 */
static int axiado_hdmi_i2c_xfer(struct i2c_adapter *adap, struct i2c_msg *msgs, int num)
{
	struct axiado_hdmi *hdmi = i2c_get_adapdata(adap);
	struct axiado_hdmi_i2c *i2c = hdmi->i2c;
	int i, ret = 0;

	mutex_lock(&i2c->lock);
	/* Clear the EDID interrupt flag and unmute the interrupt */
	hdmi_writeb(hdmi, HDMI_INTERRUPT_MASK1, m_INT_EDID_READY);
	hdmi_writeb(hdmi, HDMI_INTERRUPT_STATUS1, m_INT_EDID_READY);

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
	/* Mute HDMI EDID interrupt */
	hdmi_writeb(hdmi, HDMI_INTERRUPT_MASK1, 0);
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

/**
 *axiado_hdmi_i2c_adapter - to add the adapter
 */
static struct i2c_adapter *axiado_hdmi_i2c_adapter(struct axiado_hdmi *hdmi)
{
	struct i2c_adapter *adap;
	struct axiado_hdmi_i2c *i2c;
	int ret;

	if (!hdmi)
		return NULL;
	i2c = devm_kzalloc(hdmi->dev, sizeof(*i2c), GFP_KERNEL);
	if (!i2c)
		return ERR_PTR(-ENOMEM);
	mutex_init(&i2c->lock);
	init_completion(&i2c->cmp);
	adap = &i2c->adap;
	adap->class = I2C_CLASS_DDC;
	adap->owner = THIS_MODULE;
	adap->dev.parent = hdmi->dev;
	adap->dev.of_node = hdmi->dev->of_node;
	adap->algo = &axiado_hdmi_algorithm;
	ret = strscpy(adap->name, "axiado HDMI", sizeof(adap->name));
	if (ret < 0) {
		pr_err("Truncation occurred copying string\n");
		return ERR_PTR(ret);
	}
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

static int hdmi_reinit_state(struct seq_file *s, void *data)
{
	struct axiado_hdmi *hdmi = (struct axiado_hdmi *)dev_get_drvdata(s->private);
	int err = -1;

	if (hdmi == NULL) {
		pr_err("HDMI Info not Obtained\n");
		return err;
	}

	err = axiado_hdmi_setup(hdmi, &hdmi->previous_mode);

	if (err < 0)
		seq_puts(s, "HDMI reinit failed\n");
	else
		seq_puts(s, "HDMI reinit Done\n");

	return err;
}

static void ax_hdmi_debugfs_exit(struct axiado_hdmi *hdmi)
{
	debugfs_remove_recursive(hdmi->debugfs);
	hdmi->debugfs = NULL;
}

static void ax_hdmi_debugfs_init(struct axiado_hdmi *hdmi)
{
	hdmi->debugfs = debugfs_create_dir("hdmi", NULL);

	debugfs_create_devm_seqfile(hdmi->dev, "hdmi_reinit_state", hdmi->debugfs,
			hdmi_reinit_state);

}

static int axiado_hdmi_bind(struct device *dev, struct device *master, void *data)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct device_node *np = dev->of_node;
	struct device_node *disp_node_entry;
	struct drm_device *drm = data;
	struct axiado_hdmi *hdmi;
	struct resource *iores;
	struct resource res;
	int irq;
	int ret;

	hdmi = devm_kzalloc(dev, sizeof(*hdmi), GFP_KERNEL);
	if (!hdmi)
		return -ENOMEM;
	hdmi->dev = dev;
	hdmi->drm_dev = drm;
	iores = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	hdmi->regs = devm_ioremap_resource(dev, iores);
	if (IS_ERR(hdmi->regs))
		return PTR_ERR(hdmi->regs);

	/* Parsing the memory region for the data control path */
	disp_node_entry = of_parse_phandle(np, "memory-region", 0);
	if (!disp_node_entry) {
		pr_err("PCIe addr memory-region property not found\n");
		return -EINVAL;
	}

	if (of_address_to_resource(disp_node_entry, 1, &res)) {
		pr_err("%s: Cannot get memory resource from device tree\n",
		       __func__);
		return -EINVAL;
	}

	/* Memory mapping for the Control Path */
	hdmi->data_reg = devm_memremap(hdmi->dev, res.start, res.end - res.start, MEMREMAP_WB);
	if (!(hdmi->data_reg)) {
		pr_err("failed to map reserved buffer!\n");
		return PTR_ERR(hdmi->data_reg);
	}

	hdmi->pclk = devm_clk_get(hdmi->dev, "clk_pclk");
	if (IS_ERR(hdmi->pclk)) {
		dev_err(hdmi->dev, "Unable to get HDMI pixel clock\n");
		return PTR_ERR(hdmi->pclk);
	}
	ret = clk_prepare_enable(hdmi->pclk);
	if (ret) {
		dev_err(hdmi->dev, "Cannot enable HDMI pixel clock: %d\n",
			      ret);
		return ret;
	}

	hdmi->ax_hdmi_pwren =
		devm_gpiod_get(hdmi->dev, "pwren", GPIOD_OUT_HIGH);
	if (IS_ERR(hdmi->ax_hdmi_pwren))
		pr_err("Could not get pwren gpio\n");

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		ret = irq;
		goto err_disable_clk;
	}
	axiado_hdmi_reset(hdmi);
	hdmi->ddc = axiado_hdmi_i2c_adapter(hdmi);
	if (IS_ERR(hdmi->ddc)) {
		ret = PTR_ERR(hdmi->ddc);
		hdmi->ddc = NULL;
		goto err_disable_clk;
	}
	hdmi->tmds_rate = clk_get_rate(hdmi->pclk);
	axiado_hdmi_i2c_init(hdmi);
	ret = axiado_hdmi_register(drm, hdmi);
	if (ret)
		goto err_put_adapter;
	dev_set_drvdata(dev, hdmi);
	hdmi_modb(hdmi, HDMI_STATUS, m_MASK_INT_HOTPLUG, v_MASK_INT_HOTPLUG(1));

	ret = devm_request_threaded_irq(dev, irq, axiado_hdmi_hardirq,
					axiado_hdmi_irq, IRQF_SHARED,
					dev_name(dev), hdmi);
	if (ret < 0)
		goto err_cleanup_hdmi;

	if (IS_ENABLED(CONFIG_DEBUG_FS))
		ax_hdmi_debugfs_init(hdmi);

	return 0;
err_cleanup_hdmi:
	hdmi->connector.funcs->destroy(&hdmi->connector);
	hdmi->encoder.funcs->destroy(&hdmi->encoder);
err_put_adapter:
	i2c_put_adapter(hdmi->ddc);
err_disable_clk:
	clk_disable_unprepare(hdmi->pclk);
	return ret;
}

static void axiado_hdmi_unbind(struct device *dev, struct device *master, void *data)
{
	struct axiado_hdmi *hdmi = dev_get_drvdata(dev);

	if (IS_ENABLED(CONFIG_DEBUG_FS))
		ax_hdmi_debugfs_exit(hdmi);
	hdmi->connector.funcs->destroy(&hdmi->connector);
	hdmi->encoder.funcs->destroy(&hdmi->encoder);
	i2c_put_adapter(hdmi->ddc);
	clk_disable_unprepare(hdmi->pclk);
}

static const struct component_ops axiado_hdmi_ops = {
	.bind = axiado_hdmi_bind,
	.unbind = axiado_hdmi_unbind,
};

static int axiado_hdmi_probe(struct platform_device *pdev)
{
	if (!pdev)
		return -EINVAL;
	return component_add(&pdev->dev, &axiado_hdmi_ops);
}

static int axiado_hdmi_remove(struct platform_device *pdev)
{
	if (!pdev)
		return -EINVAL;
	component_del(&pdev->dev, &axiado_hdmi_ops);
	return 0;
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

MODULE_AUTHOR("AXIADO CORPORATION");
MODULE_DESCRIPTION("Axiado HDMI TX Driver");
MODULE_LICENSE("GPL");
