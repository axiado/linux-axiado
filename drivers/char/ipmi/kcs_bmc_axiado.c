// SPDX-License-Identifier: GPL-2.0-only
/*
 * Axiado KCS BMC Driver for IPMI protocol handling without KCS BMC framework
 *
 * Copyright (c) 2025 Axiado Corporation
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/regmap.h>
#include <linux/miscdevice.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <linux/mutex.h>
#include <linux/ipmi_bmc.h>
#include <linux/of_address.h>

#define DRIVER_NAME "axiado-kcs-bmc"

/* IPMI KCS Protocol Phases */
enum kcs_ipmi_phases {
	KCS_PHASE_IDLE,
	KCS_PHASE_WRITE_START,
	KCS_PHASE_WRITE_DATA,
	KCS_PHASE_WRITE_END_CMD,
	KCS_PHASE_WRITE_DONE,
	KCS_PHASE_WAIT_READ,
	KCS_PHASE_READ,
	KCS_PHASE_ABORT_ERROR1,
	KCS_PHASE_ABORT_ERROR2,
	KCS_PHASE_ERROR
};

/* IPMI KCS Error Codes (IPMI Specification v2.0) */
enum kcs_ipmi_errors {
	KCS_NO_ERROR                = 0x00,
	KCS_ABORTED_BY_COMMAND      = 0x01,
	KCS_ILLEGAL_CONTROL_CODE    = 0x02,
	KCS_LENGTH_ERROR            = 0x06,
	KCS_UNSPECIFIED_ERROR       = 0xFF
};

/* KCS Interface States (IPMI Specification) */
enum kcs_states {
	IDLE_STATE  = 0,
	READ_STATE  = 1,
	WRITE_STATE = 2,
	ERROR_STATE = 3,
};

/* KCS Control Commands (IPMI Specification) */
#define KCS_CMD_GET_STATUS_ABORT    0x60
#define KCS_CMD_WRITE_START         0x61
#define KCS_CMD_WRITE_END           0x62
#define KCS_CMD_READ_BYTE           0x68

/* KCS Status Register Bits (IPMI Specification) */
#define KCS_STATUS_STATE(state)     ((state) << 6)
#define KCS_STATUS_STATE_MASK       GENMASK(7, 6)
#define KCS_STATUS_CMD_DAT          BIT(3)
#define KCS_STATUS_SMS_ATN          BIT(2)
#define KCS_STATUS_IBF              BIT(1)
#define KCS_STATUS_OBF              BIT(0)

#define KCS_COUNT                   4
#define KCS_MSG_BUFSIZ              1000
#define KCS_ZERO_DATA               0

#define DEVICE_NAME                 "ipmi-kcs"

/* KCS register definitions (Section B - peripheral registers) */
#define KCS_DATA_IN_REG(n)          (0x000 + ((n) * 0x10))
#define KCS_DATA_OUT_REG(n)         (0x004 + ((n) * 0x10))
#define KCS_COMMAND_REG(n)          (0x008 + ((n) * 0x10))
#define KCS_STATUS_REG(n)           (0x00C + ((n) * 0x10))

/* KCS ATU register calculation macros (Section C - CSR registers) */
#define ATU_KCS_START_LSB(n)        (0x000 + ((n) * 8))
#define ATU_KCS_START_MSB(n)        (0x004 + ((n) * 8))
#define ATU_KCS_END_LSB(n)          (0x088 + ((n) * 8))
#define ATU_KCS_END_MSB(n)          (0x08C + ((n) * 8))

/* ATU Control Registers (Section C - CSR registers) */
#define ATU_ENABLE                  0x150
#define ATU_OFFSET                  0x154
#define ATU_ERROR_STATUS            0x158
#define ATU_INTERNAL_ISR            0x15C
#define ATU_INTERNAL_ISR_MASK       0x160

#define KCS_INTERNAL_IRQ_RX_WRITE   BIT(0)  /* RX write interrupt */
#define KCS_INTERNAL_IRQ_TX_READ    BIT(1)  /* TX read interrupt */
#define KCS_INTERNAL_IRQ_CMD_WRITE  BIT(2)  /* CMD write interrupt */

/* All KCS interrupts enabled (RX + TX + CMD) */
#define KCS_INTERNAL_IRQ_ALL        (KCS_INTERNAL_IRQ_RX_WRITE | \
				     KCS_INTERNAL_IRQ_TX_READ | \
				     KCS_INTERNAL_IRQ_CMD_WRITE)

/* Default KCS interrupt mask (RX write + CMD write) */
#define KCS_INTERNAL_IRQ_INPUT      (KCS_INTERNAL_IRQ_RX_WRITE | \
				     KCS_INTERNAL_IRQ_CMD_WRITE)

#define ATU_ISR                     0x168
#define ATU_ISR_MASK                0x16C

/* Helper macros for ATU enable bits */
#define ATU_ENABLE_KCS_BIT(n)       BIT(n)

/* KCS offset encoding for ATU hardware register */
enum kcs_offset_enc {
	KCS_OFFSET_1_BYTE   = 0,    /* offset=1  -> hw_val=0 */
	KCS_OFFSET_2_BYTE   = 1,    /* offset=2  -> hw_val=1 */
	KCS_OFFSET_4_BYTE   = 2,    /* offset=4  -> hw_val=2 */
	KCS_OFFSET_8_BYTE   = 3,    /* offset=8  -> hw_val=3 */
	KCS_OFFSET_16_BYTE  = 4,    /* offset=16 -> hw_val=4 */
	KCS_OFFSET_INVALID  = 0xFF
};

/**
 * struct axiado_kcs_bmc - Axiado KCS BMC driver instance
 * @dev: Platform device pointer
 * @regmap: Unified regmap from eSPI parent
 * @perif_base: Base address for peripheral registers
 * @atu_base: Base address for ATU registers
 * @index: KCS channel index (0-3)
 * @base_addr: Base address for KCS registers
 * @io_addr: I/O port address from host perspective
 * @status_reg_offset: Offset of status register from host perspective
 * @hw_offset_enc: Hardware encoding for ATU offset register
 * @addr_size: Size of KCS registers
 * @irq: Virtual IRQ number from regmap IRQ chip
 * @isr_bit: ISR bit number for this KCS channel
 * @lock: Spinlock for protocol state protection
 * @phase: Current IPMI KCS protocol phase
 * @error: Current KCS error code
 * @queue: Wait queue for blocking reads
 * @data_in_avail: Flag indicating input data is available
 * @data_in_idx: Current index in input data buffer
 * @data_in: Input data buffer for IPMI requests
 * @data_out_idx: Current index in output data buffer
 * @data_out_len: Length of output data
 * @data_out: Output data buffer for IPMI responses
 * @miscdev: Character device for userspace interface
 * @mutex: Mutex for file operations
 * @kbuffer: Temporary buffer for userspace data transfers
 */
