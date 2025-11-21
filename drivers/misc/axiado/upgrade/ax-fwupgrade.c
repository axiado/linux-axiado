// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Axiado FW Update Driver
 *
 * Copyright (c) 2025 Axiado Corporation
 */

#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/mm.h>
#include <linux/cdev.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include "ax-fwupgrade.h"

static LIST_HEAD(device_list);
static DECLARE_BITMAP(minors, AXDEV_MAX_DEVICES);

static int axdev_open(struct inode *inode, struct file *file)
{
	struct ax_upgrade_device *axdev;

	axdev = container_of(inode->i_cdev, struct ax_upgrade_device, ax_cdev);
	if (!axdev) {
		pr_err("Device opening failed\n");
		return -EINVAL;
	}

	file->private_data = axdev;

	pr_info("AXIADO Upgrade Device opened\n");
	return 0;
}

static int axdev_release(struct inode *inode, struct file *file)
{
	pr_info("AXIADO Upgrade Device closed\n");
	return 0;
}

static int axdev_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct ax_upgrade_device *axdev = NULL;
	unsigned long pfn = 0;

	if (!file) {
		pr_err(" File NULL pointer\n");
		return -EINVAL;
	}

	if (!vma) {
		pr_err(" vma NULL pointer\n");
		return -EINVAL;
	}

	axdev = file->private_data;
	if (!axdev) {
		pr_err(" Device is NULL pointer\n");
		return -EINVAL;
	}

	pfn = axdev->data_buf_base >> PAGE_SHIFT;

	if ((vma->vm_end - vma->vm_start) > axdev->data_buf_size) {
		pr_info("Upgrade: requested memory is not allowed!!\n");
		return -ENOMEM;
	}

	return remap_pfn_range(vma, vma->vm_start, pfn, vma->vm_end - vma->vm_start,
			vma->vm_page_prot);
}

/* Character device interface */
static const struct file_operations axdev_fops = {
	.owner =	THIS_MODULE,
	.mmap =		axdev_mmap,
	.open =		axdev_open,
	.release =	axdev_release,
};

static int ax_upgrade_probe(struct platform_device *pdev)
{
	struct ax_upgrade_device *axdev;
	struct resource *dts_data_ptr_resource;
	struct device *dev;
	int ret = 0;
	unsigned long minor;

	dev_info(&pdev->dev, "Axiado Firmware upgrade driver probed!");

	/* Allocate memory for the device structure */
	axdev = devm_kzalloc(&pdev->dev, sizeof(*axdev), GFP_KERNEL);
	if (!axdev)
		return -ENOMEM;

	/* Get the shared memory resource from the device tree */
	dts_data_ptr_resource = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	axdev->data_buf_base = dts_data_ptr_resource->start;
	axdev->data_buf_size = resource_size(dts_data_ptr_resource);
	pr_debug("Data buffer range: %llx, size %llx\n",
		 axdev->data_buf_base,
		 axdev->data_buf_size);

	ret = alloc_chrdev_region(&axdev->ax_dev, 0, 1, "ax");
	if (ret < 0)
		return ret;

	cdev_init(&axdev->ax_cdev, &axdev_fops);

	/* Check if the axdev_class already exists, if not then create it.If it does exist,
	 * then just use it
	 */
	if (!axdev->axdev_class) {
		axdev->axdev_class = class_create("upgrade");
		if (IS_ERR(axdev->axdev_class)) {
			dev_err(&pdev->dev, "Failed to create axdev class\n");
			return PTR_ERR(axdev->axdev_class);
		}
	}

	INIT_LIST_HEAD(&axdev->device_entry);

	minor = find_first_zero_bit(minors, AXDEV_MAX_DEVICES);
	if (minor < AXDEV_MAX_DEVICES) {
		axdev->ax_dev = MKDEV(AXDEV_MAJOR, minor);
		dev = device_create(axdev->axdev_class, &pdev->dev, axdev->ax_dev,
				    axdev, "axfwupdate");
		ret = PTR_ERR_OR_ZERO(dev);
	} else {
		dev_dbg(&pdev->dev, "no minor number available!\n");
		ret = -ENODEV;
	}
	if (ret == 0)
		set_bit(minor, minors);

	/* Register the character device */
	ret = cdev_add(&axdev->ax_cdev, axdev->ax_dev, 1);
	if (ret < 0) {
		device_destroy(axdev->axdev_class, axdev->ax_dev);
		clear_bit(minor, minors);
		return ret;
	}

	/* Add the device to the global list */
	list_add(&axdev->device_entry, &device_list);

	/* Store the device structure in the platform data */
	platform_set_drvdata(pdev, axdev);

	return 0;
}

static int ax_upgrade_remove(struct platform_device *pdev)
{
	struct ax_upgrade_device *axdev = platform_get_drvdata(pdev);

	/* Clean up operations */
	list_del(&axdev->device_entry);
	cdev_del(&axdev->ax_cdev);
	device_destroy(axdev->axdev_class, axdev->ax_dev);
	clear_bit(MINOR(axdev->ax_dev), minors);
	class_destroy(axdev->axdev_class);

	return 0;
}

static const struct of_device_id ax_upgrade_of_match[] = {
	{ .compatible = "axiado,ax-fwupgrade", },
	{ },
};

MODULE_DEVICE_TABLE(of, ax_upgrade_of_match);

static struct platform_driver ax_upgrade_driver = {
	.driver = {
	.name = "ax-fwupgrade",
	.of_match_table = ax_upgrade_of_match,
	},
	.probe = ax_upgrade_probe,
	.remove = ax_upgrade_remove,
};

module_platform_driver(ax_upgrade_driver);

MODULE_DESCRIPTION("Axiado Firmware-upgrade driver");
MODULE_LICENSE("GPL");
