/* lkm.h
 *
 * Linux Kernel Module (LKM) interface
 */

/*****************************************************************************
* Copyright (c) 2016-2020 by Rambus, Inc. and/or its subsidiaries.
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

#ifndef LKM_H_
#define LKM_H_


/*----------------------------------------------------------------------------
 * This module implements (provides) the following interface(s):
 */


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Driver Framework Basic Definitions API
#include "basic_defs.h"             // bool

// Driver Framework C Run-Time Library API
#include "clib.h"                   // memcmp

// Linux Kernel API
#include <linux/types.h>            // resource_size_t
#include <linux/compiler.h>         // __iomem


/*----------------------------------------------------------------------------
 * Definitions and macros
 */

typedef struct
{
    // Driver name
    char * driver_name_p;

    // Run-time power management data,
    // pointer to struct dev_pm_ops containing device PM callbacks
    void * pm_p;

    // Implementation-specific initialization data, see implementation notes
    void * custom_init_data_p;

    // identifier of the resource associated with the device controlled
    // by the device driver implemented in the LKM
    int res_id;

    // Resource size in bytes
    resource_size_t res_byte_count;

    // Use Message Signaled Interrupts, true - use, false - do not use
    bool f_use_msi;

    // true - keep the device resource mapped after LKM initialization,
    // false - unmap the device resource mapped after LKM initialization
    bool f_retain_map;

} lkm_init_t;


/*----------------------------------------------------------------------------
 * lkm_init
 *
 * Initializes the kernel module and registers it as a device driver
 * in the kernel.
 *
 * Return value
 *      >=0 - success
 *      <0  - failure
 */
int
lkm_init(
        lkm_init_t * const init_data_p);


/*-----------------------------------------------------------------------------
 * lkm_uninit
 *
 * Uninitializes the kernel module registered with the lkm_init() function.
 *
 * Return value
 *      None
 */
void
lkm_uninit(void);


/*-----------------------------------------------------------------------------
 * lkm_device_generic_get
 *
 * Returns pointer to a generic Linux kernel struct device.
 *
 * Return value
 *      Pointer to struct device.
 */
void *
lkm_device_generic_get(void);


/*-----------------------------------------------------------------------------
 * lkm_device_specific_get
 *
 * Returns pointer to a specific Linux kernel struct device,
 * such as pci_dev for PCI, or platform_device for platform.
 *
 * Return value
 *      Pointer to struct device.
 */
void *
lkm_device_specific_get(void);


/*-----------------------------------------------------------------------------
 * lkm_phys_base_addr_get
 *
 * Returns a pointer to a physical base address of the device memory mapped
 * IO space (physical resource address)
 *
 * Return value
 *      Pointer to a physical resource address of the device.
 */
void *
lkm_phys_base_addr_get(void);


/*-----------------------------------------------------------------------------
 * lkm_mapped_base_addr_get
 *
 * Returns a pointer to a mapped to the kernel memory base address
 * of the device memory mapped IO space (mapped or virtual resource address)
 *
 * Return value
 *      Pointer to a mapped resource address of the device.
 */
void __iomem *
lkm_mapped_base_addr_get(void);


/* end of file lkm.h */


#endif /* LKM_H_ */
