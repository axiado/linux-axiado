/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Axiado eSPI Slave Controller register definitions
 *
 * Copyright (c) 2025 Axiado Corporation
 */

#ifndef _AXIADO_ESPI_H_
#define _AXIADO_ESPI_H_

#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/bits.h>
#include <linux/regmap.h>

/* eSPI Core Registers */
#define ESPI_CORE_CONFIG		0x00
#define ESPI_DEVICE_ID			0x04
#define ESPI_GEN_CAP_CONFIG		0x08
#define ESPI_CH0_CAP_CONFIG		0x0C
#define ESPI_CH1_CAP_CONFIG		0x10
#define ESPI_CH2_CAP_CONFIG		0x14
#define ESPI_CH3_CAP_CONFIG		0x18
#define ESPI_CH3_CAP_CONFIG_2		0x1C
#define ESPI_CH3_CAP_CONFIG_3		0x20
#define ESPI_CH3_CAP_CONFIG_4		0x24
#define SLAVE_WAIT_STATE_CONFIG		0x28
#define TX_CHN_AVAIL_REQ		0x2C
#define ESPI_QUEUE_STATUS		0x30
#define ERROR_STATUS			0x34
#define FIFO_STATUS			0x38
#define FIFO_FLUSH			0x3C
#define IRQ_ENABLE			0x40
#define IRQ_STATUS			0x44
#define IRQ_ENABLE_ESPI			0x48
#define IRQ_STATUS_ESPI			0x4C
#define IRQ_ENABLE_FIFO			0x50
#define IRQ_STATUS_FIFO			0x54
#define READ_DATA_AVAIL			0x58
#define PC_RX_DATA_FIFO			0x5C
#define NP_RX_DATA_FIFO			0x60
#define PC_TX_DATA_FIFO			0x64
#define NP_TX_DATA_FIFO			0x68
#define FLASH_NP_TX_DATA_FIFO		0x6C
#define FLASH_C_TX_DATA_FIFO		0x70
#define OOB_TX_DATA_FIFO		0x74
#define OOB_RX_DATA_FIFO		0x78
#define FLASH_NP_RX_DATA_FIFO		0x7C
#define FLASH_C_RX_DATA_FIFO		0x80
#define VW_RX_DATA_FIFO			0x84
#define VW_TX_DATA_FIFO			0x88
#define SOC_SLV_TIMEOUT			0x8C
#define RESET_CONFIG			0x90
#define CS_DEASSERT_IO_ALERT_GAP	0x94
#define PUT_VWIRE_COUNT			0x98
#define IRQ_ENABLE_FIFO_2		0x9C
#define IRQ_STATUS_FIFO_2		0xA0
#define DMA_TIMEOUT_ABORT_DISABLE	0xA4
#define SOC_MST_TIMEOUT			0xA8
#define DEBUG_SELECT			0xAC
#define DMA_CONFIG_CTRL			0xE0
#define DMA_CONFIG_CTRL_2		0xE4
#define DATA_FIFO			0x20C

/* ESPI_CORE_CONFIG bits */
#define ESPI_CORE_ENABLE		BIT(0)
#define ESPI_ALERT_ENABLE		BIT(1)

/* ESPI_GEN_CAP_CONFIG bits */
#define ESPI_GEN_CRC_CHECK_EN		BIT(31)
#define ESPI_GEN_RESP_MODIFIER_EN	BIT(30)
#define ESPI_GEN_RTC_INTEGRATED		BIT(29)
#define ESPI_GEN_ALERT_MODE		BIT(28)
#define ESPI_GEN_IO_MODE_MASK		GENMASK(27, 26)
#define ESPI_GEN_IO_MODE_SHIFT		26
#define ESPI_GEN_IO_MODE_SINGLE		(0x0 << 26)
#define ESPI_GEN_IO_MODE_DUAL		(0x1 << 26)
#define ESPI_GEN_IO_MODE_QUAD		(0x2 << 26)
#define ESPI_GEN_IO_MODE_SUPPORT_MASK	GENMASK(25, 24)
#define ESPI_GEN_IO_MODE_SUPPORT_SHIFT	24
#define ESPI_GEN_OPEN_DRAIN_ALERT	BIT(23)
#define ESPI_GEN_OPER_FREQ_MASK		GENMASK(22, 20)
#define ESPI_GEN_OPER_FREQ_SHIFT	20
#define ESPI_GEN_OPEN_DRAIN_SUPPORT	BIT(19)
#define ESPI_GEN_MAX_FREQ_MASK		GENMASK(18, 16)
#define ESPI_GEN_MAX_FREQ_SHIFT		16
#define ESPI_GEN_MAX_WAIT_MASK		GENMASK(15, 12)
#define ESPI_GEN_MAX_WAIT_SHIFT		12
#define ESPI_GEN_CHANNEL_SUPPORT_MASK	GENMASK(7, 0)
#define ESPI_GEN_PERI_CH_SUPPORT	BIT(0)
#define ESPI_GEN_VW_CH_SUPPORT		BIT(1)
#define ESPI_GEN_OOB_CH_SUPPORT		BIT(2)
#define ESPI_GEN_FLASH_CH_SUPPORT	BIT(3)

