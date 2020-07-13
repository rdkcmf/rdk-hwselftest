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
 * @file wa_agg.h
 *
 * @brief Interface of test results aggregation functionality.
 *
 */

/** @addtogroup WA_AGG
 *  @{
 */

#ifndef WA_AGG_H
#define WA_AGG_H

/*****************************************************************************
 * STANDARD INCLUDE FILES
 *****************************************************************************/
#include <time.h>
#include <stdbool.h>

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/
#include "wa_diag.h"
#include "wa_json.h"

#ifdef __cplusplus
extern "C"
{
#endif

/*****************************************************************************
 * EXPORTED TYPES
 *****************************************************************************/
typedef struct
{
    const char *diag;
    time_t timestamp;
    int result;
}
WA_AGG_DiagResult_t;

typedef struct
{
    bool dirty;
    char client[32];
    time_t start_time;
    time_t end_time;
    int diag_count;
    WA_AGG_DiagResult_t *diag_results;
}
WA_AGG_AggregateResults_t;

/*****************************************************************************
 * EXPORTED FUNCTIONS
 *****************************************************************************/

/**
 * @brief Initialises internal aggregator structures.
 *
 * @param diags Available diags
 *
 * @returns Operation status.
 * @retval 0 for success, non-zero otherwise
 */
int WA_AGG_Init(const WA_DIAG_proceduresConfig_t *diags);

/**
 * @brief Frees internal aggregator structures.
 *
 * @returns Operation status.
 * @retval 0 for success, non-zero otherwise
 */
int WA_AGG_Exit();

/**
 * @brief Marks start of a test run.
 *
 * @param timestamp Test run start time (epoch).
 *
 * @returns Operation status.
 * @retval 0 for success, non-zero otherwise
 */
int WA_AGG_StartTestRun(const char *client, time_t timestamp);

/**
 * @brief Marks end of a test run.
 *
 * @param timestamp Test run finish time (epoch).
 *
 * @returns Operation status.
 * @retval 0 for success, non-zero otherwise
 */
int WA_AGG_FinishTestRun(time_t timestamp);

/**
 * @brief Sets result of a test run in the current test run.
 *
 * @param diag_name Name of the test.
 * @param result    Result of the test.
 * @param timestamp Test finish time (epoch).
 *
 * @returns Operation status.
 * @retval 0 for success, non-zero otherwise
 */
int WA_AGG_SetTestResult(const char *diag_name, int result, time_t timestamp);

/**
 * @brief Sets the bool value whether to write the results or not.
 *
 * @param writeResult true or false.
 */
void WA_AGG_SetWriteTestResult(bool writeResult);

/**
 * @brief Returns latest available aggregated results if available.
 *
 * @returns Latest available aggregated results
 * @retval Pointer to WA_AGG_AggregateResults_t structure, NULL if not available.
 */
const WA_AGG_AggregateResults_t *WA_AGG_GetPreviousResults();

/**
 * @brief Serialises aggregated results WA_AGG_AggregateResults_t structure to a JSON object.
 *
 * @param results  Results structure to serialise.
 * @param out_json Pointer to JSON object pointer to fill.
 *
 * @returns Operation status.
 * @retval 0 for success, non-zero otherwise
 */
int WA_AGG_Serialise(const WA_AGG_AggregateResults_t *results, json_t **out_json);

/**
 * @brief Deserialises a JSON object with results to WA_AGG_AggregateResults_t structure.
 *
 * @param results  Results structure to serialise.
 * @param out_json Pointer to JSON object pointer to fill.
 *
 * @returns Operation status.
 * @retval 0 for success, non-zero otherwise
 */
int WA_AGG_Deserialise(json_t *json, WA_AGG_AggregateResults_t *out_results);

#ifdef __cplusplus
}
#endif

#endif /* WA_AGG_H */

/* EOF */
