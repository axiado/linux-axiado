// SPDX-License-Identifier: GPL-2.0-only
/*
 * Axiado XIIC I2C slave-only driver
 *
 * Based on i2c-xiic.c
 * Copyright (c) 2002-2007 Xilinx Inc.
 * Copyright (c) 2009-2010 Intel Corporation.
 * Copyright (c) 2024-2025 Axiado Corporation.
 *
 * This driver supports the Xilinx XIIC I2C controller
 * in slave-only mode, using regmap for register access.
 */

/* Supports:
 * Axiado integration of the Xilinx XIIC I2C core (slave-only mode)
 */

#include <linux/cpumask.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/i2c.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqdesc.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/reset.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <uapi/linux/sched/types.h>

/* Local common hardware definitions */
#include "i2c-xiic-common.h"

#define DRIVER_NAME "i2c-axiado-xiic"

static void set_irq_thread_props(int irq, int priority, int cpu)
{
	struct irq_desc *desc;
	struct task_struct *irq_thread = NULL;
	struct sched_param param;
	cpumask_t mask;
	int ret;

	desc = irq_to_desc(irq);
	if (desc)
		irq_thread = desc->action ? desc->action->thread : NULL;

	if (!irq_thread) {
		pr_warn("IRQ %d: no thread found (is it threaded?)\n", irq);
		return;
	}

	param.sched_priority = clamp(priority, 1, 99);
	ret = sched_setscheduler(irq_thread, SCHED_FIFO, &param);
	if (ret)
		pr_warn("IRQ %d: failed to set RT priority (err=%d)\n", irq, ret);
	else
		pr_info("IRQ %d: RT priority set to %d\n", irq, param.sched_priority);

	cpumask_clear(&mask);
	cpumask_set_cpu(cpu, &mask);
	ret = set_cpus_allowed_ptr(irq_thread, &mask);
	if (ret)
		pr_warn("IRQ %d: failed to set CPU affinity (err=%d)\n", irq, ret);
	else
		pr_info("IRQ %d: pinned to CPU%d\n", irq, cpu);

}

enum xilinx_i2c_slave_state {
	XLNX_I2C_SLAVE_IDLE = 0,
	XLNX_I2C_SLAVE_REPEATED_START,
	XLNX_I2C_SLAVE_SEND_REQUESTED,
	XLNX_I2C_SLAVE_SEND_PROCESSED,
	XLNX_I2C_SLAVE_RECV_REQUESTED,
	XLNX_I2C_SLAVE_RECV_RECEIVED,
	XLNX_I2C_SLAVE_SEND_DONE,
	XLNX_I2C_SLAVE_RECV_DONE,
	XLNX_I2C_SLAVE_ERROR
};

enum xiic_endian { LITTLE, BIG };

struct xiic_pkt_ctx {
	u8 isr;
	u8 sr;
	u8 ier;
	u8 rx_fifo_occ;
	u8 cr;
};

/**
 * struct xiic_i2c_slave - Internal representation of the XIIC I2C slave
 * @dev:               Pointer to device structure
 * @regmap:            Regmap for register access
 * @regmap_base_offset: Base offset for regmap access
 * @adap:              Kernel I2C adapter representation
 * @lock:              Protects access to this structure and hardware registers
 * @endianness:        Big/little-endian byte order
 * @slave:             I2C slave client handle
 * @slave_state:       Current slave state
 * @pkt_ctx:           Packet context (moved from static vars)
 * @tx_length:         Transmit message length
 * @tx_index:          Current position in transmit buffer
 * @is_backend_stop:   Indicates backend stop condition
 * @write_requested:   Indicates a write request is pending
 */
struct xiic_i2c_slave {
	struct device *dev;
	struct regmap *regmap;
	u32 regmap_base_offset;
	struct i2c_adapter adap;
	/** Protects access to this structure and hardware registers */
	struct mutex lock;
	enum xiic_endian endianness;
	struct i2c_client *slave;
	enum xilinx_i2c_slave_state slave_state;
	struct xiic_pkt_ctx pkt_ctx;
	u8 tx_length;
	u8 tx_index;
	bool is_backend_stop;
	bool write_requested;
};

/*
 * For the register read and write functions, a little-endian and big-endian
 * version are necessary. Endianness is detected during the probe function.
 * Only the least significant byte [doublet] of the register are ever
 * accessed. This requires an offset of 3 [2] from the base address for
 * big-endian systems.
 */

