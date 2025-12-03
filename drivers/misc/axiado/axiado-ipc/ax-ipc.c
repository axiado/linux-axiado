// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Copyright (c) 2021-25 Axiado Corporation (or its affiliates).
 */

#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/smp.h>
#include <linux/irqdomain.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/mm.h>
#include "ax-ipc.h"

static LIST_HEAD(device_list);
static DECLARE_BITMAP(minors, AXDEV_MAX_DEVICES);

/**
 * @brief This function is used to read data from the RX queue
 * @param queue Pointer to the queue struct
 * @param ipc_pkt Pointer to the ipc_pkt_s struct
 * @return Returns 0 on success or error code on failure
 */
static int queue_read(struct ax_queue_s *queue, struct ipc_pkt_s *ipc_pkt)
{
	if (!queue || !ipc_pkt) {
		pr_err("Invalid parameter\n");
		return -EINVAL;
	}

	if (queue->read_ptr != queue->write_ptr) {
		memcpy((void *)ipc_pkt,
		       (const void *)&queue->ipc_pkt[queue->read_ptr],
		       sizeof(*ipc_pkt));

		/* After having read the data, increment the read pointer */
		queue->read_ptr = (queue->read_ptr + 1) % QUEUE_SIZE;
	} else {
		pr_info("Queue is empty\n");
		return -ENODATA;
	}

	return 0;
}

/**
 * @brief This function is used to write data to the TX queue
 * @param queue Pointer to the queue struct
 * @param ipc_pkt Pointer to the ipc_pkt_s struct
 * @return Returns 0 on success or error code on failure
 */
static int queue_write(struct ax_queue_s *queue, struct ipc_pkt_s *ipc_pkt)
{
	u32 next_write_ptr = 0;

	if (!queue || !ipc_pkt) {
		pr_err("Invalid parameter\n");
		return -EINVAL;
	}

	next_write_ptr = (queue->write_ptr + 1) % QUEUE_SIZE;
	if (next_write_ptr != queue->read_ptr) {
		memcpy((void *)&queue->ipc_pkt[queue->write_ptr],
		       (const void *)ipc_pkt, sizeof(*ipc_pkt));
		queue->write_ptr = next_write_ptr;
	} else {
		pr_info("Queue is full\n");
		return -ENOSPC;
	}

	return 0;
}

static int ax_clear_intr(struct ax_ipc_device *axdev)
{
	if (!axdev) {
		pr_err("Invalid parameter\n");
		return -EINVAL;
	}

	/* Clear the IPC interrupt based on the Remote
	 * M0 is outier in terms of clearing the int
	 */
	if (strncmp(axdev->ax_remote_id, "M0", 2) == 0)
		writel(CLEAR_M0_INT, axdev->ipc_rx_reg);
	else
		writel(CLEAR_ML_FWL_INT, axdev->ipc_rx_reg);

	/* TODO : Need a better parsing mechanism to clear the interrupt */

	return 0;
}

/**
 * @brief This function is used to trigger the interrupt on the remote CPU
 * @param axdev Pointer to the ax_ipc_device struct
 * @return Returns 0 on success or error code on failure
 */
static int ax_ipc_trigger(struct ax_ipc_device *axdev)
{
	if (!axdev) {
		pr_err("Invalid parameter\n");
		return -EINVAL;
	}

	void __iomem *ipc_trigger_reg = axdev->ipc_tx_reg;

	/* Write a value to the trigger offset to trigger the interrupt */
	writel(TRIG_INT, ipc_trigger_reg);

	return 0;
}

/**
 * @brief This function is used to clear the interrupt triggered from the remote CPU
 */
