/* ringhelper.c
 *
 * Ring Helper Library implementation.
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

#include "c_ringhelper.h"  // configuration options

#include "basic_defs.h"     // bool, min
#include "ringhelper.h"     // API to implement
#include "clib.h"           // memset


/*----------------------------------------------------------------------------
 * ring_helper_init
 *
 * This routine must be called once to initialize the administration block
 * related to a ring.
 *
 * See header file for function specification.
 */
int
ring_helper_init(
        volatile ring_helper_t * const ring_p,
        volatile const ring_helper_callback_interface_t * const callback_if_p,
        const bool f_separate_rings,
        const unsigned int command_ring_max_descriptors,
        const unsigned int result_ring_max_descriptors)
{
#ifdef RINGHELPER_STRICT_ARGS
    if (ring_p == NULL ||
        callback_if_p == NULL ||
        command_ring_max_descriptors < 1)
    {
        // invalid argument
        return -1;
    }

    if (f_separate_rings)
    {
#ifdef RINGHELPER_REMOVE_SEPARATE_RING_SUPPORT
        // not supported
        return -1;
#else
        if (result_ring_max_descriptors < 1)
        {
            // invalid arguments
            return -1;
        }
#endif
    }

    if (callback_if_p->read_func_p == NULL ||
        callback_if_p->write_func_p == NULL)
    {
        return -1;
    }

#ifndef RINGHELPER_REMOVE_STATUSFUNC
    if (callback_if_p->status_func_p == NULL)
    {
        return -1;
    }
#endif

#endif /* RINGHELPER_STRICT_ARGS */

    ring_p->cb = *callback_if_p;
    ring_p->f_supports_device_read_pos = true;      // initial assumption
    ring_p->in_size = command_ring_max_descriptors;
    ring_p->in_tail = 0;
    ring_p->out_head = 0;

#ifndef RINGHELPER_REMOVE_SEPARATE_RING_SUPPORT
    if (f_separate_rings)
    {
        // separate rings
        ring_p->f_separate = true;
        ring_p->out_size = result_ring_max_descriptors;
    }
    else
#endif /* !RINGHELPER_REMOVE_SEPARATE_RING_SUPPORT */
    {
        // combined rings
        ring_p->f_separate = false;
        ring_p->out_size = command_ring_max_descriptors;
    }

    return 0;   // success
}


/*----------------------------------------------------------------------------
 * ring_helper_put
 *
 * This function tries to add a number of descriptors to the command ring
 * specified.
 *
 * See header file for function specification.
 */
