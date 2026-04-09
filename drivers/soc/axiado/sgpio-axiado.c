// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Axiado Serial GPIO (SGPIO) Driver
 *
 * This driver provides support for Axiado's Serial GPIO controller, which
 * implements a serial interface for GPIO operations. The driver supports both
 * 128-bit and 512-bit design variants and provides GPIO functionality.
 *
 * Copyright (c) 2021-25 Axiado Corporation.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/irqdomain.h>
#include <linux/gpio/driver.h>
#include <linux/types.h>

#include <linux/spi/spi.h>
#include <linux/of_irq.h>
#include <linux/delay.h>
#include <linux/gpio.h>

#include <linux/ktime.h>
#include <linux/printk.h>
#ifdef CONFIG_AXIADO_SGPIO_REGMAP
#include <linux/regmap.h>
#endif
#include "sgpio-axiado.h"

/* Define the global slice array */
struct ax3000_slice_info slice[MAX_SLICE_COUNT];

static void ax3000_sgpio_set(struct gpio_chip *chip, unsigned int offset, int value)
{
	uint32_t bank = GET_BANK(offset/2);
	uint32_t position = GET_POS(offset/2);
	struct ax3000_sgpio *sgpio = gpiochip_get_data(chip);

	if (value)
		slice[2].reg_ss[bank] |= 1 << position;
	else
		slice[2].reg_ss[bank] &= ~(1 << position);

	sgpio_reg_write(sgpio, sgpio->regs->slice_dout_ss + (bank * 4),
			slice[2].reg_ss[bank]);
}

static int ax3000_sgpio_get(struct gpio_chip *chip, unsigned int offset)
{
	uint32_t bank = GET_BANK(offset/2);
	uint32_t position = GET_POS(offset/2);
	int rc;

	if (!(offset % 2))
		rc = IS_BIT_SET(slice[3].reg_ss[bank], position);
	else
		rc = IS_BIT_SET(slice[2].reg_ss[bank], position);

	return rc;
}

static int ax3000_sgpio_dir_in(struct gpio_chip *chip, unsigned int offset)
{
	if (!(offset % 2))
		return 0;
	else
		return -EINVAL;
}

static int ax3000_sgpio_dir_out(struct gpio_chip *chip, unsigned int offset,
		int value)
{
	if (offset % 2) {
		if (chip->set)
			chip->set(chip, offset, value);
		return 0;
	} else
		return -EINVAL;
}

static irqreturn_t sgpio_irq_handler(int irq, void *arg)
{
	struct ax3000_sgpio *sgpio = (struct ax3000_sgpio *)arg;
	u32 status, new_value;
	u32 changed_value;
	int i, bit;

	/* Read-on-clear (ACK) parent cause */
	status = sgpio_reg_read(sgpio, sgpio->regs->slice_status);
	status >>= STATUS_SHIFT;

	for (i = 0; i < sgpio->max_offset_regs; i++) {
		if (IS_BIT_SET(status, i)) {
			new_value = sgpio_reg_read(sgpio,
					sgpio->regs->slice_din_ss + (i * 4));

			changed_value = slice[3].reg_ss[i] ^ new_value;
			slice[3].reg_ss[i] = new_value;

			while (changed_value) {
				bit = __ffs(changed_value);
				changed_value &= ~BIT(bit);

				if (sgpio->mask_status[OFFSET(i, bit)] == 1) {
					int child = sgpio->irq_number[OFFSET(i, bit)];

					/* This is the “fake IRQ” for gpiomon */
					handle_nested_irq(child);
				}
			}
		}
	}

	return IRQ_HANDLED;
}