struct axiado_kcs_bmc {
	struct device *dev;
	struct regmap *regmap;
	u16 perif_base;
	u16 atu_base;

	/* Hardware configuration */
	int index;
	u32 base_addr;
	u32 io_addr;
	u32 status_reg_offset;
	enum kcs_offset_enc hw_offset_enc;
	u32 addr_size;
	int irq;
	int isr_bit;

	/* IPMI Protocol State */
	spinlock_t lock;
	enum kcs_ipmi_phases phase;
	enum kcs_ipmi_errors error;

	/* Message Buffers */
	wait_queue_head_t queue;
	bool data_in_avail;
	int data_in_idx;
	u8 *data_in;
	int data_out_idx;
	int data_out_len;
	u8 *data_out;

	/* Character Device */
	struct miscdevice miscdev;
	struct mutex mutex; /* protects register access */
	u8 *kbuffer;
};

/* Global array of KCS instances for API functions */
static struct axiado_kcs_bmc *kcs_instances[KCS_COUNT];

/* Forward declarations */
static enum kcs_offset_enc kcs_offset_to_hw_enc(u32 offset);
static int axiado_kcs_configure_atu(struct axiado_kcs_bmc *kcs_axiado);
static void axiado_kcs_disable_atu(struct axiado_kcs_bmc *kcs_axiado);

/* Register access helpers */
static inline u32 kcs_reg_read(struct axiado_kcs_bmc *kcs, u32 offset)
{
	u32 val;
	int ret = regmap_read(kcs->regmap, offset, &val);

	if (ret) {
		dev_err(kcs->dev, "Failed to read register 0x%x: %d\n",
			offset, ret);
		return 0;
	}
	return val;
}

static inline int kcs_reg_write(struct axiado_kcs_bmc *kcs, u32 offset, u32 val)
{
	int ret = regmap_write(kcs->regmap, offset, val);

	if (ret) {
		dev_err(kcs->dev, "Failed to write register 0x%x: %d\n",
			offset, ret);
	}
	return ret;
}

static inline int kcs_reg_update_bits(struct axiado_kcs_bmc *kcs, u32 offset,
				      u32 mask, u32 val)
{
	int ret = regmap_update_bits(kcs->regmap, offset, mask, val);

	if (ret) {
		dev_err(kcs->dev,
			"Failed to update register 0x%x: %d\n",
			offset, ret);
	}
	return ret;
}

/**
 * kcs_offset_to_hw_enc - Convert DT offset to hardware encoding
 * @offset: Offset value from device tree (1, 2, 4, 8, 16)
 *
 * Return: Hardware encoding value, or KCS_OFFSET_INVALID on error
 */
static enum kcs_offset_enc kcs_offset_to_hw_enc(u32 offset)
{
	switch (offset) {
	case 1:
		return KCS_OFFSET_1_BYTE;
	case 2:
		return KCS_OFFSET_2_BYTE;
	case 4:
		return KCS_OFFSET_4_BYTE;
	case 8:
		return KCS_OFFSET_8_BYTE;
	case 16:
		return KCS_OFFSET_16_BYTE;
	default:
		return KCS_OFFSET_INVALID;
	}
}

/**
 * axiado_kcs_get_resources() - Get eSPI resources (regmap and register bases)
 * @kcs: KCS instance
 * @espi_np: Parent eSPI device node
 * @mfd_dev: Grandparent MFD device
 *
 * Gets the regmap from MFD device and reads the parent eSPI controller's
 * 'reg' property to extract the peripheral and ATU register base addresses.
 *
 * eSPI reg format: <core_base core_size perif_base perif_size atu_base atu_size>
 *
 * Return: 0 on success, negative error on failure
 */
static int axiado_kcs_get_resources(struct axiado_kcs_bmc *kcs,
				    struct device_node *espi_np,
				    struct device *mfd_dev)
{
	u64 addr, size;
	int ret;

	if (!espi_np) {
		dev_err(kcs->dev, "eSPI device node is NULL\n");
		return -EINVAL;
	}

	if (!mfd_dev) {
		dev_err(kcs->dev, "MFD device is NULL\n");
		return -EINVAL;
	}

	/* Get regmap from MFD (grandparent) */
	kcs->regmap = dev_get_regmap(mfd_dev, NULL);
	if (!kcs->regmap) {
		dev_err(kcs->dev, "Failed to get regmap from MFD\n");
		return -ENODEV;
	}

	/* Read peripheral region (index 1) from eSPI reg property */
	ret = of_property_read_reg(espi_np, 1, &addr, &size);
	if (ret) {
		dev_err(kcs->dev, "Failed to read eSPI peripheral region: %d\n", ret);
		return ret;
	}
	kcs->perif_base = addr;

	/* Read ATU region (index 2) from eSPI reg property */
	ret = of_property_read_reg(espi_np, 2, &addr, &size);
	if (ret) {
		dev_err(kcs->dev, "Failed to read eSPI ATU region: %d\n", ret);
		return ret;
	}
	kcs->atu_base = addr;

	dev_dbg(kcs->dev, "eSPI register offsets - Perif: 0x%04llx, ATU: 0x%04llx\n",
		(u64)kcs->perif_base, (u64)kcs->atu_base);

	return 0;
}

static inline void set_state(struct axiado_kcs_bmc *kcs_axiado, u8 state)
{
	u32 status_val = KCS_STATUS_STATE(state);

	kcs_reg_update_bits(kcs_axiado,
			    kcs_axiado->perif_base + KCS_STATUS_REG(kcs_axiado->index),
			    KCS_STATUS_STATE_MASK, status_val);
	dev_dbg(kcs_axiado->dev, "KCS%d: Set state to %d\n", kcs_axiado->index, state);
}

/**
 * write_data_out - Write data to output register
 * @kcs_axiado: KCS driver instance
 * @data: Data byte to write
 */
