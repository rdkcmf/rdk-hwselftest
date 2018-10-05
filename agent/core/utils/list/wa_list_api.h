/*
 * If not stated otherwise in this file or this component's Licenses.txt file the
 * following copyright and licenses apply:
 *
 * Copyright 2017 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

/**
 * @file wa_list_api.h
 */

#ifndef WA_UTILS_LIST_API_H
#define WA_UTILS_LIST_API_H

/** @defgroup WA_UTILS_LIST
 *  @{
 */

/*****************************************************************************
 * STANDARD INCLUDE FILES
 *****************************************************************************/

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/
#include "wa_list.h"

#ifdef __cplusplus
extern "C"
{
#endif

/*****************************************************************************
 * EXPORTED DEFINITIONS
 *****************************************************************************/

/** Value for iterator if points at non existing entry.
 * Consideration: Currently the maximum list capacity is hardcoded as an array in list.h.
 */
#define WA_UTILS_LIST_NO_ELEM WA_UTILS_LIST_NO_ENTRY

/*****************************************************************************
 * EXPORTED TYPES
 *****************************************************************************/
WA_UTILS_LIST_DECLARE_LIST_TYPE(void);

/* list head used if static declaration is needed */
typedef WA_UTILS_LIST(void) WA_UTILS_LIST_t;

/*****************************************************************************
 * EXPORTED VARIABLES
 *****************************************************************************/

/*****************************************************************************
 * EXPORTED FUNCTIONS
 *****************************************************************************/

/**
 * @brief Purge list
 *
 * @param pList valid pointer to the existing list
 *
 * @note This API will not \c free() list entries
 */
void WA_UTILS_LIST_Purge(void *pList);

/**
 * @brief Purge list
 *
 * @param pList valid pointer to the existing list
 *
 * @note This API will \c free() all list entries first
 */
void WA_UTILS_LIST_AllocPurge(void *pList);

/**
 * @brief Adds given pointer to the back of the list
 *
 * @param pList pointer to the existing list
 * @param pElem pointer to the preallocated element
 *
 * @return pointer to the added element or NULL on error
 */
void *WA_UTILS_LIST_AddBack(void *pList, void *pElem);

/**
 * @brief Allocate list element and add to the back
 *
 * @param pList pointer to the existing list
 * @param elemSize size of the list element to be allocated
 *
 * @return pointer to the allocated and added element or NULL on error
 */
void *WA_UTILS_LIST_AllocAddBack(void *pList, size_t elemSize);

/**
 * @brief Adds given pointer before iterator.
 * If iterator == \c WA_UTILS_LIST_NO_ELEM add at the end of the list.
 *
 * @param pList pointer to the existing list
 * @param interator an iterator
 * @param pElem pointer to the preallocated element
 *
 * @return pointer to the added element or NULL on error
 */
void *WA_UTILS_LIST_InsertBefore(void *pList, void *iterator, void *pElem);

/**
 * @brief Allocate list element and add before iterator.
 * If iterator == \c WA_UTILS_LIST_NO_ELEM add at the end of the list.
 *
 * @param pList pointer to the existing list
 * @param interator an iterator
 * @param elemSize size of the list element to be allocated
 *
 * @return pointer to the allocated and added element or NULL on error
 */
void *WA_UTILS_LIST_AllocInsertBefore(void *pList, void *iterator, size_t elemSize);

/**
 * @brief Find and remove given list element if exists.
 *
 * @param pList pointer to the existing list
 * @param pElem pointer to the preallocated element
 *
 * @note This API will not \c free() the entry
 */
void WA_UTILS_LIST_Remove(void *pList, void *pElem);

/**
 * @brief Find and remove and deallocate list element if exists.
 *
 * @param pList pointer to the existing list
 * @param elem element to find and remove
 *
 * @note Only the \c elem is deallocated, any internal sub-allocations must be removed before
 */
void WA_UTILS_LIST_AllocRemove(void *pList, void *elem);

/**
 * @brief Returns list front iterator.
 *
 * @param pList pointer to the existing list
 *
 * @return an iterator
 */
void *WA_UTILS_LIST_FrontIterator(void *pList);

/**
 * @brief Returns next adjacent elament iterator.
 *
 * @param pList pointer to the existing list
 * @param interator an iterator
 *
 * @return an iterator
 */
void *WA_UTILS_LIST_NextIterator(void *pList, void *iterator);

/**
 * @brief Returns previous adjacent elament iterator.
 *
 * @param pList pointer to the existing list
 * @param interator an iterator
 *
 * @return an iterator
 */
void *WA_UTILS_LIST_PrevIterator(void *pList, void *iterator);

/**
 * @brief Replace data pointer at iterator
 *
 * @param pList pointer to the existing list
 * @param interator an iterator
 * @param pElem pointer to the preallocated element
 *
 */

void WA_UTILS_LIST_ReplaceAtIterator(void *pList, void *iterator, void *pElem);

/**
 * @brief Returns next adjacent elament iterator.
 *
 * @param pList pointer to the existing list
 * @param interator an iterator
 *
 * @return pointer to the data element
 */
void *WA_UTILS_LIST_DataAtIterator(void *pList, void *iterator);

/**
 * @brief Return number fo list elements
 *
 * @param pList pointer to the list
 *
 * @return number of elements on the list
 */
unsigned int WA_UTILS_LIST_ElemCount(void *pList);

#ifdef __cplusplus
}
#endif

/* End of doxygen group */
/*! @} */

#endif                                           /* WA_UTILS_LIST_API_H */