int
ring_helper_put(
        volatile ring_helper_t * const ring_p,
        const void * descriptors_p,
        const int descriptor_count)
{
    int A, N, w1, w2;

#ifdef RINGHELPER_STRICT_ARGS
    if (ring_p == NULL ||
        descriptors_p == NULL ||
        descriptor_count < 0)
    {
        return -1;
    }
#endif /* RINGHELPER_STRICT_ARGS */

    if (descriptor_count == 0)
        return 0;

    w2 = 0;

#ifndef RINGHELPER_REMOVE_SEPARATE_RING_SUPPORT
    // out of the descriptors provided, calculate the maximum number of
    // descriptors that can be written sequentially before the ring is full.
    if (ring_p->f_separate)
    {
        // separate rings

        // ask how far the device has processed the ring
        // we do this on every call and do not cache the result
        int device_read_head = -1;    // not supported

#ifndef RINGHELPER_REMOVE_STATUSFUNC
        if (ring_p->f_supports_device_read_pos)
        {
            int res;

            res = ring_p->cb.status_func_p(
                                 ring_p->cb.callback_param1_p,
                                 ring_p->cb.callback_param2,
                                 &device_read_head);

            if (res < 0)
                return res;     // ## RETURN ##

            // suppress these calls if the device does not support it
            if (device_read_head < 0)
                ring_p->f_supports_device_read_pos = false;
        }
#endif /* !RINGHELPER_REMOVE_STATUSFUNC */

        if (device_read_head < 0)
        {
            // device does not expose its read position
            // this means we cannot calculate how much space is available
            // the WriteFunc will have to check, descriptor by descriptor
            A = ring_p->in_size;

            // note: under this condition we rely on the implementation of
            // the callback interface to handle ring-full condition and not
            // overwrite existing descriptors. Because of this, we can
            // fill the ring to the limit and do not have to keep 1 free
            // position as done below.
        }
        else
        {
            unsigned int device_in_head = (unsigned int)device_read_head;

            // based on the device read position we can calculate
            // how many positions in the ring are free
            if (ring_p->in_tail < device_in_head)
            {
                // we have wrapped around
                // available space is between the two
                A = device_in_head - ring_p->in_tail;
            }
            else
            {
                // used positions are between the two pointers
                // rest is free
                A = ring_p->in_size - (ring_p->in_tail - device_in_head);
            }

            // avoid filling the entire ring
            // so we can differentiate full from empty
            if (A != 0)
                A--;
        }
    }
    else
#endif /* !RINGHELPER_REMOVE_SEPARATE_RING_SUPPORT */
    {
        // combined rings

        // Critical: we have to be careful not to read the out_head more
        //           than one, since it might change in between!
        unsigned int out_head_copy = ring_p->out_head;

        // we can write descriptors up to the point where we expect the
        // result descriptors
        if (ring_p->in_tail < out_head_copy)
        {
            // used positions are around the wrap point
            // free positions are between the pointers
            A = out_head_copy - ring_p->in_tail;
        }
        else
        {
            // used positions are between the two pointers
            // rest is free
            A = ring_p->in_size - (ring_p->in_tail - out_head_copy);
        }

        // avoid filling the entire ring
        // so we can differentiate full from empty
        // (when it contains all commands or all results)
        if (A != 0)
            A--;
    }

    // limit based on provided descriptors
    A = MIN(A, descriptor_count);

    // limit for sequential writing
    N = MIN(A, (int)(ring_p->in_size - ring_p->in_tail));

    // bail out early if there is no space
    if (N == 0)
    {
        return 0;       // ## RETURN ##
    }

    w1 = ring_p->cb.write_func_p(
                        ring_p->cb.callback_param1_p,
                        ring_p->cb.callback_param2,
                        /*write_index:*/ring_p->in_tail,
                        /*write_count:*/N,
                        /*AvailableSpace*/A,
                        descriptors_p,
                        descriptor_count,
                        /*SkipCount:*/0);

    if (w1 <= 0)
    {
        //  0: no descriptors could be added
        // <0: failure
        return w1;      // ## RETURN ##
    }

    if (w1 == N &&
        w1 < descriptor_count &&
        A > N)
    {
        // we have written all possible positions up to the end of the ring
        // now write the rest
        N = A - N;

        w2 = ring_p->cb.write_func_p(
                            ring_p->cb.callback_param1_p,
                            ring_p->cb.callback_param2,
                            /*write_index:*/0,
                            /*write_count:*/N,
                            /*AvailableSpace*/N,
                            descriptors_p,
                            descriptor_count,
                            /*SkipCount:*/w1);

        if (w2 < 0)
        {
            // failure
            return w2;      // ## RETURN ##
        }
    }

    // now update the position for the next write
    {
        unsigned int i = ring_p->in_tail + w1 + w2;

        // do not use % operator to avoid costly divisions
        if (i >= ring_p->in_size)
            i -= ring_p->in_size;

        ring_p->in_tail = i;
    }

    // return how many descriptors were added
    return w1 + w2;
}


/*----------------------------------------------------------------------------
 * ring_helper_get
 *
 * This routine retrieves a number of descriptors from the result ring
 * specified.
 *
 * See header file for function specification.
 */
