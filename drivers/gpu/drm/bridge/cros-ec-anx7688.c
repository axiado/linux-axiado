// SPDX-License-Identifier: GPL-2.0-only
/*
 * CrOS EC ANX7688 HDMI->DP bridge driver
 *
 * Copyright 2020 Google LLC
 * Copyright 2026 Axiado Corporation
 */

#include <drm/drm_bridge.h>
#include <drm/drm_print.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/types.h>

/* Register addresses */
#define ANX7688_VENDOR_ID_REG		0x00
#define ANX7688_DEVICE_ID_REG		0x02

#define ANX7688_FW_VERSION_REG		0x80

#define ANX7688_DP_BANDWIDTH_REG	0x85
#define ANX7688_DP_LANE_COUNT_REG	0x86

#define ANX7688_VENDOR_ID		0x1f29
#define ANX7688_VENDOR_ID2		0xaaaa
#define ANX7688_DEVICE_ID		0x7688

/* First supported firmware version (0.85) */
#define ANX7688_MINIMUM_FW_VERSION	0x0085

/* Axiado platform specific register addresses */
#define ANX7688_I2C_ADDRESS1		0x2c
#define ANX7688_I2C_ADDRESS2		0x38

#define ANX7688_HPD_REG		0x82
#define ANX7688_CTRL_REG2	0x83
#define ANX7688_STATUS_REG1	0x12
#define ANX7688_STATUS_REG2	0x62

#define ANX7688_HPD_REG_VALUE		0x30
#define ANX7688_CTRL_REG2_VALUE	0x01
#define ANX7688_REG_TH_VALUE1	0x01
#define ANX7688_REG_TH_VALUE2	0x04

#define MAX_DELAY_COUNT 15

static const struct regmap_config cros_ec_anx7688_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
};

static const struct  regmap_config secondary_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
};

static const struct  regmap_config tertiary_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
};

struct cros_ec_anx7688 {
	struct i2c_client *client;
	struct regmap *regmap;
	struct i2c_client *client_sec;
	struct regmap *regmap_sec;
	struct i2c_client *client_ter;
	struct regmap *regmap_ter;
	struct drm_bridge bridge;
	struct gpio_desc *anx_reset;
	struct gpio_desc *anx_pwren;
	bool filter;

};

static inline struct cros_ec_anx7688 *
bridge_to_cros_ec_anx7688(struct drm_bridge *bridge)
{
	return container_of(bridge, struct cros_ec_anx7688, bridge);
}

static bool cros_ec_anx7688_bridge_mode_fixup(struct drm_bridge *bridge,
					      const struct drm_display_mode *mode,
					      struct drm_display_mode *adjusted_mode)
{
	struct cros_ec_anx7688 *anx = bridge_to_cros_ec_anx7688(bridge);
	int totalbw, requiredbw;
	u8 dpbw, lanecount;
	u8 regs[2];
	int ret;

	if (!anx->filter)
		return true;

	/* Read both regs 0x85 (bandwidth) and 0x86 (lane count). */
	ret = regmap_bulk_read(anx->regmap, ANX7688_DP_BANDWIDTH_REG, regs, 2);
	if (ret < 0) {
		DRM_ERROR("Failed to read bandwidth/lane count\n");
		return false;
	}
	dpbw = regs[0];
	lanecount = regs[1];

	/* Maximum 0x19 bandwidth (6.75 Gbps Turbo mode), 2 lanes */
	if (dpbw > 0x19 || lanecount > 2) {
		DRM_ERROR("Invalid bandwidth/lane count (%02x/%d)\n", dpbw,
			  lanecount);
		return false;
	}

	/* Compute available bandwidth (kHz) */
	totalbw = dpbw * lanecount * 270000 * 8 / 10;

	/* Required bandwidth (8 bpc, kHz) */
	requiredbw = mode->clock * 8 * 3;

	DRM_DEBUG_KMS("DP bandwidth: %d kHz (%02x/%d); mode requires %d Khz\n",
		      totalbw, dpbw, lanecount, requiredbw);

