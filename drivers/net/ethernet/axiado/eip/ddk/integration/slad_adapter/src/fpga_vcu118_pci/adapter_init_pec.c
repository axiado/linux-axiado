/* adapter_init_pec.c
 *
 * Adapter module responsible for Adapter PEC initialization tasks.
 *
 */

/*****************************************************************************
* Copyright (c) 2012-2022 by Rambus, Inc. and/or its subsidiaries.
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

#include "adapter_init.h"


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Top-level Adapter configuration
#include "cs_adapter.h"

#ifdef ADAPTER_PEC_INTERRUPTS_ENABLE
#include "adapter_interrupts.h" // adapter_interrupts_init,
                                // adapter_interrupts_uninit
#endif

// Logging API
#include "log.h"            // LOG_*

// Driver Framework device API
#include "device_mgmt.h"    // eip_device_initialize, eip_device_uninitialize
#include "device_rw.h"      // device_read32, device_write32

// Driver Framework DMAResource API
#include "dmares_mgmt.h"    // dma_resource_init, dma_resource_uninit

// Driver Framework Basic Definitions API
#include "basic_defs.h"     // bool, true, false


/*----------------------------------------------------------------------------
 * Local variables
 */

static bool adapter_is_initialized = false;
static int device_irq;

/*----------------------------------------------------------------------------
 * adapter_init
 *
 * Return value
 *     true   Success
 *     false  Failure (fatal!)
 */
bool
adapter_init(void)
{
    device_irq = -1;

    if (adapter_is_initialized != false)
    {
        LOG_WARN("adapter_init: Already initialized\n");
        return true;
    }

    // trigger first-time initialization of the adapter
    if (eip_device_initialize(&device_irq) < 0)
        return false;

    if (!dma_resource_init())
    {
        eip_device_uninitialize();
        return false;
    }

#ifdef ADAPTER_PEC_INTERRUPTS_ENABLE
    if (adapter_interrupts_init(device_irq) < 0)
    {
        LOG_CRIT("adapter_init: adapter_interrupts_init failed\n");

        dma_resource_uninit();

        eip_device_uninitialize();

        return false;
    }
#endif

    adapter_is_initialized = true;

    return true;    // success
}


/*----------------------------------------------------------------------------
 * adapter_uninit
 */
void
adapter_uninit(void)
{
    if (!adapter_is_initialized)
    {
        LOG_WARN("adapter_uninit: Adapter is not initialized\n");
        return;
    }

    adapter_is_initialized = false;

    dma_resource_uninit();

#ifdef ADAPTER_PEC_INTERRUPTS_ENABLE
    adapter_interrupts_uninit(device_irq);
#endif

    eip_device_uninitialize();
}


/*----------------------------------------------------------------------------
 * adapter_report_build_params
 */