static void sgpio_hw_init(struct ax3000_sgpio *sgpio)
{
	uint32_t bank;
	uint32_t position;
	int i = 0;

	/* slice A0, Clock Pin - 0 */
	sgpio_reg_write(sgpio, sgpio->regs->slice_mux_0,
			0x306); /* sgpio_mux_config */
	sgpio_reg_write(sgpio, sgpio->regs->slice_preset_0,
			sgpio->preset_value); /* preset for countl */
	sgpio_reg_write(sgpio, sgpio->regs->slice_count_0,
			sgpio->count_value); /* count start value */
	sgpio_reg_write(sgpio, sgpio->regs->slice_pos_0,
			0x1f001f); /* pos and pos preset */

	/* Slice B1, Data Load Pin - 1 */
	bank = GET_BANK((sgpio->ngpios-1));
	position = GET_POS((sgpio->ngpios-1));

	sgpio_reg_write(sgpio, sgpio->regs->slice_mux_1,
			0x304); /* slice_mux_config */

	for (i = 0; i < bank; i++) {
		sgpio_reg_write(sgpio, sgpio->regs->slice_ld + (i * 4),
				0xffffffff); /* shift reg */
		sgpio_reg_write(sgpio, sgpio->regs->slice_ld_ss + (i * 4),
				0xffffffff); /* shift reg ss */
	}

	if (position) {
		sgpio_reg_write(sgpio, sgpio->regs->slice_ld + (i * 4),
				SET_TILL_POS(slice[1].reg_ss[i],
					     position)); /* shift reg */
		sgpio_reg_write(sgpio, sgpio->regs->slice_ld_ss + (i * 4),
				SET_TILL_POS(slice[1].reg_ss[i],
					     position)); /* shift reg ss */
	}

	sgpio_reg_write(sgpio, sgpio->regs->slice_preset_1,
			sgpio->preset_value); /* preset for count */
	sgpio_reg_write(sgpio, sgpio->regs->slice_count_1,
			sgpio->count_value); /* count start value */
	sgpio_reg_write(sgpio, sgpio->regs->slice_pos_1,
			sgpio->pos_reg); /* pos and pos preset */

	/* Slice C2, Data Out Pin - 2 */
	bank = GET_BANK((sgpio->ngpios));
	position = GET_POS((sgpio->ngpios));

	sgpio_reg_write(sgpio, sgpio->regs->slice_mux_2,
			0x304); //slice_mux_config

	for (i = 0; i < bank; i++) {
		sgpio_reg_write(sgpio, sgpio->regs->slice_dout + (i * 4),
				slice[2].reg_ss[i]); /* shift reg */
		sgpio_reg_write(sgpio, sgpio->regs->slice_dout_ss + (i * 4),
				slice[2].reg_ss[i]); /* shift reg ss */
	}

	if (position) {
		sgpio_reg_write(sgpio, sgpio->regs->slice_dout + (i * 4),
				slice[2].reg_ss[i]); /* shift reg */
		sgpio_reg_write(sgpio, sgpio->regs->slice_dout_ss + (i * 4),
				slice[2].reg_ss[i]); /* shift reg ss */
	}

	sgpio_reg_write(sgpio, sgpio->regs->slice_preset_2,
			sgpio->preset_value); /* preset for count */
	sgpio_reg_write(sgpio, sgpio->regs->slice_count_2,
			sgpio->count_value); /* count start value */
	sgpio_reg_write(sgpio, sgpio->regs->slice_pos_2,
			sgpio->pos_reg); /* pos and pos preset */

	/* Slice D3, Data In Pin - 3 */
	sgpio_reg_write(sgpio, sgpio->regs->slice_mux_3,
			0x14C); /* slice_mux_config */
	sgpio_reg_write(sgpio, sgpio->regs->slice_preset_3,
			sgpio->preset_value); /* preset for count */
	sgpio_reg_write(sgpio, sgpio->regs->slice_count_3,
			sgpio->count_value); /* count start value */
	sgpio_reg_write(sgpio, sgpio->regs->slice_pos_3,
			sgpio->pos_reg); /* pos and pos preset */

	/* Slice E4, Output Enable for respective pins */
	sgpio_reg_write(sgpio, sgpio->regs->slice_mux_4,
			0x104); /* slice_mux_config */
	sgpio_reg_write(sgpio, sgpio->regs->slice_oe,
			0xffffffff); /* shift reg */
	sgpio_reg_write(sgpio, sgpio->regs->slice_oe_ss,
			0xffffffff); /* shift reg ss */
	sgpio_reg_write(sgpio, sgpio->regs->slice_preset_4,
			sgpio->preset_value); /* preset for count */
	sgpio_reg_write(sgpio, sgpio->regs->slice_count_4,
			sgpio->count_value); /* count start value */
	sgpio_reg_write(sgpio, sgpio->regs->slice_pos_4,
			0x1f001f); /* pos and pos preset */

	sgpio_reg_write(sgpio, sgpio->regs->slice_mask,
			0xdfff); /* Enabling the interrupt for new data */

	sgpio_reg_write(sgpio, sgpio->regs->slice_ctrl_en,
			0xffff); /* cntrl_enable 'b100001111  ckfl d bmgn a */
	sgpio_reg_write(sgpio, sgpio->regs->slice_ctrl_en_pos,
			0xffff); /* out_mux_config 'b1000000 */
}

