// SPDX-License-Identifier: GPL-2.0-only
/*
 * Axiado UART Driver
 * Copyright (c) 2025 Axiado Corporation
 *
 * This driver provides 8250 based UART implementation for Axiado's eSPI
 * external logic hardware.
 *
 * Hardware Architecture:
 * - Uses Axiado's eSPI ATU (Address Translation Unit) for host address mapping
 * - Supports 4 separate UART registers per channel
 * - Each instance has two UART connected with back to back
 * - [HOST] <-UART over eSPI-> [eSPI] <-eSPI Iface-> [UART0_X] <- rx/tx -> [UART1_X]
 * - Integrates with eSPI external logic interrupt system
 * - Uses regmap interface for hardware register access
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/serial_8250.h>
#include <linux/serial_reg.h>
#include <linux/serial_core.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/bitops.h>
#include <linux/regmap.h>
#include <linux/clk.h>

#include "axiado-espi.h"
#include "8250.h"

#include <linux/kthread.h>
#include <uapi/linux/sched/types.h>

#define DRIVER_NAME "axiado-espi-uart"

/* Maximum number of UART channels */
#define UART_COUNT 4

/* UART register blocks in Section B - Based on actual documentation ranges
 * From register doc:
 * UART0: 0x1040-0x105C (28 bytes used, 0x1C)
 * UART1: 0x1060-0x107C (28 bytes used, 0x1C)
 * UART2: 0x1080-0x109C (28 bytes used, 0x1C)
 * UART3: 0x10A0-0x10BC (28 bytes used, 0x1C)
 * Each UART uses 28 bytes but spaced 32 bytes apart for alignment
 */
#define UART_REG_BLOCK_SIZE  0x20 /* 32-byte spacing between UARTs */
#define UART_BLOCK_USED_SIZE 0x1C /* 28 bytes actually used per UART */
#define UART_BASE_OFFSET(n)  (0x040 + ((n) * UART_REG_BLOCK_SIZE))

/* UART register offsets within each UART block (16750 standard) */
#define UART_REG_RBR_THR 0x00 /* Receive/Transmit Holding Register */
#define UART_REG_IER     0x04 /* Interrupt Enable Register */
#define UART_REG_IIR_FCR 0x08 /* Interrupt ID/FIFO Control Register */
#define UART_REG_LCR     0x0C /* Line Control Register */
#define UART_REG_MCR     0x10 /* Modem Control Register */
#define UART_REG_LSR     0x14 /* Line Status Register */
#define UART_REG_MSR     0x18 /* Modem Status Register */
#define UART_REG_SCR     0x1C /* Scratch Register */
/* When LCR[7] = 1 (DLAB), RBR/THR become DLL/DLM */
#define UART_REG_DLL     0x00 /* Divisor Latch Low (when DLAB=1) */
#define UART_REG_DLM     0x04 /* Divisor Latch High (when DLAB=1) */

/* ATU register calculation macros - Section C offsets */
#define ATU_UART_START_LSB(n) \
	(0x020 + ((n) * 8)) /* UART0=0x20, UART1=0x28, etc */
#define ATU_UART_START_MSB(n) (0x024 + ((n) * 8))
#define ATU_UART_END_LSB(n) \
	(0x0A8 + ((n) * 8)) /* UART0=0xA8, UART1=0xB0, etc */
#define ATU_UART_END_MSB(n) (0x0AC + ((n) * 8))

/* ATU Control Registers - Section C */
#define ATU_ENABLE 0x150 /* ATU Enable Register */
#define ATU_ERROR_STATUS 0x158 /* ATU Error Status */

/* UART Status Registers - Section C (from register documentation) */
#define ESPI_UART_STATUS_REG(n) \
	(0x170 + ((n) * 0x0C)) /* UART0=0x170, UART1=0x17C, etc */

