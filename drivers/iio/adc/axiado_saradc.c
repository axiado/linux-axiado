// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * axiado-saradc.c- File for Axiado Saradc driver
 * Copyright (c) 2021-2023,2025 Axiado Corporation.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/of_device.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>

/* Register offsets from SARADC base */
#define AX_SARADC_GLOBAL_CTRL 0x0004
#define AX_SARADC_MANUAL_CTRL 0x0008
#define AX_SARADC_DOUT        0x001C

/* Field definitions for GLOBAL_CTRL register */
#define AX_SARADC_CH_EN_SHIFT      16  /* Bits 31:16 channel enable */
#define AX_SARADC_SAMPLE_SHIFT      5  /* Bits 6:5 sample number */
#define AX_SARADC_MODE_SHIFT        3  /* Bits 4:3 mode */
#define AX_SARADC_PD                BIT(2)  /* Power down */
#define AX_SARADC_ENABLE            BIT(0)  /* ADC controller enable */

/* Channel enables: 16b mask for 16 single-ended inputs */
#define AX_SARADC_ALL_CHANNELS     0xFFFF
#define AX_SARADC_CH_EN_MASK(x)    ((u32)((x) << AX_SARADC_CH_EN_SHIFT))

/* Sample, mode fields */
#define AX_SARADC_SAMPLE_16        (0 << AX_SARADC_SAMPLE_SHIFT)
#define AX_SARADC_MODE             (1 << AX_SARADC_MODE_SHIFT)

#define AX_SARADC_MANUAL_CTRL_EN(n) ((2 * (n)) + 1)
#define AX_RESOLUTION_BITS          10
#define AX_VREF_FIXED               1100
#define AX_SARADC_MAX_CHANNELS      16
#define AX_SARADC_CONV_CYCLES       13

/**
 * struct axiado_saradc - private data structure for driver instance
 * @regs:      register map base address
 * @clk:       ADC reference clock handle
 * @clk_rate:  reference clock frequency
 * @vref:      Vref regulator handle
 * @lock:      mutex for serializing access
 */

struct axiado_saradc {
	void __iomem *regs;
	struct clk *clk;
	unsigned long clk_rate;
	struct regulator *vref;
	struct mutex lock;
};

/**
 * axiado_saradc_conversion() - trigger and read a polled ADC conversion
 * @info:   driver state pointer
 * @chan:   channel number [0..15]
 * val:     raw voltage
 *
 * Returns 0 on success, negative error code on failure.
 *
 * Starts manual conversion for channel chan, waits for 13 cycles as per
 * hardware spec, then reads the 10-bit result.
 */

static int axiado_saradc_conversion(struct axiado_saradc *info,
				    struct iio_chan_spec const *chan, int *val)
{
	unsigned long usecs;
	/* Select the channel to be used and trigger conversion */
	iowrite32(AX_SARADC_MANUAL_CTRL_EN(chan->channel),
			info->regs + AX_SARADC_MANUAL_CTRL);

	if (!info->clk_rate)
		return -EINVAL;

	/* Hardware requires 13 conversion cycles at clk_rate */
	usecs = DIV_ROUND_UP(AX_SARADC_CONV_CYCLES * 1000000, info->clk_rate);
	usleep_range(usecs, usecs + 10);

	*val = ioread32(info->regs + AX_SARADC_DOUT);

	/* Stop manual conversion */
	iowrite32(0, info->regs + AX_SARADC_MANUAL_CTRL);
	return 0;
}

/**
 * axiado_saradc_read_raw() - IIO callback for raw value or scale
 * @indio_dev:     dev pointer
 * @chan:          channel info
 * @val:           voltage val
 * @val2:          resolution bits
 * @mask:          mask for switch
 *
 * IIO callback for raw value or scale
 */

static int axiado_saradc_read_raw(struct iio_dev *indio_dev,
				  struct iio_chan_spec const *chan,
				  int *val, int *val2, long mask)
{
	struct axiado_saradc *info = iio_priv(indio_dev);
	int ret;

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		mutex_lock(&info->lock);
		ret = axiado_saradc_conversion(info, chan, val);
		mutex_unlock(&info->lock);
		return ret ? ret : IIO_VAL_INT;
	case IIO_CHAN_INFO_SCALE:
		/* 1.1V fixed Vref in millivolts, 10 bits resolution */
		*val = AX_VREF_FIXED;
		*val2 = AX_RESOLUTION_BITS;
		return IIO_VAL_FRACTIONAL_LOG2;

	default:
		return -EINVAL;
	}
}

static const struct iio_info axiado_saradc_iio_info = {
	.read_raw = axiado_saradc_read_raw,
};

#define AX_SARADC_CHANNEL(_index, _id) {                                       \
	.type = IIO_VOLTAGE,                                                   \
	.indexed = 1,                                                          \
	.channel = _index,                                                     \
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),                          \
	.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE),                  \
	.datasheet_name = _id, .scan_index = _index,                           \
	.scan_type = {                                                         \
		.sign = 'u',                                                   \
		.realbits = AX_RESOLUTION_BITS,                                \
		.storagebits = 32,                                             \
		.endianness = IIO_CPU,                                         \
	},                                                                     \
}

