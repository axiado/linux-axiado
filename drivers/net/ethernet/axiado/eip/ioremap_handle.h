/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (C) 2022-2026 Axiado Corporation. */

#ifndef IOREMAP_H_
#define IOREMAP_H_
#include <linux/compiler.h>

struct ioremap_handle {
	void __iomem *mapped_base_addr;
	phys_addr_t phy_base_addr;
	resource_size_t byte_count;
};

int ioremap_init(struct ioremap_handle *handle, u32 base_address,
		 u32 byte_count);

int ioremap_read8(struct ioremap_handle *handle, u32 offset, u8 *value);

int ioremap_write8(struct ioremap_handle *handle, u32 offset, const u8 value);

int ioremap_write32(struct ioremap_handle *handle, u32 offset, u32 value);

#endif