static inline void write_data_out(struct axiado_kcs_bmc *kcs_axiado, u8 data)
{
	kcs_reg_write(kcs_axiado,
		      kcs_axiado->perif_base + KCS_DATA_OUT_REG(kcs_axiado->index),
		      data);
	dev_dbg(kcs_axiado->dev, "KCS%d: Write DATA_OUT=0x%02x\n", kcs_axiado->index, data);
}

/**
 * read_data_in - Read data from input register
 * @kcs_axiado: KCS driver instance
 *
 * Return: Data byte from input register
 */
static inline u8 read_data_in(struct axiado_kcs_bmc *kcs_axiado)
{
	u32 data;

	data = kcs_reg_read(kcs_axiado,
			    kcs_axiado->perif_base + KCS_DATA_IN_REG(kcs_axiado->index));
	dev_dbg(kcs_axiado->dev, "KCS%d: Read DATA_IN=0x%02x\n", kcs_axiado->index, (u8)data);
	return (u8)data;
}

/**
 * read_command - Read command from command register
 * @kcs_axiado: KCS driver instance
 *
 * Return: Command byte from command register
 */
static inline u8 read_command(struct axiado_kcs_bmc *kcs_axiado)
{
	u32 data;

	data = kcs_reg_read(kcs_axiado,
			    kcs_axiado->perif_base + KCS_COMMAND_REG(kcs_axiado->index));
	dev_dbg(kcs_axiado->dev, "KCS%d: Read COMMAND=0x%02x\n", kcs_axiado->index, (u8)data);
	return (u8)data;
}

/**
 * read_status - Read status register
 * @kcs_axiado: KCS driver instance
 *
 * Return: Current status register value
 */
static inline u8 read_status(struct axiado_kcs_bmc *kcs_axiado)
{
	u32 data;

	data = kcs_reg_read(kcs_axiado,
			    kcs_axiado->perif_base + KCS_STATUS_REG(kcs_axiado->index));
	return (u8)data;
}

/**
 * update_status - Update specific status register bits
 * @kcs_axiado: KCS driver instance
 * @mask: Bit mask for bits to update
 * @val: New values for masked bits
 */
static inline void update_status(struct axiado_kcs_bmc *kcs_axiado,
				 u8 mask, u8 val)
{
	kcs_reg_update_bits(kcs_axiado,
			    kcs_axiado->perif_base + KCS_STATUS_REG(kcs_axiado->index),
			    mask, val);
	dev_dbg(kcs_axiado->dev, "KCS%d: Update STATUS mask=0x%02x val=0x%02x\n",
		kcs_axiado->index, mask, val);
}

/**
 * kcs_bmc_force_abort - Force KCS interface into error state
 * @kcs_axiado: KCS driver instance
 *
 * Forces the KCS interface into an error state and resets all protocol
 * state. This is used when unrecoverable errors occur or when the
 * userspace application requests a forced abort.
 */
static void kcs_bmc_force_abort(struct axiado_kcs_bmc *kcs_axiado)
{
	set_state(kcs_axiado, ERROR_STATE);
	read_data_in(kcs_axiado);  /* Dummy read to clear IBF */
	write_data_out(kcs_axiado, KCS_ZERO_DATA);

	kcs_axiado->phase = KCS_PHASE_ERROR;
	kcs_axiado->data_in_avail = false;
	kcs_axiado->data_in_idx = 0;

	dev_warn(kcs_axiado->dev, "KCS%d: Forced abort, phase=ERROR\n",
		 kcs_axiado->index);
}

/**
 * kcs_bmc_handle_data - Handle data phase of KCS protocol
 * @kcs_axiado: KCS driver instance
 *
 * Handles the data phase of the IPMI KCS protocol. This function is called
 * when the host writes data (not commands) to the interface. It manages
 * the various phases of data transfer including write operations, read
 * operations, and error handling.
 */
