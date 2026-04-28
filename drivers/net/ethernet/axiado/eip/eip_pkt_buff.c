// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022-2026 Axiado Corporation.
 */

#include "api_dmabuf.h"
#include "eip.h"
#include "log.h"
#include "da_pec_internal.h"

/**
 * @brief eip_create_tx_buffers - creates tx packet buffers
 *
 * @param eip_ring eip ring interface structure
 * @return int returns 0 on success and negative value on failure
 */
int eip_create_tx_buffers(struct eip_ring_interface *eip_ring)
{
	u32 i;
	int err;

	eip_ring->tx_buff = kcalloc(EIP_MAX_TX_PKT_BUFFERS,
				    sizeof(struct eip_pkt_buff), GFP_ATOMIC);
	if (!eip_ring->tx_buff) {
		err = -ENOMEM;
		goto err_alloc_tx_buffers;
	}

	eip_ring->tx_nxt_to_use = 0;
	eip_ring->tx_nxt_to_clean = 0;

	for (i = 0; i < EIP_MAX_TX_PKT_BUFFERS; i++) {
		dma_buf_status_t dma_status;
		dma_buf_properties_t dma_properties = { 0, 0, 0, 0 };

		dma_properties.f_cached = true;
		dma_properties.alignment = DA_PEC_DMA_ALIGNMENT_BYTE_COUNT;
		dma_properties.bank = DA_PEC_BANK_PACKET;
		dma_properties.size = EIP_TX_BUF_SIZE;

		dma_status = dma_buf_alloc(dma_properties,
					   &eip_ring->tx_buff[i].host_address,
					   &eip_ring->tx_buff[i].handle);
		if (dma_status != DMABUF_STATUS_OK) {
			err = -ENOMEM;
			goto err_dmabuff_alloc;
		}
	}

	LOG_INFO("tx pkt handles created\n");
	return 0;

err_dmabuff_alloc:
	eip_delete_tx_buffers(eip_ring);
err_alloc_tx_buffers:
	return err;
}

/**
 * @brief eip_delete_tx_buffers - delete tx pkt buffers
 *
 * @param priv eip private structure
 */
void eip_delete_tx_buffers(struct eip_ring_interface *eip_ring)
{
	u32 i;

	if (eip_ring->tx_buff) {
		for (i = 0; i < EIP_MAX_TX_PKT_BUFFERS; i++)
			if (eip_ring->tx_buff[i].handle.p) {
				dma_buf_release(eip_ring->tx_buff[i].handle);
				eip_ring->tx_buff[i].handle.p = NULL;
			}
		kfree(eip_ring->tx_buff);
		eip_ring->tx_buff = NULL;
	}

	LOG_INFO("eip tx buffers deleted for rid: %u\n", eip_ring->rid);
}

/**
 * @brief eip_create_rx_buffer - creates the rx_buffer in rx_buffer ring
 *
 * @param eip_ring eip ring interface structure
 * @return int
 */
int eip_create_rx_buffer(struct eip_ring_interface *eip_ring, int nxt_to_use)
{
	u32 i;
	dma_buf_status_t dma_status;
	dma_buf_properties_t dma_properties = { 0, 0, 0, 0 };

	i = nxt_to_use;

	dma_properties.f_cached = true;
	dma_properties.alignment = DA_PEC_DMA_ALIGNMENT_BYTE_COUNT;
	dma_properties.bank = DA_PEC_BANK_PACKET;
	dma_properties.size = EIP_RX_BUF_SIZE;

	dma_status = dma_buf_alloc(dma_properties,
				   &eip_ring->rx_buff[i].host_address,
				   &eip_ring->rx_buff[i].handle);
	if (dma_status != DMABUF_STATUS_OK) {
		LOG_CRIT("eip rx buffer dma_buf_alloc() failed with err:%d\n",
			 dma_status);
		return -ENOMEM;
	}

	LOG_INFO("eip rx buffer created\n");
	return 0;
}

/**
 * @brief eip_create_rx_buffers - create rx buffers
 *
 * @param eip_ring eip ring interface structure
 * @return int - returns 0 on success and negative on failure
 */
int eip_create_rx_buffers(struct eip_ring_interface *eip_ring)
{
	dma_buf_status_t dma_status;
	dma_buf_properties_t dma_properties = { 0, 0, 0, 0 };
	u8 i;
	int err;

	eip_ring->rx_buff = kcalloc(MAX_RD_COUNT,
				    sizeof(struct eip_pkt_buff), GFP_ATOMIC);
	if (!eip_ring->rx_buff) {
		err = -ENOMEM;
		goto err_alloc_rx_buffers;
	}
	eip_ring->rx_nxt_to_use = 0;
	eip_ring->rx_nxt_to_clean = 0;

	/* Initialise packet buffers to default interface id as
	 * we reuse these packet buffers and don't create them again on packet receive
	 * For other interafces we create them during prepare_rds
	 */
	if (eip_ring->rid == DEFAULT_INTERFACE_ID) {
		/* This will be removed once we started using build_skb*/
		dma_properties.f_cached = true;
		dma_properties.alignment = DA_PEC_DMA_ALIGNMENT_BYTE_COUNT;
		dma_properties.bank = DA_PEC_BANK_PACKET;
		dma_properties.size = EIP_RX_BUF_SIZE;
		for (i = 0; i < MAX_RD_COUNT; i++) {
			dma_status = dma_buf_alloc(dma_properties,
						   &eip_ring->rx_buff[i].host_address,
				&eip_ring->rx_buff[i].handle);
			if (dma_status != DMABUF_STATUS_OK) {
				err = -ENOMEM;
				goto err_dmabuff_alloc;
			}
		}

		LOG_INFO("eip rx pkt buffers created\n");
	}

	return 0;

err_dmabuff_alloc:
	for (int j = 0; j < i; j++) {
		if (eip_ring->rx_buff[j].handle.p) {
			dma_buf_release(eip_ring->rx_buff[j].handle);
			eip_ring->rx_buff[j].handle.p = NULL;
		}
	}
	kfree(eip_ring->rx_buff);
	eip_ring->rx_buff = NULL;
err_alloc_rx_buffers:
	return err;
}

/**
 * @brief eip_delete_rx_buffers - eip delete rx pkt buffers
 *
 * @param eip_ring eip ring interface structure
 */
void eip_delete_rx_buffers(struct eip_ring_interface *eip_ring)
{
	u32 i;

	if (eip_ring->rx_buff) {
		for (i = 0; i < MAX_RD_COUNT; i++) {
			if (eip_ring->rx_buff[i].handle.p) {
				dma_buf_release(eip_ring->rx_buff[i].handle);
				eip_ring->rx_buff[i].handle.p = NULL;
			}
		}

		kfree(eip_ring->rx_buff);
		eip_ring->rx_buff = NULL;
	}
	LOG_INFO("eip rx buffers deleted\n");
}
