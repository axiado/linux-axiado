/* adapter_global drbg_init.c
 *
 * Initialize Global DRBG Control functionality.
 */

/*****************************************************************************
* Copyright (c) 2017-2020 by Rambus, Inc. and/or its subsidiaries.
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

#include "adapter_global_drbg_init.h"


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Default configuration
#include "c_adapter_global.h"

// Global Control DRBG API
#include "api_global_eip74.h"

// Driver Framework Basic Definitions API
#include "basic_defs.h"         // u8, u32, bool

// Driver Framework C Library API
#include "clib.h"               // memcpy, zeroinit

// Log API
#include "log.h"

/*----------------------------------------------------------------------------
 * Definitions and macros
 */


/*----------------------------------------------------------------------------
 * Local variables
 */

static bool f_drbg_present;

#ifdef __KERNEL__
#include <linux/random.h>
#define global_drbg_entropy_get(p) get_random_bytes(p, 48);
#else
#include <stdio.h>
/*----------------------------------------------------------------------------
 * global_drbg_entropy_get
 *
 * Get 48 bytes of entropy to initialize/reseed DRBG.
 */
static void
global_drbg_entropy_get(
    u8 * key_p)
{
    FILE *rng = fopen("/dev/urandom","rb");
    if (rng==NULL)
    {
        LOG_CRIT("/dev/urandom not available\n");
        return;
    }
    if (fread(key_p, 1, 48, rng) < 48)
    {
        LOG_CRIT("random data not read\n");
        fclose(rng);
        return;
    }
    log_hex_dump("entropy",0,key_p,48);

    fclose(rng);
}

#endif

/*----------------------------------------------------------------------------
 * bool_to_string()
 *
 * Convert boolean value to string.
 */
static const char *
bool_to_string(
        const bool b)
{
    if (b)
        return "true";
    else
        return "false";
}


/*----------------------------------------------------------------------------
 * adapter_global_drbg_status_report()
 *
 * Obtain all available global status information from the Global DRBG
 * hardware and report it.
 */
void
adapter_global_drbg_status_report(void)
{
    global_control74_error_t rc;
    global_control74_status_t status;

    LOG_INFO("DA_GC: Global_DRBG_StatusReport \n");

    LOG_CRIT("DA_GC: Global DRBG status\n");
    rc = global_control74_status_get(&status);
    if (rc != GLOBAL_CONTROL_EIP74_NO_ERROR)
    {
        LOG_CRIT("EIP74 status get error\n");
        return;
    }
    log_formatted_message(
        "EIP 74 status: gen_block_count=%u StuckOut=%s\n"
        "\t\tNotInitialized=%s ReseedErr=%s ReseedWarn=%s\n"
        "\t\tInstantiated=%s available_count=%u\n",
        status.generate_block_count,
        bool_to_string(status.f_stuck_out),
        bool_to_string(status.f_not_initialized),
        bool_to_string(status.f_reseed_error),
        bool_to_string(status.f_reseed_warning),
        bool_to_string(status.f_instantiated),
        status.available_count);
}


/*----------------------------------------------------------------------------
 * adapter_global_drbg_init()
 *
 */
bool
adapter_global_drbg_init(void)
{
    global_control74_error_t rc;
    global_control74_capabilities_t capabilities;
    global_control74_configuration_t configuration;
    u8 entropy[48];

    LOG_INFO("DA_GC: Global_DRBG_Init \n");

    zeroinit(configuration);
    configuration.f_stuck_out = true;

    global_drbg_entropy_get(entropy);

    rc = global_control74_init(&configuration, entropy);
    if (rc == GLOBAL_CONTROL_EIP74_ERROR_NOT_IMPLEMENTED)
    {
        LOG_CRIT("EIP74 not present\n");
        return true;
    }
    if (rc == GLOBAL_CONTROL_EIP74_NO_ERROR)
    {
        f_drbg_present = true;
        log_formatted_message("EIP74 initialized OK\n");
    }
    else
    {
        LOG_CRIT("EIP74 initialization error\n");
    }

    capabilities.sz_text_description[0] = 0;

    global_control74_capabilities_get(&capabilities);

    LOG_CRIT("DA_GC: Global DRBG capabilities: %s\n",
             capabilities.sz_text_description);

    adapter_global_drbg_status_report();

    return true;
}


/*----------------------------------------------------------------------------
 * adapter_global_drbg_uninit()
 *
 */
void
adapter_global_drbg_uninit(void)
{
    LOG_INFO("\n\t\t adapter_global_drbg_uninit \n");

    if (f_drbg_present)
    {
        adapter_global_drbg_status_report();

        global_control74_uninit();
    }
}


/* end of file adapter_global_drbg_init.c */
