// SPDX-License-Identifier: GPL-2.0+
/*
 * Axiado AX3000/AX3005 USB Device Controller driver.
 *
 * Copyright (c) 2019 Corigine Inc.
 * Copyright (c) 2021-2026 Axiado Corporation.
 */
#include <linux/sched.h>
#include <linux/scatterlist.h>

#include "crg-udc.h"

void crg_udc_advance_dequeue_pt(struct crg_udc_ep *udc_ep)
{
	struct crg_udc_request *udc_req;

	if (!list_empty(&udc_ep->queue)) {
		udc_req = list_entry(udc_ep->queue.next, struct crg_udc_request,
				     queue);

		if (udc_req->first_trb)
			udc_ep->deq_pt = udc_req->first_trb;
		else
			udc_ep->deq_pt = udc_ep->enq_pt;
	} else {
		udc_ep->deq_pt = udc_ep->enq_pt;
	}
}

unsigned int crg_udc_count_trbs_linear(dma_addr_t addr, u64 len)
{
	unsigned int num_trbs = 0;

	while (len) {
		u32 chunk = crg_udc_trb_chunk_len(addr, len);

		if (!chunk)
			return 0;
		num_trbs++;
		addr += chunk;
		len -= chunk;
	}

	return num_trbs;
}

unsigned int crg_udc_count_trbs_sg(struct crg_udc_request *udc_req)
{
	struct scatterlist *sg;
	u64 remaining = udc_req->usb_req.length;
	unsigned int i;
	unsigned int num_trbs = 0;

	for_each_sg(udc_req->usb_req.sg, sg, udc_req->usb_req.num_sgs, i) {
		dma_addr_t addr;
		u64 len;

		if (!sg_dma_len(sg))
			continue;
		if (!remaining)
			break;

		len = min_t(u64, sg_dma_len(sg), remaining);
		addr = sg_dma_address(sg);
		while (len) {
			u32 chunk = crg_udc_trb_chunk_len(addr, len);

			if (!chunk)
				return 0;
			num_trbs++;
			addr += chunk;
			len -= chunk;
			remaining -= chunk;
		}
	}

	return remaining ? 0 : num_trbs;
}

static dma_addr_t event_trb_virt_to_dma(struct crg_udc_event *udc_event,
					struct crg_event_trb *event)
{
	dma_addr_t dma_addr = 0;
	unsigned long seg_offset;

	if (!udc_event || !event)
		return DMA_MAPPING_ERROR;

	if ((void *)event < udc_event->event_ring.cpu_addr ||
	    (void *)event >=
		    udc_event->event_ring.cpu_addr + udc_event->event_ring.len)
		return DMA_MAPPING_ERROR;

	seg_offset = (void *)event - udc_event->event_ring.cpu_addr;
	dma_addr = udc_event->event_ring.dma_addr + seg_offset;

	return dma_addr;
}