static int sgpio_set_irq_type(struct irq_data *d, unsigned int type)
{
	switch (type) {
	case IRQ_TYPE_EDGE_BOTH:
	case IRQ_TYPE_EDGE_RISING:
	case IRQ_TYPE_EDGE_FALLING:
		irq_set_handler_locked(d, handle_edge_irq);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static void sgpio_ack_irq(struct irq_data *d)
{
	/* Nothing to do for software IRQ */
}

static void sgpio_mask_irq(struct irq_data *d)
{
	struct gpio_chip *chip;
	struct ax3000_sgpio *sgpio;
	uint32_t irq_num;

	/* Get the gpio_chip associated with the IRQ number */
	chip = irq_data_get_irq_chip_data(d);  // Get gpio_chip for the IRQ
	if (!chip) {
		pr_err("Unable to get gpio_chip for IRQ\n");
		return;
	}

	/* Get the private data associated with the gpio_chip */
	sgpio = gpiochip_get_data(chip);
	if (!sgpio) {
		pr_err("Unable to get chip data\n");
		return;
	}

	irq_num = irqd_to_hwirq(d);
	sgpio->irq_number[irq_num/2] = 0;
	sgpio->mask_status[irq_num/2] = 0;
}

static void sgpio_unmask_irq(struct irq_data *d)
{
	struct gpio_chip *chip;
	struct ax3000_sgpio *sgpio;
	uint32_t irq_num;

	/* Get the gpio_chip associated with the IRQ number */
	chip = irq_data_get_irq_chip_data(d);  // Get gpio_chip for the IRQ
	if (!chip) {
		pr_err("Unable to get gpio_chip for IRQ\n");
		return;
	}

	/* Get the private data associated with the gpio_chip */
	sgpio = gpiochip_get_data(chip);
	if (!sgpio) {
		pr_err("Unable to get chip data\n");
		return;
	}

	irq_num = irqd_to_hwirq(d);
	sgpio->irq_number[irq_num/2] = gpio_to_irq(sgpio->chip.base + irq_num);
	sgpio->mask_status[irq_num/2] = 1;
}

static void sgpio_irq_shutdown(struct irq_data *d)
{
	/* Shutdown callback ensures IRQ is properly disabled when
	 * the IRQ chip is being removed. This helps ensure IRQ handler
	 * threads are terminated before the chip is removed.
	 */
	sgpio_mask_irq(d);
}

static int sgpio_probe(struct platform_device *pdev)
{
	int rc;
	int irq;
	int i;
	const __be32 *prop;
	struct gpio_irq_chip *girq;
	struct ax3000_sgpio *sgpio;
	uint32_t variant;
	uint32_t dout_value;
	int dout_reverse;
#ifdef CONFIG_AXIADO_SGPIO_REGMAP
	u32 reg_base_offset = 0;
#endif

	sgpio = devm_kzalloc(&pdev->dev, sizeof(*sgpio), GFP_KERNEL);
	if (!sgpio)
		return -ENOMEM;

#ifdef CONFIG_AXIADO_SGPIO_REGMAP
	/* Try to get regmap from parent device first */
	sgpio->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (sgpio->regmap) {
		/* Get the base address offset from device tree */
		rc = of_property_read_u32(pdev->dev.of_node, "reg",
					  &reg_base_offset);
		if (rc) {
			dev_err(&pdev->dev, "Failed to read reg property: %d\n",
				rc);
			return rc;
		}
		sgpio->regmap_base_offset = reg_base_offset;
		dev_info(&pdev->dev, "Using regmap with base offset: 0x%x\n",
			 reg_base_offset);
	} else {
#endif
		/* Fall back to direct memory mapping */
		sgpio->membase = devm_platform_ioremap_resource(pdev, 0);
		if (IS_ERR(sgpio->membase)) {
			dev_err(&pdev->dev, "Failed to map SGPIO memory resource\n");
			return PTR_ERR(sgpio->membase);
		}
		dev_info(&pdev->dev, "Using direct memory mapping\n");
#ifdef CONFIG_AXIADO_SGPIO_REGMAP
	}
#endif

	rc = device_property_read_u32(&pdev->dev, "ngpios", &sgpio->ngpios);
	if (rc < 0) {
		dev_err(&pdev->dev, "Could not read ngpios property\n");
		return -EINVAL;
	}

	if (device_property_read_u32(&pdev->dev, "design-variant", &variant)) {
		dev_err(&pdev->dev, "design-variant not specified in DT\n");
		return -EINVAL;
	}

	/* FPGA B0 SGPIO design variant 128 bit or 512 bit */
	if (variant == 128) {
		sgpio->regs = (struct sgpio_reg_offsets *)&design_128_bit_regs;
		sgpio->max_sgpio_pins = 128;
		sgpio->max_offset_regs = 4;
	} else if (variant == 512) {
		sgpio->regs = (struct sgpio_reg_offsets *)&design_512_bit_regs;
		sgpio->max_sgpio_pins = 512;
		sgpio->max_offset_regs = 16;
	} else {
		return -EINVAL;
	}

	if (sgpio->ngpios > sgpio->max_sgpio_pins) {
		dev_err(&pdev->dev, "ngpio is greater than 512 pins\n");
		return -EINVAL;
	}

	rc = device_property_read_u32(&pdev->dev, "bus-frequency", &sgpio->bus_frequency);
	if (rc < 0) {
		dev_err(&pdev->dev, "Could not read bus-frequency property\n");
		return -EINVAL;
	}

	rc = device_property_read_u32(&pdev->dev, "apb-frequency", &sgpio->apb_frequency);
	if (rc < 0) {
		dev_err(&pdev->dev, "Could not read apb-frequency property\n");
		return -EINVAL;
	}

	sgpio->preset_value = (sgpio->apb_frequency / sgpio->bus_frequency) - 1;
	sgpio->count_value = sgpio->preset_value;

	sgpio->pos = sgpio->ngpios - 1;
	sgpio->pos_reset = sgpio->ngpios - 1;

	sgpio->pos_reg = (sgpio->pos_reset<<16) | sgpio->pos;

	prop = of_get_property(pdev->dev.of_node, "dout-init", NULL);
	if (!prop) {
		dev_err(&pdev->dev, "Failed to get dout-init\n");
		return -EINVAL;
	}

	for (i = 0; i < sgpio->max_offset_regs; i++) {
		slice[2].reg_ss[i] = 0;
		dout_value = be32_to_cpu(prop[i]);

		for (dout_reverse = 0; dout_reverse < 32; ++dout_reverse) {
			slice[2].reg_ss[i] <<= 1;
			slice[2].reg_ss[i] |= (dout_value & 1);
			dout_value >>= 1;
		}
	}

	sgpio_hw_init(sgpio);

	irq = irq_of_parse_and_map(pdev->dev.of_node, 0);

	if (irq < 0) {
		pr_debug("SGPIO : failed to get IRQ = %d\n", irq);
		return irq;
	}

	/* Store IRQ number for cleanup in remove function */
	sgpio->parent_irq = irq;

	rc = devm_request_threaded_irq(&pdev->dev, irq, NULL,
			sgpio_irq_handler, IRQF_ONESHOT,
			"sgpio_irq", sgpio);

	if (rc < 0) {
		pr_debug("SGPIO: Handler registration failed\n");
		return rc;
	}

	sgpio->chip.parent = &pdev->dev;
	sgpio->chip.ngpio = sgpio->ngpios * 2;
	sgpio->chip.owner = THIS_MODULE;
	sgpio->chip.direction_input = ax3000_sgpio_dir_in;
	sgpio->chip.direction_output = ax3000_sgpio_dir_out;
	sgpio->chip.get = ax3000_sgpio_get;
	sgpio->chip.set = ax3000_sgpio_set;
	sgpio->chip.can_sleep = true;
	sgpio->chip.label = dev_name(&pdev->dev);
	sgpio->chip.base = -1;

	/* Set up the irqchip dynamically */
	sgpio->irq.name = "SGPIO-IRQ";
	sgpio->irq.irq_ack = sgpio_ack_irq;
	sgpio->irq.irq_mask = sgpio_mask_irq;
	sgpio->irq.irq_unmask = sgpio_unmask_irq;
	sgpio->irq.irq_set_type = sgpio_set_irq_type;
	sgpio->irq.irq_shutdown = sgpio_irq_shutdown;
	sgpio->irq.flags = IRQCHIP_IMMUTABLE;

	/* Get a pointer to the gpio_irq_chip */
	girq = &sgpio->chip.irq;
	girq->chip = &sgpio->irq;
	/* This will let us handle the parent IRQ in the driver */
	girq->parent_handler = NULL;
	girq->num_parents = 0;
	girq->parents = NULL;
	girq->default_type = IRQ_TYPE_NONE;
	girq->handler = handle_bad_irq;

	rc = devm_gpiochip_add_data(&pdev->dev, &sgpio->chip, sgpio);
	if (rc < 0) {
		dev_err(&pdev->dev, "Could not register gpiochip, %d\n", rc);
		return rc;
	}

	/* Store sgpio pointer for cleanup in remove function */
	platform_set_drvdata(pdev, sgpio);

	return 0;
}

static int sgpio_remove(struct platform_device *pdev)
{
	struct ax3000_sgpio *sgpio = platform_get_drvdata(pdev);
	int i;

	if (!sgpio)
		return 0;

	/* Disable interrupts in hardware before removing GPIO chip */
	if (sgpio->regs) {
		/* Disable interrupt mask */
		sgpio_reg_write(sgpio, sgpio->regs->slice_mask, 0x0);
		/* Disable slice control */
		sgpio_reg_write(sgpio, sgpio->regs->slice_ctrl_en, 0x0);
	}

	/* Disable and synchronize the parent IRQ to ensure any pending
	 * IRQ handlers complete before the GPIO chip is removed.
	 * This prevents use-after-free errors if IRQ handlers try to
	 * access the GPIO chip after it's been removed.
	 */
	if (sgpio->parent_irq >= 0) {
		/* Disable the parent IRQ to prevent new interrupts */
		disable_irq(sgpio->parent_irq);
		/* Synchronize to ensure any pending IRQ handlers complete */
		synchronize_irq(sgpio->parent_irq);
	}

	/* Disable all GPIO IRQs before removing the chip.
	 * This ensures all IRQ handler threads are terminated before
	 * the GPIO chip IRQ chip is removed. The GPIO subsystem will
	 * automatically remove the GPIO chip via devm_gpiochip_add_data,
	 * but we disable IRQs first to ensure handler threads complete
	 * and prevent new interrupts.
	 */
	if (sgpio->chip.irq.domain) {
		struct irq_domain *domain = sgpio->chip.irq.domain;
		unsigned int irq;
		int hwirq;

		/* Iterate through all GPIOs and disable their IRQs */
		for (hwirq = 0; hwirq < sgpio->chip.ngpio; hwirq++) {
			irq = irq_find_mapping(domain, hwirq);
			if (irq) {
				/* Disable the IRQ to stop handler threads */
				disable_irq(irq);
				/* Synchronize to ensure handler completes */
				synchronize_irq(irq);
			}
		}
	}

	/* Clear internal IRQ state to prevent stale references */
	for (i = 0; i < sgpio->max_sgpio_pins; i++) {
		sgpio->mask_status[i] = 0;
		sgpio->irq_number[i] = 0;
	}

	return 0;
}

static const struct of_device_id ax_sgpio_match[] = {
	{ .compatible = "axiado,sgpio"},
	{ }
};
MODULE_DEVICE_TABLE(of, ax_sgpio_match);

static struct platform_driver sgpio_driver = {
	.driver = {
		.name = "sgpio",
		.owner = THIS_MODULE,
		.of_match_table = ax_sgpio_match,
	},
	.probe = sgpio_probe,
	.remove = sgpio_remove,
};

static int __init ax_sgpio_init(void)
{
	int ret;

	/* Register the SGPIO Platform driver */
	ret = platform_driver_register(&sgpio_driver);
	if (ret < 0) {
		pr_err("Failed to register SGPIO driver\n");
		return ret;
	}

	return 0;
}

static void __exit ax_sgpio_exit(void)
{
	platform_driver_unregister(&sgpio_driver);
}

module_init(ax_sgpio_init);
module_exit(ax_sgpio_exit);

MODULE_DESCRIPTION("Axiado Serial GPIO Driver");
MODULE_AUTHOR("Axiado Corporation");
MODULE_LICENSE("GPL");
