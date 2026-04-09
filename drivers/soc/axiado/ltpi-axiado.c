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

#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/jiffies.h>

static DECLARE_WAIT_QUEUE_HEAD(wq);
static bool ready;
static int param_nl_ngpios = -1;
static long param_nl_gpio_vals[MAX_BANKS];
static int param_nl_num_gpio_args = 0;
static int param_ll_ngpios = -1;
static long param_ll_gpio_vals[MAX_BANKS];
static int param_ll_num_gpio_args = 0;

/**
 * wait_ready - Timeout for the ltpi link status check
 * @timeout_ms: Timeout delay
 *  @return: Timedout
 */
int wait_ready(unsigned int timeout_ms) {
	long tout = msecs_to_jiffies(timeout_ms);

	if (!wait_event_timeout(wq, ready, tout))
		return -ETIMEDOUT;

	return 0;
}

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

	if (gc == &ltpi->nl.gc) {
		for (i = 0; i < ltpi->nl.num_banks; i++) {
			u32 bank_val = 0;

			dest[i] = 0;
			switch (mode) {
				case 0: /* inputs */
					bank_val = ltpi_reg_read(ltpi,
							REG_LTPI_NL_DI_ADRS_OFFSET + (i * 4));
					break;
				case 1: /* mask */
					bank_val = ltpi_reg_read(ltpi,
							REG_LTPI_NL_MASK_ADRS_OFFSET + (i * 4));
					break;
				case 2: /* outputs */
					bank_val = ltpi_reg_read(ltpi,
							REG_LTPI_NL_DO_ADRS_OFFSET + (i * 4));
					break;
				default:
					return;
			}
			update_bank(dest, i, bank_val);
		}
	} else {
		for (i = 0; i < ltpi->ll.num_banks; i++) {
			u32 bank_val = 0;

			dest[i] = 0;
			switch (mode) {
				case 0: /* inputs */
					bank_val = ltpi_reg_read(ltpi,
							REG_LTPI_LL_DI_ADRS_OFFSET + (i * 4));
					break;
				case 1: /* mask */
					bank_val = ltpi_reg_read(ltpi,
							REG_LTPI_LL_MASK_ADRS_OFFSET + (i * 4));
					break;
				case 2: /* outputs */
					bank_val = ltpi_reg_read(ltpi,
							REG_LTPI_LL_DO_ADRS_OFFSET + (i * 4));
					break;
				default:
					return;
			}
			update_bank(dest, i, bank_val);
		}
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

	for (i = 0; i < ltpi->nl.num_banks; i++) {
		ltpi_reg_write(ltpi, REG_LTPI_NL_MASK_ADRS_OFFSET + (i * 4),
				ltpi->nl.mask[i]);
	}

	for (i = 0; i < ltpi->ll.num_banks; i++) {
		ltpi_reg_write(ltpi, REG_LTPI_LL_MASK_ADRS_OFFSET + (i * 4),
				ltpi->ll.mask[i]);
	}
}

static inline struct ltpi_gpio *owner_from_hwirq(struct ax3000_ltpi *c, u32 hwirq)
{
	if (hwirq >= c->ll.irq_bit_base &&
			hwirq <  c->ll.irq_bit_base + c->ll.ngpios)
		return &c->ll;

	return &c->nl;
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
	int lines;

	if (gc == &ltpi->nl.gc) {
		lines = ltpi->nl.lines;
		bank_val = ltpi->nl.outputs_cache[GET_BANK(offset)];
	}
	else {
		lines = ltpi->ll.lines;
		bank_val = ltpi->ll.outputs_cache[GET_BANK(offset)];
	}

	if (offset >= lines)
		return;

	mutex_lock(&ltpi->lock);

	if (value)
		bank_val |= BIT(GET_POS(offset));
	else
		bank_val &= ~BIT(GET_POS(offset));

	if (gc == &ltpi->nl.gc) {
		update_bank(ltpi->nl.outputs_cache, GET_BANK(offset), bank_val);
		ltpi_reg_write(ltpi, REG_LTPI_NL_DO_ADRS_OFFSET + (GET_BANK(offset) * 4),
				bank_val);
	} else {
		update_bank(ltpi->ll.outputs_cache, GET_BANK(offset), bank_val);
		ltpi_reg_write(ltpi, REG_LTPI_LL_DO_ADRS_OFFSET + (GET_BANK(offset) * 4),
				bank_val);
	}

	mutex_unlock(&ltpi->lock);
}

/**
 * ax3000_ltpi_get - Get GPIO input value
 * @chip: GPIO chip structure
 * @offset: GPIO offset to read
 * Return: GPIO value (0 or 1) on success, -EINVAL on error
 */
