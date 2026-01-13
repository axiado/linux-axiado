/* lkm.c
 *
 * Linux Kernel Module implementation for platform device drivers
 *
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

/*----------------------------------------------------------------------------
 * This module implements (provides) the following interface(s):
 */

// Linux Kernel Module interface
#include "lkm.h"


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Default configuration
#include "c_lkm.h"

// Driver Framework Basic Definitions API
#include "basic_defs.h"             // u32, NULL, inline, bool,
                                    // IDENTIFIER_NOT_USED

// Driver Framework C Run-Time Library API
#include "clib.h"                   // memcmp

// Logging API
#undef LOG_SEVERITY_MAX
#define LOG_SEVERITY_MAX  LKM_LOG_SEVERITY
#include "log.h"                    // LOG_*

// Linux Kernel API
#include <linux/types.h>            // phys_addr_t, resource_size_t
#include <linux/version.h>          // LINUX_VERSION_CODE, KERNEL_VERSION
#include <linux/device.h>           // struct device
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>  // platform_*,
#include <linux/of_platform.h>      // of_*,
#include <asm/io.h>                 // ioremap, iounmap
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/moduleparam.h>
#include <linux/clk.h>              // clk_*


/*----------------------------------------------------------------------------
 * Definitions and macros
 */


// LKM PCI implementation administration data
typedef struct
{
    // declarations native to Linux kernel
    struct platform_device * platform_device_p;

    // virtual address returned by ioremap()
    u32 __iomem * mapped_base_addr_p;

    // physical address passed to ioremap()
    phys_addr_t phys_base_addr;

    // platform device driver data
    struct platform_driver platform_driver;

    // device resource (IO space) identifier
    int res_id;

    // device resource size in bytes
    resource_size_t res_byte_count;

    // device virtual IRQ number
    int virt_irq_nr [LKM_PLATFORM_IRQ_COUNT];

    bool f_retain_map;

    // Initialization flag, true - initialized, otherwise - false
    bool f_initialized;

} lkm_admin_t;


/*----------------------------------------------------------------------------
 * Local variables
 */
static struct clk *gateclk;

static const struct of_device_id lkm_dt_id_table[] =
{
    { .compatible = LKM_PLATFORM_DEVICE_NAME, },
    { },
};

// LKM administration data
static lkm_admin_t lkm_admin;


/*----------------------------------------------------------------------------
 * lkm_probe
 */
static int
lkm_probe(
        struct platform_device * platform_device_p)
{
    int i;
    lkm_admin_t * p = &lkm_admin;
    struct resource * rsc_p = NULL;

    LOG_INFO(LKM_LOG_PREFIX "%s: entered\n", __func__);

    if (platform_device_p == NULL)
    {
        LOG_CRIT(LKM_LOG_PREFIX
                 "%s: failed, missing platform device\n",
                 __func__);
        return -ENODEV;
    }

    if (of_find_compatible_node(NULL, NULL, LKM_PLATFORM_DEVICE_NAME))
    {
        LOG_INFO(LKM_LOG_PREFIX
                 "%s: found requested device %s\n",
                 __func__,
                 LKM_PLATFORM_DEVICE_NAME);
    }
    else
    {
        LOG_CRIT(LKM_LOG_PREFIX
                 "%s: device %s not supported\n",
                 __func__,
                 LKM_PLATFORM_DEVICE_NAME);
        return -ENODEV;
    }

    // remember the device reference
    p->platform_device_p = platform_device_p;

