// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * ax-pwm-fan.c - Hwmon driver for fans connected to PWM lines and tach inputs.
 *
 * Copyright (C) 2022-2023,2025 Axiado Corporation.
 */

#include <linux/hwmon.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/sysfs.h>

#define MAX_PWM 255
#define TACH_REG_SIZE 0x400

/* TACH Register offsets */
#define AX_TACH_CNTRL       0x00
#define AX_TACH_TIMER_CNT   0x04
#define AX_TACH_COUNT       0x08
#define AX_TACH_INTR_STS    0x0C

#define AX_TACH_CLEAR_INTR  0x1
#define AX_TACH_EN          0x3
#define AX_TACH_CNT         0x7735940 /* Timer set to 1s */

/**
 * struct axiado_pwm_fan_tach - Tachometer capture state
 * @irq: IRQ line used for tach capture
 * @pulses_per_revolution: pulses per mechanical revolution
 * @count: last-latched pulse count sampled by the ISR
 *
 * Holds per-device tachometer metadata and the last sampled count
 * updated from the interrupt handler.  The count is read under
 * @tach_lock spin lock to synchronize ISR and readers.
 */
struct axiado_pwm_fan_tach {
	int irq;
	u32 pulses_per_revolution;
	u32 count;
};

/**
 * struct axiado_pwm_fan_ctx - Device context for PWM fan + tach
 * @tach_lock: serializes PWM state changes and value updates
 * @pwm: PWM device obtained from the PWM framework
 * @pwm_state: cached PWM state used with pwm_apply_might_sleep()
 * @tach_base: I/O base for tach registers
 * @tach: tach sub-structure with IRQ and pulse info
 * @pwm_value: cached 0..255 user-scale duty value
 *
 */
struct axiado_pwm_fan_ctx {
	spinlock_t tach_lock;
	struct pwm_device *pwm;
	struct pwm_state pwm_state;
	void __iomem *tach_base;
	struct axiado_pwm_fan_tach tach;
	unsigned int pwm_value;
};

/* HWMON channel declarations (pwm1 and fan1_input are exposed). */
static const u32 pwm_fan_pwm_config[] = { HWMON_PWM_INPUT, 0 };
static const u32 pwm_fan_fan_config[] = { HWMON_F_INPUT, 0 };

static const struct hwmon_channel_info pwm_fan_channel_pwm = {
	.type = hwmon_pwm,
	.config = pwm_fan_pwm_config,
};

static const struct hwmon_channel_info pwm_fan_channel_fan = {
	.type = hwmon_fan,
	.config = pwm_fan_fan_config,
};

static const struct hwmon_channel_info *pwm_fan_info[] = {
	&pwm_fan_channel_pwm,
	&pwm_fan_channel_fan,
	NULL
};

/**
 * axiado_tach_irq_handler() - Tachometer IRQ handler
 * @irq: IRQ number
 * @dev: driver context pointer
 *
 * Context: Hard IRQ context.  Must not sleep.
 * Return: IRQ_HANDLED on success.
 */

static irqreturn_t axiado_tach_irq_handler(int irq, void *dev)
{
	struct axiado_pwm_fan_ctx *ctx = dev;
	unsigned long flags;
	u32 val;

	val = ioread32(ctx->tach_base + AX_TACH_INTR_STS);

	if (!val)
		return IRQ_NONE;

	spin_lock_irqsave(&ctx->tach_lock, flags);
	ctx->tach.count = ioread32(ctx->tach_base + AX_TACH_COUNT);
	spin_unlock_irqrestore(&ctx->tach_lock, flags);

	iowrite32(AX_TACH_CLEAR_INTR, ctx->tach_base + AX_TACH_INTR_STS);

	return IRQ_HANDLED;
}

/**
 * axiado_set_pwm() - Set fan speed in 0..255 scale
 * @ctx: device context
 * @pwm: desired user-scale PWM value (0..255)
 *
 * Converts a user-scale duty value to nanoseconds using the current
 * cached @pwm_state.period and calls pwm_apply_might_sleep() to update
 * duty and enabled state atomically.  Reuses cached state to minimize
 * register traffic and preserves semantics of duty=0 vs disabled.
 *
 * Context: May sleep.  Serialized by @ctx->lock.
 * Return: 0 on success or negative errno from pwm_apply_might_sleep().
 */
