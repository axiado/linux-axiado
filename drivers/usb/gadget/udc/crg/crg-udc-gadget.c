// SPDX-License-Identifier: GPL-2.0+
/*
 * Axiado AX3000/AX3005 USB Device Controller driver.
 *
 * Copyright (c) 2019 Corigine Inc.
 * Copyright (c) 2021-2026 Axiado Corporation.
 */
#include <linux/iopoll.h>
#include <linux/scatterlist.h>

#include "crg-udc.h"
#include "crg-udc-trace.h"

/* Forward declarations for functions used before their definition. */
static int crg_udc_epcx_setup(struct crg_udc_ep *udc_ep);
static int crg_udc_epcx_update_dqptr(struct crg_udc_ep *udc_ep);
static int crg_udc_queue_trbs(struct crg_udc_ep *udc_ep_ptr,
			      struct crg_udc_request *udc_req_ptr, bool b_isoc,
			      u32 xfer_ring_size, u32 num_trbs_needed,
			      u64 buffer_length);
static int crg_udc_reinit(struct crg_gadget_dev *crg_udc);
static bool crg_udc_sg_advance(struct crg_udc_request *udc_req, u32 bytes);
static bool crg_udc_sg_current(struct crg_udc_request *udc_req, dma_addr_t *dma,
			       u32 *len);
static int crg_udc_sg_init(struct crg_udc_request *udc_req);
static int crg_udc_validate_ep_desc(struct crg_udc_ep *udc_ep,
				    const struct usb_endpoint_descriptor *desc);
static bool is_request_dequeued(struct crg_gadget_dev *crg_udc,
				struct crg_udc_ep *udc_ep,
				struct crg_event_trb *event);
static int set_ep_halt(struct crg_gadget_dev *crg_udc, int dci,
		       unsigned long *flags);

static int crg_gadget_get_frame(struct usb_gadget *g)
{
	struct crg_gadget_dev *crg_udc = gadget_to_udc(g);

	return FIELD_GET(CRG_U3DC_MFINDEX_FRAME_MASK,
			 readl(&crg_udc->uccr->mfindex));
}

static int crg_gadget_pullup(struct usb_gadget *g, int is_on)
{
	struct crg_gadget_dev *crg_udc = gadget_to_udc(g);
	unsigned long flags;

	spin_lock_irqsave(&crg_udc->udc_lock, flags);
	crg_udc->pullup_on = !!is_on;
	crg_udc_set_pullup(crg_udc, crg_udc->pullup_on);
	spin_unlock_irqrestore(&crg_udc->udc_lock, flags);
	trace_crg_udc_pullup(crg_udc->pullup_on);
	return 0;
}

static int crg_gadget_set_remote_wakeup(struct usb_gadget *g, int set)
{
	struct crg_gadget_dev *crg_udc = gadget_to_udc(g);
	unsigned long flags;

	spin_lock_irqsave(&crg_udc->udc_lock, flags);
	crg_udc->u2_rwe = !!set;
	g->wakeup_armed = !!set;
	spin_unlock_irqrestore(&crg_udc->udc_lock, flags);

	return 0;
}

static int crg_gadget_set_selfpowered(struct usb_gadget *g, int set)
{
	struct crg_gadget_dev *crg_udc = gadget_to_udc(g);
	unsigned long flags;

	spin_lock_irqsave(&crg_udc->udc_lock, flags);
	g->is_selfpowered = !!set;
	spin_unlock_irqrestore(&crg_udc->udc_lock, flags);

	return 0;
}

static void crg_udc_set_speed(struct usb_gadget *gadget,
			      enum usb_device_speed speed)
{
	struct crg_gadget_dev *crg_udc = gadget_to_udc(gadget);

	crg_udc->max_speed = speed;
}

static int crg_gadget_start(struct usb_gadget *g,
			    struct usb_gadget_driver *driver)
{
	struct crg_gadget_dev *crg_udc = gadget_to_udc(g);
	int ret;

	/* The UDC core calls udc_set_speed() before udc_start(). */
	ret = crg_udc_program_speed(crg_udc);
	if (ret)
		return ret;

	crg_udc->gadget_driver = driver;
	crg_udc_set_vbus_detect(crg_udc, true);
	return 0;
}

static int crg_gadget_stop(struct usb_gadget *g)
{
	struct crg_gadget_dev *crg_udc;
	unsigned long flags = 0;

	crg_udc = gadget_to_udc(g);

	/*
	 * Clear gadget_driver first so crg_udc_handle_port_status() can no
	 * longer schedule udc_reset_work (it only does so while
	 * gadget_driver is set), then cancel_work_sync() is guaranteed to
	 * be the last word: no new reset notification can be queued after
	 * it returns.
	 */
	spin_lock_irqsave(&crg_udc->udc_lock, flags);
	crg_udc->gadget_driver = NULL;
	spin_unlock_irqrestore(&crg_udc->udc_lock, flags);

	cancel_work_sync(&crg_udc->udc_reset_work);

	crg_udc->device_state = USB_STATE_ATTACHED;
	crg_udc_set_vbus_detect(crg_udc, false);

	spin_lock_irqsave(&crg_udc->udc_lock, flags);

	/*
	 * crg_udc_reset() completes any queued requests via
	 * crg_udc_ep_nuke() -> crg_udc_req_done(). crg_udc_req_done() drops udc_lock
	 * (IRQ-safe) around each ->complete() callback and re-acquires it
	 * afterwards, so gadget completion callbacks are never invoked with
	 * the spinlock held. flags is threaded through for that purpose.
	 */
	if (crg_udc_reset(crg_udc, &flags)) {
		dev_err(crg_udc->dev, "controller reset failed during stop\n");
		spin_unlock_irqrestore(&crg_udc->udc_lock, flags);
		return -EIO;
	}
	if (crg_udc_reset_data_struct(crg_udc)) {
		dev_err(crg_udc->dev,
			"controller data reset failed during stop\n");
		spin_unlock_irqrestore(&crg_udc->udc_lock, flags);
		return -EIO;
	}

	crg_udc->connected = 0;
	crg_udc->gadget.speed = USB_SPEED_UNKNOWN;

	if (crg_udc_init_ep0(crg_udc)) {
		spin_unlock_irqrestore(&crg_udc->udc_lock, flags);
		return -EIO;
	}

	spin_unlock_irqrestore(&crg_udc->udc_lock, flags);

	return 0;
}

static int crg_gadget_wakeup(struct usb_gadget *g)
{
	struct crg_gadget_dev *crg_udc = gadget_to_udc(g);
	struct crg_uccr __iomem *uccr = crg_udc->uccr;
	unsigned long flags;
	u32 portsc;
	int ret = 0;

	spin_lock_irqsave(&crg_udc->udc_lock, flags);
	if (!crg_udc->connected || g->speed > USB_SPEED_HIGH) {
		ret = -EOPNOTSUPP;
		goto out;
	}
	if (!crg_udc->u2_rwe) {
		ret = -EACCES;
		goto out;
	}

	portsc = readl(&uccr->portsc);
	if (FIELD_GET(CRG_U3DC_PORTSC_PLS_MASK, portsc) !=
	    CRG_U3DC_PORTSC_PLS_U3) {
		ret = -EINVAL;
		goto out;
	}

	portsc &= ~(CRG_U3DC_PORTSC_W1C_MASK | CRG_U3DC_PORTSC_PLS_MASK);
	portsc |= CRG_U3DC_PORTSC_LWS;
	writel(portsc, &uccr->portsc);
out:
	spin_unlock_irqrestore(&crg_udc->udc_lock, flags);
	return ret;
}

static void crg_gadget_async_callbacks(struct usb_gadget *g, bool enable)
{
	struct crg_gadget_dev *crg_udc = gadget_to_udc(g);
	unsigned long flags;

	spin_lock_irqsave(&crg_udc->udc_lock, flags);
	crg_udc->async_callbacks = enable;
	spin_unlock_irqrestore(&crg_udc->udc_lock, flags);
}

struct usb_request *crg_udc_alloc_request(struct usb_ep *_ep, gfp_t gfp_flags)
{
	struct crg_udc_request *udc_req_ptr;

	udc_req_ptr = kzalloc(sizeof(*udc_req_ptr), gfp_flags);

	if (!udc_req_ptr)
		return NULL;

	udc_req_ptr->usb_req.dma = DMA_MAPPING_ERROR;
	INIT_LIST_HEAD(&udc_req_ptr->queue);

	trace_crg_udc_alloc_request(
		container_of(_ep, struct crg_udc_ep, usb_ep), udc_req_ptr);

	return &udc_req_ptr->usb_req;
}

int crg_udc_build_td(struct crg_udc_ep *udc_ep_ptr,
		     struct crg_udc_request *udc_req_ptr)
{
	int status = 0;
	struct crg_gadget_dev *crg_udc = udc_ep_ptr->crg_udc;
	u32 num_trbs_needed;
	u64 buffer_length;

	if (udc_req_ptr->trbs_needed) {
		/* If this is called to complete pending TRB transfers
		 * of previous Request
		 */
		buffer_length = udc_req_ptr->buff_len_left;
		num_trbs_needed = udc_req_ptr->trbs_needed;
	} else {
		buffer_length = udc_req_ptr->usb_req.length;
		if (udc_req_ptr->usb_req.num_mapped_sgs)
			num_trbs_needed = crg_udc_count_trbs_sg(udc_req_ptr);
		else
			num_trbs_needed = crg_udc_count_trbs_linear(
				udc_req_ptr->usb_req.dma, buffer_length);
		if (!num_trbs_needed && buffer_length)
			return -EINVAL;
		if (!num_trbs_needed)
			num_trbs_needed = 1;
	}

	if (usb_endpoint_xfer_control(udc_ep_ptr->desc))
		status = crg_udc_queue_ctrl(udc_ep_ptr, udc_req_ptr,
					    num_trbs_needed);
	else if (usb_endpoint_xfer_isoc(udc_ep_ptr->desc))
		status = crg_udc_queue_trbs(udc_ep_ptr, udc_req_ptr, true,
					    CRG_UDC_ISOC_EP_TD_RING_SIZE,
					    num_trbs_needed, buffer_length);
	else if (usb_endpoint_xfer_bulk(udc_ep_ptr->desc))
		status = crg_udc_queue_trbs(udc_ep_ptr, udc_req_ptr, false,
					    CRG_UDC_BULK_EP_TD_RING_SIZE,
					    num_trbs_needed, buffer_length);
	else
		/* interrupt endpoint */
		status = crg_udc_queue_trbs(udc_ep_ptr, udc_req_ptr, false,
					    CRG_UDC_INT_EP_TD_RING_SIZE,
					    num_trbs_needed, buffer_length);

	if (!status && !usb_endpoint_xfer_control(udc_ep_ptr->desc))
		crg_udc_knock_doorbell(crg_udc, udc_ep_ptr->dci);

	return status;
}