/* ESPI_CH0_CAP_CONFIG bits */
#define ESPI_CH0_MAX_READ_REQ_MASK	GENMASK(14, 12)
#define ESPI_CH0_MAX_READ_REQ_SHIFT	12
#define ESPI_CH0_MAX_PAYLOAD_MASK	GENMASK(10, 8)
#define ESPI_CH0_MAX_PAYLOAD_SHIFT	8
#define ESPI_CH0_MAX_PAYLOAD_SUP_MASK	GENMASK(6, 4)
#define ESPI_CH0_MAX_PAYLOAD_SUP_SHIFT	4
#define ESPI_CH0_BUS_MASTER_EN		BIT(2)
#define ESPI_CH0_READY			BIT(1)
#define ESPI_CH0_ENABLE			BIT(0)

/* FIFO_FLUSH bits */
#define ESPI_FIFO_PC_RX_FLUSH		BIT(0)
#define ESPI_FIFO_NP_RX_FLUSH		BIT(1)
#define ESPI_FIFO_PC_TX_FLUSH		BIT(2)
#define ESPI_FIFO_NP_TX_FLUSH		BIT(3)
#define ESPI_FIFO_OOB_RX_FLUSH		BIT(4)
#define ESPI_FIFO_OOB_TX_FLUSH		BIT(5)
#define ESPI_FIFO_FLASH_NP_RX_FLUSH	BIT(6)
#define ESPI_FIFO_FLASH_C_RX_FLUSH	BIT(7)
#define ESPI_FIFO_FLASH_NP_TX_FLUSH	BIT(8)
#define ESPI_FIFO_FLASH_C_TX_FLUSH	BIT(9)
#define ESPI_FIFO_DATA_FLUSH		BIT(10)
#define ESPI_FIFO_VW_RX_FLUSH		BIT(11)
#define ESPI_FIFO_VW_TX_FLUSH		BIT(12)
#define ESPI_FIFO_ALL_FLUSH		(ESPI_FIFO_PC_RX_FLUSH | \
					 ESPI_FIFO_NP_RX_FLUSH | \
					 ESPI_FIFO_PC_TX_FLUSH | \
					 ESPI_FIFO_NP_TX_FLUSH | \
					 ESPI_FIFO_OOB_RX_FLUSH | \
					 ESPI_FIFO_OOB_TX_FLUSH | \
					 ESPI_FIFO_FLASH_NP_RX_FLUSH | \
					 ESPI_FIFO_FLASH_C_RX_FLUSH | \
					 ESPI_FIFO_FLASH_NP_TX_FLUSH | \
					 ESPI_FIFO_FLASH_C_TX_FLUSH | \
					 ESPI_FIFO_DATA_FLUSH | \
					 ESPI_FIFO_VW_RX_FLUSH | \
					 ESPI_FIFO_VW_TX_FLUSH)

/* IRQ_ENABLE/IRQ_STATUS bits */
#define ESPI_IRQ_ESPI			BIT(0)
#define ESPI_IRQ_FIFO			BIT(1)

