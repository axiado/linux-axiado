/* adapter_alloc.h
 *
 * Linux kernel-space implementation of the Adapter buffer allocation
 * functionality.
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

#ifndef ADAPTER_ALLOC_H_
#define ADAPTER_ALLOC_H_

/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Linux Kernel API
#include <linux/kernel.h>
#include <linux/slab.h>         // kmalloc, kfree
#include <linux/hardirq.h>      // in_atomic


/*----------------------------------------------------------------------------
  adapter_alloc
*/
static inline void *
adapter_alloc(unsigned int size)
{
    if (size > 0) // Many callers do not check if requested size is non-zero
        // kmalloc() may return non-NULL for size=0
        return kmalloc(size, in_atomic() ? GFP_ATOMIC : GFP_KERNEL);
    else
        return NULL; // ... but only extreme optimists do not check the result!
}


/*----------------------------------------------------------------------------
  adapter_free
*/
static inline void
adapter_free(void *p)
{
    if (p != NULL)
    {
        kfree(p); // kfree() may take a NULL pointer but why bother
        p = NULL;
    }
}


#endif /* ADAPTER_ALLOC_H_ */


/* end of file adapter_alloc.h */
