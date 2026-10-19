/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Axiado AX3000/AX3005 USB Device Controller driver.
 * Debug/trace decode helpers.
 *
 * Copyright (c) 2021-2026 Axiado Corporation.
 */

#ifndef __CRG_UDC_DEBUG_H
#define __CRG_UDC_DEBUG_H

#include "crg-udc.h"

static inline const char *crg_udc_trb_type_string(u32 type)
{
	switch (type) {
	case TRB_TYPE_XFER_NORMAL:
		return "Normal";
	case TRB_TYPE_XFER_DATA_STAGE:
		return "Data Stage";
	case TRB_TYPE_XFER_STATUS_STAGE:
		return "Status Stage";
	case TRB_TYPE_XFER_DATA_ISOCH:
		return "Isoch";
	case TRB_TYPE_LINK:
		return "Link";
	case TRB_TYPE_NO_OP:
		return "No-Op";
	case TRB_TYPE_EVT_TRANSFER:
		return "Transfer Event";
	case TRB_TYPE_EVT_CMD_COMPLETION:
		return "Command Completion Event";
	case TRB_TYPE_EVT_PORT_STATUS_CHANGE:
		return "Port Status Change Event";
	case TRB_TYPE_EVT_MFINDEX_WRAP:
		return "MFINDEX Wrap Event";
	case TRB_TYPE_EVT_SETUP_PKT:
		return "Setup Packet Event";
	default:
		return "Reserved";
	}
}

static inline const char *crg_udc_cmpl_code_string(u32 code)
{
	switch (code) {
	case CMPL_CODE_INVALID:
		return "Invalid";
	case CMPL_CODE_SUCCESS:
		return "Success";
	case CMPL_CODE_DATA_BUFFER_ERR:
		return "Data Buffer Error";
	case CMPL_CODE_BABBLE_DETECTED_ERR:
		return "Babble Detected";
	case CMPL_CODE_USB_TRANS_ERR:
		return "USB Transaction Error";
	case CMPL_CODE_TRB_ERR:
		return "TRB Error";
	case CMPL_CODE_TRB_STALL:
		return "Stall";
	case CMPL_CODE_INVALID_STREAM_TYPE_ERR:
		return "Invalid Stream Type";
	case CMPL_CODE_SHORT_PKT:
		return "Short Packet";
	case CMPL_CODE_RING_UNDERRUN:
		return "Ring Underrun";
	case CMPL_CODE_RING_OVERRUN:
		return "Ring Overrun";
	case CMPL_CODE_EVENT_RING_FULL_ERR:
		return "Event Ring Full";
	case CMPL_CODE_STOPPED:
		return "Stopped";
	case CMPL_CODE_STOPPED_LENGTH_INVALID:
		return "Stopped - Length Invalid";
	case CMPL_CODE_ISOCH_BUFFER_OVERRUN:
		return "Isoch Buffer Overrun";
	case CMPL_CODE_PROTOCOL_STALL:
		return "Protocol Stall";
	case CMPL_CODE_SETUP_TAG_MISMATCH:
		return "Setup Tag Mismatch";
	case CMPL_CODE_HALTED:
		return "Halted";
	case CMPL_CODE_HALTED_LENGTH_INVALID:
		return "Halted - Length Invalid";
	case CMPL_CODE_DISABLED:
		return "Disabled";
	case CMPL_CODE_DISABLED_LENGTH_INVALID:
		return "Disabled - Length Invalid";
	default:
		return "Unknown";
	}
}

static inline const char *crg_udc_decode_portsc(char *str, size_t size,
						u32 portsc)
{
	scnprintf(str, size, "0x%08x, PLS: %lu, Speed: %lu, %s%s%s%s",
		 portsc, FIELD_GET(CRG_U3DC_PORTSC_PLS_MASK, portsc),
		 FIELD_GET(CRG_U3DC_PORTSC_SPEED_MASK, portsc),
		 (portsc & CRG_U3DC_PORTSC_CCS) ? "CCS " : "",
		 (portsc & CRG_U3DC_PORTSC_PED) ? "PED " : "",
		 (portsc & CRG_U3DC_PORTSC_PR) ? "PR " : "",
		 (portsc & CRG_U3DC_PORTSC_PP) ? "PP " : "");

	return str;
}

static inline const char *crg_udc_decode_trb(char *str, size_t size,
					     struct crg_transfer_trb *trb)
{
	u32 dw2 = le32_to_cpu(trb->dw2);
	u32 dw3 = le32_to_cpu(trb->dw3);
	u32 type = FIELD_GET(TRB_TYPE_MASK, dw3);

	scnprintf(str, size,
		  "type: %s, len: %lu, cycle: %lu, chain: %lu, ioc: %lu",
		  crg_udc_trb_type_string(type),
		  FIELD_GET(TRB_TRANSFER_LEN_MASK, dw2),
		  dw3 & TRB_CYCLE_BIT_MASK ? 1UL : 0UL,
		  dw3 & TRB_CHAIN_BIT_MASK ? 1UL : 0UL,
		  dw3 & TRB_INTR_ON_COMPLETION_MASK ? 1UL : 0UL);

	return str;
}

static inline const char *crg_udc_decode_event_trb(char *str, size_t size,
						   struct crg_event_trb *trb)
{
	u32 dw2 = le32_to_cpu(trb->dw2);
	u32 dw3 = le32_to_cpu(trb->dw3);
	u32 type = FIELD_GET(EVE_TRB_TYPE_MASK, dw3);

	scnprintf(str, size, "type: %s, cmpl: %s, len: %lu, dci: %lu",
		  crg_udc_trb_type_string(type),
		  crg_udc_cmpl_code_string(
			  FIELD_GET(EVE_TRB_COMPL_CODE_MASK, dw2)),
		  FIELD_GET(EVE_TRB_TRAN_LEN_MASK, dw2),
		  FIELD_GET(EVE_TRB_ENDPOINT_ID_MASK, dw3));

	return str;
}

#endif /* __CRG_UDC_DEBUG_H */
