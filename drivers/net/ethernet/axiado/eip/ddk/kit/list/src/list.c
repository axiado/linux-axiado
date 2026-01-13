/* list.c
 *
 * This Module implements the list API
 */

/*****************************************************************************
* Copyright (c) 2012-2020 by Rambus, Inc. and/or its subsidiaries.
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

// list API
#include "list.h"


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Default configuration
#include "c_list.h"

// Driver Framework Basic Definitions API
#include "basic_defs.h"


/*----------------------------------------------------------------------------
 * Definitions and macros
 */

typedef struct
{
    // list head
    list_element_t * head_p;

    // list tail
    list_element_t * tail_p;

    // number of elements in the list
    unsigned int element_count;

} list_t;

/*----------------------------------------------------------------------------
 * Local variables
 */

// Statically allocated list instances. This will be deprecated in future.
static list_t list [LIST_MAX_NOF_INSTANCES];


/*----------------------------------------------------------------------------
 * list_init
 *
 */
list_status_t
list_init(
        const unsigned int list_id,
        void * const list_instance_p)
{
#ifdef LIST_STRICT_ARGS
    if (list_id >= LIST_MAX_NOF_INSTANCES)
        return LIST_ERROR_BAD_ARGUMENT;
#endif // LIST_STRICT_ARGS

    // Initialize the list instance
    {
        list_t * list_p;

        if (list_instance_p)
            list_p = (list_t*)list_instance_p;
        else
            list_p = &list[list_id];

        list_p->element_count    = 0;
        list_p->head_p          = NULL;
        list_p->tail_p          = NULL;
    }

    return LIST_STATUS_OK;
}


/*----------------------------------------------------------------------------
 * list_uninit
 *
 */
list_status_t
list_uninit(
        const unsigned int list_id,
        void * const list_instance_p)
{
#ifdef LIST_STRICT_ARGS
    if (list_id >= LIST_MAX_NOF_INSTANCES)
        return LIST_ERROR_BAD_ARGUMENT;
#endif // LIST_STRICT_ARGS

    // Un-initialize the list instance
    {
        list_t * list_p;

        if (list_instance_p)
            list_p = (list_t*)list_instance_p;
        else
            list_p = &list[list_id];

        list_p->element_count    = 0;
        list_p->head_p          = NULL;
        list_p->tail_p          = NULL;
    }

    return LIST_STATUS_OK;
}


/*----------------------------------------------------------------------------
 * list_add_to_head
 *
 */
list_status_t
list_add_to_head(
        const unsigned int list_id,
        void * const list_instance_p,
        list_element_t * const element_p)
{
#ifdef LIST_STRICT_ARGS
    if (list_id >= LIST_MAX_NOF_INSTANCES)
        return LIST_ERROR_BAD_ARGUMENT;

    if (element_p == NULL)
        return LIST_ERROR_BAD_ARGUMENT;
#endif // LIST_STRICT_ARGS

    // Add the element at the list head
    {
        list_element_t * temp_element_p;
        list_t * list_p;

        if (list_instance_p)
            list_p = (list_t*)list_instance_p;
        else
            list_p = &list[list_id];

        temp_element_p           = list_p->head_p;
        list_p->head_p          = element_p;

        // Previous element in the list, this is a head
        element_p->internal[0]  = NULL;

        // Next element in the list
        element_p->internal[1]  = temp_element_p;

        // Check if this is the first element
        if (list_p->element_count == 0)
            list_p->tail_p = list_p->head_p;
        else
            // Link the old head to the new head
            temp_element_p->internal[0] = element_p;

        list_p->element_count++;
    }

    return LIST_STATUS_OK;
}


/*----------------------------------------------------------------------------
 * list_remove_from_tail
 *
 */
list_status_t
list_remove_from_tail(
        const unsigned int list_id,
        void * const list_instance_p,
        list_element_t ** const element_pp)
{
#ifdef LIST_STRICT_ARGS
    if (list_id >= LIST_MAX_NOF_INSTANCES)
        return LIST_ERROR_BAD_ARGUMENT;

    if (element_pp == NULL)
        return LIST_ERROR_BAD_ARGUMENT;

#endif // LIST_STRICT_ARGS

    // Remove the element from the list tail
    {
        list_element_t * temp_element_p;
        list_t * list_p;

        if (list_instance_p)
            list_p = (list_t*)list_instance_p;
        else
            list_p = &list[list_id];

#ifdef LIST_STRICT_ARGS
        if (list_p->element_count == 0)
            return LIST_ERROR_BAD_ARGUMENT;
#endif // LIST_STRICT_ARGS

        // Get the previous for the tail element in the list
        temp_element_p = (list_element_t*)list_p->tail_p->internal[0];

        list_p->tail_p->internal[0] = NULL;
        list_p->tail_p->internal[1] = NULL;
        *element_pp                 = list_p->tail_p;

        // Set the new tail
        list_p->tail_p              = temp_element_p;

        // Check if this is the last element
        if (list_p->element_count == 1)
            list_p->head_p = NULL;
        else
            // New tail must have no next element
            list_p->tail_p->internal[1] = NULL;

        list_p->element_count--;
    }

    return LIST_STATUS_OK;
}


/*----------------------------------------------------------------------------
 * list_remove_anywhere
 */
