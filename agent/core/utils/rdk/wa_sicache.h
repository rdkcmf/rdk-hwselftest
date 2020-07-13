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
 * @file
 *
 * @brief This file contains interface functions for rdk si cache.
 */

/** @addtogroup WA_UTILS_SICACHE
 *  @{
 */

#ifndef WA_UTILS_SICACHE_H
#define WA_UTILS_SICACHE_H

/*****************************************************************************
 * STANDARD INCLUDE FILES
 *****************************************************************************/

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif

/*****************************************************************************
 * EXPORTED DEFINITIONS
 *****************************************************************************/

/*****************************************************************************
 * EXPORTED TYPES
 *****************************************************************************/

typedef struct {
    unsigned int freq;
    unsigned int mod;
    unsigned int prog;
} WA_UTILS_SICACHE_Entries_t;

/*****************************************************************************
 * EXPORTED VARIABLES
 *****************************************************************************/

/*****************************************************************************
 * EXPORTED FUNCTIONS
 *****************************************************************************/

/**
 * Acquires entries from sicache.
 *
 * @param numEntries maximum number of entries toacquire
 * @param ppEntries pointer to where the allocated array of entries will be stored
 *
 * @note The allocated pEntries must be freed by the caller.
 *
 * @returns number of acquired entries
 * @retval -1 error
 */
int WA_UTILS_SICACHE_TuningRead(int numEntries, WA_UTILS_SICACHE_Entries_t **ppEntries);

int WA_UTILS_SICACHE_TuningGetLuckyId();

void WA_UTILS_SICACHE_TuningSetLuckyId(int id);

void WA_UTILS_SICACHE_TuningSetTuneData(char *tuneData);

/*****************************************************************************
 * LOCAL FUNCTIONS
 *****************************************************************************/

#ifdef __cplusplus
}
#endif

#endif
