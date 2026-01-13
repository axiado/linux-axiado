/* rpm_device_macros.h
 *
 * Runtime power Management (RPM) device Macros API
 *
 */

/*****************************************************************************
* Copyright (c) 2015-2020 by Rambus, Inc. and/or its subsidiaries.
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

#ifndef INCLUDE_GUARD_RPM_DEVICE_MACROS_H
#define INCLUDE_GUARD_RPM_DEVICE_MACROS_H

/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Driver Framework Basic Definitions API
#include "basic_defs.h"     // false, IDENTIFIER_NOT_USED


/*----------------------------------------------------------------------------
 * Definitions and macros
 */

#define RPM_SUCCESS     0   // No error

#define RPM_DEVICE_CAPABILITIES_STR_MACRO     "RPM stubbed"


/*----------------------------------------------------------------------------
 * RPM_DEVICE_INIT_START_MACRO
 *
 * Expands into a single line expression
 */
#define RPM_DEVICE_INIT_START_MACRO(DevId, SuspendFunc, ResumeFunc) RPM_SUCCESS

/*----------------------------------------------------------------------------
 * RPM_DEVICE_INIT_STOP_MACRO
 *
 * Expands into a single line expression
 */
#define RPM_DEVICE_INIT_STOP_MACRO(DevId) RPM_SUCCESS


/*----------------------------------------------------------------------------
 * RPM_DEVICE_UNINIT_START_MACRO
 *
 * Expands into a single line expression
 */
#define RPM_DEVICE_UNINIT_START_MACRO(DevId, f_resume) RPM_SUCCESS


/*----------------------------------------------------------------------------
 * RPM_DEVICE_UNINIT_STOP_MACRO
 *
 * Expands into a single line expression
 */
#define RPM_DEVICE_UNINIT_STOP_MACRO(DevId) RPM_SUCCESS


/*----------------------------------------------------------------------------
 * RPM_DEVICE_IO_START_MACRO
 *
 * Expands into a single line expression
 */
#define RPM_DEVICE_IO_START_MACRO(DevId, flag) RPM_SUCCESS


/*----------------------------------------------------------------------------
 * RPM_DEVICE_IO_STOP_MACRO
 *
 * Expands into a single line expression
 */
#define RPM_DEVICE_IO_STOP_MACRO(DevId, flag) RPM_SUCCESS


#endif /* INCLUDE_GUARD_RPM_DEVICE_MACROS_H */


/* rpm_device_macros.h */