static int  axiado_set_pwm(struct axiado_pwm_fan_ctx *ctx, u64 pwm)
{
	u64 period, duty_cycle;
	int ret = 0;
	unsigned long flags;
	struct pwm_state *state = &ctx->pwm_state;

	spin_lock_irqsave(&ctx->tach_lock, flags);
	if (ctx->pwm_value == pwm)
		goto exit_set_pwm_err;

	period = state->period;
	duty_cycle = DIV_ROUND_UP((u64)pwm * (period - 1), MAX_PWM);
	state->duty_cycle = duty_cycle;
	state->enabled = pwm ? true : false;

	ret = pwm_apply_might_sleep(ctx->pwm, state);
	if (!ret)
		ctx->pwm_value = pwm;
exit_set_pwm_err:
	spin_unlock_irqrestore(&ctx->tach_lock, flags);
	return ret;
}

/**
 * axiado_fan_write() - HWMON write callback
 * @dev: hwmon device
 * @type: sensor type (hwmon_pwm)
 * @attr: attribute selector
 * @channel: channel index (0 for pwm1)
 * @val: new value
 *
 * Return: 0 on success or negative errno.
 */
static int axiado_fan_write(struct device *dev, enum hwmon_sensor_types type,
		u32 attr, int channel, long val)
{
	struct axiado_pwm_fan_ctx *ctx = dev_get_drvdata(dev);
	int ret;

	if (val < 0 || val > MAX_PWM)
		return -EINVAL;

	ret = axiado_set_pwm(ctx, val);
	if (ret)
		return ret;
	return 0;
}

/**
 * pwm_get_rpm() - Convert pulses to RPM
 * @ctx: device context
 *
 * Return: computed RPM.
 */

static unsigned int pwm_get_rpm(struct axiado_pwm_fan_ctx *ctx)
{
	u64 pulses, rpm;
	u32 pulses_per_revolution;
	unsigned long flags;

	spin_lock_irqsave(&ctx->tach_lock, flags);
	pulses = ctx->tach.count;
	pulses_per_revolution = ctx->tach.pulses_per_revolution;
	spin_unlock_irqrestore(&ctx->tach_lock, flags);

	if (!pulses_per_revolution)
		return 0;

	/* The timer is set to 1s, the number of pulses per second can
	 * be converted to pulses/min by pulses*60. The pulses per
	 * rotation is 2 if not overridden by dts entry,So rpm=(pulses*60)/PPR.
	 */
	rpm = (pulses * 60);
	do_div(rpm, pulses_per_revolution);

	return rpm;
}

/**
 * axiado_pwm_fan_read() - HWMON read callback
 * @dev: hwmon device
 * @type: sensor type (hwmon_pwm or hwmon_fan)
 * @attr: attribute selector
 * @channel: channel index
 * @val: out parameter to store result
 *
 * Returns current user-scale PWM value or computed RPM.
 *
 * Return: 0 on success or -EOPNOTSUPP for unknown attrs.
 */

static int axiado_pwm_fan_read(struct device *dev, enum hwmon_sensor_types type,
		u32 attr, int channel, long *val)
{
	struct axiado_pwm_fan_ctx *ctx = dev_get_drvdata(dev);
	unsigned long flags;
	unsigned int rpm;

	switch (type) {
	case hwmon_pwm:
		spin_lock_irqsave(&ctx->tach_lock, flags);
		*val = ctx->pwm_value;
		spin_unlock_irqrestore(&ctx->tach_lock, flags);
		return 0;

	case hwmon_fan:
		rpm = pwm_get_rpm(ctx);
		*val = rpm;
		return 0;

	default:
		return -EOPNOTSUPP;
	}
}

/**
 * axiado_pwm_fan_is_visible() - HWMON permission callback
 * @data: driver data (unused)
 * @type: sensor type
 * @attr: attribute selector
 * @channel: channel index (0)
 *
 * Exposes pwm1 read/write and fan1_input read-only in sysfs.
 *
 * Return: 0644 for pwm1, 0444 for fan1_input, else 0.
 */