/* eSPI UART reigster address offsets in each block*/
#define ESPI_UART_REG_THR(n)	(ESPI_UART_STATUS_REG(n) + 0x0)
#define ESPI_UART_REG_RBR(n)	(ESPI_UART_STATUS_REG(n) + 0x0)
#define ESPI_UART_REG_DLL(n)	(ESPI_UART_STATUS_REG(n) + 0x0)
#define ESPI_UART_REG_DLM(n)	(ESPI_UART_STATUS_REG(n) + 0x0)
#define ESPI_UART_REG_IER(n)	(ESPI_UART_STATUS_REG(n) + 0x1)
#define ESPI_UART_REG_IIR(n)	(ESPI_UART_STATUS_REG(n) + 0x1)
#define ESPI_UART_REG_FCR(n)	(ESPI_UART_STATUS_REG(n) + 0x1)
#define ESPI_UART_REG_LCR(n)	(ESPI_UART_STATUS_REG(n) + 0x1)
#define ESPI_UART_REG_MCR(n)	(ESPI_UART_STATUS_REG(n) + 0x2)
#define ESPI_UART_REG_LSR(n)	(ESPI_UART_STATUS_REG(n) + 0x2)
#define ESPI_UART_REG_MSR(n)	(ESPI_UART_STATUS_REG(n) + 0x2)
#define ESPI_UART_REG_SCR(n)	(ESPI_UART_STATUS_REG(n) + 0x2)

/* Interrupt Registers - Section C */
#define ATU_INTERNAL_ISR      0x15C /* Internal ISR Register */
#define ATU_INTERNAL_ISR_MASK 0x160 /* Internal ISR Mask Register */
#define ATU_ISR               0x168 /* Interrupt Status Register (Clear-On-Read) */
#define ATU_ISR_MASK          0x16C /* Interrupt Status Mask Register */

/* UART clock divider register */
#define ESPI_UART_CLKDIV 0x164 /* eSPI UART Clock Divider Register */

/* FIFO Depths (from documentation) */
#define UART0_FIFO_SIZE 16 /* UART 0 (eSPI side) */
#define UART1_FIFO_SIZE 64 /* UART 1 (CPU side) */

/* Default baud rate */
#define DEFAULT_BAUD_RATE 115200

/* Helper macros for ATU enable bits */
#define ATU_ENABLE_UART_BIT(n) BIT(4 + (n)) /* UART0=bit4, UART1=bit5, etc */

#define ATU_REG_BLOCK_SIZE	0x1d4

/**
 * struct axiado_espi_uart - UART driver data with virtual IRQ support
 * @dev: Platform device
 * @regmap: Unified regmap from eSPI parent
 * @perif_base: Base address for peripheral registers
 * @atu_base: Base address for ATU registers
 * @uart_port: Standard 8250 UART port
 * @index: UART channel index (0-3)
 * @io_addr: I/O port address from host perspective
 * @fifo_size: eSPI side FIFO size
 * @cpu_fifo_size: CPU side FIFO size
 * @irq_cpu: Virtual IRQ for CPU side interrupt
 * @line: TTY line number
 * @clk: UART clock
 * @isr_bit: ISR bit number for this KCS channel
 * @mutex: Mutex for file operations
 */
struct axiado_espi_uart {
	struct device *dev;
	struct work_struct tx_work;
	struct regmap *regmap;
	u16 perif_base;
	u16 atu_base;
	struct uart_8250_port *uart_port;
	int index;
	u32 io_addr; /* I/O port address from host perspective */
	u32 fifo_size; /* UART 0 (eSPI side) FIFO size */
	u32 cpu_fifo_size; /* UART 1 (CPU side) FIFO size */
	int irq_cpu; /* Virtual IRQ for CPU side */
	int line; /* TTY line number */
	struct clk *clk;
	int isr_bit; /* CPU side IRQ bit number */
	struct mutex mutex; /* protects register access */
};

/* Global array of UART instances */
static struct axiado_espi_uart *uart_instances[UART_COUNT];

/**
 * axiado_uart_read_cpu_reg - Read a register from UART 1 (CPU side)
 * @uart: UART driver data
 * @reg: Register offset within UART block
 *
 * Return: Register value
 */
static u32 axiado_uart_read_cpu_reg(struct axiado_espi_uart *uart, u32 reg)
{
	u32 offset = UART_BASE_OFFSET(uart->index) + reg;
	u32 value;
	int ret;

	/* Ensure we don't access beyond the 28-byte used block */
	if (reg >= UART_BLOCK_USED_SIZE) {
		dev_err(uart->dev,
			"Invalid UART register offset: 0x%x (max: 0x%x)\n", reg,
			UART_BLOCK_USED_SIZE - 1);
		return 0;
	}

	ret = regmap_read(uart->regmap, uart->perif_base + offset, &value);

	if (ret) {
		dev_err(uart->dev,
			"Failed to read CPU UART register 0x%x: %d\n", offset,
			ret);
		return 0;
	}

	return value & 0xFF; /* UART registers are 8-bit */
}

/**
 * axiado_uart_write_cpu_reg - Write a register to UART 1 (CPU side)
 * @uart: UART driver data
 * @reg: Register offset within UART block
 * @value: Value to write
 */
