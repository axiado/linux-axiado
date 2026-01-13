/* adapter_interrupts.h
 *
 * Adapter Interrupts interface
 */

/*****************************************************************************
* Copyright (c) 2010-2020 by Rambus, Inc. and/or its subsidiaries.
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

#ifndef INCLUDE_GUARD_ADAPTER_INTERRUPTS_H
#define INCLUDE_GUARD_ADAPTER_INTERRUPTS_H

/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Driver Framework Basic Defs API
#include "basic_defs.h"     // bool


/*----------------------------------------------------------------------------
 * Definitions and macros
 */

typedef void (* adapter_interrupt_handler_t)(const int n_irq, const unsigned int flags);


/*----------------------------------------------------------------------------
 *                           Adapter_Interrupts
 *----------------------------------------------------------------------------
 */

int
adapter_interrupts_init(
        const int n_irq);

int
adapter_interrupts_uninit(
        const int n_irq);

int
adapter_interrupt_set_handler(
        const int n_irq,
        adapter_interrupt_handler_t handler_function);

int
adapter_interrupt_enable(
        const int n_irq,
        const unsigned int flags);

/*
 * This function enables execution context (allows it to run) that services
 * the n_irq interrupt.
 *
 * Note: this function is different from the adapter_interrupt_enable()
 *       function which only enables the n_irq interrupt event at an interrupt
 *       controller. The interrupt can be dispatched to some execution context
 *       (for example, a thread) that can be also enabled or disabled.
 */
int
adapter_interrupt_context_enable(
        const int n_irq,
        const unsigned int flags);

int
adapter_interrupt_clear(
        const int n_irq,
        const unsigned int flags);

int
adapter_interrupt_clear_and_enable(
        const int n_irq,
        const unsigned int flags);

int
adapter_interrupt_disable(
        const int n_irq,
        const unsigned int flags);

/*
 * This function disables execution context (allows it to run) that services
 * the n_irq interrupt.
 *
 * Note: this function is different from the adapter_interrupt_disable()
 *       function which only disables the n_irq interrupt event at an interrupt
 *       controller. The interrupt can be dispatched to some execution context
 *       (for example, a thread) that can be also enabled or disabled.
 */
int
adapter_interrupt_context_disable(
        const int n_irq,
        const unsigned int flags);

int
adapter_interrupts_resume(void);


// Host hardware specific extensions
#include "adapter_interrupts_ext.h"


#endif /* Include Guard */


/* end of file adapter_interrupts.h */
