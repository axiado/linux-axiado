// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022-2026 Axiado Corporation.
 */

#include <linux/io.h>
#include "log.h"
#include "ioremap_handle.h"
#include "da_pec_internal.h"

/**
 * @brief ioremap_init - initalises the ioremap handle
 *
 * @param handle - pointer to ioremap handle
 * @param base_address - physical address that has to io remap
 * @param byte_count - size of the address
 * @return int - zero on success and negative on failure
 */
int ioremap_init(struct ioremap_handle *handle, u32 base_address,
		 u32 byte_count)
{
	handle->mapped_base_addr = ioremap(base_address, byte_count);
	if (!handle->mapped_base_addr) {
		LOG_CRIT("Failed to map physical address 0x%X. Mapped addr = NULL\n",
			 base_address);
		goto err_exit;
	}

	handle->phy_base_addr = base_address;
	handle->byte_count = byte_count;

	LOG_INFO("Successfully mapped physical address 0x%X\n", base_address);
	return 0;

err_exit:
	return -1;
}

/**
 * @brief ioremap_read8 - - reads 8bits(1byte) from the remapped memory
 *
 * @param handle - ioremap_handle
 * @param offset - offset address
 * @param value - pointer to store the value read from remapped memory
 * @return int - 0 on success and negative on failure
 */
int ioremap_read8(struct ioremap_handle *handle, u32 offset, u8 *value)
{
	if (offset < handle->byte_count) {
		*value = readb(handle->mapped_base_addr + offset);
		LOG_DEBG("reading  0x%llX =>  0X%02X\n",
			 (unsigned long long)handle->phy_base_addr + offset,
			 *value);
	} else {
		goto err_exit;
	}

	return 0;

err_exit:
	LOG_CRIT("Failed to read from base_address 0x%llX,  Invalid offset : 0x%08X",
		 (unsigned long long)handle->phy_base_addr, offset);
	return -EINVAL;
}

/**
 * @brief ioremap_write8 - writes 8bits(1byte) to remapped memory
 * @param handle - ioremap_handle
 * @param offset - offset address
 * @param value - value that has to written to remapped memory
 * @return int - 0 on success and negative on failure
 */
int ioremap_write8(struct ioremap_handle *handle, u32 offset, const u8 value)
{
	if (offset < handle->byte_count) {
		*((u8 *)((u8 *)handle->mapped_base_addr + offset)) = value;
		LOG_DEBG("writing to  0x%llX <-0x%X",
			 (unsigned long long)handle->phy_base_addr + offset,
			 value);
	} else {
		goto err_exit;
	}

	return 0;

err_exit:
	LOG_CRIT("Failed to write to base_address 0x%llX. Invalid offset : 0x%08X",
		 (unsigned long long)handle->phy_base_addr, offset);
	return -EINVAL;
}

/**
 * @brief ioremap_write32 - writes 32bits(4 bytes or 1 word) to remapped memory
 *
 * @param handle - ioremap_handle
 * @param offset -  offset address
 * @param value - value that has to written to remapped memory
 * @return int -  0 on success and negative on failure
 */
int ioremap_write32(struct ioremap_handle *handle, u32 offset, u32 value)
{
	if (offset < handle->byte_count) {
		*((u32 *)((u8 *)handle->mapped_base_addr + offset)) = value;
		LOG_DEBG("writing to  0x%llX <-0x%X",
			 (unsigned long long)handle->phy_base_addr + offset,
			 value);
	} else {
		goto err_exit;
	}

	return 0;

err_exit:
	LOG_CRIT("Failed to write to base_address 0x%llX. Invalid offset : 0x%08X",
		 (unsigned long long)handle->phy_base_addr, offset);
	return -EINVAL;
}