static const struct iio_chan_spec axiado_saradc_iio_channels[] = {
	AX_SARADC_CHANNEL(0, "adc0"),   AX_SARADC_CHANNEL(1, "adc1"),
	AX_SARADC_CHANNEL(2, "adc2"),   AX_SARADC_CHANNEL(3, "adc3"),
	AX_SARADC_CHANNEL(4, "adc4"),   AX_SARADC_CHANNEL(5, "adc5"),
	AX_SARADC_CHANNEL(6, "adc6"),   AX_SARADC_CHANNEL(7, "adc7"),
	AX_SARADC_CHANNEL(8, "adc8"),   AX_SARADC_CHANNEL(9, "adc9"),
	AX_SARADC_CHANNEL(10, "adc10"), AX_SARADC_CHANNEL(11, "adc11"),
	AX_SARADC_CHANNEL(12, "adc12"), AX_SARADC_CHANNEL(13, "adc13"),
	AX_SARADC_CHANNEL(14, "adc14"), AX_SARADC_CHANNEL(15, "adc15"),
};

/**
 * axiado_saradc_probe() - platform probe entrypoint
 * @pdev:      platform device instance
 *
 * Probes the AX3000 SARADC hardware, sets up resources/register map,
 * enables all channels and registers the IIO device.
 *
 * Returns 0 on success or negative error on failure.
 */
static int axiado_saradc_probe(struct platform_device *pdev)
{
	struct axiado_saradc *info = NULL;
	struct device_node *np = pdev->dev.of_node;
	struct iio_dev *indio_dev = NULL;
	struct resource *mem;
	int ret;
	u32 reg;

	if (!np)
		return -ENODEV;

	indio_dev = devm_iio_device_alloc(&pdev->dev, sizeof(*info));
	if (!indio_dev) {
		dev_err(&pdev->dev, "failed allocating iio device\n");
		return -ENOMEM;
	}
	info = iio_priv(indio_dev);

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem)
		return -ENODEV;

	info->regs = devm_ioremap_resource(&pdev->dev, mem);
	if (IS_ERR(info->regs))
		return PTR_ERR(info->regs);

	info->clk = devm_clk_get(&pdev->dev, "refclk");
	if (IS_ERR(info->clk)) {
		dev_err(&pdev->dev, "failed to get clk\n");
		return PTR_ERR(info->clk);
	}

	ret = clk_prepare_enable(info->clk);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to enable clk\n");
		return ret;
	}

	info->clk_rate = clk_get_rate(info->clk);

	info->vref = devm_regulator_get(&pdev->dev, "vref");
	if (IS_ERR(info->vref)) {
		dev_err(&pdev->dev, "failed to get regulator, %ld\n",
			PTR_ERR(info->vref));
		ret = PTR_ERR(info->vref);
		goto err_clk_disable;
	}

	ret = regulator_enable(info->vref);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to enable vref regulator\n");
		goto err_clk_disable;
	}

	mutex_init(&info->lock);
	reg = AX_SARADC_CH_EN_MASK(AX_SARADC_ALL_CHANNELS) |
		AX_SARADC_SAMPLE_16 |
		AX_SARADC_MODE |
		AX_SARADC_ENABLE;

	iowrite32(AX_SARADC_PD, info->regs + AX_SARADC_GLOBAL_CTRL);
	iowrite32(reg, info->regs + AX_SARADC_GLOBAL_CTRL);

	indio_dev->name = dev_name(&pdev->dev);
	indio_dev->dev.parent = &pdev->dev;
	indio_dev->info = &axiado_saradc_iio_info;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = axiado_saradc_iio_channels;
	indio_dev->num_channels = ARRAY_SIZE(axiado_saradc_iio_channels);

	/* Sanity check for possible later IP variants with more channels */
	if (indio_dev->num_channels > AX_SARADC_MAX_CHANNELS) {
		dev_err(&pdev->dev, "max channels exceeded\n");
		ret = -EINVAL;
		goto err_regulator_disable;
	}

	ret = devm_iio_device_register(&pdev->dev, indio_dev);
	if (ret) {
		dev_err(&pdev->dev,
			"failed to device  registration with IIO\n");
		goto err_regulator_disable;
	}

	platform_set_drvdata(pdev, indio_dev);
	return 0;

err_regulator_disable:
	regulator_disable(info->vref);
err_clk_disable:
	clk_disable_unprepare(info->clk);
	return ret;
}

/**
 * axiado_saradc_remove() - clean up resources on device removal
 * @pdev: platform device pointer
 *
 * This function disables the reference clock and regulator.
 */
static int axiado_saradc_remove(struct platform_device *pdev)
{
	struct iio_dev *indio_dev = platform_get_drvdata(pdev);
	struct axiado_saradc *info = iio_priv(indio_dev);

	regulator_disable(info->vref);
	clk_disable_unprepare(info->clk);
	return 0;
}

static const struct of_device_id axiado_saradc_match[] = {
	{ .compatible = "axiado,ax3000-saradc", },
	{},
};
MODULE_DEVICE_TABLE(of, axiado_saradc_match);

static struct platform_driver axiado_saradc_driver = {
	.probe		= axiado_saradc_probe,
	.remove		= axiado_saradc_remove,
	.driver		= {
		.name	= "axiado-saradc",
		.of_match_table = axiado_saradc_match,
	},
};

module_platform_driver(axiado_saradc_driver);

MODULE_AUTHOR("AXIADO CORPORATION");
MODULE_DESCRIPTION("AXIADO SARADC driver");
MODULE_LICENSE("GPL");
