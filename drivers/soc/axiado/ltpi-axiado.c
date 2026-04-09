// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Axiado LVDS Tunneling Protocol & Interface (LTPI) Driver
 *
 * Copyright (c) 2021-25 Axiado Corporation.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/bitmap.h>
#include <linux/gpio/driver.h>

#include <linux/spi/spi.h>
#include <linux/of_irq.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/irqdomain.h>

#ifdef CONFIG_AXIADO_LTPI_REGMAP
#include <linux/regmap.h>
#endif

#ifdef CONFIG_AXIADO_LTPI_DEBUGFS
#include <linux/debugfs.h>
#endif

#include "ltpi-axiado.h"

static int param_nl_ngpios = -1;
static long param_nl_gpio_vals[MAX_BANKS];
static int param_nl_num_gpio_args = 0;

/**
 * update_bank - Update a bank value in the destination array
 * @dest: Destination array to update
 * @index: Bank index to update
 * @value: Value to store in the bank
 */
static inline void update_bank(u32 *dest, int index, u32 value)
{
	dest[index] = value;
}

/**
 * read_all_banks - Read all GPIO banks for a specific mode
 * @gc: GPIO chip structure
 * @dest: Destination array to store bank values
 * @mode: Mode to read (0=inputs, 1=mask, 2=outputs)
 */
static void read_all_banks(struct gpio_chip *gc, u32 *dest, int mode)
{
	struct ax3000_ltpi *ltpi = gpiochip_get_data(gc);
	int i;

	for (i = 0; i < ltpi->num_banks; i++) {
		u32 bank_val = 0;

		dest[i] = 0;
		switch (mode) {
		case 0: /* inputs */
			bank_val = ltpi_reg_read(ltpi,
						 REG_LTPI_DI_ADRS_OFFSET + (i * 4));
			break;
		case 1: /* mask */
			bank_val = ltpi_reg_read(ltpi,
						 REG_LTPI_MASK_ADRS_OFFSET + (i * 4));
			break;
		case 2: /* outputs */
			bank_val = ltpi_reg_read(ltpi,
						 REG_LTPI_DO_ADRS_OFFSET + (i * 4));
			break;
		default:
			return;
		}
		update_bank(dest, i, bank_val);
	}
}

/**
 * ltpi_workqueue - Workqueue function to update interrupt masks
 * @work: Work structure containing LTPI device data
 */
static void ltpi_workqueue(struct work_struct *work)
{
	struct ax3000_ltpi *ltpi =
		container_of(work, struct ax3000_ltpi, ltpi_work);
	int i;

	for (i = 0; i < ltpi->num_banks; i++) {
		ltpi_reg_write(ltpi, REG_LTPI_MASK_ADRS_OFFSET + (i * 4),
			       ltpi->mask[i]);
	}
}

/**
 * ax3000_ltpi_set - Set GPIO output value
 * @gc: GPIO chip structure
 * @offset: GPIO offset to set
 * @value: Value to set (0 or 1)
 */
static void ax3000_ltpi_set(struct gpio_chip *gc, unsigned int offset,
			    int value)
{
	struct ax3000_ltpi *ltpi = gpiochip_get_data(gc);
	u32 bank_val;

	if (offset >= ltpi->lines)
		return;

	mutex_lock(&ltpi->lock);

	bank_val = ltpi->outputs_cache[GET_BANK(offset)];

	if (value)
		bank_val |= BIT(GET_POS(offset));
	else
		bank_val &= ~BIT(GET_POS(offset));

	update_bank(ltpi->outputs_cache, GET_BANK(offset), bank_val);

	ltpi_reg_write(ltpi, REG_LTPI_DO_ADRS_OFFSET + (GET_BANK(offset) * 4),
		       bank_val);

	mutex_unlock(&ltpi->lock);
}

/**
 * ax3000_ltpi_get - Get GPIO input value
 * @chip: GPIO chip structure
 * @offset: GPIO offset to read
 * Return: GPIO value (0 or 1) on success, -EINVAL on error
 */
