/* cs_hwpal_ext.h
 *
 * Security-IP-197 (FPGA) PCI chip specific configuration parameters
 */

/*****************************************************************************
* Copyright (c) 2011-2022 by Rambus, Inc. and/or its subsidiaries.
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

#ifndef CS_HWPAL_EXT_H_
#define CS_HWPAL_EXT_H_


/*----------------------------------------------------------------------------
 * Definitions and macros
 */

// For obtaining the IRQ number
#ifdef DRIVER_INTERRUPTS
#define HWPAL_INTERRUPTS
#endif

// FPGA board device ID.
#define HWPAL_DEVICE_ID             0x6018

// Xilinx PCI vendor ID
#define HWPAL_VENDOR_ID             0x10EE

#define HWPAL_MAGIC_PCICONFIGSPACE  0xFF434647      // 43 46 47 = C F G

#define HWPAL_REMAP_ADDRESSES   ;

#define HWPAL_DEVICE_TO_FIND       "PCI.0" // PCI Bar 0

#define HWPAL_DEVICE_ADD_PCI    HWPAL_DEVICE_ADD("PCI_CONFIG_SPACE", 0,  \
                                  HWPAL_MAGIC_PCICONFIGSPACE,            \
                                  HWPAL_MAGIC_PCICONFIGSPACE + 1024,     \
                                  7)

// definition of static resources inside the PCI device
// Refer to the data sheet of device for the correct values
//                       Name         DevNr  Start    Last  flags (see below)
#define HWPAL_DEVICES \
        HWPAL_DEVICE_ADD_PCI, \
        HWPAL_DEVICE_ADD("EIP197_GLOBAL", 0, 0,        0xfffff,  7),  \
        HWPAL_DEVICE_ADD("EIP207_FLUE0",  0, 0x00000,  0x01fff,  7),  \
        HWPAL_DEVICE_ADD("EIP202_CDR0",   0, 0x80000,  0x80fff,  7),  \
        HWPAL_DEVICE_ADD("EIP202_RDR0",   0, 0x80000,  0x80fff,  7),  \
        HWPAL_DEVICE_ADD("EIP202_CDR1",   0, 0x81000,  0x81fff,  7),  \
        HWPAL_DEVICE_ADD("EIP202_RDR1",   0, 0x81000,  0x81fff,  7),  \
        HWPAL_DEVICE_ADD("EIP202_CDR2",   0, 0x82000,  0x82fff,  7),  \
        HWPAL_DEVICE_ADD("EIP202_RDR2",   0, 0x82000,  0x82fff,  7),  \
        HWPAL_DEVICE_ADD("EIP202_CDR3",   0, 0x83000,  0x83fff,  7),  \
        HWPAL_DEVICE_ADD("EIP202_RDR3",   0, 0x83000,  0x83fff,  7),  \
        HWPAL_DEVICE_ADD("EIP202_CDR4",   0, 0x84000, 0x84fff, 7),  \
        HWPAL_DEVICE_ADD("EIP202_RDR4",   0, 0x84000, 0x84fff, 7),  \
        HWPAL_DEVICE_ADD("EIP202_CDR5",   0, 0x85000, 0x85fff, 7),  \
        HWPAL_DEVICE_ADD("EIP202_RDR5",   0, 0x85000, 0x85fff, 7),  \
        HWPAL_DEVICE_ADD("EIP202_CDR6",   0, 0x86000, 0x86fff, 7),  \
        HWPAL_DEVICE_ADD("EIP202_RDR6",   0, 0x86000, 0x86fff, 7),  \
        HWPAL_DEVICE_ADD("EIP202_CDR7",   0, 0x87000, 0x87fff, 7),  \
        HWPAL_DEVICE_ADD("EIP202_RDR7",   0, 0x87000, 0x87fff, 7),  \
        HWPAL_DEVICE_ADD("EIP202_CDR8",   0, 0x88000, 0x88fff, 7),  \
        HWPAL_DEVICE_ADD("EIP202_RDR8",   0, 0x88000, 0x88fff, 7),  \
        HWPAL_DEVICE_ADD("EIP202_CDR9",   0, 0x89000, 0x89fff, 7),  \
        HWPAL_DEVICE_ADD("EIP202_RDR9",   0, 0x89000, 0x89fff, 7),  \
        HWPAL_DEVICE_ADD("EIP202_CDR10",   0, 0x8a000, 0x8afff, 7),  \
        HWPAL_DEVICE_ADD("EIP202_RDR10",   0, 0x8a000, 0x8afff, 7),  \
        HWPAL_DEVICE_ADD("EIP202_CDR11",   0, 0x8b000, 0x8bfff, 7),  \
        HWPAL_DEVICE_ADD("EIP202_RDR11",   0, 0x8b000, 0x8bfff, 7),  \
        HWPAL_DEVICE_ADD("EIP202_CDR12",   0, 0x8c000, 0x8cfff, 7),  \
        HWPAL_DEVICE_ADD("EIP202_RDR12",   0, 0x8c000, 0x8cfff, 7),  \
        HWPAL_DEVICE_ADD("EIP202_CDR13",   0, 0x8d000, 0x8dfff, 7),  \
        HWPAL_DEVICE_ADD("EIP202_RDR13",   0, 0x8d000, 0x8dfff, 7),  \
        HWPAL_DEVICE_ADD("EIP201_GLOBAL", 0, 0x9f800,  0x9f8ff,  7),  \
        HWPAL_DEVICE_ADD("EIP201_RING0",  0, 0x9e800,  0x9e8ff,  7),  \
        HWPAL_DEVICE_ADD("EIP201_RING1",  0, 0x9d800,  0x9d8ff,  7),  \
        HWPAL_DEVICE_ADD("EIP201_RING2",  0, 0x9c800,  0x9c8ff,  7),  \
        HWPAL_DEVICE_ADD("EIP201_RING3",  0, 0x9b800,  0x9b8ff,  7),  \
        HWPAL_DEVICE_ADD("EIP74",         0, 0xf7000,  0xf707f,  7),  \
        HWPAL_DEVICE_ADD("EIP201_CS",     0, 0xf7800,  0xf78ff,  7),
        //HWPAL_DEVICE_ADD("BOARD_CTRL",    0, 0x200000,  0x20ffff, 7),

