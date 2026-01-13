/* api_memxs.h
 *
 * Low-level Memory Access (MemXS) API
 *
 * This API should be used for debugging purposes only.
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


#ifndef API_MEMXS_H_
#define API_MEMXS_H_


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Driver Framework Basic Definitions API
#include "basic_defs.h"


/*----------------------------------------------------------------------------
 * Definitions and macros
 */

typedef enum
{
    MEMXS_STATUS_OK,              // Operation is successful
    MEMXS_INVALID_PARAMETER,      // Invalid input parameter
    MEMXS_UNSUPPORTED_FEATURE,    // Feature is not implemented
    MEMXS_ERROR                   // Operation has failed
} mem_xs_status_t;

/*----------------------------------------------------------------------------
 * mem_xs_handle_t
 *
 * This handle is a reference to a memory resource.
 *
 * The handle is set to NULL when mem_xs_handle_t handle.p is equal to NULL.
 *
 */
typedef struct
{
    void * p;
} mem_xs_handle_t;

/*----------------------------------------------------------------------------
 * mem_xs_dev_info_t
 *
 * Memory device data structure.
 *
 */
typedef struct
{
    // Memory device handle
    mem_xs_handle_t handle;

    // device name (zero-terminated string)
    const char * name_p;

    // device index
    unsigned int index;

    // device start offset
    unsigned int first_ofs;

    // device end offset
    unsigned int last_ofs;

} mem_xs_dev_info_t;

/*----------------------------------------------------------------------------
 * mem_xs_null_handle
 *
 * This handle can be assigned to a variable of type mem_xs_handle_t.
 *
 */
extern const mem_xs_handle_t mem_xs_null_handle;


/*----------------------------------------------------------------------------
 * mem_xs_init
 *
 * An application must call this API once prior to using any other API
 * functions. During this call the service ensures that the Additional address
 * Space and device are accessible for read/write operations.
 *
 * Return value:
 *      true:  Initialization was successful, the other APIs can now be used.
 *      false: Initialization failed, the other APIs cannot be used.
 */
bool
mem_xs_init (void);


/*----------------------------------------------------------------------------
 * mem_xs_handle_is_same
 *
 * Check whether provided handle1 is equal to provided handle2.
 *
 * handle1_p
 *      First handle
 *
 * handle2_p
 *      Second handle
 *
 * Return values
 *      true:  provided handles are equal
 *      false: provided handles are not equal
 *
 */
bool
mem_xs_handle_is_same(
        const mem_xs_handle_t * const handle1_p,
        const mem_xs_handle_t * const handle2_p);


/*-----------------------------------------------------------------------------
 * mem_xs_device_count_get
 *
 * Returns the number of device which memory (MMIO, internal RAM) can be
 * accessed via this API.
 *
 * device_count_p (output)
 *      Pointer to a memory location where the number of supported devices
 *      will be stored.
 *
 * Return:
 *     MEMXS_STATUS_OK
 *     MEMXS_INVALID_PARAMETER
 *     MEMXS_ERROR
 */
mem_xs_status_t
mem_xs_device_count_get(
        unsigned int * const device_count_p);


/*-----------------------------------------------------------------------------
 * mem_xs_device_info_get
 *
 * Returns the data structure that describes the device identified by
 * device_index parameter.
 *
 * device_index (input)
 *      device index, from 0 to the number obtained via
 *      the mem_xs_device_count_get() function.
 *
 * device_info_p (output)
 *      Pointer to a memory location where the number of supported devices
 *      will be stored.
 *
 * Return:
 *     MEMXS_STATUS_OK
 *     MEMXS_INVALID_PARAMETER
 *     MEMXS_ERROR
 */
mem_xs_status_t
mem_xs_device_info_get(
        const unsigned int device_index,
        mem_xs_dev_info_t * const device_info_p);


/*----------------------------------------------------------------------------
 * mem_xs_read32
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
 * handle (input)
 *     handle for the device instance which memory must be read.
 *
 * byte_offset (input)
 *     The byte offset within the device for the resource to read.
 *
 * Return value
 *     The value read.
 *
 * When the handle or offset parameters are invalid, the implementation will
 * return an unspecified value.
 */
u32
mem_xs_read32(
        const mem_xs_handle_t handle,
        const unsigned int byte_offset);


/*----------------------------------------------------------------------------
 * mem_xs_write32
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
 * handle (input)
 *     handle for the device instance which memory must be read.
 *
 * byte_offset (input)
 *     The byte offset within the device for the resource to write.
 *
 * value (input)
 *     The 32bit value to write.
 *
 * Return value
 *     None
 *
 * The write can only be successful when the handle and byte_offset parameters
 * are valid.
 */
void
mem_xs_write32(
        const mem_xs_handle_t handle,
        const unsigned int byte_offset,
        const u32 value);


/*----------------------------------------------------------------------------
 * mem_xs_read32_array
 *
 * This function perform the same task as mem_xs_read32 for an array of
 * consecutive 32bit words, allowing the implementation to use a more optimal
 * burst-read (if available).
 *
 * See mem_xs_read32 for pre-conditions and a more detailed description.
 *
 * handle (input)
 *     handle for the device instance which memory must be read.
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
 *     None.
 */
void
mem_xs_read32_array(
        const mem_xs_handle_t handle,
        const unsigned int start_byte_offset,
        u32 * memory_dst_p,
        const int count);


/*----------------------------------------------------------------------------
 * mem_xs_write32_array
 *
 * This function perform the same task as mem_xs_write32 for an array of
 * consecutive 32bit words, allowing the implementation to use a more optimal
 * burst-write (if available).
 *
 * See mem_xs_write32 for pre-conditions and a more detailed description.
 *
 * handle (input)
 *     handle for the device instance which memory must be read.
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
 *     None.
 */
void
mem_xs_write32_array(
        const mem_xs_handle_t handle,
        const unsigned int start_byte_offset,
        const u32 * memory_src_p,
        const int count);


#endif /* API_MEMXS_H_ */


/* end of file api_memxs.h */
