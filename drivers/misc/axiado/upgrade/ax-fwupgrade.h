/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * AXIADO Firmware Upgrade Driver
 *
 * Copyright (c) 2025 Axiado Corporation
 */

#ifndef __AX_FWUPGRADE_H
#define __AX_FWUPGRADE_H

/**
 * @brief Major number
 *
 * This value has been referenced from Firmware upgrade design document.
 */
#define AXDEV_MAJOR		(144)

/**
 * @brief Maximum number of devices this driver supports
 *
 * For now keeping it as per the maximum number of CPUs.
 *
 */
#define AXDEV_MAX_DEVICES	(10)

/**
 * @brief AX Upgrade device.
 */
struct ax_upgrade_device {
	struct cdev ax_cdev;			/**< Character device instance */
	struct class *axdev_class;		/**< The class for the device */
	struct list_head device_entry;		/**< The list head for the device */
	phys_addr_t data_buf_base;		/**< Physical address of data buffer */
	phys_addr_t data_buf_size;		/**< Size of the data buffer */
	dev_t ax_dev;				/**< The device struct instance */
};
#endif /* #ifndef __AX_FWUPGRADE_H */
