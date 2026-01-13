/* device_mgmt.h
 *
 * Driver Framework, device API, Management functions
 *
 * The document "Driver Framework Porting Guide" contains the detailed
 * specification of this API. The information contained in this header file
 * is for reference only.
 */

/*****************************************************************************
* Copyright (c) 2007-2020 by Rambus, Inc. and/or its subsidiaries.
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

#ifndef INCLUDE_GUARD_DEVICE_MGMT_H
#define INCLUDE_GUARD_DEVICE_MGMT_H

/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Driver Framework device API API
#include "device_types.h"   // device_handle_t, device_reference_t

// Driver Framework Basic Definitions API
#include "basic_defs.h"     // bool, u32, inline


/*----------------------------------------------------------------------------
 * Definitions and macros
 */


/*----------------------------------------------------------------------------
 * eip_device_initialize
 *
 * This function must be called exactly once to initialize the device
 * implementation before any other API function may be used.
 *
 * custom_init_data_p
 *     This anonymous parameter can be used to pass information from the caller
 *     to the driver framework implementation.
 *
 * Return value
 *     0    Success
 *     <0   error code (implementation specific)
 */
int
eip_device_initialize(
        void * custom_init_data_p);


/*----------------------------------------------------------------------------
 * eip_device_uninitialize
 *
 * This function can be called to shut down the device implementation. The
 * caller must make sure none of the other API functions are called after or
 * during the invocation of this UnInitialize function. After this call
 * returns the API state is back in "uninitialized" and the eip_device_initialize
 * function may be called anew.
 *
 * Return value
 *     None
 */
void
eip_device_uninitialize(void);


/*----------------------------------------------------------------------------
 * eip_device_find
 *
 * This function must be used to retrieve a handle for a certain device that
 * is identified by a string. The exact strings supported is implementation
 * specific and will differ from product to product.
 * Note that this function may be called more than once to retrieve the same
 * handle for the same device.
 *
 * device_name_p (input)
 *     Pointer to the (zero-terminated) string that represents the device.
 *
 * Return value
 *     NULL    No device found with requested
 *     !NULL   device handle that can be used in the device API
 */
device_handle_t
eip_device_find(
        const char * sz_device_name_p);


/*-----------------------------------------------------------------------------
 * eip_device_get_reference
 *
 * This function can be used to obtain the implementation-specific device
 * reference
 *
 * device (input)
 *     handle for the device instance as returned by eip_device_find for which
 *     the reference must be obtained.
 *
 * data_p (output)
 *     Pointer to memory location where device data will be stored.
 *     If not used then set to NULL.
 *
 * Return value
 *     NULL    No reference found for the requested device instance
 *     !NULL   device reference
 */
device_reference_t
eip_device_get_reference(
        const device_handle_t device,
        device_data_t * const data_p);


/*-----------------------------------------------------------------------------
 * eip_device_get_name
 *
 * This function returns the device name for the provided index.
 *
 * index (input)
 *     device index in the device list for which the name must be obtained
 *
 * Return value
 *     NULL    No valid device found in the device list at the provided index
 *     !NULL   device name
 */
char *
eip_device_get_name(
        const unsigned int index);


/*-----------------------------------------------------------------------------
 * eip_device_get_index
 *
 * This function returns the device index for the provided device handle.
 *
 * device (input)
 *     handle for the device instance as returned by eip_device_find for which
 *     the reference must be obtained.
 *
 * Return value
 *     >= 0 device index
 *     <0   error
 */
int
eip_device_get_index(
        const device_handle_t device);


/*-----------------------------------------------------------------------------
 * eip_device_get_count
 *
 * This function returns the number of devices present in the device list.
 *
 * Return value
 *     device count.
 */
unsigned int
eip_device_get_count(void);


/*----------------------------------------------------------------------------
 * eip_device_get_properties
 *
 * Retrieve the properties of a device, the same as used in eip_device_add
 *
 * index (input)
 *     device index indicating which device to read the properties from.
 *
 * props_p (output)
 *     Pointer to memory location where device properties are stored,
 *     may not be NULL.
 *
 * f_valid_p (output)
 *     Flag indicating if there is a valid device at this index.
 *
 * Return value
 *     0    success
 *     -1   failure
 */
int
eip_device_get_properties(
        const unsigned int index,
        device_properties_t * const props_p,
        bool * const f_valid_p);


/*----------------------------------------------------------------------------
 * eip_device_add
 *
 * Adds a new device to the device list.
 *
 * This function must be called before any other function can reference
 * this device.
 *
 * index (input)
 *     device index where the device must be added in the device list
 *
 * props_p (input)
 *     Pointer to memory location where device properties are stored,
 *     may not be NULL
 *
 * Return value
 *     0    success
 *     -1   failure
 */
int
eip_device_add(
        const unsigned int index,
        const device_properties_t * const props_p);


/*----------------------------------------------------------------------------
 * eip_device_remove
 *
 * Removes device from the device list at the requested index,
 * the device must be previously added either statically or via a call
 * to the eip_device_add() function.
 *
 * This function must be called when no other driver function can reference
 * this device.
 *
 * index (input)
 *     device index where the device must be added in the device list
 *
 * Return value
 *     0    success
 *     -1   failure
 */
int
eip_device_remove(
        const unsigned int index);


#endif /* Include Guard */


/* end of file device_mgmt.h */
