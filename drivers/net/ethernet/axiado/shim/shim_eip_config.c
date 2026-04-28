// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * shim_eip_config - Configuration of command token words for ingress packets,
 * Handles the setup of Inline Command Descriptors (CD) and Transform Records (TR)
 * for the EIP engine.
 *
 * Copyright (c) 2022-2026 Axiado Corporation
 */

#include <linux/device.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/types.h>

#include "mac_config.h"
#include "shim_eip_config.h"
#include "shim_mac.h"
#include "shim_platform.h"

/**
 * set_cd_app_id - Set App_Id in [15:9] bits of the command descriptor word.
 * @cd_word: Pointer to the Command Descriptor word to be modified.
 * @app_id:  App Id to be set in CD word.
 *
 * This does not modify any other bits in cd_word.
 */
static void set_cd_app_id(u32 *cd_word, u32 app_id)
{
	u32 val = *cd_word;

	/* Clear App_id bits (9 to 15) */
	val &= 0xffff01ff;
	/* Set new App_id */
	val |= ((app_id << 9) & 0x0000fe00);

	*cd_word = val;
}

void shim_write_word_cd(u32 base_offset, u8 word, u32 val)
{
	u8 word_offset;

#ifndef CONFIG_ARCH_AX3005
	word_offset = word * 4;
#else
	shim_write_word(base_offset, word);
	word_offset = 4;
#endif
	shim_write_word(base_offset + word_offset, val);

	return;
}

/**
 * mac_cd_config - Configure Command Descriptor for a MAC (Ingress).
 * @dev: Device structure for logging.
 * @mac_idx: Index of the MAC (0 for 10G, 1-4 for 1G).
 *
 * Return: SHIM_STATUS_SUCCESS.
 */
static enum AX_SHIM_STATUS mac_cd_config(struct device *dev, int mac_idx)
{
	u32 offset = CMD_TOKEN_WORD_0_OFFSET;
	u32 cd_tkn_base;
	u32 token_val;
	u32 appid;

	/* Calculate base address for this MAC's command descriptor token */
	cd_tkn_base = SHIM_CD_TOKEN_CONFIG_BASE_OFFSET +
		      (mac_idx * SHIM_CD_TOKEN_CONFIG_BYTE_CNT);

	dev_dbg(dev, "Configuring MAC-%d Cmd Desc at base: 0x%x\n", mac_idx,
		cd_tkn_base);

	/* WORD-0: IP_pkt_length[15:0]= 0; AP[16] = 0 (ARC4 pre-fetch ctrl);
	 * options[29:17] = 0x1800; type[31:30] = 0x3(Extended cmd token)
	 */
	shim_write_word_cd(cd_tkn_base + offset, 0, CMD_TOKEN_WORD_0_VAL);

	/* WORD-1: AAD_length[7:0] = 0; App_id[15:9] = mac_idx or 10G ID */
	token_val = CMD_TOKEN_WORD_1_VAL;
	/* If mac_idx is 0 (10G), use MAC_10G_APPID (5), else use mac_idx */
	appid = (!mac_idx) ? MAC_10G_APPID : mac_idx;
	set_cd_app_id(&token_val, appid);
	shim_write_word_cd(cd_tkn_base + offset, 1, token_val);

	/* WORD-2: For DTL or Flow lookup : 0xFFFFFFFC */
	shim_write_word_cd(cd_tkn_base + offset, 2, CMD_TOKEN_WORD_2_VAL);
	/* WORD-3: For DTL or Flow lookup : 0xFFFFFFFF*/
	shim_write_word_cd(cd_tkn_base + offset, 3, CMD_TOKEN_WORD_3_VAL);

	/* WORD-4: user_def[15:0] = 0; strip_padding[22] = 0;
	 * Allow_padding[23] = 1; HW_service[29:24] = 0x2(LIP/IIP);
	 * flow_lookup[31] = 0;
	 */
	shim_write_word_cd(cd_tkn_base + offset, 4, CMD_TOKEN_WORD_4_VAL);

#if VLAN_SUPPORT_ENABLED
	/* Refer Security-IP-197_FW3.3_Firmware-Reference-Manual_RevE
	 * Page no.17, 26
	 *
	 * WORD-5:  offset[15:8] = 0x0; next_header[23:16] = 0; FL[24] = 0;
	 * Ipv4ChkSum[25] = 0; L4ChkSum[26] = 0; parseEther[27] = 1;
	 * KeepOuter[28] = 0
	 */
	shim_write_word_cd(cd_tkn_base + offset, 5, 0x08000000);
#else
	/* WORD-5:  offset[15:8] = 0xE; next_header[23:16] = 0; FL[24] = 0;
	 * Ipv4ChkSum[25] = 0; L4ChkSum[26] = 0; parseEther[27] = 0;
	 * KeepOuter[28] = 0
	 */
	shim_write_word_cd(cd_tkn_base + offset, 5, CMD_TOKEN_WORD_5_VAL);
#endif