void crg_udc_clear_portpm(struct crg_gadget_dev *crg_udc)
{
	struct crg_uccr __iomem *uccr = crg_udc->uccr;
	u32 tmp;

	tmp = readl(&uccr->u3portpmsc);

	tmp &= (~CRG_U3DC_U3PORTPM_U1IEN);
	FIELD_MODIFY(CRG_U3DC_U3PORTPM_U1TMOUT_MASK, &tmp, 0);
	tmp &= (~CRG_U3DC_U3PORTPM_U2IEN);
	FIELD_MODIFY(CRG_U3DC_U3PORTPM_U2TMOUT_MASK, &tmp, 0);

	writel(tmp, &uccr->u3portpmsc);

	crg_udc->feature_u1_enable = 0;
	crg_udc->feature_u2_enable = 0;
}

static int crg_udc_ep_dequeue(struct usb_ep *_ep, struct usb_request *_req)
{
	struct crg_udc_ep *udc_ep_ptr;
	struct crg_gadget_dev *crg_udc;
	struct crg_uccr __iomem *uccr;
	struct crg_udc_request *udc_req = NULL;
	struct crg_udc_request *iter;
	struct crg_ep_cx *p_ep_cx;
	struct crg_transfer_trb *pause_pt;
	enum crg_ep_state old_ep_state;
	u32 tmp, deq_pt_lo, deq_pt_hi;
	dma_addr_t dq_pt_addr;
	u8 dci;
	bool found = false;
	unsigned long flags;

	if (!_ep || !_req)
		return -EINVAL;

	udc_ep_ptr = container_of(_ep, struct crg_udc_ep, usb_ep);
	crg_udc = udc_ep_ptr->crg_udc;
	uccr = crg_udc->uccr;
	dci = udc_ep_ptr->dci;

	spin_lock_irqsave(&crg_udc->udc_lock, flags);
	old_ep_state = udc_ep_ptr->ep_state;

	list_for_each_entry(iter, &udc_ep_ptr->queue, queue) {
		if (&iter->usb_req == _req) {
			udc_req = iter;
			found = true;
			break;
		}
	}

	if (!found) {
		spin_unlock_irqrestore(&crg_udc->udc_lock, flags);
		return -EINVAL;
	}

	if (!udc_req->first_trb) {
		crg_udc_req_done(udc_ep_ptr, udc_req, -ECONNRESET);
		spin_unlock_irqrestore(&crg_udc->udc_lock, flags);
		return 0;
	}

	if (dci == 0) {
		spin_unlock_irqrestore(&crg_udc->udc_lock, flags);
		return -EINVAL;
	}

	if (old_ep_state == EP_STATE_RUNNING) {
		int ret = crg_udc_issue_command(crg_udc, CRG_CMD_STOP_EP,
						BIT(dci), 0);

		if (ret) {
			spin_unlock_irqrestore(&crg_udc->udc_lock, flags);
			return ret;
		}
		if (readl_poll_timeout_atomic(&uccr->ep_running, tmp,
					      !(tmp & BIT(dci)), 10, 10000)) {
			spin_unlock_irqrestore(&crg_udc->udc_lock, flags);
			return -ETIMEDOUT;
		}
		udc_ep_ptr->ep_state = EP_STATE_STOPPED;
	}

	p_ep_cx = &crg_udc->p_epcx[dci - 2];
	deq_pt_lo = le32_to_cpu(p_ep_cx->dw2) & EP_CX_TR_DQPT_LO_MASK;
	deq_pt_hi = le32_to_cpu(p_ep_cx->dw3);
	dq_pt_addr = (dma_addr_t)deq_pt_lo | ((dma_addr_t)deq_pt_hi << 32);
	pause_pt = crg_udc_tran_trb_dma_to_virt(udc_ep_ptr, dq_pt_addr);
	if (!pause_pt) {
		if (old_ep_state == EP_STATE_RUNNING) {
			udc_ep_ptr->ep_state = EP_STATE_RUNNING;
			crg_udc_knock_doorbell(crg_udc, dci);
		}
		spin_unlock_irqrestore(&crg_udc->udc_lock, flags);
		return -EIO;
	}

	if (crg_udc_is_pointer_less_than(pause_pt, udc_req->first_trb,
					 udc_ep_ptr)) {
		crg_udc_squeeze_xfer_ring(udc_ep_ptr, udc_req);
		crg_udc_req_done(udc_ep_ptr, udc_req, -ECONNRESET);
	} else if (udc_req->last_trb &&
		   crg_udc_is_pointer_less_than(udc_req->last_trb, pause_pt,
						udc_ep_ptr)) {
		crg_udc_req_done(udc_ep_ptr, udc_req, -ECONNRESET);
	} else {
		struct crg_udc_request *next_req = NULL;

		udc_req->usb_req.actual =
			crg_udc_actual_before(udc_ep_ptr, udc_req, pause_pt);

		if (!list_is_last(&udc_req->queue, &udc_ep_ptr->queue))
			next_req = list_entry(udc_req->queue.next,
					      struct crg_udc_request, queue);

		if (next_req && next_req->first_trb)
			udc_ep_ptr->deq_pt = next_req->first_trb;
		else
			udc_ep_ptr->deq_pt = udc_ep_ptr->enq_pt;

		if (crg_udc_epcx_update_dqptr(udc_ep_ptr)) {
			spin_unlock_irqrestore(&crg_udc->udc_lock, flags);
			return -EIO;
		}
		crg_udc_req_done(udc_ep_ptr, udc_req, -ECONNRESET);
	}

	if (old_ep_state == EP_STATE_RUNNING) {
		udc_ep_ptr->ep_state = EP_STATE_RUNNING;
		crg_udc_knock_doorbell(crg_udc, dci);
	}

	spin_unlock_irqrestore(&crg_udc->udc_lock, flags);
	return 0;
}

/* endpoint operations */

static int crg_udc_ep_disable(struct usb_ep *ep)
{
	struct crg_udc_ep *udc_ep;
	struct crg_gadget_dev *crg_udc;
	struct crg_ep_cx *p_ep_cx;
	enum crg_ep_state ep_state;
	struct crg_uccr __iomem *uccr;
	unsigned long flags = 0;

	if (!ep)
		return -EINVAL;

	udc_ep = container_of(ep, struct crg_udc_ep, usb_ep);
	crg_udc = udc_ep->crg_udc;

	if (udc_ep->dci == 0)
		return 0;

	spin_lock_irqsave(&crg_udc->udc_lock, flags);

	uccr = crg_udc->uccr;
	p_ep_cx = &crg_udc->p_epcx[udc_ep->dci - 2];

	ep_state = crg_udc_get_ep_state(crg_udc, udc_ep->dci);
	if (ep_state == EP_STATE_DISABLED) {
		/* get here if ep is already disabled */
		spin_unlock_irqrestore(&crg_udc->udc_lock, flags);
		return -EINVAL;
	}

	/* Maybe we need to halt ep before ep disable */

	/*
	 * ep_enable is a write-1-to-disable register: endpoints are enabled
	 * via the CONFIG_EP command, but writing a set bit here disables the
	 * corresponding endpoint.
	 */
	writel(BIT(udc_ep->dci), &uccr->ep_enable);

	udc_ep->desc = NULL;
	udc_ep->comp_desc = NULL;

	/* clean up the request queue */
	crg_udc_ep_nuke(udc_ep, -ESHUTDOWN);

	/* decrement ep counters */
	crg_udc->num_enabled_eps--;
	/* clean up the endpoint context */
	memset(p_ep_cx, 0, sizeof(struct crg_ep_cx));

	/* If device state was changed to default by port
	 * reset, should not overwrite it again
	 */
	if (crg_udc->num_enabled_eps == 0 &&
	    crg_udc->device_state == USB_STATE_CONFIGURED) {
		crg_udc->device_state = USB_STATE_ADDRESS;
	}

	udc_ep->ep_state = EP_STATE_DISABLED;

	spin_unlock_irqrestore(&crg_udc->udc_lock, flags);

	return 0;
}

static int crg_udc_ep_enable(struct usb_ep *ep,
			     const struct usb_endpoint_descriptor *desc)
{
	struct crg_udc_ep *udc_ep;
	struct crg_gadget_dev *crg_udc;
	u32 param0;
	int status;
	unsigned long flags = 0;

	if (!ep || !desc || desc->bDescriptorType != USB_DT_ENDPOINT)
		return -EINVAL;
	udc_ep = container_of(ep, struct crg_udc_ep, usb_ep);
	/* ep0 is always running */
	if (udc_ep->dci == 0)
		return 0;
	crg_udc = udc_ep->crg_udc;
	if (!crg_udc->gadget_driver)
		return -ESHUTDOWN;
	spin_lock_irqsave(&crg_udc->udc_lock, flags);
	/* crg ep context start from ep1 */
	if (crg_udc_get_ep_state(crg_udc, udc_ep->dci) != EP_STATE_DISABLED) {
		int ret;

		spin_unlock_irqrestore(&crg_udc->udc_lock, flags);
		ret = crg_udc_ep_disable(ep);
		if (ret)
			return ret;
		spin_lock_irqsave(&crg_udc->udc_lock, flags);
	}
	udc_ep->desc = desc;
	udc_ep->comp_desc = ep->comp_desc;
	status = crg_udc_validate_ep_desc(udc_ep, desc);
	if (status) {
		udc_ep->desc = NULL;
		udc_ep->comp_desc = NULL;
		spin_unlock_irqrestore(&crg_udc->udc_lock, flags);
		return status;
	}
	/* setup endpoint context for regular endpoint
	 * the endpoint context for control endpoint has been
	 * set up in probe function
	 */
	if (udc_ep->dci) {
		u32 ring_size = 0;
		size_t len;

		if (usb_endpoint_xfer_bulk(desc))
			ring_size = CRG_UDC_BULK_EP_TD_RING_SIZE;
		else if (usb_endpoint_xfer_isoc(desc))
			ring_size = CRG_UDC_ISOC_EP_TD_RING_SIZE;
		else if (usb_endpoint_xfer_int(desc))
			ring_size = CRG_UDC_INT_EP_TD_RING_SIZE;

		if (!ring_size) {
			spin_unlock_irqrestore(&crg_udc->udc_lock, flags);
			return -EINVAL;
		}
		len = ring_size * sizeof(struct crg_transfer_trb);

		if (udc_ep->tran_ring_info.cpu_addr &&
		    udc_ep->tran_ring_info.len != len) {
			void *old_vaddr = udc_ep->tran_ring_info.cpu_addr;
			dma_addr_t old_dma = udc_ep->tran_ring_info.dma_addr;
			size_t old_len = udc_ep->tran_ring_info.len;

			udc_ep->tran_ring_info.cpu_addr = NULL;
			udc_ep->enq_pt = NULL;
			udc_ep->deq_pt = NULL;

			spin_unlock_irqrestore(&crg_udc->udc_lock, flags);
			dma_free_coherent(crg_udc->dev, old_len, old_vaddr,
					  old_dma);
			spin_lock_irqsave(&crg_udc->udc_lock, flags);

			if (udc_ep->desc != desc ||
			    udc_ep->ep_state != EP_STATE_DISABLED) {
				spin_unlock_irqrestore(&crg_udc->udc_lock,
						       flags);
				return -EAGAIN;
			}
		}

		/* setup transfer ring */
		if (!udc_ep->tran_ring_info.cpu_addr) {
			dma_addr_t dma;
			void *vaddr;

			spin_unlock_irqrestore(&crg_udc->udc_lock, flags);
			vaddr = dma_alloc_coherent(crg_udc->dev, len, &dma,
						   GFP_KERNEL);
			if (!vaddr)
				return -ENOMEM;
			spin_lock_irqsave(&crg_udc->udc_lock, flags);
			if (udc_ep->desc != desc ||
			    udc_ep->ep_state != EP_STATE_DISABLED) {
				spin_unlock_irqrestore(&crg_udc->udc_lock,
						       flags);
				dma_free_coherent(crg_udc->dev, len, vaddr,
						  dma);
				return -EAGAIN;
			}
			udc_ep->tran_ring_info.cpu_addr = vaddr;
			udc_ep->tran_ring_info.dma_addr = dma;
			udc_ep->tran_ring_info.len = len;
			udc_ep->first_trb = vaddr;
			udc_ep->last_trb = udc_ep->first_trb + ring_size - 1;
		}
		memset(udc_ep->first_trb, 0, udc_ep->tran_ring_info.len);
		/* Ensure that transfer ring is updated */
		dma_wmb();
		crg_udc_setup_link_trb(udc_ep->last_trb, true,
				       udc_ep->tran_ring_info.dma_addr);
		udc_ep->enq_pt = udc_ep->first_trb;
		udc_ep->deq_pt = udc_ep->first_trb;
		udc_ep->pcs = 1;
		udc_ep->tran_ring_full = false;
		crg_udc->num_enabled_eps++;
		status = crg_udc_epcx_setup(udc_ep);
		if (status)
			goto err_ep_enable;
	}
	param0 = BIT(udc_ep->dci);
	status = crg_udc_issue_command(crg_udc, CRG_CMD_CONFIG_EP, param0, 0);
	if (status)
		goto err_ep_enable;
	if (crg_udc->device_state == USB_STATE_ADDRESS)
		crg_udc->device_state = USB_STATE_CONFIGURED;
	udc_ep->wedge = false;
	udc_ep->usb_ep.enabled = 1;
	udc_ep->ep_state = EP_STATE_RUNNING;
	spin_unlock_irqrestore(&crg_udc->udc_lock, flags);
	return 0;

err_ep_enable:
	if (udc_ep->dci && crg_udc->num_enabled_eps)
		crg_udc->num_enabled_eps--;
	udc_ep->desc = NULL;
	udc_ep->comp_desc = NULL;
	udc_ep->ep_state = EP_STATE_DISABLED;
	spin_unlock_irqrestore(&crg_udc->udc_lock, flags);

	if (udc_ep->tran_ring_info.cpu_addr) {
		dma_free_coherent(crg_udc->dev, udc_ep->tran_ring_info.len,
				  udc_ep->tran_ring_info.cpu_addr,
				  udc_ep->tran_ring_info.dma_addr);
		udc_ep->tran_ring_info.cpu_addr = NULL;
		udc_ep->tran_ring_info.dma_addr = DMA_MAPPING_ERROR;
		udc_ep->tran_ring_info.len = 0;
		udc_ep->first_trb = NULL;
		udc_ep->last_trb = NULL;
	}
	return status;
}

