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
 * @file wa_list_api.c
 *
 * @brief This is the LIST api
 */

/** @defgroup  WA_UTILS_LIST_API LIST public API
 *  @ingroup LIST
 *  @{
 */

/*****************************************************************************
 * STANDARD INCLUDE FILES
 *****************************************************************************/
#include <stdint.h>
#include <stdlib.h>

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/
#include "wa_list_api.h"

/*****************************************************************************
 * LOCAL FUNCTION PROTOTYPES
 *****************************************************************************/

/*****************************************************************************
 * GLOBAL VARIABLE DEFINITIONS
 *****************************************************************************/

/*****************************************************************************
 * LOCAL DEFINITIONS
 *****************************************************************************/
#define WA_UTILS_LIST_FROM_POINTER(p) (*(WA_UTILS_LIST(void) *)(p))
/*****************************************************************************
 * LOCAL TYPES
 *****************************************************************************/

/*****************************************************************************
 * LOCAL VARIABLE DECLARATIONS
 *****************************************************************************/

/*****************************************************************************
 * FUNCTION DEFINITIONS
 *****************************************************************************/

/**
 */
void WA_UTILS_LIST_Purge(void *pList)
{
    WA_UTILS_LIST_ClearList(WA_UTILS_LIST_FROM_POINTER(pList));
}

/**
 */
void WA_UTILS_LIST_AllocPurge(void *pList)
{
    WA_UTILS_LIST_ELM(void) *pListIterator;
    void *p;

    for(pListIterator = WA_UTILS_LIST_FrontListIterator(WA_UTILS_LIST_FROM_POINTER(pList));
            pListIterator != WA_UTILS_LIST_NO_ENTRY;
            pListIterator = WA_UTILS_LIST_NextElem(pListIterator))
    {
        p = WA_UTILS_LIST_Pdata(pListIterator);
        if(p != NULL)
        {
            free(p);
        }
    }
    WA_UTILS_LIST_ClearList(WA_UTILS_LIST_FROM_POINTER(pList));
}

/**
 */
void *WA_UTILS_LIST_AddBack(void *pList, void *pElem)
{
    unsigned int i;

    i = WA_UTILS_LIST_GetListCount(WA_UTILS_LIST_FROM_POINTER(pList));
    WA_UTILS_LIST_AddListBack(WA_UTILS_LIST_FROM_POINTER(pList), pElem);
    if(WA_UTILS_LIST_GetListCount(WA_UTILS_LIST_FROM_POINTER(pList)) == i)
    {
        pElem = NULL;
    }
    return pElem;
}

/**
 */
void *WA_UTILS_LIST_AllocAddBack(void *pList, size_t elemSize)
{
    unsigned int i;
    void *p;

    p = malloc(elemSize); /* Consideration: use memalign() if needed */
    if(p != NULL)
    {
        i = WA_UTILS_LIST_GetListCount(WA_UTILS_LIST_FROM_POINTER(pList));
        WA_UTILS_LIST_AddListBack(WA_UTILS_LIST_FROM_POINTER(pList), p);
        if(WA_UTILS_LIST_GetListCount(WA_UTILS_LIST_FROM_POINTER(pList)) == i)
        {
            free(p);
            p = NULL;
        }
    }
    return p;
}

/**
 */
void *WA_UTILS_LIST_InsertBefore(void *pList, void *iterator, void *pElem)
{
    unsigned int i;

    i = WA_UTILS_LIST_GetListCount(WA_UTILS_LIST_FROM_POINTER(pList));
    if(iterator != WA_UTILS_LIST_NO_ENTRY)
    {
        WA_UTILS_LIST_InsertListFront(WA_UTILS_LIST_FROM_POINTER(pList),
                (WA_UTILS_LIST_ELM(void) *)iterator, pElem);
    }
    else
    {
        WA_UTILS_LIST_AddListBack(WA_UTILS_LIST_FROM_POINTER(pList), pElem);
    }
    if(WA_UTILS_LIST_GetListCount(WA_UTILS_LIST_FROM_POINTER(pList)) == i)
    {
        pElem = NULL;
    }
    return pElem;
}

/**
 */
void *WA_UTILS_LIST_AllocInsertBefore(void *pList, void *iterator, size_t elemSize)
{
    unsigned int i;
    void *p;

    p = malloc(elemSize); /* Consideration: use memalign() if needed */
    if(p != NULL)
    {
        i = WA_UTILS_LIST_GetListCount(WA_UTILS_LIST_FROM_POINTER(pList));
        if(iterator != WA_UTILS_LIST_NO_ENTRY)
        {
            WA_UTILS_LIST_InsertListFront(WA_UTILS_LIST_FROM_POINTER(pList),
                    (WA_UTILS_LIST_ELM(void) *)iterator, p);
        }
        else
        {
            WA_UTILS_LIST_AddListBack(WA_UTILS_LIST_FROM_POINTER(pList), p);
        }
        if(WA_UTILS_LIST_GetListCount(WA_UTILS_LIST_FROM_POINTER(pList)) == i)
        {
            free(p);
            p = NULL;
        }
    }
    return p;
}

/**
 */
void WA_UTILS_LIST_Remove(void *pList, void *pElem)
{
    WA_UTILS_LIST_RemoveList(WA_UTILS_LIST_FROM_POINTER(pList), pElem);
}

/**
 */
void WA_UTILS_LIST_AllocRemove(void *pList, void *elem)
{
    WA_UTILS_LIST_ELM(void) *pListIterator;
    void *p;

    for(pListIterator = WA_UTILS_LIST_FrontListIterator(WA_UTILS_LIST_FROM_POINTER(pList));
            pListIterator != WA_UTILS_LIST_NO_ENTRY;
            pListIterator = WA_UTILS_LIST_NextElem(pListIterator))
    {
        p = WA_UTILS_LIST_Pdata(pListIterator);
        if(p == elem)
        {
            if(p != NULL)
            {
                free(p);
            }
            WA_UTILS_LIST_RemoveList(WA_UTILS_LIST_FROM_POINTER(pList), p);
            break;
        }
    }
}

/**
 */
void *WA_UTILS_LIST_FrontIterator(void *pList)
{
    return (void *)WA_UTILS_LIST_FrontListIterator(WA_UTILS_LIST_FROM_POINTER(pList));
}

/**
 */
void *WA_UTILS_LIST_NextIterator(void *pList, void *iterator)
{
    (void)(pList); //unused
    return (void *) WA_UTILS_LIST_NextElem((WA_UTILS_LIST_ELM(void) *)iterator);
}

/**
 */
void *WA_UTILS_LIST_PrevIterator(void *pList, void *iterator)
{
    (void)(pList); //unused
    return (void *) WA_UTILS_LIST_PrevElem((WA_UTILS_LIST_ELM(void) *)iterator);
}

/**
 */
void WA_UTILS_LIST_ReplaceAtIterator(void *pList, void *iterator, void *pElem)
{
    if(iterator)
    {
        ((WA_UTILS_LIST_ELM(void) *)iterator)->pData = pElem;
    }
}

/**
 */
void *WA_UTILS_LIST_DataAtIterator(void *pList, void *iterator)
{
    (void)(pList); //unused
    return (void *) WA_UTILS_LIST_Pdata((WA_UTILS_LIST_ELM(void) *)iterator);
}

/**
 */
unsigned int WA_UTILS_LIST_ElemCount(void *pList)
{
    return WA_UTILS_LIST_GetListCount(WA_UTILS_LIST_FROM_POINTER(pList));
}

/*****************************************************************************
 * LOCAL FUNCTIONS
 *****************************************************************************/

/* End of doxygen group */
/*! @} */

