// SPDX-License-Identifier: GPL-2.0+
/*
 * Axiado AX3000/AX3005 USB Device Controller driver.
 *
 * Copyright (c) 2019 Corigine Inc.
 * Copyright (c) 2021-2026 Axiado Corporation.
 */
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/property.h>
#include <linux/reset.h>
#include <linux/slab.h>

#include "crg-udc.h"

static int crg_gadget_irq_init(struct platform_device *pdev,
			       struct crg_gadget_dev *crg_udc);
static void crg_udc_phy_exit(void *data);

static void crg_udc_gadget_release(struct device *dev)
{
	struct crg_gadget_dev *crg_udc;

	crg_udc = container_of(dev, struct crg_gadget_dev, gadget.dev);
	kfree(crg_udc);
}

/*
 * command related ops
 */
int crg_udc_issue_command(struct crg_gadget_dev *crg_udc,
			  enum crg_cmd_type type, u32 param0, u32 param1)
{
	struct crg_uccr __iomem *uccr = crg_udc->uccr;
	u32 status;
	bool check_complete = false;
	u32 tmp;

	/*
	 * The command state machine only runs while the controller itself is
	 * running. Once RUN is cleared (e.g. during udc_stop()/reset()), the
	 * hardware never processes queued commands, so ACTIVE never clears.
	 * Skip the busy/completion checks in that case instead of getting
	 * stuck on a command the hardware will never service.
	 */
	tmp = readl(&uccr->control);
	if (tmp & CRG_U3DC_CTRL_RUN)
		check_complete = true;

	if (check_complete) {
		tmp = readl(&uccr->cmd_control);
		if (tmp & CRG_U3DC_CMD_CTRL_ACTIVE) {
			dev_err(crg_udc->dev,
				"previous command is not complete\n");
			return -EBUSY;
		}
	}
	/* Ensure that everything is written before issuing new command */
	dma_wmb();

	writel(param0, &uccr->cmd_param0);
	writel(param1, &uccr->cmd_param1);

	tmp = CRG_U3DC_CMD_CTRL_ACTIVE |
	      FIELD_PREP(CRG_U3DC_CMD_CTRL_TYPE_MASK, type);
	writel(tmp, &uccr->cmd_control);

	if (!check_complete)
		return 0;

	if (readl_poll_timeout_atomic(&uccr->cmd_control, tmp,
				      !(tmp & CRG_U3DC_CMD_CTRL_ACTIVE), 1,
				      1000)) {
		dev_dbg(crg_udc->dev, "command timed out\n");
		return -ETIMEDOUT;
	}

	status = FIELD_GET(CRG_U3DC_CMD_CTRL_STATUS_MASK, tmp);
	if (status) {
		dev_dbg(crg_udc->dev, "command failed with status %u\n",
			status);
		return -EIO;
	}

	return 0;
}

static void crg_udc_clk_disable(void *data)
{
	clk_disable_unprepare(data);
}