	if (totalbw == 0) {
		DRM_ERROR("Bandwidth/lane count are 0, not rejecting modes\n");
		return true;
	}

	return totalbw >= requiredbw;
}

static const struct drm_bridge_funcs cros_ec_anx7688_bridge_funcs = {
	.mode_fixup = cros_ec_anx7688_bridge_mode_fixup,
};

static int cros_ec_anx7688_bridge_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct device_node *node = dev->of_node;
	struct cros_ec_anx7688 *anx7688;
	u32 data;
	u16 vendor, device, fw_version, max_count = MAX_DELAY_COUNT;
	u8 buffer[4];
	int ret;
	u32 hpd = 0;

	anx7688 = devm_kzalloc(dev, sizeof(*anx7688), GFP_KERNEL);
	if (!anx7688)
		return -ENOMEM;

	anx7688->client = client;
	i2c_set_clientdata(client, anx7688);

	anx7688->anx_pwren =
		devm_gpiod_get(&client->dev, "pwren", GPIOD_OUT_HIGH);
	if (IS_ERR(anx7688->anx_pwren))
		dev_err(dev, "Could not get pwren gpio\n");

	usleep_range(30000, 31000);

	anx7688->anx_reset =
		devm_gpiod_get(&client->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(anx7688->anx_reset))
		dev_err(dev, "Could not get reset gpio\n");

	gpiod_set_value(anx7688->anx_reset, 0);
	usleep_range(20000, 21000);
	gpiod_set_value(anx7688->anx_pwren, 0);
	usleep_range(30000, 31000);

	gpiod_set_value(anx7688->anx_pwren, 1);
	usleep_range(20000, 21000);
	gpiod_set_value(anx7688->anx_reset, 1);

	anx7688->regmap = devm_regmap_init_i2c(client, &cros_ec_anx7688_regmap_config);
	if (IS_ERR(anx7688->regmap)) {
		ret = PTR_ERR(anx7688->regmap);
		dev_err(dev, "regmap i2c init failed: %d\n", ret);
		return ret;
	}

	anx7688->client_sec = i2c_new_dummy_device(client->adapter,
						   ANX7688_I2C_ADDRESS1);
	if (IS_ERR(anx7688->client_sec)) {
		i2c_unregister_device(anx7688->client_sec);
		return PTR_ERR(anx7688->client_sec);
	}

	anx7688->regmap_sec = devm_regmap_init_i2c(anx7688->client_sec,
						   &secondary_regmap_config);
	if (IS_ERR(anx7688->regmap_sec)) {
		i2c_unregister_device(anx7688->client_sec);
		return PTR_ERR(anx7688->regmap_sec);
	}

	anx7688->client_ter = i2c_new_dummy_device(client->adapter,
						    ANX7688_I2C_ADDRESS2);
	if (IS_ERR(anx7688->client_ter)) {
		i2c_unregister_device(anx7688->client_ter);
		return PTR_ERR(anx7688->client_ter);
	}

	anx7688->regmap_ter = devm_regmap_init_i2c(anx7688->client_ter,
						    &tertiary_regmap_config);
	if (IS_ERR(anx7688->regmap_ter)) {
		i2c_unregister_device(anx7688->client_ter);
		return PTR_ERR(anx7688->regmap_ter);
	}
	/* Read both vendor and device id (4 bytes). */
	ret = regmap_bulk_read(anx7688->regmap, ANX7688_VENDOR_ID_REG,
			       buffer, 4);
	if (ret) {
		dev_err(dev, "Failed to read chip vendor/device id\n");
		return ret;
	}

	vendor = (u16)buffer[1] << 8 | buffer[0];
	device = (u16)buffer[3] << 8 | buffer[2];
	if ((vendor != ANX7688_VENDOR_ID && vendor != ANX7688_VENDOR_ID2) ||
	    device != ANX7688_DEVICE_ID) {
		dev_err(dev, "Invalid vendor/device id %04x/%04x\n",
			vendor, device);
		return -ENODEV;
	}

	ret = regmap_bulk_read(anx7688->regmap, ANX7688_FW_VERSION_REG,
			       buffer, 2);
	if (ret) {
		dev_err(dev, "Failed to read firmware version\n");
		return ret;
	}

	fw_version = (u16)buffer[0] << 8 | buffer[1];
	dev_info(dev, "ANX7688 firmware version 0x%04x\n", fw_version);

	ret = regmap_write(anx7688->regmap_sec, ANX7688_CTRL_REG2,
			   ANX7688_CTRL_REG2_VALUE);
	if (ret) {
		dev_err(dev, "Failed to write 0x1 in reg 0x83 of 0x2C\n");
		return ret;
	}

	msleep(500);
	do {
		regmap_read(anx7688->regmap, ANX7688_STATUS_REG2, &data);
		mdelay(1);
		dev_info(dev, "%s: Anx7688: 0x62=>%x\n", __func__, data);
	} while ((max_count-- > 0) && (data != ANX7688_REG_TH_VALUE2));

	ret = regmap_write(anx7688->regmap, ANX7688_STATUS_REG2, ANX7688_CTRL_REG2_VALUE);
	if (ret) {
		dev_err(dev, "Failed to write 0x1 in reg 0x62 of 0x28\n");
		return ret;
	}

	max_count = MAX_DELAY_COUNT;
	do {
		regmap_read(anx7688->regmap, ANX7688_STATUS_REG1, &data);
		mdelay(1);
		dev_info(dev, "Anx7688: 0x12=>%x:%x\n", data, (data & ANX7688_REG_TH_VALUE1));
	} while ((max_count-- > 0) && ((data & ANX7688_REG_TH_VALUE1) !=
					  ANX7688_REG_TH_VALUE1));
	ret = of_property_read_u32(node, "force-hpd", &hpd);
	if (ret == 0 && hpd == 1) {
		ret = regmap_write(anx7688->regmap_ter, ANX7688_HPD_REG,
				   ANX7688_HPD_REG_VALUE);
		if (ret) {
			dev_err(dev, "Failed to write 0x30 in reg 0x82 of 0x38\n");
			return ret;
		}
		ret = regmap_read(anx7688->regmap_ter, ANX7688_HPD_REG, &data);
		dev_info(dev, "After Force HPD Anx7688: %x\n", data);
	}

	anx7688->bridge.of_node = dev->of_node;

	/* FW version >= 0.85 supports bandwidth/lane count registers */
	if (fw_version >= ANX7688_MINIMUM_FW_VERSION)
		anx7688->filter = true;
	else
		/* Warn, but not fail, for backwards compatibility */
		DRM_WARN("Old ANX7688 FW version (0x%04x), not filtering\n",
			 fw_version);

	anx7688->bridge.funcs = &cros_ec_anx7688_bridge_funcs;
	drm_bridge_add(&anx7688->bridge);

	return 0;
}

static void cros_ec_anx7688_bridge_remove(struct i2c_client *client)
{
	struct cros_ec_anx7688 *anx7688 = i2c_get_clientdata(client);

	drm_bridge_remove(&anx7688->bridge);
}

static const struct of_device_id cros_ec_anx7688_bridge_match_table[] = {
	{ .compatible = "google,cros-ec-anx7688" },
	{ }
};
MODULE_DEVICE_TABLE(of, cros_ec_anx7688_bridge_match_table);

static struct i2c_driver cros_ec_anx7688_bridge_driver = {
	.probe = cros_ec_anx7688_bridge_probe,
	.remove = cros_ec_anx7688_bridge_remove,
	.driver = {
		.name = "cros-ec-anx7688-bridge",
		.of_match_table = cros_ec_anx7688_bridge_match_table,
	},
};

module_i2c_driver(cros_ec_anx7688_bridge_driver);

MODULE_DESCRIPTION("ChromeOS EC ANX7688 HDMI->DP bridge driver");
MODULE_AUTHOR("Nicolas Boichat <drinkcat@chromium.org>");
MODULE_AUTHOR("Enric Balletbo i Serra <enric.balletbo@collabora.com>");
MODULE_LICENSE("GPL");
