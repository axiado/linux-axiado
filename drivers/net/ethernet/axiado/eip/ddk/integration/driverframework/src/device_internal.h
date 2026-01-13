/* device_internal.h
 *
 * Driver Framework device internal interface.
 *
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

#ifndef DEVICE_INTERNAL_H_
#define DEVICE_INTERNAL_H_

/*----------------------------------------------------------------------------
 * This module implements (provides) the following interface(s):
 */


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Default configuration
#include "c_device_generic.h"

// Driver Framework device API
#include "device_types.h"           // device_handle_t

// Driver Framework device platform interface
#include "device_platform.h"        // Device_Platform_*


/*----------------------------------------------------------------------------
 * Definitions and macros
 */

// Global administration data
typedef struct
{
    // Initialization flag
    bool f_initialized;

    device_platform_global_t platform; // platform-specific global data
} device_global_admin_t;

// internal statically configured device administration data
typedef struct
{
    const char * dev_name;
    unsigned int device_nr;
    unsigned int first_ofs;
    unsigned int last_ofs;
    unsigned int flags; // swapping, tracing
} device_admin_static_t;

// internal device administration data
typedef struct
{
    char * dev_name;
    unsigned int device_nr;
    unsigned int first_ofs;
    unsigned int last_ofs;
    unsigned int flags; // swapping, tracing

#ifdef HWPAL_DEVICE_MAGIC
    unsigned int magic;
#endif

    device_platform_t platform; // platform-specific device data

    unsigned int device_id; // index in the device list

} device_admin_t;

#define HWPAL_DEVICE_ADD(_name, _devnr, _firstofs, _lastofs, _flags) \
                        {_name, _devnr, _firstofs, _lastofs, _flags}


/*----------------------------------------------------------------------------
 * Local variables
 */


/*----------------------------------------------------------------------------
 * device_internal_static_count_get
 *
 * Returns number of statically configured devices in the HWPAL_DEVICES device
 * list.
 *
 */
unsigned int
device_internal_static_count_get(void);


/*----------------------------------------------------------------------------
 * device_internal_count_get
 *
 * Returns number of maximum supported devices in the device list.
 *
 */
unsigned int
device_internal_count_get(void);


/*----------------------------------------------------------------------------
 * device_internal_admin_static_get
 *
 * Returns pointer to the memory location where the statically configured
 * device list is stored.
 *
 */
const device_admin_static_t *
device_internal_admin_static_get(void);


/*----------------------------------------------------------------------------
 * device_internal_admin_get
 *
 * Returns pointer to the memory location where the device list is stored.
 *
 */
device_admin_t **
device_internal_admin_get(void);


/*----------------------------------------------------------------------------
 * device_internal_admin_global_get
 *
 * Returns pointer to the memory location where the global device
 * administration data is stored.
 *
 */
device_global_admin_t *
device_internal_admin_global_get(void);


/*----------------------------------------------------------------------------
 * device_internal_alloc
 *
 * Returns a pointer to a newly allocated block byte_count bytes long, or a null
 * pointer if the block could not be allocated.
 *
 */
void *
device_internal_alloc(
        unsigned int byte_count);


/*----------------------------------------------------------------------------
 * device_internal_free
 *
 * Deallocates the block of memory pointed at by ptr.
 *
 */
void
device_internal_free(
        void * ptr);


/*----------------------------------------------------------------------------
 * device_internal_initialize
 *
 * See eip_device_initialize() description.
 *
 */
int
device_internal_initialize(
        void * custom_init_data_p);


/*----------------------------------------------------------------------------
 * device_internal_uninitialize
 *
 * See eip_device_uninitialize() description.
 *
 */
void
device_internal_uninitialize(void);


/*-----------------------------------------------------------------------------
 * device_internal_find
 *
 * See eip_device_find() description.
 *
 * index (input)
 *      device  index in the device list.
 *
 */
device_handle_t
device_internal_find(
        const char * device_name_p,
        const unsigned int index);


/*-----------------------------------------------------------------------------
 * device_internal_get_index
 *
 * See eip_device_get_index() description.
 *
 */
int
device_internal_get_index(
        const device_handle_t device);


#endif // DEVICE_INTERNAL_H_


/* end of file device_internal.h */
