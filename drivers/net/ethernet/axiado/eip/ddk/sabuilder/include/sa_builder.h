/* sa_builder.h
 *
 * Main API for the SA Builder.
 */

/*****************************************************************************
* Copyright (c) 2011-2020 by Rambus, Inc. and/or its subsidiaries.
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


#ifndef SA_BUILDER_H_
#define SA_BUILDER_H_


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

#include "sa_builder_params.h"

// Driver Framework Basic Definitions API
#include "basic_defs.h"


/*----------------------------------------------------------------------------
 * Definitions and macros
 */
typedef enum
{
    SAB_STATUS_OK,
    SAB_INVALID_PARAMETER,
    SAB_UNSUPPORTED_FEATURE,
    SAB_ERROR
} sa_builder_status_t;


/*----------------------------------------------------------------------------
 * sa_builder_get_sizes();
 *
 * Compute the required sizes in 32-bit words of any of up to three memory
 * areas used by the SA.
 *
 * sa_params_p (input)
 *   Pointer to the SA parameters structure.
 * sa_word32_count_p (output)
 *   The size of the normal SA buffer,
 * sa_state_word32_count_p (output)
 *   The size of any SA state record.
 * arc4_state_word32_count_p (output) T
 *   The size of any ARCFOUR state buffer (output).
 *
 * When the SA state record or ARCFOUR state buffer are not required by
 * the packet engine for this transform, the corresponding size outputs
 * are returned as zero.
 *
 * If any of the output parameters is a null pointer,
 * the corresponding size will not be returned.
 *
 * The sa_params_p structure must be fully filled in: it must have the
 * same contents as it would have when sa_builder_build_sa is called.
 *
 * Return:
 * SAB_STATUS_OK on success
 * SAB_INVALID_PARAMETER when the record referenced by sa_params_p is invalid,
 * SAB_UNSUPPORTED_FEATURE when sa_params_p describes an operations that
 *    is not supported on the hardware for which this SA builder
 *    is configured.
 */
sa_builder_status_t
sa_builder_get_sizes(
    sa_builder_params_t *const sa_params_p,
    unsigned int *const sa_word32_count_p,
    unsigned int *const sa_state_word32_count_p,
    unsigned int *const arc4_state_word32_count_p);


/*----------------------------------------------------------------------------
 * sa_builder_build_sa();
 *
 * Construct the SA record for the operation described in sa_params_p in
 * up to three memory buffers. Update the field offset values in the
 * SA parameter structure.
 *
 * sa_params_p (input, output)
 *    Pointer to the SA parameters structure. Field offset values will
 *    be updated.
 * sa_buffer_p (output)
 *    Pointer to the the normal SA buffer,
 * sa_state_buffer_p (output)
 *    Pointer to the SA state record buffer,
 * arc4_state_buffer_p (output)
 *    Pointer to the ARCFOUR state buffer.
 *
 * Each of the Buffer arguments must point to a word-aligned
 * memory buffer whose size in words is at least equal to the
 * corresponding size parameter returned by sa_builder_get_sizes().
 *
 * If any of the three buffers is not required for the SA (the
 * corresponding size in sa_builder_get_sizes() is 0), the corresponding
 * Buffer arguments to this function may be a null pointer.
 *
 * Return:
 * SAB_STATUS_OK on success
 * SAB_INVALID_PARAMETER when sa_params_p is invalid, or if any of
 *    the buffer arguments  is a null pointer while the corresponding buffer
 *    would be required for the operation.
 * SAB_UNSUPPORTED_FEATURE when sa_params_p describes an operations that
 *    is not supported on the hardware for which this SA builder
 *    is configured.
 */
sa_builder_status_t
sa_builder_build_sa(
    sa_builder_params_t * const sa_params_p,
    u32 *const sa_buffer_p,
    u32 *const sa_state_buffer_p,
    u32 *const arc4_state_buffer_p);


/*----------------------------------------------------------------------------
 * sa_builder_set_large_transform_offset();
 *
 * Set the (global) offset between the fields in Large transform records versus
 * small transform records. For some hardware configurations (especially those
 * with 1024-bit anti-replay masks) large transform records must contain more
 * space than usual. This function is intended to be used when the hardware
 * configuration is only known at runtime.
 *
 * offset (inout)
 *     offset value in words.
 * Return:
 * SAB_STATUS_OK on success
 * SAB_INVALID_PARAMETER when the offset is out of range.
 * SAB_UNSUPPORTED_FEATURE when the SA Builder is configured without support
 * for this feature.
 */
sa_builder_status_t
sa_builder_set_large_transform_offset(
    unsigned int offset);


#endif /* SA_BUILDER_H_ */


/* end of file sa_builder.h */
