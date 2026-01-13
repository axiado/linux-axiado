/* ringhelper.h
 *
 * Ring Helper Library API specification.
 */

/*****************************************************************************
* Copyright (c) 2008-2020 by Rambus, Inc. and/or its subsidiaries.
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

#ifndef INCLUDE_GUARD_RINGHELPER_H
#define INCLUDE_GUARD_RINGHELPER_H

#include "basic_defs.h"

/*----------------------------------------------------------------------------
 * ring_helper_cb_write_func_t
 *
 * This routine must write a limited number of descriptors to the command ring
 * and hand these off to the device for processing, using the mechanism
 * supported by the device.
 *
 * If this is a command-only ring (not shared with resutls) AND the
 * implementation of the callback interface is unable to provide the read
 * position of the device in the command ring (see ring_helper_cb_status_func_t)
 * then the Ring Helper module cannot know if the ring is almost full.
 * Therefore, under these conditions, the implementation of this callback
 * is also required to:
 * - check that the descriptor position to be written is indeed free and does
 *   not contain an unprocessed command descriptor (this happens when the ring
 *   becomes full).
 *
 * This function can be called in response to invoking the ring_helper_put API
 * function and is expected to be fully re-entrant. If this is not possible,
 * the caller must avoid the re-entrance of the ring_helper_put API function.
 *
 * callback_param1_p
 * callback_param2
 *     These parameters are anonymous for the Ring Helper and were provided by
 *     the caller during the ring initialization. They have meaning for the
 *     the module that is invoked by this callback.
 *
 * write_index
 *     zero-based index number in the command ring from where the descriptors
 *     must be written. The caller guarantees that the descriptors can be
 *     written sequentially and no index number wrapping will be required.
 *     The implementation is expected to know the address of the command ring
 *     and size of a descriptor.
 *
 * write_count
 *     The number of descriptors to immediately add to the command ring.
 *
 * AvailableSpace
 *     The currently available number of free positions in the command ring
 *     to where the descriptors can be written.
 *     The following always holds:
 *
 *     write_count <= AvailableSpace
 *
 *
 * descriptors_p
 *     Pointer to the memory where the descriptors are currently stored.
 *     The descriptors are available sequentially and back-to-back.
 *
 * descriptor_count
 *     The total number of descriptors that were requested to be added
 *     to the command ring by the caller of the ring_helper_put function.
 *     When a write operation is split in two (due to index number wrapping),
 *     this parameter is greater than write_count. Also if there is not enough
 *     space available in the ring, this paramater is greater than write_count.
 *     So the following holds:
 *
 *     descriptor_count == write_count, if no index number wrapping occured
 *                        in the caller's decision before calling this routine,
 *                        and there is enough space in the ring;
 *     descriptor_count > write_count, if an index number wrapping occured
 *                       in the caller's decision before calling this routine
 *                       or there is not enough space in the ring;
 *
 * descriptor_skip_count
 *     The number of descriptors from descriptors_p already copied. These
 *     descriptors must be skipped. This parameter is required because the Ring
 *     Helper does not know the size of a descriptor.
 *     When an operation is split in two (due to index number wrapping),
 *     the first call has this parameter set to zero,
 *     and the second has it set to the number of descriptors written
 *     by the first call.
 *
 * Return value
 *     <0  Failure code, this can be a Driver Library specific error code.
 *      0  No descriptors could be written, but no explicit error
 *     >0  Actual number of descriptors written
 *
 * It should be noted that returning a failure code will cause synchronization
 * with the device to be lost and probably requires a reset of the device to
 * recover.
 */
typedef int (* ring_helper_cb_write_func_t)(
                    void * const callback_param1_p,
                    const int callback_param2,
                    const unsigned int write_index,
                    const unsigned int write_count,
                    const unsigned int AvailableSpace,
                    const void * descriptors_p,
                    const int descriptor_count,
                    const unsigned int descriptor_skip_count);


