/* device_lkm.c
 *
 * This is the Linux Kernel-mode Driver Framework v4 device API
 * implementation for open Firmware. The implementation is device-agnostic and
 * receives configuration details from the c_device_lkm.h file.
 *
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

/*----------------------------------------------------------------------------
 * This module implements (provides) the following interface(s):
 */

#include "device_mgmt.h"            // API to implement
#include "device_rw.h"              // API to implement

// Driver Framework device internal interface
#include "device_internal.h"        // Device_Internal_*


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Default configuration
#include "c_device_lkm.h"

// Driver Framework device API
#include "device_swap.h"            // device_swap_endian32

// Driver Framework device platform interface
#include "device_platform.h"        // Device_Platform_*

#ifdef HWPAL_DEVICE_USE_RPM
// Runtime power Management Kernel Macros API
#include "rpm_kernel_macros.h"      // RPM_*
#endif

// Logging API
#include "log.h"                    // LOG_*

// Driver Framework C Run-Time Library API
#include "clib.h"                   // memcmp

// Driver Framework Basic Definitions API
#include "basic_defs.h"             // u32, NULL, inline, bool,
                                    // IDENTIFIER_NOT_USED

// Linux Kernel Module interface
#include "lkm.h"

// Linux Kernel API
#include <linux/platform_device.h>  // platform_*,
#include <asm/io.h>                 // ioread32, iowrite32
#include <linux/compiler.h>         // __iomem
#include <linux/slab.h>             // kmalloc, kfree
#include <linux/hardirq.h>          // in_atomic


/*----------------------------------------------------------------------------
 * Definitions and macros
 */

#define HWPAL_FLAG_READ     BIT_0   // 1
#define HWPAL_FLAG_WRITE    BIT_1   // 2
#define HWPAL_FLAG_SWAP     BIT_2   // 4
#define HWPAL_FLAG_HA       BIT_5   // 32

// c_device_lkm.h file defines a HWPAL_REMAP_ADDRESSES that
// depends on HWPAL_REMAP_ONE
#define HWPAL_REMAP_ONE(_old, _new) \
    case _old: \
        device_byte_offset = _new; \
        break;


/*----------------------------------------------------------------------------
 * Local variables
 */

// statically configured devices
static const device_admin_static_t hwpal_lib_devices_static[] =
{
    HWPAL_DEVICES
};

// number of statically configured devices
#define HWPAL_DEVICE_STATIC_COUNT \
                (sizeof(hwpal_lib_devices_static) \
                            / sizeof(device_admin_static_t))

// All supported devices
static device_admin_t * hwpal_lib_devices_p [HWPAL_DEVICE_COUNT];

// Global administration data
static device_global_admin_t hwpal_lib_device_global;


/*----------------------------------------------------------------------------
 * hwpal_hexdump
 *
 * This function hex-dumps an array of u32.
 */
#if ((defined(HWPAL_TRACE_DEVICE_READ)) || (defined(HWPAL_TRACE_DEVICE_WRITE)))
static void
hwpal_hexdump(
        const char * array_name_p,
        const char * device_name_p,
        const unsigned int byte_offset,
        const u32 * word_array_p,
        const unsigned int word_count,
        bool f_swap_endianness)
{
    unsigned int i;

    log_formatted_message(
        "%s: "
        "byte offsets 0x%x - 0x%x"
        " (%s)\n"
        "  ",
        array_name_p,
        byte_offset,
        byte_offset + word_count*4 -1,
        device_name_p);

    for (i = 1; i <= word_count; i++)
    {
        u32 value = word_array_p[i - 1];

        if (f_swap_endianness)
            value = device_swap_endian32(value);

        log_formatted_message(" 0x%08x", value);

        if ((i & 7) == 0)
            log_message("\n  ");
    }

    if ((word_count & 7) != 0)
        log_message("\n");
}
#endif


/*----------------------------------------------------------------------------
 * device_remap_device_address
 *
 * This function remaps certain device addresses (relative within the whole
 * device address map) to other addresses. This is needed when the integration
 * has remapped some EIP device registers to other addresses. The EIP Driver
 * Libraries assume the devices always have the same internal layout.
 */