	/* WORD-6: Metadata[31:0] = 0 */
	shim_write_word_cd(cd_tkn_base + offset, 6, CMD_TOKEN_WORD_6_VAL);

	/* WORD-7: Metadata[31:0] = 0 */
	shim_write_word_cd(cd_tkn_base + offset, 7, CMD_TOKEN_WORD_7_VAL);

	/* WORD-8: Metadata[31:0] = 0 */
	shim_write_word_cd(cd_tkn_base + offset, 8, CMD_TOKEN_WORD_8_VAL);

	/* WORD-9: Metadata[31:0] = 0 */
	shim_write_word_cd(cd_tkn_base + offset, 9, CMD_TOKEN_WORD_9_VAL);

	/* WORD-10: Metadata[31:0] = 0 */
	shim_write_word_cd(cd_tkn_base + offset, 10, CMD_TOKEN_WORD_10_VAL);

	/* WORD-11: Metadata[31:0] = 0 */
	shim_write_word_cd(cd_tkn_base + offset, 11, CMD_TOKEN_WORD_11_VAL);

	return SHIM_STATUS_SUCCESS;
}

/**
 * shim_inline_cd_config - Configure inline command descriptors for all enabled MACs.
 * @dev: Device structure for logging.
 *
 * Iterates through all MACs and configures the Command Descriptor if the MAC
 * is enabled.
 *
 * Return: SHIM_STATUS_SUCCESS on success, error code on failure.
 */
enum AX_SHIM_STATUS shim_inline_cd_config(struct device *dev)
{
	int mac_idx;
	enum AX_SHIM_STATUS ret;

	dev_dbg(dev, "Configuring Inline Command Descriptors\n");

	for (mac_idx = 0; mac_idx < MAX_MAC_CNT; mac_idx++) {
		/* Check if the mac is enabled */
		if (mac_is_enabled(mac_idx)) {
			ret = mac_cd_config(dev, mac_idx);
			if (ret != SHIM_STATUS_SUCCESS) {
				dev_err(dev, "MAC-%d CD Config Failed: %d\n",
					mac_idx, ret);
				return ret;
			}
		}
	}

	dev_dbg(dev, "Inline Command Descriptors Configured Successfully\n");
	return SHIM_STATUS_SUCCESS;
}

/**
 * tdest_mask_config - Configure TDEST mask for each enabled MAC.
 * @dev: Device structure for logging.
 *
 * Sets the traffic destination masks based on the MAC index.
 *
 * Return: SHIM_STATUS_SUCCESS.
 */
enum AX_SHIM_STATUS tdest_mask_config(struct device *dev)
{
	int mac_idx;
	u32 offset;

	dev_dbg(dev, "Configuring TDEST masks\n");

	/* 10G MAC */
	if (mac_is_enabled(0)) {
		offset = TDEST_MASK_BASE_OFFSET + (0 * TDEST_MASK_BYTE_CNT);
		shim_write_word(offset, TDEST_MASK_VALUE_XG);
	}

	/* 1G MACs */
	for (mac_idx = 1; mac_idx < MAX_MAC_CNT; mac_idx++) {
		if (!mac_is_enabled(mac_idx))
			continue;

		offset = TDEST_MASK_BASE_OFFSET +
			 (mac_idx * TDEST_MASK_BYTE_CNT);

		switch (mac_idx) {
		case 1:
			shim_write_word(offset, TDEST_MASK_VALUE_1G_0);
			break;
		case 2:
			shim_write_word(offset, TDEST_MASK_VALUE_1G_1);
			break;
		case 3:
			shim_write_word(offset, TDEST_MASK_VALUE_1G_2);
			break;
		case 4:
			shim_write_word(offset, TDEST_MASK_VALUE_1G_3);
			break;
		default:
			dev_err(dev, "Invalid MAC Index: %d\n", mac_idx);
			break;
		}
	}

	return SHIM_STATUS_SUCCESS;
}
