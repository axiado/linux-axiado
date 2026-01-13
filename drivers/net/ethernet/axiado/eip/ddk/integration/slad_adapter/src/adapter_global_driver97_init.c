/* adapter_global_driver97_init.c
 *
 * Adapter top level module,
 * Security-IP-97 global control driver's entry point.
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

#include "api_global_driver97_init.h"    // Driver Init API


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Top-level Adapter configuration
#include "c_adapter_global.h"             // ADAPTER_GLOBAL_DRIVER_NAME

// Adapter Initialization API - for global only tjc
#include "adapter_global_control_init.h"  // Adapter_* for global only

// Logging API
#include "log.h"                          // LOG_INFO


/*----------------------------------------------------------------------------
 * driver97_global_init
 */
int
driver97_global_init(void)
{
    LOG_INFO("\n\t driver97_global_init \n");

    LOG_INFO("%s driver: initializing\n", ADAPTER_GLOBAL_DRIVER_NAME);

    adapter_global_control_report_build_params();

    if (!adapter_global_control_init())
    {
        return -1;
    }

    LOG_INFO("\n\t driver97_global_init done \n");

    return 0;   // success
}


/*----------------------------------------------------------------------------
 * driver97_global_exit
 */
void
driver97_global_exit(void)
{
    LOG_INFO("\n\t driver97_global_exit \n");

    LOG_INFO("%s driver: exit\n", ADAPTER_GLOBAL_DRIVER_NAME);

    adapter_global_control_uninit();

    LOG_INFO("\n\t driver97_global_exit done \n");
}


#include "adapter_driver97_global_init_ext.h"


/* end of file adapter_global_driver97_init.c */
