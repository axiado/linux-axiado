/* adapter_int_shdevxs.c
 *
 * Adapter Shared device Access module responsible for interrupts.
 * Implementation depends on the Kernel Support Driver.
 *
 */

/*****************************************************************************
* Copyright (c) 2012-2022 by Rambus, Inc. and/or its subsidiaries.
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

#include "adapter_interrupts.h"


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Driver Framework Basic Definitions API
#include "basic_defs.h"            // bool, IDENTIFIER_NOT_USED

// Kernel support driver
#include "shdevxs_irq.h"


/*----------------------------------------------------------------------------
 * Definitions and macros
 */


/*----------------------------------------------------------------------------
 * Local variables
 */


/*----------------------------------------------------------------------------
 * adapter_interrupt_enable
 */
int
adapter_interrupt_enable(
        const int n_irq,
        const unsigned int flags)
{
    return SHDevXS_IRQ_Enable(n_irq, flags);
}


/*----------------------------------------------------------------------------
 * adapter_interrupt_disable
 *
 */
int
adapter_interrupt_disable(
        const int n_irq,
        const unsigned int flags)
{
    return SHDevXS_IRQ_Disable(n_irq, flags);
}


/*----------------------------------------------------------------------------
 * adapter_interrupt_set_handler
 */
int
adapter_interrupt_set_handler(
        const int n_irq,
        adapter_interrupt_handler_t handler_function)
{
    return SHDevXS_IRQ_SetHandler(n_irq, handler_function);
}


/*----------------------------------------------------------------------------
 * adapter_interrupts_init
 */
int
adapter_interrupts_init(
        const int n_irq)
{
    int res = SHDevXS_IRQ_Init();

    IDENTIFIER_NOT_USED(n_irq);

    return res;
}


/*----------------------------------------------------------------------------
 * adapter_interrupts_uninit
 */
int
adapter_interrupts_uninit(
        const int n_irq)
{
    IDENTIFIER_NOT_USED(n_irq);
    return SHDevXS_IRQ_UnInit();
}


/*----------------------------------------------------------------------------
 * adapter_interrupts_resume
 */
int
adapter_interrupts_resume(void)
{
    // Resume aic devices is done in SHDevXS IRQ module
    return 0; // success
}


/* end of file adapter_int_shdevxs.c */
