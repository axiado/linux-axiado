/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * shim_eip_congif.h - function and macro definination for shim eip_config driver.
 *
 * Copyright (c) 2022-2026 Axiado Corporation
 */

#ifndef _SHIM_EIP_CONFIG_H_
#define _SHIM_EIP_CONFIG_H_

#include <linux/types.h>
#include "shim_mac.h"

/* Offset for MAC for command token words */
#define SHIM_CD_TOKEN_CONFIG_BASE_OFFSET 0x300
#define SHIM_CD_TOKEN_CONFIG_BYTE_CNT 0x20

/* Offset for token word within a Cmd Token */
#define CMD_TOKEN_WORD_0_OFFSET 0x0

/* offset for each MAC for tdest mask reg. */
#define TDEST_MASK_BASE_OFFSET 0x20
#define TDEST_MASK_BYTE_CNT 0x4

#define VLAN_SUPPORT_ENABLED 1

enum AX_SHIM_STATUS shim_inline_cd_config(struct device *dev);
enum AX_SHIM_STATUS tdest_mask_config(struct device *dev);

#endif /* _SHIM_EIP_CONFIG_H_ */