static int ax3000_ltpi_get(struct gpio_chip *chip, unsigned int offset)
{
	struct ax3000_ltpi *ltpi = gpiochip_get_data(chip);
	unsigned int rel_offset = offset - ltpi->lines;
	u32 bank_val;

	if (offset < ltpi->lines)
		return -EINVAL;

	mutex_lock(&ltpi->lock);
	bank_val = ltpi_reg_read(ltpi,
				 REG_LTPI_DI_ADRS_OFFSET + GET_BANK(rel_offset) * 4);
	mutex_unlock(&ltpi->lock);

	return IS_BIT_SET(bank_val, GET_POS(rel_offset));
}

static int ax3000_ltpi_dir_in(struct gpio_chip *chip, unsigned int offset)
{
	struct ax3000_ltpi *ltpi = gpiochip_get_data(chip);

	if (offset >= ltpi->lines)
		return 0;
	else
		return -EINVAL;
}

static int ax3000_ltpi_dir_out(struct gpio_chip *chip, unsigned int offset,
			       int value)
{
	struct ax3000_ltpi *ltpi = gpiochip_get_data(chip);

	if (offset < ltpi->lines) {
		ax3000_ltpi_set(chip, offset, value);
		return 0;
	} else {
		return -EINVAL;
	}
}

static void ltpi_ack_irq(struct irq_data *d)
{
	struct gpio_chip *chip = irq_data_get_irq_chip_data(d);
	struct ax3000_ltpi *ltpi = gpiochip_get_data(chip);

	/*
	 * Acknowledge the hardware interrupt by reading the GPIO Interrupt
	 * Status Register. The hardware specification defines its type as
	 * 'rc' (Read-to-Clear), meaning the act of reading it clears the
	 * pending interrupt source bits and de-asserts the IRQ line.
	 */
	(void)ltpi_reg_read(ltpi, REG_LTPI_STATUS_ADRS_OFFSET);
}

static int ltpi_set_irq_type(struct irq_data *d, unsigned int type)
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

static void ltpi_mask_irq(struct irq_data *d)
{
	struct gpio_chip *chip = irq_data_get_irq_chip_data(d);
	struct ax3000_ltpi *ltpi = gpiochip_get_data(chip);
	int hwirq = irqd_to_hwirq(d);

	ltpi->mask[GET_BANK(hwirq)] |= (1 << GET_POS(hwirq));
	schedule_work(&ltpi->ltpi_work);
}

static void ltpi_unmask_irq(struct irq_data *d)
{
	struct gpio_chip *chip = irq_data_get_irq_chip_data(d);
	struct ax3000_ltpi *ltpi = gpiochip_get_data(chip);
	int hwirq = irqd_to_hwirq(d);

	pr_debug("%s, hwirq %d\n", __func__, hwirq);

	ltpi->mask[GET_BANK(hwirq)] &= ~(1 << GET_POS(hwirq));
	schedule_work(&ltpi->ltpi_work);
}

static int ltpi_irq_reqres(struct irq_data *d)
{
	struct gpio_chip *chip = irq_data_get_irq_chip_data(d);
	struct ax3000_ltpi *ltpi = gpiochip_get_data(chip);
	int hwirq = irqd_to_hwirq(d);

	/* input gpio in lower half, index starting from ltpi->lines */
	unsigned int gpio = hwirq + ltpi->lines;

	pr_debug("%s: hwirq %d -> gpio %d\n", __func__, hwirq, gpio);
	return gpiochip_reqres_irq(chip, gpio);
}

static void ltpi_irq_relres(struct irq_data *d)
{
	struct gpio_chip *chip = irq_data_get_irq_chip_data(d);
	struct ax3000_ltpi *ltpi = gpiochip_get_data(chip);
	int hwirq = irqd_to_hwirq(d);
	unsigned int gpio = hwirq + ltpi->lines;

	gpiochip_relres_irq(chip, gpio);
}