static irqreturn_t ax_ipc_handler(int irq, void *data)
{
	struct ax_ipc_device *axdev = (struct ax_ipc_device *)data;
	struct kernel_siginfo info;
	struct siginfo *sig_info;
	u32 reg_ep_id = 0;
	u32 reg_ep_id_index = 0;

	pr_debug("AXIADO IPC Interrupt received\n");

#if AX_IPC_TEST_UT
	volatile u32 q_data = 0;
	int ret = 0;
	struct ipc_pkt_s ipc_pkt;

	ret = queue_read(axdev->rx_queue, &ipc_pkt);
	if (ret) {
		pr_err("UT : Failed to read data from the %s rx_queue: %d\n",
		       axdev->ax_remote_id, ret);
		return ret;
	}

	q_data = ipc_pkt.data;
	pr_debug("UT : A53-Data from %s : %d\n", axdev->ax_remote_id, q_data);

	if (ax_clear_intr(axdev))
		pr_err("UT : Failed to clear the interrupt\n");
	pr_debug("UT : AXIADO IPC Interrupt handled\n");
	return IRQ_HANDLED;
#endif

	/* Read the endpoint ID from the RX queue to send the signal
	 * to the correct user space application
	 */
	pr_debug("Read ptr %x\n", axdev->rx_queue->read_ptr);
	if (axdev->rx_queue->read_ptr >= QUEUE_SIZE) {
		pr_err("Read ptr %x\n", axdev->rx_queue->read_ptr);
		if (ax_clear_intr(axdev))
			pr_err("Failed to clear the interrupt\n");
		axdev->rx_queue->read_ptr = 0;

		return IRQ_HANDLED;
	}
	reg_ep_id =
		axdev->rx_queue->ipc_pkt[axdev->rx_queue->read_ptr].endpoint_id;
	pr_debug("A53 Endpoint ID: %x\n", reg_ep_id);

	/* Get the index of the endpoint ID and pass that as the signal num */
	reg_ep_id_index = AX_GET_EP_INDEX(reg_ep_id);
	if (reg_ep_id_index <= MAX_ENDPOINTS &&
	    axdev->ep_task[reg_ep_id_index].task) {
		/* Send signal to the user space application */
		memset(&info, 0, sizeof(struct kernel_siginfo));
		sig_info = (struct siginfo *)&info;
		sig_info->si_signo = reg_ep_id_index;
		sig_info->si_code = SI_QUEUE;

		pr_debug("Sending signal to app with endpoint %x\n", reg_ep_id);
		if (send_sig_info(AX_NOTIFY_SIGNAL, &info,
				  axdev->ep_task[reg_ep_id_index].task) < 0)
			pr_err("Unable to send signal\n");
	} else {
		pr_err("Invalid endpoint ID\n");
	}
	/* Clear the interrupt */
	if (ax_clear_intr(axdev)) {
		pr_err("Failed to clear the interrupt\n");
		return IRQ_NONE;
	}
	pr_debug("AXIADO IPC Interrupt handled\n");

	return IRQ_HANDLED;
}

#if AX_IPC_TEST_UT
/**
 * @brief This is a UT function used to transfer data from the local CPU to the remote CPU
 * @param axdev Pointer to the ax_ipc_device struct
 */
static int txfr_to_remote_cpu_ut(struct ax_ipc_device *axdev)
{
	u32 i = 0;
	u32 ret = 0;
	struct ipc_pkt_s ipc_pkt;

	if (!axdev || !axdev->tx_queue) {
		pr_err("Invalid parameter\n");
		return -EINVAL;
	}

	/* Setting Magic Number in RX Queue to notify Remote CPU that A53 is ready */
	axdev->rx_queue->magic_num = M0_M55_A53_MAGIC_NUM;

	/*
	 * Wait for Remote CPU to set the magic number in the TX queue
	 */
	while (axdev->tx_queue->magic_num != M0_M55_A53_MAGIC_NUM)
		msleep(20);
	pr_info("A53 TX now sending data\n");

	for (i = 400; i < 500; i++) {
		ipc_pkt.data = i;

		ret = queue_write(axdev->tx_queue, &ipc_pkt);
		if (ret) {
			pr_err("Failed to write data to the %s tx_queue: %d\n",
			       axdev->ax_remote_id, ret);
			return ret;
		}

		ret = ax_ipc_trigger(axdev);
		if (ret) {
			pr_err("Failed to trigger %s IPC: %d\n",
			       axdev->ax_remote_id, ret);
			return ret;
		}
		msleep(20);
	}

	return 0;
}
#endif

/**
 * @brief This function is used to transfer data from the local CPU to the remote CPU
 * @param axdev Pointer to the ax_ipc_device struct
 */
static int txfr_to_remote_cpu(struct ax_ipc_device *axdev,
			      struct ipc_pkt_s *ipc_pkt)
{
	u32 ret = 0;
	unsigned long flags;

	if (!axdev) {
		pr_err("axdev is NULL\n");
		return -EINVAL;
	}
	/*Because the function will be called from both user space and kernel space,
	 * a spin lock is added to avoid race conditions.
	 */
	spin_lock_irqsave(&axdev->lock, flags);
	if (!axdev->tx_queue) {
		pr_err("Invalid parameter\n");
		spin_unlock_irqrestore(&axdev->lock, flags);
		return -EINVAL;
	}