void
adapter_report_build_params(void)
{
    // This function is dependent on config file cs_adapter.h.
    // Please update this when config file for Adapter is changed.
    log_formatted_message("Adapter build configuration of %s:\n",
        ADAPTER_VERSION_STRING);

#define REPORT_SET(_X) \
    log_formatted_message("\t" #_X "\n")

#define REPORT_STR(_X) \
    log_formatted_message("\t" #_X ": %s\n", _X)

#define REPORT_INT(_X) \
    log_formatted_message("\t" #_X ": %d\n", _X)

#define REPORT_HEX32(_X) \
    log_formatted_message("\t" #_X ": 0x%08X\n", _X)

#define REPORT_EQ(_X, _Y) \
    log_formatted_message("\t" #_X " == " #_Y "\n")

#define REPORT_EXPL(_X, _Y) \
    log_formatted_message("\t" #_X _Y "\n")

    // Adapter PEC
#ifdef ADAPTER_PEC_DBG
    REPORT_SET(ADAPTER_PEC_DBG);
#endif

#ifdef ADAPTER_PEC_STRICT_ARGS
    REPORT_SET(ADAPTER_PEC_STRICT_ARGS);
#endif

#ifdef ADAPTER_PEC_ENABLE_SCATTERGATHER
    REPORT_SET(ADAPTER_PEC_ENABLE_SCATTERGATHER);
#endif

#ifdef ADAPTER_PEC_SEPARATE_RINGS
    REPORT_SET(ADAPTER_PEC_SEPARATE_RINGS);
#else
    REPORT_EXPL(ADAPTER_PEC_SEPARATE_RINGS, " is NOT set => Overlapping");
#endif

#ifdef ADAPTER_PEC_ARMRING_ENABLE_SWAP
    REPORT_SET(ADAPTER_PEC_ARMRING_ENABLE_SWAP);
#endif

    REPORT_INT(ADAPTER_PEC_DEVICE_COUNT);
    REPORT_INT(ADAPTER_PEC_MAX_PACKETS);
    REPORT_INT(ADAPTER_MAX_PECLOGICDESCR);
    REPORT_INT(ADAPTER_PEC_MAX_SAS);
    REPORT_INT(ADAPTER_DESCRIPTORDONETIMEOUT);
    REPORT_INT(ADAPTER_DESCRIPTORDONECOUNT);

#ifdef ADAPTER_REMOVE_BOUNCEBUFFERS
    REPORT_EXPL(ADAPTER_REMOVE_BOUNCEBUFFERS, " is SET => Bounce DISABLED");
#else
    REPORT_EXPL(ADAPTER_REMOVE_BOUNCEBUFFERS, " is NOT set => Bounce ENABLED");
#endif

#ifdef ADAPTER_EIP202_INTERRUPTS_ENABLE
    REPORT_EXPL(ADAPTER_EIP202_INTERRUPTS_ENABLE,
            " is SET => Interrupts ENABLED");
#else
    REPORT_EXPL(ADAPTER_EIP202_INTERRUPTS_ENABLE,
            " is NOT set => Interrupts DISABLED");
#endif

#ifdef ADAPTER_PCL_ENABLE
    REPORT_SET(ADAPTER_PCL_ENABLE);
    REPORT_INT(ADAPTER_PCL_FLOW_HASH_ENTRIES_COUNT);
#endif

#ifdef ADAPTER_64BIT_HOST
    REPORT_EXPL(ADAPTER_64BIT_HOST,
                " is SET => addresses are 64-bit");
#else
    REPORT_EXPL(ADAPTER_64BIT_HOST,
                " is NOT set => addresses are 32-bit");
#endif

#ifdef ADAPTER_64BIT_DEVICE
    REPORT_EXPL(ADAPTER_64BIT_DEVICE,
                " is SET => full 64-bit DMA addresses usable");
#else
    REPORT_EXPL(ADAPTER_64BIT_DEVICE,
                " is NOT set => DMA addresses must be below 4GB");
#endif

#ifdef ADAPTER_DMARESOURCE_BANKS_ENABLE
    REPORT_SET(ADAPTER_DMARESOURCE_BANKS_ENABLE);
#endif

    // Adapter Global Classification Control
#ifdef ADAPTER_CS_TIMER_PRESCALER
    REPORT_INT(ADAPTER_CS_TIMER_PRESCALER);
#endif

    // Log
    log_formatted_message("Logging:\n");

#if (LOG_SEVERITY_MAX == LOG_SEVERITY_INFO)
    REPORT_EQ(LOG_SEVERITY_MAX, LOG_SEVERITY_INFO);
#elif (LOG_SEVERITY_MAX == LOG_SEVERITY_WARNING)
    REPORT_EQ(LOG_SEVERITY_MAX, LOG_SEVERITY_W_A_R_N_I_N_G);
#elif (LOG_SEVERITY_MAX == LOG_SEVERITY_CRITICAL)
    REPORT_EQ(LOG_SEVERITY_MAX, LOG_SEVERITY_CRITICAL);
#else
    REPORT_EXPL(LOG_SEVERITY_MAX, " - Unknown (not info/warn/crit)");
#endif

    // Adapter other
    log_formatted_message("Other:\n");
    REPORT_STR(ADAPTER_DRIVER_NAME);
    REPORT_STR(ADAPTER_LICENSE);
    REPORT_HEX32(ADAPTER_INTERRUPTS_TRACEFILTER);
}


/* end of file adapter_init_pec.c */