static irqreturn_t ltpi_irq_thread(int irq, void *data)
{
	struct ax3000_ltpi *ltpi = (struct ax3000_ltpi *)data;
	u32 current_state[MAX_BANKS];
	u32 changed_bits[MAX_BANKS];
	u32 hwirq;

	unsigned long *cur = (unsigned long *)&current_state;
	unsigned long *cha = (unsigned long *)&changed_bits;
	unsigned long *in = (unsigned long *)ltpi->inputs_cache;
	unsigned int nbit = (unsigned int)ltpi->lines;

	read_all_banks(&ltpi->chip, current_state, 0);

#ifdef CONFIG_AXIADO_LTPI_DEBUGFS
	if (ltpi->is_simulating_irq) {
		pr_info("ltpi irq: fire %d for simulation test\n",
			ltpi->simulating_irq);
		generic_handle_irq(irq_find_mapping(ltpi->irq_domain,
						    ltpi->simulating_irq));
		ltpi->is_simulating_irq = false;
		return IRQ_HANDLED;
	}
#endif
	bitmap_xor(cha, cur, in, nbit);
	bitmap_copy(in, cur, nbit);

	for_each_set_bit(hwirq, cha, ltpi->lines) {
		/* unmnask: 0 */
		if (!IS_BIT_SET(ltpi->mask[GET_BANK(hwirq)], GET_POS(hwirq))) {
			pr_debug("ltpi irq: fire %d\n", hwirq);
			generic_handle_irq(irq_find_mapping(ltpi->irq_domain, hwirq));
		}
	}

	read_all_banks(&ltpi->chip, ltpi->inputs_cache, 0);
	return IRQ_HANDLED;
}

static int ltpi_irq_domain_map(struct irq_domain *d, unsigned int virq,
			       irq_hw_number_t hw)
{
	struct ax3000_ltpi *ltpi = d->host_data;

	if (hw >= ltpi->lines) {
		pr_err("IRQ: invalid map hwirq=%lu, max=%d\n", hw, ltpi->lines);
		return -EINVAL;
	}

	pr_debug("ltpi irq: mapping virq=%u to hwirq=%lu.\n", virq, hw);

	irq_set_chip_and_handler(virq, &ltpi->irq, handle_edge_irq);
	irq_set_chip_data(virq, &ltpi->chip);
	irq_set_nested_thread(virq, 0);
	return 0;
}

static int ltpi_irq_domain_xlate(struct irq_domain *d,
				 struct device_node *ctrlr, const u32 *intspec,
				 unsigned int intsize, unsigned long *out_hwirq,
				 unsigned int *out_type)
{
	*out_hwirq = intspec[0];
	*out_type = intspec[1];

	pr_debug("ltpi xlate: hwirq=%u, type=%u\n", intspec[0], intspec[1]);

	return 0;
}

static const struct irq_domain_ops ltpi_irq_domain_ops = {
	.map = ltpi_irq_domain_map,
	.xlate = ltpi_irq_domain_xlate,
};

#ifdef CONFIG_AXIADO_LTPI_DEBUGFS
static int simulate_irq_write(void *data, u64 val)
{
	struct ax3000_ltpi *ltpi = data;

	ltpi->simulating_irq = val;

	if (ltpi->simulating_irq >= ltpi->lines)
		return -EINVAL;

	ltpi->is_simulating_irq = true;

	pr_info("debugfs: Updated irq = %d, trigger by simulate_parent_irq\n",
		ltpi->simulating_irq);
	return 0;
}
DEFINE_DEBUGFS_ATTRIBUTE(fops_simulate_irq, NULL, simulate_irq_write, "%llu\n");

static int simulate_parent_irq_write(void *data, u64 val)
{
	struct ax3000_ltpi *ltpi = data;
	unsigned int irq = ltpi->chip.irq.parents[0];

	pr_info("debugfs: simulating parent IRQ %d\n", irq);

	generic_handle_irq(irq);

	return 0;
}
DEFINE_DEBUGFS_ATTRIBUTE(fops_simulate_parent_irq, NULL,
			 simulate_parent_irq_write, "%llu\n");