static void axiado_uart_write_cpu_reg(struct axiado_espi_uart *uart, u32 reg,
				      u8 value)
{
	u32 offset = UART_BASE_OFFSET(uart->index) + reg;
	int ret;

	/* Ensure we don't access beyond the 28-byte used block */
	if (reg >= UART_BLOCK_USED_SIZE) {
		dev_err(uart->dev,
			"Invalid UART register offset: 0x%x (max: 0x%x)\n", reg,
			UART_BLOCK_USED_SIZE - 1);
		return;
	}


	ret = regmap_write(uart->regmap, uart->perif_base + offset, value);
	if (ret) {
		dev_err(uart->dev,
			"Failed to write CPU UART register 0x%x: %d\n", offset,
			ret);
	}
}

/**
 * axiado_uart_read_espi_reg - Read a register from UART 0 (eSPI side) for monitoring
 * @uart: UART driver data
 * @reg: Register offset
 *
 * Return: Register value, zero will be returned in error cases.
 */
static u32 axiado_uart_read_espi_reg(struct axiado_espi_uart *uart, u32 reg)
{
	u32 value;
	int ret;

	/* Ensure we don't access beyond the ESPI ATU block */
	if (reg > ATU_REG_BLOCK_SIZE) {
		dev_err(uart->dev,
			"Invalid ATU register offset: 0x%x (max: 0x%x)\n", reg,
			UART_BLOCK_USED_SIZE);
		return 0;
	}


	/* Read from UART status register block - this gives us access to UART 0 registers */
	ret = regmap_read(uart->regmap, uart->atu_base + reg,
			  &value);
	if (ret) {
		dev_err(uart->dev, "Failed to read eSPI UART status: %d\n",
			ret);
		return 0;
	}
	return value;
}

/**
 * axiado_uart_write_espi_reg - Write a register from UART 0 (eSPI side) for monitoring
 * @uart: UART driver data
 * @reg: Register offset
 * @value: Value to write
 *
 * Return: A value of zero will be returned on success, a negative errno will
 * be returned in error cases.
 */
static int axiado_uart_write_espi_reg(struct axiado_espi_uart *uart, u32 reg,
					u32 value)
{
	int ret;

	/* Ensure we don't access beyond the ESPI ATU block */
	if (reg > ATU_REG_BLOCK_SIZE) {
		dev_err(uart->dev,
			"Invalid ATU register offset: 0x%x (max: 0x%x)\n", reg,
			UART_BLOCK_USED_SIZE);
		return -EINVAL;
	}


	ret = regmap_write(uart->regmap, uart->atu_base + reg, value);
	if (ret) {
		dev_err(uart->dev, "Failed to write eSPI UART status: %d\n",
			ret);
		return ret;
	}
	return 0;
}

/**
 * axiado_uart_configure_atu - Configure ATU for UART channel
 * @uart: UART driver data
 *
 * Return: 0 on success, negative error code on failure
 */
static int axiado_uart_configure_atu(struct axiado_espi_uart *uart)
{
	u64 start_addr = uart->io_addr;
	/* Standard 8-byte UART register space */
	u64 end_addr = uart->io_addr + 0x7;
	u32 atu_en_bit = ATU_ENABLE_UART_BIT(uart->index);
	int ret;

	/* Configure ATU start address (64-bit) */
	ret = axiado_uart_write_espi_reg(uart, ATU_UART_START_LSB(uart->index),
			   (u32)(start_addr & 0xFFFFFFFF));
	if (ret) {
		dev_err(uart->dev, "Failed to write ATU start LSB\n");
		return ret;
	}

	ret = axiado_uart_write_espi_reg(uart, ATU_UART_START_MSB(uart->index),
			   (u32)(start_addr >> 32));
	if (ret) {
		dev_err(uart->dev, "Failed to write ATU start MSB\n");
		return ret;
	}

	/* Configure ATU end address (64-bit) */
	ret = axiado_uart_write_espi_reg(uart, ATU_UART_END_LSB(uart->index),
			   (u32)(end_addr & 0xFFFFFFFF));
	if (ret) {
		dev_err(uart->dev, "Failed to write ATU end LSB\n");
		return ret;
	}

	ret = axiado_uart_write_espi_reg(uart, ATU_UART_END_MSB(uart->index),
			   (u32)(end_addr >> 32));
	if (ret) {
		dev_err(uart->dev, "Failed to write ATU end MSB\n");
		return ret;
	}

	/* Enable the ATU */
	ret = regmap_update_bits(uart->regmap, uart->atu_base + ATU_ENABLE,
					atu_en_bit, atu_en_bit);

	if (ret) {
		dev_err(uart->dev, "Failed to enable ATU\n");
		return ret;
	}

	dev_dbg(
		uart->dev,
		"Configured ATU for UART%d: range 0x%llx-0x%llx (28-byte block)\n",
		uart->index, start_addr, end_addr);

	return 0;
}