/* Regmap-based register access functions */
static inline u32 xiic_read_regmap(struct xiic_i2c_slave *i2c, u32 reg)
{
	u32 val = 0;
	int ret;

	ret = regmap_read(i2c->regmap, i2c->regmap_base_offset + reg, &val);
	if (ret < 0) {
		dev_err(i2c->dev, "regmap_read failed for reg 0x%x: %d\n",
			i2c->regmap_base_offset + reg, ret);
		return 0;
	}
	return val;
}

static inline void xiic_write_regmap(struct xiic_i2c_slave *i2c, u32 val, u32 reg)
{
	int ret;

	ret = regmap_write(i2c->regmap, i2c->regmap_base_offset + reg, val);
	if (ret < 0) {
		dev_err(i2c->dev,
			"regmap_write failed for reg 0x%x = 0x%x: %d\n",
			i2c->regmap_base_offset + reg, val, ret);
	}
}

static inline void xiic_setreg8(struct xiic_i2c_slave *i2c, int reg, u8 value)
{
	if (i2c->endianness == LITTLE)
		xiic_write_regmap(i2c, value, reg);
	else
		xiic_write_regmap(i2c, value, reg + 3);
}

static inline u8 xiic_getreg8(struct xiic_i2c_slave *i2c, int reg)
{
	u8 ret;

	if (i2c->endianness == LITTLE)
		ret = (u8)xiic_read_regmap(i2c, reg);
	else
		ret = (u8)xiic_read_regmap(i2c, reg + 3);
	return ret;
}

static inline void xiic_setreg16(struct xiic_i2c_slave *i2c, int reg, u16 value)
{
	if (i2c->endianness == LITTLE)
		xiic_write_regmap(i2c, value, reg);
	else
		xiic_write_regmap(i2c, cpu_to_be16(value), reg + 2);
}

static inline void xiic_setreg32(struct xiic_i2c_slave *i2c, int reg, int value)
{
	if (i2c->endianness == LITTLE)
		xiic_write_regmap(i2c, value, reg);
	else
		xiic_write_regmap(i2c, cpu_to_be32(value), reg);
}

static inline u32 xiic_getreg32(struct xiic_i2c_slave *i2c, int reg)
{
	u32 ret;

	if (i2c->endianness == LITTLE)
		ret = xiic_read_regmap(i2c, reg);
	else
		ret = be32_to_cpu(xiic_read_regmap(i2c, reg));
	return ret;
}

/* Optimized IRQ functions - direct register access */
static inline void xiic_irq_dis(struct xiic_i2c_slave *i2c, u32 mask)
{
	u32 ier = xiic_read_regmap(i2c, XIIC_IIER_OFFSET);

	xiic_write_regmap(i2c, ier & ~mask, XIIC_IIER_OFFSET);
}

static inline void xiic_irq_en(struct xiic_i2c_slave *i2c, u32 mask)
{
	u32 ier = xiic_read_regmap(i2c, XIIC_IIER_OFFSET);

	xiic_write_regmap(i2c, ier | mask, XIIC_IIER_OFFSET);
}

static inline void xiic_irq_clr(struct xiic_i2c_slave *i2c, u32 mask)
{
	u32 isr = xiic_getreg32(i2c, XIIC_IISR_OFFSET);

	xiic_setreg32(i2c, XIIC_IISR_OFFSET, isr & mask);
}

static inline void xiic_irq_clr_en(struct xiic_i2c_slave *i2c, u32 mask)
{
	xiic_irq_clr(i2c, mask);
	xiic_irq_en(i2c, mask);
}

/* Optimized CR functions - direct register access */
static inline void xiic_cr_en(struct xiic_i2c_slave *i2c, u32 mask)
{
	u32 cr = xiic_read_regmap(i2c, XIIC_CR_REG_OFFSET);

	xiic_write_regmap(i2c, cr | mask, XIIC_CR_REG_OFFSET);
}

static inline void xiic_cr_dis(struct xiic_i2c_slave *i2c, u32 mask)
{
	u32 cr = xiic_read_regmap(i2c, XIIC_CR_REG_OFFSET);

	xiic_write_regmap(i2c, cr & ~mask, XIIC_CR_REG_OFFSET);
}

