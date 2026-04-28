// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * interrupt.c - EIP ring interrupt handling
 *
 * Copyright (C) 2022-2026 Axiado Corporation.
 */

#include <linux/etherdevice.h>
#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>

#include "da_pec_internal.h"
#include "eip.h"
#include "log.h"
#include "pec_api.h"
#include "device_mgmt.h"
#include "device_rw.h"

#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/types.h>

/**
 * @brief IRQ handler for all aic-Advanced Interrupt Controller-0 of EIP.
 * @param irq number for aic-0 interrupt.
 * @param dev net_dev struct
 * @return
 */
static irqreturn_t irq_handler_ring(int irq, void *data)
{
	struct eip_ring_interface *eip_ring = (struct eip_ring_interface *)data;

	if (!eip_ring || eip_ring->rid >= RING_INTERFACE_CNT) {
		LOG_CRIT("ERROR: failed to get eip_ring\n");
		return IRQ_NONE;
	}

	ack_rdr_irq(eip_ring->rid);
	disable_rdr_irq(eip_ring->rid);

	//TODO: handle IPSEC_LOOKASIDE_INTERFACE
	if (eip_ring->rid == REJECTED_FLOW_RULE_RING_ID) {
		tasklet_schedule(&eip_ring->rdr_handler.tasklet_ring);
	} else {
		if (likely(napi_schedule_prep(&eip_ring->rdr_handler.napi_ring)))
			__napi_schedule(&eip_ring->rdr_handler.napi_ring);
		else
			LOG_CRIT("ERROR : Not able to schedule the rx_cleanup\n");
	}

	return IRQ_HANDLED;
}

/**
 * @brief Acknowledge interrupt for particular EIP-ring.
 * @param priv privat struct containing interface_id.
 */
void ack_rdr_irq(u8 rid)
{
	device_handle_t dev;
	u32 curr_val, offset;

	dev = eip_get_global_dev_handle();
	if (!dev)
		return;

	offset = 0x8083C + rid * 0x1000; // 4K separated.
	curr_val = device_read32(dev, offset);

	if (device_read32(dev, offset))
		device_write32(dev, offset, (curr_val | 0x10));
}

/**
 * @brief Enable interrupt for particular EIP-ring.
 * @param priv privat struct containing interface_id.
 */
void enable_rdr_irq(u8 rid)
{
	device_handle_t dev;
	u32 curr_val, new_val, offset;

	dev = eip_get_global_dev_handle();
	if (!dev)
		return;

	offset = 0x9E808 - (rid * 0x1000);

	/* Enable interrupt */
	curr_val = device_read32(dev, offset);
	/* TODO replace 0x3 with 0x2.
	 * 0x3 represents both-CDR and RDR.
	 * We need only RDR here.
	 */
	new_val = 0x3 << (2 * rid);
	device_write32(dev, offset, (curr_val | new_val));

	/* Set RDR irq threshold to 1 RD*/
	offset = 0x80828 + rid * 0x1000;
	curr_val = device_read32(dev, offset);
	device_write32(dev, offset, 0x20);
}

/**
 * @brief Disable interrupt for particular EIP-ring.
 * @param priv private struct containing interface_id.
 */
void disable_rdr_irq(u8 rid)
{
	device_handle_t dev;
	u32 curr_val, new_val, offset;

	dev = eip_get_global_dev_handle();
	if (!dev)
		return;

	offset = 0x9E814 - (rid * 0x1000);

	curr_val = device_read32(dev, offset);
	/* TODO replace 0x3 with 0x2.
	 * 0x3 represents both-CDR and RDR.
	 * We need only RDR here.
	 */
	new_val = 0x3 << (2 * rid);
	device_write32(dev, offset, (curr_val | new_val));
}

/**
 * @brief EIP polling function that is registered with napi.
 * @param napi struct ptr
 * @param budget Napi will continue to poll if work-done is equal to the budget
 * @return work-done(Received RD count) by the napi scheduler.
 */
int eip_rdr_poll(struct napi_struct *napi, int budget)
{
	struct eip_ring_interface *eip_ring =
		container_of((union ring_handler *)napi,
			     struct eip_ring_interface, rdr_handler);
	unsigned int work_done = 0, total_work = 0;

	LOG_DEBG("%s: Trying Rx from Ring-%d", __func__, eip_ring->rid);
	while (total_work < budget) {
		work_done = eip_rx_clean_rdr(eip_ring);
		total_work += work_done;

		/* If budget fully consumed, continue polling */
		if (work_done < MAX_RD_COUNT) {
			/* only re-enable interrupt if stack agrees polling is really done */
			if (likely(napi_complete_done(napi, total_work)))
				enable_rdr_irq(eip_ring->rid);
			break;
		}
	}

	return total_work;
}

static void tasklet_eip_rdr(struct tasklet_struct *t)
{
	struct eip_ring_interface *eip_ring =
		container_of((union ring_handler *)t, struct eip_ring_interface,
			     rdr_handler);
	unsigned int work_done = 0;

	work_done = eip_rx_clean_rdr(eip_ring);
	enable_rdr_irq(eip_ring->rid);
}

/**
 * @brief Register irq handler for interrupt.
 * @param eip_ring eip ring interface
 * @param ndev netdev device struct ptr
 * @param dev platform device struct ptr for DT IRQ lookup
 * @return 0 if successful, otehrwise non-zero.
 */
int interrupt_init(struct eip_ring_interface *eip_ring, struct net_device *ndev,
		   struct device *dev)
{
	struct platform_device *pdev;
	int irq;
	int ret;

	if (eip_ring->rid >= RING_INTERFACE_CNT)
		return -EINVAL;

	if (!dev) {
		LOG_CRIT("EIP device missing for IRQ lookup\n");
		return -ENODEV;
	}

	snprintf(eip_ring->name, EIP_IRQ_NAME_LEN, "eip_ring%u", eip_ring->rid);

	pdev = to_platform_device(dev);
	/* Ring IRQs start at index 1 in HCP device (index 0 is EIP-197 crypto) */
	irq = platform_get_irq_byname(pdev, eip_ring->name);

	if (irq < 0) {
		LOG_CRIT("Failed to get IRQ for ring %u: %d\n", eip_ring->rid,
			 irq);
		return irq;
	}

	eip_ring->irq = irq;
	//TODO: handle IPSEC_LOOKASIDE_INTERFACE
	if (eip_ring->rid == REJECTED_FLOW_RULE_RING_ID) {
		tasklet_setup(&eip_ring->rdr_handler.tasklet_ring,
			      tasklet_eip_rdr);
	} else {
		/* use DFL netdev for napi */
		netif_napi_add_weight(ndev, &eip_ring->rdr_handler.napi_ring,
				      eip_rdr_poll, EIP_NAPI_BUDGET);
	}
	ret = request_irq(irq, irq_handler_ring, IRQF_SHARED, eip_ring->name,
			  eip_ring);
	LOG_CRIT("register IRQ %d status %d, ring %u, name %s\n", irq, ret,
		 eip_ring->rid, eip_ring->name);

	return ret;
}

/**
 * @brief free the irq handler associated with the interface.
 * @param priv eip_priv struct ptr
 */
void eip_free_irq(struct eip_ring_interface *eip_ring)
{
	if (!eip_ring->irq)
		return;

	if (!irq_get_irq_data(eip_ring->irq)) {
		LOG_WARN("IRQ %d is already freed.\n", eip_ring->irq);
		return;
	}
	free_irq(eip_ring->irq, eip_ring);
	eip_ring->irq = 0;
}