static void kcs_bmc_handle_data(struct axiado_kcs_bmc *kcs_axiado)
{
	u8 data;

	dev_dbg(kcs_axiado->dev, "KCS%d: Handle data: phase=%d, data_in_idx=%d\n",
		kcs_axiado->index, kcs_axiado->phase, kcs_axiado->data_in_idx);

	switch (kcs_axiado->phase) {
	case KCS_PHASE_WRITE_START:
		/* Transition from WRITE_START to WRITE_DATA phase */
		dev_dbg(kcs_axiado->dev, "KCS%d: WRITE_START -> WRITE_DATA\n",
			kcs_axiado->index);
		kcs_axiado->phase = KCS_PHASE_WRITE_DATA;
		fallthrough;

	case KCS_PHASE_WRITE_DATA:
		/* Receiving data bytes from host */
		if (kcs_axiado->data_in_idx < KCS_MSG_BUFSIZ) {
			set_state(kcs_axiado, WRITE_STATE);
			write_data_out(kcs_axiado, KCS_ZERO_DATA);
			data = read_data_in(kcs_axiado);
			kcs_axiado->data_in[kcs_axiado->data_in_idx++] = data;
			dev_dbg(kcs_axiado->dev, "KCS%d: WRITE_DATA[%d]=0x%02x\n",
				kcs_axiado->index, kcs_axiado->data_in_idx - 1, data);
		} else {
			dev_warn(kcs_axiado->dev, "Buffer full, forcing abort");
			kcs_bmc_force_abort(kcs_axiado);
			kcs_axiado->error = KCS_LENGTH_ERROR;
		}
		break;

	case KCS_PHASE_WRITE_END_CMD:
		/* Final data byte after WRITE_END command */
		if (kcs_axiado->data_in_idx < KCS_MSG_BUFSIZ) {
			set_state(kcs_axiado, READ_STATE);
			data = read_data_in(kcs_axiado);
			kcs_axiado->data_in[kcs_axiado->data_in_idx++] = data;
			dev_dbg(kcs_axiado->dev, "KCS%d: WRITE_END final data[%d]=0x%02x\n",
				kcs_axiado->index, kcs_axiado->data_in_idx - 1, data);
			kcs_axiado->phase = KCS_PHASE_WRITE_DONE;
			kcs_axiado->data_in_avail = true;
			dev_dbg(kcs_axiado->dev,
				"KCS%d: Message complete, %d bytes received, waking userspace\n",
				kcs_axiado->index, kcs_axiado->data_in_idx);
			wake_up_interruptible(&kcs_axiado->queue);
		} else {
			dev_warn(kcs_axiado->dev, "Buffer full during WRITE_END");
			kcs_bmc_force_abort(kcs_axiado);
			kcs_axiado->error = KCS_LENGTH_ERROR;
		}
		break;

	case KCS_PHASE_READ:
		/* Sending response bytes to host */
		data = read_data_in(kcs_axiado);
		dev_dbg(kcs_axiado->dev,
			"KCS%d: READ phase, host sent=0x%02x, out_idx=%d, out_len=%d\n",
			kcs_axiado->index, data, kcs_axiado->data_out_idx,
			kcs_axiado->data_out_len);

		if (kcs_axiado->data_out_idx == kcs_axiado->data_out_len)
			set_state(kcs_axiado, IDLE_STATE);

		if (data != KCS_CMD_READ_BYTE) {
			dev_warn(kcs_axiado->dev,
				 "Expected READ_BYTE (0x68), got 0x%02x", data);
			set_state(kcs_axiado, ERROR_STATE);
			write_data_out(kcs_axiado, KCS_ZERO_DATA);
			break;
		}

		if (kcs_axiado->data_out_idx == kcs_axiado->data_out_len) {
			dev_dbg(kcs_axiado->dev, "KCS%d: All data sent, ending transmission\n",
				kcs_axiado->index);
			write_data_out(kcs_axiado, KCS_ZERO_DATA);
			kcs_axiado->phase = KCS_PHASE_IDLE;
			break;
		}

		write_data_out(kcs_axiado,
			       kcs_axiado->data_out[kcs_axiado->data_out_idx]);
		dev_dbg(kcs_axiado->dev, "KCS%d: Sent response byte[%d]=0x%02x\n",
			kcs_axiado->index, kcs_axiado->data_out_idx,
			kcs_axiado->data_out[kcs_axiado->data_out_idx]);
		kcs_axiado->data_out_idx++;
		break;

	case KCS_PHASE_ABORT_ERROR1:
		/* First phase of abort sequence - send error code */
		dev_dbg(kcs_axiado->dev, "KCS%d: ABORT_ERROR1, sending error=0x%02x\n",
			kcs_axiado->index, kcs_axiado->error);
		set_state(kcs_axiado, READ_STATE);
		read_data_in(kcs_axiado);
		write_data_out(kcs_axiado, kcs_axiado->error);
		kcs_axiado->phase = KCS_PHASE_ABORT_ERROR2;
		break;

	case KCS_PHASE_ABORT_ERROR2:
		/* Second phase of abort sequence - complete abort */
		dev_dbg(kcs_axiado->dev, "KCS%d: ABORT_ERROR2, finalizing abort\n",
			kcs_axiado->index);
		set_state(kcs_axiado, IDLE_STATE);
		read_data_in(kcs_axiado);
		write_data_out(kcs_axiado, KCS_ZERO_DATA);
		kcs_axiado->phase = KCS_PHASE_IDLE;
		break;

	default:
		dev_warn(kcs_axiado->dev, "Unknown phase %d, forcing abort",
			 kcs_axiado->phase);
		kcs_bmc_force_abort(kcs_axiado);
		break;
	}
}

/**
 * kcs_bmc_handle_cmd - Handle command phase of KCS protocol
 * @kcs_axiado: KCS driver instance
 *
 * Handles the command phase of the IPMI KCS protocol. This function is
 * called when the host writes commands to the interface. It processes
 * WRITE_START, WRITE_END, and GET_STATUS_ABORT commands according to
 * the IPMI specification.
 */
static void kcs_bmc_handle_cmd(struct axiado_kcs_bmc *kcs_axiado)
{
	u8 cmd;

	set_state(kcs_axiado, WRITE_STATE);
	write_data_out(kcs_axiado, KCS_ZERO_DATA);
	cmd = read_command(kcs_axiado);

	dev_dbg(kcs_axiado->dev, "KCS%d: Received command=0x%02x, current phase=%d\n",
		kcs_axiado->index, cmd, kcs_axiado->phase);

	switch (cmd) {
	case KCS_CMD_WRITE_START:
		/* Start of new IPMI message write sequence */
		dev_dbg(kcs_axiado->dev, "KCS%d: WRITE_START command, starting new message\n",
			kcs_axiado->index);
		kcs_axiado->phase = KCS_PHASE_WRITE_START;
		kcs_axiado->error = KCS_NO_ERROR;
		kcs_axiado->data_in_avail = false;
		kcs_axiado->data_in_idx = 0;
		break;

	case KCS_CMD_WRITE_END:
		/* End of IPMI message write sequence */
		dev_dbg(kcs_axiado->dev, "KCS%d: WRITE_END command\n", kcs_axiado->index);
		if (kcs_axiado->phase != KCS_PHASE_WRITE_DATA) {
			dev_warn(kcs_axiado->dev,
				 "WRITE_END in wrong phase %d, aborting",
				 kcs_axiado->phase);
			kcs_bmc_force_abort(kcs_axiado);
			break;
		}
		kcs_axiado->phase = KCS_PHASE_WRITE_END_CMD;
		break;

	case KCS_CMD_GET_STATUS_ABORT:
		/* Get status or abort current operation */
		dev_dbg(kcs_axiado->dev, "KCS%d: GET_STATUS_ABORT command\n",
			kcs_axiado->index);
		if (kcs_axiado->error == KCS_NO_ERROR)
			kcs_axiado->error = KCS_ABORTED_BY_COMMAND;
		kcs_axiado->phase = KCS_PHASE_ABORT_ERROR1;
		kcs_axiado->data_in_avail = false;
		kcs_axiado->data_in_idx = 0;
		break;

	default:
		/* Unknown command received */
		dev_warn(kcs_axiado->dev, "Unknown command 0x%02x, forcing abort",
			 cmd);
		kcs_bmc_force_abort(kcs_axiado);
		kcs_axiado->error = KCS_ILLEGAL_CONTROL_CODE;
		break;
	}
}

/**
 * axiado_kcs_irq_handler - Threaded IRQ handler for KCS events
 * @irq: IRQ number
 * @data: Driver instance pointer
 *
 * Handles KCS interrupts in threaded context. This allows for safe use
 * of potentially sleeping regmap operations. The handler processes KCS
 * events based on the Command/Data bit in the status register and
 * manages the IPMI KCS protocol state machine.
 *
 * Return: IRQ_HANDLED if interrupt was handled, IRQ_NONE otherwise
 */