void crg_udc_free_resources(struct crg_gadget_dev *crg_udc)
{
	int i;

	if (crg_udc->uccr) {
		u32 val = readl(&crg_udc->uccr->control);

		val &= ~(CRG_U3DC_CTRL_INT_EN | CRG_U3DC_CTRL_RUN);
		writel(val, &crg_udc->uccr->control);
	}
	if (crg_udc->irq >= 0) {
		devm_free_irq(crg_udc->dev, crg_udc->irq, crg_udc);
		crg_udc->irq = -1;
		crg_udc->gadget.irq = 0;
	}

	for (i = 0; i < CRG_RING_NUM; i++) {
		struct crg_udc_event *udc_event = &crg_udc->udc_event[i];

		if (udc_event->event_ring.cpu_addr) {
			dma_free_coherent(crg_udc->dev,
					  udc_event->event_ring.len,
					  udc_event->event_ring.cpu_addr,
					  udc_event->event_ring.dma_addr);
			udc_event->event_ring.cpu_addr = NULL;
			udc_event->event_ring.dma_addr = DMA_MAPPING_ERROR;
			udc_event->event_ring.len = 0;
			udc_event->evt_dq_pt = NULL;
		}
		if (udc_event->erst.cpu_addr) {
			dma_free_coherent(crg_udc->dev, udc_event->erst.len,
					  udc_event->erst.cpu_addr,
					  udc_event->erst.dma_addr);
			udc_event->erst.cpu_addr = NULL;
			udc_event->erst.dma_addr = DMA_MAPPING_ERROR;
			udc_event->erst.len = 0;
			udc_event->p_erst = NULL;
		}
	}

	if (crg_udc->ep_cx.cpu_addr) {
		dma_free_coherent(crg_udc->dev, crg_udc->ep_cx.len,
				  crg_udc->ep_cx.cpu_addr,
				  crg_udc->ep_cx.dma_addr);
		crg_udc->ep_cx.cpu_addr = NULL;
		crg_udc->ep_cx.dma_addr = DMA_MAPPING_ERROR;
		crg_udc->ep_cx.len = 0;
		crg_udc->p_epcx = NULL;
	}

	for (i = 0; i < CRG_NUM_EP_CX; i++) {
		struct crg_udc_ep *udc_ep = &crg_udc->udc_ep[i];

		if (udc_ep->tran_ring_info.cpu_addr) {
			dma_free_coherent(crg_udc->dev,
					  udc_ep->tran_ring_info.len,
					  udc_ep->tran_ring_info.cpu_addr,
					  udc_ep->tran_ring_info.dma_addr);
			udc_ep->tran_ring_info.cpu_addr = NULL;
			udc_ep->tran_ring_info.dma_addr = DMA_MAPPING_ERROR;
			udc_ep->tran_ring_info.len = 0;
			udc_ep->first_trb = NULL;
			udc_ep->last_trb = NULL;
			udc_ep->enq_pt = NULL;
			udc_ep->deq_pt = NULL;
		}
	}

	if (crg_udc->status_req) {
		crg_udc_free_request(&crg_udc->udc_ep[0].usb_ep,
				     &crg_udc->status_req->usb_req);
		crg_udc->status_req = NULL;
	}
}

static int crg_udc_init_hw(struct crg_gadget_dev *crg_udc)
{
	struct device *dev = crg_udc->dev;
	int ret;

	crg_udc->clk = devm_clk_get_optional(dev, NULL);
	if (IS_ERR(crg_udc->clk))
		return dev_err_probe(dev, PTR_ERR(crg_udc->clk),
				     "failed to get clock\n");

	ret = clk_prepare_enable(crg_udc->clk);
	if (ret)
		return dev_err_probe(dev, ret, "failed to enable clock\n");

	ret = devm_add_action_or_reset(dev, crg_udc_clk_disable, crg_udc->clk);
	if (ret)
		return ret;

	crg_udc->rst =
		devm_reset_control_get_optional_exclusive_deasserted(dev, NULL);
	if (IS_ERR(crg_udc->rst))
		return dev_err_probe(dev, PTR_ERR(crg_udc->rst),
				     "failed to get reset\n");

	crg_udc->phy = devm_phy_optional_get(dev, NULL);
	if (IS_ERR(crg_udc->phy))
		return dev_err_probe(dev, PTR_ERR(crg_udc->phy),
				     "failed to get phy\n");

	ret = phy_init(crg_udc->phy);
	if (ret)
		return dev_err_probe(dev, ret, "failed to init phy\n");

	ret = phy_power_on(crg_udc->phy);
	if (ret) {
		phy_exit(crg_udc->phy);
		return dev_err_probe(dev, ret, "failed to power on phy\n");
	}

	return devm_add_action_or_reset(dev, crg_udc_phy_exit, crg_udc->phy);
}

static void crg_udc_phy_exit(void *data)
{
	struct phy *phy = data;

	phy_power_off(phy);
	phy_exit(phy);
}