static int xiic_clear_rx_fifo(struct xiic_i2c_slave *i2c)
{
	u8 sr;
	unsigned long timeout;

	timeout = jiffies + XIIC_I2C_TIMEOUT;
	for (sr = xiic_getreg8(i2c, XIIC_SR_REG_OFFSET);
	     !(sr & XIIC_SR_RX_FIFO_EMPTY_MASK);
	     sr = xiic_getreg8(i2c, XIIC_SR_REG_OFFSET)) {
		xiic_getreg8(i2c, XIIC_DRR_REG_OFFSET);
		if (time_after(jiffies, timeout)) {
			dev_err(i2c->dev, "Failed to clear rx fifo\n");
			return -ETIMEDOUT;
		}
	}

	return 0;
}

static int xiic_slave_reinit(struct xiic_i2c_slave *i2c)
{
	int ret;
	u32 tmp;
	u16 slave_addr;

	if (!i2c->slave) {
		dev_err(i2c->dev,
			"No slave client registered, cannot reinit\n");
		return -ENODEV;
	}

	slave_addr = i2c->slave->addr; /* dynamic from i2c_client */

	/* Reset the IIC device */
	xiic_setreg32(i2c, XIIC_RESETR_OFFSET, XIIC_RESET_MASK);

	/* Clear NAAS/AAS if set */
	if (xiic_getreg32(i2c, XIIC_IISR_OFFSET) & XIIC_INTR_NAAS_MASK)
		xiic_irq_clr(i2c, XIIC_INTR_NAAS_MASK);

	if (xiic_getreg32(i2c, XIIC_IISR_OFFSET) & XIIC_INTR_AAS_MASK)
		xiic_irq_clr(i2c, XIIC_INTR_AAS_MASK);

	/* Enable interrupts */
	xiic_setreg32(i2c, XIIC_DGIER_OFFSET, XIIC_GINTR_ENABLE_MASK);

	/* Enable AAS, RX full, TX empty, TX error */
	xiic_setreg32(i2c, XIIC_IIER_OFFSET,
		      XIIC_INTR_AAS_MASK | XIIC_INTR_RX_FULL_MASK |
			      XIIC_INTR_TX_EMPTY_MASK |
			      XIIC_INTR_TX_ERROR_MASK);

	tmp = xiic_getreg32(i2c, XIIC_IIER_OFFSET);
	dev_info(i2c->dev, "XIIC_IIER = 0x%x\n", tmp);

	/* Program the slave address (shifted for LSB=R/W bit) */
	xiic_setreg32(i2c, XIIC_ADR_REG_OFFSET, (slave_addr << 1));

	tmp = xiic_getreg32(i2c, XIIC_ADR_REG_OFFSET);
	if (tmp != (slave_addr << 1)) {
		dev_err(i2c->dev,
			"Failed to set XIIC slave addr, reg=0x%x expected=0x%x\n",
			tmp, slave_addr << 1);
		return -EINVAL;
	}

	/* RX FIFO depth = maximum (0x0 means full depth) */
	xiic_setreg32(i2c, XIIC_RFD_REG_OFFSET, 0x0);

	/* Ensure RX FIFO is empty */
	ret = xiic_clear_rx_fifo(i2c);
	if (ret) {
		dev_err(i2c->dev, "RX FIFO not empty during reinit\n");
		return ret;
	}

	/* Enable IIC device */
	xiic_setreg32(i2c, XIIC_CR_REG_OFFSET, XIIC_CR_ENABLE_DEVICE_MASK);

	tmp = xiic_getreg32(i2c, XIIC_CR_REG_OFFSET);
	if (tmp != XIIC_CR_ENABLE_DEVICE_MASK) {
		dev_err(i2c->dev, "XIIC_CR_REG not set correctly (0x%x)\n",
			tmp);
		return -EINVAL;
	}

	dev_info(i2c->dev, "XIIC slave reinitialized at addr 0x%02x\n",
		 slave_addr);

	return 0;
}

static void xiic_deinit(struct xiic_i2c_slave *i2c)
{
	u8 cr;

	xiic_setreg32(i2c, XIIC_RESETR_OFFSET, XIIC_RESET_MASK);

	/* Disable IIC Device. */
	cr = xiic_getreg8(i2c, XIIC_CR_REG_OFFSET);
	xiic_setreg8(i2c, XIIC_CR_REG_OFFSET, cr & ~XIIC_CR_ENABLE_DEVICE_MASK);
}

