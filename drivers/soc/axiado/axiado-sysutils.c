// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * SYSUTILS driver for Axiado
 *
 * Copyright (C) 2026 Axiado Corporation.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/io.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/idr.h>
#include <linux/sysfs.h>
#include <linux/slab.h>
#include <linux/align.h>
#include <linux/ioport.h>
#include <linux/types.h>

#include "axiado-sysutils.h"

#define AXIADO_SYSUTILS_MAX_MINORS 256

static dev_t axiado_sysutils_devt_base;
static struct class *axiado_sysutils_class;
static DEFINE_IDA(axiado_sysutils_ida);


struct ax_board_map {
	u32 id;
	const char *name;
};

static const struct ax_board_map ax_board_map_tbl[] = {

	{ 0x486f7065, "TST3000" },
	{ 0x486f706C, "TST3081" },

	{ 0x7661696D, "SCM3001" },
	{ 0x7661696E, "SCM3001" },
	{ 0x76616970, "SCM3001" },

	{ 0x736e6f77, "SCM3002" },
	{ 0x736E6F78, "SCM3002-V2" },

	{ 0x6D54686F, "SCM3003" },
	{ 0x6D546870, "SCM3003-V2" },

	{ 0x6D546873, "SCM3080-MT" },
	{ 0x6D546876, "SCM3080-MT" },
};

static const char *axiado_board_name(u32 id)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ax_board_map_tbl); i++) {
		if (ax_board_map_tbl[i].id == id)
			return ax_board_map_tbl[i].name;
	}
	return "unknown";
}

static ssize_t board_id_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	struct sysutils_dev *mdev = dev_get_drvdata(dev);

	if (!mdev)
		return -EINVAL;

	return sysfs_emit(buf, "0x%08x\n", mdev->board_id);
}
static DEVICE_ATTR_RO(board_id);

static ssize_t board_name_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct sysutils_dev *mdev = dev_get_drvdata(dev);

	if (!mdev)
		return -EINVAL;

	return sysfs_emit(buf, "%s\n", axiado_board_name(mdev->board_id));
}
static DEVICE_ATTR_RO(board_name);

static void sysutils_unregister_chardev(struct sysutils_dev *mdev)
{
	if (!mdev)
		return;

	if (mdev->devnode_created) {
		device_destroy(axiado_sysutils_class, mdev->devt);
		mdev->devnode_created = false;
	}

	if (mdev->cdev_added) {
		cdev_del(&mdev->cdev);
		mdev->cdev_added = false;
	}

	if (mdev->minor >= 0) {
		ida_free(&axiado_sysutils_ida, mdev->minor);
		mdev->minor = -1;
	}

	mdev->devt = 0;
}

static int sysutils_register_chardev(struct sysutils_dev *mdev,
				     const struct file_operations *fops)
{
	int ret;

	mdev->minor = ida_alloc_max(&axiado_sysutils_ida,
				    AXIADO_SYSUTILS_MAX_MINORS - 1, GFP_KERNEL);
	if (mdev->minor < 0)
		return mdev->minor;

	mdev->devt = MKDEV(MAJOR(axiado_sysutils_devt_base), mdev->minor);

	cdev_init(&mdev->cdev, fops);
	mdev->cdev.owner = THIS_MODULE;

	ret = cdev_add(&mdev->cdev, mdev->devt, 1);
	if (ret)
		goto err;

	mdev->cdev_added = true;

	if (IS_ERR(device_create(axiado_sysutils_class, mdev->dev, mdev->devt,
				 mdev, "%s", mdev->devnode_name))) {
		ret = -EINVAL;
		goto err;
	}

	mdev->devnode_created = true;
	return 0;

err:
	sysutils_unregister_chardev(mdev);
	return ret;
}