int crg_udc_init_event_ring(struct crg_gadget_dev *crg_udc, int index)
{
	struct crg_uicr __iomem *uicr = crg_udc->uicr[index];
	struct crg_udc_event *udc_event = &crg_udc->udc_event[index];

	int ret;
	u32 buff_length;
	dma_addr_t mapping;
	bool erst_allocated = false;

	buff_length = CRG_ERST_SIZE * sizeof(struct crg_erst);
	if (!udc_event->erst.cpu_addr) {
		udc_event->erst.cpu_addr = dma_alloc_coherent(
			crg_udc->dev, buff_length, &mapping, GFP_ATOMIC);

		if (!udc_event->erst.cpu_addr)
			return -ENOMEM;
		erst_allocated = true;
	} else {
		mapping = udc_event->erst.dma_addr;
	}

	udc_event->erst.len = buff_length;
	udc_event->erst.dma_addr = mapping;
	udc_event->p_erst = udc_event->erst.cpu_addr;

	buff_length = CRG_EVENT_RING_SIZE * sizeof(struct crg_event_trb);
	if (!udc_event->event_ring.cpu_addr) {
		udc_event->event_ring.cpu_addr = dma_alloc_coherent(
			crg_udc->dev, buff_length, &mapping, GFP_ATOMIC);

		if (!udc_event->event_ring.cpu_addr) {
			ret = -ENOMEM;
			goto err_free_erst;
		}
	} else {
		mapping = udc_event->event_ring.dma_addr;
	}

	udc_event->event_ring.len = buff_length;
	udc_event->event_ring.dma_addr = mapping;
	udc_event->evt_dq_pt = udc_event->event_ring.cpu_addr;

	udc_event->evt_seg0_last_trb =
		(struct crg_event_trb *)(udc_event->event_ring.cpu_addr) +
		(CRG_EVENT_RING_SIZE - 1);

	udc_event->ccs = 1;

	udc_event->p_erst->seg_addr_lo =
		cpu_to_le32(lower_32_bits(udc_event->event_ring.dma_addr));
	udc_event->p_erst->seg_addr_hi =
		cpu_to_le32(upper_32_bits(udc_event->event_ring.dma_addr));
	udc_event->p_erst->seg_size = cpu_to_le32(CRG_EVENT_RING_SIZE);
	udc_event->p_erst->rsvd = cpu_to_le32(0);
	/* Ensure that event ring is updated */
	dma_wmb();
	/* clear the event ring, to avoid hw unexpected ops
	 * because of dirty data
	 */
	memset(udc_event->event_ring.cpu_addr, 0, buff_length);

	/* Ensure that event ring is clear before programming
	 * ERSTBA/ERSTSZ/ERDP
	 */
	dma_wmb();

	writel(CRG_ERST_SIZE, &uicr->erstsz);
	writel(lower_32_bits(udc_event->erst.dma_addr), &uicr->erstbalo);
	writel(upper_32_bits(udc_event->erst.dma_addr), &uicr->erstbahi);
	writel(lower_32_bits(udc_event->event_ring.dma_addr) |
		       CRG_U3DC_ERDPLO_EHB,
	       &uicr->erdplo);
	writel(upper_32_bits(udc_event->event_ring.dma_addr), &uicr->erdphi);

	writel((CRG_U3DC_IMAN_INT_EN | CRG_U3DC_IMAN_INT_PEND), &uicr->iman);
	/* Interrupt moderation: 4000 * 250ns = 1ms */
	writel(4000, &uicr->imod);
	return 0;

err_free_erst:
	/* Only free what this call allocated; a pre-existing erst is owned
	 * by an earlier successful init and released by crg_udc_free_resources().
	 */
	if (erst_allocated) {
		dma_free_coherent(crg_udc->dev, udc_event->erst.len,
				  udc_event->erst.cpu_addr,
				  udc_event->erst.dma_addr);
		udc_event->erst.cpu_addr = NULL;
	}
	return ret;
}

static bool is_event_ring_x_empty(struct crg_gadget_dev *crg_udc, int index)
{
	struct crg_event_trb *event;
	struct crg_udc_event *udc_event;

	udc_event = &crg_udc->udc_event[index];
	if (udc_event->evt_dq_pt) {
		event = (struct crg_event_trb *)udc_event->evt_dq_pt;

		if (FIELD_GET(EVE_TRB_CYCLE_BIT_MASK,
			      le32_to_cpu(event->dw3)) != udc_event->ccs)
			return true;
	}

	return false;
}

bool crg_udc_is_event_rings_empty(struct crg_gadget_dev *crg_udc)
{
	int i;

	for (i = 0; i < CRG_RING_NUM; i++) {
		if (!is_event_ring_x_empty(crg_udc, i)) {
			dev_err(crg_udc->dev, "event ring not empty\n");
			return false;
		}
	}
	return true;
}

bool crg_udc_is_pointer_less_than(struct crg_transfer_trb *a,
				  struct crg_transfer_trb *b,
				  struct crg_udc_ep *udc_ep)
{
	if (!a || !b || !udc_ep->first_trb || !udc_ep->last_trb)
		return false;
	if (b > a && (udc_ep->enq_pt >= b || udc_ep->enq_pt < a))
		return true;
	if (b < a && (udc_ep->enq_pt >= b && udc_ep->enq_pt < a))
		return true;
	return false;
}