static irqreturn_t xiic_slave_process(int irq, void *dev_id)
{
	struct xiic_i2c_slave *i2c = dev_id;
	u32 pending, isr, ier;
	u32 clr = 0;
	u32 cr = 0;
	u32 sr = 0;
	u32 ret = 0;
	u8 tx_byte = 0, rx_byte = 0x20;
	static bool ssif_bmc_send_enabled = true;

	if (!i2c->slave || !i2c->slave->slave_cb) {
		dev_dbg(i2c->adap.dev.parent,
			"i2c->slave == NULL or i2c->slave->slave_cb == NULL\n");
		return IRQ_HANDLED;
	}

	mutex_lock(&i2c->lock);

	/* MFD doesn't support burst read yet. Let's use normal read */
	isr = xiic_getreg32(i2c, XIIC_IISR_OFFSET);
	i2c->pkt_ctx.isr = isr;
	sr = xiic_getreg8(i2c, XIIC_SR_REG_OFFSET);
	i2c->pkt_ctx.sr = sr;
	ier = xiic_getreg32(i2c, XIIC_IIER_OFFSET);
	i2c->pkt_ctx.ier = ier;
	cr = xiic_getreg8(i2c, XIIC_CR_REG_OFFSET);
	pending = i2c->pkt_ctx.isr & i2c->pkt_ctx.ier;

	dev_dbg(i2c->adap.dev.parent,
		"--> ISR: 0x%x, IER: 0x%x, pend: 0x%x, sr = %x i2c slave state = %d, cr = %x\n",
		isr, ier, pending, sr, i2c->slave_state, cr);
	switch (i2c->slave_state) {
	case XLNX_I2C_SLAVE_IDLE:
	case XLNX_I2C_SLAVE_REPEATED_START:
		if (pending & XIIC_INTR_AAS_MASK) {
			xiic_irq_clr_en(i2c, XIIC_INTR_NAAS_MASK);
			clr |= XIIC_INTR_AAS_MASK;
			xiic_irq_clr(i2c, clr);
			i2c->is_backend_stop = false;
			if (sr & XIIC_SR_MSTR_RDING_SLAVE_MASK) {
				ret = i2c_slave_event(i2c->slave,
						      I2C_SLAVE_READ_REQUESTED,
						      &tx_byte);
				xiic_setreg8(i2c, XIIC_DTR_REG_OFFSET, tx_byte);
				xiic_irq_en(i2c, XIIC_INTR_TX_EMPTY_MASK);
				i2c->slave_state =
					XLNX_I2C_SLAVE_SEND_REQUESTED;
				i2c->tx_length = tx_byte;
				i2c->tx_index = 0;
			} else {
				/* Make sure the last transaction is done.*/
				if (i2c->write_requested && ssif_bmc_send_enabled) {
					i2c_slave_event(i2c->slave,
							I2C_SLAVE_STOP, NULL);
					i2c->write_requested = false;
				}
				ret = i2c_slave_event(i2c->slave,
						      I2C_SLAVE_WRITE_REQUESTED,
						      &rx_byte);
				i2c->write_requested = true;
				/* If backend is busy, enable NACK for next op.
				 * Usually, it will be read requested.
				 */
				if (ret) {
					ssif_bmc_send_enabled = false;
					xiic_cr_en(i2c,
						   XIIC_CR_TX_FIFO_RESET_MASK |
							   XIIC_CR_NO_ACK_MASK);
				} else {
					ssif_bmc_send_enabled = true;
					xiic_cr_dis(i2c,
						    XIIC_CR_TX_FIFO_RESET_MASK |
							XIIC_CR_NO_ACK_MASK);
				}
				i2c->slave_state =
					XLNX_I2C_SLAVE_RECV_REQUESTED;
				xiic_irq_en(i2c, XIIC_INTR_RX_FULL_MASK);
			}
		} else if(pending & XIIC_INTR_RX_FULL_MASK) {
			/* Drain all RX data until FIFO becomes empty */
			while (!(xiic_getreg8(i2c, XIIC_SR_REG_OFFSET) &
				XIIC_SR_RX_FIFO_EMPTY_MASK)) {
				xiic_getreg8(i2c, XIIC_DRR_REG_OFFSET);
			}

			/* Clear RX_FULL interrupt after draining */
			xiic_irq_clr(i2c, XIIC_INTR_RX_FULL_MASK);

		} else if(pending & XIIC_INTR_TX_EMPTY_MASK){
			/* Clear and disable the TX_EMPTY interrupt */
			xiic_irq_clr(i2c, XIIC_INTR_TX_EMPTY_MASK);
			xiic_irq_dis(i2c, XIIC_INTR_TX_EMPTY_MASK);

			/* Reset TX FIFO to remove the empty flag */
			xiic_cr_en(i2c, XIIC_CR_TX_FIFO_RESET_MASK);
		}
		break;

	case XLNX_I2C_SLAVE_RECV_REQUESTED:
	case XLNX_I2C_SLAVE_RECV_RECEIVED:
		if (pending & XIIC_INTR_RX_FULL_MASK) {
			clr |= XIIC_INTR_RX_FULL_MASK;
			while (!(xiic_getreg8(i2c, XIIC_SR_REG_OFFSET) &
				 XIIC_SR_RX_FIFO_EMPTY_MASK) &&
			       (!(xiic_getreg8(i2c, XIIC_IISR_OFFSET) &
				  XIIC_INTR_NAAS_MASK))) {
				u8 val = xiic_getreg8(i2c, XIIC_DRR_REG_OFFSET);
				if (ssif_bmc_send_enabled)
					i2c_slave_event(i2c->slave,
						I2C_SLAVE_WRITE_RECEIVED, &val);
			}
			i2c->slave_state = XLNX_I2C_SLAVE_RECV_RECEIVED;
		} else if(pending & XIIC_INTR_TX_EMPTY_MASK){
			clr |= XIIC_INTR_TX_EMPTY_MASK;
			i2c->slave_state = XLNX_I2C_SLAVE_IDLE;
		}
		break;
	case XLNX_I2C_SLAVE_SEND_REQUESTED:
	case XLNX_I2C_SLAVE_SEND_PROCESSED:
		if (pending & XIIC_INTR_TX_EMPTY_MASK) {
			clr |= XIIC_INTR_TX_EMPTY_MASK;
			i2c->slave_state = XLNX_I2C_SLAVE_SEND_PROCESSED;
			/* In MGX, TX_EMPTY is not triggered fast enough.
			 * We need to send byte-by-byte.
			 */
			if ((xiic_getreg8(i2c, XIIC_SR_REG_OFFSET) &
			     XIIC_SR_TX_FIFO_EMPTY_MASK) &&
			    i2c->tx_index < i2c->tx_length) {
				u8 val = 1;

				i2c_slave_event(i2c->slave,
						I2C_SLAVE_READ_PROCESSED, &val);
				if (i2c->tx_index + 1 == i2c->tx_length) {
					xiic_setreg8(i2c, XIIC_DTR_REG_OFFSET,
						     val);
					i2c->slave_state =
						XLNX_I2C_SLAVE_SEND_DONE;
					i2c->tx_index++;
					break;
				}
				xiic_setreg8(i2c, XIIC_DTR_REG_OFFSET, val);
				i2c->tx_index++;
			}
		} else if(pending & XIIC_INTR_RX_FULL_MASK) {
			/* Drain all RX data until FIFO becomes empty */
			while (!(xiic_getreg8(i2c, XIIC_SR_REG_OFFSET) &
				XIIC_SR_RX_FIFO_EMPTY_MASK)) {
				xiic_getreg8(i2c, XIIC_DRR_REG_OFFSET);
			}
			/* Clear RX_FULL interrupt after draining */
			xiic_irq_clr(i2c, XIIC_INTR_RX_FULL_MASK);
		}
		break;
	case XLNX_I2C_SLAVE_SEND_DONE:
		/* PEC byte */
		clr |= XIIC_INTR_AAS_MASK;
		if (!(pending & XIIC_INTR_TX_ERROR_MASK) &&
		    ((pending & XIIC_INTR_TX_EMPTY_MASK)) &&
		    i2c->tx_index == i2c->tx_length) {
			u8 val = 0x1;

			i2c_slave_event(i2c->slave, I2C_SLAVE_READ_PROCESSED,
					&val);
			clr |= XIIC_INTR_TX_EMPTY_MASK;
			xiic_setreg8(i2c, XIIC_DTR_REG_OFFSET, val);
		}
		break;
	/* For critical Error, we reinit the i2c slave */
	case XLNX_I2C_SLAVE_ERROR:
		dev_warn(i2c->dev,
			 "TX_ERROR in unexpected state=%d, forcing STOP\n",
			 i2c->slave_state);
		xiic_slave_reinit(i2c);
		i2c->is_backend_stop = true;
		i2c->write_requested = false;
		i2c_slave_event(i2c->slave, I2C_SLAVE_STOP, NULL);
		i2c->slave_state = XLNX_I2C_SLAVE_IDLE;
		clr |= pending;
	default:
		break;
	}
	/* Process NAAS interrupt. It means the repeat start or end of receive data. */
	if (pending & XIIC_INTR_NAAS_MASK) {
		xiic_irq_dis(i2c, XIIC_INTR_NAAS_MASK);
		xiic_cr_dis(i2c,
			    XIIC_CR_TX_FIFO_RESET_MASK | XIIC_CR_NO_ACK_MASK);
		u8 sr = xiic_getreg8(i2c, XIIC_SR_REG_OFFSET);
		/* If Bus is not busy, usually it means stop condition. */
		if (!(sr & XIIC_SR_BUS_BUSY_MASK) &&
		    !(pending & XIIC_INTR_TX_EMPTY_MASK)) {
			/* STOP condition */
			clr |= XIIC_INTR_NAAS_MASK | XIIC_INTR_AAS_MASK |
			       XIIC_INTR_TX_ERROR_MASK;
			if (ssif_bmc_send_enabled)
				i2c_slave_event(i2c->slave, I2C_SLAVE_STOP, NULL);
			i2c->is_backend_stop = true;
			i2c->write_requested = false;
			i2c->slave_state = XLNX_I2C_SLAVE_IDLE;
			i2c->tx_index = 0;
			i2c->tx_length = 0;
		} else {
			clr |= XIIC_INTR_NAAS_MASK;
			if (i2c->slave_state == XLNX_I2C_SLAVE_RECV_RECEIVED &&
			    (pending & XIIC_INTR_RX_FULL_MASK)) {
				/* For Write -> block read (Write->Read) */
				if (ssif_bmc_send_enabled)
					i2c_slave_event(i2c->slave, I2C_SLAVE_STOP,
						NULL);
				i2c->slave_state = XLNX_I2C_SLAVE_IDLE;
				i2c->is_backend_stop = true;
				i2c->write_requested = false;
				i2c->tx_index = 0;
				i2c->tx_length = 0;
			} else if ((pending & XIIC_INTR_TX_ERROR_MASK)) {
				/* If backend is busy, we will NACK the read address.
				 * It leads to here — consider it as stop.
				 */
				clr |= XIIC_INTR_TX_ERROR_MASK |
				       XIIC_INTR_TX_EMPTY_MASK |
				       XIIC_INTR_AAS_MASK;
				if (ssif_bmc_send_enabled)
					i2c_slave_event(i2c->slave, I2C_SLAVE_STOP,
						NULL);
				i2c->is_backend_stop = true;
				i2c->write_requested = false;
				i2c->slave_state = XLNX_I2C_SLAVE_IDLE;
				i2c->tx_index = 0;
				i2c->tx_length = 0;
			} else if (i2c->slave_state ==
					   XLNX_I2C_SLAVE_RECV_RECEIVED &&
				   !(pending & XIIC_INTR_RX_FULL_MASK) &&
				   !(pending & XIIC_INTR_TX_EMPTY_MASK)) {
				/* Get NAAS and no pending TX & RX */
				if (ssif_bmc_send_enabled)
					i2c_slave_event(i2c->slave, I2C_SLAVE_STOP,
						NULL);
				i2c->slave_state = XLNX_I2C_SLAVE_IDLE;
				i2c->is_backend_stop = true;
				i2c->write_requested = false;
				i2c->tx_index = 0;
				i2c->tx_length = 0;
			} else {
				/* Repeated START — do NOT send STOP */
				i2c->slave_state =
					XLNX_I2C_SLAVE_REPEATED_START;
			}
		}
	}

	if (pending & XIIC_INTR_TX_ERROR_MASK &&
	    i2c->slave_state == XLNX_I2C_SLAVE_SEND_DONE) {
		clr |= XIIC_INTR_TX_ERROR_MASK | XIIC_INTR_AAS_MASK;
		xiic_cr_dis(i2c,
			    XIIC_CR_TX_FIFO_RESET_MASK | XIIC_CR_NO_ACK_MASK);
		if (i2c->slave_state == XLNX_I2C_SLAVE_SEND_PROCESSED ||
		    i2c->slave_state == XLNX_I2C_SLAVE_SEND_REQUESTED ||
		    i2c->slave_state == XLNX_I2C_SLAVE_SEND_DONE) {
			if (ssif_bmc_send_enabled)
				i2c_slave_event(i2c->slave, I2C_SLAVE_STOP, NULL);
			i2c->is_backend_stop = true;
			i2c->write_requested = false;
			i2c->slave_state = XLNX_I2C_SLAVE_IDLE;
		} else {
			/* Unexpected: RX path shouldn't trigger TX_ERROR.
			 * If we really get here, let's reset the slave.
			 */
			dev_warn(i2c->dev,
				 "TX_ERROR in unexpected state=%d, forcing STOP\n",
				 i2c->slave_state);
			xiic_slave_reinit(i2c);
			i2c->is_backend_stop = true;
			i2c->write_requested = false;
			i2c->slave_state = XLNX_I2C_SLAVE_IDLE;
			clr |= pending;
			goto out;
		}
	}
	if (pending == 0x0) {
		if (!i2c->is_backend_stop) {
			dev_warn(i2c->dev,
				 "backend is not stopped, but no pending interrupts\n");
			i2c->slave_state = XLNX_I2C_SLAVE_IDLE;
			i2c->write_requested = false;
			if (ssif_bmc_send_enabled)
				i2c_slave_event(i2c->slave, I2C_SLAVE_STOP, NULL);
		}
	}
out:
	xiic_irq_clr(i2c, clr);
	mutex_unlock(&i2c->lock);
	return IRQ_HANDLED;
}