void crg_udc_ep_nuke(struct crg_udc_ep *udc_ep, int status)
{
	struct crg_udc_request *req;
	struct list_head nuke_list;

	list_replace_init(&udc_ep->queue, &nuke_list);

	while (!list_empty(&nuke_list)) {
		req = list_first_entry(&nuke_list, struct crg_udc_request,
				       queue);

		crg_udc_req_done(udc_ep, req, status);
	}
}

static int crg_udc_ep_queue(struct usb_ep *_ep, struct usb_request *_req,
			    gfp_t gfp_flags)
{
	struct crg_udc_request *udc_req_ptr;
	struct crg_udc_ep *udc_ep_ptr;
	struct crg_gadget_dev *crg_udc;
	enum dma_data_direction dma_data_dir;
	const struct usb_endpoint_descriptor *desc;
	unsigned long flags;
	int status;
	bool is_sg;

	if (!_ep || !_req)
		return -EINVAL;

	udc_req_ptr = container_of(_req, struct crg_udc_request, usb_req);
	udc_ep_ptr = container_of(_ep, struct crg_udc_ep, usb_ep);
	crg_udc = udc_ep_ptr->crg_udc;
	is_sg = _req->num_sgs != 0 && _req->length != 0;

	if (is_sg && !_req->sg)
		return -EINVAL;
	if (_req->stream_id)
		return -EOPNOTSUPP;

	spin_lock_irqsave(&crg_udc->udc_lock, flags);
	/*
	 * A completion callback for an aborted EP0 transfer may requeue the
	 * same request. Reject EP0 requests while the previous control
	 * transfer is being aborted or EP0 is in an error state. EP0 becomes
	 * available again only after the new SETUP has been accepted and
	 * the EP0 transfer ring has been successfully reinitialized.
	 */
	if (udc_ep_ptr->dci == 0 && crg_udc->ep0_state != EP0_STATE_IDLE) {
		spin_unlock_irqrestore(&crg_udc->udc_lock, flags);
		return -ECONNRESET;
	}
	if (!udc_ep_ptr->first_trb || !_req->complete ||
	    (!is_sg && !_req->buf) || !list_empty(&udc_req_ptr->queue)) {
		spin_unlock_irqrestore(&crg_udc->udc_lock, flags);
		return -EINVAL;
	}
	if (crg_udc->resetting) {
		spin_unlock_irqrestore(&crg_udc->udc_lock, flags);
		return -ESHUTDOWN;
	}
	if (!udc_ep_ptr->desc) {
		spin_unlock_irqrestore(&crg_udc->udc_lock, flags);
		return -EINVAL;
	}
	desc = udc_ep_ptr->desc;

	if (usb_endpoint_xfer_control(udc_ep_ptr->desc) && is_sg) {
		spin_unlock_irqrestore(&crg_udc->udc_lock, flags);
		return -EOPNOTSUPP;
	}

	if (usb_endpoint_xfer_control(udc_ep_ptr->desc) && !_req->length) {
		if (crg_udc->setup_status == DATA_STAGE_XFER)
			crg_udc->setup_status = STATUS_STAGE_RECV;
		else
			crg_udc->setup_status = STATUS_STAGE_XFER;
		crg_udc_build_ep0_status(&crg_udc->udc_ep[0], false,
					 -EINPROGRESS, udc_req_ptr, 0, 0);
		spin_unlock_irqrestore(&crg_udc->udc_lock, flags);
		return 0;
	}

	if (usb_endpoint_xfer_control(udc_ep_ptr->desc)) {
		if (crg_udc->setup_status == DATA_STAGE_XFER ||
		    crg_udc->setup_status == STATUS_STAGE_XFER) {
			dma_data_dir = DMA_TO_DEVICE;
		} else if (crg_udc->setup_status == DATA_STAGE_RECV ||
			   crg_udc->setup_status == STATUS_STAGE_RECV) {
			dma_data_dir = DMA_FROM_DEVICE;
		} else {
			spin_unlock_irqrestore(&crg_udc->udc_lock, flags);
			return -EINVAL;
		}
	} else {
		dma_data_dir = usb_endpoint_dir_in(udc_ep_ptr->desc) ?
				       DMA_TO_DEVICE :
				       DMA_FROM_DEVICE;
	}
	spin_unlock_irqrestore(&crg_udc->udc_lock, flags);

	crg_udc_clear_req_container(udc_req_ptr);
	udc_req_ptr->mapped = false;

	if (_req->length) {
		udc_req_ptr->map_dir = dma_data_dir;
		status = usb_gadget_map_request_by_dev(
			crg_udc->dev, _req, dma_data_dir == DMA_TO_DEVICE);
		if (status)
			goto err_unmap;

		if (is_sg) {
			if (!_req->num_mapped_sgs ||
			    crg_udc_sg_init(udc_req_ptr)) {
				status = -EIO;
				goto err_unmap;
			}
		} else if (_req->dma_mapped) {
			udc_req_ptr->mapped = true;
		}
	}

	spin_lock_irqsave(&crg_udc->udc_lock, flags);
	if (udc_ep_ptr->desc != desc || !udc_req_ptr->usb_req.complete ||
	    !list_empty(&udc_req_ptr->queue) || crg_udc->resetting) {
		status = -ESHUTDOWN;
		spin_unlock_irqrestore(&crg_udc->udc_lock, flags);
		crg_udc_unmap_request(udc_ep_ptr, udc_req_ptr);
		return status;
	}

	_req->status = -EINPROGRESS;
	_req->actual = 0;

	if (udc_ep_ptr->tran_ring_full)
		status = 0;
	else
		status = crg_udc_build_td(udc_ep_ptr, udc_req_ptr);

	if (!status)
		list_add_tail(&udc_req_ptr->queue, &udc_ep_ptr->queue);
	spin_unlock_irqrestore(&crg_udc->udc_lock, flags);

	if (status)
		crg_udc_unmap_request(udc_ep_ptr, udc_req_ptr);
	return status;

err_unmap:
	crg_udc_unmap_request(udc_ep_ptr, udc_req_ptr);
	return status;
}

static int crg_udc_ep_set_halt(struct usb_ep *_ep, int value)
{
	struct crg_udc_ep *udc_ep_ptr;
	int status;
	unsigned long flags = 0;
	struct crg_gadget_dev *crg_udc;

	if (!_ep)
		return -EINVAL;

	udc_ep_ptr = container_of(_ep, struct crg_udc_ep, usb_ep);
	crg_udc = udc_ep_ptr->crg_udc;

	spin_lock_irqsave(&crg_udc->udc_lock, flags);
	if (!udc_ep_ptr->desc) {
		spin_unlock_irqrestore(&crg_udc->udc_lock, flags);
		return -EINVAL;
	}
	if (value && usb_endpoint_dir_in(udc_ep_ptr->desc) &&
	    !list_empty(&udc_ep_ptr->queue)) {
		dev_err(crg_udc->dev, "set_halt: list not empty\n");
		spin_unlock_irqrestore(&crg_udc->udc_lock, flags);
		return -EAGAIN;
	}

	status = crg_udc_ep_halt(udc_ep_ptr, value, 1, &flags);

	spin_unlock_irqrestore(&crg_udc->udc_lock, flags);

	return status;
}

static int crg_udc_ep_set_wedge(struct usb_ep *_ep)
{
	struct crg_udc_ep *udc_ep_ptr;
	int status;
	unsigned long flags = 0;
	struct crg_gadget_dev *crg_udc;

	if (!_ep)
		return -EINVAL;

	udc_ep_ptr = container_of(_ep, struct crg_udc_ep, usb_ep);
	crg_udc = udc_ep_ptr->crg_udc;

	spin_lock_irqsave(&crg_udc->udc_lock, flags);

	if (!udc_ep_ptr->desc) {
		spin_unlock_irqrestore(&crg_udc->udc_lock, flags);
		return -EINVAL;
	}

	udc_ep_ptr->wedge = 1;

	status = crg_udc_ep_halt(udc_ep_ptr, 1, 1, &flags);

	spin_unlock_irqrestore(&crg_udc->udc_lock, flags);

	return status;
}