	ret = queue_write(axdev->tx_queue, ipc_pkt);
	if (ret) {
		pr_err("Failed to write data to the %s tx_queue: %d\n",
		       axdev->ax_remote_id, ret);
		spin_unlock_irqrestore(&axdev->lock, flags);
		return ret;
	}

	ret = ax_ipc_trigger(axdev);
	if (ret) {
		pr_err("Failed to trigger %s IPC: %d\n", axdev->ax_remote_id,
		       ret);
		spin_unlock_irqrestore(&axdev->lock, flags);
		return ret;
	}

	spin_unlock_irqrestore(&axdev->lock, flags);

	return 0;
}

/**
 * @brief This function is used to transfer data from the remote CPU to the local CPU
 * @param axdev Pointer to the ax_ipc_device struct
 * @param ipc_pkt Pointer to the ipc_pkt_s struct
 */
static int txfr_from_remote_cpu(struct ax_ipc_device *axdev,
				struct ipc_pkt_s *ipc_pkt)
{
	u32 ret = 0;

	if (!axdev || !axdev->rx_queue) {
		pr_err("Invalid parameter\n");
		return -EINVAL;
	}
	/**
	 * TODO: Check if the ep id matches the process that called the recv IOCTL
	 * and honors the in order request sequence from different processes
	 */
	ret = queue_read(axdev->rx_queue, ipc_pkt);
	if (ret) {
		pr_err("Failed to read data from the %s rx_queue: %d\n",
		       axdev->ax_remote_id, ret);
		return ret;
	}

	return 0;
}

/**
 * @brief This function is used to register the endpoint ID for the user space application
 * @param axdev Pointer to the ax_ipc_device struct
 * @param endpoint_id Endpoint ID to be registered
 */
static int ax_register_endpoint(struct ax_ipc_device *axdev, int endpoint_id)
{
	int ep_idx = 0;

	/* Assign an index for the endpoint ID */
	ep_idx = AX_GET_EP_INDEX(endpoint_id);

	if (!axdev) {
		pr_err("Invalid parameter\n");
		return -EINVAL;
	}

	/* Check if it is a valid endpoint */
	if (ep_idx >= MAX_ENDPOINTS) {
		pr_err("Invalid endpoint ID\n");
		return -EINVAL;
	}

	/* Check if the endpoint is already registered */
	if (axdev->ep_task[ep_idx].task) {
		pr_err("Endpoint %x already registered\n", endpoint_id);
		return -EINVAL;
	}

	/* Register the endpoint and capture task info */
	axdev->ep_task[ep_idx].task = get_current();
	pr_info("Registered for notification for endpoint ID %x with index[%x]\n",
		endpoint_id, ep_idx);

	return 0;
}

/*--------------------------------Character Dev Support---------------------------------*/

static int axdev_open(struct inode *inode, struct file *file)
{
	struct ax_ipc_device *axdev;
	int ret = 0;

	axdev = container_of(inode->i_cdev, struct ax_ipc_device, ax_cdev);
	file->private_data = axdev;

	pr_info("AXIADO IPC Device:%s opened\n", axdev->ax_remote_id);

	return ret;
}

static int axdev_release(struct inode *inode, struct file *file)
{
	pr_info("AXIADO IPC Device closed\n");
	return 0;
}

