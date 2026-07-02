// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Driver for Axiado Pulse Width Modulation Controller
 *
 * Copyright (c) 2021-25 Axiado Corporation.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/pwm.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/clk.h>
#include <linux/types.h>

/* PWM Register offsets */
#define AX_PWM_CNTRL     0x0000
#define AX_PWM_PERIOD    0x0004
#define AX_PWM_HIGH      0x0008

/* PWM Channels */
#define AX_PWM_NUM 0x10

/* Period and Dutycycle Range */
#define PERIOD_MIN_VAL (-2)
#define PERIOD_MAX_VAL 0xFFFFFFFE
#define DUTYCYCLE_MIN_VAL (-1)
#define DUTYCYCLE_MAX_VAL 0xFFFFFFFD

/* Control Register Bits */
#define PWM_CTRL_ENABLE 0x1
#define PWM_CTRL_DISABLE 0x0

/*
 * struct axiado_pwm - PWM channel properties
 * @clk_hz: Clock frequency driving the PWM
 * @source: Source information (device-specific usage)
 */
struct axiado_pwm {
	u32 clk_hz;
	u32 source;
};

/*
 * struct axiado_pwm_chip - PWM chip data
 * @hwaddr: hardware base address (optional)
 * @clk: PWM clock reference
 * @clk_rate: cached PWM clock rate in Hz
 * @base: MMIO base address of PWM registers
 * @pwms: Array of PWM channel info
 */
struct axiado_pwm_chip {
	u32 hwaddr;
	struct clk *clk;
	unsigned long clk_rate;
	u8 __iomem *base;
	struct axiado_pwm pwms[AX_PWM_NUM];
};

/*
 * axiado_pwm_get - Retrieve axiado_pwm_chip private data from pwm_chip pointer
 * @chip: pointer to pwm_chip struct
 *
 * Return: pointer to the driver's axiado_pwm_chip private data
 */
static inline struct axiado_pwm_chip *axiado_pwm_get(struct pwm_chip *chip)
{
	return pwmchip_get_drvdata(chip);
}

/*
 * axiado_pwm_config - Configure duty cycle and period for a PWM channel
 * @chip: PWM chip pointer
 * @pwm: PWM device representing the channel
 * @duty_ns: Requested duty cycle in nanoseconds
 * @period_ns: Requested period in nanoseconds
 *
 * Validates parameters, then programs the hardware registers with
 * the period and duty cycle values.
 *
 * Return: 0 on success, negative errno on invalid input
 */
static int axiado_pwm_config(struct pwm_chip *chip, struct pwm_device *pwm,
			int duty_ns, int period_ns)
{
	struct axiado_pwm_chip *axpwm = axiado_pwm_get(chip);

	if ((period_ns <= PERIOD_MIN_VAL) || /* Checking period minimum value */
	    (period_ns >= PERIOD_MAX_VAL) || /* Checking period maximum value */
	    (duty_ns <=
	     DUTYCYCLE_MIN_VAL) || /* Checking dutycycle minimum value */
	    (duty_ns >=
	     DUTYCYCLE_MAX_VAL)) { /* Checking dutycyle maximum value */
		pr_err("axiado: PWM: Invalid period or dutycycle values\n");
		return -EINVAL;
	}

	/* Setting period */
	iowrite32(period_ns, (axpwm->base + AX_PWM_PERIOD));
	/* Setting duty cycle */
	iowrite32(duty_ns, (axpwm->base + AX_PWM_HIGH));
	return 0;
}

/*
 * axiado_pwm_enable - Enable PWM output on a given channel
 * @chip: PWM chip pointer
 * @pwm: PWM device representing the channel
 *
 * Writes to the control register to start PWM output.
 *
 * Return: always 0
 */
static int axiado_pwm_enable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct axiado_pwm_chip *axpwm = axiado_pwm_get(chip);

	pr_debug("axiado: PWM: Enabling PWM\n");
	iowrite32(PWM_CTRL_ENABLE, (axpwm->base + AX_PWM_CNTRL));
	return 0;
}