int crg_udc_process_event_ring(struct crg_gadget_dev *crg_udc, int index,
			       unsigned long *flags)
{
	struct crg_event_trb *event;
	struct crg_udc_event *udc_event;
	struct crg_uicr __iomem *uicr = crg_udc->uicr[index];
	u32 val;
	dma_addr_t erdp;
	int ret;
	unsigned int processed = 0;

	if (!uicr)
		return -ENODEV;

	val = readl(&uicr->iman);
	val |= CRG_U3DC_IMAN_INT_PEND;
	writel(val, &uicr->iman);

	udc_event = &crg_udc->udc_event[index];
	while (udc_event->evt_dq_pt) {
		event = (struct crg_event_trb *)udc_event->evt_dq_pt;

		if (FIELD_GET(EVE_TRB_CYCLE_BIT_MASK,
			      le32_to_cpu(event->dw3)) != udc_event->ccs)
			break;
		/* Read the payload only after observing device ownership. */
		dma_rmb();

		ret = crg_udc_handle_event(crg_udc, event, flags);
		if (ret == -ECONNRESET) {
			/* The disconnect path rebuilt and reprogrammed the ring. */
			return 0;
		}
		if (ret < 0)
			dev_dbg(crg_udc->dev, "failed to process event: %d\n",
				ret);

		/* Consume malformed events so they cannot pin the ring head. */
		if (event == udc_event->evt_seg0_last_trb) {
			udc_event->ccs = udc_event->ccs ? 0 : 1;
			udc_event->evt_dq_pt = udc_event->event_ring.cpu_addr;
		} else {
			udc_event->evt_dq_pt++;
		}

		/*
		 * udc_event state is only re-read from crg_udc, never cached
		 * across this drop, so releasing and reacquiring the lock
		 * here is safe and lets other udc_lock users, and this CPU's
		 * other interrupts, make progress during a busy event ring.
		 */
		if (++processed % CRG_EVENT_RING_BATCH == 0) {
			erdp = event_trb_virt_to_dma(udc_event,
						     udc_event->evt_dq_pt);
			if (!dma_mapping_error(crg_udc->dev, erdp)) {
				writel(upper_32_bits(erdp), &uicr->erdphi);
				writel(lower_32_bits(erdp) |
					       CRG_U3DC_ERDPLO_EHB,
				       &uicr->erdplo);
			}
			spin_unlock_irqrestore(&crg_udc->udc_lock, *flags);
			cond_resched();
			spin_lock_irqsave(&crg_udc->udc_lock, *flags);
		}
	}

	/* update dequeue pointer */
	erdp = event_trb_virt_to_dma(udc_event, udc_event->evt_dq_pt);
	if (dma_mapping_error(crg_udc->dev, erdp))
		return -EINVAL;
	writel(upper_32_bits(erdp), &uicr->erdphi);
	writel(lower_32_bits(erdp) | CRG_U3DC_ERDPLO_EHB, &uicr->erdplo);

	return 0;
}

/* This function will go through the list of the USB requests for the
 * given endpoint and schedule any unscheduled trb's to the xfer ring
 */
int crg_udc_queue_pending_trbs(struct crg_udc_ep *udc_ep_ptr)
{
	struct crg_udc_request *udc_req_ptr, *tmp;
	int ret;

	list_for_each_entry_safe(udc_req_ptr, tmp, &udc_ep_ptr->queue, queue) {
		if (udc_req_ptr->all_trbs_queued)
			continue;

		ret = crg_udc_build_td(udc_ep_ptr, udc_req_ptr);
		if (ret == -ENOSPC || ret == -EBUSY) {
			/*
			 * Ring is full; leave this request queued and retry
			 * it (and anything behind it) on the next TRB
			 * completion via crg_udc_queue_pending_trbs().
			 */
			return ret;
		} else if (ret) {
			/*
			 * A non-capacity failure is not transient: fail this
			 * request instead of leaving it stuck forever, and
			 * keep rebuilding the rest of the queue.
			 */
			dev_err(udc_ep_ptr->crg_udc->dev,
				"failed to build pending transfer: %d\n", ret);
			list_del_init(&udc_req_ptr->queue);
			crg_udc_req_done(udc_ep_ptr, udc_req_ptr, ret);
			continue;
		}
		break;
	}

	return 0;

}

/* num_trbs here is the size of the ring. */
u32 crg_udc_room_on_ring(struct crg_gadget_dev *crg_udc, u32 num_trbs,
			 struct crg_transfer_trb *p_ring,
			 struct crg_transfer_trb *enq_pt,
			 struct crg_transfer_trb *dq_pt)
{
	u32 i = 0;

	if (enq_pt == dq_pt) {
		/* ring is empty */
		return num_trbs - 2;
	}