static int crg_udc_probe(struct platform_device *pdev)
{
	unsigned long flags = 0;
	int ret;
	int i;
	struct crg_gadget_dev *crg_udc;
	const struct crg_udc_priv *priv;

	priv = device_get_match_data(&pdev->dev);
	if (!priv)
		return -ENODEV;

	crg_udc = kzalloc(sizeof(*crg_udc), GFP_KERNEL);
	if (!crg_udc)
		return -ENOMEM;
	usb_initialize_gadget(&pdev->dev, &crg_udc->gadget,
			      crg_udc_gadget_release);

	crg_udc->dev = &pdev->dev;
	crg_udc->irq = -1;

	ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(64));
	if (ret) {
		ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32));
		if (ret) {
			dev_err(&pdev->dev, "no usable DMA mask\n");
			goto err_free_resources;
		}
	}

	ret = crg_udc_init_hw(crg_udc);
	if (ret)
		goto err_free_resources;

	crg_udc->ep0_desc.bLength = USB_DT_ENDPOINT_SIZE;
	crg_udc->ep0_desc.bDescriptorType = USB_DT_ENDPOINT;
	crg_udc->ep0_desc.bEndpointAddress = 0;
	crg_udc->ep0_desc.bmAttributes = USB_ENDPOINT_XFER_CONTROL;
	crg_udc->ep0_desc.wMaxPacketSize = cpu_to_le16(64);

	crg_udc->u12_forbidden = priv->quirks & CRG_QUIRK_DIS_U1U2_ENTRY;
	crg_udc->u12_initiate_forbidden = priv->quirks &
					  CRG_QUIRK_DIS_U1U2_INITIATION;
	crg_udc->reinit_ep0_on_bus_reset = priv->quirks &
					   CRG_QUIRK_REINIT_EP0_ON_RESET;

	spin_lock_init(&crg_udc->udc_lock);
	INIT_WORK(&crg_udc->udc_reset_work, crg_udc_reset_work);
	platform_set_drvdata(pdev, crg_udc);

	crg_udc->gadget.ops = &crg_gadget_ops;
	crg_udc->gadget.ep0 = &crg_udc->udc_ep[0].usb_ep;
	INIT_LIST_HEAD(&crg_udc->gadget.ep_list);
	/*
	 * The controller supports up to SuperSpeed. Honour the standard
	 * "maximum-speed" property to cap it; default to the hardware
	 * maximum when the property is absent.
	 */
	crg_udc->gadget.max_speed = usb_get_maximum_speed(&pdev->dev);
	if (crg_udc->gadget.max_speed == USB_SPEED_UNKNOWN)
		crg_udc->gadget.max_speed = USB_SPEED_SUPER;
	crg_udc->max_speed = crg_udc->gadget.max_speed;
	crg_udc->gadget.speed = USB_SPEED_UNKNOWN;
	crg_udc->gadget.name = KBUILD_MODNAME;
	crg_udc->gadget.sg_supported = true;
	crg_udc->gadget.wakeup_capable = true;
	crg_udc->connected = 0;
	crg_udc->dev_addr = 0;

	crg_udc->mmio_virt_base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(crg_udc->mmio_virt_base)) {
		ret = dev_err_probe(&pdev->dev,
				    PTR_ERR(crg_udc->mmio_virt_base),
				    "failed to map registers\n");
		goto err_put_gadget;
	}

	/* set controller device role */
	writel((readl(crg_udc->mmio_virt_base + CRG_UDC_MODE_REG) |
		CRG_UDC_ROLE_DEVICE),
	       crg_udc->mmio_virt_base + CRG_UDC_MODE_REG);
	for (i = 0; i < CRG_RING_NUM; i++) {
		crg_udc->uicr[i] = crg_udc->mmio_virt_base + CRG_UICR_OFFSET +
				   i * CRG_UICR_STRIDE;
	}
	crg_udc->uccr = crg_udc->mmio_virt_base + CRG_UCCR_OFFSET;

	spin_lock_irqsave(&crg_udc->udc_lock, flags);
	ret = crg_udc_reset(crg_udc, &flags);
	spin_unlock_irqrestore(&crg_udc->udc_lock, flags);
	if (ret)
		goto err_free_resources;

	crg_udc_clear_portpm(crg_udc);

	ret = crg_gadget_irq_init(pdev, crg_udc);
	if (ret)
		goto err_free_resources;

	ret = crg_udc_reset_data_struct(crg_udc);
	if (ret) {
		dev_err(crg_udc->dev, "crg_udc_reset_data_struct error\n");
		goto err_free_resources;
	}

	crg_udc_init_ep_info(crg_udc);
	ret = crg_udc_init_ep0(crg_udc);
	if (ret)
		goto err_free_resources;

	crg_udc->udc_ep[0].desc = &crg_udc->ep0_desc;

	ret = usb_add_gadget(&crg_udc->gadget);
	if (ret)
		goto err_free_resources;

	return 0;

err_free_resources:
	crg_udc_free_resources(crg_udc);
err_put_gadget:
	usb_put_gadget(&crg_udc->gadget);
	return ret;
}