static long axdev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct ax_ipc_device *axdev = file->private_data;
	struct ipc_pkt_s ipc_pkt;
	int reg_ep_id = 0;
	struct buf_pool_s pool_info;
	int ret = 0;

	pr_info("AXIADO IPC Device ioctl\n");

	if (!arg) {
		pr_err("Invalid argument\n");
		return -EINVAL;
	}

	switch (cmd) {
	case AX_IPC_IOC_SEND:
		if (copy_from_user(&ipc_pkt, (struct ipc_pkt_s *)arg,
				   sizeof(ipc_pkt))) {
			pr_err("Data Write : Err!\n");
			return -EFAULT;
		}
		pr_debug("IOCTL WR:Endpoint= %x\n", ipc_pkt.endpoint_id);
		pr_debug("IOCTL WR:Event= %x\n", ipc_pkt.event);
		pr_debug("IOCTL WR:Length= %x\n", ipc_pkt.length);
		pr_debug("IOCTL WR:Data= %llx\n", ipc_pkt.data);
		txfr_to_remote_cpu(axdev, &ipc_pkt);
		break;
	case AX_IPC_IOC_RECV:
		txfr_from_remote_cpu(axdev, &ipc_pkt);
		pr_debug("IOCTL Read:Endpoint= %x\n", ipc_pkt.endpoint_id);
		pr_debug("IOCTL Read:Event= %x\n", ipc_pkt.event);
		pr_debug("IOCTL Read:Length= %x\n", ipc_pkt.length);
		pr_debug("IOCTL Read:Data= %llx\n", ipc_pkt.data);
		if (copy_to_user((struct ipc_pkt_s *)arg, &ipc_pkt,
				 sizeof(ipc_pkt))) {
			pr_err("IOCTL RD: Err!\n");
			return -EFAULT;
		}
		break;
	case AX_IPC_GET_BUF_SIZE:
		pool_info.phys_base = axdev->data_buf_base;
		pool_info.size = axdev->data_buf_size;
		pr_debug("Data buffer range: %llx, size %llx\n",
			 axdev->data_buf_base, axdev->data_buf_size);
		if (copy_to_user((int *)arg, &pool_info,
				 sizeof(struct buf_pool_s))) {
			pr_err("Error in getting total buffer size!\n");
			return -EFAULT;
		}
		break;
	case AX_IPC_IOC_NOTIFY:
		if (copy_from_user(&reg_ep_id, (int *)arg, sizeof(int))) {
			pr_err("Endpoint wasn't registered for notification\n");
			return -EFAULT;
		}
		ret = ax_register_endpoint(axdev, reg_ep_id);
		if (ret) {
			pr_err("Failed to register endpoint %x\n", reg_ep_id);
			return ret;
		}
		break;
	default:
		pr_info("Invalid IOCTL command\n");
		return -EINVAL;
	}

	return 0;
}

/*
 * TODO : This will be modified once there is clarity on much memory
 * can be mapped and on how pointers will be shared b/w kernel and user space
 */
static int axdev_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct ax_ipc_device *axdev = NULL;

	if (!file) {
		pr_err(" File NULL pointer\n");
		return -1;
	}
	axdev = file->private_data;
	/** allocate pages */
	unsigned long pfn = axdev->data_buf_base >> PAGE_SHIFT;

	return remap_pfn_range(vma, vma->vm_start, pfn,
			       vma->vm_end - vma->vm_start, vma->vm_page_prot);
}

/* Character device interface */
static const struct file_operations axdev_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = axdev_ioctl,
	.mmap = axdev_mmap,
	.open = axdev_open,
	.release = axdev_release,
};

