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

#ifndef WA_UTILS_LIST_PRIVATE_H
#define WA_UTILS_LIST_PRIVATE_H

#include <string.h>

/* Use already set WA_UTILS_LIST_MAX_LIST_SIZE if available.
 * This lets other modules to control the lists size.
 */
#ifndef WA_UTILS_LIST_MAX_LIST_SIZE
#define WA_UTILS_LIST_MAX_LIST_SIZE (128)
#endif

#ifdef WA_STEST
#undef WA_UTILS_LIST_MAX_LIST_SIZE
#define WA_UTILS_LIST_MAX_LIST_SIZE (256)
#endif

#define WA_UTILS_LIST_AllocListPool(l, pi, pe)                      \
    if ((pi) != WA_UTILS_LIST_NO_ENTRY)                             \
    {                                                               \
        int allocListPoolIterator = 0;                              \
        (pe) = WA_UTILS_LIST_NO_ENTRY;                              \
                                                                    \
        for (allocListPoolIterator = 0;                             \
             allocListPoolIterator < WA_UTILS_LIST_MAX_LIST_SIZE;   \
             ++allocListPoolIterator)                               \
        {                                                           \
            if ((l).elmPool[allocListPoolIterator].inUse == 0)      \
            {                                                       \
                (pe) = &((l).elmPool[allocListPoolIterator]);       \
                (pe)->inUse = 1;                                    \
                (pe)->pData = (pi);                                 \
                break;                                              \
            }                                                       \
        }                                                           \
    }

#define WA_UTILS_LIST_FreeListPool(pe)                          \
    if ((pe) != WA_UTILS_LIST_NO_ENTRY)                         \
    {                                                           \
        /* Clear the structure */                               \
        memset((pe), 0, sizeof(*(pe)));                         \
    }

#endif                                           /* WA_UTILS_LIST_PRIVATE_H */