static void crg_udc_remove(struct platform_device *pdev)
{
	struct crg_gadget_dev *crg_udc = platform_get_drvdata(pdev);
	u32 tmp;

	crg_udc->device_state = USB_STATE_ATTACHED;
	crg_udc_set_vbus_detect(crg_udc, false);

	usb_del_gadget(&crg_udc->gadget);
	cancel_work_sync(&crg_udc->udc_reset_work);
	crg_udc_free_resources(crg_udc);

	/* set controller host role */
	tmp = readl(crg_udc->mmio_virt_base + CRG_UDC_MODE_REG) & ~0x1;
	writel(tmp, crg_udc->mmio_virt_base + CRG_UDC_MODE_REG);

	platform_set_drvdata(pdev, NULL);
	usb_put_gadget(&crg_udc->gadget);
}

static void crg_udc_shutdown(struct platform_device *pdev)
{
	struct crg_gadget_dev *crg_udc = platform_get_drvdata(pdev);
	unsigned long flags;

	spin_lock_irqsave(&crg_udc->udc_lock, flags);
	crg_udc->device_state = USB_STATE_ATTACHED;
	crg_udc_set_pullup(crg_udc, false);
	spin_unlock_irqrestore(&crg_udc->udc_lock, flags);
	crg_udc_set_vbus_detect(crg_udc, false);
	if (crg_udc->uccr) {
		u32 val = readl(&crg_udc->uccr->control);

		val &= ~CRG_U3DC_CTRL_INT_EN;
		writel(val, &crg_udc->uccr->control);
	}
	if (crg_udc->irq >= 0)
		synchronize_irq(crg_udc->irq);
}

static int __maybe_unused crg_udc_suspend(struct device *dev)
{
	struct crg_gadget_dev *crg_udc = dev_get_drvdata(dev);
	unsigned long flags;

	/* Stop the pullup while the controller is suspended. */
	spin_lock_irqsave(&crg_udc->udc_lock, flags);
	crg_udc_set_pullup(crg_udc, false);
	spin_unlock_irqrestore(&crg_udc->udc_lock, flags);
	return 0;
}

static int __maybe_unused crg_udc_resume(struct device *dev)
{
	struct crg_gadget_dev *crg_udc = dev_get_drvdata(dev);
	unsigned long flags;

	spin_lock_irqsave(&crg_udc->udc_lock, flags);
	if (crg_udc->gadget_driver && crg_udc->pullup_on)
		crg_udc_set_pullup(crg_udc, true);
	spin_unlock_irqrestore(&crg_udc->udc_lock, flags);

	return 0;
}

/* Program the speed limit while the controller is stopped. */
int crg_udc_program_speed(struct crg_gadget_dev *crg_udc)
{
	u32 cfg, speed;

	switch (crg_udc->max_speed) {
	case USB_SPEED_FULL:
		speed = CRG_U3DC_CFG0_MAXSPEED_FS;
		break;
	case USB_SPEED_HIGH:
		speed = CRG_U3DC_CFG0_MAXSPEED_HS;
		break;
	case USB_SPEED_SUPER:
		speed = CRG_U3DC_CFG0_MAXSPEED_SS;
		break;
	case USB_SPEED_SUPER_PLUS:
		speed = CRG_U3DC_CFG0_MAXSPEED_SSP;
		break;
	default:
		return -EINVAL;
	}

	cfg = readl(&crg_udc->uccr->config0);
	cfg &= ~CRG_U3DC_CFG0_MAXSPEED_MASK;
	cfg |= FIELD_PREP(CRG_U3DC_CFG0_MAXSPEED_MASK, speed);
	writel(cfg, &crg_udc->uccr->config0);

	return 0;
}

int crg_udc_reset_data_struct(struct crg_gadget_dev *crg_udc)
{
	int i;
	int ret;
	struct crg_uccr __iomem *uccr = crg_udc->uccr;
	u32 val;

	crg_udc->ep0_state = EP0_STATE_IDLE;

	val = readl(&uccr->control);
	val &= (~(CRG_U3DC_CTRL_INT_EN | CRG_U3DC_CTRL_RUN));
	writel(val, &uccr->control);

	ret = crg_udc_program_speed(crg_udc);
	if (ret)
		return ret;

	for (i = 0; i < CRG_RING_NUM; i++) {
		ret = crg_udc_init_event_ring(crg_udc, i);
		if (ret)
			return ret;
	}

	ret = crg_udc_init_device_context(crg_udc);
	if (ret)
		return ret;

	if (!crg_udc->status_req) {
		struct usb_request *req;

		req = crg_udc_alloc_request(&crg_udc->udc_ep[0].usb_ep,
					    GFP_ATOMIC);
		if (!req)
			return -ENOMEM;

		crg_udc->status_req =
			container_of(req, struct crg_udc_request, usb_req);
	}

	/* other hw ops */
	return 0;
}

