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
#include <stdio.h>
#include "wa_list_api.h"


int main(void)
{
    LIST_t list;
    int *p;
    void *listIterator = WA_UTILS_LIST_NO_ELEM;

    LIST_Purge(&list);

    p = WA_UTILS_LIST_AllocAddBack(&list, sizeof(int));
    if(p == NULL)
    {
        goto end;
    }
    *p = 3;

    p = WA_UTILS_LIST_AllocInsertBefore(&list, listIterator, sizeof(int));
    if(p == NULL)
    {
        goto end;
    }
    *p = 4;

    listIterator = WA_UTILS_LIST_FrontIterator(&list);
    listIterator = WA_UTILS_LIST_NextIterator(&list, listIterator);

    p = WA_UTILS_LIST_AllocInsertBefore(&list, listIterator, sizeof(int));
    if(p == NULL)
    {
        goto end;
    }
    *p = 5;

    for(listIterator = WA_UTILS_LIST_FrontIterator(&list);
            listIterator != WA_UTILS_LIST_NO_ELEM;
            listIterator = WA_UTILS_LIST_NextIterator(&list, listIterator))
    {
        p = WA_UTILS_LIST_DataAtIterator(&list, listIterator);
        printf("HWST_DBG |elem: %d\n", *p);
    }
    printf("HWST_DBG |Num: %d\n", WA_UTILS_LIST_ElemCount(&list));
    LIST_AllocPurge(&list);
    printf("HWST_DBG |Num: %d\n", WA_UTILS_LIST_ElemCount(&list));
    end:
    LIST_AllocPurge(&list);
    return 0;
}
