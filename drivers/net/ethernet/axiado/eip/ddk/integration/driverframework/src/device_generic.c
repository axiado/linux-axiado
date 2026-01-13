/* device_generic.c
 *
 * This is the generic Driver Framework v4 device API implementation.
 */

/*****************************************************************************
* Copyright (c) 2017-2023 by Rambus, Inc. and/or its subsidiaries.
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

// Driver Framework device API
#include "device_mgmt.h"            // API to implement


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Driver Framework Basic Defs API
#include "basic_defs.h"             // IDENTIFIER_NOT_USED, NULL

// Driver Framework C Run-time Library API
#include "clib.h"                   // strlen, strcpy

// Driver Framework device internal interface
#include "device_internal.h"        // Device_Internal_*


/*----------------------------------------------------------------------------
 * Definitions and macros
 */

// Default configuration
#include "c_device_generic.h"

// Logging API
#include "log.h"


/*----------------------------------------------------------------------------
 * Local variables
 */


/*-----------------------------------------------------------------------------
 * device_lib_device_exists
 *
 * Checks if a device with device_name_p is already present in the device list
 *
 */
static bool
device_lib_device_exists(
        const char * device_name_p)
{
    unsigned int i = 0;
    device_admin_t ** dev_admin_pp = device_internal_admin_get();

    if (device_name_p == NULL)
        return false;

    while(dev_admin_pp[i])
    {
        if (strcmp(dev_admin_pp[i]->dev_name, device_name_p) == 0)
            return true;
        i++;
    }

    return false;
}


/*-----------------------------------------------------------------------------
 * device_mgmt API
 *
 * These functions support finding a device given its name.
 * A handle is returned that is needed in the device_rw API
 * to read or write the device
 */

/*-----------------------------------------------------------------------------
 * eip_device_initialize
 */
int
eip_device_initialize(
        void * custom_init_data_p)
{
    unsigned int res;
    unsigned int dev_stat_count = device_internal_static_count_get();
    unsigned int dev_count = device_internal_count_get();
    const device_admin_static_t * dev_stat_admin_p =
                            device_internal_admin_static_get();
    device_admin_t ** dev_admin_pp = device_internal_admin_get();
    device_global_admin_t * dev_global_admin_p =
                            device_internal_admin_global_get();

    if (dev_global_admin_p->f_initialized)
        return 0; // already initialized, success

    if (dev_stat_count > dev_count)
    {
        LOG_CRIT("%s: Invalid number of static devices (%d), max %d\n",
                 __func__,
                 (int)dev_stat_count,
                 dev_count);
        return -1; // failed
    }

    // Copy static devices
    for (res = 0; res < dev_stat_count; res++)
    {
        if (device_lib_device_exists(dev_stat_admin_p[res].dev_name))
        {
            LOG_CRIT("%s: failed, device (index %d) with name %s exists\n",
                     __func__,
                     res,
                     dev_stat_admin_p[res].dev_name);
            goto error_exit;
        }

        // Allocate memory for device administration data
        dev_admin_pp[res] = device_internal_alloc(sizeof(device_admin_t));
        if (dev_admin_pp[res] == NULL)
        {
            LOG_CRIT("%s: failed to allocate device (index %d, name %s)\n",
                     __func__,
                     res,
                     dev_stat_admin_p[res].dev_name);
            goto error_exit;
        }

        // Allocate and copy device name
        dev_admin_pp[res]->dev_name =
                device_internal_alloc((unsigned int)strlen(dev_stat_admin_p[res].dev_name)+1);
        if (dev_admin_pp[res]->dev_name == NULL)
        {
            LOG_CRIT("%s: failed to allocate device (index %d) name %s\n",
                     __func__,
                     res,
                     dev_stat_admin_p[res].dev_name);
            goto error_exit;
        }
        strcpy(dev_admin_pp[res]->dev_name, dev_stat_admin_p[res].dev_name);

        // Copy the rest of device data
        dev_admin_pp[res]->device_nr = dev_stat_admin_p[res].device_nr;
        dev_admin_pp[res]->first_ofs = dev_stat_admin_p[res].first_ofs;
        dev_admin_pp[res]->last_ofs  = dev_stat_admin_p[res].last_ofs;
        dev_admin_pp[res]->flags    = dev_stat_admin_p[res].flags;

#ifdef HWPAL_DEVICE_MAGIC
        dev_admin_pp[res]->magic    = HWPAL_DEVICE_MAGIC;
#endif
        dev_admin_pp[res]->device_id = res;
    }

    res = (unsigned int)device_internal_initialize(custom_init_data_p);
    if (res != 0)
    {
        LOG_CRIT("%s: failed, error %d\n", __func__, res);
        goto error_exit;
    }

    dev_global_admin_p->f_initialized = true;
    return 0; // success

error_exit:
    // Free all allocated memory
    for (res = 0; res < dev_stat_count; res++)
        if (dev_admin_pp[res])
        {
            device_internal_free(dev_admin_pp[res]->dev_name);
            device_internal_free(dev_admin_pp[res]);
            dev_admin_pp[res] = NULL;
        }

    return -1; // failed
}


