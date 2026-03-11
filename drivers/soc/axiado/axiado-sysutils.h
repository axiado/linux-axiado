/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * SYSUTILS driver for Axiado
 *
 * Copyright (C) 2026 Axiado Corporation (or its affiliates). All rights reserved.
 */

#ifndef __SYSUTILS_H__
#define __SYSUTILS_H__

#include <linux/types.h> /* u32 */
#include <linux/device.h> /* struct device */
#include <linux/cdev.h> /* struct cdev */
#include <linux/io.h> /* __iomem */
#include <linux/stddef.h> /* bool */

/* LOGGER DEFINITIONS */
#define AX_LOG_MAGIC_NUMBER 0xD15C0
#define AX_LOG_READ_PTR 0x04
#define AX_LOG_WRITE_PTR 0x08
#define AX_LOG_BUFF_LIMIT 4096
#define AX_LOG_BUFFER_SIZE 63980

struct ax_log_t {
	u32 magic_number;
	u32 read_ptr;
	u32 write_ptr;
	u32 reserved[2];
	char buffer[AX_LOG_BUFFER_SIZE];
} __aligned(64);

/* DEVICE TYPES */
enum sysutils_type {
	SYSUTILS_LOGGER = 0,
	SYSUTILS_FWUPGRADE,
	SYSUTILS_BOARD_ID,
};

/* PER-DEVICE STRUCTURE */
struct sysutils_dev {
	enum sysutils_type type;
	struct device *dev;

	/* ---- Character device ---- */
	struct cdev cdev;
	dev_t devt;
	const char *devnode_name;
	bool cdev_added;
	bool devnode_created;
	int minor;

	/* ---- Common mapped base ---- */
	void __iomem *base;

	/* ---- LOGGER ---- */
	struct ax_log_t __iomem *log;

	/* ---- FW UPGRADE ---- */
	phys_addr_t phys_base;
	size_t size;

	/* ---- BOARD ID ---- */
	u32 board_id;
};

#endif /* __SYSUTILS_H__ */
