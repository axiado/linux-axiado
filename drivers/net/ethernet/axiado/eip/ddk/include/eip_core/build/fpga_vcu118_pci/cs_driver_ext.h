/* cs_driver_ext.h
 *
 * Top-level Product configuration Settings specific for FPGA.
 */

/*****************************************************************************
* Copyright (c) 2010-2020 by Rambus, Inc. and/or its subsidiaries.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are
* met:
*
* 1. Redistributions of source code must retain the above copyright
* notice, this list of conditions and the following disclaimer.
*
* 2. Redistributions in binary form must reproduce the above copyright
* notice, this list of conditions and the following disclaimer in the
* documentation and/or other materials provided with the distribution.
*
* 3. Neither the name of the copyright holder nor the names of its
* contributors may be used to endorse or promote products derived from
* this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
* A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
* HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
* THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*****************************************************************************/

#ifndef INCLUDE_GUARD_CS_DRIVER_EXT_H
#define INCLUDE_GUARD_CS_DRIVER_EXT_H

// Driver license for the module registration with the Linux kernel
#define DRIVER_LICENSE  "Dual BSD/GPL"

// FPGA DMA buffer alignment is 16 bytes
#define DRIVER_DMA_ALIGNMENT_BYTE_COUNT     32

// Disable usage of the EIP-202 Ring Arbiter
//#define DRIVER_EIP202_RA_DISABLE

// Enables DMA banks
#define DRIVER_DMARESOURCE_BANKS_ENABLE


// DMA bank to use for SA's  (PEC API)
#define DRIVER_PEC_BANK_SA                             1 // Static bank

// DMA resource bank for transform records allocation
// by the PCL API implementation, static bank
#define DRIVER_PCL_BANK_TRANSFORM                      DRIVER_PEC_BANK_SA

// DMA resource bank for flow records allocation by the PCL API implementation,
// static bank
#define DRIVER_PCL_BANK_FLOW                           DRIVER_PCL_BANK_TRANSFORM

// Each static bank for DMA resources must have 2 lists
#define DRIVER_LIST_HWPAL_MAX_NOF_INSTANCES            2


/****************************************************************************
 * Adapter Global configuration parameters
 */

// FPGA board specific parameters
//#define ADAPTER_GLOBAL_BOARDCTRL_SUPPORT_ENABLE
//#define ADAPTER_GLOBAL_FPGA_HW_RESET_ENABLE


#endif /* Include Guard */


/* end of file cs_driver_ext.h */