/**
 * axiado_uart_disable_atu - Disable ATU for UART channel
 * @uart: UART driver data
 */
static void axiado_uart_disable_atu(struct axiado_espi_uart *uart)
{
	u32 atu_en_bit = ATU_ENABLE_UART_BIT(uart->index);

	/* Disable the ATU */
	regmap_update_bits(uart->regmap, uart->atu_base + ATU_ENABLE,
							atu_en_bit, 0);

	dev_dbg(uart->dev, "Disabled ATU for UART%d\n", uart->index);
}

/**
 * axiado_uart_get_resources() - Get eSPI resources (regmap and register bases)
 * @uart: UART instance
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
static int axiado_uart_get_resources(struct axiado_espi_uart *uart,
				    struct device_node *espi_np,
				    struct device *mfd_dev)
{
	u64 addr, size;
	int ret;

	if (!espi_np) {
		dev_err(uart->dev, "eSPI device node is NULL\n");
		return -EINVAL;
	}

	if (!mfd_dev) {
		dev_err(uart->dev, "MFD device is NULL\n");
		return -EINVAL;
	}

	/* Get regmap from MFD (grandparent) */
	uart->regmap = dev_get_regmap(mfd_dev, NULL);
	if (!uart->regmap) {
		dev_err(uart->dev, "Failed to get regmap from MFD\n");
		return -ENODEV;
	}

	/* Read peripheral region (index 1) from eSPI reg property */
	ret = of_property_read_reg(espi_np, 1, &addr, &size);
	if (ret) {
		dev_err(uart->dev, "Failed to read eSPI peripheral region: %d\n", ret);
		return ret;
	}
	uart->perif_base = addr;

	/* Read ATU region (index 2) from eSPI reg property */
	ret = of_property_read_reg(espi_np, 2, &addr, &size);
	if (ret) {
		dev_err(uart->dev, "Failed to read eSPI ATU region: %d\n", ret);
		return ret;
	}
	uart->atu_base = addr;

	dev_dbg(uart->dev, "eSPI register offsets - Perif: 0x%04llx, ATU: 0x%04llx\n",
		(u64)uart->perif_base, (u64)uart->atu_base);

	return 0;
}

/**
 * axiado_uart_configure_baud_rate - Configure baud rate with master-slave setup
 * @uart: UART driver data
 * @baud: Baud rate
 */
__maybe_unused static void axiado_uart_configure_baud_rate(struct axiado_espi_uart *uart,
					    unsigned int baud)
{
	u8 dll;
	u8 dlm;
	u8 orig_lcr;

	/* UART 0 (eSPI side) operates as slave - DLL/DLM default to 0 for
	 * max frequency. This is handled automatically by the hardware
	 * as per documentation
	 */

	/* Copy settings from UART0 (eSPI Side) */
	dll = (axiado_uart_read_espi_reg(
			uart, ESPI_UART_REG_DLL(uart->index)) >> 16) & 0xFF;
	dlm = (axiado_uart_read_espi_reg(
			uart, ESPI_UART_REG_DLM(uart->index)) >> 24) & 0xFF;

	orig_lcr = axiado_uart_read_cpu_reg(uart, UART_REG_LCR);
	/* set DLAB */
	axiado_uart_write_cpu_reg(uart, UART_REG_LCR, UART_LCR_DLAB);
	axiado_uart_write_cpu_reg(uart, UART_REG_DLL, dll);
	axiado_uart_write_cpu_reg(uart, UART_REG_DLM, dlm);
	/* clear DLAB */
	axiado_uart_write_cpu_reg(uart, UART_REG_LCR, orig_lcr);
}

/**
 * axiado_uart_setup_fifos - Setup FIFO configuration for both UARTs
 * @uart: UART driver data
 */