/*-----------------------------------------------------------------------------
 * eip_device_uninitialize
 */
void
eip_device_uninitialize(void)
{
    unsigned int dev_count = device_internal_count_get();
    device_admin_t ** dev_admin_pp = device_internal_admin_get();
    device_global_admin_t * dev_global_admin_p =
                        device_internal_admin_global_get();

    LOG_INFO("%s: unregister driver\n", __func__);

    if (dev_global_admin_p->f_initialized)
    {
        unsigned int i;

        device_internal_uninitialize();

        // Free all allocated memory
        for (i = 0; i < dev_count; i++)
            if (dev_admin_pp[i])
            {
                device_internal_free(dev_admin_pp[i]->dev_name);
                device_internal_free(dev_admin_pp[i]);
                dev_admin_pp[i] = NULL;
            }

        dev_global_admin_p->f_initialized = false;
    }
}


/*-----------------------------------------------------------------------------
 * eip_device_find
 */
device_handle_t
eip_device_find(
        const char * device_name_p)
{
    unsigned int i;

    unsigned int dev_count = device_internal_count_get();
    device_admin_t ** dev_admin_pp = device_internal_admin_get();
    device_global_admin_t * dev_global_admin_p =
                        device_internal_admin_global_get();

    if (!dev_global_admin_p->f_initialized)
    {
        LOG_CRIT("%s: failed, not initialized\n", __func__);
        return NULL;
    }

#ifdef HWPAL_STRICT_ARGS_CHECK
    if (device_name_p == NULL)
    {
        LOG_CRIT("%s: failed, invalid name\n", __func__);
        return NULL; // not supported, thus not found
    }
#endif

    // walk through the device list and compare the device name
    for (i = 0; i < dev_count; i++)
        if (dev_admin_pp[i] &&
            strcmp(device_name_p, dev_admin_pp[i]->dev_name) == 0)
            return device_internal_find(device_name_p, i);

    LOG_CRIT("%s: failed to find device '%s'\n", __func__, device_name_p);

    return NULL;
}


/*----------------------------------------------------------------------------
 * eip_device_get_properties
 */
int
eip_device_get_properties(
        const unsigned int index,
        device_properties_t * const props_p,
        bool * const f_valid_p)
{
    device_admin_t ** dev_admin_pp = device_internal_admin_get();
    device_global_admin_t * dev_global_admin_p =
                        device_internal_admin_global_get();

    if (!dev_global_admin_p->f_initialized)
    {
        LOG_CRIT("%s: failed, not initialized\n", __func__);
        return -1;
    }

#ifdef HWPAL_STRICT_ARGS_CHECK
    if (index >= device_internal_count_get())
    {
        LOG_CRIT("%s: failed, invalid index %d, max device count %d\n",
                 __func__,
                 index,
                 device_internal_count_get());
        return -1;
    }

    if (props_p == NULL || f_valid_p == NULL)
    {
        LOG_CRIT("%s: failed, invalid pointers for device index %d\n",
                 __func__,
                 index);
        return -1;
    }
#endif

    if (!dev_admin_pp[index])
    {
        LOG_INFO("%s: device with index %d not present\n",
                 __func__,
                 index);
        *f_valid_p = false;
    }
    else
    {
        props_p->name_p          = dev_admin_pp[index]->dev_name;
        props_p->device_nr        = dev_admin_pp[index]->device_nr;
        props_p->start_byte_offset = dev_admin_pp[index]->first_ofs;
        props_p->last_byte_offset  = dev_admin_pp[index]->last_ofs;
        props_p->flags           = (char)dev_admin_pp[index]->flags;
        *f_valid_p = true;
    }
    return 0;
}


/*----------------------------------------------------------------------------
 * eip_device_add
 */