static umode_t axiado_pwm_fan_is_visible(const void *data,
		enum hwmon_sensor_types type,
		u32 attr, int channel)
{
	switch (type) {
	case hwmon_pwm:
		return 0644;

	case hwmon_fan:
		return 0444;

	default:
		return 0;
	}
}

static const struct hwmon_ops pwm_fan_hwmon_ops = {
	.is_visible = axiado_pwm_fan_is_visible,
	.read = axiado_pwm_fan_read,
	.write = axiado_fan_write,
};
static const struct hwmon_chip_info pwm_fan_chip_info = {
	.ops  = &pwm_fan_hwmon_ops,
	.info = pwm_fan_info,
};


/**
 * pwm_fan_pwm_disable() - devm action to disable PWM on teardown
 * @__ctx: device context
 *
 * Disables the PWM using the cached state during device-managed
 * paths.
 *
 */
static void pwm_fan_pwm_disable(void *__ctx)
{
	struct axiado_pwm_fan_ctx *ctx = __ctx;

	ctx->pwm_state.enabled = false;
	pwm_apply_might_sleep(ctx->pwm, &ctx->pwm_state);
}

/**
 * axiado_pwm_fan_probe() - Probe a PWM fan + tach instance
 * @pdev: platform device
 *
 * Maps tach registers, acquires the PWM, initializes cached state,
 * enforces a safe period bound for 0..255 scaling, sets an initial
 * maximum duty to start airflow, registers the hwmon device, and
 * enables the tach block/IRQ.
 *
 * Context: Process context.  May sleep.
 * Return: 0 on success or negative errno.
 */

static int axiado_pwm_fan_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct axiado_pwm_fan_ctx *ctx;
	int ret;
	struct resource *res;


	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	spin_lock_init(&ctx->tach_lock);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	ctx->tach_base = devm_ioremap_resource(dev, res);
	if (IS_ERR(ctx->tach_base))
		return PTR_ERR(ctx->tach_base);
	ctx->pwm = devm_pwm_get(dev, NULL);
	if (IS_ERR(ctx->pwm))
		return dev_err_probe(dev, PTR_ERR(ctx->pwm), "Could not get PWM\n");

	platform_set_drvdata(pdev, ctx);

	pwm_init_state(ctx->pwm, &ctx->pwm_state);

	/*
	 * axiado_set_pwm assumes that MAX_PWM * (period - 1) fits into an unsigned
	 * long. Check this here to prevent the fan running at a too low
	 * frequency.
	 */
	if (ctx->pwm_state.period > ULONG_MAX / MAX_PWM + 1) {
		dev_err(dev, "Configured period too big\n");
		return -EINVAL;
	}

	/* Set duty cycle to maximum allowed and enable PWM output */
	ret = axiado_set_pwm(ctx, MAX_PWM);
	if (ret) {
		dev_err(dev, "Failed to configure PWM: %d\n", ret);
		return ret;
	}
	ret = devm_add_action_or_reset(dev, pwm_fan_pwm_disable, ctx);
	if (ret)
		return ret;

	/* Always one tachometer per fan device */
	ctx->tach.irq = platform_get_irq(pdev, 0);
	if (ctx->tach.irq < 0)
		return dev_err_probe(dev, ctx->tach.irq,
				"Failed to get tachometer IRQ\n");

	ret = devm_request_irq(dev, ctx->tach.irq, axiado_tach_irq_handler, 0,
			dev_name(dev), ctx);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to request tach IRQ\n");

	/*
	 * Default pulses-per-revolution = 2
	 * (Can be overridden from DT if "pulses-per-revolution" property exists)
	 */
	ctx->tach.pulses_per_revolution = 2;
	of_property_read_u32(dev->of_node, "pulses-per-revolution",
			&ctx->tach.pulses_per_revolution);

	if (!ctx->tach.pulses_per_revolution) {
		dev_err(dev, "pulses-per-revolution cannot be zero\n");
		return -EINVAL;
	}

	dev_dbg(dev, "Fan tachometer: irq=%d, pulses_per_revolution=%u\n",
			ctx->tach.irq, ctx->tach.pulses_per_revolution);

	/* Register with HWMON subsystem */
	ret = PTR_ERR_OR_ZERO(devm_hwmon_device_register_with_info(dev,
			"axpwmfan", ctx, &pwm_fan_chip_info, NULL));
	if (ret)
		return ret;
	dev_dbg(dev, "Axiado PWM fan controller initialized (HWMON)\n");

	iowrite32(AX_TACH_CNT, ctx->tach_base + AX_TACH_TIMER_CNT);
	iowrite32(AX_TACH_EN, ctx->tach_base + AX_TACH_CNTRL);

	return 0;
}