    if (p->f_retain_map)
    {
        // get platform device physical address
        // Exported under GPL
        rsc_p = platform_get_resource(platform_device_p,
                                      IORESOURCE_MEM,
                                      p->res_id);

        // device tree specific for this OF device
        // only 32-bit physical addresses are supported
        if(rsc_p)
        {
            LOG_INFO(LKM_LOG_PREFIX
                     "%s: mem start=%08x, end=%08x, flags=%08x\n",
                     __func__,
                     (unsigned int)rsc_p->start,
                     (unsigned int)rsc_p->end,
                     (unsigned int)rsc_p->flags);
        }
        else
        {
            LOG_CRIT(LKM_LOG_PREFIX
                     "%s: memory resource id %d not found\n",
                     __func__,
                     p->res_id);
            return -ENODEV;
        }

        p->phys_base_addr      = rsc_p->start;
        p->res_byte_count      = rsc_p->end - rsc_p->start + 1;

        // now map the chip into kernel memory
        // so we can access the EIP static resources

        // note: ioremap is uncached by default
        p->mapped_base_addr_p = ioremap(p->phys_base_addr, p->res_byte_count);
        if (p->mapped_base_addr_p == NULL)
        {
            LOG_CRIT(LKM_LOG_PREFIX
                     "%s: failed to ioremap platform driver %s, "
                     "resource id %d, phys addr 0x%p, size %ul\n",
                     __func__,
                     p->platform_driver.driver.name,
                     p->res_id,
                     (void*)p->phys_base_addr,
                     (unsigned int)p->res_byte_count);
            return -ENODEV;
        }

        LOG_INFO(LKM_LOG_PREFIX
                 "%s: Mapped platform driver %s addr %p, phys addr 0x%p, "
                 "sizeof(resource_size_t)=%d\n resource id=%d, size=%ul\n",
                 __func__,
                 p->platform_driver.driver.name,
                 p->mapped_base_addr_p,
                 (void*)p->phys_base_addr,
                 (int)sizeof(resource_size_t),
                 p->res_id,
                 (unsigned int)p->res_byte_count);
    }

    // Optional device clock control functionality,
    // only required when the device clock needs to be enabled via the kernel
    gateclk = clk_get(&platform_device_p->dev, NULL);
    if (!IS_ERR(gateclk))
    {
        // Exported under GPL
        clk_prepare_enable(gateclk);
        LOG_INFO("%s: clk_prepare_enable() successful\n", __func__);
    }
    else
    {
        // Not all devices support it
        LOG_INFO("%s: clk_get() could not obtain clock\n", __func__);
    }

    for (i = 0; i < LKM_PLATFORM_IRQ_COUNT; i++)
    {
        int irq_nr, irq_index;

        irq_index = LKM_PLATFORM_IRQ_COUNT > 1 ? i : LKM_PLATFORM_IRQ_IDX;

        // Exported under GPL
        irq_nr = platform_get_irq(platform_device_p, irq_index);
        if (irq_nr < 0)
        {
            LOG_INFO(LKM_LOG_PREFIX "%s: failed to get IRQ for index %d\n",
                     __func__,
                     irq_index);
            return -ENODEV;
        }
        else
            p->virt_irq_nr[i] = irq_nr;
    }

    LOG_INFO(LKM_LOG_PREFIX "%s: left\n", __func__);

    // return 0 to indicate "we decided to take ownership"
    return 0;
}


/*----------------------------------------------------------------------------
 * lkm_remove
 */
static int
lkm_remove(
        struct platform_device * platform_device_p)
{
    lkm_admin_t * p = &lkm_admin;

    LOG_INFO(LKM_LOG_PREFIX "%s: entered\n", __func__);

    if (p->platform_device_p != platform_device_p)
    {
        LOG_CRIT(LKM_LOG_PREFIX
                 "%s: failed, missing or wrong platform device\n",
                 __func__);
        return -ENODEV;
    }

    LOG_INFO(LKM_LOG_PREFIX
             "%s: mapped base addr=%p\n", __func__, p->mapped_base_addr_p);

    // Optional device clock control functionality
    if (!IS_ERR(gateclk))
    {
        // Exported under GPL
        clk_disable_unprepare(gateclk);
        clk_put(gateclk);
    }

    if (p->mapped_base_addr_p && p->f_retain_map)
    {
        iounmap(p->mapped_base_addr_p);
        p->mapped_base_addr_p = NULL;
    }

