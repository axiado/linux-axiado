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
 * @chip: pwm_chip core structure
 * @hwaddr: hardware base address (optional)
 * @clk: PWM clock reference
 * @base: MMIO base address of PWM registers
 * @pwms: Array of PWM channel info
 */
struct axiado_pwm_chip {
	struct pwm_chip chip;
	u32 hwaddr;
	struct clk *clk;
	u8 __iomem *base;
	struct axiado_pwm pwms[AX_PWM_NUM];
};

/*
 * axiado_pwm_get - Retrieve axiado_pwm_chip container from pwm_chip pointer
 * @chip: pointer to pwm_chip struct
 *
 * Return: pointer to containing axiado_pwm_chip structure
 */
static inline struct axiado_pwm_chip *axiado_pwm_get(struct pwm_chip *chip)
{
	return container_of(chip, struct axiado_pwm_chip, chip);
}

/*
 * axiado_pwm_disable - Disable PWM output on a given channel
 * @chip: PWM chip pointer
 * @pwm: PWM device representing the channel
 *
 * Writes zero to the control register of the specified channel to
 * stop PWM output.
 */
static void axiado_pwm_disable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct axiado_pwm_chip *axpwm = axiado_pwm_get(chip);

	pr_debug("axiado: PWM: Disabling PWM\n");
	iowrite32(PWM_CTRL_DISABLE, (axpwm->base + AX_PWM_CNTRL));
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

	dev_dbg(chip->dev, "configuring %p/%s, %d/%d ns\n", axpwm,
		pwm->label, duty_ns, period_ns);
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

	if (state->polarity != PWM_POLARITY_NORMAL)
		return -EINVAL;

	if (!state->enabled) {
		if (pwm->state.enabled)
			axiado_pwm_disable(chip, pwm);

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
	.owner = THIS_MODULE,
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
	struct axiado_pwm_chip *axpwm;
	struct resource *r_mem;
	int ret;

	axpwm = devm_kzalloc(&pdev->dev, sizeof(*axpwm), GFP_KERNEL);
	if (!axpwm)
		return -ENOMEM;

	r_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	axpwm->base = devm_ioremap_resource(&pdev->dev, r_mem);
	if (IS_ERR(axpwm->base)) {
		dev_err(&pdev->dev, "Failed to map registers\n");
		return PTR_ERR(axpwm->base);
	}

	axpwm->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(axpwm->clk)) {
		dev_err(&pdev->dev, "Failed to get clock\n");
		return PTR_ERR(axpwm->clk);
	}

	ret = clk_prepare_enable(axpwm->clk);
	if (ret) {
		dev_err(&pdev->dev, "Failed to enable clock: %d\n", ret);
		return ret;
	}

	axpwm->chip.dev = &pdev->dev;
	axpwm->chip.ops = &axiado_pwm_ops;
	axpwm->chip.npwm = AX_PWM_NUM;
	axpwm->chip.base = -1;

	ret = pwmchip_add(&axpwm->chip);
	if (ret) {
		dev_err(&pdev->dev, "Failed to add PWM chip: %d\n", ret);
		clk_disable_unprepare(axpwm->clk);
		return ret;
	}

	platform_set_drvdata(pdev, axpwm);

	return 0;
}

/*
 * axiado_pwm_remove - Platform driver remove callback
 * @pdev: Platform device pointer
 *
 * Unregisters the PWM chip and disables the clock.
 *
 * Return: always 0
 */
static int axiado_pwm_remove(struct platform_device *pdev)
{
	struct axiado_pwm_chip *axpwm = platform_get_drvdata(pdev);

	pwmchip_remove(&axpwm->chip);
	clk_disable_unprepare(axpwm->clk);


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
	.remove = axiado_pwm_remove,
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