static ssize_t sysutils_logger_read(struct sysutils_dev *mdev, char __user *buf,
				    size_t count)
{
	u32 current_write_ptr;
	u32 read_ptr;
	size_t num_bytes_to_read;
	bool wrap_around = false;
	char *tmp;

	if (readl(&mdev->log->magic_number) != AX_LOG_MAGIC_NUMBER)
		return 0;

	current_write_ptr = ALIGN_DOWN(readl(&mdev->log->write_ptr), 4);
	read_ptr = readl(&mdev->log->read_ptr);

	if ((read_ptr & 0x3) != 0) {
		read_ptr = ALIGN_DOWN(read_ptr, 4);
		if (read_ptr >= AX_LOG_BUFFER_SIZE)
			read_ptr = 0;
		writel(read_ptr, &mdev->log->read_ptr);
	}

	if (current_write_ptr >= read_ptr) {
		num_bytes_to_read = current_write_ptr - read_ptr;
	} else {
		num_bytes_to_read = AX_LOG_BUFFER_SIZE - read_ptr;
		if (num_bytes_to_read <= AX_LOG_BUFF_LIMIT)
			wrap_around = true;
	}

	if (count > num_bytes_to_read)
		count = num_bytes_to_read;

	if (!count)
		return 0;

	tmp = kmalloc(count, GFP_KERNEL);
	if (!tmp)
		return -ENOMEM;

	memcpy_fromio(tmp, (void __iomem *)(mdev->log->buffer + read_ptr),
		      count);

	if (copy_to_user(buf, tmp, count)) {
		kfree(tmp);
		return -EFAULT;
	}

	kfree(tmp);

	if (wrap_around)
		writel(0, &mdev->log->read_ptr);
	else
		writel(read_ptr + count, &mdev->log->read_ptr);

	read_ptr = readl(&mdev->log->read_ptr);
	if ((read_ptr & 0x3) != 0)
		writel(ALIGN_DOWN(read_ptr, 4), &mdev->log->read_ptr);

	return count;
}

static int sysutils_fwupgrade_mmap(struct sysutils_dev *mdev,
				   struct vm_area_struct *vma)
{
	unsigned long size;
	unsigned long pfn;

	if (vma->vm_pgoff != 0)
		return -EINVAL;

	size = vma->vm_end - vma->vm_start;
	if (size > mdev->size)
		return -ENOMEM;

	vm_flags_set(vma, VM_IO | VM_DONTEXPAND | VM_DONTDUMP);

	pfn = (unsigned long)(mdev->phys_base >> PAGE_SHIFT);

	return remap_pfn_range(vma, vma->vm_start, pfn, size,
			       vma->vm_page_prot);
}

static int sysutils_open(struct inode *inode, struct file *file)
{
	struct sysutils_dev *mdev;

	mdev = container_of(inode->i_cdev, struct sysutils_dev, cdev);
	file->private_data = mdev;
	return 0;
}

static ssize_t sysutils_read(struct file *file, char __user *buf, size_t count,
			     loff_t *ppos)
{
	struct sysutils_dev *mdev = file->private_data;

	if (mdev->type == SYSUTILS_LOGGER)
		return sysutils_logger_read(mdev, buf, count);

	return -EINVAL;
}

static int sysutils_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct sysutils_dev *mdev = file->private_data;

	if (mdev->type == SYSUTILS_FWUPGRADE)
		return sysutils_fwupgrade_mmap(mdev, vma);

	return -EINVAL;
}

static const struct file_operations sysutils_fops = {
	.owner = THIS_MODULE,
	.open = sysutils_open,
	.read = sysutils_read,
	.mmap = sysutils_mmap,
};

