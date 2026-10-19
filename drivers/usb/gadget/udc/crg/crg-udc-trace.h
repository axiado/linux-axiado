/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Axiado AX3000/AX3005 USB Device Controller driver.
 * Trace support header file.
 *
 * Copyright (c) 2021-2026 Axiado Corporation.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM crg-udc

#undef TRACE_SYSTEM_VAR
#define TRACE_SYSTEM_VAR crg_udc

#if !defined(__CRG_UDC_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define __CRG_UDC_TRACE_H

#include <linux/types.h>
#include <linux/tracepoint.h>
#include <linux/usb/ch9.h>
#include "crg-udc.h"
#include "crg-udc-debug.h"

#define CRG_MSG_MAX 200

DECLARE_EVENT_CLASS(crg_udc_log_enable_disable, TP_PROTO(int set), TP_ARGS(set),
		    TP_STRUCT__entry(__field(int, set)),
		    TP_fast_assign(__entry->set = set;),
		    TP_printk("%s", __entry->set ? "enabled" : "disabled"));

DEFINE_EVENT(crg_udc_log_enable_disable, crg_udc_pullup, TP_PROTO(int set),
	     TP_ARGS(set));

TRACE_EVENT(crg_udc_no_room_on_ring, TP_PROTO(char *msg), TP_ARGS(msg),
	    TP_STRUCT__entry(__string(text, msg)),
	    TP_fast_assign(__assign_str(text);),
	    TP_printk("%s", __get_str(text)));

TRACE_EVENT(crg_udc_ep_halt,
	    TP_PROTO(struct crg_udc_ep *udc_ep, u8 halt, u8 wedge),
	    TP_ARGS(udc_ep, halt, wedge),
	    TP_STRUCT__entry(__string(name, udc_ep->usb_ep.name)
				     __field(u8, halt) __field(u8, wedge)),
	    TP_fast_assign(__assign_str(name); __entry->halt = halt;
			   __entry->wedge = wedge;),
	    TP_printk("%s: %s%s", __get_str(name),
		      __entry->halt ? "halted" : "cleared",
		      __entry->wedge ? " (wedged)" : ""));

TRACE_EVENT(crg_udc_portsc, TP_PROTO(u32 portsc), TP_ARGS(portsc),
	    TP_STRUCT__entry(__field(u32, portsc)),
	    TP_fast_assign(__entry->portsc = portsc;),
	    TP_printk("%s",
		      crg_udc_decode_portsc(__get_buf(CRG_MSG_MAX), CRG_MSG_MAX,
					    __entry->portsc)));

TRACE_EVENT(crg_udc_doorbell, TP_PROTO(struct crg_gadget_dev *crg_udc, int dci),
	    TP_ARGS(crg_udc, dci),
	    TP_STRUCT__entry(__string(name, crg_udc->udc_ep[dci].usb_ep.name)
				     __field(int, dci)),
	    TP_fast_assign(__assign_str(name); __entry->dci = dci;),
	    TP_printk("%s, dci %d", __get_str(name), __entry->dci));

DECLARE_EVENT_CLASS(
	crg_udc_log_ctrl, TP_PROTO(struct usb_ctrlrequest *ctrl), TP_ARGS(ctrl),
	TP_STRUCT__entry(__field(u8, bRequestType) __field(u8, bRequest)
				 __field(u16, wValue) __field(u16, wIndex)
					 __field(u16, wLength)),
	TP_fast_assign(__entry->bRequestType = ctrl->bRequestType;
		       __entry->bRequest = ctrl->bRequest;
		       __entry->wValue = le16_to_cpu(ctrl->wValue);
		       __entry->wIndex = le16_to_cpu(ctrl->wIndex);
		       __entry->wLength = le16_to_cpu(ctrl->wLength);),
	TP_printk("%s", usb_decode_ctrl(__get_buf(CRG_MSG_MAX), CRG_MSG_MAX,
					__entry->bRequestType,
					__entry->bRequest, __entry->wValue,
					__entry->wIndex, __entry->wLength)));

DEFINE_EVENT(crg_udc_log_ctrl, crg_udc_ctrl_req,
	     TP_PROTO(struct usb_ctrlrequest *ctrl), TP_ARGS(ctrl));