static int ax_ipc_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	const char *label;
	int ret;
	int ipc_irq;
	struct ax_ipc_device *axdev;
	struct resource *dts_shared_mem_res, *dts_tx_intr_resource,
		*dts_rx_intr_resource, *dts_data_ptr_resource;
	resource_size_t rx_queue_start;
	resource_size_t tx_queue_start;
	u64 shared_mem_base, tx_intr_base, rx_intr_base;
	u64 shared_mem_size, tx_intr_reg_size, rx_intr_reg_size;
	u32 __iomem *rx_queue_s, *tx_queue_s;
	unsigned long minor;
	struct device *dev;
	void __iomem *boot_status_reg;

	dev_info(&pdev->dev, "AXIADO IPC Enter Probe function!!");

	/* Allocate memory for the device structure */
	axdev = devm_kzalloc(&pdev->dev, sizeof(*axdev), GFP_KERNEL);
	if (!axdev)
		return -ENOMEM;

	ret = of_property_read_string(node, "LABEL", &label);
	if (ret < 0) {
		dev_err(&pdev->dev, "Cannot read remote device label\n");
		return ret;
	}

	/* Save the device label as Remote ID */
	axdev->ax_remote_id = label;
	axdev->each_buf_size = DATA_BUFFER_SIZE;

	/* Get the shared memory resource from the device tree */
	dts_shared_mem_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	shared_mem_base = dts_shared_mem_res->start;
	shared_mem_size = resource_size(dts_shared_mem_res);

	/* Assigning static queues at well spaced offsets.
	 * M0 is the outlier in the queue defines where RX and TX
	 * queues are swapped
	 */
	if (strncmp(axdev->ax_remote_id, "M0", 2) == 0) {
		rx_queue_start = shared_mem_base;
		tx_queue_start = (shared_mem_base + QUEUE_MEM_SIZE);
	} else {
		rx_queue_start = (shared_mem_base + QUEUE_MEM_SIZE);
		tx_queue_start = shared_mem_base;
	}

	/* Map the RX queue to the kernel */
	rx_queue_s = ioremap(rx_queue_start, QUEUE_MEM_SIZE);
	axdev->rx_queue = (struct ax_queue_s *)rx_queue_s;

	/* Map the TX queue to the kernel */
	tx_queue_s = ioremap(tx_queue_start, QUEUE_MEM_SIZE);
	axdev->tx_queue = (struct ax_queue_s *)tx_queue_s;

	/* Map the TX interrupt resource from the device tree */
	dts_tx_intr_resource = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	tx_intr_base = dts_tx_intr_resource->start;
	tx_intr_reg_size = resource_size(dts_tx_intr_resource);

	/* Map the RX interrupt resource from the device tree */
	dts_rx_intr_resource = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	rx_intr_base = dts_rx_intr_resource->start;
	rx_intr_reg_size = resource_size(dts_rx_intr_resource);

	dts_data_ptr_resource = platform_get_resource(pdev, IORESOURCE_MEM, 3);

	/* Get the physical address of the shared memory between local CPU and remote CPU.
	 * If the IPC packet delivers data with address instead of actual data,
	 * use the physical address as the parameter.
	 */
	axdev->shared_mem_phy_addr = dts_data_ptr_resource->start;
	dev_dbg(&pdev->dev, "axdev->shared_mem_phy_addr: %x\n",
		axdev->shared_mem_phy_addr);

	if (!dts_data_ptr_resource) {
		dev_err(&pdev->dev, "Failed to get memory resource\n");
		return -ENODEV;
	}

	/* First page for kernel space */
	axdev->data_buf_base =
		dts_data_ptr_resource->start + AX_IPC_KERN_BUFF_LEN;
	axdev->data_buf_size =
		resource_size(dts_data_ptr_resource) - AX_IPC_KERN_BUFF_LEN;

	/* Map the first page */
	axdev->kern_ipc_buf = devm_ioremap(
		&pdev->dev, dts_data_ptr_resource->start, AX_IPC_KERN_BUFF_LEN);
	if (!axdev->kern_ipc_buf) {
		dev_err(&pdev->dev, "Failed to ioremap memory region\n");
		return -ENOMEM;
	}

	dev_dbg(&pdev->dev, "The remote CPU ID is: %s\n", axdev->ax_remote_id);
	dev_dbg(&pdev->dev, "Shared Mem Range: %llx - %llx\n", shared_mem_base,
		shared_mem_base + shared_mem_size - 1);
	dev_dbg(&pdev->dev, "TX Interrupt Reg Range: %llx - %llx\n",
		tx_intr_base, tx_intr_base + tx_intr_reg_size - 1);
	dev_dbg(&pdev->dev, "RX Interrupt Reg Range: %llx - %llx\n",
		rx_intr_base, rx_intr_base + rx_intr_reg_size - 1);
	pr_debug("Data buffer range: %llx, size %llx\n", axdev->data_buf_base,
		 axdev->data_buf_size);
	dev_dbg(&pdev->dev, "rx_queue_start: %llx\n", rx_queue_start);
	dev_dbg(&pdev->dev, "tx_queue_start: %llx\n", tx_queue_start);

	ret = alloc_chrdev_region(&axdev->ax_dev, 0, 1, "ax");
	if (ret < 0)
		return ret;

	cdev_init(&axdev->ax_cdev, &axdev_fops);

	/* Check if the axdev_class already exists, if not then create it.
	 * If it does exist, then just use it
	 */
	if (!axdev->axdev_class) {
		axdev->axdev_class = class_create(axdev->ax_remote_id);
		if (IS_ERR(axdev->axdev_class)) {
			dev_err(&pdev->dev, "Failed to create axdev class\n");
			return PTR_ERR(axdev->axdev_class);
		}
	}

	INIT_LIST_HEAD(&axdev->device_entry);

	minor = find_first_zero_bit(minors, AXDEV_MAX_DEVICES);
	if (minor < AXDEV_MAX_DEVICES) {
		axdev->ax_dev = MKDEV(AXDEV_MAJOR, minor);
		dev = device_create(axdev->axdev_class, &pdev->dev,
				    axdev->ax_dev, axdev, "axdev%s",
				    axdev->ax_remote_id);
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

	dev_info(&pdev->dev, "axdev%s created\n", axdev->ax_remote_id);

	/* Store the device structure in the platform data */
	platform_set_drvdata(pdev, axdev);

	axdev->ax_base = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	WARN_ON(!axdev->ax_base);

	/* Map the IPC TX register */
	axdev->ipc_tx_reg = devm_ioremap_resource(
		&pdev->dev, platform_get_resource(pdev, IORESOURCE_MEM, 1));
	if (IS_ERR(axdev->ipc_tx_reg))
		return PTR_ERR(axdev->ipc_tx_reg);

	/* Map the IPC RX register */
	axdev->ipc_rx_reg = devm_ioremap_resource(
		&pdev->dev, platform_get_resource(pdev, IORESOURCE_MEM, 2));
	if (IS_ERR(axdev->ipc_rx_reg))
		return PTR_ERR(axdev->ipc_rx_reg);

	ipc_irq = irq_of_parse_and_map(node, 0);
	if (ipc_irq <= 0) {
		dev_err(&pdev->dev, "failed to parse IPC interrupt\n");
		return -EINVAL;
	}

	ret = request_irq(ipc_irq, ax_ipc_handler,
			  IRQF_TRIGGER_HIGH | IRQF_NO_SUSPEND, "ax-ipc", axdev);
	if (ret) {
		dev_err(&pdev->dev, "failed to request IPC interrupt\n");
		return ret;
	}

	/* Initialize the RX and TX queues */
	axdev->tx_queue->read_ptr = 0;
	axdev->tx_queue->write_ptr = 0;

#if AX_IPC_TEST_UT
	dev_info(&pdev->dev,
		 "Running UT where A53 sends data to Remote Processor!!");

	/* Run the UT function to send data to remote CPU */
	ret = txfr_to_remote_cpu_ut(axdev);
	if (ret < 0) {
		dev_err(&pdev->dev,
			"Failed to send data to remote CPU %s with error %d\n",
			axdev->ax_remote_id, ret);
		return ret;
	}
	dev_info(&pdev->dev, "Successfully sent data to remote CPU %s\n",
		 axdev->ax_remote_id);
#endif

	dev_info(&pdev->dev, "AXIADO IPC Probe Success!!");

	spin_lock_init(&axdev->lock);

	if (strncmp(axdev->ax_remote_id, "M0", 2) == 0) {
		/* Probe is successful for M0 IPC so send boot-up done from here */
		boot_status_reg = devm_ioremap_resource(
			&pdev->dev,
			platform_get_resource(pdev, IORESOURCE_MEM, 4));
		if (IS_ERR(boot_status_reg))
			return PTR_ERR(boot_status_reg);
		/* Write a value for APPS(Application subsystem) boot_up done */
		writel(APPS_BOOT_DONE, boot_status_reg);
	}
	return 0;
}