static irqreturn_t axiado_kcs_irq_handler(int irq, void *data)
{
	struct axiado_kcs_bmc *kcs_axiado = data;
	u8 status;

	/* Use mutex since we're in threaded context and regmap can sleep */
	mutex_lock(&kcs_axiado->mutex);

	/* Process only one event per IRQ - let hardware re-trigger for more */
	status = read_status(kcs_axiado);
	dev_dbg(kcs_axiado->dev, "KCS%d: IRQ: status=0x%02x\n", kcs_axiado->index, status);

	if (status & KCS_STATUS_IBF) {
		/* Handle based on Command/Data bit */
		if (status & KCS_STATUS_CMD_DAT) {
			dev_dbg(kcs_axiado->dev, "KCS%d: Handling COMMAND (C/D=1)\n",
				kcs_axiado->index);
			kcs_bmc_handle_cmd(kcs_axiado);
		} else {
			dev_dbg(kcs_axiado->dev, "KCS%d: Handling DATA (C/D=0)\n",
				kcs_axiado->index);
			kcs_bmc_handle_data(kcs_axiado);
		}

		mutex_unlock(&kcs_axiado->mutex);
		return IRQ_HANDLED;
	}

	mutex_unlock(&kcs_axiado->mutex);
	return IRQ_NONE;
}

/**
 * axiado_kcs_configure_atu - Configure ATU for KCS channel
 * @kcs_axiado: KCS driver instance
 *
 * Configures the Address Translation Unit (ATU) for this KCS channel.
 * The ATU maps host I/O addresses to internal register addresses and
 * handles the address offset encoding for different KCS layouts.
 *
 * ATU Configuration:
 * - Start/End addresses define the host address range
 * - Offset encoding determines register spacing
 * - Enable bit activates the address translation
 *
 * Return: 0 on success, negative error code on failure
 */
static int axiado_kcs_configure_atu(struct axiado_kcs_bmc *kcs_axiado)
{
	u64 start_addr = kcs_axiado->io_addr;
	u64 end_addr = kcs_axiado->io_addr + kcs_axiado->status_reg_offset;
	u32 atu_en_bit = ATU_ENABLE_KCS_BIT(kcs_axiado->index);
	u32 offset_mask;
	u32 offset_value;
	int ret;

	/* Configure ATU start address (64-bit) */
	ret = kcs_reg_write(kcs_axiado,
			    kcs_axiado->atu_base + ATU_KCS_START_LSB(kcs_axiado->index),
			    (u32)(start_addr & 0xFFFFFFFF));
	if (ret)
		return ret;

	ret = kcs_reg_write(kcs_axiado,
			    kcs_axiado->atu_base + ATU_KCS_START_MSB(kcs_axiado->index),
			    (u32)(start_addr >> 32));
	if (ret)
		return ret;

	/* Configure ATU end address (64-bit) */
	ret = kcs_reg_write(kcs_axiado,
			    kcs_axiado->atu_base + ATU_KCS_END_LSB(kcs_axiado->index),
			    (u32)(end_addr & 0xFFFFFFFF));
	if (ret)
		return ret;

	ret = kcs_reg_write(kcs_axiado,
			    kcs_axiado->atu_base + ATU_KCS_END_MSB(kcs_axiado->index),
			    (u32)(end_addr >> 32));
	if (ret)
		return ret;

	/* Configure offset using pre-computed hardware encoding */
	offset_mask = 0xF << (kcs_axiado->index * 4);
	offset_value = kcs_axiado->hw_offset_enc << (kcs_axiado->index * 4);
	ret = kcs_reg_update_bits(kcs_axiado, kcs_axiado->atu_base + ATU_OFFSET,
				  offset_mask, offset_value);
	if (ret)
		return ret;

	/* Enable the ATU */
	ret = kcs_reg_update_bits(kcs_axiado, kcs_axiado->atu_base + ATU_ENABLE,
				  atu_en_bit, atu_en_bit);
	if (ret)
		return ret;

	dev_dbg(kcs_axiado->dev,
		"Configured ATU for KCS%d: range 0x%llx-0x%llx, hw_encoding=%d\n",
		kcs_axiado->index, start_addr, end_addr,
		kcs_axiado->hw_offset_enc);

	return 0;
}

/**
 * axiado_kcs_disable_atu - Disable ATU for KCS channel
 * @kcs_axiado: KCS driver instance
 */
static void axiado_kcs_disable_atu(struct axiado_kcs_bmc *kcs_axiado)
{
	u32 atu_en_bit = ATU_ENABLE_KCS_BIT(kcs_axiado->index);

	kcs_reg_update_bits(kcs_axiado, kcs_axiado->atu_base + ATU_ENABLE,
			    atu_en_bit, 0);
	dev_dbg(kcs_axiado->dev, "Disabled ATU for KCS%d\n", kcs_axiado->index);
}

/**
 * axiado_kcs_enable_interrupts - Enable KCS interrupts
 * @kcs_axiado: KCS driver instance
 *
 * Enables interrupts for this KCS channel by configuring both the
 * internal interrupt mask (for specific KCS events) and the external
 * interrupt mask (for the overall channel).
 *
 * Return: 0 on success, negative error code on failure
 */
static int axiado_kcs_enable_interrupts(struct axiado_kcs_bmc *kcs_axiado)
{
	u32 ext_bit = BIT(kcs_axiado->isr_bit);
	u32 internal_bits = KCS_INTERNAL_IRQ_INPUT; /* host writes */
	u32 channel_mask;
	u32 current_mask;
	u32 channel_bits;
	int ret;

	/* Enable internal interrupt bits for this channel */
	internal_bits <<= (kcs_axiado->index * 3);
	channel_mask = 0x7 << (kcs_axiado->index * 3);

	ret = kcs_reg_update_bits(kcs_axiado,
				  kcs_axiado->atu_base + ATU_INTERNAL_ISR_MASK,
				  channel_mask,
				  ~internal_bits & channel_mask); /* Clear mask = enable */
	if (ret)
		return ret;

	/* Read back the register to show actual state */
	current_mask = kcs_reg_read(kcs_axiado,
				    kcs_axiado->atu_base + ATU_INTERNAL_ISR_MASK);

	channel_bits = (current_mask >> (kcs_axiado->index * 3)) & 0x7;
	dev_dbg(kcs_axiado->dev,
		"KCS%d Internal ISR: 0x%x (RX:%d, TX:%d, CMD:%d)\n",
		kcs_axiado->index, channel_bits,
		!!(channel_bits & BIT(0)),  /* RX write */
		!!(channel_bits & BIT(1)),  /* TX read */
		!!(channel_bits & BIT(2))); /* CMD write */

	/* Enable external ISR */
	ret = kcs_reg_update_bits(kcs_axiado, kcs_axiado->atu_base + ATU_ISR_MASK,
				  ext_bit, 0); /* Clear mask = enable */
	if (ret)
		return ret;

	dev_dbg(kcs_axiado->dev, "KCS%d: Interrupts enabled (ISR bit %d)\n",
		kcs_axiado->index, kcs_axiado->isr_bit);

	return 0;
}

