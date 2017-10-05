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

#ifndef WA_UTILS_LIST_H
#define WA_UTILS_LIST_H
/*****************************************************************************
 * wa_list.h
 *
 * Implements doubly linked Lists in macros. The List operations are:
 *
 *   WA_UTILS_LIST_DECLARE_LIST_TYPE(t);       declare a type alias for a list of type t
 *   WA_UTILS_LIST(t) l;                       declare l as a list of type t
 *   WA_UTILS_LIST_ELM(t) e;                   declare e as an element of a list of type t
 *   WA_UTILS_LIST_ClearList(l);               initialize list to empty
 *   WA_UTILS_LIST_AddListBack(l, pItem);      add an item to back of list
 *   WA_UTILS_LIST_AddListFront(l, pItem);     add an item to front of list
 *   pItem = WA_UTILS_LIST_PeekListFront(l);   peek at front list item
 *   WA_UTILS_LIST_DiscardListFront(l);        discard front list item
 *   WA_UTILS_LIST_DeListFront(l, &pItem);     get (& remove from list) front list item
 *   pItem = WA_UTILS_LIST_PeekListBack(l);    peek at back list item
 *   WA_UTILS_LIST_DiscardListBack(l);         discard back list item
 *   WA_UTILS_LIST_DeListBack(l, &pItem);      get (& remove from list) back list item
 *   WA_UTILS_LIST_RemoveList(l, pItem);       search and remove an item from the list
 *   n = WA_UTILS_LIST_GetListCount(l);        get number of elements in the list
 *   pElem = WA_UTILS_LIST_NextElem(pElem);    get next element
 *   pElem = WA_UTILS_LIST_PrevElem(pElem);    get previous element
 *   pItem = WA_UTILS_LIST_Pdata(pElem);       get pointer to data item
 *
 * (pItem == WA_UTILS_LIST_NO_ENTRY) after WA_UTILS_LIST_PeekListFront(),
 * WA_UTILS_LIST_PeekListBack(), WA_UTILS_LIST_DeListFront()
 * or WA_UTILS_LIST_DeListBack() implies the list was empty.
 *
 * See comments before each macro definition for a more complete
 * description of each one.
 *
 * Example:
 *
 *   typedef struct Foo_tag Foo_t;
 *
 *   WA_UTILS_LIST_DECLARE_LIST_TYPE(Foo_t);
 *
 *   struct Foo_tag
 *   {
 *       UInt32 uiFoo;
 *   };
 *
 *   LIST(Foo_t) lFoo;
 *   LIST(Foo_t) lFooBar;
 *   Foo_t aFooArray[20];
 *
 *   void FooBar(void)
 *   {
 *       UInt32 i;
 *       Foo_t *pThisFoo;
 *
 *       WA_UTILS_LIST_ClearList(lFoo);
 *       WA_UTILS_LIST_ClearList(lFooBar);
 *       for (i = 0; i < 20; i++)
 *       {
 *           aFooArray[i].uiFoo = i;
 *           WA_UTILS_LIST_AddListBack(lFoo, &(aFooArray[i]));
 *       }
 *
 *       while (1)
 *       {
 *           pThisFoo = WA_UTILS_LIST_PeekListFront(lFoo);
 *           if (pThisFoo == WA_UTILS_LIST_NO_ENTRY)
 *           {
 *               break;
 *           }
 *           WA_UTILS_LIST_DiscardListFront(lFoo);
 *           pThisFoo->uiFoo += 100;
 *           WA_UTILS_LIST_AddListBack(lFooBar, pThisFoo);
 *       }
 *   }
 *
 *
 */
#include "wa_list_private.h"

#define WA_UTILS_LIST_NO_ENTRY ((void *) 0)

/*****************************************************************************
 * EXPORTED DEFINITIONS
 *****************************************************************************/
#define WA_UTILS_LIST(t) list_##t
#define WA_UTILS_LIST_ELM(t) list_elm_##t

#define WA_UTILS_LIST_DECLARE_LIST_TYPE(t)              \
typedef struct tag_list_elm_##t                         \
{                                                       \
    int inUse;                                          \
    t *pData;                                           \
    struct tag_list_elm_##t *pNext;                     \
    struct tag_list_elm_##t *pPrev;                     \
} WA_UTILS_LIST_ELM(t);                                 \
                                                        \
typedef struct tag_list_##t                             \
{                                                       \
    list_elm_##t *pFront;                               \
    list_elm_##t *pBack;                                \
    list_elm_##t *pTemp;                                \
    unsigned numElements;                               \
    list_elm_##t elmPool[WA_UTILS_LIST_MAX_LIST_SIZE];  \
} WA_UTILS_LIST(t)