static int ax3000_ltpi_get(struct gpio_chip *gc, unsigned int offset)
{
	struct ax3000_ltpi *ltpi = gpiochip_get_data(gc);
	unsigned int rel_offset;
	u32 bank_val;
	int lines;

	if (gc == &ltpi->nl.gc) {
		lines = ltpi->nl.lines;
		rel_offset = offset - ltpi->nl.lines;
	} else {
		lines = ltpi->ll.lines;
		rel_offset = offset - ltpi->ll.lines;
	}

	if (offset < lines)
		return -EINVAL;

	mutex_lock(&ltpi->lock);

	if (gc == &ltpi->nl.gc)
		bank_val = ltpi_reg_read(ltpi,
				REG_LTPI_NL_DI_ADRS_OFFSET + GET_BANK(rel_offset) * 4);
	else
		bank_val = ltpi_reg_read(ltpi,
				REG_LTPI_LL_DI_ADRS_OFFSET + GET_BANK(rel_offset) * 4);

	mutex_unlock(&ltpi->lock);

	return !!(bank_val & BIT(GET_POS(rel_offset)));
}

static int ax3000_ltpi_dir_in(struct gpio_chip *gc, unsigned int offset)
{
	struct ax3000_ltpi *ltpi = gpiochip_get_data(gc);
	int lines;

	if (gc == &ltpi->nl.gc)
		lines = ltpi->nl.lines;
	else
		lines = ltpi->ll.lines;

	if (offset >= lines)
		return 0;
	else
		return -EINVAL;
}