// Use the entire EIP-197 address map size + BOARD_CTRL
//TODO: or 0x400000 ? ddk doesn't program beyone 1MB
#define HWPAL_DEVICE_RESOURCE_BYTE_COUNT        0x100000

// flags:
//   bit0 = Trace reads (requires HWPAL_TRACE_DEVICE_READ)
//   bit1 = Trace writes (requires HWPAL_TRACE_DEVICE_WRITE)
//   bit2 = Swap word endianness (requires HWPAL_DEVICE_ENABLE_SWAP)


#define HWPAL_USE_MSI

// Enables DMA resources banks so that different memory regions can be used
// for DMA buffer allocation
#ifdef DRIVER_DMARESOURCE_BANKS_ENABLE
#define HWPAL_DMARESOURCE_BANKS_ENABLE
#endif // DRIVER_DMARESOURCE_BANKS_ENABLE

#ifdef HWPAL_DMARESOURCE_BANKS_ENABLE
// Definition of DMA banks, one dynamic and 1 static
//                                 bank    type   Shared  Cached  addr  Blocks   Block size
#define HWPAL_DMARESOURCE_BANKS                                                              \
  HWPAL_DMARESOURCE_BANK_ADD (0,       0,     0,      1,      0,    0,         0),     \
    HWPAL_DMARESOURCE_BANK_ADD (1,       1,     1,      1,      0,                       \
    DRIVER_DMA_BANK_ELEMENT_COUNT,                           \
                DRIVER_DMA_BANK_ELEMENT_BYTE_COUNT)
#endif // HWPAL_DMARESOURCE_BANKS_ENABLE

// Virtex7 FPGA PCIe AXI bridge uses posted-writes for the EIP-197 slave
// interface, this is a workaround (wait specified time before reading
// the register back)
// Note: fast write-read sequences are used by the Global Control driver


#endif /* CS_HWPAL_EXT_H_ */


/* end of file cs_hwpal_ext.h */
