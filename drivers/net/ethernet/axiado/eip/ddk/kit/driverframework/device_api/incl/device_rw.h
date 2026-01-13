/* device_rw.h
 *
 * Driver Framework, device API, Read/Write functions
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

#ifndef INCLUDE_GUARD_DEVICE_RW_H
#define INCLUDE_GUARD_DEVICE_RW_H

// Driver Framework Basic Defs API
#include "basic_defs.h"     // u32, inline

// Driver Framework device API
#include "device_types.h"   // device_handle_t


/*----------------------------------------------------------------------------
 * device_read32
 *
 * This function can be used to read one static 32bit resource inside a device
 * (typically a register or memory location). Since reading registers can have
 * side effects, the implementation must guarantee that the resource will be
 * read only once and no neighboring resources will be accessed.
 *
 * If required (decided based on internal configuration), on the fly endianness
 * swapping of the value read will be performed before it is returned to the
 * caller.
 *
 * device (input)
 *     handle for the device instance as returned by eip_device_find.
 *
 * byte_offset (input)
 *     The byte offset within the device for the resource to read.
 *
 * Return value
 *     The value read.
 *
 * When the device or offset parameters are invalid, or a read error occurs,
 * the implementation will return an unspecified value.
 */
u32
device_read32(
        const device_handle_t device,
        const unsigned int byte_offset);


/*----------------------------------------------------------------------------
 * device_read32_check
 *
 * This function can be used to read one static 32-bit resource inside a device
 * (typically a register or memory location). Since reading registers can have
 * side effects, the implementation must guarantee that the resource will be
 * read only once and no neighboring resources will be accessed.
 *
 * If required (decided based on internal configuration), on the fly endianess
 * swapping of the value read will be performed before it is returned to the
 * caller.
 *
 * device (input)
 *     handle for the device instance as returned by eip_device_find.
 *
 * byte_offset (input)
 *     The byte offset within the device for the resource to read.
 *
 * value_p (output)
 *     The 32-bit value read from the register.
 *
 * Return value
 *     0 for success, non-zero value for read error.
 *
 * When the device or offset parameters are invalid, or a read error occurs,
 * the implementation will return an unspecified value.
 */
int
device_read32_check(
        const device_handle_t device,
        const unsigned int byte_offset,
        u32 * const value_p);


/*----------------------------------------------------------------------------
 * device_write32
 *
 * This function can be used to write one static 32bit resource inside a
 * device (typically a register or memory location). Since writing registers
 * can have side effects, the implementation must guarantee that the resource
 * will be written exactly once and no neighboring resources will be
 * accessed.
 *
 * If required (decided based on internal configuration), on the fly endianness
 * swapping of the value to be written will be performed.
 *
 * device (input)
 *     handle for the device instance as returned by eip_device_find.
 *
 * byte_offset (input)
 *     The byte offset within the device for the resource to write.
 *
 * value (input)
 *     The 32bit value to write.
 *
 * Return value
 *     0 for success, non-zero value for write error.
 *
 * The write can only be successful when the device and byte_offset parameters
 * are valid.
 */
int
device_write32(
        const device_handle_t device,
        const unsigned int byte_offset,
        const u32 value);


/*----------------------------------------------------------------------------
 * device_read32_array
 *
 * This function perform the same task as device_read32 for an array of
 * consecutive 32bit words, allowing the implementation to use a more optimal
 * burst-read (if available).
 *
 * See device_read32 for pre-conditions and a more detailed description.
 *
 * device (input)
 *     handle for the device instance as returned by eip_device_find.
 *
 * start_byte_offset (input)
 *     byte offset of the first resource to read.
 *     This value is incremented by 4 for each following resource.
 *
 * memory_dst_p (output)
 *     Pointer to the memory where the retrieved words will be stored.
 *
 * count (input)
 *     The number of 32bit words to transfer.
 *
 * Return value
 *     0 for success, non-zero value for read error.
 *
 * When the device or offset parameters are invalid, or a read error occurs,
 * the implementation will return unspecified array values.
 */
int
device_read32_array(
        const device_handle_t device,
        const unsigned int start_byte_offset,
        u32 * memory_dst_p,
        const int count);


/*----------------------------------------------------------------------------
 * device_write32_array
 *
 * This function perform the same task as device_write32 for an array of
 * consecutive 32bit words, allowing the implementation to use a more optimal
 * burst-write (if available).
 *
 * See device_write32 for pre-conditions and a more detailed description.
 *
 * device (input)
 *     handle for the device instance as returned by eip_device_find.
 *
 * start_byte_offset (input)
 *     byte offset of the first resource to write.
 *     This value is incremented by 4 for each following resource.
 *
 * memory_src_p (input)
 *     Pointer to the memory where the values to be written are located.
 *
 * count (input)
 *     The number of 32bit words to transfer.
 *
 * Return value
 *     0 for success, non-zero value for write error.
 */
int
device_write32_array(
        const device_handle_t device,
        const unsigned int start_byte_offset,
        const u32 * memory_src_p,
        const int count);


/* The defines below specify the possible return codes of the implementations
   of these API functions */

/* device error return code for invalid parameter */
#define DEVICE_RW_PARAM_ERROR 0x00000010

/* device error return code for a not initialized situation */
#define DEVICE_NOT_INITIALIZED_ERROR 0xEEEEEEEE

#endif /* Include Guard */

/* end of file device_rw.h */
