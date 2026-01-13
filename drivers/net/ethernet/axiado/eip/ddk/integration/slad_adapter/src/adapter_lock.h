/* adapter_lock.h
 *
 * Adapter concurrency (locking) management
 *
 * Adapter_Lock functions usage examples
 *
 * Example 1:
 *
 * static adapter_lock_t lock;
 * static unsigned long flags;
 *
 * adapter_lock_alloc(lock);
 * adapter_lock_acquire(lock, &flags);
 * ...
 * adapter_lock_release(lock, &flags);
 * adapter_lock_free(lock);
 *
 * Example 2:
 *
 * static ADAPTER_LOCK_DEFINE(lock_p);
 * static unsigned long flags;
 *
 * adapter_lock_acquire(lock_p, &flags);
 * ...
 * adapter_lock_release(lock_p, &flags);
 *
 */

/*****************************************************************************
* Copyright (c) 2013-2020 by Rambus, Inc. and/or its subsidiaries.
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

#ifndef INCLUDE_GUARD_ADAPTER_LOCK_H
#define INCLUDE_GUARD_ADAPTER_LOCK_H

/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Driver Framework Basic Definitions API
#include "basic_defs.h"         // bool


/*----------------------------------------------------------------------------
 * Definitions and macros
 */

// lock, use adapter_lock_null to set to a known uninitialized value
typedef void * adapter_lock_t;

// Critical section
typedef struct
{
    volatile void * p [2]; // Place holder

} adapter_lock_cs_t;


/*----------------------------------------------------------------------------
 * adapter_lock_t
 *
 * This handle can be assigned to a variable of type adapter_lock_t.
 *
 */
extern const adapter_lock_t adapter_lock_null;


/*----------------------------------------------------------------------------
 * adapter_lock_alloc
 */
adapter_lock_t
adapter_lock_alloc(void);


/*----------------------------------------------------------------------------
 * adapter_lock_free
 */
void
adapter_lock_free(
        adapter_lock_t lock);


/*----------------------------------------------------------------------------
 * adapter_lock_acquire
 *
 * Acquire the lock.
 *
 * lock (input):
 *      lock to acquire
 *
 * flags (input):
 *      Pointer to memory location where implementation-specific flags
 *      can be stored.
 *
 * Return value:
 *     none
 */
void
adapter_lock_acquire(
        adapter_lock_t lock,
        unsigned long * flags);


/*----------------------------------------------------------------------------
 * adapter_lock_release
 *
 * Release the lock previously acquired via adapter_lock_acquire().
 *
 * lock (input):
 *      lock to be released
 *
 * flags (input):
 *      Pointer to memory location where flags were stored by
 *      the corresponding adapter_lock_acquire() call.
 *
 * Return value:
 *     none
 */
void
adapter_lock_release(
        adapter_lock_t lock,
        unsigned long * flags);


/*----------------------------------------------------------------------------
 * adapter_lock_cs_set
 *
 * Set the lock for the critical section.
 *
 * Note: This function must be called prior to calling adapter_lock_cs_enter()
 *       or adapter_lock_cs_leave() functions.
 *       The lock cannot be changes while the critical section is entered.
 *
 * cs_p (output):
 *      Critical section where the lock data must be set.
 *
 * lock (input):
 *      Pointer to a lock instantiated by the ADAPTER_LOCK_DEFINE macro or
 *      allocated by adapter_lock_alloc() function
 *
 * Return value:
 *     none
 */
void
adapter_lock_cs_set(
        adapter_lock_cs_t * const cs_p,
        adapter_lock_t lock);


/*----------------------------------------------------------------------------
 * adapter_lock_cs_get
 *
 * Get the lock for the critical section.
 *
 * cs_p (input):
 *      Critical section for which the lock object must be obtained.
 *
 * Return value:
 *     lock
 */
adapter_lock_t
adapter_lock_cs_get(
        adapter_lock_cs_t * const cs_p);


/*----------------------------------------------------------------------------
 * adapter_lock_cs_enter
 *
 * Enter critical section
 *
 * Return code:
 *     true - section entered
 *     false - section not entered, another context is already executing it
 */
bool
adapter_lock_cs_enter(
        adapter_lock_cs_t * const cs_p);


/*----------------------------------------------------------------------------
 * adapter_lock_cs_leave
 *
 * Leave critical section
 *
 * Return value:
 *     none
 */
void
adapter_lock_cs_leave(
        adapter_lock_cs_t * const cs_p);


// Adapter Locking API extensions
#include "adapter_lock_ext.h"           // ADAPTER_LOCK_DEFINE


#endif // INCLUDE_GUARD_ADAPTER_LOCK_H


/* end of file adapter_lock.h */