int
eip_device_add(
        const unsigned int index,
        const device_properties_t * const props_p)
{
    device_admin_t ** dev_admin_pp = device_internal_admin_get();
    device_global_admin_t * dev_global_admin_p =
                        device_internal_admin_global_get();

    if (!dev_global_admin_p->f_initialized)
    {
        LOG_CRIT("%s: failed, not initialized\n", __func__);
        return -1;
    }

#ifdef HWPAL_STRICT_ARGS_CHECK
    if (index >= device_internal_count_get())
    {
        LOG_CRIT("%s: failed, invalid index %d, max device count %d\n",
                 __func__,
                 index,
                 device_internal_count_get());
        return -1;
    }

    if (props_p == NULL || props_p->name_p == NULL)
    {
        LOG_CRIT("%s: failed, invalid properties for device index %d\n",
                 __func__,
                 index);
        return -1;
    }
#endif

    if (dev_admin_pp[index])
    {
        LOG_CRIT("%s: device with index %d already added\n",
                 __func__,
                 index);
        return -1;
    }

    if (device_lib_device_exists(props_p->name_p))
    {
        LOG_CRIT("%s: device with name %s already added\n",
                 __func__,
                 props_p->name_p);
        return -1;
    }

    // Allocate memory for device administration data
    dev_admin_pp[index] = device_internal_alloc(sizeof(device_admin_t));
    if (dev_admin_pp[index] == NULL)
    {
        LOG_CRIT("%s: failed to allocate device (index %d, name %s)\n",
                 __func__,
                 index,
                 props_p->name_p);
        return -1;
    }

    // Allocate and copy device name
    dev_admin_pp[index]->dev_name =
                    device_internal_alloc((unsigned int)strlen(props_p->name_p)+1);
    if (dev_admin_pp[index]->dev_name == NULL)
    {
        LOG_CRIT("%s: failed to allocate device (index %d) name %s\n",
                 __func__,
                 index,
                 props_p->name_p);
        device_internal_free(dev_admin_pp[index]);
        dev_admin_pp[index] = NULL;
        return -1;
    }
    strcpy(dev_admin_pp[index]->dev_name, props_p->name_p);

    // Copy the rest
    dev_admin_pp[index]->device_nr  = props_p->device_nr;
    dev_admin_pp[index]->first_ofs  = props_p->start_byte_offset;
    dev_admin_pp[index]->last_ofs   = props_p->last_byte_offset;
    dev_admin_pp[index]->flags     = (unsigned int)props_p->flags;

    // Set default values
#ifdef HWPAL_DEVICE_MAGIC
    dev_admin_pp[index]->magic     = HWPAL_DEVICE_MAGIC;
#endif

    dev_admin_pp[index]->device_id  = index;

    return 0; // success
}


/*----------------------------------------------------------------------------
 * eip_device_remove
 */
int
eip_device_remove(
        const unsigned int index)
{
    device_admin_t ** dev_admin_pp = device_internal_admin_get();
    device_global_admin_t * dev_global_admin_p =
                        device_internal_admin_global_get();

    if (!dev_global_admin_p->f_initialized)
    {
        LOG_CRIT("%s: failed, not initialized\n", __func__);
        return -1;
    }

#ifdef HWPAL_STRICT_ARGS_CHECK
    if (index >= device_internal_count_get())
    {
        LOG_CRIT("%s: failed, invalid index %d, max device count %d\n",
                 __func__,
                 index,
                 device_internal_count_get());
        return -1;
    }
#endif

    if (!dev_admin_pp[index])
    {
        LOG_CRIT("%s: device with index %d already removed\n",
                 __func__,
                 index);
        return -1;
    }
    else
    {
        // Free device memory
        device_internal_free(dev_admin_pp[index]->dev_name);
        device_internal_free(dev_admin_pp[index]);
        dev_admin_pp[index] = NULL;
    }

    return 0; // success
}


/*-----------------------------------------------------------------------------
 * eip_device_get_name
 */
char *
eip_device_get_name(
        const unsigned int index)
{
    device_admin_t ** dev_admin_pp = device_internal_admin_get();
    device_global_admin_t * dev_global_admin_p =
                        device_internal_admin_global_get();

    if (!dev_global_admin_p->f_initialized)
    {
        LOG_CRIT("%s: failed, not initialized\n", __func__);
        return NULL;
    }

#ifdef HWPAL_STRICT_ARGS_CHECK
    if (index >= device_internal_count_get())
    {
        LOG_CRIT("%s: failed, invalid index %d, max device count %d\n",
                 __func__,
                 index,
                 device_internal_count_get());
        return NULL;
    }
#endif

    if (!dev_admin_pp[index])
    {
        LOG_CRIT("%s: device with index %d already removed\n",
                 __func__,
                 index);
        return NULL;
    }

    return dev_admin_pp[index]->dev_name; // success
}


/*-----------------------------------------------------------------------------
 * eip_device_get_index
 */
int
eip_device_get_index(
        const device_handle_t device)
{
    device_global_admin_t * dev_global_admin_p =
                        device_internal_admin_global_get();

    if (!dev_global_admin_p->f_initialized)
    {
        LOG_CRIT("%s: failed, not initialized\n", __func__);
        return -1;
    }

    return device_internal_get_index(device);
}


/*-----------------------------------------------------------------------------
 * eip_device_get_count
 */
unsigned int
eip_device_get_count(void)
{
    return device_internal_count_get();
}


/* end of file device_generic.c */