/* IRQ_ENABLE_ESPI/IRQ_STATUS_ESPI bits */
#define ESPI_IRQ_PUT_PC_DONE		BIT(0)
#define ESPI_IRQ_PUT_NP_DONE		BIT(1)
#define ESPI_IRQ_PUT_IO_WR_SHORT_DONE	BIT(2)
#define ESPI_IRQ_PUT_IO_RD_SHORT_DONE	BIT(3)
#define ESPI_IRQ_PUT_MEM_WR_SHORT_DONE	BIT(4)
#define ESPI_IRQ_PUT_MEM_RD_SHORT_DONE	BIT(5)
#define ESPI_IRQ_GET_PC_DONE		BIT(6)
#define ESPI_IRQ_GET_NP_DONE		BIT(7)
#define ESPI_IRQ_PUT_VW_DONE		BIT(8)
#define ESPI_IRQ_GET_VW_DONE		BIT(9)
#define ESPI_IRQ_PUT_OOB_DONE		BIT(10)
#define ESPI_IRQ_GET_OOB_DONE		BIT(11)
#define ESPI_IRQ_PUT_FLASH_NP_DONE	BIT(12)
#define ESPI_IRQ_PUT_FLASH_C_DONE	BIT(13)
#define ESPI_IRQ_GET_FLASH_NP_DONE	BIT(14)
#define ESPI_IRQ_GET_FLASH_C_DONE	BIT(15)
#define ESPI_IRQ_ERROR_DETECT		BIT(16)
#define ESPI_IRQ_APPEND_DONE		BIT(17)
#define ESPI_IRQ_DEFER_RSP_DONE		BIT(18)
#define ESPI_IRQ_SPLIT_COMP_WITH_DATA	BIT(19)
#define ESPI_IRQ_PERIPHERAL_READ_TYPE	BIT(20)
#define ESPI_IRQ_FLASH_READ_TYPE	BIT(21)
#define ESPI_IRQ_RESET_SLAVE_DONE	BIT(22)
#define ESPI_IRQ_RESET_MASTER_ASSERT	BIT(23)
#define ESPI_IRQ_RESET_MASTER_DEASSERT	BIT(24)
#define ESPI_IRQ_SET_CONFIG_DONE	BIT(25)
#define ESPI_IRQ_INBAND_RESET_DONE	BIT(26)

/* ERROR_STATUS bits */
#define ESPI_ERR_INVALID_CMD		BIT(0)
#define ESPI_ERR_INVALID_CTYPE		BIT(1)
#define ESPI_ERR_INVALID_CRC		BIT(2)
#define ESPI_ERR_UNEXPEC_CS_DEASSERT	BIT(3)
#define ESPI_ERR_PUT_WOUT_FREE		BIT(4)
#define ESPI_ERR_GET_WOUT_AVAIL		BIT(5)
#define ESPI_ERR_MAL_PERI_CHANNEL	BIT(6)
#define ESPI_ERR_MAL_VW_CHANNEL		BIT(7)
#define ESPI_ERR_MAL_OOB_CHANNEL	BIT(8)
#define ESPI_ERR_MAL_FLASH_CHANNEL	BIT(9)
#define ESPI_ERR_PAGE_ERR		BIT(10)
#define ESPI_ERR_ALL			(ESPI_ERR_INVALID_CMD | \
					 ESPI_ERR_INVALID_CTYPE | \
					 ESPI_ERR_INVALID_CRC | \
					 ESPI_ERR_UNEXPEC_CS_DEASSERT | \
					 ESPI_ERR_PUT_WOUT_FREE | \
					 ESPI_ERR_GET_WOUT_AVAIL | \
					 ESPI_ERR_MAL_PERI_CHANNEL | \
					 ESPI_ERR_MAL_VW_CHANNEL | \
					 ESPI_ERR_MAL_OOB_CHANNEL | \
					 ESPI_ERR_MAL_FLASH_CHANNEL | \
					 ESPI_ERR_PAGE_ERR)

/* DMA_CONFIG_CTRL bits */
#define ESPI_DMA_CH0_RX_DMA_ENABLE	BIT(0)
#define ESPI_DMA_CH3_RX_DMA_ENABLE	BIT(1)

/* DMA_CONFIG_CTRL_2 bits */
#define ESPI_DMA_CH0_RX_DMA_CTRL	BIT(0)
#define ESPI_DMA_CH3_RX_DMA_CTRL	BIT(1)
#define ESPI_DMA_CH0_RX_MODE_DATA_ONLY	BIT(0)
#define ESPI_DMA_CH3_RX_MODE_DATA_ONLY	BIT(1)
#define ESPI_DMA_RX_MODE_DATA_ONLY	(ESPI_DMA_CH0_RX_MODE_DATA_ONLY | \
					 ESPI_DMA_CH3_RX_MODE_DATA_ONLY)

/* FIFO_STATUS bits */
#define ESPI_FIFO_STS_NP_RX_EMPTY	BIT(14)
#define ESPI_FIFO_STS_VW_RX_EMPTY	BIT(24)

/* TX_CHN_AVAIL_REQ bits */
#define ESPI_PC_TX_AVAIL_REQ		BIT(0)
#define ESPI_NP_TX_AVAIL_REQ		BIT(1)
#define ESPI_VW_TX_AVAIL_REQ		BIT(8)

/* eSPI cycle types */
#define PUT_IOWR_SHORT_4		0x47
#define PUT_IOWR_SHORT_2		0x45
#define PUT_IOWR_SHORT_1		0x44
#define PUT_IORD_SHORT_1		0x40