static int crg_udc_epcx_setup(struct crg_udc_ep *udc_ep)
{
	struct crg_gadget_dev *crg_udc = udc_ep->crg_udc;
	const struct usb_endpoint_descriptor *desc = udc_ep->desc;
	const struct usb_ss_ep_comp_descriptor *comp_desc = udc_ep->comp_desc;
	u8 dci = udc_ep->dci;
	struct crg_ep_cx *epcx = &crg_udc->p_epcx[dci - 2];
	enum crg_ep_type ep_type;
	u16 maxburst = 0;
	u8 maxstreams = 0;
	u16 maxsize;
	u32 dw;

	/* corigine gadget dir should be opposite to host dir */
	if (usb_endpoint_dir_out(desc))
		ep_type = usb_endpoint_type(desc) + EP_TYPE_INVALID2;
	else
		ep_type = usb_endpoint_type(desc);

	maxsize = usb_endpoint_maxp(desc) & 0x07ff;

	if (crg_udc->gadget.speed >= USB_SPEED_SUPER) {
		if (!comp_desc)
			return -EINVAL;
		maxburst = comp_desc->bMaxBurst;

		if (usb_endpoint_xfer_bulk(udc_ep->desc))
			maxstreams = comp_desc->bmAttributes & 0x1f;

	} else if ((crg_udc->gadget.speed == USB_SPEED_HIGH ||
		    crg_udc->gadget.speed == USB_SPEED_FULL) &&
		   (usb_endpoint_xfer_int(udc_ep->desc) ||
		    usb_endpoint_xfer_isoc(udc_ep->desc))) {
		if (crg_udc->gadget.speed == USB_SPEED_HIGH)
			maxburst = (usb_endpoint_maxp(desc) >> 11) & 0x3;
		if (maxburst == 0x3) {
			dev_err(crg_udc->dev, "invalid maxburst\n");
			maxburst = 0x2;
		}
	}

	/* fill ep_dw0 */
	dw = 0;
	FIELD_MODIFY(EP_CX_LOGICAL_EP_NUM_MASK, &dw, udc_ep->dci / 2);
	FIELD_MODIFY(EP_CX_INTERVAL_MASK, &dw, desc->bInterval);
	if (maxstreams) {
		dev_err(crg_udc->dev, "maxstream=%d is not expected\n",
			maxstreams);
	}
	epcx->dw0 = cpu_to_le32(dw);

	/* fill ep_dw1 */
	dw = 0;
	FIELD_MODIFY(EP_CX_EP_TYPE_MASK, &dw, ep_type);
	FIELD_MODIFY(EP_CX_MAX_PACKET_SIZE_MASK, &dw, maxsize);
	FIELD_MODIFY(EP_CX_MAX_BURST_SIZE_MASK, &dw, maxburst);
	epcx->dw1 = cpu_to_le32(dw);

	/* fill ep_dw2 */
	dw = lower_32_bits(udc_ep->tran_ring_info.dma_addr) &
	     EP_CX_TR_DQPT_LO_MASK;
	FIELD_MODIFY(EP_CX_DEQ_CYC_STATE_MASK, &dw, udc_ep->pcs);
	epcx->dw2 = cpu_to_le32(dw);

	/* fill ep_dw3 */
	dw = upper_32_bits(udc_ep->tran_ring_info.dma_addr);
	epcx->dw3 = cpu_to_le32(dw);
	/* Ensure that epcx is updated */
	dma_wmb();
	return 0;
}

static int crg_udc_epcx_update_dqptr(struct crg_udc_ep *udc_ep)
{
	struct crg_gadget_dev *crg_udc = udc_ep->crg_udc;
	u8 dci = udc_ep->dci;
	struct crg_ep_cx *epcx;
	u32 dw;
	dma_addr_t dqptaddr;
	u32 cmd_param0;

	if (dci < 2 || dci >= CRG_NUM_EP_CX) {
		dev_err(crg_udc->dev, "invalid DCI %u for dequeue pointer\n", dci);
		return -EINVAL;
	}

	epcx = &crg_udc->p_epcx[dci - 2];
	dqptaddr = crg_udc_tran_trb_virt_to_dma(udc_ep, udc_ep->deq_pt);

	if (dqptaddr == DMA_MAPPING_ERROR)
		return -EINVAL;

	/* fill ep_dw2 */
	dw = lower_32_bits(dqptaddr) & EP_CX_TR_DQPT_LO_MASK;
	FIELD_MODIFY(EP_CX_DEQ_CYC_STATE_MASK, &dw, udc_ep->pcs);
	epcx->dw2 = cpu_to_le32(dw);

	/* fill ep_dw3 */
	dw = upper_32_bits(dqptaddr);
	epcx->dw3 = cpu_to_le32(dw);

	cmd_param0 = BIT(udc_ep->dci);
	/* Ensure that dqptr is updated */
	dma_wmb();

	return crg_udc_issue_command(crg_udc, CRG_CMD_SET_TR_DQPTR, cmd_param0, 0);
}

void crg_udc_free_request(struct usb_ep *_ep, struct usb_request *_req)
{
	struct crg_udc_request *udc_req_ptr;

	if (!_ep || !_req)
		return;

	udc_req_ptr = container_of(_req, struct crg_udc_request, usb_req);

	trace_crg_udc_free_request(container_of(_ep, struct crg_udc_ep, usb_ep),
				   udc_req_ptr);

	if (udc_req_ptr->mapped || udc_req_ptr->usb_req.num_mapped_sgs)
		crg_udc_unmap_request(container_of(_ep, struct crg_udc_ep,
						   usb_ep),
				      udc_req_ptr);
	kfree(udc_req_ptr);
}

static int crg_udc_queue_trbs(struct crg_udc_ep *udc_ep_ptr,
			      struct crg_udc_request *udc_req_ptr, bool b_isoc,
			      u32 xfer_ring_size, u32 num_trbs_needed,
			      u64 buffer_length)
{
	struct crg_gadget_dev *crg_udc = udc_ep_ptr->crg_udc;
	struct crg_transfer_trb *p_xfer_ring = udc_ep_ptr->first_trb;
	struct crg_transfer_trb *enq_pt = udc_ep_ptr->enq_pt;
	struct usb_request *usb_req = &udc_req_ptr->usb_req;
	u32 num_trbs_ava, count, i;
	u32 td_trbs;
	u32 td_size;
	u32 j = 1;
	u32 intr_rate = b_isoc ? CRG_UDC_ISOC_EP_INTERRUPT_RATE :
				 CRG_UDC_BULK_EP_INTERRUPT_RATE;
	bool full_td = true;
	bool need_zlp = false;
	bool is_sg = usb_req->num_mapped_sgs;

	if (!b_isoc && usb_req->zero && usb_req->length &&
	    !(usb_req->length % udc_ep_ptr->usb_ep.maxpacket))
		need_zlp = true;

	if (!num_trbs_needed)
		return -EINVAL;

	/* TD_SIZE is five bits and encodes the number of TRBs remaining
	 * in this TD minus one. A request may contain multiple TDs.
	 */
	td_trbs = min_t(u32, num_trbs_needed, 32);

	num_trbs_ava = crg_udc_room_on_ring(crg_udc, xfer_ring_size,
					    p_xfer_ring, udc_ep_ptr->enq_pt,
					    udc_ep_ptr->deq_pt);

	if (num_trbs_ava < td_trbs) {
		if (b_isoc)
			return -EBUSY;
		trace_crg_udc_no_room_on_ring(
			"deferring TRBs to next doorbell");
		count = num_trbs_ava;
		full_td = false;
		udc_ep_ptr->tran_ring_full = true;
		need_zlp = false;
	} else {
		count = td_trbs;
	}

	if (!count)
		return -ENOSPC;

	td_size = count - 1;

	if (is_sg && !udc_req_ptr->sg && crg_udc_sg_init(udc_req_ptr))
		return -EINVAL;

	u8 original_pcs = udc_ep_ptr->pcs;

	for (i = 0; i < count; i++) {
		dma_addr_t trb_buf_addr;
		u32 trb_len, sg_len = 0;
		bool chain_bit = true;
		bool ioc = false;
		bool short_pkt = usb_endpoint_dir_out(udc_ep_ptr->desc);

		if (is_sg) {
			if (!crg_udc_sg_current(udc_req_ptr, &trb_buf_addr,
						&sg_len))
				return -EINVAL;
			if (!sg_len ||
			    dma_mapping_error(crg_udc->dev, trb_buf_addr))
				return -EINVAL;
			trb_len = min_t(
				u64, buffer_length,
				min_t(u32, sg_len,
				      crg_udc_trb_chunk_len(trb_buf_addr,
							    buffer_length)));
		} else {
			if (buffer_length)
				trb_buf_addr = usb_req->dma + (usb_req->length -
							       buffer_length);
			else
				trb_buf_addr = 0;
			trb_len = crg_udc_trb_chunk_len(trb_buf_addr,
							buffer_length);
		}

		if (!trb_len && buffer_length)
			return -EINVAL;

		buffer_length -= trb_len;

		if (i == count - 1) {
			chain_bit = false;
			ioc = true;
			if (!buffer_length)
				udc_req_ptr->all_trbs_queued = true;
		}

		if (!full_td && j == intr_rate) {
			ioc = true;
			j = 0;
		}

		if (b_isoc) {
			crg_udc_setup_trb(
				enq_pt, trb_len, trb_buf_addr, td_size,
				i == 0 ? (original_pcs ^ 1) : udc_ep_ptr->pcs,
				TRB_TYPE_XFER_DATA_ISOCH, short_pkt, chain_bit,
				ioc, false, 0, true, 0, 1, 0);
		} else {
			bool azp = ioc && !chain_bit && need_zlp;

			crg_udc_setup_trb(
				enq_pt, trb_len, trb_buf_addr, td_size,
				i == 0 ? (original_pcs ^ 1) : udc_ep_ptr->pcs,
				TRB_TYPE_XFER_NORMAL, short_pkt, chain_bit, ioc,
				false, 0, false, 0, 0, azp);
		}

		trace_crg_udc_queue_trb(udc_ep_ptr, enq_pt);

		if (is_sg && !crg_udc_sg_advance(udc_req_ptr, trb_len))
			return -EINVAL;

		td_size--;
		enq_pt++;
		j++;

		if (enq_pt == udc_ep_ptr->last_trb) {
			dma_wmb();
			le32p_replace_bits(&enq_pt->dw3, udc_ep_ptr->pcs,
					   TRB_CYCLE_BIT_MASK);
			udc_ep_ptr->pcs ^= 1;
			enq_pt = udc_ep_ptr->first_trb;
		}
	}

	dma_wmb();
	le32p_replace_bits(&udc_ep_ptr->enq_pt->dw3, original_pcs,
			   TRB_CYCLE_BIT_MASK);

	if (!udc_req_ptr->trbs_needed)
		udc_req_ptr->first_trb = udc_ep_ptr->enq_pt;

	udc_ep_ptr->enq_pt = enq_pt;
	if (count == num_trbs_ava)
		udc_ep_ptr->tran_ring_full = true;
	udc_req_ptr->buff_len_left = buffer_length;
	if (buffer_length)
		udc_req_ptr->trbs_needed = num_trbs_needed - count;
	else
		udc_req_ptr->trbs_needed = 0;

	if (!udc_req_ptr->buff_len_left) {
		if (udc_ep_ptr->enq_pt == udc_req_ptr->first_trb)
			udc_req_ptr->last_trb = udc_ep_ptr->last_trb - 1;
		else
			udc_req_ptr->last_trb = udc_ep_ptr->enq_pt - 1;
	}

	return 0;
}

