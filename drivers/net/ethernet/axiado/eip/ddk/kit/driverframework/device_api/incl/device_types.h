/* device_types.h
 *
 * Driver Framework, device API, type Definitions
 *
 * The document "Driver Framework Porting Guide" contains the detailed
 * specification of this API. The information contained in this header file
 * is for reference only.
 */

/*****************************************************************************
* Copyright (c) 2007-2023 by Rambus, Inc. and/or its subsidiaries.
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

#ifndef INCLUDE_GUARD_DEVICE_TYPES_H
#define INCLUDE_GUARD_DEVICE_TYPES_H

/*----------------------------------------------------------------------------
 * device_handle_t
 *
 * This handle represents a device, typically one hardware block instance.
 *
 * The device API can access the static device resources (registers and RAM
 * inside the device) using offsets inside the device. This abstracts memory
 * map knowledge and simplifies device instantiation.
 *
 * Each device has its own configuration, including the endianness swapping
 * need for the words transferred. Endianness swapping can thus be performed
 * on the fly and transparent to the caller.
 *
 * The details of the handle are implementation specific and must not be
 * relied on, with one exception: NULL is guaranteed to be a non-existing
 * handle.
 */

typedef void * device_handle_t;


/*----------------------------------------------------------------------------
 * device_reference_t
 *
 * This is an implementation-specific reference for the device. It can
 * be passed from the implementation of the device API to other modules
 * for use, for example, with OS services that require such a reference.
 *
 * The details of the handle are implementation specific and must not be
 * relied on, with one exception: NULL is guaranteed to be a non-existing
 * handle.
 */
typedef void * device_reference_t;


/*----------------------------------------------------------------------------
 * device_data_t
 *
 * This is an implementation-specific reference for the device. It can
 * be passed from the implementation of the device API to other modules
 * for use, for example, with OS services that require such a reference.
 */
typedef struct
{
    // Physical address of the device mapped in memory
    void * phys_addr;

} device_data_t;


// device properties structure
typedef struct
{
    // device name
    char * name_p;

    // number of hardware device, for example different address ranges.
    unsigned int device_nr;

    // device offset range inside system memory map
    unsigned int start_byte_offset;
    unsigned int last_byte_offset;

    // Implementation specific device flags
    char flags;

} device_properties_t;


#endif /* Include Guard */


/* end of file device_types.h */