/* Standard I/O addresses */
#define KCS_DATA1			0xCA2
#define KCS_STS1			0xCA3
#define PORT_80				0x80

/* KCS Status register bits */
#define ESPI_KCS_STATUS_CMD_DAT		BIT(3)
#define ESPI_KCS_STATUS_IBF		BIT(1)
#define ESPI_KCS_STATUS_OBF		BIT(0)

/* Channel-specific interrupt groupings */
#define ESPI_IRQ_PERIF_CHANNEL		(ESPI_IRQ_PUT_PC_DONE | \
					 ESPI_IRQ_PUT_NP_DONE | \
					 ESPI_IRQ_PUT_IO_WR_SHORT_DONE | \
					 ESPI_IRQ_PUT_IO_RD_SHORT_DONE | \
					 ESPI_IRQ_PUT_MEM_WR_SHORT_DONE | \
					 ESPI_IRQ_PUT_MEM_RD_SHORT_DONE | \
					 ESPI_IRQ_GET_PC_DONE | \
					 ESPI_IRQ_GET_NP_DONE | \
					 ESPI_IRQ_PERIPHERAL_READ_TYPE)

#define ESPI_IRQ_VW_CHANNEL		(ESPI_IRQ_PUT_VW_DONE | \
					 ESPI_IRQ_GET_VW_DONE)

#define ESPI_IRQ_OOB_CHANNEL		(ESPI_IRQ_PUT_OOB_DONE | \
					 ESPI_IRQ_GET_OOB_DONE)

#define ESPI_IRQ_FLASH_CHANNEL		(ESPI_IRQ_PUT_FLASH_NP_DONE | \
					 ESPI_IRQ_PUT_FLASH_C_DONE | \
					 ESPI_IRQ_GET_FLASH_NP_DONE | \
					 ESPI_IRQ_GET_FLASH_C_DONE | \
					 ESPI_IRQ_FLASH_READ_TYPE)

#define ESPI_IRQ_RESET			(ESPI_IRQ_RESET_SLAVE_DONE | \
					 ESPI_IRQ_RESET_MASTER_ASSERT | \
					 ESPI_IRQ_RESET_MASTER_DEASSERT)

#define ESPI_IRQ_CONFIG			(ESPI_IRQ_SET_CONFIG_DONE | \
					 ESPI_IRQ_INBAND_RESET_DONE)

#define ESPI_IRQ_ALL_ESPI		(ESPI_IRQ_PERIF_CHANNEL | \
					 ESPI_IRQ_VW_CHANNEL | \
					 ESPI_IRQ_OOB_CHANNEL | \
					 ESPI_IRQ_FLASH_CHANNEL | \
					 ESPI_IRQ_RESET | \
					 ESPI_IRQ_CONFIG | \
					 ESPI_IRQ_ERROR_DETECT)

/**
 * struct espi_config - eSPI controller configuration
 * @fifo_flush_timeout_ms: Timeout for FIFO flush operations in milliseconds
 * @poll_delay_min_us: Minimum polling delay in microseconds
 * @poll_delay_max_us: Maximum polling delay in microseconds
 * @max_retries: Maximum number of retry attempts for operations
 */
struct espi_config {
	u32 fifo_flush_timeout_ms;
	u32 poll_delay_min_us;
	u32 poll_delay_max_us;
	u32 max_retries;
};

/* Hardware variant types */
enum espi_hw_type {
	ESPI_HW_TYPE_MMIO,    /* Direct MMIO access */
	ESPI_HW_TYPE_MFD,     /* Parent MFD regmap access */
};

/**
 * struct espi_hw_config - Hardware-specific configuration
 * @ext_atu_base: External ATU base offset
 * @hw_type: Hardware type identifier
 */
struct espi_hw_config {
	u32 ext_atu_base;
	enum espi_hw_type hw_type;
};

/**
 * struct ax_espi - eSPI slave controller instance
 * @dev: Device pointer
 * @regmap: Unified regmap from parent MFD device
 * @ext_irq_data: IRQ chip data for virtual domain (child devices)
 * @lock: Protects register access and critical sections
 * @core_base: Base address for eSPI core registers
 * @irq: Primary eSPI interrupt from MFD
 * @ext_irq: External logic interrupt for child devices
 * @config: Controller configuration parameters
 * @hw_config: Hardware-specific configuration
 */
struct ax_espi {
	struct device *dev;
	struct regmap *regmap;
	struct regmap_irq_chip_data *ext_irq_data;
	u16 core_base;
	int irq;
	int ext_irq;
	struct espi_config config;
	const struct espi_hw_config *hw_config;
};

#endif /* _AXIADO_ESPI_H_ */