    LOG_INFO(LKM_LOG_PREFIX "%s: left\n", __func__);

    return 0;
}


/*-----------------------------------------------------------------------------
 * lkm_init
 */
int
lkm_init(
        lkm_init_t * const init_data_p)
{
    int status;
    lkm_admin_t * p = &lkm_admin;

    LOG_INFO(LKM_LOG_PREFIX "%s: entered\n", __func__);

    // Check input parameters
    if(init_data_p == NULL)
    {
        LOG_CRIT(LKM_LOG_PREFIX "%s: failed, missing init data\n", __func__);
        return -1;
    }

    if (p->f_initialized)
    {
        LOG_CRIT(LKM_LOG_PREFIX "%s: failed, already initialized\n", __func__);
        return -2;
    }

    // Fill in PCI device driver data
    p->f_retain_map                    = init_data_p->f_retain_map;
    p->res_id                         = init_data_p->res_id; // not used

    p->platform_driver.probe         = lkm_probe;
    p->platform_driver.remove        = lkm_remove;

    p->platform_driver.driver.name            = init_data_p->driver_name_p;
    p->platform_driver.driver.owner           = THIS_MODULE;
    p->platform_driver.driver.pm              = init_data_p->pm_p;
    p->platform_driver.driver.of_match_table  = lkm_dt_id_table;

    // Exported under GPL
    status = platform_driver_register(&p->platform_driver);
    if (status < 0)
    {
        LOG_CRIT(LKM_LOG_PREFIX
                 "%s: failed to register platform device driver\n",
                 __func__);
        return -3;
    }

    if (p->platform_device_p == NULL)
    {
        LOG_CRIT(LKM_LOG_PREFIX "%s: failed, no device detected\n", __func__);
        platform_driver_unregister(&p->platform_driver);
        return -4;
    }

    // if provided, custom_init_data_p points to an "int"
    // we return a pointer to the arrays of irq numbers
    init_data_p->custom_init_data_p = p->virt_irq_nr;

    p->f_initialized = true;

    LOG_INFO(LKM_LOG_PREFIX "%s: left\n", __func__);

    return 0; // success
}


/*-----------------------------------------------------------------------------
 * lkm_uninit
 */
void
lkm_uninit(void)
{
    lkm_admin_t * p = &lkm_admin;

    LOG_INFO(LKM_LOG_PREFIX "%s: entered\n", __func__);

    if (!p->f_initialized)
    {
        LOG_CRIT(LKM_LOG_PREFIX "%s: failed, not initialized yet\n", __func__);
        return;
    }

    LOG_INFO(LKM_LOG_PREFIX
             "%s: calling platform_driver_unregister\n",
             __func__);

    platform_driver_unregister(&p->platform_driver);

    zeroinit(lkm_admin); //p->f_initialized = false;

    LOG_INFO(LKM_LOG_PREFIX "%s: left\n", __func__);
}


/*-----------------------------------------------------------------------------
 * lkm_device_generic_get
 */
void *
lkm_device_generic_get(void)
{
    lkm_admin_t * p = &lkm_admin;

    return &p->platform_device_p->dev;
}


/*-----------------------------------------------------------------------------
 * lkm_device_specific_get
 */
void *
lkm_device_specific_get(void)
{
    lkm_admin_t * p = &lkm_admin;

    return p->platform_device_p;
}


/*-----------------------------------------------------------------------------
 * lkm_phys_base_addr_get
 */
void *
lkm_phys_base_addr_get(void)
{
    lkm_admin_t * p = &lkm_admin;

    return (void*)p->phys_base_addr;
}


/*-----------------------------------------------------------------------------
 * lkm_mapped_base_addr_get
 */
void __iomem *
lkm_mapped_base_addr_get(void)
{
    lkm_admin_t * p = &lkm_admin;

    return p->mapped_base_addr_p;
}


/* end of file lkm.c */
