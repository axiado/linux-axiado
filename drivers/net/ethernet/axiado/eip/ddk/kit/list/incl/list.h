/* list.h
 *
 * The list API for manipulation of a list of elements.
 *
 * This API can be used to implement lists of variable size.
 * The list elements must be allocated statically or dynamically by the API
 * user.
 *
 * The list instances can be allocated statically or dynamically.
 *
 * Note: the support for static list instances will be deprecated
 *       in future versions. Do not use it anymore!
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

#ifndef LIST_H_
#define LIST_H_


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */


/*----------------------------------------------------------------------------
 * Definitions and macros
 */

// Dummy list ID to use with the list API
#define LIST_DUMMY_LIST_ID          0

#define LIST_INTERNAL_DATA_SIZE     2

/*----------------------------------------------------------------------------
 * list_status_t
 *
 * Return values for all the API functions.
 */
typedef enum
{
    LIST_STATUS_OK,
    LIST_ERROR_BAD_ARGUMENT,
    LIST_ERROR_INTERNAL
} list_status_t;


/*----------------------------------------------------------------------------
 * list_element_t
 *
 * The element data structure.
 *
 */
typedef struct
{
    // Pointer to a data object associated with this element,
    // can be filled in by the API user
    void * data_object_p;

    // data used internally by the API implementation only,
    // may not be written by the API user
    void * internal[LIST_INTERNAL_DATA_SIZE];

} list_element_t;


/*----------------------------------------------------------------------------
 * list_init
 *
 * Initializes a list instance. The list is empty when this function
 * returns. The List_Add() function must be used to populate the list
 * instance with the elements.
 *
 * list_id (input)
 *     list instance identifier to be initialized.
 *     Ignored if list_instance_p is not NULL.
 *
 * list_instance_p (input)
 *     Pointer to the list instance to be initialized.
 *     If the list_id is used then list_instance_p can be set to NULL.
 *
 * This function is re-entrant for different list instances.
 * This function is not re-entrant for the same list instance.
 *
 * This function must be called before any other function
 * for the same list instance.
 *
 * Return Values
 *     LIST_STATUS_OK: Success, handle_p was written.
 *     LIST_ERROR_BAD_ARGUMENT: Invalid input parameter.
 */
list_status_t
list_init(
        const unsigned int list_id,
        void * const list_instance_p);


/*----------------------------------------------------------------------------
 * list_uninit
 *
 * Uninitializes the requested list instance. All the resources
 * associated with this instance will be freed before this function returns.
 *
 * list_id (input)
 *     list instance identifier to be initialized.
 *     Ignored if list_instance_p is not NULL.
 *
 * list_instance_p (input)
 *     Pointer to the list instance to be initialized.
 *     If the list_id is used then list_instance_p can be set to NULL.
 *
 * This function is re-entrant for different list instances.
 * This function is not re-entrant for the same list instance.
 *
 * This function must be called in the end when the list instance is not needed
 * anymore.
 *
 * Return Values
 *     LIST_STATUS_OK: Success, instance is uninitialized.
 *     LIST_ERROR_BAD_ARGUMENT: Invalid input parameter.
 */
list_status_t
list_uninit(
        const unsigned int list_id,
        void * const list_instance_p);


/*----------------------------------------------------------------------------
 * list_add_to_head
 *
 * Adds an element to the list head. The element will be added to
 * the list instance when this function returns LIST_STATUS_OK.
 *
 * list_id (input)
 *     list instance identifier to be initialized.
 *     Ignored if list_instance_p is not NULL.
 *
 * list_instance_p (input)
 *     Pointer to the list instance to be initialized.
 *     If the list_id is used then list_instance_p can be set to NULL.
 *
 * element_p (input)
 *     Pointer to the element to be added to the list instance. This element
 *     may not be already present in the list. Cannot be NULL.
 *
 * This function is re-entrant for different list instances.
 * This function is not re-entrant for the same list instance.
 *
 * Return Values
 *     LIST_STATUS_OK: Success.
 *     LIST_ERROR_BAD_ARGUMENT: Invalid input parameter.
 */
list_status_t
list_add_to_head(
        const unsigned int list_id,
        void * const list_instance_p,
        list_element_t * const element_p);


/*----------------------------------------------------------------------------
 * list_remove_from_tail
 *
 * Removes an element from the list tail. The element will be removed from
 * the list instance when this function returns LIST_STATUS_OK.
 *
 * list_id (input)
 *     list instance identifier to be initialized.
 *     Ignored if list_instance_p is not NULL.
 *
 * list_instance_p (input)
 *     Pointer to the list instance to be initialized.
 *     If the list_id is used then list_instance_p can be set to NULL.
 *
 * element_pp (output)
 *     Pointer to the memory location where the element from the list
 *     instance will be stored.
 *
 * This function is re-entrant for different list instances.
 * This function is not re-entrant for the same list instance.
 *
 * Return Values
 *     LIST_STATUS_OK: Success, element_pp was written.
 *     LIST_ERROR_BAD_ARGUMENT: Invalid input parameter.
 */