static int dump_cache(void *data, u64 val)
{
	struct ax3000_ltpi *ltpi = data;
	int i;

	pr_info("\n inputs_cache:\n");
	for (i = 0; i < ltpi->num_banks; i++)
		pr_info("    bank[%d]: 0x%08x\n", i, ltpi->inputs_cache[i]);
	pr_info(" mask cache:\n");
	for (i = 0; i < ltpi->num_banks; i++)
		pr_info("    bank[%d]: 0x%08x\n", i, ltpi->mask[i]);
	pr_info(" outputs_cache:\n");
	for (i = 0; i < ltpi->num_banks; i++)
		pr_info("    bank[%d]: 0x%08x\n", i, ltpi->outputs_cache[i]);
	pr_info("\n");

	return 0;
}
DEFINE_DEBUGFS_ATTRIBUTE(dump_caches, NULL, dump_cache, "%llu\n");

static void setup_ltpi_debugfs(struct ax3000_ltpi *ltpi)
{
	ltpi->debugfs_root = debugfs_create_dir("ltpi-gpio", NULL);

	debugfs_create_file("simulate_virtual_irq", 0200, ltpi->debugfs_root,
			    ltpi, &fops_simulate_irq);
	debugfs_create_file("simulate_parent_irq", 0200, ltpi->debugfs_root,
			    ltpi, &fops_simulate_parent_irq);
	debugfs_create_file("dump_caches", 0200, ltpi->debugfs_root, ltpi,
			    &dump_caches);
}
#endif

/**
 * ltpi_probe - Probe function for LTPI GPIO driver
 * @spi: Platform device structure
 * Return: 0 on success, negative error code on failure
 */
