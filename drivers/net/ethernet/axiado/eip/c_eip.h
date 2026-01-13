/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (C) 2022-2026 Axiado Corporation. */

#ifndef C_EIP_H_
#define C_EIP_H_
#include "cs_da_pec.h"

#ifndef LOG_SEVERITY_MAX
#define LOG_SEVERITY_MAX LOG_SEVERITY_INFO
#endif

#ifndef DA_PEC_LICENSE
#define DA_PEC_LICENSE "Dual BSD/GPL"
#endif

#ifndef DA_PEC_BANK_PACKET
#define DA_PEC_BANK_PACKET 0
#endif

#ifndef DA_PEC_BANK_TOKEN
#define DA_PEC_BANK_TOKEN 0
#endif

#ifndef DA_PEC_BANK_SA
#define DA_PEC_BANK_SA 0
#endif

#ifndef DA_PEC_MIN_SA_BYTE_COUNT
#define DA_PEC_MIN_SA_BYTE_COUNT 256
#endif

// enable gather-usage for AES-GCM
//#define DA_PEC_GATHER

// Enables multiple packet test
//#define DA_PEC_MULTIPLE_PKT_TEST

// Default DMA buffer allocation alignment (4 - 4 bytes)
#ifndef DA_PEC_DMA_ALIGNMENT_BYTE_COUNT
#define DA_PEC_DMA_ALIGNMENT_BYTE_COUNT 4
#endif

// Packet get retries
#ifndef DA_PEC_PKT_GET_RETRY_COUNT
#define DA_PEC_PKT_GET_RETRY_COUNT 10
#endif // DA_PEC_PKT_GET_RETRY_COUNT

// Packet get wait timeout (in milliseconds)
#ifndef DA_PEC_PKT_GET_TIMEOUT_MS
#define DA_PEC_PKT_GET_TIMEOUT_MS 10
#endif // DA_PEC_PKT_GET_RETRY_COUNT

#define AXI_MODIFICATIONS
#endif
/* end of file c_eip.h */
