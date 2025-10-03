/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef _I2C_CADENCE_H
#define _I2C_CADENCE_H

#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/completion.h>
#include <linux/clk.h>
#include <linux/reset.h>
#include <linux/pinctrl/consumer.h>
#if IS_ENABLED(CONFIG_I2C_CADENCE_REGMAP)
#include <linux/regmap.h>
#endif

#if IS_ENABLED(CONFIG_I2C_SLAVE)
/**
 * enum cdns_i2c_mode - I2C Controller current operating mode
 *
 * @CDNS_I2C_MODE_SLAVE:       I2C controller operating in slave mode
 * @CDNS_I2C_MODE_MASTER:      I2C Controller operating in master mode
 */
enum cdns_i2c_mode {
	CDNS_I2C_MODE_SLAVE,
	CDNS_I2C_MODE_MASTER,
};

/**
 * enum cdns_i2c_slave_state - Slave state when I2C is operating in slave mode
 *
 * @CDNS_I2C_SLAVE_STATE_IDLE: I2C slave idle
 * @CDNS_I2C_SLAVE_STATE_SEND: I2C slave sending data to master
 * @CDNS_I2C_SLAVE_STATE_RECV: I2C slave receiving data from master
 */
enum cdns_i2c_slave_state {
	CDNS_I2C_SLAVE_STATE_IDLE,
	CDNS_I2C_SLAVE_STATE_SEND,
	CDNS_I2C_SLAVE_STATE_RECV,
};
#endif

/**
 * struct cdns_i2c - I2C device private data structure
 *
 * @dev:		Pointer to device structure
 * @membase:		Base address of the I2C device
 * @adap:		I2C adapter instance
 * @p_msg:		Message pointer
 * @err_status:		Error status in Interrupt Status Register
 * @xfer_done:		Transfer complete status
 * @p_send_buf:		Pointer to transmit buffer
 * @p_recv_buf:		Pointer to receive buffer
 * @send_count:		Number of bytes still expected to send
 * @recv_count:		Number of bytes still expected to receive
 * @curr_recv_count:	Number of bytes to be received in current transfer
 * @input_clk:		Input clock to I2C controller
 * @i2c_clk:		Maximum I2C clock speed
 * @bus_hold_flag:	Flag used in repeated start for clearing HOLD bit
 * @clk:		Pointer to struct clk
 * @clk_rate_change_nb:	Notifier block for clock rate changes
 * @reset:		Reset control for the device
 * @quirks:		flag for broken hold bit usage in r1p10
 * @ctrl_reg:		Cached value of the control register.
 * @rinfo:		I2C GPIO recovery information
 * @ctrl_reg_diva_divb: value of fields DIV_A and DIV_B from CR register
 * @slave:		Registered slave instance.
 * @dev_mode:		I2C operating role(master/slave).
 * @slave_state:	I2C Slave state(idle/read/write).
 * @fifo_depth:		The depth of the transfer FIFO
 * @transfer_size:	The maximum number of bytes in one transfer
 */
struct cdns_i2c {
	struct device		*dev;
	void __iomem *membase;
#if IS_ENABLED(CONFIG_I2C_CADENCE_REGMAP)
	struct regmap *regmap;
	u32 regmap_base_offset;
#endif
	struct i2c_adapter adap;
	struct i2c_msg *p_msg;
	int err_status;
	struct completion xfer_done;
	unsigned char *p_send_buf;
	unsigned char *p_recv_buf;
	unsigned int send_count;
	unsigned int recv_count;
	unsigned int curr_recv_count;
	unsigned long input_clk;
	unsigned int i2c_clk;
	unsigned int bus_hold_flag;
	struct clk *clk;
	struct notifier_block clk_rate_change_nb;
	struct reset_control *reset;
	u32 quirks;
	u32 ctrl_reg;
	struct i2c_bus_recovery_info rinfo;
#if IS_ENABLED(CONFIG_I2C_SLAVE)
	u16 ctrl_reg_diva_divb;
	struct i2c_client *slave;
	enum cdns_i2c_mode dev_mode;
	enum cdns_i2c_slave_state slave_state;
#endif
	u32 fifo_depth;
	unsigned int transfer_size;
};

/* Shared functions exported from i2c-cadence.c */
int cdns_i2c_probe_common(struct platform_device *pdev, struct cdns_i2c *id);
void cdns_i2c_remove_common(struct cdns_i2c *id);

#endif /* _I2C_CADENCE_H */