/**
 * axiado_kcs_disable_interrupts - Disable KCS interrupts
 * @kcs_axiado: KCS driver instance
 */
static void axiado_kcs_disable_interrupts(struct axiado_kcs_bmc *kcs_axiado)
{
	u32 ext_bit = BIT(kcs_axiado->isr_bit);
	u32 channel_mask = 0x7 << (kcs_axiado->index * 3);

	/* Disable internal interrupts for this channel */
	kcs_reg_update_bits(kcs_axiado, kcs_axiado->atu_base + ATU_INTERNAL_ISR_MASK,
			    channel_mask, channel_mask); /* Set mask = disable */

	/* Disable external ISR */
	kcs_reg_update_bits(kcs_axiado, kcs_axiado->atu_base + ATU_ISR_MASK,
			    ext_bit, ext_bit); /* Set mask = disable */

	dev_dbg(kcs_axiado->dev, "KCS%d: Interrupts disabled\n", kcs_axiado->index);
}

/**
 * to_kcs_axiado - Get KCS driver instance from file pointer
 * @filp: File pointer
 *
 * Return: Pointer to KCS driver instance
 */
static inline struct axiado_kcs_bmc *to_kcs_axiado(struct file *filp)
{
	return container_of(filp->private_data, struct axiado_kcs_bmc, miscdev);
}

/**
 * axiado_kcs_open - Open KCS character device
 * @inode: Inode structure
 * @filp: File pointer
 *
 * Called when userspace opens the KCS character device. Initializes
 * the device state, enables interrupts, and sets up the initial
 * IPMI KCS protocol state.
 *
 * Return: 0 on success, negative error code on failure
 */
static int axiado_kcs_open(struct inode *inode, struct file *filp)
{
	struct axiado_kcs_bmc *kcs_axiado = to_kcs_axiado(filp);
	int ret;

	ret = axiado_kcs_enable_interrupts(kcs_axiado);
	if (ret) {
		dev_err(kcs_axiado->dev, "Failed to enable interrupts: %d", ret);
		return ret;
	}

	/* Initialize to idle state */
	spin_lock(&kcs_axiado->lock);
	kcs_axiado->phase = KCS_PHASE_IDLE;
	kcs_axiado->error = KCS_NO_ERROR;
	kcs_axiado->data_in_avail = false;
	kcs_axiado->data_in_idx = 0;
	kcs_axiado->data_out_idx = 0;
	kcs_axiado->data_out_len = 0;
	spin_unlock(&kcs_axiado->lock);

	/* Set hardware state outside spinlock - regmap operations can sleep */
	set_state(kcs_axiado, IDLE_STATE);

	dev_dbg(kcs_axiado->dev, "KCS%d: Device opened\n", kcs_axiado->index);
	return 0;
}

/**
 * axiado_kcs_poll - Poll for KCS device events
 * @filp: File pointer
 * @wait: Poll table
 *
 * Implements the poll operation for the KCS character device.
 * Returns EPOLLIN when IPMI request data is available for reading.
 *
 * Return: Poll mask indicating available operations
 */
static __poll_t axiado_kcs_poll(struct file *filp, poll_table *wait)
{
	struct axiado_kcs_bmc *kcs_axiado = to_kcs_axiado(filp);
	__poll_t mask = 0;

	poll_wait(filp, &kcs_axiado->queue, wait);

	spin_lock_irq(&kcs_axiado->lock);
	if (kcs_axiado->data_in_avail)
		mask |= EPOLLIN;
	spin_unlock_irq(&kcs_axiado->lock);

	return mask;
}

/**
 * axiado_kcs_read - Read IPMI request from KCS device
 * @filp: File pointer
 * @buf: User buffer to copy data to
 * @count: Maximum number of bytes to read
 * @ppos: File position (unused)
 *
 * Reads an IPMI request message from the KCS interface. This function
 * blocks (unless O_NONBLOCK is set) until a complete IPMI message
 * is available from the host.
 *
 * Return: Number of bytes read, or negative error code
 */
static ssize_t axiado_kcs_read(struct file *filp, char __user *buf,
			       size_t count, loff_t *ppos)
{
	struct axiado_kcs_bmc *kcs_axiado = to_kcs_axiado(filp);
	bool data_avail;
	size_t data_len;
	ssize_t ret;

	if (!(filp->f_flags & O_NONBLOCK))
		wait_event_interruptible(kcs_axiado->queue,
					 kcs_axiado->data_in_avail);

	mutex_lock(&kcs_axiado->mutex);

	spin_lock_irq(&kcs_axiado->lock);
	data_avail = kcs_axiado->data_in_avail;
	if (data_avail) {
		data_len = kcs_axiado->data_in_idx;
		memcpy(kcs_axiado->kbuffer, kcs_axiado->data_in, data_len);
	}
	spin_unlock_irq(&kcs_axiado->lock);

	if (!data_avail) {
		ret = -EAGAIN;
		goto out_unlock;
	}

	if (count < data_len) {
		dev_err(kcs_axiado->dev, "channel=%u with too large data: %zu",
			kcs_axiado->index, data_len);

		spin_lock_irq(&kcs_axiado->lock);
		kcs_bmc_force_abort(kcs_axiado);
		spin_unlock_irq(&kcs_axiado->lock);

		ret = -EOVERFLOW;
		goto out_unlock;
	}

	if (copy_to_user(buf, kcs_axiado->kbuffer, data_len)) {
		ret = -EFAULT;
		goto out_unlock;
	}

	ret = data_len;

	spin_lock_irq(&kcs_axiado->lock);
	if (kcs_axiado->phase == KCS_PHASE_WRITE_DONE) {
		kcs_axiado->phase = KCS_PHASE_WAIT_READ;
		kcs_axiado->data_in_avail = false;
		kcs_axiado->data_in_idx = 0;
	} else {
		ret = -EAGAIN;
	}
	spin_unlock_irq(&kcs_axiado->lock);

out_unlock:
	mutex_unlock(&kcs_axiado->mutex);
	return ret;
}