static int xiic_reg_slave(struct i2c_client *slave)
{
	struct xiic_i2c_slave *i2c = i2c_get_adapdata(slave->adapter);
	int ret;

	if (!i2c)
		return -ENODEV;

	mutex_lock(&i2c->lock);

	if (i2c->slave) {
		dev_err(i2c->dev, "A slave is already registered at 0x%x\n",
			i2c->slave->addr);
		mutex_unlock(&i2c->lock);
		return -EBUSY;
	}

	/* Save the new slave client */
	i2c->slave = slave;

	dev_info(i2c->dev, "Registering slave at address 0x%x\n", slave->addr);

	/* Reconfigure hardware with this address */
	ret = xiic_slave_reinit(i2c);
	if (ret < 0) {
		dev_err(i2c->dev, "Failed to reinit with slave addr 0x%x\n",
			slave->addr);
		i2c->slave = NULL;
		mutex_unlock(&i2c->lock);
		return ret;
	}

	mutex_unlock(&i2c->lock);

	return 0;
}

static int xiic_unreg_slave(struct i2c_client *slave)
{
	struct xiic_i2c_slave *i2c = i2c_get_adapdata(slave->adapter);

	if (!i2c)
		return -ENODEV;

	mutex_lock(&i2c->lock);

	if (i2c->slave != slave) {
		mutex_unlock(&i2c->lock);
		return -EINVAL;
	}

	dev_info(i2c->dev, "Unregistering slave at address 0x%x\n",
		 slave->addr);

	/* Reset hardware back to a safe state */
	xiic_deinit(i2c);

	/* Drop reference to the slave */
	i2c->slave = NULL;

	mutex_unlock(&i2c->lock);

	return 0;
}