static inline unsigned int
device_remap_device_address(
        unsigned int device_byte_offset)
{
#ifdef HWPAL_REMAP_ADDRESSES
    switch(device_byte_offset)
    {
        // include the remap statements
        HWPAL_REMAP_ADDRESSES

        default:
            break;
    }
#endif

    return device_byte_offset;
}


/*----------------------------------------------------------------------------
 * HWPALLib_Device2RecPtr
 *
 * This function converts an device_handle_t received via one of the
 * device API functions into a hwpal_lib_devices_p record pointer, if it is
 * valid.
 *
 * Return value
 *     NULL    Provided device handle was not valid
 *     other   Pointer to a device_admin_t record
 */
static inline device_admin_t *
hwpal_lib_device2_record_ptr(
        device_handle_t device)
{
    device_admin_t * p = (void *)device;

    if (p == NULL)
        return NULL;

#ifdef HWPAL_DEVICE_MAGIC
    if (p->magic != HWPAL_DEVICE_MAGIC)
        return NULL;
#endif

    return p;
}


/*----------------------------------------------------------------------------
 * hwpal_lib_is_valid
 *
 * This function checks that the parameters are valid to make the access.
 *
 * device_p is valid
 * byte_offset is 32-bit aligned
 * byte_offset is inside device memory range
 */
static inline bool
hwpal_lib_is_valid(
        const device_admin_t * const device_p,
        const unsigned int byte_offset)
{
    if (device_p == NULL)
        return false;

    if (byte_offset & 3)
        return false;

    if (device_p->first_ofs + byte_offset > device_p->last_ofs)
        return false;

    return true;
}


/*-----------------------------------------------------------------------------
 * device_internal interface
 *
 */

/*----------------------------------------------------------------------------
 * device_internal_static_count_get
 */
unsigned int
device_internal_static_count_get(void)
{
    return HWPAL_DEVICE_STATIC_COUNT;
}


/*----------------------------------------------------------------------------
 * device_internal_count_get
 */
unsigned int
device_internal_count_get(void)
{
    return HWPAL_DEVICE_COUNT;
}


/*----------------------------------------------------------------------------
 * device_internal_admin_static_get
 */
const device_admin_static_t *
device_internal_admin_static_get(void)
{
    return hwpal_lib_devices_static;
}


/*----------------------------------------------------------------------------
 * device_internal_admin_get
 *
 * Returns pointer to the memory location where the device list is stored.
 *
 */
device_admin_t **
device_internal_admin_get(void)
{
    return hwpal_lib_devices_p;
}


/*----------------------------------------------------------------------------
 * device_internal_admin_global_get
 */
device_global_admin_t *
device_internal_admin_global_get(void)
{
    return &hwpal_lib_device_global;
}


/*----------------------------------------------------------------------------
 * device_internal_alloc
 */
void *
device_internal_alloc(
        unsigned int byte_count)
{
    return kmalloc(byte_count, in_atomic() ? GFP_ATOMIC : GFP_KERNEL);
}


/*----------------------------------------------------------------------------
 * device_internal_free
 */
void
device_internal_free(
        void * ptr)
{
    kfree(ptr);
}


/*-----------------------------------------------------------------------------
 * device_internal_initialize
 */