/*****************************************************************************
 *
 * MACRO:      WA_UTILS_LIST_ClearList
 *
 * PURPOSE:    Clear a List, any contents of the list are lost
 *
 * PARAMS:
 *
 *    Identifier:    l
 *    Description:   A structure of type LIST
 *
 ******************************************************************************/
#define WA_UTILS_LIST_ClearList(l)    (memset(&(l), 0, sizeof((l))))

/*****************************************************************************
 *
 * MACRO:      WA_UTILS_LIST_AddListBack
 *
 * PURPOSE:    Add to the back of a List
 *
 * PARAMS:
 *
 *    Identifier:    l
 *    Description:   A structure of type LIST
 *
 *    Identifier:    pi
 *    Description:   Pointer to item to add
 *
 ******************************************************************************/
#define WA_UTILS_LIST_AddListBack(l, pi)                        \
    {                                                           \
        if ((pi) != WA_UTILS_LIST_NO_ENTRY)                     \
        {                                                       \
            /* Allocate an element */                           \
            WA_UTILS_LIST_AllocListPool((l), (pi), (l).pTemp);  \
            if ((l).pTemp != WA_UTILS_LIST_NO_ENTRY)            \
            {                                                   \
                if ((l).pBack == WA_UTILS_LIST_NO_ENTRY)        \
                {                                               \
                    (l).pFront = (l).pTemp;                     \
                }                                               \
                else                                            \
                {                                               \
                    ((l).pBack)->pNext = (l).pTemp;             \
                }                                               \
                ((l).pTemp)->pPrev = (l).pBack;                 \
                (l).pBack = (l).pTemp;                          \
                ((l).pTemp)->pNext = WA_UTILS_LIST_NO_ENTRY;    \
                (l).numElements++;                              \
            }                                                   \
        }                                                       \
     }

/*****************************************************************************
 *
 * MACRO:      WA_UTILS_LIST_AddListFront
 *
 * PURPOSE:    Add to the front of a List
 *
 * PARAMS:
 *
 *    Identifier:    l
 *    Description:   A structure of type LIST
 *
 *    Identifier:    pi
 *    Description:   Pointer to item to add
 *
 ******************************************************************************/
#define WA_UTILS_LIST_AddListFront(l, pi)                       \
    {                                                           \
        if ((pi) != WA_UTILS_LIST_NO_ENTRY)                     \
        {                                                       \
            /* Allocate an element */                           \
            WA_UTILS_LIST_AllocListPool((l), (pi), (l).pTemp);  \
            if ((l).pTemp != WA_UTILS_LIST_NO_ENTRY)            \
            {                                                   \
                if ((l).pFront == WA_UTILS_LIST_NO_ENTRY)       \
                {                                               \
                    (l).pBack = (l).pTemp;                      \
                }                                               \
                else                                            \
                {                                               \
                    ((l).pFront)->pPrev = (l).pTemp;            \
                }                                               \
                ((l).pTemp)->pNext = (l).pFront;                \
                (l).pFront = (l).pTemp;                         \
                ((l).pTemp)->pPrev = WA_UTILS_LIST_NO_ENTRY;    \
                (l).numElements++;                              \
            }                                                   \
        }                                                       \
    }

/*****************************************************************************
 *
 * MACRO:      WA_UTILS_LIST_InsertListFront
 *
 * PURPOSE:    Add before current element of a List
 *
 * PARAMS:
 *
 *    Identifier:    l
 *    Description:   A structure of type LIST
 *    
 *    Identifier:    pe
 *    Description:   Pointer to element
 *    
 *    Identifier:    pi
 *    Description:   Pointer to item to add
 *
 ******************************************************************************/
#define WA_UTILS_LIST_InsertListFront(l, pe, pi)                \
    {                                                           \
        if ((pe) && ((pi) != WA_UTILS_LIST_NO_ENTRY))           \
        {                                                       \
            /* Allocate an element */                           \
            WA_UTILS_LIST_AllocListPool((l), (pi), (l).pTemp);  \
            if ((l).pTemp != WA_UTILS_LIST_NO_ENTRY)            \
            {                                                   \
                ((l).pTemp)->pNext = (pe);                      \
                ((l).pTemp)->pPrev = (pe)->pPrev;               \
                if ((pe)->pPrev != WA_UTILS_LIST_NO_ENTRY)      \
                {                                               \
                    (pe)->pPrev->pNext = (l).pTemp;             \
                }                                               \
                else                                            \
                {                                               \
                    (l).pFront = (l).pTemp;                     \
                }                                               \
                (pe)->pPrev = (l).pTemp;                        \
                (l).numElements++;                              \
            }                                                   \
        }                                                       \
    }