int
ring_helper_get(
        volatile ring_helper_t * const ring_p,
        const int ready_count,
        void * descriptors_p,
        const int descriptors_limit)
{
    int A, N;
    int r1, r2;

    r2 = 0;

#ifdef RINGHELPER_STRICT_ARGS
    if (ring_p == NULL ||
        descriptors_p == NULL ||
        descriptors_limit < 0)
    {
        return -1;
    }
#endif /* RINGHELPER_STRICT_ARGS */

    if (descriptors_limit == 0 ||
        ready_count == 0)
    {
        // no space in output buffer
        // or no descriptors ready
        return 0;
    }

    // calculate the maximum number of descriptors that can be retrieved
    // sequentially from this read position, taking into account the
    // descriptors_limit and the ready_count (if available)

    // A = entries in result ring from read position till end
    A = ring_p->out_size - ring_p->out_head;

    N = MIN(A, descriptors_limit);

    if (ready_count > 0)
        N = MIN(N, ready_count);

    // now retrieve this number of descriptors
    r1 = ring_p->cb.read_func_p(
                        ring_p->cb.callback_param1_p,
                        ring_p->cb.callback_param2,
                        /*read_index:*/ring_p->out_head,
                        /*read_limit:*/N,
                        descriptors_p,
                        /*SkipCount:*/0);

    if (r1 <= 0)
    {
        //  0: if we got nothing on the first call, we can stop here
        // <0: error while reading
        //     this means we cannot maintain read synchronization

        return r1;      // ## RETURN ##
    }

    // if we got the maximum, we can try to read more
    // after wrapping to the start of the buffer
    if (r1 == N &&
        r1 < descriptors_limit &&
        r1 != ready_count)
    {
        // A = number of entries in ring up to previous read-start position
        A = ring_p->out_head;

        N = MIN(A, descriptors_limit - r1);

        if (ready_count > 0)
            N = MIN(N, ready_count - r1);

        r2 = ring_p->cb.read_func_p(
                            ring_p->cb.callback_param1_p,
                            ring_p->cb.callback_param2,
                            /*read_index:*/0,        // start of buffer
                            /*read_limit:*/N,
                            descriptors_p,
                            /*SkipCount:*/r1);

        if (r2 < 0)
        {
            // failure
            return r2;      // ## RETURN ##
        }
    }

    // now update the position for the next read
    {
        unsigned int i = ring_p->out_head + r1 + r2;

        // do not use % operator to avoid costly divisions
        if (i >= ring_p->out_size)
            i -= ring_p->out_size;

        ring_p->out_head = i;
    }

    // return the number of descriptors read
    return r1 + r2;
}


/*----------------------------------------------------------------------------
 * ring_helper_fill_level_get
 *
 * This function return a number of filled (used) descriptors in the ring
 * specified.
 *
 * See header file for function specification.
 */
int
ring_helper_fill_level_get(
        volatile ring_helper_t * const ring_p)
{
#ifdef RINGHELPER_STRICT_ARGS
    if (ring_p == NULL)
    {
        return -1;
    }
#endif /* RINGHELPER_STRICT_ARGS */

#ifndef RINGHELPER_REMOVE_SEPARATE_RING_SUPPORT
    // out of the descriptors provided, calculate the maximum number of
    // descriptors that can be written sequentially before the ring is full.
    if (ring_p->f_separate)
    {
        // separate rings

        return -1; // not implemented
    }
    else
#endif  /* !RINGHELPER_REMOVE_SEPARATE_RING_SUPPORT */
    {
        // combined rings

        unsigned int fill_level;
        unsigned int out_head_copy;

        // Critical: we have to be careful not to read the out_head more
        //           than one, since it might change in between!
        out_head_copy = ring_p->out_head;

        if (ring_p->in_tail < out_head_copy)
        {
            // used positions are around the wrap point
            // free positions are between the pointers
            fill_level = ring_p->in_size - (out_head_copy - ring_p->in_tail);
        }
        else
        {
            // used positions are between the two pointers
            // rest is free
            fill_level = ring_p->in_tail - out_head_copy;
        }

        // avoid filling the entire ring
        // so we can differentiate full from empty
        // (when it contains all commands or all results)
        if (fill_level < ring_p->in_size)
            fill_level++;

        return (int)fill_level;
    }
}


/* end of file ringhelper.c */