/**
 * axiado_kcs_write - Write IPMI response to KCS device
 * @filp: File pointer
 * @buf: User buffer containing response data
 * @count: Number of bytes to write
 * @ppos: File position (unused)
 *
 * Writes an IPMI response message to the KCS interface for transmission
 * to the host. The response must be written after a complete request
 * has been read from the interface.
 *
 * Return: Number of bytes written, or negative error code
 */
static ssize_t axiado_kcs_write(struct file *filp, const char __user *buf,
				size_t count, loff_t *ppos)
{
	struct axiado_kcs_bmc *kcs_axiado = to_kcs_axiado(filp);
	ssize_t ret;

	/* Minimum response size: netfn + cmd + ccode */
	if (count < 3 || count > KCS_MSG_BUFSIZ)
		return -EINVAL;

	mutex_lock(&kcs_axiado->mutex);

	if (copy_from_user(kcs_axiado->kbuffer, buf, count)) {
		ret = -EFAULT;
		goto out_unlock;
	}

	spin_lock_irq(&kcs_axiado->lock);
	if (kcs_axiado->phase == KCS_PHASE_WAIT_READ) {
		kcs_axiado->phase = KCS_PHASE_READ;
		kcs_axiado->data_out_idx = 1;
		kcs_axiado->data_out_len = count;
		memcpy(kcs_axiado->data_out, kcs_axiado->kbuffer, count);
		ret = count;
	} else {
		ret = -EINVAL;
	}
	spin_unlock_irq(&kcs_axiado->lock);

	/* Write first response byte outside spinlock - regmap can sleep */
	if (ret > 0)
		write_data_out(kcs_axiado, kcs_axiado->data_out[0]);

out_unlock:
	mutex_unlock(&kcs_axiado->mutex);
	return ret;
}

/**
 * axiado_kcs_ioctl - Handle IOCTL operations for KCS device
 * @filp: File pointer
 * @cmd: IOCTL command
 * @arg: IOCTL argument
 *
 * Handles IOCTL operations for the KCS character device. Supports
 * standard IPMI BMC operations like setting/clearing SMS_ATN and
 * forcing abort conditions.
 *
 * Return: 0 on success, negative error code on failure
 */
static long axiado_kcs_ioctl(struct file *filp, unsigned int cmd,
			     unsigned long arg)
{
	struct axiado_kcs_bmc *kcs_axiado = to_kcs_axiado(filp);
	long ret = 0;

	spin_lock_irq(&kcs_axiado->lock);

	switch (cmd) {
	case IPMI_BMC_IOCTL_SET_SMS_ATN:
		update_status(kcs_axiado, KCS_STATUS_SMS_ATN, KCS_STATUS_SMS_ATN);
		break;

	case IPMI_BMC_IOCTL_CLEAR_SMS_ATN:
		update_status(kcs_axiado, KCS_STATUS_SMS_ATN, 0);
		break;

	case IPMI_BMC_IOCTL_FORCE_ABORT:
		kcs_bmc_force_abort(kcs_axiado);
		break;

	default:
		ret = -EINVAL;
		break;
	}

	spin_unlock_irq(&kcs_axiado->lock);
	return ret;
}

/**
 * axiado_kcs_release - Close KCS character device
 * @inode: Inode structure
 * @filp: File pointer
 *
 * Return: 0 on success
 */
static int axiado_kcs_release(struct inode *inode, struct file *filp)
{
	struct axiado_kcs_bmc *kcs_axiado = to_kcs_axiado(filp);

	kcs_bmc_force_abort(kcs_axiado);
	axiado_kcs_disable_interrupts(kcs_axiado);

	dev_dbg(kcs_axiado->dev, "KCS%d: Device closed\n", kcs_axiado->index);
	return 0;
}

/* File operations structure for KCS character device */
static const struct file_operations axiado_kcs_fops = {
	.owner          = THIS_MODULE,
	.open           = axiado_kcs_open,
	.read           = axiado_kcs_read,
	.write          = axiado_kcs_write,
	.release        = axiado_kcs_release,
	.poll           = axiado_kcs_poll,
	.unlocked_ioctl = axiado_kcs_ioctl,
};

/**
 * axiado_kcs_probe - Probe KCS BMC driver
 * @pdev: Platform device
 *
 * Probes and initializes a KCS BMC driver instance. This function:
 * - Parses device tree properties
 * - Allocates and initializes driver structures
 * - Configures hardware (ATU, interrupts)
 * - Registers character device
 * - Sets up IPMI protocol state machine
 *
 * Return: 0 on success, negative error code on failure
 */
