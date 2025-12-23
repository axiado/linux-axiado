/* SPDX-License-Identifier: GPL-2.0-or-later */

/*
 * Copyright (c) 2021-25 Axiado Corporation (or its affiliates).
 */

/*
 * AXIADO IPC Driver
 */

#ifndef __AX_IPC_H
#define __AX_IPC_H

/* Enable/disable UT macro for running an example txfr from Kernel space */
#define AX_IPC_TEST_UT 0

#define AXDEV_MAJOR 143
#define AXDEV_MAX_DEVICES 24

/* IOCTL commands */
#define AX_IPC_IOC_MAGIC 0x66

/* Used by the user space application to send data to the remote CPU */
#define AX_IPC_IOC_SEND _IOW(AX_IPC_IOC_MAGIC, 0, struct ipc_pkt_s)

/* Used by the user space application to receive data from the remote CPU */
#define AX_IPC_IOC_RECV _IOR(AX_IPC_IOC_MAGIC, 1, struct ipc_pkt_s)

/* Used by user space application to register an endpoint ID and a task struct */
#define AX_IPC_IOC_NOTIFY _IOW(AX_IPC_IOC_MAGIC, 2, int)

/* Used by user space application to set the individual data buffer size */
#define AX_IPC_GET_BUF_SIZE _IOR(AX_IPC_IOC_MAGIC, 3, struct buf_pool_s)

/**
 * @brief Defines the size of the IPC queue.
 */
#define QUEUE_SIZE 64

/**
 * @brief Defines the magic number for the M0_M55_A53 IPC communication.
 */
#define M0_M55_A53_MAGIC_NUM 0xFD550A53

/**
 * @brief Defines the size of the shared memory block used by the IPC.
 */
#define QUEUE_MEM_SIZE 0x4000

/**
 * @brief Max number of endpoints
 */
#define MAX_ENDPOINTS 256

/**
 * @brief Get the endpoint id index
 */
#define AX_GET_EP_INDEX(ep) ((ep)&0xFF)

/**
 * @brief Active (1) eMMC partition value
 */
#define AX_ACTIVE_KERNEL_MAGIC (0xacce55ed)

/**
 * @brief Backup (2) eMMC partition value
 */
#define AX_BACKUP_KERNEL_MAGIC (0xb01dface)

/**
 * @brief APPS boot-up done
 * This status will indicate the system manager that the system is booted up fine.
 */
#define APPS_BOOT_DONE (0x49154d)

/**
 * @brief Data buffer size
 */
#define DATA_BUFFER_SIZE (256)

#define AX_NOTIFY_SIGNAL (0x33)

/**
 * @brief Describes the structure and allocation of the IPC buffer.
 *
 * The IPC buffer now includes a kernel space IPC buffer. The first
 * `AX_IPC_KERN_BUFF_LEN` bytes are allocated for kernel IPC, while the
 * remaining bytes are used for user space IPC.
 *
 * The address of the user space IPC buffer is mapped to a memory address
 * that aligns with `PAGE_SHIFT` (i.e., `axdev->data_buf_base >> PAGE_SHIFT`).
 *
 * @note The value of `AX_IPC_KERN_BUFF_LEN` should be a multiple of `PAGE_SHIFT`.
 */
#define AX_IPC_KERN_BUFF_LEN (1 << PAGE_SHIFT)
/**
 * @brief Buffer pool information
 */
struct buf_pool_s {
	u32 phys_base; /**< Physical pool base */
	u32 size; /**< Physical pool size */
};

/**
 * @brief Represents a TLV structure that is packed and aligned to 8 bytes.
 */
struct ipc_pkt_s {
	volatile u32 endpoint_id; /**< The endpoint ID for the TLV */
	volatile u32 event; /**< The event for the TLV */
	volatile u32 length; /**< The length of the TLV data */
	volatile u32 is_pointer : 1; /**< Is data field pointer? */
	volatile u32 rsvd : 31; /**< The rsvd word for alignment */
	volatile u64 data; /**< The data for the TLV */
} __aligned(8);

/**
 * @brief Queue struct for Axiado IPC
 */
struct ax_queue_s {
	volatile u32 magic_num; /**< The magic number for the queue */
	volatile u32 read_ptr; /**< The read pointer for the queue */
	volatile u32 write_ptr; /**< The write pointer for the queue */
	volatile u32 shared_sem; /**< The shared semaphore for the queue */
	volatile struct ipc_pkt_s
		ipc_pkt[QUEUE_SIZE]; /**<The IPC packet pointers for the queue*/
} __aligned(8);

/**
 * @brief Endpoint struct
 */
struct ep_task_info {
	struct task_struct *task; /**< Task struct specific to the endpoint */
};

/**
 * @brief AX IPC device.
 */
struct ax_ipc_device {
	void __iomem *ax_base; /**< The AX base address for the device */
	const char *ax_remote_id; /**< The name of the remote processor */
	void __iomem *ipc_tx_reg; /**< The IPC transmit register for the device */
	void __iomem *ipc_rx_reg; /**< The IPC receive register for the device */
	struct ax_queue_s
		*rx_queue; /**< The rx_queue for given remote device instance */
	struct ax_queue_s
		*tx_queue; /**< The tx_queue for given remote device instance */
	struct class *axdev_class; /**< The class for the device */
	dev_t ax_dev; /**< The device struct instance */
	struct cdev ax_cdev; /**< Character device instance */
	struct list_head device_entry; /**< The list head for the device */
	struct ep_task_info
		ep_task[MAX_ENDPOINTS]; /**< The task info for the endpoint */
	phys_addr_t data_buf_base; /**< Physical address of data buffer */
	phys_addr_t data_buf_size; /**< Size of the data buffer */
	u32 each_buf_size; /**< Size of each data buffer */
	void __iomem *kern_ipc_buf; /**< Kernel space IPC data buffer */
	phys_addr_t shared_mem_phy_addr; /**< physical address of the share memory*/
	spinlock_t lock; /**< The spinlock for the device */
};

/**
 * @brief Defines a trigger interrupt for the device.
 */
#define TRIG_INT 0x1

/**
 * @brief Defines a clear interrupt values for different remote device
 */
#define CLEAR_M0_INT 0x1
#define CLEAR_ML_FWL_INT 0x0

#endif /* __AX_IPC_H */