/*----------------------------------------------------------------------------
 * ring_helper_cb_read_func_t
 *
 * This routine is expected to read a limited number of descriptors from the
 * result ring and then mark these positions as free, allowing new result
 * descriptors (separate rings) or command descriptors (shared ring) to be
 * written to these position. The exact method used depends on the device
 * implementation.
 *
 * This function can be called in response to invoking the ring_helper_get API
 * function and is expected to be fully re-entrant. If this is not possible,
 * the caller must avoid the re-entrance of the ring_helper_get API function.
 *
 * callback_param1_p
 * callback_param2
 *     These parameters are anonymous for the Ring Helper and were provided by
 *     the caller during the ring initialization. They have meaning for the
 *     the module that is invoked by this callback.
 *
 * read_limit
 *     The maximum number of descriptors that fit in descriptors_p.
 *     Also the maximum number of descriptors that can be read sequentially
 *     without having to wrap the read_index.
 *     Also the maximum number of descriptors that are ready, if this was
 *     indicated by the ready_count parameter in the call to ring_helper_get.
 *
 * read_index
 *     zero-based position in the result ring where the first descriptor can be
 *     read. The caller guarantees that up to read_limit descriptors can be read
 *     sequentially from this position without having to wrap the read_index.
 *     The implementation is expected to know the address of the command ring
 *     and size of a descriptor.
 *
 * descriptors_p
 *     Pointer to the block of memory where up to read_limit whole descriptors
 *     can be written back-to-back by the implementation.
 *
 * descriptor_skip_count
 *     The number of descriptors already stored from descriptors_p. This memory
 *     must be skipped. This parameter is required because the Ring Helper does
 *     not know the size of a descriptor.
 *     When an operation is split in two, the first call has this parameter set
 *     to zero, and the second has it set to the number of descriptors returned
 *     by the first call.
 *
 * Return value
 *    <0  Failure code, this can be a Driver Library specific error code.
 *     0  No ready descriptors were available in the result ring.
 *    >0  Actual number of descriptors copied from the result ring to the
 *        buffer pointed to by descriptors_p.
 *
 * It should be noted that returning a failure code will cause synchronization
 * with the device to be lost and probably requires a reset of the device to
 * recover.
 */
typedef int (* ring_helper_cb_read_func_t)(
                    void * const callback_param1_p,
                    const int callback_param2,
                    const unsigned int read_index,
                    const unsigned int read_limit,
                    void * descriptors_p,
                    const unsigned int descriptor_skip_count);


/*----------------------------------------------------------------------------
 * ring_helper_cb_status_func_t
 *
 * This routine reads and return the ring status from the device.
 *
 * This function can be called in response to invoking the ring_helper_put API
 * function and is expected to be fully re-entrant. If this is not possible,
 * the caller must avoid the re-entrance of the ring_helper_put API function.
 *
 * callback_param1_p
 * callback_param2
 *     These parameters are anonymous for the Ring Helper and were provided by
 *     the caller during the ring initialization. They have meaning for the
 *     the module that is invoked by this callback.
 *
 * device_read_pos_p
 *     Pointer to the output parameter that will receive the most recent read
 *     position used by the device. The value is the zero-based descriptor
 *     index in the command ring. The caller will assume that the device has
 *     not yet processed this descriptor.
 *     The value -1 must be returned when the device cannot provide this info.
 *     The implementation is expected to consistently return this value and
 *     not to return -1 selectively. When the function has returned -1 once,
 *     it is assumed not to support this functionality and will not be called
 *     anew thereafter.
 *
 * Return value
 *    <0  Failure code, this can be a Driver Library specific error code.
 *     0  Success  (also to be used when *device_read_pos_p was set to -1)
 *
 * It should be noted that returning a failure code will cause synchronization
 * with the device to be lost and probably requires a reset of the device to
 * recover.
 */
typedef int (* ring_helper_cb_status_func_t)(
                    void * const callback_param1_p,
                    const int callback_param2,
                    int * const device_read_pos_p);


/*----------------------------------------------------------------------------
 * ring_helper_callback_interface_t
 *
 * data structure containing pointers to callback functions that will be used
 * by the Ring Helper Library to manipulate descriptors in the ring.
 */
typedef struct
{
    // pointers to the functions that form the Callback Interface
    // see type definitions above for details

    // note: all three API functions are mandatory to provide
    ring_helper_cb_write_func_t  write_func_p;
    ring_helper_cb_read_func_t   read_func_p;
    ring_helper_cb_status_func_t status_func_p;

    // the following two parameters will be used as parameters when invoking
    // the above callback functions. The example usage is shown below.
    void * callback_param1_p;        // typically I/O Area pointer
    int callback_param2;             // typically Ring number

} ring_helper_callback_interface_t;


/*----------------------------------------------------------------------------
 * ring_helper_t
 *
 * data structure used to hold the ring administration data. The caller must
 * maintain an instance of this data structure for each ring.
 *
 * The caller should not access this data structure directly.
 *
 * For currency analysis, the following details have been specified:
 * - ring_helper_init writes all fields.
 * - ring_helper_put and ring_helper_get do NOT write shared fields and can
 *   therefore be used concurrently.
 * - ring_helper_get and RingHelper_Notify are multiple exlusive use by design.
 */