/**
 * pwm_fan_disable() - Helper to disable PWM output
 * @dev: device
 *
 * Disables the PWM cleanly while preserving @ctx->pwm_state for a
 * later resume that restores the previous duty/enable state.
 *
 * Return: 0 on success or negative errno.
 */

static int pwm_fan_disable(struct device *dev)
{
	struct axiado_pwm_fan_ctx *ctx = dev_get_drvdata(dev);
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&ctx->tach_lock, flags);
	if (ctx->pwm_value) {
		/* keep ctx->pwm_state unmodified for pwm_fan_resume() */
		struct pwm_state state = ctx->pwm_state;

		state.duty_cycle = 0;
		state.enabled = false;
		ret = pwm_apply_might_sleep(ctx->pwm, &state);
		if (ret < 0)
			return ret;
	}
	spin_unlock_irqrestore(&ctx->tach_lock, flags);
	return 0;
}

/**
 * axiado_pwm_fan_shutdown() - Platform shutdown callback
 * @pdev: platform device
 *
 * Pause the PWM output during shutdown to avoid uncontrolled fan
 * behavior across reboot/poweroff transitions.
 */

static void axiado_pwm_fan_shutdown(struct platform_device *pdev)
{
	pwm_fan_disable(&pdev->dev);
}

#ifdef CONFIG_PM_SLEEP
/**
 * pwm_fan_suspend() - System suspend callback
 * @dev: device
 *
 * Disables PWM before suspend; tach capture is left as-is.
 *
 * Return: 0 on success or negative errno.
 */

static int pwm_fan_suspend(struct device *dev)
{
	return pwm_fan_disable(dev);
}

/**
 * pwm_fan_resume() - System resume callback
 * @dev: device
 *
 * Restores the previously cached PWM state if @pwm_value != 0.
 *
 * Return: 0 on success or negative errno.
 */

static int pwm_fan_resume(struct device *dev)
{
	struct axiado_pwm_fan_ctx *ctx = dev_get_drvdata(dev);
	int ret = 0;
	unsigned long flags;

	spin_lock_irqsave(&ctx->tach_lock, flags);
	if (ctx->pwm_value == 0)
		goto out;

	ret = pwm_apply_might_sleep(ctx->pwm, &ctx->pwm_state);

out:
	spin_unlock_irqrestore(&ctx->tach_lock, flags);
	return ret;
}
#endif

static SIMPLE_DEV_PM_OPS(pwm_fan_pm, pwm_fan_suspend, pwm_fan_resume);

static const struct of_device_id of_pwm_fan_match[] = {
	{ .compatible = "axiado,ax3000-pwm-fan", },
	{},
};
MODULE_DEVICE_TABLE(of, of_pwm_fan_match);

static struct platform_driver pwm_fan_driver = {
	.probe		= axiado_pwm_fan_probe,
	.shutdown	= axiado_pwm_fan_shutdown,
	.driver	= {
		.name		= "ax3000-pwm-fan",
		.pm		= &pwm_fan_pm,
		.of_match_table	= of_pwm_fan_match,
	},
};

module_platform_driver(pwm_fan_driver);

MODULE_AUTHOR("AXIADO CORPORATION");
MODULE_DESCRIPTION("AXIADO PWM Fan driver");
MODULE_LICENSE("GPL");
