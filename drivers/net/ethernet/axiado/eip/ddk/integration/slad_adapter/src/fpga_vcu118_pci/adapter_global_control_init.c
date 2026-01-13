/* adapter_global_control_init.c
 *
 * Adapter module responsible for adapter global control initialization tasks.
 *
 */

/*****************************************************************************
* Copyright (c) 2012-2020 by Rambus, Inc. and/or its subsidiaries.
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

#include "adapter_global_control_init.h"


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Top-level Adapter configuration
#include "c_adapter_global.h"
#include "c_adapter_cs.h"

// Logging API
#include "log.h"            // LOG_*

// Driver Framework device API
#include "device_mgmt.h"    // eip_device_initialize, eip_device_uninitialize
#include "device_rw.h"      // device_read32, device_write32

// Driver Framework Basic Definitions API
#include "basic_defs.h"     // bool, true, false

#ifdef GLOBALCONTROL_BUILD
#include "shdevxs_init.h"  // SHDevXS_Global_init()
#endif


/*----------------------------------------------------------------------------
 * Local variables
 */

static bool adapter_is_initialized = false;

#ifdef ADAPTER_GLOBAL_BOARDCTRL_SUPPORT_ENABLE
static device_handle_t adapter_device_boardctrl;
#endif


/*----------------------------------------------------------------------------
 * adapter_global_control_init
 *
 * Return value
 *     true   Success
 *     false  Failure (fatal!)
 */
bool
adapter_global_control_init(void)
{
    int n_irq = -1;

    if (adapter_is_initialized != false)
    {
        LOG_WARN("adapter_global_control_init: Already initialized\n");
        return true;
    }

    // trigger first-time initialization of the adapter
    if (eip_device_initialize(&n_irq) < 0)
        return false;

#ifdef GLOBALCONTROL_BUILD
    LOG_INFO("\n SHDevXS_Global_SetPrivileged \n");
    if (SHDevXS_Global_SetPrivileged() != 0)
    {
        LOG_CRIT("%s: SHDevXS_Global_SetPrivileged() failed\n", __func__);
        return false;
    }
#endif

#ifdef ADAPTER_GLOBAL_BOARDCTRL_SUPPORT_ENABLE
    adapter_device_boardctrl = eip_device_find("BOARD_CTRL");
    if (adapter_device_boardctrl == NULL)
    {
        LOG_CRIT("adapter_global_control_init: "
                 "Failed to locate BOARD_CTRL\n");
        return false;
    }
#endif // ADAPTER_GLOBAL_BOARDCTRL_SUPPORT_ENABLE

    // FPGA board specific functionality
#ifdef ADAPTER_GLOBAL_BOARDCTRL_SUPPORT_ENABLE
    {
#ifdef ADAPTER_GLOBAL_FPGA_HW_RESET_ENABLE
        // Perform HW Reset for the EIP-197 FPGA board
        device_write32(adapter_device_boardctrl, 0, 0);
        device_write32(adapter_device_boardctrl, 0, 1);
        device_write32(adapter_device_boardctrl, 0, 0);
#endif // ADAPTER_GLOBAL_FPGA_HW_RESET_ENABLE
    }
#endif // ADAPTER_GLOBAL_BOARDCTRL_SUPPORT_ENABLE

    adapter_is_initialized = true;

    return true;    // success
}


/*----------------------------------------------------------------------------
 * adapter_global_control_uninit
 */
void
adapter_global_control_uninit(void)
{
    if (!adapter_is_initialized)
    {
        LOG_WARN("adapter_global_control_uninit: Adapter is uninitialized\n");
        return;
    }

    adapter_is_initialized = false;

    eip_device_uninitialize();
}


/*----------------------------------------------------------------------------
 * adapter_global_control_report_build_params
 */
void
adapter_global_control_report_build_params(void)
{
#ifdef LOG_INFO_ENABLED
    int dummy = 0;

    // This function is dependent on config file cs_adapter.h.
    // Please update this when config file for Adapter is changed.
    log_formatted_message("Adapter Global Control build configuration:\n");

#define REPORT_SET(_X) \
    log_formatted_message("\t" #_X "\n")

#define REPORT_STR(_X) \
    log_formatted_message("\t" #_X ": %s\n", _X)

#define REPORT_INT(_X) \
    dummy = _X; log_formatted_message("\t" #_X ": %d\n", _X)

#define REPORT_HEX32(_X) \
    dummy = _X; log_formatted_message("\t" #_X ": 0x%08X\n", _X)

#define REPORT_EQ(_X, _Y) \
    dummy = (_X + _Y); log_formatted_message("\t" #_X " == " #_Y "\n")

#define REPORT_EXPL(_X, _Y) \
    log_formatted_message("\t" #_X _Y "\n")

#ifdef ADAPTER_64BIT_HOST
    REPORT_EXPL(ADAPTER_64BIT_HOST,
                " is SET => addresses are 64-bit");
#else
    REPORT_EXPL(ADAPTER_64BIT_HOST,
                " is NOT set => addresses are 32-bit");
#endif

    // Global interrupts
#ifdef ADAPTER_GLOBAL_INTERRUPTS_TRACEFILTER
    REPORT_INT(ADAPTER_GLOBAL_INTERRUPTS_TRACEFILTER);
#endif

    // Adapter Global Classification Control
#ifdef ADAPTER_CS_TIMER_PRESCALER
    REPORT_INT(ADAPTER_CS_TIMER_PRESCALER);
#endif

#ifdef ADAPTER_CS_MAX_NOF_FLOW_HASH_TABLES_TO_USE
    REPORT_INT(ADAPTER_CS_MAX_NOF_FLOW_HASH_TABLES_TO_USE);
#endif

#ifdef ADAPTER_CS_GLOBAL_DEVICE_NAME
    REPORT_STR(ADAPTER_CS_GLOBAL_DEVICE_NAME);
#endif

#ifdef ADAPTER_GLOBAL_BOARDCTRL_SUPPORT_ENABLE
    REPORT_SET(ADAPTER_GLOBAL_BOARDCTRL_SUPPORT_ENABLE);
#endif

    // Log
    log_formatted_message("Logging:\n");

#if (LOG_SEVERITY_MAX == LOG_SEVERITY_INFO)
    REPORT_EQ(LOG_SEVERITY_MAX, LOG_SEVERITY_INFO);
#elif (LOG_SEVERITY_MAX == LOG_SEVERITY_WARNING)
    REPORT_EQ(LOG_SEVERITY_MAX, LOG_SEVERITY_WARNING);
#elif (LOG_SEVERITY_MAX == LOG_SEVERITY_CRITICAL)
    REPORT_EQ(LOG_SEVERITY_MAX, LOG_SEVERITY_CRITICAL);
#else
    REPORT_EXPL(LOG_SEVERITY_MAX, " - Unknown (not info/warn/crit)");
#endif


    IDENTIFIER_NOT_USED(dummy);

    // Adapter other
    log_formatted_message("Other:\n");
    REPORT_STR(ADAPTER_GLOBAL_DRIVER_NAME);
    REPORT_STR(ADAPTER_GLOBAL_LICENSE);

#endif //LOG_INFO_ENABLED
}


/* end of file adapter_global_control_init.c */