list_status_t
list_remove_anywhere(
        const unsigned int list_id,
        void * const list_instance_p,
        list_element_t * const element_p)
{
#ifdef LIST_STRICT_ARGS
    if (list_id >= LIST_MAX_NOF_INSTANCES)
        return LIST_ERROR_BAD_ARGUMENT;

    if (element_p == NULL)
        return LIST_ERROR_BAD_ARGUMENT;
#endif // LIST_STRICT_ARGS

    // Remove the element from the list tail
    {
        list_element_t * prev_element_p, * next_element_p;
        list_t * list_p;

        if (list_instance_p)
            list_p = (list_t*)list_instance_p;
        else
            list_p = &list[list_id];

#ifdef LIST_STRICT_ARGS
        if (list_p->element_count == 0)
            return LIST_ERROR_BAD_ARGUMENT;

        // Check element belongs to this list
        {
            unsigned int i;
            list_element_t * temp_element_p = list_p->head_p;

            for (i = 0; i < list_p->element_count; i++)
            {
                list_element_t * p;

                if (temp_element_p == element_p)
                    break; // Found

                p = temp_element_p->internal[1];
                if (p)
                    temp_element_p = p; // not end of list yet
                else
                    return LIST_ERROR_BAD_ARGUMENT; // Not found
            }

            if (temp_element_p != element_p)
                return LIST_ERROR_BAD_ARGUMENT; // Not found
        }
#endif // LIST_STRICT_ARGS

        prev_element_p = element_p->internal[0];
        next_element_p = element_p->internal[1];

        element_p->internal[0] = NULL;
        element_p->internal[1] = NULL;

        if (prev_element_p)
            prev_element_p->internal[1] = next_element_p;
        else
            list_p->head_p = next_element_p;

        if (next_element_p)
            next_element_p->internal[0] = prev_element_p;
        else
            list_p->tail_p = prev_element_p;

        list_p->element_count--;
    }

    return LIST_STATUS_OK;
}


/*----------------------------------------------------------------------------
 * list_get_list_element_count
 *
 */
list_status_t
list_get_list_element_count(
        const unsigned int list_id,
        void * const list_instance_p,
        unsigned int * const count_p)
{
#ifdef LIST_STRICT_ARGS
    if (list_id >= LIST_MAX_NOF_INSTANCES)
        return LIST_ERROR_BAD_ARGUMENT;

    if (count_p == NULL)
        return LIST_ERROR_BAD_ARGUMENT;
#endif // LIST_STRICT_ARGS

    {
        list_t * list_p;

        if (list_instance_p)
            list_p = (list_t*)list_instance_p;
        else
            list_p = &list[list_id];

        *count_p = list_p->element_count;
    }

    return LIST_STATUS_OK;
}


#ifdef LIST_FULL_API
/*----------------------------------------------------------------------------
 * list_remove_from_head
 *
 */
list_status_t
list_remove_from_head(
        const unsigned int list_id,
        void * const list_instance_p,
        list_element_t ** const element_pp)
{
    list_t * list_p;

    if (list_instance_p)
        list_p = (list_t*)list_instance_p;
    else
        list_p = &list[list_id];

#ifdef LIST_STRICT_ARGS
    if (list_id >= LIST_MAX_NOF_INSTANCES)
        return LIST_ERROR_BAD_ARGUMENT;

    if (element_pp == NULL)
        return LIST_ERROR_BAD_ARGUMENT;

    if (list_p->element_count == 0)
        return LIST_ERROR_BAD_ARGUMENT;
#endif // LIST_STRICT_ARGS

    // Remove the element from the list head
    {
        list_element_t * temp_element_p;

        // Get the next for the head element in the list
        temp_element_p = (list_element_t*)list_p->head_p->internal[1];

        list_p->head_p->internal[0] = NULL;
        list_p->head_p->internal[1] = NULL;
        *element_pp                 = list_p->head_p;

        list_p->head_p              = temp_element_p;

        // Check if this is the last element
        if (list_p->element_count == 1)
            list_p->tail_p = NULL;
        else
            list_p->head_p->internal[0] = NULL;

        list_p->element_count--;
    }

    return LIST_STATUS_OK;
}


/*----------------------------------------------------------------------------
 * list_get_head
 */
list_status_t
list_get_head(
        const unsigned int list_id,
        void * const list_instance_p,
        const list_element_t ** const element_pp)
{
#ifdef LIST_STRICT_ARGS
    if (list_id >= LIST_MAX_NOF_INSTANCES)
        return LIST_ERROR_BAD_ARGUMENT;

    if (element_pp == NULL)
        return LIST_ERROR_BAD_ARGUMENT;
#endif // LIST_STRICT_ARGS

    // Get the list head
    {
        list_t * list_p;

        if (list_instance_p)
            list_p = (list_t*)list_instance_p;
        else
            list_p = &list[list_id];

        *element_pp = list_p->head_p;
    }

    return LIST_STATUS_OK;
}
#endif // LIST_FULL_API


/*----------------------------------------------------------------------------
 * list_get_instance_byte_count
 *
 * Gets the memory size of the list instance (in bytes) excluding the list
 * elements memory size. This list memory size can be used to allocate a list
 * instance and pass a pointer to it subsequently to the List_*() functions.
 *
 * This function is re-entrant and can be called any time.
 *
 * Return Values
 *     size of the list administration memory in bytes.
 */
unsigned int
list_get_instance_byte_count(void)
{
    return sizeof(list_t);
}


/* end of file list.c */