static u32 xiic_func(struct i2c_adapter *adap)
{
	return I2C_FUNC_SLAVE;
}

static const struct i2c_algorithm xiic_algorithm = {
	.functionality = xiic_func,
	.reg_slave = xiic_reg_slave,
	.unreg_slave = xiic_unreg_slave,
};

#if defined(CONFIG_OF)
static const struct of_device_id axiado_xiic_of_match[] = {
	{ .compatible = "axiado,xps-iic-slave" },
	{},
};

MODULE_DEVICE_TABLE(of, axiado_xiic_of_match);
#endif

static int xiic_i2c_slave_probe(struct platform_device *pdev)
{
	struct xiic_i2c_slave *i2c;
	int ret, irq;
	u32 sr;
	u32 reg_base_offset = 0;
	u32 rt_prio, cpu;

	dev_info(&pdev->dev, "I2C probe start\n");

	i2c = devm_kzalloc(&pdev->dev, sizeof(*i2c), GFP_KERNEL);
	if (!i2c)
		return -ENOMEM;

	i2c->dev = &pdev->dev;
	platform_set_drvdata(pdev, i2c);

	/* Try regmap from parent */
	i2c->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!i2c->regmap) {
		dev_err(&pdev->dev, "Parent regmap not found\n");
		return -ENODEV;
	}
	/* Read base offset from device tree */
	ret = of_property_read_u32(pdev->dev.of_node, "reg",
				   &reg_base_offset);
	if (ret) {
		dev_err(&pdev->dev, "Failed to read reg property: %d\n",
			ret);
		return ret;
	}
	i2c->regmap_base_offset = reg_base_offset;
	dev_info(&pdev->dev, "Using regmap with base offset: 0x%x\n",
		 reg_base_offset);

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;

	dev_info(i2c->dev, "Map IRQ no : %d\n", irq);

	/* Prepare adapter */
	i2c->adap.owner = THIS_MODULE;
	i2c->adap.algo = &xiic_algorithm;
	i2c->adap.dev.parent = &pdev->dev;
	i2c->adap.dev.of_node = pdev->dev.of_node;
	snprintf(i2c->adap.name, sizeof(i2c->adap.name), DRIVER_NAME);

	i2c_set_adapdata(&i2c->adap, i2c);

	mutex_init(&i2c->lock);

	i2c->tx_length = 0;
	i2c->tx_index = 0;
	i2c->is_backend_stop = true;
	i2c->write_requested = false;

	/* Detect endianness */
	i2c->endianness = LITTLE;
	sr = xiic_getreg32(i2c, XIIC_SR_REG_OFFSET);
	if (!(sr & XIIC_SR_TX_FIFO_EMPTY_MASK))
		i2c->endianness = BIG;

	/* Add adapter */
	ret = i2c_add_adapter(&i2c->adap);
	if (ret) {
		xiic_deinit(i2c);
		mutex_destroy(&i2c->lock);
		return ret;
	}

	/* Request threaded IRQ */
	ret = devm_request_threaded_irq(&pdev->dev, irq, NULL,
					xiic_slave_process, IRQF_ONESHOT,
					"xiic_i2c_slave", i2c);
	if (ret < 0) {
		dev_err(i2c->dev, "Cannot claim IRQ\n");
		i2c_del_adapter(&i2c->adap);
		xiic_deinit(i2c);
		mutex_destroy(&i2c->lock);
		return ret;
	}

	/* Defaults if DT props missing */
	rt_prio = 0;          /* 0 => don't change priority */
	cpu = UINT_MAX;       /* UINT_MAX => don't pin */

	of_property_read_u32(pdev->dev.of_node, "axiado,irq-rt-priority", &rt_prio);
	of_property_read_u32(pdev->dev.of_node, "axiado,irq-cpu", &cpu);

	if (rt_prio || cpu != UINT_MAX)
		set_irq_thread_props(irq, rt_prio ? rt_prio : 60,
				     cpu != UINT_MAX ? cpu : 0);

	dev_info(i2c->dev, "I2C probe completed\n");
	return 0;
}

static int xiic_i2c_slave_remove(struct platform_device *pdev)
{
	struct xiic_i2c_slave *i2c = platform_get_drvdata(pdev);

	if (!i2c)
		return 0;

	if (i2c->slave) {
		dev_info(i2c->dev, "unregistering device\n");
		i2c_unregister_device(i2c->slave);
		i2c->slave = NULL; /* Clear the pointer */
	}
	xiic_deinit(i2c);

	i2c_del_adapter(&i2c->adap);

	mutex_destroy(&i2c->lock);

	dev_info(i2c->dev, "I2C driver removed\n");
	return 0;
}

static struct platform_driver axiado_xiic_i2c_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = axiado_xiic_of_match,
	},
	.probe = xiic_i2c_slave_probe,
	.remove = xiic_i2c_slave_remove,
};

module_platform_driver(axiado_xiic_i2c_driver);

MODULE_DESCRIPTION("Axiado I2C slave-only driver based on Xilinx XIIC core");
MODULE_AUTHOR("AXIADO CORPORATION");
MODULE_LICENSE("GPL");