static int ax3000_ltpi_dir_out(struct gpio_chip *gc, unsigned int offset,
		int value)
{
	struct ax3000_ltpi *ltpi = gpiochip_get_data(gc);
	int lines;

	if (gc == &ltpi->nl.gc)
		lines = ltpi->nl.lines;
	else
		lines = ltpi->ll.lines;

	if (offset < lines) {
		ax3000_ltpi_set(gc, offset, value);
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
	struct ax3000_ltpi *ltpi = irq_data_get_irq_chip_data(d);
	u32 hwirq = irqd_to_hwirq(d);
	struct ltpi_gpio *chip = owner_from_hwirq(ltpi, hwirq);

	if (chip == &ltpi->nl)
		ltpi->nl.mask[GET_BANK(hwirq)] |= (1 << GET_POS(hwirq));
	else
		ltpi->ll.mask[GET_BANK(hwirq)] |= (1 << GET_POS(hwirq));

	schedule_work(&ltpi->ltpi_work);
}

static void ltpi_unmask_irq(struct irq_data *d)
{
	struct ax3000_ltpi *ltpi = irq_data_get_irq_chip_data(d);
	u32 hwirq = irqd_to_hwirq(d);
	struct ltpi_gpio *chip = owner_from_hwirq(ltpi, hwirq);

	if (chip == &ltpi->nl)
		ltpi->nl.mask[GET_BANK(hwirq)] &= ~(1 << GET_POS(hwirq));
	else
		ltpi->ll.mask[GET_BANK(hwirq)] &= ~(1 << GET_POS(hwirq));

	schedule_work(&ltpi->ltpi_work);
}

static int ltpi_irq_reqres(struct irq_data *d)
{
	struct ax3000_ltpi *ltpi = irq_data_get_irq_chip_data(d);
	u32 hwirq = irqd_to_hwirq(d);
	struct ltpi_gpio *chip = owner_from_hwirq(ltpi, hwirq);
	u32 local = hwirq - chip->irq_bit_base;

	if (local >= chip->ngpios)
		return -EINVAL;

	/* Select chip by range and compute local safely */
	if (hwirq >= ltpi->ll.irq_bit_base &&
			hwirq < (ltpi->ll.irq_bit_base + ltpi->ll.lines)) {

		local = hwirq - ltpi->ll.irq_bit_base;

		if (local >= ltpi->ll.lines)
			return -EINVAL;

		return gpiochip_lock_as_irq(&ltpi->ll.gc, local);
	}

	if (hwirq >= ltpi->nl.irq_bit_base &&
			hwirq <  ltpi->nl.irq_bit_base + ltpi->nl.lines) {

		local = hwirq - ltpi->nl.irq_bit_base;

		if (local >= ltpi->nl.lines)
			return -EINVAL;

		return gpiochip_lock_as_irq(&ltpi->nl.gc, local);
	}

	dev_err(ltpi->dev, "reqres: hwirq=%u out of range\n", hwirq);
	return -EINVAL;
}

static void ltpi_irq_relres(struct irq_data *d)
{
	struct ax3000_ltpi *ltpi = irq_data_get_irq_chip_data(d);
	u32 hwirq = irqd_to_hwirq(d);
	struct ltpi_gpio *chip = owner_from_hwirq(ltpi, hwirq);
	u32 local = hwirq - chip->irq_bit_base;

	/* Select chip by range and compute local safely */
	if (hwirq >= ltpi->ll.irq_bit_base &&
			hwirq < (ltpi->ll.irq_bit_base + ltpi->ll.lines)) {

		local = hwirq - ltpi->ll.irq_bit_base;

		dev_info(ltpi->dev, "reqres: hwirq=%u -> LL local=%u\n", hwirq, local);
		return gpiochip_relres_irq(&ltpi->ll.gc, local);
	}

	if (hwirq >= ltpi->nl.irq_bit_base &&
			hwirq <  ltpi->nl.irq_bit_base + ltpi->nl.lines) {

		local = hwirq - ltpi->nl.irq_bit_base;

		dev_info(ltpi->dev, "reqres: hwirq=%u -> NL local=%u\n", hwirq, local);
		return gpiochip_relres_irq(&ltpi->nl.gc, local);
	}
}

static irqreturn_t ltpi_irq_thread(int irq, void *data)
{
	struct ax3000_ltpi *ltpi = (struct ax3000_ltpi *)data;
	u32 current_state[MAX_BANKS];
	u32 changed_bits[MAX_BANKS];
	u32 hwirq;

	unsigned long *cur = (unsigned long *)&current_state;
	unsigned long *cha = (unsigned long *)&changed_bits;
	unsigned long *in = (unsigned long *)ltpi->nl.inputs_cache;
	unsigned int nbit = (unsigned int)ltpi->nl.lines;

	read_all_banks(&ltpi->nl.gc, current_state, 0);

#ifdef CONFIG_AXIADO_LTPI_DEBUGFS
	if (ltpi->is_simulating_irq) {
		pr_info("ltpi irq: fire %d for simulation test\n",
				ltpi->simulating_irq);
		handle_nested_irq(irq_find_mapping(ltpi->irq_domain,
					ltpi->simulating_irq));
		ltpi->is_simulating_irq = false;
		return IRQ_HANDLED;
	}
#endif
	bitmap_xor(cha, cur, in, nbit);
	bitmap_copy(in, cur, nbit);

	for_each_set_bit(hwirq, cha, ltpi->nl.lines) {
		/* unmnask: 0 */
		if (!(ltpi->nl.mask[GET_BANK(hwirq)] & BIT(GET_POS(hwirq)))) {
			pr_debug("ltpi irq: fire %d\n", hwirq);
			handle_nested_irq(irq_find_mapping(ltpi->irq_domain, hwirq));
		}
	}

	read_all_banks(&ltpi->nl.gc, ltpi->nl.inputs_cache, 0);
	ltpi_reg_read(ltpi, REG_LTPI_STATUS_ADRS_OFFSET);

	return IRQ_HANDLED;
}

static int ltpi_irq_domain_map(struct irq_domain *d, unsigned int virq,
		irq_hw_number_t hwirq)
{
	struct ax3000_ltpi *ltpi = d->host_data;

	if (hwirq >= ltpi->total_lines)
		return -EINVAL;

	irq_set_chip_data(virq, ltpi); /* MUST be core */
	irq_set_chip_and_handler(virq, &ltpi->irq, handle_edge_irq);
	irq_set_noprobe(virq);

	return 0;
}

static int ltpi_irq_domain_xlate(struct irq_domain *d,
		struct device_node *ctrlr, const u32 *intspec,
		unsigned int intsize, unsigned long *out_hwirq,
		unsigned int *out_type)
{
	*out_hwirq = intspec[0];
	*out_type = intspec[1];

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

	if (ltpi->simulating_irq >= ltpi->nl.lines)
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
	unsigned int irq = ltpi->nl.gc.irq.parents[0];

	pr_info("debugfs: simulating parent IRQ %d\n", irq);

	handle_nested_irq(irq);

	return 0;
}
DEFINE_DEBUGFS_ATTRIBUTE(fops_simulate_parent_irq, NULL,
		simulate_parent_irq_write, "%llu\n");

static int dump_cache(void *data, u64 val)
{
	struct ax3000_ltpi *ltpi = data;
	int i;

	pr_info("\n inputs_cache:\n");
	for (i = 0; i < ltpi->nl.num_banks; i++)
		pr_info("    bank[%d]: 0x%08x\n", i, ltpi->nl.inputs_cache[i]);
	pr_info(" mask cache:\n");
	for (i = 0; i < ltpi->nl.num_banks; i++)
		pr_info("    bank[%d]: 0x%08x\n", i, ltpi->nl.mask[i]);
	pr_info(" outputs_cache:\n");
	for (i = 0; i < ltpi->nl.num_banks; i++)
		pr_info("    bank[%d]: 0x%08x\n", i, ltpi->nl.outputs_cache[i]);
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

static int link_status_check(struct ax3000_ltpi *ltpi) {
	u8 val;
	val = ((ltpi_reg_read(ltpi, REG_LTPI_LINK_STATUS_OFFSET) >> LOCAL_STATE_SHIFT) & 0xF);

	switch (val) {
		case LINK_DETECT:
		case LINK_SPEED:
		case ADVERTISE:
		case CONFIGURATION:
			return LTPI_LINK_STATUS_FAILED;
			break;
		case OPERATIONAL:
			return LTPI_LINK_STATUS_SUCCESS;
			break;
	}

	return LTPI_LINK_STATUS_FAILED;
}

static void local_capabilities_configure(struct ax3000_ltpi *ltpi) {
	u32 local_val_l;
	u32 local_val_h;
	u32 new_local_val_l = 0;
	u32 new_local_val_h = 0;
	u32 remote_val_l;
	u32 remote_val_h;
	u32 val;

	/* Read the HPM and Local default capapbilities for comparision */
	local_val_l = ltpi_reg_read(ltpi, REG_LTPI_LOCAL_CAPA_LOW_OFFSET);
	local_val_h = ltpi_reg_read(ltpi, REG_LTPI_LOCAL_CAPA_HIGH_OFFSET);
	remote_val_l = ltpi_reg_read(ltpi, REG_LTPI_REMOTE_CAPA_LOW_OFFSET);
	remote_val_h = ltpi_reg_read(ltpi, REG_LTPI_REMOTE_CAPA_HIGH_OFFSET);

	/* LTPI reset bit set */
	val = ltpi_reg_read(ltpi, REG_LTPI_CONTROL_STATUS_REG1_OFFSET);
	val |= RESET_BIT;
	ltpi_reg_write(ltpi, REG_LTPI_CONTROL_STATUS_REG1_OFFSET, val);

	/* Update the local capabilities based on the DTS entry */
	ltpi_reg_write(ltpi, REG_LTPI_LOCAL_CAPA_LOW_OFFSET, ltpi->cap_l);
	ltpi_reg_write(ltpi, REG_LTPI_LOCAL_CAPA_HIGH_OFFSET, ltpi->cap_h);

	/* Compare the Local and HPM capabilities and update the Local configuration */
	new_local_val_l |= (remote_val_l & local_val_l) &
		(LTPI_CAP_GPIO_BIT |
		 LTPI_CAP_I2C_BIT |
		 LTPI_CAP_UART_BIT |
		 LTPI_CAP_DATA_BIT |
		 LTPI_CAP_OEM_BIT |
		 LTPI_CAP_I2C_CH0_BIT |
		 LTPI_CAP_I2C_CH1_BIT |
		 LTPI_CAP_I2C_CH2_BIT |
		 LTPI_CAP_I2C_CH3_BIT |
		 LTPI_CAP_I2C_CH4_BIT |
		 LTPI_CAP_I2C_CH5_BIT |
		 LTPI_CAP_I2C_ECHO_BIT);

	if (((remote_val_l >> LTPI_CAP_NL_GPIO_SHIFT) & 0x3FF) <= NL_MAX_GPIO_PINS)
		new_local_val_l |= (((remote_val_l >> LTPI_CAP_NL_GPIO_SHIFT) & 0x3FF) << LTPI_CAP_NL_GPIO_SHIFT);
	else
		new_local_val_l |= (NL_MAX_GPIO_PINS << 8);

	new_local_val_h |= (remote_val_h & local_val_h) &
		(LTPI_CAP_UART_FLOW_CONTROL_BIT |
		 LTPI_CAP_UART0_EN_BIT |
		 LTPI_CAP_UART1_EN_BIT);

	ltpi_reg_write(ltpi, REG_LTPI_LOCAL_CAPA_LOW_OFFSET, new_local_val_l);
	ltpi_reg_write(ltpi, REG_LTPI_LOCAL_CAPA_HIGH_OFFSET, new_local_val_h);

	/* LTPI reset bit clear */
	val = ltpi_reg_read(ltpi, REG_LTPI_CONTROL_STATUS_REG1_OFFSET);
	val &= ~RESET_BIT;
	ltpi_reg_write(ltpi, REG_LTPI_CONTROL_STATUS_REG1_OFFSET, val);
}

/* GPIO to IRQ (required for gpiomon) */
static int ltpi_to_irq(struct gpio_chip *gc, unsigned int offset)
{
	struct ax3000_ltpi *ltpi = gpiochip_get_data(gc);
	int base;
	int virq;

	if (gc == &ltpi->nl.gc) {
		base = ltpi->nl.irq_bit_base;
		offset -= ltpi->nl.lines;
	}
	else if (gc == &ltpi->ll.gc) {
		base = ltpi->ll.irq_bit_base;
		offset -= ltpi->ll.lines;
	}
	else
		return -EINVAL;

	u32 hwirq = base + offset;
	virq = irq_find_mapping(ltpi->irq_domain, hwirq);

	return virq;
}

/* ----- init one gpiochip wrapper ----- */
static void ltpi_init_chip(struct ax3000_ltpi *ltpi, struct ltpi_gpio *chip,
		enum gpio_type type, const char *label, u32 ngpios,
		u32 irq_base, struct fwnode_handle *fwnode)
{
	chip->core = ltpi;
	chip->type = type;
	chip->ngpios = ngpios;
	chip->irq_bit_base = irq_base;

	chip->gc.label = label;
	chip->gc.parent = ltpi->dev;
	chip->gc.owner = THIS_MODULE;
	chip->gc.base = -1;
	chip->gc.ngpio = ngpios * 2;
	chip->gc.fwnode = fwnode;

	chip->gc.get = ax3000_ltpi_get;
	chip->gc.set = ax3000_ltpi_set;
	chip->gc.direction_input = ax3000_ltpi_dir_in;
	chip->gc.direction_output = ax3000_ltpi_dir_out;
	chip->gc.to_irq = ltpi_to_irq;
}

/*
 * ltpi_gpio_probe - gpio probe after link established
 * @ltpi: a pointer for struct ax3000_ltpi
 */
static int ltpi_gpio_probe(struct ax3000_ltpi *ltpi)
{
	struct platform_device *pdev =
		container_of_const(ltpi->dev, struct platform_device, dev);
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct device_node *nl_node, *ll_node;
	int irq, rc, i;

	nl_node = of_get_child_by_name(np, "nl-gpio");
	ll_node = of_get_child_by_name(np, "ll-gpio");

	if (!nl_node || !ll_node) {
		dev_err(dev, "need nl-gpio and ll-gpio child nodes\n");
		rc = -EINVAL;
		goto out_put;
	}

	if (of_property_read_u32(nl_node, "ngpios", &ltpi->nl.lines) ||
			of_property_read_u32(ll_node, "ngpios", &ltpi->ll.lines) ||
			of_property_read_u32_array(nl_node, "dout-init",
				ltpi->nl.outputs_cache, ltpi->nl.num_banks) ||
			of_property_read_u32_array(ll_node, "dout-init",
				ltpi->ll.outputs_cache, ltpi->ll.num_banks)) {
		dev_err(dev, "missing DT properties (ngpios, dout-init)\n");
		rc = -EINVAL;
		goto out_put;
	}

	if (param_nl_ngpios != -1)
		ltpi->nl.lines = param_nl_ngpios;
	if (param_ll_ngpios != -1)
		ltpi->ll.lines = param_ll_ngpios;

	if (ltpi->nl.lines > NL_MAX_GPIO_PINS) {
		dev_err(&pdev->dev,
				"lines-per-function is greater than %d pins\n", NL_MAX_GPIO_PINS);
		return -EINVAL;
	}

	if (ltpi->ll.lines > LL_MAX_GPIO_PINS) {
		dev_err(&pdev->dev,
				"lines-per-function is greater than %d pins\n", LL_MAX_GPIO_PINS);
		return -EINVAL;
	}

	ltpi->nl.num_banks = (ltpi->nl.lines + PINS_PER_GPIO_BANK - 1) / PINS_PER_GPIO_BANK;
	ltpi->ll.num_banks = (ltpi->ll.lines + PINS_PER_GPIO_BANK - 1) / PINS_PER_GPIO_BANK;

	dev_dbg(&pdev->dev, "nl_lines %d, banks %d\n", ltpi->nl.lines, ltpi->nl.num_banks);
	dev_dbg(&pdev->dev, "ll_lines %d, banks %d\n", ltpi->ll.lines, ltpi->ll.num_banks);

	ltpi->nl.mask = devm_kcalloc(&pdev->dev, ltpi->nl.num_banks, sizeof(u32),
			GFP_KERNEL);

	ltpi->ll.mask = devm_kcalloc(&pdev->dev, ltpi->ll.num_banks, sizeof(u32),
			GFP_KERNEL);

	ltpi->nl.inputs_cache = devm_kcalloc(&pdev->dev, ltpi->nl.num_banks,
			sizeof(u32), GFP_KERNEL);

	ltpi->ll.inputs_cache = devm_kcalloc(&pdev->dev, ltpi->ll.num_banks,
			sizeof(u32), GFP_KERNEL);

	ltpi->nl.outputs_cache = devm_kcalloc(&pdev->dev, ltpi->nl.num_banks,
			sizeof(u32), GFP_KERNEL);

	ltpi->ll.outputs_cache = devm_kcalloc(&pdev->dev, ltpi->ll.num_banks,
			sizeof(u32), GFP_KERNEL);

	if (!ltpi->nl.mask ||
			!ltpi->ll.mask ||
			!ltpi->nl.inputs_cache ||
			!ltpi->ll.inputs_cache ||
			!ltpi->nl.outputs_cache ||
			!ltpi->ll.outputs_cache)
		return -ENOMEM;

	/* module parameter first */
	if (param_nl_num_gpio_args == ltpi->nl.num_banks) {
		for (i = 0; i < ltpi->nl.num_banks; i++)
			update_bank(ltpi->nl.outputs_cache, i, param_nl_gpio_vals[i]);
	}

	for (i = 0; i < ltpi->nl.num_banks; i++) {
		ltpi_reg_write(ltpi, REG_LTPI_NL_DO_ADRS_OFFSET + (i * 4),
				ltpi->nl.outputs_cache[i]);
		dev_info(&pdev->dev, "Default dout-init[%d] = 0x%08x\n", i,
				ltpi->nl.outputs_cache[i]);
	}

	if (param_ll_num_gpio_args == ltpi->ll.num_banks) {
		for (i = 0; i < ltpi->ll.num_banks; i++)
			update_bank(ltpi->ll.outputs_cache, i, param_ll_gpio_vals[i]);
	}

	for (i = 0; i < ltpi->ll.num_banks; i++) {
		ltpi_reg_write(ltpi, REG_LTPI_LL_DO_ADRS_OFFSET + (i * 4),
				ltpi->ll.outputs_cache[i]);
		dev_info(&pdev->dev, "Default dout-init[%d] = 0x%08x\n", i,
				ltpi->ll.outputs_cache[i]);
	}

	/* total lines must cover both ranges */
	ltpi->total_lines = max(NL_IRQ_BIT_BASE + ltpi->nl.lines,
			LL_IRQ_BIT_BASE + ltpi->ll.lines);
	ltpi->total_banks = DIV_ROUND_UP(ltpi->total_lines, PINS_PER_GPIO_BANK);

	ltpi->irq.name = "LTPI-IRQ";
	ltpi->irq.irq_ack = ltpi_ack_irq;
	ltpi->irq.irq_mask = ltpi_mask_irq;
	ltpi->irq.irq_unmask = ltpi_unmask_irq;
	ltpi->irq.irq_set_type = ltpi_set_irq_type;
	ltpi->irq.irq_request_resources = ltpi_irq_reqres;
	ltpi->irq.irq_release_resources = ltpi_irq_relres;

	ltpi->irq.flags |= IRQCHIP_IMMUTABLE; /* prevent change from gpiolib */

	ltpi->irq_domain = irq_domain_add_linear(np, ltpi->total_lines,
			&ltpi_irq_domain_ops, ltpi);
	if (!ltpi->irq_domain) {
		rc = -ENOMEM;
		goto out_domain;
	}

	for (i=0; i< ltpi->total_lines; i++)
		irq_create_mapping(ltpi->irq_domain, i);

	ltpi_init_chip(ltpi, &ltpi->nl, NL_GPIO, "axiado_ltpi_nl_gpio",
			ltpi->nl.lines, NL_IRQ_BIT_BASE, of_fwnode_handle(nl_node));

	ltpi_init_chip(ltpi, &ltpi->ll, LL_GPIO, "axiado_ltpi_ll_gpio",
			ltpi->ll.lines, LL_IRQ_BIT_BASE, of_fwnode_handle(ll_node));

	irq = irq_of_parse_and_map(pdev->dev.of_node, 0);

	if (!irq) {
		dev_err(&pdev->dev, "ltpi : failed to get irq = %d\n", irq);
		return -EINVAL;
	}

	/* mask all interrupts */
	for (i = 0; i < ltpi->nl.num_banks; i++) {
		ltpi->nl.mask[i] = 0xffffffff;
		ltpi_reg_write(ltpi, REG_LTPI_NL_MASK_ADRS_OFFSET + (i * 4),
				ltpi->nl.mask[i]);
	}

	for (i = 0; i < ltpi->ll.num_banks; i++) {
		ltpi->ll.mask[i] = 0xffffffff;
		ltpi_reg_write(ltpi, REG_LTPI_LL_MASK_ADRS_OFFSET + (i * 4),
				ltpi->ll.mask[i]);
	}

	rc = devm_gpiochip_add_data(&pdev->dev, &ltpi->nl.gc, ltpi);
	if (rc < 0) {
		dev_err(&pdev->dev, "Could not register gpiochip, %d\n", rc);
		irq_domain_remove(ltpi->irq_domain);
		return rc;
	}

	rc = devm_gpiochip_add_data(&pdev->dev, &ltpi->ll.gc, ltpi);
	if (rc < 0) {
		dev_err(&pdev->dev, "Could not register gpiochip, %d\n", rc);
		irq_domain_remove(ltpi->irq_domain);
		return rc;
	}

	rc = devm_request_threaded_irq(&pdev->dev, irq, NULL, ltpi_irq_thread,
			IRQF_ONESHOT, "ltpi-gpio", ltpi);
	if (rc) {
		dev_err(&pdev->dev, "Failed to request parent IRQ %d: %d\n",
				irq, rc);
		return rc;
	}

#ifdef CONFIG_AXIADO_LTPI_DEBUGFS
	if (ltpi->irq_domain == irq_find_host(pdev->dev.of_node))
		pr_info("LTPI PROBE SUCCESS: IRQ domain was correctly registered.\n");

	setup_ltpi_debugfs(ltpi);
#endif
	read_all_banks(&ltpi->nl.gc, ltpi->nl.inputs_cache, 0);
	read_all_banks(&ltpi->ll.gc, ltpi->ll.inputs_cache, 0);

	return 0;

out_put:
	if (nl_node) of_node_put(nl_node);
	if (ll_node) of_node_put(ll_node);
	return rc;

out_domain:
	irq_domain_remove(ltpi->irq_domain);
	goto out_put;
}

/*
 * ltpi_link_workqueue - Workqueue function to ltpi link state
 * @work: Work structure containing LTPI device data
 */
static void ltpi_link_workqueue(struct work_struct *work)
{
	struct ax3000_ltpi *ltpi =
		container_of_const(work, struct ax3000_ltpi, ltpi_link_work.work);
	int rc = 0;
	u32 val;

	if (link_status_check(ltpi))
		local_capabilities_configure(ltpi);

	rc = regmap_read(ltpi->regmap, ltpi->regmap_base_offset, &val);

	if (!rc && (val & LINK_STATUS_READY_BIT)) {
		dev_info(ltpi->dev, "LTPI Link is in Operational state\n");
		return;
	}
	if (ltpi->link_retry < LTPI_LINK_MAX_RETRIES) {
		ltpi->link_retry++;
		queue_delayed_work(system_wq, &ltpi->ltpi_link_work,
				msecs_to_jiffies(LTPI_LINK_RETRY_INTERVAL_MS));
	} else {
		dev_err(ltpi->dev, "LTPI Link training timed out after %d retries\n",
				LTPI_LINK_MAX_RETRIES);
	}
}

static ssize_t link_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct ax3000_ltpi *ltpi = dev_get_drvdata(dev);
	u8 val = ((ltpi_reg_read(ltpi, REG_LTPI_LINK_STATUS_OFFSET) >> LOCAL_STATE_SHIFT) & 0xF);
	switch (val) {
		case LINK_DETECT: return sysfs_emit(buf, "detect\n");
		case LINK_SPEED: return sysfs_emit(buf, "speed\n");
		case ADVERTISE: return sysfs_emit(buf, "advertise\n");
		case CONFIGURATION: return sysfs_emit(buf, "configuration\n");
		case OPERATIONAL: return sysfs_emit(buf, "operational\n");
	}
	return 0;
}
static DEVICE_ATTR_RO(link);
static struct attribute *link_attrs[] = {
	&dev_attr_link.attr,
	NULL,
};
static const struct attribute_group link_attr_group = {
	.attrs = link_attrs
};

/**
 * ltpi_probe - Probe function for LTPI GPIO driver
 * @spi: Platform device structure
 * Return: 0 on success, negative error code on failure
 */
static int ltpi_probe(struct platform_device *spi)
{
	struct ax3000_ltpi *ltpi;
	int rc = 0;
	u32 nl_gpio_count;
	u32 i2c_channels;

#ifdef CONFIG_AXIADO_LTPI_REGMAP
	u32 reg_base_offset = 0;
#endif
	ltpi = devm_kzalloc(&spi->dev, sizeof(*ltpi), GFP_KERNEL);
	if (!ltpi)
		return -ENOMEM;

	ltpi->dev = &spi->dev;
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

	if (device_property_read_bool(&spi->dev, "ltpi-support-gpio"))
		ltpi->cap_l |= LTPI_CAP_GPIO_BIT;

	if (device_property_read_bool(&spi->dev, "ltpi-support-i2c"))
		ltpi->cap_l |= LTPI_CAP_I2C_BIT;

	if (device_property_read_bool(&spi->dev, "ltpi-support-uart"))
		ltpi->cap_l |= LTPI_CAP_UART_BIT;

	if (device_property_read_bool(&spi->dev, "ltpi-support-data"))
		ltpi->cap_l |= LTPI_CAP_DATA_BIT;

	if (device_property_read_bool(&spi->dev, "ltpi-support-oem"))
		ltpi->cap_l |= LTPI_CAP_OEM_BIT;

	rc = device_property_read_u32(&spi->dev, "ltpi-support-nl-gpio-count", &nl_gpio_count);

	if (rc < 0) {
		dev_err(&spi->dev, "could not read ltpi-support-nl-gpio-count property\n");
		return -EINVAL;
	}

	ltpi->cap_l |= (nl_gpio_count << LTPI_CAP_NL_GPIO_SHIFT);

	rc = device_property_read_u32(&spi->dev, "ltpi-support-i2c-channels", &i2c_channels);

	if (rc < 0) {
		dev_err(&spi->dev, "could not read ltpi-support-i2c-channels property\n");
		return -EINVAL;
	}

	ltpi->cap_l |= (i2c_channels << LTPI_CAP_I2C_SHIFT);

	if (device_property_read_bool(&spi->dev, "ltpi-support-i2c-echo"))
		ltpi->cap_l |= LTPI_CAP_I2C_ECHO_BIT;

	if (device_property_read_bool(&spi->dev, "ltpi-support-uart-flow-control"))
		ltpi->cap_h |= LTPI_CAP_UART_FLOW_CONTROL_BIT;

	if (device_property_read_bool(&spi->dev, "ltpi-support-uart0"))
		ltpi->cap_h |= LTPI_CAP_UART0_EN_BIT;

	if (device_property_read_bool(&spi->dev, "ltpi-support-uart1"))
		ltpi->cap_h |= LTPI_CAP_UART1_EN_BIT;


	INIT_WORK(&ltpi->ltpi_work, ltpi_workqueue);
	INIT_DELAYED_WORK(&ltpi->ltpi_link_work, ltpi_link_workqueue);
	mutex_init(&ltpi->lock);

	ltpi->link_retry = 0;

	/* The initial state of the LTPI GPIO output can affect how the remote eFUSE behaves
	 * during the power-on sequence. Therefore, it is important to initialize the GPIO
	 * before the LTPI link is established.
	 */
	rc = ltpi_gpio_probe(ltpi);
	if (rc) {
		dev_err(ltpi->dev, "LTPI GPIO probe fails\n");
		return rc;
	}
	queue_delayed_work(system_wq, &ltpi->ltpi_link_work, msecs_to_jiffies(100));

	rc = sysfs_create_group(&spi->dev.kobj, &link_attr_group);
	if (rc)
		dev_err(&spi->dev, "Fail to create sysfs\n");
	return rc;
}

static int ltpi_remove(struct platform_device *spi)
{
	struct ax3000_ltpi *ltpi = platform_get_drvdata(spi);
	int hwirq;

	cancel_work_sync(&ltpi->ltpi_work);
	cancel_delayed_work_sync(&ltpi->ltpi_link_work);

	sysfs_remove_group(&spi->dev.kobj, &link_attr_group);

#ifdef CONFIG_AXIADO_LTPI_DEBUGFS
	debugfs_remove_recursive(ltpi->debugfs_root);
#endif
	for (hwirq = 0; hwirq < ltpi->total_lines; hwirq++) {
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
 *      Each value corresponds to a 32-bit bank.
 *      (Chunk 0: GPIO 0-31, Chunk 1: GPIO 32-63, etc.).
 *      Format: nl_gpio_vals=0x11111111,0x22222222,0x33333333,0x44444444
 */
module_param_named(nl_ngpios, param_nl_ngpios , int, 0444);
MODULE_PARM_DESC(nl_ngpios, " Number of NL GPIOs. Range 1-128.");
module_param_array_named(nl_gpio_vals, param_nl_gpio_vals, long, &param_nl_num_gpio_args, 0444);
MODULE_PARM_DESC(nl_gpio_vals, " Init values for NL GPIO banks. 32-bit chunks.");

/**
 * Module Parameters
 * @ll_ngpios: Specify the total number of LL GPIOs.
 * @ll_gpio_vals: A comma-separated list of initial values for the GPIO banks.
 *      LL GPIO has a single 32-bit bank.
 *      Format: ll_gpio_vals=0x00000000
 */
module_param_named(ll_ngpios, param_ll_ngpios , int, 0444);
MODULE_PARM_DESC(ll_ngpios, " Number of LL GPIOs. Range 1-32.");
module_param_array_named(ll_gpio_vals, param_ll_gpio_vals, long, &param_ll_num_gpio_args, 0444);
MODULE_PARM_DESC(ll_gpio_vals, " Init values for LL GPIO banks. 32-bit chunks.");

MODULE_DESCRIPTION("Axiado LTPI Driver");
MODULE_AUTHOR("Axiado Corporation");
MODULE_LICENSE("GPL");