DECLARE_EVENT_CLASS(
	crg_udc_log_trb,
	TP_PROTO(struct crg_udc_ep *udc_ep, struct crg_transfer_trb *trb),
	TP_ARGS(udc_ep, trb),
	TP_STRUCT__entry(__string(name, udc_ep->usb_ep.name)
				 __field(struct crg_transfer_trb *, trb)
					 __dynamic_array(char, decoded,
							 CRG_MSG_MAX)),
	TP_fast_assign(__assign_str(name); __entry->trb = trb;
		       crg_udc_decode_trb(__get_dynamic_array(decoded),
					  CRG_MSG_MAX, trb);),
	TP_printk("%s: trb %p, %s", __get_str(name), __entry->trb,
		  __get_str(decoded)));

DEFINE_EVENT(crg_udc_log_trb, crg_udc_queue_trb,
	     TP_PROTO(struct crg_udc_ep *udc_ep, struct crg_transfer_trb *trb),
	     TP_ARGS(udc_ep, trb));

TRACE_EVENT(
	crg_udc_event_trb, TP_PROTO(struct crg_event_trb *trb), TP_ARGS(trb),
	TP_STRUCT__entry(__field(struct crg_event_trb *, trb)
				 __dynamic_array(char, decoded, CRG_MSG_MAX)),
	TP_fast_assign(__entry->trb = trb;
		       crg_udc_decode_event_trb(__get_dynamic_array(decoded),
						CRG_MSG_MAX, trb);),
	TP_printk("trb %p, %s", __entry->trb, __get_str(decoded)));

DECLARE_EVENT_CLASS(
	crg_udc_log_request,
	TP_PROTO(struct crg_udc_ep *udc_ep, struct crg_udc_request *udc_req),
	TP_ARGS(udc_ep, udc_req),
	TP_STRUCT__entry(
		__string(name, udc_ep->usb_ep.name) __field(
			struct usb_request *, request) __field(void *, buf)
			__field(unsigned int, actual) __field(unsigned int,
							      length)
				__field(int, status) __field(dma_addr_t, dma)
					__field(int, zero) __field(int,
								   short_not_ok)
						__field(int, no_interrupt)),
	TP_fast_assign(__assign_str(name); __entry->request = &udc_req->usb_req;
		       __entry->buf = udc_req->usb_req.buf;
		       __entry->actual = udc_req->usb_req.actual;
		       __entry->length = udc_req->usb_req.length;
		       __entry->status = udc_req->usb_req.status;
		       __entry->dma = udc_req->usb_req.dma;
		       __entry->zero = udc_req->usb_req.zero;
		       __entry->short_not_ok = udc_req->usb_req.short_not_ok;
		       __entry->no_interrupt = udc_req->usb_req.no_interrupt;),
	TP_printk(
		"%s: req: %p, buf: %p, length: %u/%u, status: %d, dma: %pad, %s%s%s",
		__get_str(name), __entry->request, __entry->buf,
		__entry->actual, __entry->length, __entry->status,
		&__entry->dma, __entry->zero ? "Z" : "z",
		__entry->short_not_ok ? "S" : "s",
		__entry->no_interrupt ? "I" : "i"));

DEFINE_EVENT(crg_udc_log_request, crg_udc_alloc_request,
	     TP_PROTO(struct crg_udc_ep *udc_ep,
		      struct crg_udc_request *udc_req),
	     TP_ARGS(udc_ep, udc_req));

DEFINE_EVENT(crg_udc_log_request, crg_udc_free_request,
	     TP_PROTO(struct crg_udc_ep *udc_ep,
		      struct crg_udc_request *udc_req),
	     TP_ARGS(udc_ep, udc_req));

DEFINE_EVENT(crg_udc_log_request, crg_udc_request_giveback,
	     TP_PROTO(struct crg_udc_ep *udc_ep,
		      struct crg_udc_request *udc_req),
	     TP_ARGS(udc_ep, udc_req));

#endif /* __CRG_UDC_TRACE_H */

/* This part must be outside header guard. */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .

#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE crg-udc-trace

#include <trace/define_trace.h>