/*****************************************************************************
 *
 * MACRO:      WA_UTILS_LIST_PeekListFront
 *
 * PURPOSE:    Peek at the front item of the List
 *
 * PARAMS:
 *
 *    Identifier:    l
 *    Description:   A structure of type LIST
 *
 ******************************************************************************/
#define WA_UTILS_LIST_PeekListFront(l) ((l).pFront ? ((l).pFront)->pData : WA_UTILS_LIST_NO_ENTRY)

/*****************************************************************************
 *
 * MACRO:      WA_UTILS_LIST_FrontListIterator
 *
 * PURPOSE:    Return the front element of the list that can be used to iterate
 *             from the front of the list
 *
 * PARAMS:
 *
 *    Identifier:    l
 *    Description:   A structure of type LIST
 *
 ******************************************************************************/
#define WA_UTILS_LIST_FrontListIterator(l) ((l).pFront)

/*****************************************************************************
 *
 * MACRO:      WA_UTILS_LIST_DiscardListFront
 *
 * PURPOSE:    Discard the front item of the List, the item is
 *             lost unless a PeekListFront() is done first to get it.
 *
 * PARAMS:
 *
 *    Identifier:    l
 *    Description:   A structure of type LIST
 *
 ******************************************************************************/
#define WA_UTILS_LIST_DiscardListFront(l)                       \
    {                                                           \
        if ((l).pFront != WA_UTILS_LIST_NO_ENTRY)               \
        {                                                       \
            (l).pTemp = (l).pFront;                             \
            (l).pFront = ((l).pFront)->pNext;                   \
            if ((l).pFront == WA_UTILS_LIST_NO_ENTRY)           \
            {                                                   \
                (l).pBack = WA_UTILS_LIST_NO_ENTRY;             \
            }                                                   \
            else                                                \
            {                                                   \
                ((l).pFront)->pPrev = WA_UTILS_LIST_NO_ENTRY;   \
            }                                                   \
            /* Return old front element to the pool */          \
            WA_UTILS_LIST_FreeListPool((l).pTemp);              \
            (l).numElements--;                                  \
        }                                                       \
    }

/*****************************************************************************
 *
 * MACRO:      WA_UTILS_LIST_DeListFront
 *
 * PURPOSE:    Remove and return the front item of the list
 *
 * PARAMS:
 *
 *    Identifier:    l
 *    Description:   A structure of type LIST
 *
 *    Identifier:    ppi
 *    Description:   Pointer to pointer to item
 *
 ******************************************************************************/
#define WA_UTILS_LIST_DeListFront(l, ppi)               \
    {                                                   \
        if (ppi != WA_UTILS_LIST_NO_ENTRY)              \
        {                                               \
            *(ppi) = WA_UTILS_LIST_PeekListFront((l));  \
            WA_UTILS_LIST_DiscardListFront((l));        \
        }                                               \
    }

/*****************************************************************************
 *
 * MACRO:      WA_UTILS_LIST_PeekListBack
 *
 * PURPOSE:    Peek at the back item of the List
 *
 * PARAMS:
 *
 *    Identifier:    l
 *    Description:   A structure of type LIST
 *
 ******************************************************************************/
#define WA_UTILS_LIST_PeekListBack(l) ((l).pBack ? ((l).pBack)->pData : WA_UTILS_LIST_NO_ENTRY)

/*****************************************************************************
 *
 * MACRO:      WA_UTILS_LIST_BackListIterator
 *
 * PURPOSE:    Return the back element of the list that can be used to iterate
 *             from the back of the list
 *
 * PARAMS:
 *
 *    Identifier:    l
 *    Description:   A structure of type LIST
 *
 ******************************************************************************/
#define WA_UTILS_LIST_BackListIterator(l) ((l).pBack)

/*****************************************************************************
 *
 * MACRO:      WA_UTILS_LIST_DiscardListBack
 *
 * PURPOSE:    Discard the back item of the List, the item is
 *             lost unless a PeekListBack() is done first to get it.
 *
 * PARAMS:
 *
 *    Identifier:    l
 *    Description:   A structure of type LIST
 *
 ******************************************************************************/
#define WA_UTILS_LIST_DiscardListBack(l)                    \
    {                                                       \
        if ((l).pBack != WA_UTILS_LIST_NO_ENTRY)            \
        {                                                   \
            (l).pTemp = (l).pBack;                          \
            (l).pBack = ((l).pBack)->pPrev;                 \
            if ((l).pBack == WA_UTILS_LIST_NO_ENTRY)        \
            {                                               \
                (l).pFront = WA_UTILS_LIST_NO_ENTRY;        \
            }                                               \
            else                                            \
            {                                               \
                ((l).pBack)->pNext = WA_UTILS_LIST_NO_ENTRY;\
            }                                               \
            /* Return old back element to the pool */       \
            WA_UTILS_LIST_FreeListPool((l).pTemp);          \
            (l).numElements--;                              \
        }                                                   \
    }

