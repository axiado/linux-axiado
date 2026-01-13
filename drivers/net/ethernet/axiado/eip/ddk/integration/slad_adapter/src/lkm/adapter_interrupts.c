/* adapter_interrupts.c
 *
 * Adapter EIP-202 module responsible for interrupts.
 *
 */

/*****************************************************************************
* Copyright (c) 2008-2022 by Rambus, Inc. and/or its subsidiaries.
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

#include "adapter_interrupts.h"


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Default Adapter configuration
#include "c_adapter_eip202.h"      // ADAPTER_*INTERRUPT*

// Driver Framework Basic Definitions API
#include "basic_defs.h"            // bool, IDENTIFIER_NOT_USED

// Driver Framework C-RunTime Library API
#include "clib.h"                  // zeroinit

// EIP-201 Advanced interrupt Controller (aic)
#include "eip201.h"

// Logging API
#include "log.h"

// Driver Framework device API
#include "device_types.h"          // device_handle_t
#include "device_mgmt.h"           // eip_device_find, eip_device_get_reference

// Linux Kernel API
#include <linux/interrupt.h>       // request_irq, free_irq,
                                   // DECLARE_TASKLET, tasklet_schedule,
                                   // IRQ_DISABLED
#include <linux/irq.h>             // IRQ_TYPE_LEVEL_HIGH
#include <linux/irqreturn.h>       // irqreturn_t

#ifdef ADAPTER_EIP202_USE_UMDEVXS_IRQ
#include "umdevxs_interrupt.h"
#endif

/*----------------------------------------------------------------------------
 * Definitions and macros
 */

#define MAX_OS_IRQS                     256

#define ARRAY_ELEMCOUNT(_a)             (sizeof(_a) / sizeof(_a[0]))

#define ADAPTERINT_REQUEST_IRQ_FLAGS    (IRQF_SHARED)

// data structure for each Advanced interrupt controller
typedef struct
{
    device_handle_t device;
    char *name;    // device name (can be found with eip_device_find().
    int irq_idx;   // if is_irq, then index of device interrupt.
        // If !is_irq. then driver IRQ number to which aic is connected.
    int irq;       // System IRQ number
    int bit_ir_qs[32]; // Mapping from source bits to internal driver IRQ numbers.
    bool is_irq; // true if aic has dedicated system IRQ line, false is it
                // is cascaded from another aic.
} adapter_aic_t;

// data structure per interrupt
typedef struct
{
    u32 source_bit_mask;
    char * name;
    adapter_aic_t *aic;
    char *aic_name;
    struct tasklet_struct tasklet;
    adapter_interrupt_handler_t handler;
    u32 counter;
    void *extra;
    bool f_have_tasklet;
    eip201_config_t config;
} adapter_interrupt_t;


/*----------------------------------------------------------------------------
 * Local variables
 */
static bool adapter_irq_initialized = false;

#define ADAPTER_EIP202_ADD_AIC(_name,_idx, isirq)  \
    {NULL, _name, _idx, -1, {0, }, isirq}


static adapter_aic_t adapter_aic_table[] = { ADAPTER_EIP202_AICS };