static int crg_udc_reinit(struct crg_gadget_dev *crg_udc)
{
	struct crg_uccr __iomem *uccr = crg_udc->uccr;
	u32 i, tmp;
	struct crg_udc_ep *udc_ep_ptr;

	crg_udc->setup_status = WAIT_FOR_SETUP;
	/* Base on Figure 9-1, default USB_STATE is attached */
	crg_udc->device_state = USB_STATE_RECONNECTING;

	/* halt all the endpoints */

	/* disable all the endpoints */
	tmp = readl(&uccr->ep_enable);
	writel(tmp, &uccr->ep_enable);
	if (readl_poll_timeout_atomic(&uccr->ep_enable, tmp, !tmp, 1, 50)) {
		dev_dbg(crg_udc->dev,
			"timeout waiting for all endpoints to disable\n");
		return -ETIMEDOUT;
	}

	for (i = 2; i < CRG_NUM_EP_CX; i++) {
		udc_ep_ptr = &crg_udc->udc_ep[i];
		udc_ep_ptr->usb_ep.enabled = 0;
		if (udc_ep_ptr->desc) {
			udc_ep_ptr->desc = NULL;
			udc_ep_ptr->comp_desc = NULL;
			crg_udc_ep_nuke(udc_ep_ptr, -ESHUTDOWN);
		}
		udc_ep_ptr->tran_ring_full = false;
		udc_ep_ptr->ep_state = EP_STATE_DISABLED;
	}
	crg_udc->num_enabled_eps = 0;

	/* we don't handle ep0 here, we crg_udc_init_ep0 when event ring is empty */

	if (crg_udc->dev_addr != 0) {
		u32 param0;

		param0 = FIELD_PREP(CRG_CMD2_0_DEV_ADDR_MASK, 0);
		if (crg_udc_issue_command(crg_udc, CRG_CMD_SET_ADDR, param0, 0))
			return -EIO;
		crg_udc->dev_addr = 0;
	}

	crg_udc_clear_portpm(crg_udc);

	return 0;
}

void crg_udc_set_pullup(struct crg_gadget_dev *crg_udc, bool is_on)
{
	struct crg_uccr __iomem *uccr = crg_udc->uccr;
	u32 val;

	if (is_on) {
		/* Enable event reporting and interrupts, then assert RUN */
		val = readl(&uccr->config1);
		val |= (CRG_U3DC_CFG1_CSC_EVENT_EN |
			CRG_U3DC_CFG1_PEC_EVENT_EN |
			CRG_U3DC_CFG1_PPC_EVENT_EN |
			CRG_U3DC_CFG1_PRC_EVENT_EN |
			CRG_U3DC_CFG1_PLC_EVENT_EN |
			CRG_U3DC_CFG1_CEC_EVENT_EN);
		writel(val, &uccr->config1);

		val = readl(&uccr->control);
		val |= (CRG_U3DC_CTRL_SYSERR_EN | CRG_U3DC_CTRL_INT_EN |
			CRG_U3DC_CTRL_RUN);
		writel(val, &uccr->control);
	} else {
		val = readl(&uccr->control);
		val &= ~CRG_U3DC_CTRL_RUN;
		writel(val, &uccr->control);
	}
}

void crg_udc_set_vbus_detect(struct crg_gadget_dev *crg_udc, bool enable)
{
	u32 val = readl(crg_udc->mmio_virt_base + CRG_UDC_VENDOR_REG);

	if (enable)
		val |= CRG_UDC_VBUS_DETECT;
	else
		val &= ~CRG_UDC_VBUS_DETECT;

	writel(val, crg_udc->mmio_virt_base + CRG_UDC_VENDOR_REG);
}

static bool crg_udc_sg_advance(struct crg_udc_request *udc_req, u32 bytes)
{
	struct scatterlist *sg = udc_req->sg;
	unsigned int i = udc_req->sg_index;

	while (bytes) {
		u32 len;

		if (!sg || i >= udc_req->usb_req.num_sgs)
			return false;

		len = sg_dma_len(sg);
		if (!len || udc_req->sg_offset == len) {
			sg = sg_next(sg);
			i++;
			udc_req->sg_offset = 0;
			continue;
		}

		len -= udc_req->sg_offset;
		if (bytes < len) {
			udc_req->sg_offset += bytes;
			bytes = 0;
			break;
		}

		bytes -= len;
		sg = sg_next(sg);
		i++;
		udc_req->sg_offset = 0;
	}

	udc_req->sg = sg;
	udc_req->sg_index = i;
	return true;
}

static bool crg_udc_sg_current(struct crg_udc_request *udc_req, dma_addr_t *dma,
			       u32 *len)
{
	struct scatterlist *sg = udc_req->sg;

	while (sg && udc_req->sg_index < udc_req->usb_req.num_sgs &&
	       !sg_dma_len(sg)) {
		sg = sg_next(sg);
		udc_req->sg_index++;
		udc_req->sg_offset = 0;
	}
	udc_req->sg = sg;

	if (!sg || udc_req->sg_index >= udc_req->usb_req.num_sgs)
		return false;

	if (udc_req->sg_offset >= sg_dma_len(sg))
		return false;

	*dma = sg_dma_address(sg) + udc_req->sg_offset;
	*len = sg_dma_len(sg) - udc_req->sg_offset;
	return true;
}

static int crg_udc_sg_init(struct crg_udc_request *udc_req)
{
	struct scatterlist *sg;
	unsigned int i;

	for_each_sg(udc_req->usb_req.sg, sg, udc_req->usb_req.num_sgs, i) {
		if (sg_dma_len(sg)) {
			udc_req->sg = sg;
			udc_req->sg_index = i;
			udc_req->sg_offset = 0;
			return 0;
		}
	}

	return -EINVAL;
}

/*
 * Undo whatever crg_udc_ep_queue() mapped for this request: the merged
 * SG list, the single buffer, or neither if the request carried a
 * pre-mapped DMA address.
 */
void crg_udc_unmap_request(struct crg_udc_ep *udc_ep,
			   struct crg_udc_request *udc_req)
{
	struct crg_gadget_dev *crg_udc = udc_ep->crg_udc;
	struct usb_request *req = &udc_req->usb_req;
	bool is_in = udc_req->map_dir == DMA_TO_DEVICE;

	if (req->num_mapped_sgs || req->dma_mapped)
		usb_gadget_unmap_request_by_dev(crg_udc->dev, req, is_in);

	if (udc_req->mapped) {
		req->dma = DMA_MAPPING_ERROR;
		udc_req->mapped = false;
	}

	udc_req->sg = NULL;
	udc_req->sg_index = 0;
	udc_req->sg_offset = 0;
}

static int crg_udc_validate_ep_desc(struct crg_udc_ep *udc_ep,
				    const struct usb_endpoint_descriptor *desc)
{
	const struct usb_ss_ep_comp_descriptor *comp = udc_ep->comp_desc;
	u16 maxpacket = usb_endpoint_maxp(desc);

	if (usb_endpoint_xfer_control(desc) || !maxpacket ||
	    maxpacket > CRG_MAX_PACKET_SIZE)
		return -EINVAL;

	if (crg_udc_index2dci(desc->bEndpointAddress) != udc_ep->dci)
		return -EINVAL;

	if (usb_endpoint_xfer_bulk(desc) && comp && usb_ss_max_streams(comp))
		return -EOPNOTSUPP;

	if (usb_endpoint_xfer_isoc(desc) && desc->bInterval == 0)
		return -EINVAL;

	if (udc_ep->crg_udc->gadget.speed >= USB_SPEED_SUPER) {
		if (!comp || comp->bMaxBurst > 15)
			return -EINVAL;
		if (usb_endpoint_xfer_isoc(desc) &&
		    ((comp->bmAttributes & 0x3) > 2))
			return -EINVAL;
	} else if (udc_ep->crg_udc->gadget.speed == USB_SPEED_HIGH) {
		if (desc->bInterval > 16 && (usb_endpoint_xfer_int(desc) ||
					     usb_endpoint_xfer_isoc(desc)))
			return -EINVAL;
		if (usb_endpoint_xfer_isoc(desc) &&
		    usb_endpoint_maxp_mult(desc) > 3)
			return -EINVAL;
	} else if (udc_ep->crg_udc->gadget.speed == USB_SPEED_FULL) {
		if (maxpacket > 64 && !usb_endpoint_xfer_isoc(desc))
			return -EINVAL;
		if (usb_endpoint_xfer_isoc(desc) && maxpacket > 1023)
			return -EINVAL;
	}

	return 0;
}

static bool crg_udc_vbus_connected(struct crg_gadget_dev *crg_udc)
{
	return !!(readl(&crg_udc->uccr->portsc) & CRG_U3DC_PORTSC_PP);
}

static void crg_ep_struct_setup(struct crg_gadget_dev *crg_udc, u32 dci,
				const char *name)
{
	struct crg_udc_ep *ep = &crg_udc->udc_ep[dci];

	ep->dci = dci;

	if (ep->dci > 1) {
		strscpy_pad(ep->name, name, sizeof(ep->name));
		ep->usb_ep.name = ep->name;

		ep->usb_ep.caps.type_iso = 1;
		ep->usb_ep.caps.type_bulk = 1;
		ep->usb_ep.caps.type_int = 1;
		if (ep->dci % 2)
			ep->usb_ep.caps.dir_out = 1;
		else
			ep->usb_ep.caps.dir_in = 1;
		usb_ep_set_maxpacket_limit(&ep->usb_ep, CRG_MAX_PACKET_SIZE);
	} else {
		strscpy(ep->name, "ep0", sizeof(ep->name));
		ep->usb_ep.name = ep->name;
		ep->usb_ep.maxpacket = 512;
		ep->usb_ep.caps.type_control = 1;
		ep->usb_ep.caps.dir_in = 1;
		ep->usb_ep.caps.dir_out = 1;
	}

	ep->usb_ep.ops = &crg_udc_ep_ops;
	ep->crg_udc = crg_udc;

	INIT_LIST_HEAD(&ep->queue);
	if (ep->dci > 1)
		list_add_tail(&ep->usb_ep.ep_list, &crg_udc->gadget.ep_list);
}

void crg_udc_reset_work(struct work_struct *work)
{
	struct crg_gadget_dev *crg_udc =
		container_of(work, struct crg_gadget_dev, udc_reset_work);

	if (crg_udc->gadget_driver)
		usb_gadget_udc_reset(&crg_udc->gadget, crg_udc->gadget_driver);
}