int
device_internal_initialize(
        void * custom_init_data_p)
{
    unsigned int i;
    lkm_init_t  lkm_init_data;

    zeroinit(lkm_init_data);

    lkm_init_data.driver_name_p        = HWPAL_DRIVER_NAME;
    lkm_init_data.res_id               = HWPAL_DEVICE_RESOURCE_ID;
    lkm_init_data.res_byte_count        = HWPAL_DEVICE_RESOURCE_BYTE_COUNT;
    lkm_init_data.f_retain_map          = true;

#ifdef HWPAL_DEVICE_USE_RPM
    lkm_init_data.pm_p                = RPM_OPS_PM;
#endif

    if (lkm_init(&lkm_init_data) < 0)
    {
        LOG_CRIT("%s: Failed to register the platform device\n", __func__);
        return -1;
    }

    // Output default IRQ line number in custom data
    {
        int * p = (int *)custom_init_data_p;

        int * irq_p = (int *)lkm_init_data.custom_init_data_p;

        *p = irq_p[0];
    }

#ifdef HWPAL_DEVICE_USE_RPM
    if (RPM_INIT_MACRO(lkm_device_generic_get()) != RPM_SUCCESS)
    {
        LOG_CRIT("%s: RPM_Init() failed\n", __func__);
        lkm_uninit();
        return -3; // error
    }
#endif

    hwpal_lib_device_global.platform.mapped_base_addr_p
                                        = lkm_mapped_base_addr_get();
    hwpal_lib_device_global.platform.platform_device_p
                                        = lkm_device_specific_get();
    hwpal_lib_device_global.platform.phys_base_addr
                                        = lkm_phys_base_addr_get();

    for(i = 0; i < HWPAL_DEVICE_COUNT; i++)
        if (hwpal_lib_devices_p[i])
        {
            LOG_INFO("%s: mapped device '%s', "
                     "virt base addr 0x%p, "
                     "start byte offset 0x%x, "
                     "last byte offset 0x%x\n",
                     __func__,
                     hwpal_lib_devices_p[i]->dev_name,
                     hwpal_lib_device_global.platform.mapped_base_addr_p,
                     hwpal_lib_devices_p[i]->first_ofs,
                     hwpal_lib_devices_p[i]->last_ofs);
        }
    return 0;
}


/*-----------------------------------------------------------------------------
 * device_internal_uninitialize
 */
void
device_internal_uninitialize(void)
{

#ifdef HWPAL_DEVICE_USE_RPM
    // Check if a race condition is possible here with auto-suspend timer
    (void)RPM_UNINIT_MACRO();
#endif

    lkm_uninit();

    // Reset global administration
    zeroinit(hwpal_lib_device_global);
}


/*-----------------------------------------------------------------------------
 * device_internal_find
 */
device_handle_t
device_internal_find(
        const char * device_name_p,
        const unsigned int index)
{
    IDENTIFIER_NOT_USED(device_name_p);

    // Return the device handle
    return (device_handle_t)hwpal_lib_devices_p[index];
}


/*-----------------------------------------------------------------------------
 * device_internal_get_index
 */
int
device_internal_get_index(
        const device_handle_t device)
{
    device_admin_t * device_p;

#ifdef HWPAL_STRICT_ARGS_CHECK
    device_p = hwpal_lib_device2_record_ptr(device);
#else
    device_p = device;
#endif

#ifdef HWPAL_STRICT_ARGS_CHECK
    if (!hwpal_lib_is_valid(device_p, 0))
    {
        LOG_CRIT("%s: invalid device (%p) or byte_offset (%u)\n",
                 __func__,
                 device,
                 0);
        return -1;
    }
#endif

    return device_p->device_id;
}


/*-----------------------------------------------------------------------------
 * device_mgmt API
 *
 * These functions support finding a device given its name.
 * A handle is returned that is needed in the device_rw API
 * to read or write the device
 */


/*-----------------------------------------------------------------------------
 * eip_device_get_reference
 */
device_reference_t
eip_device_get_reference(
        const device_handle_t device,
        device_data_t * const data_p)
{
    device_reference_t dev_reference;

    // There exists only one reference for this implementation
    IDENTIFIER_NOT_USED(device);

    if (!hwpal_lib_device_global.f_initialized)
    {
        LOG_CRIT("%s: failed, not initialized\n", __func__);
        return NULL;
    }

    // Return the platform device reference
    // (pointer to the Linux device structure)
    dev_reference = &hwpal_lib_device_global.platform.platform_device_p->dev;

    if (data_p)
        data_p->phys_addr = NULL;  // TODO this is called to get DMA etc as welll.. old-customization has some base-address

    return dev_reference;
}


/*-----------------------------------------------------------------------------
 * device_rw API
 *
 * These functions can be used to transfer a single 32bit word or an array of
 * 32bit words to or from a device.
 * Endianness swapping is performed on the fly based on the configuration for
 * this device.
 *
 */

/*-----------------------------------------------------------------------------
 * device_read32
 */