static void axiado_uart_setup_fifos(struct axiado_espi_uart *uart)
{
	u8 fcr_value;

	mutex_lock(&uart->mutex);

	/* Configure UART 1 (CPU side) - 64-byte FIFO */
	fcr_value = UART_FCR_ENABLE_FIFO | UART_FCR_CLEAR_RCVR |
		    UART_FCR_CLEAR_XMIT;
	if (uart->cpu_fifo_size == 64)
		fcr_value |= UART_FCR_TRIGGER_14; /* Trigger at 56 bytes */
	else
		fcr_value |= UART_FCR_TRIGGER_8; /* Trigger at half-full */

	axiado_uart_write_cpu_reg(uart, UART_REG_IIR_FCR, fcr_value);

	mutex_unlock(&uart->mutex);

	dev_dbg(
		uart->dev,
		"FIFO configuration: CPU=%u bytes, eSPI=%u bytes (28-byte register block)\n",
		uart->cpu_fifo_size, uart->fifo_size);
}

/**
 * axiado_uart_cpu_irq_handler - CPU side UART interrupt handler
 * @irq: Virtual IRQ number
 * @data: Driver data
 *
 * Return: IRQ_HANDLED
 */
static irqreturn_t axiado_uart_cpu_irq_handler(int irq, void *data)
{
	struct uart_port *port = data;
	struct axiado_espi_uart *uart = port->private_data;
	struct uart_8250_port *up = up_to_u8250p(port);
	u32 iir, lsr;


	/* Use mutex since we're in threaded context and regmap can sleep */
	mutex_lock(&uart->mutex);

	iir = axiado_uart_read_cpu_reg(uart, UART_REG_IIR_FCR);
	if (iir & UART_IIR_NO_INT)
		return IRQ_HANDLED;

	switch (iir & UART_IIR_ID) {
	case UART_IIR_RDI: /* Received Data Available */
	case UART_IIR_RLSI: /* Receiver Line Status */
	case UART_IIR_RX_TIMEOUT: /* FIFO Timeout */
		lsr = serial_in(up, UART_LSR);
		serial8250_rx_chars(up, lsr);
		break;
	/*
	 * write to UART 1, data flows as
	 * App -> TTY -> 8250  -> UART1 -> TX/RX lines -> UART 0 -> eSPI
	 * use standard 8250 framework for TX
	 */
	case UART_IIR_THRI: /* Transmitter Holding Register Empty */
		if (up->port.state != NULL)
			serial8250_tx_chars(up);

		break;
	case UART_IIR_MSI: /* Modem Status Change */
		/* TODO: handle modem status change event */
		up->msr_saved_flags =
			axiado_uart_read_cpu_reg(uart, UART_REG_MSR);
		break;
	}

	mutex_unlock(&uart->mutex);

	return IRQ_HANDLED;
}

static void axiado_serial_out(struct uart_port *port, int offset, int value)
{
	struct axiado_espi_uart *uart = port->private_data;

	axiado_uart_write_cpu_reg(uart, offset * sizeof(u32), value);

}

static unsigned int axiado_serial_in(struct uart_port *port, int offset)
{
	struct axiado_espi_uart *uart = port->private_data;
	unsigned int val;


	val = axiado_uart_read_cpu_reg(uart, offset * sizeof(u32));

	return val;
}

/**
 * axiado_uart_startup - UART port startup callback
 * @port: UART port
 *
 * Return: 0 on success, negative error code on failure
 */
static int axiado_uart_startup(struct uart_port *port)
{
	struct axiado_espi_uart *uart = port->private_data;
	u8 dll, dlm, fcr, lcr;

	/* Setup FIFOs */
	axiado_uart_setup_fifos(uart);

	/* Copy settings from UART0 (eSPI Side) */
	dll = (axiado_uart_read_espi_reg(
			uart, ESPI_UART_REG_DLL(uart->index)) >> 16) & 0xFF;
	dlm = (axiado_uart_read_espi_reg(
			uart, ESPI_UART_REG_DLM(uart->index)) >> 24) & 0xFF;
	fcr = (axiado_uart_read_espi_reg(
			uart, ESPI_UART_REG_FCR(uart->index)) >> 16) & 0xFF;
	lcr = (axiado_uart_read_espi_reg(
			uart, ESPI_UART_REG_LCR(uart->index)) >> 16) & 0xFF;

	axiado_uart_write_cpu_reg(uart, UART_REG_LCR, UART_LCR_DLAB);
	axiado_uart_write_cpu_reg(uart, UART_REG_DLL, dll);
	axiado_uart_write_cpu_reg(uart, UART_REG_DLM, dlm);

	/* clear DLAB */
	axiado_uart_write_cpu_reg(uart, UART_REG_LCR, 0x3);
	axiado_uart_write_cpu_reg(uart, UART_REG_IIR_FCR, fcr);

	axiado_uart_write_cpu_reg(uart, UART_REG_LCR, lcr);

	/* Enable interrupts on CPU side */
	axiado_uart_write_cpu_reg(uart, UART_REG_IER,
		UART_IER_RDI | UART_IER_RLSI | UART_IER_THRI | UART_IER_MSI);

	/* Unmask CPU side interrupt in CSR registers */
	regmap_update_bits(uart->regmap, uart->atu_base + ATU_ISR_MASK,
				BIT(uart->isr_bit),
				0); /* Enable CPU side interrupt */

	dev_info(uart->dev, "UART%d started\n", uart->index);
	return 0;
}