	while (enq_pt != dq_pt) {
		i++;

		enq_pt++;

		if (FIELD_GET(TRB_TYPE_MASK, le32_to_cpu(enq_pt->dw3)) ==
		    TRB_TYPE_LINK)
			enq_pt = p_ring;

		if (i > num_trbs)
			break;
	}

	return i - 1;
}

void crg_udc_setup_datastage_trb(struct crg_transfer_trb *p_trb, dma_addr_t dma,
				 u8 pcs, u32 transfer_length, u32 td_size,
				 bool ioc, bool azp, u8 dir, u8 setup_tag)
{
	u32 tmp;

	p_trb->dw0 = cpu_to_le32(lower_32_bits(dma));
	p_trb->dw1 = cpu_to_le32(upper_32_bits(dma));

	/* TRB_Transfer_Length
	 * For USB_DIR_OUT, this field is the number of data bytes expected
	 * from xhc. For USB_DIR_IN, this field is the number of data bytes
	 * the device will send.
	 */
	tmp = 0;
	FIELD_MODIFY(TRB_TRANSFER_LEN_MASK, &tmp, transfer_length);
	FIELD_MODIFY(TRB_TD_SIZE_MASK, &tmp, td_size);
	FIELD_MODIFY(TRB_INTR_TARGET_MASK, &tmp, 0);
	p_trb->dw2 = cpu_to_le32(tmp);

	tmp = 0;
	FIELD_MODIFY(TRB_CYCLE_BIT_MASK, &tmp, pcs);
	FIELD_MODIFY(TRB_INTR_ON_SHORT_PKT_MASK, &tmp, 1);
	FIELD_MODIFY(TRB_INTR_ON_COMPLETION_MASK, &tmp, ioc);
	FIELD_MODIFY(TRB_TYPE_MASK, &tmp, TRB_TYPE_XFER_DATA_STAGE);
	FIELD_MODIFY(TRB_APPEND_ZLP_MASK, &tmp, azp);
	FIELD_MODIFY(DATA_STAGE_TRB_DIR_MASK, &tmp, dir);
	FIELD_MODIFY(TRB_SETUP_TAG_MASK, &tmp, setup_tag);

	/* Publish ownership only after the TRB payload is visible. */
	dma_wmb();
	p_trb->dw3 = cpu_to_le32(tmp);
}

void crg_udc_setup_link_trb(struct crg_transfer_trb *link_trb, bool toggle,
			    dma_addr_t next_trb)
{
	u32 dw = 0;

	link_trb->dw0 = cpu_to_le32(lower_32_bits(next_trb));
	link_trb->dw1 = cpu_to_le32(upper_32_bits(next_trb));
	link_trb->dw2 = cpu_to_le32(0);

	FIELD_MODIFY(TRB_TYPE_MASK, &dw, TRB_TYPE_LINK);
	if (toggle)
		FIELD_MODIFY(TRB_LINK_TOGGLE_CYCLE_MASK, &dw, 1);
	else
		FIELD_MODIFY(TRB_LINK_TOGGLE_CYCLE_MASK, &dw, 0);

	/* Publish ownership only after the link address is visible. */
	dma_wmb();
	link_trb->dw3 = cpu_to_le32(dw);
}

void crg_udc_setup_status_trb(struct crg_gadget_dev *crg_udc,
			      struct crg_transfer_trb *p_trb,
			      struct usb_request *usb_req, u8 pcs, u8 set_addr,
			      u8 stall)
{
	u32 tmp, dir = 0;

	/* There are some cases where crg_udc_setup_status_trb() is called with
	 * usb_req set to NULL.
	 */
	p_trb->dw0 = cpu_to_le32(0);
	p_trb->dw1 = cpu_to_le32(0);

	tmp = 0;
	FIELD_MODIFY(TRB_INTR_TARGET_MASK, &tmp, 0);
	p_trb->dw2 = cpu_to_le32(tmp);

	tmp = 0;
	FIELD_MODIFY(TRB_CYCLE_BIT_MASK, &tmp, pcs);
	FIELD_MODIFY(TRB_INTR_ON_COMPLETION_MASK, &tmp, 1);
	FIELD_MODIFY(TRB_TYPE_MASK, &tmp, TRB_TYPE_XFER_STATUS_STAGE);

	dir = (crg_udc->setup_status == STATUS_STAGE_XFER) ? 0 : 1;
	FIELD_MODIFY(DATA_STAGE_TRB_DIR_MASK, &tmp, dir);

	FIELD_MODIFY(TRB_SETUP_TAG_MASK, &tmp, crg_udc->setup_tag);
	FIELD_MODIFY(STATUS_STAGE_TRB_STALL_MASK, &tmp, stall);
	FIELD_MODIFY(STATUS_STAGE_TRB_SET_ADDR_MASK, &tmp, set_addr);

	/* Publish ownership only after the TRB payload is visible. */
	dma_wmb();
	p_trb->dw3 = cpu_to_le32(tmp);
}