u32
device_read32(
        const device_handle_t device,
        const unsigned int byte_offset)
{
    u32 value = 0;
    device_read32_check(device, byte_offset, &value);
    return value;
}


/*-----------------------------------------------------------------------------
 * device_read32_check
 */
int
device_read32_check(
        const device_handle_t device,
        const unsigned int byte_offset,
        u32 * const value_p)
{
    device_admin_t * device_p;
    u32 value = 0;

    if (!hwpal_lib_device_global.f_initialized)
    {
        LOG_CRIT("%s: failed, not initialized\n", __func__);
        return DEVICE_RW_PARAM_ERROR;
    }

#ifdef HWPAL_STRICT_ARGS_CHECK
    device_p = hwpal_lib_device2_record_ptr(device);
#else
    device_p = device;
#endif

#ifdef HWPAL_STRICT_ARGS_CHECK
    if (!hwpal_lib_is_valid(device_p, byte_offset))
    {
        LOG_CRIT("%s: invalid device (%p) or byte_offset (%u)\n",
                 __func__,
                 device,
                 byte_offset);
        return DEVICE_RW_PARAM_ERROR;
    }
#endif

#ifdef HWPAL_ENABLE_HA_SIMULATION
    if (device_p->flags & HWPAL_FLAG_HA)
    {
        // HA simulation mode
        // disable access to PKA_MASTER_SEQ_CTRL
        if (byte_offset == 0x3FC8)
        {
            value = 0;
            goto HA_SKIP;
        }
    }
#endif

    {
        unsigned int device_byte_offset = device_p->first_ofs + byte_offset;

        device_byte_offset = device_remap_device_address(device_byte_offset);

#ifdef HWPAL_DEVICE_DIRECT_MEMIO
        value = *(u32 *)(uintptr_t)(hwpal_lib_device_global.platform.mapped_base_addr_p +
                                                (device_byte_offset / 4));
#else
        value = ioread32(hwpal_lib_device_global.platform.mapped_base_addr_p +
                                                (device_byte_offset / 4));
#endif

#ifdef HWPAL_DEVICE_ENABLE_SWAP
        if (device_p->flags & HWPAL_FLAG_SWAP)
            value = device_swap_endian32(value);
#endif

        smp_rmb();
    }

#ifdef HWPAL_ENABLE_HA_SIMULATION
HA_SKIP:
#endif

#ifdef HWPAL_TRACE_DEVICE_READ
    if (device_p->flags & HWPAL_FLAG_READ)
    {
        unsigned int device_byte_offset = device_p->first_ofs + byte_offset;
        unsigned int device_byte_offset2 =
                device_remap_device_address(device_byte_offset);

        if (device_byte_offset2 != device_byte_offset)
        {
            device_byte_offset2 -= device_p->first_ofs;
            log_formatted_message("%s: 0x%x(was 0x%x) = 0x%08x (%s)\n",
                                 __func__,
                                 device_byte_offset2,
                                 byte_offset,
                                 (unsigned int)value,
                                 device_p->dev_name);
        }
        else
            log_formatted_message("%s: 0x%x = 0x%08x (%s)\n",
                                 __func__,
                                 byte_offset,
                                 (unsigned int)value,
                                 device_p->dev_name);
    }
#endif /* HWPAL_TRACE_DEVICE_READ */

    *value_p = value;

    return 0;
}


/*-----------------------------------------------------------------------------
 * device_write32
 */