/**
 * axiado_uart_shutdown - UART port shutdown callback
 * @port: UART port
 */
static void axiado_uart_shutdown(struct uart_port *port)
{
	struct axiado_espi_uart *uart = port->private_data;


	/* Disable interrupts */
	axiado_uart_write_cpu_reg(uart, UART_REG_IER, 0);

	/* Mask CPU side interrupt in CSR registers */
	regmap_update_bits(uart->regmap, uart->atu_base + ATU_ISR_MASK,
			   BIT(uart->isr_bit), BIT(uart->isr_bit));

	dev_info(uart->dev, "UART%d shutdown\n", uart->index);
}

/**
 * axiado_uart_set_termios - Set terminal parameters
 * @port: UART port
 * @termios: New terminal settings
 * @old: Old terminal settings
 */
static void axiado_uart_set_termios(struct uart_port *port,
				    struct ktermios *termios,
				    const struct ktermios *old)
{
	struct axiado_espi_uart *uart = port->private_data;
	unsigned char lcr;

	/* Set data bits, parity, and stop bits */
	switch (termios->c_cflag & CSIZE) {
	case CS5:
		lcr = UART_LCR_WLEN5;
		break;
	case CS6:
		lcr = UART_LCR_WLEN6;
		break;
	case CS7:
		lcr = UART_LCR_WLEN7;
		break;
	case CS8:
	default:
		lcr = UART_LCR_WLEN8;
		break;
	}

	if (termios->c_cflag & CSTOPB)
		lcr |= UART_LCR_STOP;
	if (termios->c_cflag & PARENB) {
		lcr |= UART_LCR_PARITY;
		if (!(termios->c_cflag & PARODD))
			lcr |= UART_LCR_EPAR;
		if (termios->c_cflag & CMSPAR)
			lcr |= UART_LCR_SPAR;
	}

	mutex_lock(&uart->mutex);

	/* Configure line control */
	axiado_uart_write_cpu_reg(uart, UART_REG_LCR, lcr);

	/* Update timeout */
	/* TODO: How to drive baud rate here ?*/
	uart_update_timeout(port, termios->c_cflag, DEFAULT_BAUD_RATE);

	mutex_unlock(&uart->mutex);

}

static void axiado_set_mctrl(struct uart_port *port, unsigned int mctrl_data)
{
}

static unsigned int axiado_get_mctrl(struct uart_port *port)
{
	return 0;
}

static void axiado_uart_stop_rx(struct uart_port *port)
{
}

static void axiado_uart_handle_tx(struct work_struct *ws)
{
	struct axiado_espi_uart *uart = container_of(ws,
					struct axiado_espi_uart, tx_work);
	struct uart_8250_port *up = uart->uart_port;

	serial8250_tx_chars(up);
}

static void axiado_uart_start_tx(struct uart_port *port)
{
	struct axiado_espi_uart *uart = port->private_data;

	schedule_work(&uart->tx_work);
}

static unsigned int axiado_uart_tx_empty(struct uart_port *port)
{

	struct uart_8250_port *up = up_to_u8250p(port);
	u32 lsr;

	lsr = serial_in(up, UART_LSR);
	return ((lsr & UART_LSR_BOTH_EMPTY) == UART_LSR_BOTH_EMPTY) ?
				TIOCSER_TEMT : 0;
}

/**
 * axiado_uart_probe - Probe UART driver
 * @pdev: Platform device
 *
 * Return: 0 on success, negative error code on failure
 */