int crg_udc_handle_port_status(struct crg_gadget_dev *crg_udc,
			       unsigned long *flags)
{
	struct crg_uccr __iomem *uccr = crg_udc->uccr;
	bool was_connected = crg_udc->connected;
	u32 portsc_val;
	u32 tmp;
	u32 speed;

	/* handle Port Reset */
	portsc_val = readl(&uccr->portsc);
	trace_crg_udc_portsc(portsc_val);
	writel(portsc_val & CRG_U3DC_PORTSC_W1C_MASK, &uccr->portsc);

	tmp = readl(&uccr->portsc);

	if (portsc_val & CRG_U3DC_PORTSC_PRC) {
		tmp = readl(&uccr->portsc);
		if (tmp & CRG_U3DC_PORTSC_PRC) {
			dev_dbg(crg_udc->dev, "PRC is still set\n");
		} else if (tmp & CRG_U3DC_PORTSC_PR) {
			/* first port status change event for port reset */
			dev_dbg(crg_udc->dev,
				"PRC is not set, but PR is set\n");
		} else {
			if (FIELD_GET(CRG_U3DC_PORTSC_PLS_MASK, tmp) != 0 ||
			    (!(tmp & CRG_U3DC_PORTSC_PED))) {
				dev_dbg(crg_udc->dev, "portsc=0x%x no PED\n",
					tmp);
				return 0;
			}

			switch (FIELD_GET(CRG_U3DC_PORTSC_SPEED_MASK, tmp)) {
			case CRG_U3DC_PORTSC_SPEED_SSP:
				speed = USB_SPEED_SUPER_PLUS;
				break;
			case CRG_U3DC_PORTSC_SPEED_SS:
				speed = USB_SPEED_SUPER;
				break;
			case CRG_U3DC_PORTSC_SPEED_HS:
				speed = USB_SPEED_HIGH;
				break;

			case CRG_U3DC_PORTSC_SPEED_FS:
				speed = USB_SPEED_FULL;
				break;

			case CRG_U3DC_PORTSC_SPEED_LS:
				speed = USB_SPEED_LOW;
				break;
			default:
				speed = USB_SPEED_UNKNOWN;
				return 0;
			}

			if (crg_udc->device_state >= USB_STATE_DEFAULT) {
				/*
				 * A single enumeration commonly involves two
				 * bus resets (an initial one, then another
				 * after SET_ADDRESS before SET_CONFIGURATION).
				 * By the second reset device_state is already
				 * USB_STATE_ADDRESS, which is >= DEFAULT but
				 * has no active configuration to tear down.
				 * Only notify composite when a configuration
				 * was actually active: otherwise this races
				 * with the SET_CONFIGURATION that's about to
				 * arrive, since the deferred work below can
				 * run after fsg_set_alt() has already raised
				 * its own FSG_STATE_CONFIG_CHANGE exception
				 * and reset_config()'s ->disable() would
				 * overwrite that exception's argument back to
				 * NULL before the function's thread consumes
				 * it, silently skipping the real enable.
				 */
				bool was_configured = crg_udc->device_state ==
						       USB_STATE_CONFIGURED;
				int reinit_ret;

				reinit_ret = crg_udc_reinit(crg_udc);
				if (reinit_ret) {
					dev_dbg(crg_udc->dev,
						"bus reset reinit failed: %d\n",
						reinit_ret);
					return reinit_ret;
				}

				/*
				 * Notify the function/composite driver of the
				 * bus reset so it tears down the active
				 * configuration (usb_gadget_udc_reset() ->
				 * ->reset()). Without this, a host that
				 * re-sends the same SET_CONFIGURATION value
				 * after a reset can see composite treat it as
				 * a no-op and skip re-running set_alt(), even
				 * though every endpoint was just disabled in
				 * software above.
				 *
				 * This must not be called synchronously here:
				 * usb_gadget_udc_reset() re-enters the
				 * function drivers, which requires dropping
				 * udc_lock. Doing that mid-event-processing
				 * would let a second port-status event run
				 * crg_udc_reinit() again while this event's
				 * SET_CONFIGURATION/set_alt() work is still
				 * landing on another thread, tearing the
				 * just-enabled endpoints back down before
				 * they can be used. Defer it to a work item
				 * so this function never has to drop the
				 * lock.
				 */
				if (was_configured && crg_udc->gadget_driver)
					schedule_work(&crg_udc->udc_reset_work);
			}

			crg_udc->gadget.speed = speed;

			crg_udc_update_ep0_maxpacketsize(crg_udc);

			crg_udc->connected = 1;

			if (crg_udc->device_state < USB_STATE_RECONNECTING)
				crg_udc_enable_setup(crg_udc);
		}
	}
	/* handle Port Reset end */

	/* handle Port Connection Change */
	tmp = readl(&uccr->portsc);
	if ((tmp & (CRG_U3DC_PORTSC_CCS | CRG_U3DC_PORTSC_PP)) ==
	    (CRG_U3DC_PORTSC_CCS | CRG_U3DC_PORTSC_PP)) {
		if (FIELD_GET(CRG_U3DC_PORTSC_PLS_MASK, tmp) != 0 ||
		    (!(tmp & CRG_U3DC_PORTSC_PED)))
			return 0;

		switch (FIELD_GET(CRG_U3DC_PORTSC_SPEED_MASK, tmp)) {
		case CRG_U3DC_PORTSC_SPEED_SSP:
			speed = USB_SPEED_SUPER_PLUS;
			break;
		case CRG_U3DC_PORTSC_SPEED_SS:
			speed = USB_SPEED_SUPER;
			break;
		case CRG_U3DC_PORTSC_SPEED_HS:
			speed = USB_SPEED_HIGH;
			break;

		case CRG_U3DC_PORTSC_SPEED_FS:
			speed = USB_SPEED_FULL;
			break;

		case CRG_U3DC_PORTSC_SPEED_LS:
			speed = USB_SPEED_LOW;
			break;
		default:
			speed = USB_SPEED_UNKNOWN;
			return 0;
		}
		crg_udc->gadget.speed = speed;

		crg_udc_update_ep0_maxpacketsize(crg_udc);

		crg_udc->connected = 1;

		if (crg_udc->device_state < USB_STATE_RECONNECTING)
			crg_udc_enable_setup(crg_udc);

	} else if (!(tmp & CRG_U3DC_PORTSC_CCS)) {
		bool cable_connected;
		int ccs_drop_ignore = 0;

		if ((FIELD_GET(CRG_U3DC_PORTSC_PLS_MASK, tmp) == 0x0) &&
		    (FIELD_GET(CRG_U3DC_PORTSC_SPEED_MASK, tmp) <
		     CRG_U3DC_PORTSC_SPEED_SS)) {
			ccs_drop_ignore = 1;
			dev_err(crg_udc->dev, "ccs glitch detect on HS/FS!\n");
		}

		if (!ccs_drop_ignore)
			crg_udc->gadget.speed = USB_SPEED_UNKNOWN;

		cable_connected = crg_udc_vbus_connected(crg_udc);
		if (cable_connected && !ccs_drop_ignore) {
			crg_udc->device_state = USB_STATE_POWERED;
			crg_udc_reinit(crg_udc);
			crg_udc->connected = 0;
		} else if (!cable_connected) {
			dev_dbg(crg_udc->dev,
				"cable disconnected, rst controller\n");
			if (was_connected)
				crg_udc_notify_disconnect(crg_udc, flags);
			if (crg_udc_reset(crg_udc, flags))
				dev_err(crg_udc->dev,
					"controller reset failed on disconnect\n");
			crg_udc_reset_data_struct(crg_udc);
			crg_udc->connected = 0;
			crg_udc_init_ep0(crg_udc);

			crg_udc->device_state = USB_STATE_ATTACHED;
			return -ECONNRESET;
		}
	}
	/* handle Port Connection Change end*/

	if (portsc_val & CRG_U3DC_PORTSC_PLC) {
		tmp = readl(&uccr->portsc);
		if (FIELD_GET(CRG_U3DC_PORTSC_PLS_MASK, tmp) == 0xf) {
			FIELD_MODIFY(CRG_U3DC_PORTSC_PLS_MASK, &tmp, 0x0);
			writel(tmp, &uccr->portsc);
		}
	}

	return 0;
}

int crg_udc_handle_xfer_event(struct crg_gadget_dev *crg_udc,
			      struct crg_event_trb *event, unsigned long *flags)
{
	u32 dw3 = le32_to_cpu(event->dw3);
	u8 dci = FIELD_GET(EVE_TRB_ENDPOINT_ID_MASK, dw3);
	u16 comp_code =
		FIELD_GET(EVE_TRB_COMPL_CODE_MASK, le32_to_cpu(event->dw2));
	struct crg_udc_ep *udc_ep_ptr;
	struct crg_udc_request *udc_req_ptr;
	struct crg_transfer_trb *trb;
	bool trbs_dequeued = false;
	u32 residual;

	if (dci >= CRG_NUM_EP_CX || dci == 1)
		return -EINVAL;

	udc_ep_ptr = &crg_udc->udc_ep[dci];
	if (!udc_ep_ptr->first_trb ||
	    crg_udc_get_ep_state(crg_udc, dci) == EP_STATE_DISABLED)
		return -ENODEV;

	if (comp_code != CMPL_CODE_STOPPED &&
	    comp_code != CMPL_CODE_STOPPED_LENGTH_INVALID &&
	    comp_code != CMPL_CODE_DISABLED &&
	    comp_code != CMPL_CODE_DISABLED_LENGTH_INVALID &&
	    comp_code != CMPL_CODE_HALTED &&
	    comp_code != CMPL_CODE_HALTED_LENGTH_INVALID)
		crg_udc_update_dequeue_pt(event, udc_ep_ptr);

	if (is_request_dequeued(crg_udc, udc_ep_ptr, event)) {
		trbs_dequeued = true;
		goto queue_more_trbs;
	}