int
device_write32(
        const device_handle_t device,
        const unsigned int byte_offset,
        const u32 value_in)
{
    device_admin_t * device_p;
    u32 value = value_in;

    if (!hwpal_lib_device_global.f_initialized)
    {
        LOG_CRIT("%s: failed, not initialized\n", __func__);
        return DEVICE_RW_PARAM_ERROR;
    }

#ifdef HWPAL_STRICT_ARGS_CHECK
    device_p = hwpal_lib_device2_record_ptr(device);
#else
    device_p = device;
#endif

#ifdef HWPAL_STRICT_ARGS_CHECK
    if (!hwpal_lib_is_valid(device_p, byte_offset))
    {
        LOG_CRIT("%s: Invalid device (%p) or byte_offset (%u)\n",
                 __func__,
                 device,
                 byte_offset);
        return DEVICE_RW_PARAM_ERROR;
    }
#endif

#ifdef HWPAL_TRACE_DEVICE_WRITE
    if (device_p->flags & HWPAL_FLAG_WRITE)
        log_formatted_message("%s: 0x%x = 0x%08x (%s)\n",
                             __func__,
                             byte_offset,
                             (unsigned int)value,
                             device_p->dev_name);
#endif /* HWPAL_TRACE_DEVICE_WRITE*/

#ifdef HWPAL_ENABLE_HA_SIMULATION
    if (device_p->flags & HWPAL_FLAG_HA)
    {
        // HA simulation mode
        // disable access to PKA_MASTER_SEQ_CTRL
        if (byte_offset == 0x3FC8)
        {
            LOG_CRIT("%s: Unexpected write to PKA_MASTER_SEQ_CTRL\n",
                        __func__);
            return 0;
        }
    }
#endif

    {
        u32 device_byte_offset = device_p->first_ofs + byte_offset;

        device_byte_offset = device_remap_device_address(device_byte_offset);

#ifdef HWPAL_DEVICE_ENABLE_SWAP
        if (device_p->flags & HWPAL_FLAG_SWAP)
            value = device_swap_endian32(value);
#endif

#ifdef HWPAL_DEVICE_DIRECT_MEMIO
        *(u32 *)(uintptr_t)(hwpal_lib_device_global.platform.mapped_base_addr_p +
                                        (device_byte_offset / 4)) = value;
#else
        iowrite32(value,
                  hwpal_lib_device_global.platform.mapped_base_addr_p +
                                                  (device_byte_offset / 4));
#endif

        smp_wmb();
    }

    return 0;
}


/*-----------------------------------------------------------------------------
 * device_read32_array
 */
int
device_read32_array(
        const device_handle_t device,
        const unsigned int start_byte_offset, // read starts here, +4 increments
        u32 * memory_dst_p,             // writing starts here
        const int count)                    // number of uint32's to transfer
{
    device_admin_t * device_p;
    unsigned int device_byte_offset;

    if (!hwpal_lib_device_global.f_initialized)
    {
        LOG_CRIT("%s: failed, not initialized\n", __func__);
        return DEVICE_RW_PARAM_ERROR;
    }

#ifdef HWPAL_STRICT_ARGS_CHECK
    device_p = hwpal_lib_device2_record_ptr(device);
#else
    device_p = device;
#endif

#ifdef HWPAL_STRICT_ARGS_CHECK
    if ((count <= 0) ||
        memory_dst_p == NULL ||
        !hwpal_lib_is_valid(device_p, start_byte_offset) ||
        !hwpal_lib_is_valid(device_p, start_byte_offset + (count - 1) * 4))
    {
        LOG_CRIT("%s: Invalid device (%p) or read area (%u-%u)\n",
                 __func__,
                 device,
                 start_byte_offset,
                 (unsigned int)(start_byte_offset +
                          (count - 1) * sizeof(u32)));
         return DEVICE_RW_PARAM_ERROR;
    }
#endif

#ifdef HWPAL_ENABLE_HA_SIMULATION
    if (device_p->flags & HWPAL_FLAG_HA)
    {
        // HA simulation mode
        // disable access to PKA_MASTER_SEQ_CTRL
        return 0;
    }
#endif

    device_byte_offset = device_p->first_ofs + start_byte_offset;

    {
        unsigned int remapped_offset;
        u32 value;
        int i;

#ifdef HWPAL_DEVICE_ENABLE_SWAP
        bool f_swap = false;

        if (device_p->flags & HWPAL_FLAG_SWAP)
            f_swap = true;
#endif
        for (i = 0; i < count; i++)
        {
            remapped_offset = device_remap_device_address(device_byte_offset);

#ifdef HWPAL_DEVICE_DIRECT_MEMIO
            value =
               *(u32*)(uintptr_t)(hwpal_lib_device_global.platform.mapped_base_addr_p +
                                                (remapped_offset / 4));
#else
            value = ioread32(hwpal_lib_device_global.platform.mapped_base_addr_p +
                                                (remapped_offset / 4));
#endif

            smp_rmb();

#ifdef HWPAL_DEVICE_ENABLE_SWAP
            // swap endianness if required
            if (f_swap)
                value = device_swap_endian32(value);
#endif

            memory_dst_p[i] = value;
            device_byte_offset +=  4;
        } // for
    }

#ifdef HWPAL_TRACE_DEVICE_READ
    if (device_p->flags & HWPAL_FLAG_READ)
    {
        hwpal_hexdump(
                      __func__,
                      device_p->dev_name,
                      device_p->first_ofs + start_byte_offset,
                      memory_dst_p,
                      count,
                      false);     // already swapped during read above
    }
#endif /* HWPAL_TRACE_DEVICE_READ */
    return 0;
}