static int axiado_uart_probe(struct platform_device *pdev)
{
	struct axiado_espi_uart *uart;
	struct uart_8250_port up;
	struct device *espi_dev, *mfd_dev;
	struct device_node *espi_np;
	struct regmap_irq_chip_data *irq_data;
	struct uart_ops *axiado_uart_ops;
	int index;
	u32 io_addr;
	u32 fifo_size;
	u32 cpu_fifo_size;
	u32 isr_bit;
	int ret;

	ret = of_property_read_u32(pdev->dev.of_node, "channel-index", &index);
	if (ret) {
		dev_err(&pdev->dev,
			"Missing or invalid channel-index property\n");
		return -EINVAL;
	}

	if (index >= UART_COUNT) {
		dev_err(&pdev->dev,
			"UART channel index %d out of range (max %d)\n", index,
			UART_COUNT - 1);
		return -EINVAL;
	}

	if (uart_instances[index]) {
		dev_err(&pdev->dev, "UART channel %d already initialized\n",
			index);
		return -EBUSY;
	}

	ret = of_property_read_u32(pdev->dev.of_node, "io-addr", &io_addr);
	if (ret) {
		dev_err(&pdev->dev, "Missing io-addr property\n");
		return -EINVAL;
	}

	/* Need to revisit */
	ret = of_property_read_u32(pdev->dev.of_node, "fifo-size", &fifo_size);
	if (ret) {
		fifo_size = UART0_FIFO_SIZE; /* Default eSPI side FIFO */
		dev_info(&pdev->dev, "Using default fifo-size: %u\n",
			 fifo_size);
	}

	ret = of_property_read_u32(pdev->dev.of_node, "cpu-fifo-size",
				   &cpu_fifo_size);
	if (ret) {
		cpu_fifo_size = UART1_FIFO_SIZE; /* Default CPU side FIFO */
		dev_info(&pdev->dev, "Using default cpu-fifo-size: %u\n",
			 cpu_fifo_size);
	}

	ret = of_property_read_u32_index(pdev->dev.of_node, "interrupts", 0,
					 &isr_bit);
	if (ret) {
		dev_err(&pdev->dev, "Failed to read interrupts property: %d\n",
			ret);
		return ret;
	}

	uart = devm_kzalloc(&pdev->dev, sizeof(*uart), GFP_KERNEL);
	if (!uart)
		return -ENOMEM;

	uart->dev = &pdev->dev;
	uart->index = index;
	uart->io_addr = io_addr;
	uart->fifo_size = fifo_size;
	uart->cpu_fifo_size = cpu_fifo_size;
	uart->isr_bit = (int)isr_bit;
	mutex_init(&uart->mutex);

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


	/* Get regmaps from parent eSPI controller */
	ret = axiado_uart_get_resources(uart, espi_np, mfd_dev);
	if (ret)
		return ret;

	irq_data = dev_get_drvdata(espi_dev);
	if (!irq_data) {
		dev_err(&pdev->dev, "eSPI IRQ chip not ready\n");
		return -EPROBE_DEFER;
	}

	/* Get virtual IRQ from parent's IRQ chip */
	uart->irq_cpu = regmap_irq_get_virq(irq_data, isr_bit);
	if (uart->irq_cpu < 0) {
		dev_err(&pdev->dev,
			"Failed to get virtual IRQ for ISR bit %u: %d\n",
			uart->isr_bit, uart->irq_cpu);
		return uart->irq_cpu;
	}

	dev_dbg(&pdev->dev,
		 "UART%d: Got virtual IRQ %d for ISR bit %u\n",
		 index, uart->irq_cpu, uart->isr_bit);

	// TODO: Can we avoid this as we don't use it?
	/* Get clock */
	uart->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(uart->clk)) {
		dev_err(&pdev->dev, "Failed to get UART clock\n");
		return PTR_ERR(uart->clk);
	}

	/* Configure ATU for UART */
	ret = axiado_uart_configure_atu(uart);
	if (ret) {
		dev_err(&pdev->dev, "Failed to configure ATU for UART %d: %d\n",
			index, ret);
		return ret;
	}

	INIT_WORK(&uart->tx_work, axiado_uart_handle_tx);

	/* Initialize 8250 UART port */
	memset(&up, 0, sizeof(up));

	up.port.dev = &pdev->dev;
	up.port.private_data = uart;
	up.port.type = PORT_16750;
	up.port.iotype = UPIO_MEM32;
	up.port.flags = UPF_BOOT_AUTOCONF | UPF_FIXED_TYPE | UPF_FIXED_PORT;
	up.port.fifosize = cpu_fifo_size;
	up.port.line = index;
	up.port.uartclk = clk_get_rate(uart->clk);
	up.capabilities = UART_CAP_FIFO | UART_CAP_AFE;

	/* Custom register access functions, use our regmap functions */
	up.port.serial_in = axiado_serial_in;
	up.port.serial_out = axiado_serial_out;
	up.port.set_mctrl = axiado_set_mctrl;
	up.port.get_mctrl = axiado_get_mctrl;
	up.port.startup = axiado_uart_startup;
	up.port.shutdown = axiado_uart_shutdown;
	up.port.set_termios = axiado_uart_set_termios;

	/* Register with serial core */
	ret = serial8250_register_8250_port(&up);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to register 8250 port: %d\n", ret);
		goto err_disable_atu;
	}
	uart->line = ret;
	uart->uart_port = serial8250_get_port(uart->line);

	axiado_uart_ops = devm_kzalloc(&pdev->dev, sizeof(struct uart_ops),
							GFP_KERNEL);
	if (!axiado_uart_ops) {
		ret = -ENOMEM;
		goto err_port_cleanup;
	}

	/* TTY UART framework calls some of the ops functions in atomic context,
	 * To avoid implementation of all ops functions, Only necessary ones are
	 * implemened and rest of are copied from default uart_port.
	 * uart_port.ops are marked as const so we can't change any pointers
	 * directly.
	 */
	memcpy(axiado_uart_ops, uart->uart_port->port.ops,
						sizeof(struct uart_ops));
	axiado_uart_ops->tx_empty = axiado_uart_tx_empty;
	axiado_uart_ops->stop_rx = axiado_uart_stop_rx;
	axiado_uart_ops->start_tx = axiado_uart_start_tx;

	uart->uart_port->port.ops  = axiado_uart_ops;

	ret = devm_request_threaded_irq(&pdev->dev, uart->irq_cpu, NULL,
					axiado_uart_cpu_irq_handler, IRQF_ONESHOT,
					"axiado-espi-uart", serial8250_get_port(ret));

	if (ret) {
		dev_err(&pdev->dev, "Failed to request CPU side IRQ: %d\n",
			ret);
		goto err_port_cleanup;
	}

	ret = regmap_update_bits(uart->regmap, uart->atu_base + ATU_ISR_MASK,
				BIT(uart->isr_bit), 0);
	if (ret) {
		dev_err(&pdev->dev, "Failed to unmask ISR bit %d: %d\n",
			uart->isr_bit, ret);
		goto err_port_cleanup;
	}

	platform_set_drvdata(pdev, uart);
	uart_instances[index] = uart;

	dev_dbg(
		&pdev->dev,
		"UART driver initialized for channel %d (CPU IRQ: %d, io_addr: 0x%04x)\n",
		index, uart->irq_cpu, io_addr);

	dev_info(&pdev->dev, "8250 AXIADO UART probe successful.\n");
	return 0;

