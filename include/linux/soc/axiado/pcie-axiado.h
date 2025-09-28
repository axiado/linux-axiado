/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2023-2025 Axiado Corporation. All rights reserved.
 */

#ifndef PCIE_AXIADO_H
#define PCIE_AXIADO_H

struct axiado_pcie;
struct platform_device;

#define MAX_DMA_LIST (16)
#define MAX_PF (1)
#define MAX_VF (64)
#define MAX_PF_MBX (4)
#define MAX_MBX (64)
#define MAX_PORT 2

/* PCIE port number */
enum axiado_pcie_port_num { PCIE_X1, PCIE_X2 };

/* PCIE BAR numbers */
enum axiado_pcie_bars { BAR_01 = 0, BAR_23 = 2, BAR_45 = 4 };

struct axiado_pcie_func_info {
	enum axiado_pcie_port_num port;
	u8 pf_num;
	u8 vf_num;
};

/* PCIE DMA Channel numbers */
enum axiado_pcie_dma_channel {
	DMA_CHANNEL_0,
	DMA_CHANNEL_1,
	DMA_CHANNEL_2,
	DMA_CHANNEL_3,
	DMA_CHANNEL_4,
	DMA_CHANNEL_5,
	DMA_CHANNEL_6,
	DMA_CHANNEL_7
};

/* PCIE DMA modes */
enum axiado_pcie_dma_mode {
	DIRECT_MODE,
	SCATTER_GATHER_00,
	SCATTER_GATHER_01,
	SCATTER_GATHER_10,
	SCATTER_GATHER_11,
};

enum ax_pcie_dma_dir {
	AX_EP_MEMORY_HOST_TO_PDEV, /** On EP, host memory read, host -> phy.func. */
	AX_EP_MEMORY_PDEV_TO_HOST, /** On EP, host memory write, phy.func. -> host */
	AX_RC_VDM_HOST_TO_PDEV, /** On RC, VDM transmit, host -> phy.func. */
	AX_EP_VDM_PDEV_TO_HOST, /** On EP, VDM transmit, phy.func. -> host */
};

/* PCIE DMA request structure */
struct axiado_pcie_dma_request {
	enum axiado_pcie_dma_channel chan_num;
	enum axiado_pcie_dma_mode dma_mode;
	enum ax_pcie_dma_dir dma_dir;
	u8 source_list_size;
	u8 dest_list_size;
	u32 dma_size;
	u64 dma_source_ptr[MAX_DMA_LIST];
	u64 dma_dest_ptr[MAX_DMA_LIST];
	u64 dma_source_size[MAX_DMA_LIST];
	u64 dma_dest_size[MAX_DMA_LIST];
};

struct dma_cfg {
	u32 src_inf;
	u32 dest_inf;
	u32 src_addr_31_0;
	u32 src_addr_63_32;
	u32 dest_addr_31_0;
	u32 dest_addr_63_32;
	u32 length;
	u32 se_cond; // start and end conditions
	u32 irq; // interrupt enable
};

u64 axiado_pcie_get_bar(struct platform_device *pdev,
			struct axiado_pcie_func_info func_info,
			enum axiado_pcie_bars bar_num, u32 *bar_size);
int axiado_pcie_is_dma_busy(struct platform_device *pdev,
			    enum axiado_pcie_port_num port,
			    enum axiado_pcie_dma_channel chan_num);
int axiado_pcie_dma_xfer(struct platform_device *pdev,
			 enum axiado_pcie_port_num port,
			 struct axiado_pcie_dma_request *dma_request);
int axiado_pcie_trigger_msi(struct platform_device *pdev,
			    struct axiado_pcie_func_info func_info,
			    u8 vector_no);
unsigned int axiado_pcie_msgbox_read(struct platform_device *pdev, u32 offset);
unsigned int axiado_pcie_msgbox_write(struct platform_device *pdev, u32 offset,
				      u32 val);
#endif /** PCIE_AXIADO_H */