/*----------------------------------------------------------------------------
 * device_write32_array
 */
int
device_write32_array(
        const device_handle_t device,
        const unsigned int start_byte_offset, // write starts here, +4 increments
        const u32 * memory_src_p,       // reading starts here
        const int count)                    // number of uint32's to transfer
{
    device_admin_t * device_p;
    unsigned int device_byte_offset;

    if (memory_src_p == NULL || count <= 0)
        return DEVICE_RW_PARAM_ERROR;

    if (!hwpal_lib_device_global.f_initialized)
    {
        LOG_CRIT("%s: failed, not initialized\n", __func__);
        return DEVICE_RW_PARAM_ERROR;
    }

#ifdef HWPAL_STRICT_ARGS_CHECK
    device_p = hwpal_lib_device2_record_ptr(device);
#else
    device_p = device;
#endif

#ifdef HWPAL_STRICT_ARGS_CHECK
    if ((count <= 0) ||
        memory_src_p == NULL ||
        !hwpal_lib_is_valid(device_p, start_byte_offset) ||
        !hwpal_lib_is_valid(device_p, start_byte_offset + (count - 1) * 4))
    {
        LOG_CRIT("%s: Invalid device (%p) or write area (%u-%u)\n",
                 __func__,
                 device,
                 start_byte_offset,
                 (unsigned int)(start_byte_offset + (count - 1) *
                                                 sizeof(u32)));
        return DEVICE_RW_PARAM_ERROR;
    }
#endif

    device_byte_offset = device_p->first_ofs + start_byte_offset;

#ifdef HWPAL_ENABLE_HA_SIMULATION
    if (device_p->flags & HWPAL_FLAG_HA)
    {
        // HA simulation mode
        // disable access to PKA_MASTER_SEQ_CTRL
        return 0;
    }
#endif

#ifdef HWPAL_TRACE_DEVICE_WRITE
    if (device_p->flags & HWPAL_FLAG_WRITE)
    {
        bool f_swap = false;

#ifdef HWPAL_DEVICE_ENABLE_SWAP
        if (device_p->flags & HWPAL_FLAG_SWAP)
            f_swap = true;
#endif

        hwpal_hexdump(
                      __func__,
                      device_p->dev_name,
                      device_byte_offset,
                      memory_src_p,
                      count,
                      f_swap);
    }
#endif /* HWPAL_TRACE_DEVICE_WRITE */

    {
        unsigned int remapped_offset;
        u32 value;
        int i;

#ifdef HWPAL_DEVICE_ENABLE_SWAP
        bool f_swap = false;

        if (device_p->flags & HWPAL_FLAG_SWAP)
            f_swap = true;
#endif

        for (i = 0; i < count; i++)
        {
            remapped_offset = device_remap_device_address(device_byte_offset);
            value = memory_src_p[i];

#ifdef HWPAL_DEVICE_ENABLE_SWAP
            if (f_swap)
                value = device_swap_endian32(value);
#endif

#ifdef HWPAL_DEVICE_DIRECT_MEMIO
            *(u32*)(uintptr_t)(hwpal_lib_device_global.platform.mapped_base_addr_p +
                                            (remapped_offset / 4)) = value;
#else
            iowrite32(value, hwpal_lib_device_global.platform.mapped_base_addr_p +
                                                (remapped_offset / 4));
#endif

            smp_wmb();

            device_byte_offset += 4;
        } // for
    }

    return 0;
}


/* end of file device_lkm.c */