/*
 * axiado_pwm_apply - Apply a PWM state change request
 * @chip: PWM chip pointer
 * @pwm: PWM device representing the channel
 * @state: Desired PWM state
 *
 * Validates the requested state, disables PWM if disabled,
 * otherwise configures and enables PWM output.
 *
 * Return: 0 on success, negative errno on failure
 */
static int axiado_pwm_apply(struct pwm_chip *chip, struct pwm_device *pwm,
			const struct pwm_state *state)
{
	int err;
	struct axiado_pwm_chip *axpwm = pwmchip_get_drvdata(chip);

	if (state->polarity != PWM_POLARITY_NORMAL)
		return -EINVAL;

	if (!state->enabled) {
		if (pwm->state.enabled)
			iowrite32(PWM_CTRL_DISABLE, (axpwm->base + AX_PWM_CNTRL));
		return 0;
	}

	err = axiado_pwm_config(chip, pwm, state->duty_cycle, state->period);
	if (err)
		return err;

	if (!pwm->state.enabled)
		err = axiado_pwm_enable(chip, pwm);

	return err;
}

/* PWM operations struct */
static const struct pwm_ops axiado_pwm_ops = {
	.apply = axiado_pwm_apply,
};

/*
 * axiado_pwm_probe - Platform driver probe callback
 * @pdev: Platform device pointer
 *
 * Allocates driver data, maps registers, obtains clock,
 * enables clock, and registers the PWM chip with the PWM subsystem.
 *
 * Return: 0 on success, error code on failure
 */
static int axiado_pwm_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct axiado_pwm_chip *axpwm;
	struct pwm_chip *chip;
	int ret;

	chip = devm_pwmchip_alloc(dev, AX_PWM_NUM, sizeof(*axpwm));
	if (IS_ERR(chip))
		return PTR_ERR(chip);

	axpwm = pwmchip_get_drvdata(chip);

	axpwm->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(axpwm->base))
		return dev_err_probe(dev, PTR_ERR(axpwm->base),
				     "Failed to map register\n");

	axpwm->clk = devm_clk_get_enabled(dev, NULL);
	if (IS_ERR(axpwm->clk))
		return dev_err_probe(dev, PTR_ERR(axpwm->clk),
				     "Failed to get/enable clock\n");

	/*
	 * Lock the clock rate so it cannot change while the PWM is in use,
	 * which lets us cache it and run the apply callback atomically.
	 */
	ret = devm_clk_rate_exclusive_get(dev, axpwm->clk);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to lock clock rate\n");

	axpwm->clk_rate = clk_get_rate(axpwm->clk);

	chip->ops = &axiado_pwm_ops;
	chip->atomic = true;

	ret = devm_pwmchip_add(dev, chip);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to add PWM chip\n");

	return 0;
}

/* Device Tree match table */
static const struct of_device_id axiado_pwm_of_match[] = {
	{ .compatible = "axiado,ax3000-pwm" },
	{},
};
MODULE_DEVICE_TABLE(of, axiado_pwm_of_match);

/* Platform driver struct */
static struct platform_driver axiado_pwm_driver = {
	.driver = {
		.name = "axiado_pwm",
		.of_match_table = axiado_pwm_of_match,
	},
	.probe = axiado_pwm_probe,
};

static int __init axiado_pwm_init(void)
{
	return platform_driver_register(&axiado_pwm_driver);
}

static void __exit axiado_pwm_exit(void)
{
	platform_driver_unregister(&axiado_pwm_driver);
}

module_init(axiado_pwm_init);
module_exit(axiado_pwm_exit);

MODULE_DESCRIPTION("PWM driver for axiado");
MODULE_AUTHOR("AXIADO CORPORATION");
MODULE_LICENSE("GPL");