#define ADAPTER_EIP202_ADD_IRQ(_name,_phy,_aicname,_tasklet,_pol)    \
    {(1<<(_phy)),#_name,NULL,_aicname,{},NULL,0,NULL,_tasklet, \
                                              EIP201_CONFIG_##_pol}

static adapter_interrupt_t adapter_irq_table[] = { ADAPTER_EIP202_IRQS };

// Define maximum number of supported interrupts
#define ADAPTER_MAX_INTERRUPTS  ARRAY_ELEMCOUNT(adapter_irq_table)

static adapter_aic_t * irq_aic_mapping[MAX_OS_IRQS];


/*----------------------------------------------------------------------------
 * adapter_int_get_active_int_nr
 *
 * Returns 0..31 depending on the lowest '1' bit.
 * Returns 32 when all bits are zero
 *
 * Using binary break-down algorithm.
 */
static inline int
adapter_int_get_active_int_nr(
        u32 sources)
{
    unsigned int int_nr = 0;
    unsigned int r16, r8, r4, r2;

    if (sources == 0)
        return 32;

    // if the lower 16 bits are empty, look at the upper 16 bits
    r16 = sources & 0xFFFF;
    if (r16 == 0)
    {
        int_nr += 16;
        r16 = sources >> 16;
    }

    // if the lower 8 bits are empty, look at the high 8 bits
    r8 = r16 & 0xFF;
    if (r8 == 0)
    {
        int_nr += 8;
        r8 = r16 >> 8;
    }
    r4 = r8 & 0xF;
    if (r4 == 0)
    {
        int_nr += 4;
        r4 = r8 >> 4;
    }

    r2 = r4 & 3;
    if (r2 == 0)
    {
        int_nr += 2;
        r2 = r4 >> 2;
    }

    // last two bits are trivial
    // 00 => cannot happen
    // 01 => +0
    // 10 => +1
    // 11 => +0
    if (r2 == 2)
        int_nr++;

    return int_nr;
}


/*----------------------------------------------------------------------------
 * adapter_int_report_interrupt_counters
 */
static void
adapter_int_report_interrupt_counters(void)
{
#if ADAPTER_EIP202_INTERRUPTS_TRACEFILTER
    int i;
    for (i=0; i<ARRAY_ELEMCOUNT(adapter_irq_table); i++)
    {
        if ( (1ULL<<i) & ADAPTER_EIP202_INTERRUPTS_TRACEFILTER)
        {
            LOG_CRIT("aic %s interrupt source %s mask %08x counter %d\n",
                     adapter_irq_table[i].aic_name,
                     adapter_irq_table[i].name,
                     adapter_irq_table[i].source_bit_mask,
                     adapter_irq_table[i].counter);
        }
    }
#endif
}


/*----------------------------------------------------------------------------
 * adapter_interrupt_enable
 */
int
adapter_interrupt_enable(
        const int n_irq,
        const unsigned int flags)
{
    int rc = -1;
    IDENTIFIER_NOT_USED(flags);

    LOG_INFO("\n\t\t %s \n", __func__);

    if (n_irq < 0 || n_irq >= ADAPTER_MAX_INTERRUPTS)
    {
            LOG_CRIT(
                    "adapter_interrupt_enable: "
                    "Failed, IRQ %d not supported\n",
                    n_irq);
    }
    else
    {
        rc=eip201_source_mask_enable_source(adapter_irq_table[n_irq].aic->device,
                                       adapter_irq_table[n_irq].source_bit_mask);
        LOG_INFO("\n\t\t\tAdapter_Interrupt_Enable "
                 "IRQ %d %s %s mask=%08x\n",
                 n_irq,
                 adapter_irq_table[n_irq].aic_name,
                 adapter_irq_table[n_irq].name,
                 adapter_irq_table[n_irq].source_bit_mask);
   }

    LOG_INFO("\n\t\t %s done \n", __func__);
    return rc;
}


/*----------------------------------------------------------------------------
 * adapter_interrupt_disable
 */
int
adapter_interrupt_disable(
        const int n_irq,
        const unsigned int flags)
{
    int rc = -1;
    IDENTIFIER_NOT_USED(flags);

    LOG_INFO("\n\t\t %s \n", __func__);

    if (n_irq < 0 || n_irq >= ADAPTER_MAX_INTERRUPTS)
    {
            LOG_CRIT(
                    "adapter_interrupt_disable: "
                    "Failed, IRQ %d not supported\n",
                    n_irq);
    }
    else
    {
        rc = eip201_source_mask_disable_source(adapter_irq_table[n_irq].aic->device,
                                       adapter_irq_table[n_irq].source_bit_mask);
        LOG_INFO("\n\t\t\tAdapter_Interrupt_Disable "
                 "IRQ %d %s %s mask=%08x\n",
                 n_irq,
                 adapter_irq_table[n_irq].aic_name,
                 adapter_irq_table[n_irq].name,
                 adapter_irq_table[n_irq].source_bit_mask);
    }

    LOG_INFO("\n\t\t %s done \n", __func__);
    return rc;
}


/*----------------------------------------------------------------------------
 * adapter_interrupt_set_handler
 */
int
adapter_interrupt_set_handler(
        const int n_irq,
        adapter_interrupt_handler_t handler_function)
{
    if (n_irq < 0 || n_irq >= ADAPTER_MAX_INTERRUPTS)
        return -1;

    LOG_INFO(
            "adapter_interrupt_set_handler: "
            "HandlerFnc=%p for IRQ %d\n",
            handler_function,
            n_irq);

    adapter_irq_table[n_irq].handler = handler_function;
    return 0;
}


/*----------------------------------------------------------------------------
 * adapter_int_common_tasklet
 *
 * This handler is scheduled in the top-halve interrupt handler when it
 * decodes one of the CDR or RDR interrupt sources.
 * The data parameter is the IRQ value (from adapter_interrupts.h) for that
 * specific interrupt source.
 */
static void
adapter_int_common_tasklet(
        unsigned long data)
{
    const unsigned int int_nr = (unsigned int)data;
    adapter_interrupt_handler_t H;

    LOG_INFO("\n\t\t%s \n", __func__);

    LOG_INFO("Tasklet invoked intnr=%d\n",int_nr);

    // verify we have a handler
    H = adapter_irq_table[int_nr].handler;

    if (H)
    {
        // invoke the handler
        H(int_nr, 0);
    }
    else
    {
        LOG_CRIT(
            "adapter_int_common_tasklet: "
            "error, disabling IRQ %d with missing handler\n",
            int_nr);

        adapter_interrupt_disable(int_nr, 0);
    }

    LOG_INFO("\n\t\t%s done\n", __func__);
}


/*----------------------------------------------------------------------------
 * adapter_int_aic_handler
 *
 * handle all interrupts connected to the specified aic.
 *
 * If this aic is connected directly to a system IRQ line, this is
 * called directly from the Top Half handler.
 *
 * If this aic is connected via an IRQ line of another aic, this is
 * called from the handler function of that interrupt.
 *
 * Return: 0 for success, -1 for failure.
 */
static int
adapter_int_aic_handler(adapter_aic_t *aic)
{
    eip201_source_bitmap_t sources;
    int int_nr, irq, rc = 0;

    LOG_INFO("\n\t\t%s \n", __func__);

    if (aic == NULL)
        return -1;

    if (aic->device == NULL)
    {
        LOG_INFO("%s: skipping spurious interrupt for aic %s, IRQ %d\n",
                 __func__,
                 aic->name,
                 aic->irq);
        goto exit; // no error
    }

    sources = eip201_source_status_read_all_enabled(aic->device);
    if (sources == 0)
    {
        rc = -1;
        goto exit; // error
    }

    eip201_acknowledge(aic->device, sources);

    LOG_INFO("%s: aic %s, IRQ %d, sources=%x\n",
             __func__,
             aic->name,
             aic->irq,
             sources);

    while (sources)
    {
        int_nr = adapter_int_get_active_int_nr(sources);

        /* Get number of first bit set */
        sources &= ~(1<<int_nr);

        /* Clear this in sources */
        irq = aic->bit_ir_qs[int_nr];

        LOG_INFO("%s: handle IRQ %d for aic %s\n", __func__, irq, aic->name);

        if (irq < 0 || irq >= ADAPTER_MAX_INTERRUPTS)
        {
            LOG_CRIT("%s: %s IRQ not defined for bit %d, disabling source\n",
                     __func__,
                     aic->name,
                     int_nr);
            eip201_source_mask_disable_source(aic->device, (1<<int_nr));
            rc = -1;
            goto exit;
        }

        adapter_irq_table[irq].counter++;
#if ADAPTER_EIP202_INTERRUPTS_TRACEFILTER
        if ( (1ULL<<irq) & ADAPTER_EIP202_INTERRUPTS_TRACEFILTER)
            LOG_CRIT("%s: encountered interrupt %d, bit %d for aic %s\n",
                     __func__,
                     irq,
                     int_nr,
                     aic->name);
#endif

        if(adapter_irq_table[irq].f_have_tasklet)
        {
            LOG_INFO("%s: Start tasklet\n", __func__);
            /* IRQ is handled via tasklet */
            tasklet_schedule(&adapter_irq_table[irq].tasklet);
            adapter_interrupt_disable(irq, 0);
        }
        else
        {
            adapter_interrupt_handler_t H = adapter_irq_table[irq].handler;
            LOG_INFO("%s: Run normal handler\n", __func__);
            /* handler is called directly */
            if (H)
            {
                H(irq, 0);
            }
            else
            {
                LOG_CRIT(
                    "%s : error, disabling IRQ %d with missing handler\n",
                    __func__,
                    irq);

                adapter_interrupt_disable(irq, 0);
            }
        }
    } // while

exit:
    LOG_INFO("\n\t\t%s done\n", __func__);
    return rc;
}


/*----------------------------------------------------------------------------
 * adapter_int_top_half_handler
 *
 * This is the interrupt handler function call by the kernel when our hooked
 * interrupt is active.
 *
 * Call the handler for the associated aic.
 */
static irqreturn_t
adapter_int_top_half_handler(
        int irq,
        void * dev_id)
{
    irqreturn_t int_rc = IRQ_NONE;
    LOG_INFO("\n\t\t%s \n", __func__);

    if (irq < 0 || irq >= MAX_OS_IRQS || irq_aic_mapping[irq]==NULL)
    {
        LOG_CRIT("%s: No aic defined for IRQ %d\n",__func__,irq);
        goto error;
    }

    if ( adapter_int_aic_handler(irq_aic_mapping[irq]) < 0)
    {
        goto error;
    }

    int_rc = IRQ_HANDLED;

error:
    LOG_INFO("\n\t\t%s done\n", __func__);

    IDENTIFIER_NOT_USED(dev_id);
    return int_rc;
}


/*----------------------------------------------------------------------------
 * adapter_int_chained_aic
 *
 * handler function for IRQ that services an entire aic.
 */
static void
adapter_int_chained_aic(const int irq, const unsigned int flags)
{
    IDENTIFIER_NOT_USED(flags);
    adapter_int_aic_handler(adapter_irq_table[irq].extra);
}


/*----------------------------------------------------------------------------
 * adapter_int_set_internal_linkage
 *
 * Create aic References in adapter_irq_table.
 * Fill in bit_ir_qs references in adapter_aic_table.
 * Perform some consistency checks.
 *
 * Return 0 on success, -1 on failure.
 */
static int
adapter_int_set_internal_linkage(void)
{
    int i,j;
    int int_nr;

    for (i=0; i<ARRAY_ELEMCOUNT(adapter_irq_table); i++)
    {
        adapter_irq_table[i].aic = NULL;
    }

    for (i=0; i<ARRAY_ELEMCOUNT(adapter_aic_table); i++)
    {
        for (j=0; j<32; j++)
        {
            adapter_aic_table[i].bit_ir_qs[j] = -1;
        }
        for (j=0; j<ARRAY_ELEMCOUNT(adapter_irq_table); j++)
        {
            if (strcmp(adapter_aic_table[i].name, adapter_irq_table[j].aic_name)
                == 0)
            {
                if (adapter_irq_table[j].aic)
                {
                    LOG_CRIT("%s: aic link set more than once\n",__func__);
                }
                adapter_irq_table[j].aic = adapter_aic_table + i;
                int_nr = adapter_int_get_active_int_nr(
                    adapter_irq_table[j].source_bit_mask);
                if (int_nr < 0 || int_nr >= 32)
                {
                    LOG_CRIT("%s: IRQ %d source bit %d out of range\n",
                             __func__,j,int_nr);
                    return -1;
                }
                else if (adapter_aic_table[i].bit_ir_qs[int_nr] >= 0)
                {
                    LOG_CRIT(
                        "%s: aic %s IRQ %d source bit %d already defined\n",
                        __func__,
                        adapter_aic_table[i].name,
                        j,
                        int_nr);
                    return -1;
                }
                else
                {
                    adapter_aic_table[i].bit_ir_qs[int_nr] = j;
                }
            }
        }
    }
    for (i=0; i<ARRAY_ELEMCOUNT(adapter_irq_table); i++)
    {
        if (adapter_irq_table[i].aic == NULL)
        {
            LOG_CRIT("%s: aic pointer of IRQ %d is null\n",__func__,i);
            return -1;
        }
    }

    return 0;
}


/*----------------------------------------------------------------------------
 * adapter_int_aic_init
 *
 */
static bool
adapter_int_aic_init(void)
{
    eip201_status_t res;
    unsigned int i;

    // Initialize all configured EIP-201 aic devices
    for (i = 0; i < ARRAY_ELEMCOUNT(adapter_aic_table); i++)
    {
        LOG_INFO("%s: Initialize aic %s\n",__func__,adapter_aic_table[i].name);

        adapter_aic_table[i].device = eip_device_find(adapter_aic_table[i].name);
        if (adapter_aic_table[i].device == NULL)
        {
            LOG_CRIT("%s: eip_device_find() failed for %s\n",
                     __func__,
                     adapter_aic_table[i].name);
            return false; // error
        }

        res = eip201_initialize(adapter_aic_table[i].device, NULL, 0);
        if (res != EIP201_STATUS_SUCCESS)
        {
            LOG_CRIT("%s: eip201_initialize() failed, error %d\n", __func__, res);
            return false; // error
        }
    }

    return true; // success
}


/*----------------------------------------------------------------------------
 * adapter_int_aic_enable
 *
 */
static void
adapter_int_aic_enable(void)
{
    unsigned int i;

    for (i = 0; i < ARRAY_ELEMCOUNT(adapter_aic_table); i++)
        if (!adapter_aic_table[i].is_irq)
            adapter_interrupt_enable(adapter_aic_table[i].irq_idx, 0);
}


/*----------------------------------------------------------------------------
 * adapter_interrupts_init
 *
 */
int
adapter_interrupts_init(
        const int n_irq)
{
    int i;
    int int_nr = n_irq;

    LOG_INFO("\n\t\t %s \n", __func__);

    if (adapter_int_set_internal_linkage() < 0)
    {
        LOG_CRIT("Interrupt aic and IRQ tables are inconsistent\n");
        return -1;
    }

    // Initialize the aic devices
    if (!adapter_int_aic_init())
        return -1;

    // Initialize the adapter_irq_table and tasklets.
    for (i=0; i<ARRAY_ELEMCOUNT(adapter_irq_table); i++)
    {
        adapter_irq_table[i].handler = NULL;
        adapter_irq_table[i].extra = NULL;
        adapter_irq_table[i].counter = 0;
        if (adapter_irq_table[i].f_have_tasklet)
            tasklet_init(&adapter_irq_table[i].tasklet,
                         adapter_int_common_tasklet,
                         (long)i);
        eip201_config_change(adapter_irq_table[i].aic->device,
                             adapter_irq_table[i].source_bit_mask,
                             adapter_irq_table[i].config);
        // Clear any pending egde-sensitive interrupts.
        eip201_acknowledge(adapter_irq_table[i].aic->device,
                           adapter_irq_table[i].source_bit_mask);
    }

    // Request the IRQs for each aic or register to IRQ of other aic.
    for (i=0; i<ARRAY_ELEMCOUNT(adapter_aic_table); i++)
    {
        if (adapter_aic_table[i].is_irq)
        {
            int res;

            LOG_INFO("\n\t\t %s: Request IRQ for aic %s\n",
                     __func__,adapter_aic_table[i].name);

#ifdef ADAPTER_EIP202_USE_UMDEVXS_IRQ
            res = UMDevXS_Interrupt_Request(adapter_int_top_half_handler,
                                            adapter_aic_table[i].irq_idx);
            int_nr = res;
#else
            {
                struct device * device_p;
                // Get device reference for this resource
                device_p = eip_device_get_reference(NULL, NULL);
                res = request_irq(int_nr,
                                  adapter_int_top_half_handler,
                                  ADAPTERINT_REQUEST_IRQ_FLAGS,
                                  ADAPTER_EIP202_DRIVER_NAME,
                                  device_p);
            }
#endif
            if (res < 0)
            {
                LOG_CRIT("%s: Request IRQ error %d\n", __func__, res);
                return res;
            }
            else
            {
                adapter_aic_table[i].irq = int_nr;
                irq_aic_mapping[int_nr] = adapter_aic_table + i;
                LOG_INFO("%s: Successfully hooked IRQ %d\n", __func__, int_nr);
            }
        }
        else
        {
            int_nr = adapter_aic_table[i].irq_idx;
            LOG_INFO("%s: Hook up aic %s to chained IRQ %d\n",
                     __func__,
                     adapter_aic_table[i].name,
                     int_nr);
            if (int_nr < 0 || int_nr >= ADAPTER_MAX_INTERRUPTS)
            {
                LOG_CRIT("%s: IRQ %d out of range\n", __func__,int_nr);
            }
            adapter_irq_table[int_nr].extra =  adapter_aic_table + i;
            adapter_interrupt_set_handler(int_nr, adapter_int_chained_aic);
        }
    }

    // Enable aic
    adapter_int_aic_enable();

    LOG_INFO("\n\t\t %s done\n", __func__);
    adapter_irq_initialized = true;

    return 0;
}


/*----------------------------------------------------------------------------
 * adapter_interrupts_uninit
 */
int
adapter_interrupts_uninit(const int n_irq)
{
    unsigned int i;
    IDENTIFIER_NOT_USED(n_irq);

    LOG_INFO("\n\t\t %s \n", __func__);
    if (!adapter_irq_initialized)
        return -1;

    // disable all interrupts
    for (i = 0; i < ARRAY_ELEMCOUNT(adapter_aic_table); i++)
    {
        if (adapter_aic_table[i].device)
        {
            eip201_source_mask_disable_source(adapter_aic_table[i].device,
                                            EIP201_SOURCE_ALL);
            adapter_aic_table[i].device = NULL;
        }

        if(adapter_aic_table[i].is_irq && adapter_aic_table[i].irq > 0)
        {
#ifdef ADAPTER_EIP202_USE_UMDEVXS_IRQ
            UMDevXS_Interrupt_Request(NULL,adapter_aic_table[i].irq_idx);
#else
            // Get device reference for this resource
            struct device * device_p = eip_device_get_reference(NULL, NULL);

            LOG_INFO("%s: Free IRQ %d for aic %s\n",
                     __func__,
                     adapter_aic_table[i].irq,
                     adapter_aic_table[i].name);

            // unhook the interrupt
            free_irq(adapter_aic_table[i].irq, device_p);

            LOG_INFO("%s: Successfully freed IRQ %d for aic %s\n",
                     __func__,
                     adapter_aic_table[i].irq,
                     adapter_aic_table[i].name);
#endif
        }
    }

    // Kill all tasklets
    for (i = 0; i < ARRAY_ELEMCOUNT(adapter_irq_table); i++)
        if (adapter_irq_table[i].f_have_tasklet)
            tasklet_kill(&adapter_irq_table[i].tasklet);

    adapter_int_report_interrupt_counters();

    zeroinit(irq_aic_mapping);

    adapter_irq_initialized = false;

    LOG_INFO("\n\t\t %s done\n", __func__);

    return 0;
}


#ifdef ADAPTER_PEC_RPM_EIP202_DEVICE0_ID
/*----------------------------------------------------------------------------
 * adapter_interrupts_resume
 */
int
adapter_interrupts_resume(void)
{
    LOG_INFO("\n\t\t %s \n", __func__);

    if (!adapter_irq_initialized)
    {
        LOG_CRIT("%s: failed, not initialized\n", __func__);
        return -1;
    }

    // Resume aic devices
    if (!adapter_int_aic_init())
        return -2; // error

    // Re-enable aic interrupts
    adapter_int_aic_enable();

    LOG_INFO("\n\t\t %s done\n", __func__);

    return 0; // success
}
#endif


/* end of file adapter_interrupts.c */