static void crg_gadget_handle_interrupt(struct crg_gadget_dev *crg_udc)
{
	struct crg_uccr __iomem *uccr = crg_udc->uccr;
	unsigned long flags = 0;
	u32 tmp_status;
	int ret;

	spin_lock_irqsave(&crg_udc->udc_lock, flags);

	tmp_status = readl(&uccr->status);

	if (tmp_status & CRG_U3DC_STATUS_SYS_ERR) {
		bool was_connected = crg_udc->connected;

		dev_err(crg_udc->dev, "controller system error\n");
		writel(CRG_U3DC_STATUS_SYS_ERR, &uccr->status);
		ret = crg_udc_reset(crg_udc, &flags);
		if (!ret)
			ret = crg_udc_reset_data_struct(crg_udc);
		if (!ret)
			ret = crg_udc_init_ep0(crg_udc);
		if (ret)
			dev_err(crg_udc->dev,
				"failed to recover from controller error: %d\n",
				ret);
		crg_udc->gadget.speed = USB_SPEED_UNKNOWN;
		crg_udc->connected = 0;
		if (was_connected)
			crg_udc_notify_disconnect(crg_udc, &flags);
		if (!ret && crg_udc->pullup_on)
			crg_udc_set_pullup(crg_udc, true);

		/*
		 * tmp_status was captured before the reset above, which tore
		 * down and rebuilt the event rings. It no longer reflects
		 * the controller's current state, and if recovery failed the
		 * rings may not be consistent at all. Skip the rest of this
		 * interrupt's processing; any event still pending will raise
		 * a fresh interrupt once the controller is running again.
		 */
		spin_unlock_irqrestore(&crg_udc->udc_lock, flags);
		return;
	}

	if (tmp_status & CRG_U3DC_STATUS_EINT) {
		int i;

		writel(CRG_U3DC_STATUS_EINT, &uccr->status);

		for (i = 0; i < CRG_RING_NUM; i++) {
			ret = crg_udc_process_event_ring(crg_udc, i, &flags);
			if (ret)
				dev_err(crg_udc->dev,
					"process event ring error: %d\n", ret);
		}
	}

	if (crg_udc->device_state == USB_STATE_RECONNECTING &&
	    crg_udc->portsc_on_reconnecting == 1 &&
	    crg_udc_is_event_rings_empty(crg_udc)) {
		crg_udc->portsc_on_reconnecting = 0;
		crg_udc_handle_port_status(crg_udc, &flags);
	}

	if (crg_udc->device_state == USB_STATE_RECONNECTING &&
	    crg_udc->connected == 1)
		crg_udc_prepare_for_setup(crg_udc);

	spin_unlock_irqrestore(&crg_udc->udc_lock, flags);
}

/*
 * Primary handler only checks whether the shared interrupt belongs to the
 * controller. All controller work, including reset polling, runs in the
 * threaded handler and therefore never busy-waits in hard IRQ context.
 */
static irqreturn_t crg_udc_common_irq(int irq, void *dev)
{
	struct crg_gadget_dev *crg_udc = dev;
	struct crg_uccr __iomem *uccr = crg_udc->uccr;
	u32 status = readl(&uccr->status);

	if (!(status & (CRG_U3DC_STATUS_EINT | CRG_U3DC_STATUS_SYS_ERR)))
		return IRQ_NONE;

	return IRQ_WAKE_THREAD;
}

static irqreturn_t crg_udc_irq_thread(int irq, void *dev)
{
	struct crg_gadget_dev *crg_udc = dev;

	crg_gadget_handle_interrupt(crg_udc);

	return IRQ_HANDLED;
}

static int crg_gadget_irq_init(struct platform_device *pdev,
			       struct crg_gadget_dev *crg_udc)
{
	int irq;
	int ret;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;

