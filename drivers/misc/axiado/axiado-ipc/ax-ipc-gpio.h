/* SPDX-License-Identifier: GPL-2.0-or-later */

/*
 * Copyright (c) 2021-25 Axiado Corporation (or its affiliates).
 */

/*
 * AXIADO IPC GPIO Driver
 */

#ifndef __AX_IPC_GPIO_H
#define __AX_IPC_GPIO_H

#define EVT_GPIO_OPS_SIG 0x00100012
#define SVM_CPU_ID 0
#define AX_DEFINE_ENDPOINT(cpu, code) \
	((((cpu) << 24) & 0xFF000000) | ((code)&0xFFFFFF))
#define AX_EP_SYSMGR_A53_PROXY AX_DEFINE_ENDPOINT(SVM_CPU_ID, 0x05)

/**
 * enum sysmc_ipc_gpio_cmds_e - IPC commands for GPIO operations
 * @SYSMC_IPC_GPIO_CMD_BYPASS_MODE: Set GPIO to bypass mode
 * @SYSMC_IPC_GPIO_CMD_GPIO_MODE: Set pin to GPIO mode
 * @SYSMC_IPC_GPIO_CMD_SET_DIR: Configure GPIO direction
 * @SYSMC_IPC_GPIO_CMD_SET_VAL: Set GPIO output value
 * @SYSMC_IPC_GPIO_CMD_GET_VAL: Get current GPIO value
 * @SYSMC_IPC_GPIO_CMD_GET_OUTPUT_VAL: Get current GPIO output register value
 */
enum sysmc_ipc_gpio_cmds_e {
	SYSMC_IPC_GPIO_CMD_BYPASS_MODE,
	SYSMC_IPC_GPIO_CMD_GPIO_MODE,
	SYSMC_IPC_GPIO_CMD_SET_DIR,
	SYSMC_IPC_GPIO_CMD_SET_VAL,
	SYSMC_IPC_GPIO_CMD_GET_VAL,
	SYSMC_IPC_GPIO_CMD_GET_OUTPUT_VAL
};

/**
 * struct sysmc_gpio_req_s - GPIO request structure for IPC communication
 * @pin_num: GPIO pin number to operate on
 * @cmd: Command to execute, using values from sysmc_ipc_gpio_cmds_e
 * @is_output: Direction setting for SET_DIR command (1:output, 0:input)
 * @value: Value to set or value read from GPIO
 */
struct sysmc_gpio_req_s {
	u8 pin_num;
	u8 cmd; /* values defined in enum sysmc_ipc_gpio_cmds_e */
	u8 is_output; /* If command is SYSMC_IPC_GPIO_CMD_SET_DIR, set 1 for output, 0 for input */
	u8 value;
};

/**
 * @brief - Send GPIO request through IPC
 * @gpio_req: Pointer to GPIO request structure
 *
 * This function sends GPIO operations to SysManager via IPC,
 * replacing direct register access for better security.
 *
 * Return: 0 on success, negative error code on failure
 */
int ax_ipc_gpio_request(struct sysmc_gpio_req_s *gpio_req);

#endif /* __AX_IPC_GPIO_H */
