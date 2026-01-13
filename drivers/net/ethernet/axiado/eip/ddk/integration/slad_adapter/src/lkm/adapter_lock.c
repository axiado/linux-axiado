/* adapter_lock.c
 *
 * Adapter concurrency (locking) management
 * Linux kernel-space implementation
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
#include "basic_defs.h"         // IDENTIFIER_NOT_USED

// Adapter lock internal API
#include "adapter_lock_internal.h"

// Adapter Memory Allocation API
#include "adapter_alloc.h"

// Logging API
#undef LOG_SEVERITY_MAX
#define LOG_SEVERITY_MAX    LOG_SEVERITY_WARN
#include "log.h"

// Linux Kernel API
#include <linux/spinlock.h>     // spinlock_*


/*----------------------------------------------------------------------------
 * Definitions and macros
 */


/*----------------------------------------------------------------------------
 * adapter_lock_alloc
 */
adapter_lock_t
adapter_lock_alloc(void)
{
    spinlock_t * lock_p;

    size_t lock_size=sizeof(spinlock_t);
    if (lock_size==0)
        lock_size=4;

    lock_p = adapter_alloc(lock_size);
    if (lock_p == NULL)
        return adapter_lock_null;

    log_formatted_message("%s: lock = spinlock\n", __func__);

    spin_lock_init(lock_p);

    return lock_p;
}


/*----------------------------------------------------------------------------
 * adapter_lock_free
 */
void
adapter_lock_free(adapter_lock_t lock)
{
    adapter_free((void*)lock);
}


/*----------------------------------------------------------------------------
 * adapter_lock_acquire
 */
void
adapter_lock_acquire(
        adapter_lock_t lock,
        unsigned long * flags)
{
    spin_lock_irqsave((spinlock_t *)lock, *flags);
}


/*----------------------------------------------------------------------------
 * adapter_lock_release
 */
void
adapter_lock_release(
        adapter_lock_t lock,
        unsigned long * flags)
{
    spin_unlock_irqrestore((spinlock_t *)lock, *flags);
}


/* end of file adapter_lock.c */