	/*
	 * IRQF_ONESHOT keeps this (level-triggered) line masked until
	 * crg_udc_irq_thread() has cleared the status bits, which is the
	 * correct pairing for IRQF_SHARED with a threaded handler.
	 */
	ret = devm_request_threaded_irq(&pdev->dev, irq, crg_udc_common_irq,
					crg_udc_irq_thread,
					IRQF_SHARED | IRQF_ONESHOT, "crg_udc",
					crg_udc);
	if (ret) {
		dev_err(crg_udc->dev, "request of irq%d failed\n", irq);
	} else {
		crg_udc->irq = irq;
		crg_udc->gadget.irq = irq;
	}

	return ret;
}

int crg_udc_reset(struct crg_gadget_dev *crg_udc, unsigned long *flags)
{
	struct crg_uccr __iomem *uccr = crg_udc->uccr;
	u32 i, tmp;
	struct crg_udc_ep *udc_ep_ptr;
	u32 val;

	val = readl(&uccr->control);
	val |= CRG_U3DC_CTRL_SWRST;
	writel(val, &uccr->control);
	crg_udc->resetting = true;
	spin_unlock_irqrestore(&crg_udc->udc_lock, *flags);
	/* Poll until SWRST clears; hardware reset takes up to 10 ms */
	if (readl_poll_timeout_atomic(&uccr->control, tmp,
			       !(tmp & CRG_U3DC_CTRL_SWRST), 10, 10000)) {
		spin_lock_irqsave(&crg_udc->udc_lock, *flags);
		crg_udc->resetting = false;
		dev_err(crg_udc->dev, "reset error\n");
		return -ETIMEDOUT;
	}
	spin_lock_irqsave(&crg_udc->udc_lock, *flags);
	crg_udc->resetting = false;

	crg_udc_clear_portpm(crg_udc);

	crg_udc->setup_status = WAIT_FOR_SETUP;
	/* Base on Figure 9-1, default USB_STATE is attached */
	crg_udc->device_state = USB_STATE_ATTACHED;
	crg_udc->dev_addr = 0;

	/* Complete any reqs on EP1-EP15 queue */
	for (i = 2; i < CRG_NUM_EP_CX; i++) {
		udc_ep_ptr = &crg_udc->udc_ep[i];

		if (udc_ep_ptr->desc) {
			udc_ep_ptr->desc = NULL;
			udc_ep_ptr->comp_desc = NULL;
			crg_udc_ep_nuke(udc_ep_ptr, -ESHUTDOWN);
		}
		udc_ep_ptr->tran_ring_full = false;
		udc_ep_ptr->ep_state = EP_STATE_DISABLED;
		/*
		 * This bypasses the generic usb_ep_disable() API, so the
		 * framework's own usb_ep.enabled bookkeeping needs to be
		 * cleared directly here too (crg_udc_reinit() already does
		 * this for the live bus-reset case). Otherwise a later
		 * usb_ep_enable() call for this endpoint short-circuits on
		 * the stale "already enabled" flag without ever calling our
		 * .enable callback, leaving desc/the ring never set up for
		 * the new session.
		 */
		udc_ep_ptr->usb_ep.enabled = 0;
	}
	crg_udc->num_enabled_eps = 0;

	/* Complete any reqs on EP0 queue */
	udc_ep_ptr = &crg_udc->udc_ep[0];
	if (udc_ep_ptr->desc) {
		udc_ep_ptr->desc = NULL;
		udc_ep_ptr->comp_desc = NULL;
		crg_udc_ep_nuke(udc_ep_ptr, -ESHUTDOWN);
	}

	return 0;
}

static const struct crg_udc_priv crg_udc_data = {
	.quirks = CRG_UDC_QUIRKS_DEFAULT,
};

static const struct of_device_id of_crg_udc_match[] = {
	{ .compatible = "axiado,ax3000-udc", .data = &crg_udc_data },
	{ },
};
MODULE_DEVICE_TABLE(of, of_crg_udc_match);

static SIMPLE_DEV_PM_OPS(crg_udc_pm_ops, crg_udc_suspend, crg_udc_resume);

static struct platform_driver crg_udc_driver = {
	.probe = crg_udc_probe,
	.remove = crg_udc_remove,
	.shutdown = crg_udc_shutdown,
	.driver = {
		.name = KBUILD_MODNAME,
		.of_match_table = of_crg_udc_match,
		.pm = &crg_udc_pm_ops,
	},
};
module_platform_driver(crg_udc_driver);

MODULE_AUTHOR("Axiado Corporation");
MODULE_DESCRIPTION("Axiado AX3000/AX3005 USB Device Controller");
MODULE_LICENSE("GPL");