static int ax_ipc_remove(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	int ipc_irq;
	struct ax_ipc_device *axdev = platform_get_drvdata(pdev);

	ipc_irq = irq_of_parse_and_map(node, 0);
	if (ipc_irq <= 0) {
		dev_err(&pdev->dev, "failed to parse IPC interrupt\n");
		return -EINVAL;
	}

	/* Clean up ops */
	free_irq(ipc_irq, axdev);
	list_del(&axdev->device_entry);
	cdev_del(&axdev->ax_cdev);
	device_destroy(axdev->axdev_class, axdev->ax_dev);
	clear_bit(MINOR(axdev->ax_dev), minors);
	class_destroy(axdev->axdev_class);

	return 0;
}

static const struct of_device_id ax_ipc_of_match[] = {
	{
		.compatible = "axiado,ax-ipc",
	},
	{},
};

MODULE_DEVICE_TABLE(of, ax_ipc_of_match);

static struct platform_driver ax_ipc_driver = {
	.driver = {
		.name = "ax-ipc",
		.of_match_table = ax_ipc_of_match,
	},
	.probe = ax_ipc_probe,
	.remove = ax_ipc_remove,
};


module_platform_driver(ax_ipc_driver);

MODULE_AUTHOR("Rishul Naik <rishul.naik@axiado.com>");
MODULE_DESCRIPTION("Axiado Inter-Processor Communication driver");
MODULE_LICENSE("GPL");