static int axiado_kcs_probe(struct platform_device *pdev)
{
	struct axiado_kcs_bmc *kcs_axiado;
	struct regmap_irq_chip_data *irq_data;
	struct device *espi_dev, *mfd_dev;
	struct device_node *espi_np;
	u32 index, isr_bit, io_addr, offset, addr_size;
	enum kcs_offset_enc hw_offset;
	int ret;

	dev_dbg(&pdev->dev, "KCS BMC driver probing\n");

	ret = of_property_read_u32(pdev->dev.of_node, "channel-index", &index);
	if (ret || index >= KCS_COUNT) {
		dev_err(&pdev->dev, "Invalid channel-index: %d\n", ret ? ret : index);
		return ret ? ret : -EINVAL;
	}

	if (kcs_instances[index]) {
		dev_err(&pdev->dev, "KCS channel %d already in use\n", index);
		return -EBUSY;
	}

	ret = of_property_read_u32_index(pdev->dev.of_node, "interrupts", 0, &isr_bit);
	if (ret) {
		dev_err(&pdev->dev, "Missing interrupts property: %d\n", ret);
		return ret;
	}

	ret = of_property_read_u32(pdev->dev.of_node, "io-addr", &io_addr);
	if (ret) {
		dev_err(&pdev->dev, "Missing io-addr property\n");
		return ret;
	}

	if (of_property_read_u32(pdev->dev.of_node, "host-status-offset", &offset)) {
		offset = 1;  /* default */
		dev_dbg(&pdev->dev, "Using default host-status-offset: %u\n", offset);
	}

	hw_offset = kcs_offset_to_hw_enc(offset);
	if (hw_offset == KCS_OFFSET_INVALID) {
		dev_err(&pdev->dev, "Invalid host-status-offset: %u (must be 1,2,4,8,16)\n",
			offset);
		return -EINVAL;
	}

	if (of_property_read_u32(pdev->dev.of_node, "addr-size", &addr_size)) {
		addr_size = 1;  /* default */
		dev_dbg(&pdev->dev, "Using default addr-size: %u\n", addr_size);
	}

	kcs_axiado = devm_kzalloc(&pdev->dev, sizeof(*kcs_axiado), GFP_KERNEL);
	if (!kcs_axiado)
		return -ENOMEM;

	kcs_axiado->dev = &pdev->dev;
	kcs_axiado->index = index;
	kcs_axiado->io_addr = io_addr;
	kcs_axiado->status_reg_offset = offset;
	kcs_axiado->hw_offset_enc = hw_offset;
	kcs_axiado->addr_size = addr_size;
	kcs_axiado->isr_bit = isr_bit;

	espi_dev = pdev->dev.parent;
	if (!espi_dev) {
		dev_err(&pdev->dev, "No parent eSPI device\n");
		return -ENODEV;
	}

	espi_np = dev_of_node(espi_dev);
	if (!espi_np) {
		dev_err(&pdev->dev, "No eSPI device node\n");
		return -ENODEV;
	}

	mfd_dev = espi_dev->parent;
	if (!mfd_dev) {
		dev_err(&pdev->dev, "No MFD device\n");
		return -ENODEV;
	}

	ret = axiado_kcs_get_resources(kcs_axiado, espi_np, mfd_dev);
	if (ret)
		return ret;

	irq_data = dev_get_drvdata(espi_dev);
	if (!irq_data) {
		dev_err(&pdev->dev, "eSPI IRQ chip not ready\n");
		return -EPROBE_DEFER;
	}

	kcs_axiado->irq = regmap_irq_get_virq(irq_data, isr_bit);
	if (kcs_axiado->irq < 0) {
		dev_err(&pdev->dev, "Failed to get IRQ: %d\n", kcs_axiado->irq);
		return kcs_axiado->irq;
	}

	spin_lock_init(&kcs_axiado->lock);
	mutex_init(&kcs_axiado->mutex);
	init_waitqueue_head(&kcs_axiado->queue);

	kcs_axiado->phase = KCS_PHASE_IDLE;
	kcs_axiado->error = KCS_NO_ERROR;
	kcs_axiado->data_in_avail = false;
	kcs_axiado->data_in_idx = 0;
	kcs_axiado->data_out_idx = 0;
	kcs_axiado->data_out_len = 0;

	kcs_axiado->data_in = devm_kmalloc(&pdev->dev, KCS_MSG_BUFSIZ, GFP_KERNEL);
	kcs_axiado->data_out = devm_kmalloc(&pdev->dev, KCS_MSG_BUFSIZ, GFP_KERNEL);
	kcs_axiado->kbuffer = devm_kmalloc(&pdev->dev, KCS_MSG_BUFSIZ, GFP_KERNEL);
	if (!kcs_axiado->data_in || !kcs_axiado->data_out || !kcs_axiado->kbuffer)
		return -ENOMEM;

	ret = axiado_kcs_configure_atu(kcs_axiado);
	if (ret) {
		dev_err(&pdev->dev, "ATU configuration failed: %d\n", ret);
		return ret;
	}

	ret = devm_request_threaded_irq(&pdev->dev, kcs_axiado->irq, NULL,
					axiado_kcs_irq_handler,
					IRQF_ONESHOT | IRQF_TRIGGER_HIGH,
					"axiado-kcs-bmc", kcs_axiado);
	if (ret) {
		dev_err(&pdev->dev, "IRQ request failed: %d\n", ret);
		goto err_disable_atu;
	}

	kcs_axiado->miscdev.minor = MISC_DYNAMIC_MINOR;
	kcs_axiado->miscdev.name = devm_kasprintf(&pdev->dev, GFP_KERNEL, "%s%u",
						  DEVICE_NAME, index);
	if (!kcs_axiado->miscdev.name) {
		ret = -ENOMEM;
		goto err_disable_atu;
	}
	kcs_axiado->miscdev.fops = &axiado_kcs_fops;

	ret = misc_register(&kcs_axiado->miscdev);
	if (ret) {
		dev_err(&pdev->dev, "Device registration failed: %d\n", ret);
		goto err_disable_atu;
	}

	platform_set_drvdata(pdev, kcs_axiado);
	kcs_instances[index] = kcs_axiado;

	dev_info(&pdev->dev, "KCS%d initialized (IRQ:%d, IO:0x%04x, ISR:%d)\n",
		 index, kcs_axiado->irq, io_addr, isr_bit);

	return 0;

err_disable_atu:
	axiado_kcs_disable_atu(kcs_axiado);
	return ret;
}

/**
 * axiado_kcs_remove - Remove KCS BMC driver
 * @pdev: Platform device
 *
 * Return: 0 on success
 */
static int axiado_kcs_remove(struct platform_device *pdev)
{
	struct axiado_kcs_bmc *kcs_axiado = platform_get_drvdata(pdev);
	int index = kcs_axiado->index;

	misc_deregister(&kcs_axiado->miscdev);

	axiado_kcs_disable_interrupts(kcs_axiado);
	axiado_kcs_disable_atu(kcs_axiado);

	kcs_instances[index] = NULL;

	dev_dbg(&pdev->dev, "KCS BMC driver removed for channel %d\n", index);
	return 0;
}

static const struct of_device_id axiado_kcs_of_match[] = {
	{ .compatible = "axiado,ax3000-kcs-bmc" },
	{},
};
MODULE_DEVICE_TABLE(of, axiado_kcs_of_match);

static struct platform_driver axiado_kcs_driver = {
	.probe = axiado_kcs_probe,
	.remove = axiado_kcs_remove,
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = of_match_ptr(axiado_kcs_of_match),
	},
};

module_platform_driver(axiado_kcs_driver);

MODULE_AUTHOR("AXIADO CORPORATION");
MODULE_DESCRIPTION("Axiado device interface to the KCS BMC device");
MODULE_LICENSE("GPL v2");