void crg_udc_setup_trb(struct crg_transfer_trb *p_trb, u32 xfer_len,
		       dma_addr_t xfer_buf_addr, u8 td_size, u8 pcs,
		       u8 trb_type, bool short_pkt, bool chain_bit,
		       bool intr_on_compl, bool b_setup_stage, u8 usb_dir,
		       bool b_isoc, u16 frame_id, bool sia, bool azp)
{
	u32 tmp;

	p_trb->dw0 = cpu_to_le32(lower_32_bits(xfer_buf_addr));
	p_trb->dw1 = cpu_to_le32(upper_32_bits(xfer_buf_addr));

	tmp = 0;
	FIELD_MODIFY(TRB_TRANSFER_LEN_MASK, &tmp, xfer_len);
	FIELD_MODIFY(TRB_TD_SIZE_MASK, &tmp, td_size);
	FIELD_MODIFY(TRB_INTR_TARGET_MASK, &tmp, 0);

	p_trb->dw2 = cpu_to_le32(tmp);

	tmp = 0;
	FIELD_MODIFY(TRB_CYCLE_BIT_MASK, &tmp, pcs);
	FIELD_MODIFY(TRB_INTR_ON_SHORT_PKT_MASK, &tmp, short_pkt);
	FIELD_MODIFY(TRB_CHAIN_BIT_MASK, &tmp, chain_bit);
	FIELD_MODIFY(TRB_INTR_ON_COMPLETION_MASK, &tmp, intr_on_compl);
	FIELD_MODIFY(TRB_APPEND_ZLP_MASK, &tmp, azp);
	FIELD_MODIFY(TRB_TYPE_MASK, &tmp, trb_type);

	if (b_setup_stage)
		FIELD_MODIFY(DATA_STAGE_TRB_DIR_MASK, &tmp, usb_dir);

	if (b_isoc) {
		FIELD_MODIFY(ISOC_TRB_FRAME_ID_MASK, &tmp, frame_id);
		FIELD_MODIFY(ISOC_TRB_SIA_MASK, &tmp, sia);
	}

	/* Publish ownership only after the TRB payload is visible. */
	dma_wmb();
	p_trb->dw3 = cpu_to_le32(tmp);
}

void crg_udc_squeeze_xfer_ring(struct crg_udc_ep *udc_ep_ptr,
			       struct crg_udc_request *udc_req_ptr)
{
	struct crg_transfer_trb *trb = udc_req_ptr->first_trb;
	struct crg_udc_request *next_req, *tmp;
	u8 new_pcs;

	if (!trb)
		return;

	/* The canceled request becomes the new enqueue point. Its original
	 * cycle bit is the cycle expected at that point after the tail is
	 * removed.
	 */
	new_pcs = FIELD_GET(TRB_CYCLE_BIT_MASK, le32_to_cpu(trb->dw3));

	while (trb != udc_ep_ptr->enq_pt) {
		memset(trb, 0, sizeof(*trb));
		trb++;
		if (trb == udc_ep_ptr->last_trb)
			trb = udc_ep_ptr->first_trb;
	}

	udc_ep_ptr->enq_pt = udc_req_ptr->first_trb;
	udc_ep_ptr->pcs = new_pcs;
	udc_ep_ptr->tran_ring_full = false;
	crg_udc_setup_link_trb(udc_ep_ptr->last_trb, true,
			udc_ep_ptr->tran_ring_info.dma_addr);
	dma_wmb();

	if (list_is_last(&udc_req_ptr->queue, &udc_ep_ptr->queue))
		return;