	switch (comp_code) {
	case CMPL_CODE_SUCCESS:
		trb = crg_udc_tran_trb_dma_to_virt(
			udc_ep_ptr,
			(dma_addr_t)le32_to_cpu(event->dw0) |
				((dma_addr_t)le32_to_cpu(event->dw1) << 32));
		if (!trb || list_empty(&udc_ep_ptr->queue))
			break;

		/*
		 * A control transfer has separate data-stage and status-stage
		 * TRBs.  The status request is an internal zero-length request
		 * and its successful completion must not go through
		 * crg_udc_build_td().  Doing that attempts to queue another
		 * control TD while the status request is already on the EP0
		 * software queue, leaving it stuck in -EINPROGRESS.  The next
		 * SETUP then sees a non-empty queue and enumeration fails.
		 */
		if (FIELD_GET(TRB_TYPE_MASK, le32_to_cpu(trb->dw3)) ==
		    TRB_TYPE_XFER_STATUS_STAGE) {
			udc_req_ptr = list_first_entry(&udc_ep_ptr->queue,
						       struct crg_udc_request,
						       queue);
			if (dci == 0) {
				udc_req_ptr->usb_req.actual = 0;
				trbs_dequeued = true;
				crg_udc_req_done(udc_ep_ptr, udc_req_ptr, 0);
				crg_udc_ep0_req_complete(udc_ep_ptr, flags);
			} else {
				dev_err(crg_udc->dev,
					"status-stage TRB on endpoint %u\n",
					dci);
			}
			break;
		}

		if (!FIELD_GET(TRB_CHAIN_BIT_MASK, le32_to_cpu(trb->dw3))) {
			udc_req_ptr = list_first_entry(&udc_ep_ptr->queue,
						       struct crg_udc_request,
						       queue);
			trbs_dequeued = true;

			if (udc_req_ptr->all_trbs_queued) {
				udc_req_ptr->usb_req.actual =
					udc_req_ptr->usb_req.length -
					udc_req_ptr->buff_len_left;
				residual = FIELD_GET(EVE_TRB_TRAN_LEN_MASK,
						     le32_to_cpu(event->dw2));
				if (residual > udc_req_ptr->usb_req.actual)
					residual = 0;
				udc_req_ptr->usb_req.actual -= residual;
				crg_udc_req_done(udc_ep_ptr, udc_req_ptr, 0);

				if (dci == 0 && udc_ep_ptr->desc &&
				    usb_endpoint_xfer_control(udc_ep_ptr->desc))
					crg_udc_ep0_req_complete(udc_ep_ptr,
								 flags);
			} else {
				int ret;

				ret = crg_udc_build_td(udc_ep_ptr, udc_req_ptr);
				if (ret) {
					udc_req_ptr->usb_req.actual =
						udc_req_ptr->usb_req.length -
						udc_req_ptr->buff_len_left;
					crg_udc_req_done(udc_ep_ptr, udc_req_ptr,
							 -EIO);
				} else {
					crg_udc_knock_doorbell(crg_udc, dci);
				}
			}
		}
		break;

	case CMPL_CODE_SHORT_PKT:
		if (list_empty(&udc_ep_ptr->queue))
			break;

		udc_req_ptr = list_first_entry(&udc_ep_ptr->queue,
					       struct crg_udc_request, queue);
		udc_req_ptr->usb_req.actual = udc_req_ptr->usb_req.length -
					      udc_req_ptr->buff_len_left;
		residual = FIELD_GET(EVE_TRB_TRAN_LEN_MASK,
				     le32_to_cpu(event->dw2));
		if (residual > udc_req_ptr->usb_req.actual)
			residual = 0;
		udc_req_ptr->usb_req.actual -= residual;

		if (udc_req_ptr->usb_req.short_not_ok &&
		    usb_endpoint_dir_out(udc_ep_ptr->desc))
			crg_udc_req_done(udc_ep_ptr, udc_req_ptr, -EREMOTEIO);
		else
			crg_udc_req_done(udc_ep_ptr, udc_req_ptr, 0);

		trbs_dequeued = true;
		crg_udc_advance_dequeue_pt(udc_ep_ptr);
		break;

	case CMPL_CODE_PROTOCOL_STALL:
	case CMPL_CODE_TRB_STALL:
		if (!list_empty(&udc_ep_ptr->queue)) {
			udc_req_ptr = list_first_entry(&udc_ep_ptr->queue,
						       struct crg_udc_request,
						       queue);
			crg_udc_req_done(udc_ep_ptr, udc_req_ptr, -EPIPE);
			trbs_dequeued = true;
		}
		if (dci == 0)
			crg_udc->setup_status = WAIT_FOR_SETUP;
		break;

	case CMPL_CODE_SETUP_TAG_MISMATCH:
		/*
		 * EP0 SETUP changes are serialized by the SETUP event handler.
		 * When a new SETUP arrives, crg_udc_abort_ep0_for_setup() stops
		 * EP0, gives back the old request, resets the EP0 ring, and then
		 * accepts the new SETUP. A SETUP_TAG_MISMATCH event generated
		 * afterwards therefore belongs to the aborted control transfer.
		 *
		 * Do not complete the current EP0 request or change setup_status
		 * here: doing so could operate on the new control transfer and
		 * recreate the EP0 queue/state race this path is intended to avoid.
		 */
		if (dci == 0) {
			dev_dbg(crg_udc->dev,
				"ignoring stale EP0 SETUP_TAG_MISMATCH\n");
			break;
		}

		/* This completion code is meaningful only for EP0. */
		dev_warn(crg_udc->dev,
			 "SETUP_TAG_MISMATCH on non-EP0 endpoint %u\n", dci);
		break;

	case CMPL_CODE_BABBLE_DETECTED_ERR:
	case CMPL_CODE_DATA_BUFFER_ERR:
	case CMPL_CODE_INVALID_STREAM_TYPE_ERR:
	case CMPL_CODE_RING_UNDERRUN:
	case CMPL_CODE_RING_OVERRUN:
	case CMPL_CODE_EVENT_RING_FULL_ERR:
	case CMPL_CODE_ISOCH_BUFFER_OVERRUN:
	case CMPL_CODE_USB_TRANS_ERR:
	case CMPL_CODE_TRB_ERR:
		dev_err(crg_udc->dev, "transfer error, completion code 0x%x\n",
			comp_code);
		if (!list_empty(&udc_ep_ptr->queue)) {
			udc_req_ptr = list_first_entry(&udc_ep_ptr->queue,
						       struct crg_udc_request,
						       queue);
			crg_udc_req_done(udc_ep_ptr, udc_req_ptr, -EIO);
			trbs_dequeued = true;
		}
		if (dci)
			set_ep_halt(crg_udc, dci, flags);
		break;

	case CMPL_CODE_STOPPED:
	case CMPL_CODE_STOPPED_LENGTH_INVALID:
	case CMPL_CODE_HALTED:
	case CMPL_CODE_HALTED_LENGTH_INVALID:
	case CMPL_CODE_DISABLED:
	case CMPL_CODE_DISABLED_LENGTH_INVALID:
		break;

	default:
		dev_err(crg_udc->dev,
			"unsupported transfer completion code 0x%x\n",
			comp_code);
		if (!list_empty(&udc_ep_ptr->queue)) {
			udc_req_ptr = list_first_entry(&udc_ep_ptr->queue,
						       struct crg_udc_request,
						       queue);
			crg_udc_req_done(udc_ep_ptr, udc_req_ptr, -EIO);
			trbs_dequeued = true;
		}
		break;
	}

queue_more_trbs:
	if (trbs_dequeued && udc_ep_ptr->tran_ring_full) {
		udc_ep_ptr->tran_ring_full = false;
		if (crg_udc_queue_pending_trbs(udc_ep_ptr))
			udc_ep_ptr->tran_ring_full = true;
	}

	return 0;
}

int crg_udc_handle_event(struct crg_gadget_dev *crg_udc,
			 struct crg_event_trb *event, unsigned long *flags)
{
	int ret;

	trace_crg_udc_event_trb(event);

	switch (FIELD_GET(EVE_TRB_TYPE_MASK, le32_to_cpu(event->dw3))) {
	case TRB_TYPE_EVT_PORT_STATUS_CHANGE:
		if (crg_udc->device_state == USB_STATE_RECONNECTING) {
			crg_udc->portsc_on_reconnecting = 1;
			break;
		}
		/* 1.Port Reset  2.Port Connection Change 3.Port Link Change */
		ret = crg_udc_handle_port_status(crg_udc, flags);
		if (ret)
			return ret;

		break;
	case TRB_TYPE_EVT_TRANSFER:
		if (crg_udc->device_state < USB_STATE_RECONNECTING) {
			dev_err(crg_udc->dev,
				"Xfer compl event rcved when dev state=%d !\n",
				crg_udc->device_state);
			break;
		}

		ret = crg_udc_handle_xfer_event(crg_udc, event, flags);
		if (ret)
			return ret;
		break;
	case TRB_TYPE_EVT_SETUP_PKT: {
		struct usb_ctrlrequest setup;
		u32 dw0 = le32_to_cpu(event->dw0);
		u32 dw1 = le32_to_cpu(event->dw1);
		u8 setup_tag;

		BUILD_BUG_ON(sizeof(struct usb_ctrlrequest) != 8);
		setup.bRequestType = dw0 & 0xff;
		setup.bRequest = (dw0 >> 8) & 0xff;
		setup.wValue = cpu_to_le16((dw0 >> 16) & 0xffff);
		setup.wIndex = cpu_to_le16(dw1 & 0xffff);
		setup.wLength = cpu_to_le16((dw1 >> 16) & 0xffff);

		setup_tag = FIELD_GET(EVE_TRB_SETUP_TAG_MASK,
				      le32_to_cpu(event->dw3));
		if (crg_udc->setup_status != WAIT_FOR_SETUP) {
			/* A new SETUP aborts the previous control transfer. The old
			 * EP0 request must be completed and its TRBs discarded before
			 * the new setup is handed to the gadget driver.
			 */
			if (crg_udc_abort_ep0_for_setup(crg_udc)) {
				crg_udc_set_ep0_halt(crg_udc);
				break;
			}
		}

		crg_udc_handle_setup_pkt(crg_udc, &setup, setup_tag, flags);

		break;
	}
	default:
		dev_dbg(crg_udc->dev, "unexpected TRB_TYPE = 0x%lx",
			FIELD_GET(EVE_TRB_TYPE_MASK, le32_to_cpu(event->dw3)));
		break;
	}

	return 0;
}

/*
 * Invoke the bound gadget driver's ->disconnect() callback, if any.
 *
 * The gadget disconnect callback may be called in interrupt context.
 * Release only udc_lock while keeping local IRQs disabled.
 */
void crg_udc_notify_disconnect(struct crg_gadget_dev *crg_udc,
			       unsigned long *flags)
{
	/* Report link loss even when there is no gadget callback. */
	usb_gadget_set_state(&crg_udc->gadget, USB_STATE_NOTATTACHED);

	if (!crg_udc->async_callbacks || !crg_udc->gadget_driver ||
	    !crg_udc->gadget_driver->disconnect)
		return;

	spin_unlock_irqrestore(&crg_udc->udc_lock, *flags);
	crg_udc->gadget_driver->disconnect(&crg_udc->gadget);
	spin_lock_irqsave(&crg_udc->udc_lock, *flags);
}

