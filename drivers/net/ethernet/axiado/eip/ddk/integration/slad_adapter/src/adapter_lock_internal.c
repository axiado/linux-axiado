/* adapter_lock_internal.c
 *
 * Adapter concurrency (locking) management
 * Generic implementation
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

/*----------------------------------------------------------------------------
 * This module implements (provides) the following interface(s):
 */

// Adapter locking API
#include "adapter_lock.h"


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Driver Framework Basic Definitions API
#include "basic_defs.h"         // bool

// Adapter lock internal API
#include "adapter_lock_internal.h"

#define TEST_SIZEOF(type, size) \
    extern int must_bigger[1 - 2*((int)(sizeof(type) > size))]

// Check the size of the data structures
TEST_SIZEOF(adapter_lock_cs_internal_t, sizeof(adapter_lock_cs_t));


/*----------------------------------------------------------------------------
 * adapter_lock_null
 *
 */
const adapter_lock_t adapter_lock_null = NULL;


/*----------------------------------------------------------------------------
 * adapter_lock_cs_set
 */
void
adapter_lock_cs_set(
        adapter_lock_cs_t * const cs_p,
        adapter_lock_t lock)
{
    adapter_lock_cs_internal_t * cs_internal_p =
                                 (adapter_lock_cs_internal_t*)cs_p;

    if (cs_internal_p)
    {
        // lock cannot be set while the critical section is entered
        if (cs_internal_p->f_locked == false)
            cs_internal_p->lock_p = lock;
    }
}


/*----------------------------------------------------------------------------
 * adapter_lock_cs_get
 */
adapter_lock_t
adapter_lock_cs_get(
        adapter_lock_cs_t * const cs_p)
{
    adapter_lock_cs_internal_t * cs_internal_p =
                                 (adapter_lock_cs_internal_t*)cs_p;

    if (cs_internal_p)
        return cs_internal_p->lock_p;
    else
        return adapter_lock_null;
}


/*----------------------------------------------------------------------------
 * adapter_lock_cs_enter
 */
bool
adapter_lock_cs_enter(
        adapter_lock_cs_t * const cs_p)
{
    unsigned long flags;
    adapter_lock_cs_internal_t * cs_internal_p =
                                (adapter_lock_cs_internal_t*)cs_p;

    if (cs_internal_p == NULL)
        return false;

    // Enter critical section
    adapter_lock_acquire(cs_internal_p->lock_p, &flags);

    // Check if critical section is already entered
    if (cs_internal_p->f_locked)
    {
        adapter_lock_release(cs_internal_p->lock_p, &flags);
        return false; // CS already entered
    }
    else
        cs_internal_p->f_locked = true; // success

    // Leave critical section
    adapter_lock_release(cs_internal_p->lock_p, &flags);

    return true;
}


/*----------------------------------------------------------------------------
 * adapter_lock_cs_leave
 */
void
adapter_lock_cs_leave(
        adapter_lock_cs_t * const cs_p)
{
    adapter_lock_cs_internal_t * cs_internal_p =
                                (adapter_lock_cs_internal_t*)cs_p;

    if (cs_internal_p)
        cs_internal_p->f_locked = false;
}


/* end of file adapter_lock_internal.c */
