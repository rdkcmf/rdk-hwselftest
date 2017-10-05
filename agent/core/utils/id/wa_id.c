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
 * @file wa_id.c
 *
 * @brief This file contains functions for generation of unique id.
 */

/** @addtogroup WA_UTILS_ID
 *  @{
 */
#ifdef WA_DEBUG
#undef WA_DEBUG
#endif
/*****************************************************************************
 * STANDARD INCLUDE FILES
 *****************************************************************************/

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/
#include "wa_debug.h"
#include "wa_id.h"
#include "wa_osa.h"

/*****************************************************************************
 * GLOBAL VARIABLE DEFINITIONS
 *****************************************************************************/

/*****************************************************************************
 * LOCAL DEFINITIONS
 *****************************************************************************/
#define PREPROCESSOR_ASSERT(x) (void)(sizeof(char[-!(x)]))

#define NONULL 0x80000000

/*****************************************************************************
 * LOCAL TYPES
 *****************************************************************************/
static WA_UTILS_ID_box_t idBox;

/*****************************************************************************
 * LOCAL FUNCTION PROTOTYPES
 *****************************************************************************/

/*****************************************************************************
 * LOCAL VARIABLE DECLARATIONS
 *****************************************************************************/
static void *idMutex = NULL;

/*****************************************************************************
 * FUNCTION DEFINITIONS
 *****************************************************************************/
int WA_UTILS_ID_Init()
{
    WA_ENTER("WA_UTILS_ID_Init()\n");

    if(idMutex != NULL)
    {
        WA_WARN("WA_UTILS_ID_Init(): already initialized\n");
        goto end;
    }

    idMutex = WA_OSA_MutexCreate();
    if(idMutex == NULL)
    {
        WA_ERROR("WA_UTILS_ID_Init(): WA_OSA_MutexCreate(): error\n");
    }
    WA_UTILS_LIST_ClearList(idBox.list);
end:
    WA_RETURN("WA_UTILS_ID_Init(): %d\n", (idMutex == NULL));
    return (idMutex == NULL);
}

int WA_UTILS_ID_Exit()
{
    int status;
    WA_ENTER("WA_UTILS_ID_Exit()\n");

    status = WA_OSA_MutexDestroy(idMutex);
    if(status != 0)
    {
        WA_ERROR("WA_UTILS_ID_Exit(): WA_OSA_MutexDestroy(): %d\n", status);
    }
    WA_RETURN("WA_UTILS_ID_Exit(): %d\n", status);
    return status;
}

int WA_UTILS_ID_Generate(WA_UTILS_ID_t *pId)
{
    int status, s1;
    WA_ENTER("WA_UTILS_ID_Generate(pId=%p)\n", pId);

    status = WA_OSA_MutexLock(idMutex);
    if(status != 0)
    {
        WA_ERROR("WA_UTILS_ID_Generate(): WA_OSA_MutexLock(): %d\n", status);
        goto end;
    }

    status = WA_UTILS_ID_GenerateUnsafe(&idBox, pId);
    if(status != 0)
    {
        WA_ERROR("WA_UTILS_ID_Generate(): WA_UTILS_ID_GenerateUnsafe(): %d\n", status);
    }

    s1 = WA_OSA_MutexUnlock(idMutex);
    if(s1 != 0)
    {
        WA_ERROR("WA_UTILS_ID_Generate(): WA_OSA_MutexUnlock(): %d\n", status);
        status = -1;
    }
end:
    WA_RETURN("WA_UTILS_ID_Generate(): %d\n", status);
    return status;
}

int WA_UTILS_ID_Release(WA_UTILS_ID_t id)
{
    int status;
    WA_ENTER("WA_UTILS_ID_Release(id=%d)\n", id);

    status = WA_OSA_MutexLock(idMutex);
    if(status != 0)
    {
        WA_ERROR("WA_UTILS_ID_Release(): WA_OSA_MutexLock(): %d\n", status);
        goto end;
    }

    WA_UTILS_ID_ReleaseUnsafe(&idBox, id);

    status = WA_OSA_MutexUnlock(idMutex);
    if(status != 0)
    {
        WA_ERROR("WA_UTILS_ID_Release(): WA_OSA_MutexUnlock(): %d\n", status);
    }
end:
    WA_RETURN("WA_UTILS_ID_Release(): %d\n", status);
    return status;
}

