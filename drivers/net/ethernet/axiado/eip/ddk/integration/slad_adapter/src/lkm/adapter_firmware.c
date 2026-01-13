/* adapter_firmware.c
 *
 * Kernel-space implementation of Interface for obtaining the firmware image.
 * Read from file system using Linux firmware API.
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

#include "adapter_firmware.h"

/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */
#include "c_adapter_firmware.h"

// Driver Framework Basic Defs API
#include "basic_defs.h"

#include <linux/firmware.h>

#include "adapter_alloc.h"

// Logging API
#include "log.h"

#include "device_mgmt.h"  // eip_device_get_reference()

/*----------------------------------------------------------------------------
 * adapter_firmware_null
 *
 */
const adapter_firmware_t adapter_firmware_null = NULL;


/*----------------------------------------------------------------------------
 * adapter_firmware_acquire
 */
adapter_firmware_t
adapter_firmware_acquire(
    const char * firmware_name_p,
    const u32 ** firmware_p,
    unsigned int  * firmware_word32_count)
{
    u32 * firmware_data_p;
    const struct firmware  *firmware;
    int rc;
    unsigned int i;

    LOG_CRIT("adapter_firmware_acquire for %s\n",firmware_name_p);

    // Initialize output parameters.
    *firmware_p = NULL;
    *firmware_word32_count = 0;

    // Request firmware via kernel API.
    rc = request_firmware(&firmware,
                          firmware_name_p,
                          eip_device_get_reference(NULL, NULL));
    if (rc < 0)
    {
        LOG_CRIT("adapter_firmware_acquire request of %s failed, rc=%d\n",
                 firmware_name_p, rc);
        return NULL;
    }

    if (firmware->data == NULL ||
        firmware->size == 0 ||
        firmware->size >= 256*1024 ||
        firmware->size % sizeof(u32) != 0)
    {
        LOG_CRIT("adapter_firmware_acquire: Invalid image size %d\n",
                 (int)firmware->size);
        release_firmware(firmware);
        return NULL;
    }

    // Allocate buffer for data
    firmware_data_p = adapter_alloc(firmware->size);
    if (firmware_data_p == NULL)
    {
        LOG_CRIT("adapter_firmware_acquire: Failed to allocate\n");
        release_firmware(firmware);
        return NULL;
    }

    // Convert bytes from file to array of 32-bit words.
    {
        const u8 *p = firmware->data;
        for (i=0; i<firmware->size / sizeof(u32); i++)
        {
            firmware_data_p[i] = p[0] | (p[1]<<8) | (p[2]<<16) | (p[3]<<24);
            p += sizeof(u32);
        }
    }

    // pass results to caller
    *firmware_p = firmware_data_p;
    *firmware_word32_count = firmware->size / sizeof(u32);

    // Release firmware data structure.
    release_firmware(firmware);

    return (adapter_firmware_t)firmware_data_p;
}

/*----------------------------------------------------------------------------
 * adapter_firmware_release
 */
void
adapter_firmware_release(
    adapter_firmware_t firmware_handle)
{
    LOG_CRIT("adapter_firmware_release\n");
    adapter_free((void*)firmware_handle);
}



/* end of file adapter_firmware.c */
