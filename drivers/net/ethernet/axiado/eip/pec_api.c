// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022-2026 Axiado Corporation.
 */

#include "api_dmabuf.h"
#include "api_pec.h"
#include "api_pec_sg.h"
#include "log.h"
#include "clib.h"

#include "eip.h"
MODULE_LICENSE("Dual BSD/GPL");

/**
 * @brief eip_pec_init - init pec apis
 *
 * @return int - 0 on success and negative on failure
 */
int eip_pec_init(u8 rid)
{
	pec_status_t status = PEC_STATUS_BUSY;
	pec_init_block_t init_block = { 0, 0, true };
	unsigned int loop_counter = DA_PEC_PKT_GET_RETRY_COUNT;
	int ret = -1;

	/* worry about r-3 later */
	if (rid == 0)
		init_block.f_continuous_scatter = false;

	LOG_INFO("EIP pec_init Interface ID %d, continuous_scatter:%d\n",
		 rid, init_block.f_continuous_scatter);

	LOG_INFO("EIP CONFIG: Initializing PEC for interface ID %d\n", rid);

	while (loop_counter > 0 && status == PEC_STATUS_BUSY) {
		status = pec_init(rid, &init_block);
		// Wait for DA_PEC_PKT_GET_TIMEOUT_MS milliseconds
		da_pec_usleep(DA_PEC_PKT_GET_TIMEOUT_MS * 1000);
		loop_counter--;
	} /* while */

	if (loop_counter <= 0) {
		LOG_INFO("EIP_Config: PEC could not be initialized, timeout\n");
	} else if (status != PEC_STATUS_OK) {
		LOG_INFO("DA_PEC: PEC could not be initialized, error=%d\n",
			 status);
	} else {
		ret = 0;
	}

	return ret;
}

/**
 * @brief eip_pec_uninit - uninit the pec apis
 *
 */
void eip_pec_uninit(int rid)
{
	unsigned int loop_counter = DA_PEC_PKT_GET_RETRY_COUNT;
	pec_status_t status = PEC_STATUS_BUSY;

	while (loop_counter > 0 && status == PEC_STATUS_BUSY) {
		status = pec_uninit(rid);
		// Wait for DA_PEC_PKT_GET_TIMEOUT_MS milliseconds
		da_pec_usleep(DA_PEC_PKT_GET_TIMEOUT_MS * 1000);
		loop_counter--;
	} // while

	// Check for timeout
	if (loop_counter <= 0)
		LOG_INFO("EIP_Config: PEC could not be un-initialized, timeout\n");
	else if (status != PEC_STATUS_OK)
		LOG_INFO("EIP_Config: PEC could not be un-initialized, error=%d\n",
			 status);
	else
		LOG_INFO("pec unloaded\n");
}
