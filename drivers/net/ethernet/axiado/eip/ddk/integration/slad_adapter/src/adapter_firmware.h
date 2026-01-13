/* adapter_firmware.h
 *
 * Interface for obtaining the firmware image.
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

#ifndef INCLUDE_GUARD_ADAPTER_FIRMWARE_H
#define INCLUDE_GUARD_ADAPTER_FIRMWARE_H


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Driver Framework Basic Defs API
#include "basic_defs.h"


/*----------------------------------------------------------------------------
 * Definitions and macros
 */

// This data type represents a firmware resource.
// use adapter_firmware_null to set to a known uninitialized value
typedef void * adapter_firmware_t;

/*----------------------------------------------------------------------------
 * adapter_firmware_null
 *
 * This handle can be assigned to a variable of type adapter_firmware_t.
 *
 */
extern const adapter_firmware_t adapter_firmware_null;


/*----------------------------------------------------------------------------
 * adapter_firmware_acquire
 *
 * Obtain access to a specific firmware image in the form of an array of 32-bit
 * words. This function allocates any required buffers to store the
 * firmware. Multiple calls to this function are possible and multiple
 * firmware images remain valid at the same time. Access to the firmware
 * image remains valid until adapter_firmware_release is called.
 *
 * firmware_name_p (input)
 *       Null terminated string that indicates which firmware to load.
 *       This is typically a file name under a implementation-defined
 *       fixed directory, but not all implementations are required to
 *       load firmware from a file system.
 *
 * firmware_p (output)
 *       Pointer to array of 32-bit words that represents the loaded firmware.
 *
 * firmware_word32_count (output)
 *       size of the array in 32-bit words.
 *
 * Return: adapter_firmware_null if firmware failed to load.
 *         any other value on success, can be passed to
 *                          adapter_firmware_release
 */
adapter_firmware_t
adapter_firmware_acquire(
    const char * firmware_name_p,
    const u32 ** firmware_p,
    unsigned int  * firmware_word32_count);

/*----------------------------------------------------------------------------
 * adapter_firmware_release
 *
 * Release any resources that were allocated by a single call to
 * adapter_firmware_acquire. It is illegal to call this function multiple
 * times for the same handle.
 *
 * firmware_handle (input)
 *         handle as returned by adapter_firmware_acquire().
 */
void
adapter_firmware_release(
   adapter_firmware_t firmware_handle);



#endif /* Include Guard */

/* end of file adapter_firmware.h */