int crg_udc_ep_halt(struct crg_udc_ep *udc_ep_ptr, int halt, int ignore_wedge,
		    unsigned long *flags)
{
	struct crg_gadget_dev *crg_udc = udc_ep_ptr->crg_udc;
	struct crg_uccr __iomem *uccr = crg_udc->uccr;
	enum crg_ep_state ep_state;
	bool reset_seq_only = false;
	int do_halt;
	u32 param0;
	u32 tmp;

	if (!udc_ep_ptr->desc) {
		dev_err(crg_udc->dev, "NULL desc\n");
		return -EINVAL;
	}

	if (usb_endpoint_xfer_isoc(udc_ep_ptr->desc)) {
		dev_err(crg_udc->dev, "Isoc ep, halt not supported\n");
		return -EOPNOTSUPP;
	}

	if (udc_ep_ptr->dci == 0)
		return 0;

	ep_state = crg_udc_get_ep_state(crg_udc, udc_ep_ptr->dci);

	if (ep_state == EP_STATE_DISABLED)
		return 0;

	if (ep_state == EP_STATE_HALTED) {
		if (halt != 0)
			return 0;

		/* want unhalt an halted ep */
		if (udc_ep_ptr->wedge && !ignore_wedge) {
			do_halt = -1;
			reset_seq_only = true;
		} else {
			do_halt = 0;
		}

	} else {
		/* ep state == running or stopped */
		if (halt != 0) {
			/* want halt a running ep */
			do_halt = 1;
		} else {
			/* reset a running ep */
			do_halt = 0;
			reset_seq_only = true;
		}
	}

	trace_crg_udc_ep_halt(udc_ep_ptr, do_halt == 1, udc_ep_ptr->wedge);

	param0 = BIT(udc_ep_ptr->dci);
	if (do_halt == 1) {
		/* setting ep to halt */
		if (crg_udc_issue_command(crg_udc, CRG_CMD_SET_HALT, param0, 0))
			return -EIO;
		if (readl_poll_timeout_atomic(&uccr->ep_running, tmp,
					      !(tmp & param0), 10, 10000))
			dev_warn(crg_udc->dev, "ep %d halt wait timed out\n",
				 udc_ep_ptr->dci);

		if (crg_udc->gadget.speed >= USB_SPEED_SUPER) {
			/* clean up the request queue */
			crg_udc_ep_nuke(udc_ep_ptr, -ECONNRESET);

			udc_ep_ptr->deq_pt = udc_ep_ptr->enq_pt;
			udc_ep_ptr->tran_ring_full = false;
		}
		/* clean up the request queue */
		udc_ep_ptr->ep_state = EP_STATE_HALTED;
	} else if (do_halt == 0) {
		/* clearing ep halt state */
		/* reset sequence number */
		if (crg_udc_issue_command(crg_udc, CRG_CMD_RESET_SEQNUM, param0,
					  0))
			return -EIO;

		if (!reset_seq_only) {
			/* Clear halt for a halted EP. */
			/* NOTE: we must CLEAR_HALT first,
			 * then SET_TR_DQPTR
			 */
			if (crg_udc_issue_command(crg_udc, CRG_CMD_CLEAR_HALT,
						  param0, 0))
				return -EIO;
			if (crg_udc_epcx_update_dqptr(udc_ep_ptr))
				return -EIO;
		}

		udc_ep_ptr->wedge = 0;
		udc_ep_ptr->ep_state = EP_STATE_RUNNING;
		/* set endpoint to running state */
		/* clear pause for the endpoint */
		if (!list_empty(&udc_ep_ptr->queue)) {
			struct crg_transfer_trb *next = udc_ep_ptr->deq_pt + 1;

			if (FIELD_GET(TRB_TYPE_MASK, le32_to_cpu(next->dw3)) ==
			    TRB_TYPE_LINK) {
				udc_ep_ptr->pcs ^= 1;
				if (crg_udc_epcx_update_dqptr(udc_ep_ptr)) {
					udc_ep_ptr->pcs ^= 1;
					return -EIO;
				}
				udc_ep_ptr->pcs ^= 1;
			}
			crg_udc_knock_doorbell(crg_udc, udc_ep_ptr->dci);
		}
	} else {
		/* wedged EP deny CLEAR HALT */
		dev_dbg(crg_udc->dev, "wedged EP deny CLEAR HALT dci = %d\n",
			udc_ep_ptr->dci);
		/* reset sequence number */
		if (reset_seq_only &&
		    crg_udc_issue_command(crg_udc, CRG_CMD_RESET_SEQNUM, param0,
					  0))
			return -EIO;
	}

	return 0;
}

enum crg_ep_state crg_udc_get_ep_state(struct crg_gadget_dev *crg_udc, int dci)
{
	struct crg_udc_ep *udc_ep_ptr;

	/*
	 * DCI 0 is ep0; DCI 1 is unused in the Corigine ep-context layout
	 * (ep contexts start from DCI 2 for ep1in/ep1out). This is why the
	 * ep-context array is indexed as p_epcx[dci - 2] elsewhere.
	 */
	if (dci < 0 || dci >= CRG_NUM_EP_CX || dci == 1)
		return EP_STATE_DISABLED;

	udc_ep_ptr = &crg_udc->udc_ep[dci];

	return udc_ep_ptr->ep_state;
}

u32 crg_udc_index2dci(u16 index)
{
	if (index == 0)
		return 0;

	return (index & USB_ENDPOINT_NUMBER_MASK) * 2 +
	       ((index & USB_DIR_IN) ? 0 : 1);
}

int crg_udc_init_device_context(struct crg_gadget_dev *crg_udc)
{
	struct crg_uccr __iomem *uccr = crg_udc->uccr;
	int ret;
	u32 buff_length;
	dma_addr_t mapping;

	/* ep0 is not included in ep contexts in crg udc */
	buff_length = (CRG_NUM_EP_CX - 2) * sizeof(struct crg_ep_cx);

	if (!crg_udc->ep_cx.cpu_addr) {
		crg_udc->ep_cx.cpu_addr = dma_alloc_coherent(
			crg_udc->dev, buff_length, &mapping, GFP_ATOMIC);
		if (!crg_udc->ep_cx.cpu_addr) {
			ret = -ENOMEM;
			return ret;
		}
	} else {
		mapping = crg_udc->ep_cx.dma_addr;
	}

	crg_udc->p_epcx = crg_udc->ep_cx.cpu_addr;
	crg_udc->ep_cx.len = buff_length;
	crg_udc->ep_cx.dma_addr = mapping;
	memset(crg_udc->p_epcx, 0, buff_length);
	/*
	 * Ensure the endpoint contexts are initialized before making their
	 * DMA address visible to the controller.
	 */
	dma_wmb();
	/* hw ops DCBAPLO DCBAPHI */
	writel(lower_32_bits(crg_udc->ep_cx.dma_addr), &uccr->dcbaplo);
	writel(upper_32_bits(crg_udc->ep_cx.dma_addr), &uccr->dcbaphi);

	return 0;
}

void crg_udc_init_ep_info(struct crg_gadget_dev *crg_udc)
{
	int i;

	/* udc_ep[0] is reserved */
	crg_ep_struct_setup(crg_udc, 0, NULL);

	for (i = 1; i < CRG_NUM_EP_CX / 2; i++) {
		char name[14];

		snprintf(name, sizeof(name), "ep%din", i);
		crg_ep_struct_setup(crg_udc, i * 2, name);
		snprintf(name, sizeof(name), "ep%dout", i);
		crg_ep_struct_setup(crg_udc, i * 2 + 1, name);
	}
}

static bool is_request_dequeued(struct crg_gadget_dev *crg_udc,
				struct crg_udc_ep *udc_ep,
				struct crg_event_trb *event)
{
	struct crg_udc_request *udc_req;
	u32 trb_pt_lo = le32_to_cpu(event->dw0);
	u32 trb_pt_hi = le32_to_cpu(event->dw1);
	u64 trb_addr = (u64)trb_pt_lo | ((u64)trb_pt_hi << 32);
	struct crg_transfer_trb *trb_pt;
	bool status = true;

	if (udc_ep->dci == 0 || list_empty(&udc_ep->queue))
		return false;

	trb_pt = crg_udc_tran_trb_dma_to_virt(udc_ep, trb_addr);
	if (!trb_pt)
		return false;
	list_for_each_entry(udc_req, &udc_ep->queue, queue) {
		if (trb_pt == udc_req->last_trb ||
		    trb_pt == udc_req->first_trb) {
			status = false;
			break;
		}

		if (crg_udc_is_pointer_less_than(trb_pt, udc_req->last_trb,
						 udc_ep) &&
		    crg_udc_is_pointer_less_than(udc_req->first_trb, trb_pt,
						 udc_ep)) {
			status = false;
			break;
		}
	}

	return status;
}

void crg_udc_knock_doorbell(struct crg_gadget_dev *crg_udc, int dci)
{
	u32 tmp;
	struct crg_uccr __iomem *uccr;

	uccr = crg_udc->uccr;
	/* Ensure everything is written before notifying the HW */
	dma_wmb();

	tmp = FIELD_PREP(CRG_U3DC_DB_TARGET_MASK, dci);
	writel(tmp, &uccr->doorbell);

	trace_crg_udc_doorbell(crg_udc, dci);
}

/*
 * Complete a request and give it back to the gadget layer via the
 * standard usb_gadget_giveback_request() helper.
 *
 * Must be called with udc_lock held and interrupts disabled.
 *
 * The request's ->complete() callback must not sleep. crg_udc_req_done() drops
 * udc_lock before the giveback and re-acquires it before returning.
 * Callers therefore cannot assume state protected by udc_lock is
 * unchanged across this call.
 */
void crg_udc_req_done(struct crg_udc_ep *udc_ep,
		      struct crg_udc_request *udc_req, int status)
{
	struct crg_gadget_dev *crg_udc = udc_ep->crg_udc;

	if (likely(udc_req->usb_req.status == -EINPROGRESS))
		udc_req->usb_req.status = status;

	trace_crg_udc_request_giveback(udc_ep, udc_req);

	list_del_init(&udc_req->queue);

	crg_udc_unmap_request(udc_ep, udc_req);

	if (udc_req->usb_req.complete) {
		spin_unlock(&crg_udc->udc_lock);
		usb_gadget_giveback_request(&udc_ep->usb_ep, &udc_req->usb_req);
		spin_lock(&crg_udc->udc_lock);
	}
}

static int set_ep_halt(struct crg_gadget_dev *crg_udc, int dci,
		       unsigned long *flags)
{
	struct crg_uccr __iomem *uccr = crg_udc->uccr;
	struct crg_udc_ep *udc_ep_ptr = &crg_udc->udc_ep[dci];
	u32 param0;
	u32 tmp;

	if (dci == 0)
		return 0;

	if (udc_ep_ptr->ep_state == EP_STATE_DISABLED ||
	    udc_ep_ptr->ep_state == EP_STATE_HALTED)
		return 0;
	if (udc_ep_ptr->desc && usb_endpoint_xfer_isoc(udc_ep_ptr->desc))
		return -EOPNOTSUPP;

	param0 = BIT(dci);
	if (crg_udc_issue_command(crg_udc, CRG_CMD_SET_HALT, param0, 0))
		return -EIO;
	if (readl_poll_timeout_atomic(&uccr->ep_running, tmp, !(tmp & param0),
				      10, 10000))
		return -ETIMEDOUT;
	/* clean up the request queue */
	crg_udc_ep_nuke(udc_ep_ptr, -ECONNRESET);

	udc_ep_ptr->deq_pt = udc_ep_ptr->enq_pt;
	udc_ep_ptr->tran_ring_full = false;
	udc_ep_ptr->ep_state = EP_STATE_HALTED;

	return 0;
}

void crg_udc_clear_req_container(struct crg_udc_request *udc_req_ptr)
{
	udc_req_ptr->buff_len_left = 0;
	udc_req_ptr->trbs_needed = 0;
	udc_req_ptr->all_trbs_queued = 0;
	udc_req_ptr->first_trb = NULL;
	udc_req_ptr->last_trb = NULL;
	udc_req_ptr->sg = NULL;
	udc_req_ptr->sg_index = 0;
	udc_req_ptr->sg_offset = 0;
}

const struct usb_ep_ops crg_udc_ep_ops = {
	.enable = crg_udc_ep_enable,
	.disable = crg_udc_ep_disable,
	.alloc_request = crg_udc_alloc_request,
	.free_request = crg_udc_free_request,
	.queue = crg_udc_ep_queue,
	.dequeue = crg_udc_ep_dequeue,
	.set_halt = crg_udc_ep_set_halt,
	.set_wedge = crg_udc_ep_set_wedge,
};

const struct usb_gadget_ops crg_gadget_ops = {
	.get_frame = crg_gadget_get_frame,
	.wakeup = crg_gadget_wakeup,
	.set_remote_wakeup = crg_gadget_set_remote_wakeup,
	.set_selfpowered = crg_gadget_set_selfpowered,
	.pullup = crg_gadget_pullup,
	.udc_start = crg_gadget_start,
	.udc_stop = crg_gadget_stop,
	.udc_set_speed = crg_udc_set_speed,
	.udc_async_callbacks = crg_gadget_async_callbacks,
};