typedef struct
{
    unsigned int in_size;
    unsigned int in_tail;               // written by Put

    unsigned int out_size;
    unsigned int out_head;              // written by Get

    bool f_separate;
    bool f_supports_device_read_pos;        // written by Put

    // callback interface
    ring_helper_callback_interface_t cb;

} ring_helper_t;


/*----------------------------------------------------------------------------
 * ring_helper_init
 *
 * This routine must be called once to initialize a ring.
 *
 * The Ring Helper module will start to use the ring(s) from index zero. The
 * caller must make sure the device is configured accordingly.
 *
 * ring_p
 *     Pointer to the ring administration data.
 *
 * callback_if_p
 *     References to the callback functions and parameters that will be used
 *     by the Ring Helper to request manipulation of the descriptors.
 *     The entire structure pointed to by this parameters will be copied.
 *
 * f_separate_rings
 *     This parameter indicates whether the command and result rings share one
 *     ring (one block of memory) or use separate rings (two memory blocks).
 *
 * max_descriptors_command_ring
 * max_descriptors_result_ring
 *     The maximum number of descriptors that fit in each of the rings.
 *     The max_descriptors_result_ring is ignored when f_separate_rings is false.
 *
 * Return value
 *    <0  Failure code
 *     0  Success
 *
 * None of the other API functions may be called for this ring_p before this
 * function returns with a success return value.
 */
int
ring_helper_init(
        volatile ring_helper_t * const ring_p,
        volatile const ring_helper_callback_interface_t * const callback_if_p,
        const bool f_separate_rings,
        const unsigned int max_descriptors_command_ring,
        const unsigned int max_descriptors_result_ring);


/*----------------------------------------------------------------------------
 * ring_helper_put
 *
 * This routine can be used to add one or more descriptors to a specific
 * command ring.
 *
 * ring_p
 *     Pointer to the ring administration data.
 *
 * descriptors_p
 *     Pointer to where to read the array of descriptors.
 *
 * descriptor_count
 *     number of descriptors stored back-to-back in the array pointed to by
 *     descriptors_p.
 *
 * Return value
 *     <0  Failure code, this can be a Driver Library specific error code
 *         returned from the ring_helper_cb_write_func_t callback function.
 *     >0  number of descriptors actually added to the command ring.
 *      0  The command ring is full, no descriptors could be added
 *         The caller should retry when the device has indicated that some
 *         descriptors have been processed.
 *
 * This function is not re-entrant for the same ring_p. It is allowed to call
 * ring_helper_get concurrently, even for the same ring_p.
 */
int
ring_helper_put(
        volatile ring_helper_t * const ring_p,
        const void * descriptors_p,
        const int descriptor_count);


/*----------------------------------------------------------------------------
 * ring_helper_get
 *
 * This routine can be used to read one or more descriptors from a specific
 * result ring.
 *
 * ring_p
 *     Pointer to the ring administration data.
 *
 * ready_count
 *     The number of descriptors the caller guarantees are available in the
 *     result ring. This information is available from some devices.
 *     Use -1 if this information is not availalble. In this case the
 *     implementation of the read function (part of the callback interface)
 *     must check this information when retrieving the descriptors.
 *
 * descriptors_p
 *     Pointer to the buffer where the descriptors will be written.
 *
 * descriptors_limit
 *     The size of the buffer pointed to by descriptors_p, expressed in the
 *     maximum number of whole descriptors that fit into the buffer.
 *     Or, if smaller than the buffer size, the maximum number of whole
 *     descriptors that will be retrieved during this call.
 *
 * Return value
 *     <0 Failure code, this can be a Driver Library specific error code
 *         returned from the ring_helper_cb_read_func_t callback function.
 *      0 No descriptors ready to be retrieved, retry later
 *     >0 number of descriptors actually written from descriptors_p
 *
 * This function is not re-entrant for the same ring_p. It is allowed to call
 * ring_helper_put concurrently, even for the same ring_p.
 */
int
ring_helper_get(
        volatile ring_helper_t * const ring_p,
        const int ready_count,
        void * descriptors_p,
        const int descriptors_limit);


/*----------------------------------------------------------------------------
 * ring_helper_fill_level_get
 *
 * This function returns a number of filled (used) descriptors in the ring
 * specified.
 *
 * ring_p
 *     Pointer to the ring administration data.
 *
 * Return value
 *     <0 Failure code, this can be a Driver Library specific error code
 *      0 No filled descriptors, empty ring
 *     >0 number of filled descriptors in the ring
 *
 * This function is not re-entrant for the same ring_p. It is not allowed
 * to call it concurrently with ring_helper_put for the same ring_p.
 */
int
ring_helper_fill_level_get(
        volatile ring_helper_t * const ring_p);


#endif /* Include Guard */


/* end of file ringhelper.h */