static int sysutils_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;
	struct sysutils_dev *mdev;
	struct resource *res;
	int ret;

	match = of_match_device(of_match_ptr(pdev->dev.driver->of_match_table),
				&pdev->dev);
	if (!match)
		return -ENODEV;

	mdev = devm_kzalloc(&pdev->dev, sizeof(*mdev), GFP_KERNEL);
	if (!mdev)
		return -ENOMEM;

	mdev->dev = &pdev->dev;
	mdev->minor = -1;

	platform_set_drvdata(pdev, mdev);

	mdev->type = (enum sysutils_type)match->data;

	switch (mdev->type) {
	case SYSUTILS_LOGGER:
		res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if (!res)
			return -ENODEV;

		mdev->base = devm_ioremap_resource(&pdev->dev, res);
		if (IS_ERR(mdev->base))
			return PTR_ERR(mdev->base);

		mdev->log = (struct ax_log_t __iomem *)mdev->base;

		if (of_property_read_string(pdev->dev.of_node, "node-name",
					    &mdev->devnode_name))
			mdev->devnode_name = dev_name(&pdev->dev);

		return sysutils_register_chardev(mdev, &sysutils_fops);

	case SYSUTILS_FWUPGRADE:
		res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if (!res)
			return -ENODEV;

		mdev->phys_base = res->start;
		mdev->size = resource_size(res);
		mdev->devnode_name = "axfwupdate";

		return sysutils_register_chardev(mdev, &sysutils_fops);

	case SYSUTILS_BOARD_ID:
		res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if (!res)
			return -ENODEV;

		mdev->base = devm_ioremap_resource(&pdev->dev, res);
		if (IS_ERR(mdev->base))
			return PTR_ERR(mdev->base);

		mdev->board_id = readl(mdev->base);

		ret = device_create_file(&pdev->dev, &dev_attr_board_id);
		if (ret)
			return ret;

		ret = device_create_file(&pdev->dev, &dev_attr_board_name);
		if (ret) {
			device_remove_file(&pdev->dev, &dev_attr_board_id);
			return ret;
		}
		return 0;
	}

	return -ENODEV;
}

static int sysutils_remove(struct platform_device *pdev)
{
	struct sysutils_dev *mdev = platform_get_drvdata(pdev);

	if (!mdev)
		return 0;

	if (mdev->type == SYSUTILS_BOARD_ID) {
		device_remove_file(&pdev->dev, &dev_attr_board_name);
		device_remove_file(&pdev->dev, &dev_attr_board_id);
		return 0;
	}

	sysutils_unregister_chardev(mdev);
	return 0;
}

static const struct of_device_id sysutils_of_match[] = {
	{ .compatible = "axiado,ax-logger", .data = (void *)SYSUTILS_LOGGER },
	{ .compatible = "axiado,ax-fwupgrade",
	  .data = (void *)SYSUTILS_FWUPGRADE },
	{ .compatible = "axiado,board-id", .data = (void *)SYSUTILS_BOARD_ID },
	{}
};
MODULE_DEVICE_TABLE(of, sysutils_of_match);

static struct platform_driver sysutils_driver = {
	.probe  = sysutils_probe,
	.remove = sysutils_remove,
	.driver = {
		.name = "axiado-sysutils",
		.of_match_table = sysutils_of_match,
	},
};

static int __init sysutils_init(void)
{
	int ret;

	ret = alloc_chrdev_region(&axiado_sysutils_devt_base, 0,
				  AXIADO_SYSUTILS_MAX_MINORS,
				  "axiado-sysutils");
	if (ret)
		return ret;

	axiado_sysutils_class = class_create("axiado-sysutils");
	if (IS_ERR(axiado_sysutils_class)) {
		ret = PTR_ERR(axiado_sysutils_class);
		unregister_chrdev_region(axiado_sysutils_devt_base,
					 AXIADO_SYSUTILS_MAX_MINORS);
		return ret;
	}

	ret = platform_driver_register(&sysutils_driver);
	if (ret) {
		class_destroy(axiado_sysutils_class);
		unregister_chrdev_region(axiado_sysutils_devt_base,
					 AXIADO_SYSUTILS_MAX_MINORS);
		return ret;
	}

	return 0;
}

static void __exit sysutils_exit(void)
{
	platform_driver_unregister(&sysutils_driver);
	class_destroy(axiado_sysutils_class);
	unregister_chrdev_region(axiado_sysutils_devt_base,
				 AXIADO_SYSUTILS_MAX_MINORS);
	ida_destroy(&axiado_sysutils_ida);
}

module_init(sysutils_init);
module_exit(sysutils_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sammai Chakravarthy <schakravarthy@axiado.com>");
MODULE_DESCRIPTION("Axiado unified sysutils driver");