static int ltpi_probe(struct platform_device *spi)
{
	struct gpio_irq_chip *girq;
	struct ax3000_ltpi *ltpi;
	int irq;
	int rc = 0;
	int i;
#ifdef CONFIG_AXIADO_LTPI_REGMAP
	u32 reg_base_offset = 0;
#endif
	ltpi = devm_kzalloc(&spi->dev, sizeof(*ltpi), GFP_KERNEL);
	if (!ltpi)
		return -ENOMEM;

	platform_set_drvdata(spi, ltpi);

#ifdef CONFIG_AXIADO_LTPI_REGMAP
	/* Try to get regmap from parent device first */
	ltpi->regmap = dev_get_regmap(spi->dev.parent, NULL);
	if (ltpi->regmap) {
		/* Get the base address offset from device tree */
		rc = of_property_read_u32(spi->dev.of_node, "reg",
					  &reg_base_offset);
		if (rc) {
			dev_err(&spi->dev, "Failed to read reg property: %d\n",
				rc);
			return rc;
		}
		ltpi->regmap_base_offset = reg_base_offset;
		dev_info(&spi->dev, "Using regmap with base offset: 0x%x\n",
			 reg_base_offset);
	} else {
#endif
		/* Fall back to direct memory mapping */
		ltpi->membase = devm_platform_ioremap_resource(spi, 0);
		if (IS_ERR(ltpi->membase)) {
			dev_err(&spi->dev,
				"Failed to map LTPI memory resource\n");
			return PTR_ERR(ltpi->membase);
		}
		dev_info(&spi->dev, "Using direct memory mapping\n");
#ifdef CONFIG_AXIADO_LTPI_REGMAP
	}
#endif
	if (param_nl_ngpios == -1) {
		rc = device_property_read_u32(&spi->dev, "ngpios", &ltpi->lines);
		if (rc < 0) {
			dev_err(&spi->dev, "could not read ngpios property\n");
			return -EINVAL;
		}
		dev_info(&spi->dev, "Using DTS ngpios = %d\n", ltpi->lines);
	} else {
		ltpi->lines = param_nl_ngpios;
		dev_info(&spi->dev, "Using parameter ngpios = %d\n", ltpi->lines);
	}

	if (ltpi->lines > MAX_GPIO_PINS) {
		dev_err(&spi->dev,
			"lines-per-function is greater than %d pins\n",
			MAX_GPIO_PINS);
		return -EINVAL;
	}

	ltpi->num_banks = (ltpi->lines + PINS_PER_GPIO_BANK - 1) / PINS_PER_GPIO_BANK;

	dev_dbg(&spi->dev, "lines %d, banks %d\n", ltpi->lines,
		ltpi->num_banks);

	ltpi->mask = devm_kcalloc(&spi->dev, ltpi->num_banks, sizeof(u32),
				  GFP_KERNEL);
	if (!ltpi->mask)
		return -ENOMEM;

	ltpi->inputs_cache = devm_kcalloc(&spi->dev, ltpi->num_banks,
					  sizeof(u32), GFP_KERNEL);
	if (!ltpi->inputs_cache)
		return -ENOMEM;

	ltpi->outputs_cache = devm_kcalloc(&spi->dev, ltpi->num_banks,
					   sizeof(u32), GFP_KERNEL);
	if (!ltpi->outputs_cache)
		return -ENOMEM;

	rc = of_property_read_u32_array(spi->dev.of_node, "dout-init",
					ltpi->outputs_cache, ltpi->num_banks);
	if (rc) {
		dev_err(&spi->dev,
			"Failed to read 'dout-init', check property count in DTB. Error: %d\n",
			rc);
		return rc;
	}

	/* module parameter first */
	if (param_nl_num_gpio_args == ltpi->num_banks) {
		for (i = 0; i < ltpi->num_banks; i++)
			update_bank(ltpi->outputs_cache, i, param_nl_gpio_vals[i]);
	}

	for (i = 0; i < ltpi->num_banks; i++) {
		ltpi_reg_write(ltpi, REG_LTPI_DO_ADRS_OFFSET + (i * 4),
			       ltpi->outputs_cache[i]);
		dev_info(&spi->dev, "Default dout-init[%d] = 0x%08x\n", i,
			 ltpi->outputs_cache[i]);
	}

	irq = irq_of_parse_and_map(spi->dev.of_node, 0);

	if (!irq) {
		dev_err(&spi->dev, "ltpi : failed to get irq = %d\n", irq);
		return -EINVAL;
	}

	/* mask all interrupts */
	for (i = 0; i < ltpi->num_banks; i++) {
		ltpi->mask[i] = 0xffffffff;
		ltpi_reg_write(ltpi, REG_LTPI_MASK_ADRS_OFFSET + (i * 4),
			       ltpi->mask[i]);
	}

	INIT_WORK(&ltpi->ltpi_work, ltpi_workqueue);
	mutex_init(&ltpi->lock);

	ltpi->irq_domain = irq_domain_create_simple(spi->dev.fwnode,
						    ltpi->lines, 0,
						    &ltpi_irq_domain_ops, ltpi);
	if (!ltpi->irq_domain)
		return -ENOMEM;

	ltpi->chip.label = dev_name(&spi->dev);
	ltpi->chip.parent = NULL; /* to use custom irq_domain */
	ltpi->chip.ngpio = ltpi->lines * 2; /* inputs + outpus */
	ltpi->chip.owner = THIS_MODULE;
	ltpi->chip.base = -1;
	ltpi->chip.direction_input = ax3000_ltpi_dir_in;
	ltpi->chip.direction_output = ax3000_ltpi_dir_out;
	ltpi->chip.get = ax3000_ltpi_get;
	ltpi->chip.set = ax3000_ltpi_set;
	ltpi->chip.can_sleep = true;

	ltpi->irq.name = "LTPI-IRQ";
	ltpi->irq.irq_ack = ltpi_ack_irq;
	ltpi->irq.irq_mask = ltpi_mask_irq;
	ltpi->irq.irq_unmask = ltpi_unmask_irq;
	ltpi->irq.irq_set_type = ltpi_set_irq_type;
	ltpi->irq.irq_request_resources = ltpi_irq_reqres;
	ltpi->irq.irq_release_resources = ltpi_irq_relres;

	ltpi->irq.flags |= IRQCHIP_IMMUTABLE; /* prevent change from gpiolib */

	girq = &ltpi->chip.irq;
	girq->chip = &ltpi->irq;
	girq->default_type = IRQ_TYPE_NONE;
	girq->domain = ltpi->irq_domain;
	girq->handler = handle_edge_irq;
	girq->num_parents = 1;
	girq->parent_handler = NULL;
	girq->parents =
		devm_kcalloc(&spi->dev, 1, sizeof(*girq->parents), GFP_KERNEL);
	if (!girq->parents)
		return -ENOMEM;
	girq->parents[0] = irq;

	rc = devm_request_threaded_irq(&spi->dev, irq, NULL, ltpi_irq_thread,
				       IRQF_ONESHOT, "ltpi-gpio", ltpi);
	if (rc) {
		dev_err(&spi->dev, "Failed to request parent IRQ %d: %d\n",
			girq->parents[0], rc);
		return rc;
	}

	rc = devm_gpiochip_add_data(&spi->dev, &ltpi->chip, ltpi);
	if (rc < 0) {
		dev_err(&spi->dev, "Could not register gpiochip, %d\n", rc);
		irq_domain_remove(ltpi->irq_domain);
		return rc;
	}

#ifdef CONFIG_AXIADO_LTPI_DEBUGFS
	if (ltpi->irq_domain == irq_find_host(spi->dev.of_node))
		pr_info("LTPI PROBE SUCCESS: IRQ domain was correctly registered.\n");

	setup_ltpi_debugfs(ltpi);
#endif
	read_all_banks(&ltpi->chip, ltpi->inputs_cache, 0);
	return 0;
}