int WA_UTILS_ID_GenerateUnsafe(WA_UTILS_ID_box_t *pBox, WA_UTILS_ID_t *pId)
{
    int status = -1;
    void *iterator;
    WA_UTILS_LIST_t *pList = &(pBox->list);
    WA_UTILS_ID_t eid;
    int idPrev = -1;
    void *iteratorAfterFree = WA_UTILS_LIST_NO_ELEM;

    WA_ENTER("WA_UTILS_ID_GenerateUnsafe(pBox=%p, pId=%p)\n", pBox, pId);

    /* WARNING: Check against WA_UTILS_LIST_t that internally uses array of a fixed size */
    PREPROCESSOR_ASSERT((WA_UTILS_ID_t)(-1) >= WA_UTILS_LIST_MAX_LIST_SIZE-1);

    if(WA_UTILS_LIST_ElemCount(pList) > (WA_UTILS_ID_t)(-1))
    {
        WA_ERROR("WA_UTILS_ID_GenerateUnsafe(): ID limit exceeded.\n");
        goto end;
    }

    ++(pBox->id);
    for(iterator = WA_UTILS_LIST_FrontIterator(pList);
        iterator != WA_UTILS_LIST_NO_ELEM;
        iterator = WA_UTILS_LIST_NextIterator(pList, iterator))
    {
        eid = (WA_UTILS_ID_t)((uintptr_t)WA_UTILS_LIST_DataAtIterator(pList, iterator) & ~NONULL);
        if(iteratorAfterFree == WA_UTILS_LIST_NO_ELEM)
        {
            if(idPrev != eid-1)
            {
                iteratorAfterFree = iterator;
            }
            else
            {
                idPrev = eid;
            }
        }
        if(pBox->id == eid)
        {
            ++(pBox->id);
        }
        else if(pBox->id < eid)
        {
            iterator = WA_UTILS_LIST_InsertBefore(pList, iterator, (void *)(uintptr_t)(pBox->id | NONULL));
            if(iterator == WA_UTILS_LIST_NO_ELEM)
            {
                WA_ERROR("WA_UTILS_ID_GenerateUnsafe(): WA_UTILS_LIST_InsertBefore() error\n");
                goto end;
            }
            status = 0;
            goto ok;
        }
    }
    if((pBox->id == 0) && (iteratorAfterFree != WA_UTILS_LIST_NO_ELEM))
    {
        pBox->id = idPrev+1;
        iterator = WA_UTILS_LIST_InsertBefore(pList, iteratorAfterFree, (void *)(uintptr_t)(pBox->id | NONULL));
        if(iterator == WA_UTILS_LIST_NO_ELEM)
        {
            WA_ERROR("WA_UTILS_ID_GenerateUnsafe(): WA_UTILS_LIST_InsertBefore() error\n");
            goto end;
        }
    }
    else
    {
        iterator = WA_UTILS_LIST_AddBack(pList, (void *)(uintptr_t)(pBox->id | NONULL));
        if(iterator == WA_UTILS_LIST_NO_ELEM)
        {
            WA_ERROR("WA_UTILS_ID_GenerateUnsafe(): WA_UTILS_LIST_AddBack() error\n");
            goto end;
        }
    }
ok:
    *pId = pBox->id;
    WA_INFO("WA_UTILS_ID_GenerateUnsafe(): id=%d\n", pBox->id);
    status = 0;
end:
    WA_RETURN("WA_UTILS_ID_GenerateUnsafe(): %d\n", status);
    return status;
}

int WA_UTILS_ID_ReleaseUnsafe(WA_UTILS_ID_box_t *pBox, WA_UTILS_ID_t id)
{
    WA_UTILS_LIST_t *pList = &(pBox->list);

    WA_ENTER("WA_UTILS_ID_ReleaseUnsafe(pBox=%p, id=%d)\n", pBox, id);

    WA_UTILS_LIST_Remove(pList, (void *)(uintptr_t)(id | NONULL));

    WA_RETURN("WA_UTILS_ID_ReleaseUnsafe(): 0\n");
    return 0;
}

/*****************************************************************************
 * LOCAL FUNCTIONS
 *****************************************************************************/

/* End of doxygen group */
/*! @} */

/* EOF */
