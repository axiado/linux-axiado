/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * shim_mac.h - Defines MAC offsets, configuration values, and return status codes
 * for the Axiado SHIM driver.
 * Copyright (c) 2022-2026 Axiado Corporation
 */

#ifndef _SHIM_MAC_H_
#define _SHIM_MAC_H_

#include <linux/device.h>
#include <linux/errno.h>
#include <linux/types.h>

/* Scratchpad register offset used for board detection (Legacy) */
#define REG_SCRATCH_PAD_REG_N2_ADRS_OFFSET 0x008

/* SGMII configuration constants */
#define SGMII_BASE 0x0
#define SGMII_CONFIG_RANGE 0x10000

/* * 1G/SGMII MAC Configuration
 * 0x03f00008: Default cut-through FIFO settings
 */
#define MAC_1G_R_RX_FIFO_VAL 0x03f00008
#define MAC_1G_R_TX_FIFO_VAL 0x03f00008
#define MAC_1G_R_FRM_LENGTH_VAL 9600

/* 0x02 : GMII mode */
#define MAC_1G_R_IF_MODE_VAL 0x02
#define MAC_1G_REVISION_VAL 0x10136
#define MAC_CC_NULL 0x00000000

/* 1G MAC Flow Control Values (0x0: Non-loopback, 0x1: Loopback) */
#define MAC_1G_0_FLOW_VAL 0x00000000
#define MAC_1G_1_FLOW_VAL 0x00000000
#define MAC_1G_2_FLOW_VAL 0x00000000
#define MAC_1G_3_FLOW_VAL 0x00000000

/* * 10G/XGMII MAC Configuration
 * Optimized for cut-through on RXAUI
 */
#define XG_R_MAC_0_VAL 0x00000000
#define XG_R_MAC_1_VAL 0x00000000
#define XG_MAC_R_RX_FIFO_VAL 0x4
#define XG_MAC_R_TX_FIFO_VAL 0x6
#define XG_MAC_R_FRM_LENGTH_VAL 9600
#define XG_MAC_R_IF_MODE_VAL 0x0
#define MAC_XG_REVISION_VAL 0x10136
#define MAC_XG_FLOW_VAL 0x00000000

/* Config for Cmd Descriptor token */

/* Set 1/0 for enabling/disabling for lookup mode.
 * If lookup is disabled, shim will use a static Transform Record.
 */
#ifndef TRANSFORM_LOOKUP_ENABLE
#define TRANSFORM_LOOKUP_ENABLE 1
#endif

/* Destination Interface to be used in static Transform Record.
 * If the value is more than 0, multi-interface support must be
 * enabled in EIP-DDK driver.
 */
#ifndef DEST_IF_ID
#define DEST_IF_ID 0
#endif

/* COMMAND TOKEN WORDS VALUE FOR CMD DESCRIPTOR
 * Refer Firmware Reference Manual of EIP - page_23
 */
#define CMD_TOKEN_WORD_0_VAL 0xF0000000
#ifndef CONFIG_ARCH_AX3005
#define CMD_TOKEN_WORD_1_VAL 0x00200200
#else
#define CMD_TOKEN_WORD_1_VAL 0x00300200
#endif

/* TR Pointer (Low/High) for Lookup Mode */
#define CMD_TOKEN_WORD_2_VAL 0xFFFFFFFC
#define CMD_TOKEN_WORD_3_VAL 0xFFFFFFFF

#define CMD_TOKEN_WORD_4_VAL 0x02800000
#define CMD_TOKEN_WORD_5_VAL 0x00000e00
#define CMD_TOKEN_WORD_6_VAL 0x00000000
#define CMD_TOKEN_WORD_7_VAL 0x00000000
#define CMD_TOKEN_WORD_8_VAL 0x00000000
#define CMD_TOKEN_WORD_9_VAL 0x00000000
#define CMD_TOKEN_WORD_10_VAL 0x00000000
#define CMD_TOKEN_WORD_11_VAL 0x00000000

/* * TDEST Mask Values */
#define TDEST_MASK_VALUE_XG 0xFFFF0A00 /* 10G Port */
#define TDEST_MASK_VALUE_1G_0 0xFFFF0200 /* 1G Port 0 */
#define TDEST_MASK_VALUE_1G_1 0xFFFF0400 /* 1G Port 1 */
#define TDEST_MASK_VALUE_1G_2 0xFFFF0600 /* 1G Port 2 */
#define TDEST_MASK_VALUE_1G_3 0xFFFF0800 /* 1G Port 3 */

/* Return status for shim functions.
 * Mapped to standard Linux error codes to facilitate future cleanup.
 */
enum AX_SHIM_STATUS {
	SHIM_STATUS_SUCCESS = 0,
	SHIM_STATUS_INVALID_ARG = -EINVAL,
	SHIM_STATUS_NULL_POINTER = -EFAULT,
	SHIM_STATUS_INTERNAL_ERROR = -EIO
};

/* Prototypes */
enum AX_SHIM_STATUS shim_mac_init(struct device *dev);
void port_set_hfifo_mode(struct device *dev, u8 mac_idx);

#endif /* _SHIM_MAC_H_ */