err_port_cleanup:
	regmap_update_bits(uart->regmap, uart->atu_base + ATU_ISR_MASK,
			   BIT(uart->isr_bit), BIT(uart->isr_bit));
	serial8250_unregister_port(uart->line);
err_disable_atu:
	axiado_uart_disable_atu(uart);
	return ret;
}

/**
 * axiado_uart_remove - Remove UART driver
 * @pdev: Platform device
 *
 * Return: 0 on success
 */
static int axiado_uart_remove(struct platform_device *pdev)
{
	struct axiado_espi_uart *uart = platform_get_drvdata(pdev);
	int index = uart->index;

	/* Mask CPU side interrupt */
	regmap_update_bits(uart->regmap, uart->atu_base + ATU_ISR_MASK,
			   BIT(uart->isr_bit), BIT(uart->isr_bit));

	cancel_work_sync(&uart->tx_work);
	/* Unregister from serial core */
	serial8250_unregister_port(uart->line);

	/* Disable ATU */
	axiado_uart_disable_atu(uart);

	/* Clear global array entry */
	uart_instances[index] = NULL;

	dev_dbg(&pdev->dev, "UART driver removed for channel %d\n", index);
	return 0;
}

static const struct of_device_id axiado_uart_of_match[] = {
	{
		.compatible = "axiado,ax3000-espi-uart",
	},
	{},
};
MODULE_DEVICE_TABLE(of, axiado_uart_of_match);

static struct platform_driver axiado_uart_driver = {
	.probe = axiado_uart_probe,
	.remove = axiado_uart_remove,
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = of_match_ptr(axiado_uart_of_match),
	},
};

module_platform_driver(axiado_uart_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Axiado Corporation");
MODULE_DESCRIPTION("Axiado eSPI UART Driver");
