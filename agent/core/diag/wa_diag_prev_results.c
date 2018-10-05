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
 * @file wa_diag_prev_results.c
 *
 * @brief Previous Results - implementation
 */

/** @addtogroup WA_DIAG_PREV_RESULTS
 *  @{
 */

/*****************************************************************************
 * STANDARD INCLUDE FILES
 *****************************************************************************/
#include <string.h>

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/
#include "wa_debug.h"
#include "wa_diag_prev_results.h"
#include "wa_agg.h"
#include "wa_log.h"
#include "wa_json.h"

/*****************************************************************************
 * LOCAL DEFINITIONS
 *****************************************************************************/
#define DEFAULT_EXPIRY_TIME (0) // never

/*****************************************************************************
 * FUNCTION DEFINITIONS
 *****************************************************************************/
int WA_DIAG_PREV_RESULTS_Info(void *instanceHandle, void *initHandle, json_t **params)
{
    WA_ENTER("WA_DIAG_PREV_RESULTS_Info(instanceHandle=%p, initHandle=%p, params=%p)\n",
            instanceHandle, initHandle, params);

    json_t * config = NULL;
    int status = 1;
    int expiry_time = DEFAULT_EXPIRY_TIME; // minutes

    json_decref(*params); //not used
    *params = NULL;

    /* try to get expiry time from config file */
    config = ((WA_DIAG_proceduresConfig_t *)initHandle)->config;
    if (config)
        json_unpack(config, "{s:i}", "expiry_time", &expiry_time);

    WA_DBG("WA_DIAG_PREV_RESULTS_Info(): results expiry time is %d min (0=never)\n", expiry_time);

    const WA_AGG_AggregateResults_t *results = WA_AGG_GetPreviousResults();
    if (results)
    {
        if (!expiry_time || ((time(0) - results->end_time) <= (time_t)(expiry_time * 60)))
        {
            // results are still valid
            if (!WA_AGG_Serialise(results, params) && *params)
            {
                json_object_set_new(*params, "results_valid", json_integer(0));
                status = 0;
            }
            else
                WA_ERROR("WA_DIAG_PREV_RESULTS_Info(): failed to serialise results\n");
        }
        else
            WA_DBG("WA_DIAG_PREV_RESULTS_Info(): previous results are too old\n");
    }
    else
        WA_DBG("WA_DIAG_PREV_RESULTS_Info(): previous results unavailable");

    if (status)
        *params = json_pack("{s:i}", "results_valid", 1);

    WA_RETURN("WA_DIAG_PREV_RESULTS_Info(): %d\n", 0);

    return 0; //always success
}

/* End of doxygen group */
/*! @} */

/* EOF */
