/* adapter_memxs.c
 *
 * Low-level Memory Access API implementation.
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

// MemXS API
#include "api_memxs.h"


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Default configuration
#include "c_adapter_memxs.h"

// Driver Framework Basic Definitions API
#include "basic_defs.h"

// Driver Framework device API
#include "device_types.h"       // device_handle_t
#include "device_rw.h"         // Device_Read/Write
#include "device_mgmt.h"

// Driver Framework C Run-Time Library API
#include "clib.h"               // memcmp


/*----------------------------------------------------------------------------
 * Definitions and macros
 */

/*----------------------------------------------------------------------------
 * mem_xs_device_admin_t
 *
 * MemXS device record
 */
typedef struct
{
    const char *        dev_name;

    unsigned int        first_ofs;

    unsigned int        last_ofs;

    device_handle_t     mem_xs_device;

} mem_xs_device_admin_t;


/*----------------------------------------------------------------------------
 * Local variables
 */

// Here is the dependency on the Driver Framework configuration
// via the MemXS configuration
#ifndef HWPAL_DEVICES
#error "Expected HWPAL_DEVICES defined by cs_memxs.h"
#endif

// Here is the dependency on the Driver Framework configuration
// via the MemXS configuration
#undef HWPAL_DEVICE_ADD
#define HWPAL_DEVICE_ADD(_name, _devnr, _firstofs, _lastofs, _flags) \
           { _name, _firstofs, _lastofs, NULL }

static mem_xs_device_admin_t mem_xs_devices[] =
{
    HWPAL_DEVICES
};

static const unsigned int mem_xs_device_count =
        (sizeof(mem_xs_devices) / sizeof(mem_xs_device_admin_t));


/*----------------------------------------------------------------------------
 * mem_xs_null_handle
 *
 */
const mem_xs_handle_t mem_xs_null_handle = { 0 };


/*----------------------------------------------------------------------------
 * mem_xs_init
 */
bool
mem_xs_init (void)
{
    unsigned int i;

    for(i = 0; i < mem_xs_device_count; i++)
    {
        mem_xs_devices[i].mem_xs_device = NULL;

        mem_xs_devices[i].mem_xs_device =
                eip_device_find (mem_xs_devices[i].dev_name);

        if (mem_xs_devices[i].mem_xs_device == NULL)
        {
            return false;
        }
    } // for

    return true;
}


/*-----------------------------------------------------------------------------
 * mem_xs_handle_is_same
 */
bool
mem_xs_handle_is_same(
        const mem_xs_handle_t * const handle1_p,
        const mem_xs_handle_t * const handle2_p)
{
    return handle1_p->p == handle2_p->p;
}


/*-----------------------------------------------------------------------------
 * mem_xs_device_count_get
 */
mem_xs_status_t
mem_xs_device_count_get(
        unsigned int * const device_count_p)
{
    if (device_count_p == NULL)
        return MEMXS_INVALID_PARAMETER;

    *device_count_p = mem_xs_device_count;

    return MEMXS_STATUS_OK;
}


/*-----------------------------------------------------------------------------
 * mem_xs_device_info_get
 */
mem_xs_status_t
mem_xs_device_info_get(
        const unsigned int device_index,
        mem_xs_dev_info_t * const device_info_p)
{
    if (device_info_p == NULL)
        return MEMXS_INVALID_PARAMETER;

    if (device_index >= mem_xs_device_count)
        return MEMXS_INVALID_PARAMETER;

    device_info_p->index    = device_index;
    device_info_p->handle.p = mem_xs_devices[device_index].mem_xs_device;
    device_info_p->name_p   = mem_xs_devices[device_index].dev_name;
    device_info_p->first_ofs = mem_xs_devices[device_index].first_ofs;
    device_info_p->last_ofs  = mem_xs_devices[device_index].last_ofs;

    return MEMXS_STATUS_OK;
}


/*----------------------------------------------------------------------------
 * mem_xs_read32
 */
u32
mem_xs_read32(
        const mem_xs_handle_t handle,
        const unsigned int byte_offset)
{
    device_handle_t device = (device_handle_t)handle.p;

    return device_read32(device, byte_offset);
}


/*----------------------------------------------------------------------------
 * mem_xs_write32
 */
void
mem_xs_write32(
        const mem_xs_handle_t handle,
        const unsigned int byte_offset,
        const u32 value)
{
    device_handle_t device = (device_handle_t)handle.p;

    device_write32(device, byte_offset, value);
}


/*----------------------------------------------------------------------------
 * mem_xs_read32_array
 */
void
mem_xs_read32_array(
        const mem_xs_handle_t handle,
        const unsigned int byte_offset,
        u32 * memory_dst_p,
        const int count)
{
    device_handle_t device = (device_handle_t)handle.p;

    device_read32_array(device, byte_offset, memory_dst_p, count);
}


/*----------------------------------------------------------------------------
 * mem_xs_write32_array
 */
void
mem_xs_write32_array(
        const mem_xs_handle_t handle,
        const unsigned int byte_offset,
        const u32 * memory_src_p,
        const int count)
{
    device_handle_t device = (device_handle_t)handle.p;

    device_write32_array(device, byte_offset, memory_src_p, count);
}


/* end of file adapter_memxs.c */
