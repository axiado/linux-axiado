/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2023-25 Axiado Corporation.
 *
 * Axiado MCTP Driver
 */

#ifndef __LINUX_AXIADO_MCTP_H
#define __LINUX_AXIADO_MCTP_H

#include <linux/types.h>

#define PCIE_VDM_HDR_SIZE	16
#define MCTP_BTU_SIZE		64

/* The MTU of the AXIADO MCTP can be 64/128/256 */
#define AXIADO_MCTP_MTU		MCTP_BTU_SIZE
#define PCIE_VDM_DATA_SIZE_DW	(AXIADO_MCTP_MTU / 4)
#define PCIE_VDM_HDR_SIZE_DW	(PCIE_VDM_HDR_SIZE / 4)
#define MCTP_MIN_PACKET_SIZE	(PCIE_VDM_HDR_SIZE + 4)
#define BUFFER_SIZE		0x1000
#define BUF_COUNT		32
#define RING_BUFF_SIZE		(BUFFER_SIZE * BUF_COUNT)
#define RING_MASK		0x1F000

#define MAX_LOOP_COUNT		20
#define AX_VDM_CTRL_VER1	(1)
/* PCIe message size is 256 bytes */
#define AX_PCIE_VDM_MAXBYTES	256
/* Number of buffers for various DMA */
#define AX_PCIE_VDM_NUMBUFS	4
/* Number of bytes from EXT range start to GBX status */
#define AX_GBX_STATUS_OFFSET	0x40

#define AX_GBX_STATUS_EMPTY	(1 << 0)
#define AX_GBX_STATUS_FULL	(1 << 1)
#define AX_GBX_STATUS_OV	(1 << 2)
#define AX_GBX_STATUS_UN	(1 << 3)
#define AX_GBX_STATUS_FLUSH	(1 << 4)
#define AX_GBX_STATUS_LEVEL	(0x7Ful << 5)

#define PCIE_MCTP_MIN_PACKET_SIZE (PCIE_VDM_HDR_SIZE + 4)

/* Per client packet cache sizes */
#define RX_RING_COUNT	64
#define TX_RING_COUNT	64

/* MCTP header definitions */
#define MCTP_HDR_SRC_EID_OFFSET		14
#define MCTP_HDR_TAG_OFFSET		15
#define MCTP_HDR_SOM			BIT(7)
#define MCTP_HDR_EOM			BIT(6)
#define MCTP_HDR_SOM_EOM		(MCTP_HDR_SOM | MCTP_HDR_EOM)
#define MCTP_HDR_TYPE_OFFSET		16
#define MCTP_HDR_TYPE_CONTROL		0
#define MCTP_HDR_TYPE_VDM_PCI		0x7e
#define MCTP_HDR_TYPE_SPDM		0x5
#define MCTP_HDR_TYPE_BASE_LAST		MCTP_HDR_TYPE_SPDM
#define MCTP_HDR_VENDOR_OFFSET		17
#define MCTP_HDR_VDM_TYPE_OFFSET	19

struct mctp_client;
struct axiado_mctp;

struct pcie_transport_hdr {
	u8 fmt_type;
	u8 mbz;
	u8 mbz_attr_len_hi;
	u8 len_lo;
	u16 requester;
	u8 tag;
	u8 code;
	u16 target;
	u16 vendor;
} __packed;

struct mctp_protocol_hdr {
	u8 ver;
	u8 dest;
	u8 src;
	u8 flags_seq_tag;
} __packed;


struct axiado_mctp_pcie_packet_data {
	u32 hdr[PCIE_VDM_HDR_SIZE_DW];
	u32 payload[PCIE_VDM_DATA_SIZE_DW];
};

struct axiado_mctp_pcie_packet {
	struct axiado_mctp_pcie_packet_data data;
	u32 size;
};
struct axiado_mctp_buffer {
	void *vaddr;
	dma_addr_t dma_handle;
};

struct axiado_mctp_channel {
	struct axiado_mctp_buffer data;
	struct axiado_mctp_buffer cmd;
	struct tasklet_struct tasklet;
	u32 buffer_count;
	u32 rd_ptr;
	u32 wr_ptr;
	bool stopped;
};

struct axiado_pcie_vdm_info {
	/** Coherent DMA handles of allocated dma buffers */
	dma_addr_t txdh[AX_PCIE_VDM_NUMBUFS];
	/** virtual addresses of allocated dma buffers */
	void *txda[AX_PCIE_VDM_NUMBUFS];
	/** number of bytes in each buffer to send */
	size_t txnumbyte[AX_PCIE_VDM_NUMBUFS];
	/** virtual addresses of allocated RX buffers */
	void *rxda[AX_PCIE_VDM_NUMBUFS];
	/** number of bytes received in each RX buffer */
	size_t rxnumbyte[AX_PCIE_VDM_NUMBUFS];
	/** mutex to lock before accessing any of the fields */
	struct mutex mtx;
};

struct axiado_mctp_devinfo {
	struct device *dev;
	struct miscdevice mctp_miscdev;
	const struct axiado_mctp_match_data *match_data;
	struct regmap *map;
	struct reset_control *reset;
	u32 version;
	/*
	 * The reset of the dma block in the MCTP-RC is connected to
	 * another reset pin.
	 */
	struct reset_control *reset_dma;
	struct axiado_mctp_channel tx;
	struct axiado_mctp_channel rx;
	struct list_head clients;
	struct mctp_client *default_client;
	struct list_head mctp_type_handlers;
	/*
	 * clients_lock protects list of clients, list of type handlers
	 * and default client
	 */
	spinlock_t clients_lock;
	struct list_head endpoints;
	size_t endpoints_count;
	wait_queue_head_t read_queue;
	bool data_available;
	/*
	 * endpoints_lock protects list of endpoints
	 */
	struct mutex endpoints_lock;
	u8 eid;
	struct platform_device *peci_mctp;
	struct platform_device *axiado_pcie_pdev;
	/* Rx pointer ring size */
	u32 rx_ring_count;
	/* Tx pointer ring size */
	u32 tx_ring_count;

	/** Gbx thread */
	struct task_struct *gbx_thread;
	struct task_struct *ocm_thread;
	struct task_struct *ocm_buff_thread;

	/* VDM specific */
	struct axiado_pcie_vdm_info vdm_info;
	/* We need to support both EP and RC mode for VDM.
	 * EP mode support is restricted only for testing, but still required till we
	 * can test interoperability
	 */
	enum axiado_pcie_port_num vdm_port; /* the PCIe controller to use for VDM generation */
	bool is_rc; /* VDM port can be EP or RC */

	struct resource *ocm_res;
	void __iomem *pcie_reg_virt;
	void __iomem *ocm_base_virt;
	void __iomem *ocm_ring_head;
	void __iomem *ocm_ring_tail;
};

struct mctp_client {
	struct kref ref;
	struct axiado_mctp_devinfo *priv;
	struct ptr_ring tx_queue;
	struct ptr_ring rx_queue;
	struct list_head link;
	wait_queue_head_t wait_queue;
};

struct mctp_type_handler {
	u8 mctp_type;
	u16 pci_vendor_id;
	u16 vdm_type;
	u16 vdm_mask;
	struct mctp_client *client;
	struct list_head link;
};


enum mctp_address_type {
	AX_MCTP_GENERIC_ADDR_FORMAT = 0,
	AX_MCTP_EXTENDED_ADDR_FORMAT = 1
};

int mbox_thread_start(void);
int mbox_thread_stop(void);

#endif /* __LINUX_AXIADO_MCTP_H */