/*****************************************************************************
 *
 * MACRO:      WA_UTILS_LIST_DeListBack
 *
 * PURPOSE:    Remove and return the back item of the list
 *
 * PARAMS:
 *
 *    Identifier:    l
 *    Description:   A structure of type LIST
 *
 *    Identifier:    ppi
 *    Description:   Pointer to pointer to item
 *
 ******************************************************************************/
#define WA_UTILS_LIST_DeListBack(l, ppi)        \
    {                                           \
        if (ppi != WA_UTILS_LIST_NO_ENTRY)      \
        {                                       \
            *(ppi) = WA_UTILS_PeekListBack((l));\
            WA_UTILS_DiscardListBack((l));      \
        }                                       \
    }

/*****************************************************************************
 *
 * MACRO:      WA_UTILS_LIST_RemoveList
 *
 * PURPOSE:    Search and remove an item from the list
 *
 * PARAMS:
 *
 *    Identifier:    l
 *    Description:   A structure of type LIST
 *
 *    Identifier:    pi
 *    Description:   Pointer to item to remove
 *
 ******************************************************************************/
#define WA_UTILS_LIST_RemoveList(l, pi)                                             \
    {                                                                               \
        /* If list empty or item is null do nothing */                              \
        if ((l).pFront != WA_UTILS_LIST_NO_ENTRY && (pi) != WA_UTILS_LIST_NO_ENTRY) \
        {                                                                           \
            for((l).pTemp = (l).pFront;                                             \
                (l).pTemp != WA_UTILS_LIST_NO_ENTRY;                                \
                (l).pTemp = ((l).pTemp)->pNext)                                     \
            {                                                                       \
                /* Item found, remove it from list */                               \
                if (((l).pTemp)->pData == (pi))                                     \
                {                                                                   \
                    /* If the item was at the front of the list */                  \
                    if ((l).pFront == (l).pTemp)                                    \
                    {                                                               \
                        WA_UTILS_LIST_DiscardListFront((l));                        \
                    }                                                               \
                    /* If the item was at the back of the list */                   \
                    else if ((l).pBack == (l).pTemp)                                \
                    {                                                               \
                        WA_UTILS_LIST_DiscardListBack((l));                         \
                    }                                                               \
                    /* Remove from anywhere else other than the front or back */    \
                    else                                                            \
                    {                                                               \
                        /* Previous item points to next of item being removed */    \
                        (((l).pTemp)->pPrev)->pNext = ((l).pTemp)->pNext;           \
                        /* Ditto for next item */                                   \
                        (((l).pTemp)->pNext)->pPrev = ((l).pTemp)->pPrev;           \
                        /* Return element to the pool */                            \
                        WA_UTILS_LIST_FreeListPool((l).pTemp);                      \
                        (l).numElements--;                                          \
                    }                                                               \
                    break;                                                          \
                }                                                                   \
            }                                                                       \
        }                                                                           \
    }

/*****************************************************************************
 *
 * MACRO:      WA_UTILS_LIST_GetListCount
 *
 * PURPOSE:    Get number of elements in the list
 *
 * PARAMS:
 *
 *    Identifier:    l
 *    Description:   A structure of type LIST
 *
 ******************************************************************************/
#define WA_UTILS_LIST_GetListCount(l) ((l).numElements)

/*****************************************************************************
 *
 * MACRO:      WA_UTILS_LIST_NextElem
 *
 * PURPOSE:    Return next element from the list
 *
 * PARAMS:
 *
 *    Identifier:    pe
 *    Description:   Pointer to element
 *
 ******************************************************************************/
#define WA_UTILS_LIST_NextElem(pe) ((pe) ? (pe)->pNext : WA_UTILS_LIST_NO_ENTRY)

/*****************************************************************************
 *
 * MACRO:      WA_UTILS_LIST_PrevElem
 *
 * PURPOSE:    Return previous element from the list
 *
 * PARAMS:
 *
 *    Identifier:    pe
 *    Description:   Pointer to element
 *
 ******************************************************************************/
#define WA_UTILS_LIST_PrevElem(pe) ((pe) ? (pe)->pPrev : WA_UTILS_LIST_NO_ENTRY)

/*****************************************************************************
 *
 * MACRO:      WA_UTILS_LIST_Pdata
 *
 * PURPOSE:    Return pointer to data item
 *
 * PARAMS:
 *
 *    Identifier:    pe
 *    Description:   Pointer to element
 *
 ******************************************************************************/
#define WA_UTILS_LIST_Pdata(pe) ((pe) ? (pe)->pData : WA_UTILS_LIST_NO_ENTRY)

#endif                                           /* _LIST_H_ */