static int ltpi_remove(struct platform_device *spi)
{
	struct ax3000_ltpi *ltpi = platform_get_drvdata(spi);
	int hwirq;

#ifdef CONFIG_AXIADO_LTPI_DEBUGFS
	debugfs_remove_recursive(ltpi->debugfs_root);
#endif
	for (hwirq = 0; hwirq < ltpi->lines; hwirq++) {
		unsigned int virq = irq_find_mapping(ltpi->irq_domain, hwirq);
		if (virq) {
			disable_irq_nosync(virq);
			irq_dispose_mapping(virq);
		}
	}

	irq_domain_remove(ltpi->irq_domain);
	return 0;
}

static const struct of_device_id ax_ltpi_match[] = { { .compatible =
							       "axiado,ltpi" },
						     {} };
MODULE_DEVICE_TABLE(of, ax_ltpi_match);

static struct platform_driver ltpi_driver = {
	.driver = {
		.name = "axiado-ltpi",
		.owner = THIS_MODULE,
		.of_match_table = ax_ltpi_match,
	},
	.probe = ltpi_probe,
	.remove = ltpi_remove,
};

static int __init ax_ltpi_init(void)
{
	int ret = platform_driver_register(&ltpi_driver);

	if (ret < 0) {
		pr_err("Failed to register LTPI driver\n");
		return ret;
	}
	return 0;
}

static void __exit ax_ltpi_exit(void)
{
	platform_driver_unregister(&ltpi_driver);
}

late_initcall(ax_ltpi_init);
module_exit(ax_ltpi_exit);

/**
 * Module Parameters
 * @nl_ngpios: Specify the total number of NL GPIOs.
 * @nl_gpio_vals: A comma-separated list of initial values for the GPIO banks.
 * 		Each value corresponds to a 32-bit bank.
 * 		(Chunk 0: GPIO 0-31, Chunk 1: GPIO 32-63, etc.).
 * 		Format: nl_gpio_vals=0x11111111,0x22222222,0x33333333,0x44444444
 */
module_param_named(nl_ngpios, param_nl_ngpios , int, 0444);
MODULE_PARM_DESC(nl_ngpios, " Number of NL GPIOs. Range 1-128.");
module_param_array_named(nl_gpio_vals, param_nl_gpio_vals, long, &param_nl_num_gpio_args, 0444);
MODULE_PARM_DESC(nl_gpio_vals, " Init values for NL GPIO banks. 32-bit chunks.");

MODULE_DESCRIPTION("Axiado LTPI Driver");
MODULE_AUTHOR("Axiado Corporation");
MODULE_LICENSE("GPL");