	next_req = udc_req_ptr;
	list_for_each_entry_safe_continue(next_req, tmp, &udc_ep_ptr->queue,
					  queue) {
		int ret;

		next_req->usb_req.status = -EINPROGRESS;
		next_req->usb_req.actual = 0;
		crg_udc_clear_req_container(next_req);
		ret = crg_udc_build_td(udc_ep_ptr, next_req);
		if (ret == -ENOSPC) {
			/* Ring is full; a later TRB completion will retry
			 * this and any remaining requests via
			 * crg_udc_queue_pending_trbs().
			 */
			udc_ep_ptr->tran_ring_full = true;
			break;
		} else if (ret) {
			/*
			 * A non-capacity failure is not transient: fail this
			 * request instead of leaving it stuck forever, and
			 * keep rebuilding the rest of the queue.
			 */
			dev_err(udc_ep_ptr->crg_udc->dev,
				"failed to rebuild queued request: %d\n", ret);
			crg_udc_req_done(udc_ep_ptr, next_req, ret);
			continue;
		}
		if (udc_ep_ptr->tran_ring_full)
			break;
	}
}

struct crg_transfer_trb *crg_udc_tran_trb_dma_to_virt(struct crg_udc_ep *udc_ep,
						    dma_addr_t address)
{
	dma_addr_t offset;
	struct crg_transfer_trb *trb_virt;

	if (lower_32_bits(address) & 0xf)
		return NULL;

	offset = address - udc_ep->tran_ring_info.dma_addr;
	if (unlikely(offset >= udc_ep->tran_ring_info.len))
		return NULL;
	offset = offset / sizeof(struct crg_transfer_trb);
	trb_virt = udc_ep->first_trb + offset;
	return trb_virt;
}

dma_addr_t crg_udc_tran_trb_virt_to_dma(struct crg_udc_ep *udc_ep,
					struct crg_transfer_trb *trb)
{
	unsigned long offset;
	int trb_idx;
	dma_addr_t dma_addr = 0;

	trb_idx = trb - udc_ep->first_trb;
	if (unlikely(trb_idx < 0))
		return DMA_MAPPING_ERROR;

	offset = trb_idx * sizeof(*trb);
	if (unlikely(offset >= udc_ep->tran_ring_info.len))
		return DMA_MAPPING_ERROR;
	dma_addr = udc_ep->tran_ring_info.dma_addr + offset;
	return dma_addr;
}

void crg_udc_update_dequeue_pt(struct crg_event_trb *event,
			       struct crg_udc_ep *udc_ep)
{
	u32 deq_pt_lo = le32_to_cpu(event->dw0);
	u32 deq_pt_hi = le32_to_cpu(event->dw1);
	u64 dq_pt_addr = (u64)deq_pt_lo | ((u64)deq_pt_hi << 32);
	struct crg_transfer_trb *deq_pt;

	deq_pt = crg_udc_tran_trb_dma_to_virt(udc_ep, dq_pt_addr);
	if (!deq_pt)
		return;
	deq_pt++;
	if (deq_pt == udc_ep->last_trb)
		deq_pt = udc_ep->first_trb;

	if (FIELD_GET(TRB_TYPE_MASK, le32_to_cpu(deq_pt->dw3)) == TRB_TYPE_LINK)
		deq_pt = udc_ep->first_trb;

	udc_ep->deq_pt = deq_pt;
}

u32 crg_udc_trb_chunk_len(dma_addr_t dma, u64 remaining)
{
	u32 boundary = CRG_UDC_TRB_MAX_BUFFER_SIZE -
		       (lower_32_bits(dma) & (CRG_UDC_TRB_MAX_BUFFER_SIZE - 1));

	return min_t(u64, remaining, boundary);
}

u32 crg_udc_actual_before(struct crg_udc_ep *udc_ep,
			  struct crg_udc_request *udc_req,
			  struct crg_transfer_trb *pause)
{
	struct crg_transfer_trb *trb = udc_req->first_trb;
	u32 actual = 0;
	u32 guard = 0;
	u32 max_iter = udc_ep->tran_ring_info.len / sizeof(*trb);

	if (!trb || !pause)
		return 0;

	while (trb != pause && trb != udc_ep->enq_pt && guard++ < max_iter) {
		u32 len;

		if (FIELD_GET(TRB_TYPE_MASK, le32_to_cpu(trb->dw3)) ==
		    TRB_TYPE_LINK) {
			trb = udc_ep->first_trb;
			continue;
		}
		len = FIELD_GET(TRB_TRANSFER_LEN_MASK, le32_to_cpu(trb->dw2));
		actual += len;
		trb++;
		if (trb == udc_ep->last_trb)
			trb = udc_ep->first_trb;
	}

	return min(actual, udc_req->usb_req.length);
}