list_status_t
list_remove_from_tail(
        const unsigned int list_id,
        void * const list_instance_p,
        list_element_t ** const element_pp);


/*----------------------------------------------------------------------------
 * list_remove_anywhere
 *
 * Removes requested element from the list. The element will be removed from
 * the list instance when this function returns LIST_STATUS_OK.
 *
 * list_id (input)
 *     list instance identifier to be initialized.
 *     Ignored if list_instance_p is not NULL.
 *
 * list_instance_p (input)
 *     Pointer to the list instance to be initialized.
 *     If the list_id is used then list_instance_p can be set to NULL.
 *
 * element_p (input/output)
 *     Pointer to the memory location where the element to be removed from
 *     the list instance is stored.
 *
 * This function is re-entrant for different list instances.
 * This function is not re-entrant for the same list instance.
 *
 * Return Values
 *     LIST_STATUS_OK: Success, element_pp was written.
 *     LIST_ERROR_BAD_ARGUMENT: Invalid input parameter.
 */
list_status_t
list_remove_anywhere(
        const unsigned int list_id,
        void * const list_instance_p,
        list_element_t * const element_p);


/*----------------------------------------------------------------------------
 * list_get_list_element_count
 *
 * Gets the number of elements added to the list.
 *
 * list_id (input)
 *     list instance identifier to be initialized.
 *     Ignored if list_instance_p is not NULL.
 *
 * list_instance_p (input)
 *     Pointer to the list instance to be initialized.
 *     If the list_id is used then list_instance_p can be set to NULL.
 *
 * count_p (output)
 *     Pointer to the memory location where the list element count
 *     will be stored.
 *
 * This function is re-entrant for different list instances.
 * This function is not re-entrant for the same list instance.
 *
 * Return Values
 *     LIST_STATUS_OK: Success, count_p was written.
 *     LIST_ERROR_BAD_ARGUMENT: Invalid input parameter.
 */
list_status_t
list_get_list_element_count(
        const unsigned int list_id,
        void * const list_instance_p,
        unsigned int * const count_p);


/*----------------------------------------------------------------------------
 * list_get_head
 *
 * Gets the list head.
 *
 * list_id (input)
 *     list instance identifier to be initialized.
 *     Ignored if list_instance_p is not NULL.
 *
 * list_instance_p (input)
 *     Pointer to the list instance to be initialized.
 *     If the list_id is used then list_instance_p can be set to NULL.
 *
 * element_pp (output)
 *     Pointer to the memory location where the element from the list
 *     instance will be stored.
 *
 * This function is re-entrant for different list instances.
 * This function is not re-entrant for the same list instance.
 *
 * Return Values
 *     LIST_STATUS_OK: Success, handle_p was written.
 *     LIST_ERROR_BAD_ARGUMENT: Invalid input parameter.
 */
list_status_t
list_get_head(
        const unsigned int list_id,
        void * const list_instance_p,
        const list_element_t ** const element_pp);


/*----------------------------------------------------------------------------
 * list_remove_from_head
 *
 * Removes an element from the list head. The element will be removed from
 * the list instance when this function returns LIST_STATUS_OK.
 *
 * list_id (input)
 *     list instance identifier to be initialized.
 *     Ignored if list_instance_p is not NULL.
 *
 * list_instance_p (input)
 *     Pointer to the list instance to be initialized.
 *     If the list_id is used then list_instance_p can be set to NULL.
 *
 * element_pp (output)
 *     Pointer to the memory location where the element from the list
 *     instance will be stored.
 *
 * This function is re-entrant for different list instances.
 * This function is not re-entrant for the same list instance.
 *
 * Return Values
 *     LIST_STATUS_OK: Success, handle_p was written.
 *     LIST_ERROR_BAD_ARGUMENT: Invalid input parameter.
 */
list_status_t
list_remove_from_head(
        const unsigned int list_id,
        void * const list_instance_p,
        list_element_t ** const element_pp);


/*----------------------------------------------------------------------------
 * list_get_next_element
 *
 * Gets the next element for the provided one.
 *
 * element_p (input)
 *     Pointer to the element for which the next element must be obtained.
 *     Cannot be NULL.
 *
 * This function is re-entrant.
 *
 * Return Values
 *     Pointer to the next element or NULL if not found.
 */
static inline list_element_t *
list_get_next_element(
        const list_element_t * const element_p)
{
    // Get the next element from the list
    return element_p->internal[1];
}



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
list_get_instance_byte_count(void);


#endif /* LIST_H_ */


/* end of file list.h */
