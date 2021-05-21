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
 * @file wa_diag_filter.h
 *
 * @brief Results Filter - interface
 */

/** @addtogroup WA_DIAG_FILTER
 *  @{
 */

#ifndef WA_DIAG_FILTER_H
#define WA_DIAG_FILTER_H

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/
#include "wa_json.h"
#include "hostIf_tr69ReqHandler.h"

#ifdef __cplusplus
extern "C"
{
#endif

/*****************************************************************************
 * EXPORTED TYPES
 *****************************************************************************/
typedef struct filter_t
{
    char filter_params[128];
    int  queue_depth;
    bool enable;
    bool results_filtered;
} filter;

typedef struct bufferFile_t
{
    const char *name;
    const char *diag_name;
    char filter_type[8];
    char result_history[128];
} bufferFile;

/*****************************************************************************
 * EXPORTED FUNCTIONS
 *****************************************************************************/
int WA_FILTER_FilterInit();
int WA_FILTER_FilterExit();

int WA_FILTER_SetFilterBuffer();
int WA_FILTER_GetFilteredResult(const char *testDiag, int status);
int WA_FILTER_DumpResultFilter();
bool WA_FILTER_IsFilterEnabled();
bool WA_FILTER_IsResultsFiltered();

/*****************************************************************************
 * LOCAL FUNCTIONS
 *****************************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* WA_DIAG_FILTER_H */

/* End of doxygen group */
/*! @} */

/* EOF */
