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
 * @file wa_agg.c
 *
 * @brief Implementation of test results aggregation functionality.
 */

/** @addtogroup WA_AGG
 *  @{
 */

/*****************************************************************************
 * STANDARD INCLUDE FILES
 *****************************************************************************/
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <telemetry_busmessage_sender.h>

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/
#include "wa_agg.h"
#include "wa_diag.h"
#include "wa_log.h"
#include "wa_debug.h"
#include "wa_osa.h"

/*****************************************************************************
 * GLOBAL VARIABLE DEFINITIONS
 *****************************************************************************/

/*****************************************************************************
 * LOCAL DEFINITIONS
 *****************************************************************************/
#define AGG_RESULTS_FILE "/tmp/hwselftest.results"
#define DEFAULT_RESULT_VALUE -200

 /*****************************************************************************
 * LOCAL TYPES
 *****************************************************************************/

/*****************************************************************************
 * LOCAL FUNCTION PROTOTYPES
 *****************************************************************************/
static int load_results(const char *file, WA_AGG_AggregateResults_t *bank);
static int save_results(const char *file, const WA_AGG_AggregateResults_t *bank);

/*****************************************************************************
 * LOCAL VARIABLE DECLARATIONS
 *****************************************************************************/
static void *api_mutex = NULL;
static int current_bank = -1;
static WA_AGG_AggregateResults_t agg_results[2];
static bool writeTestResult = true;

/*****************************************************************************
 * FUNCTION DEFINITIONS
 *****************************************************************************/

int WA_AGG_Init(const WA_DIAG_proceduresConfig_t *diags)
{
    int status = 1;

    WA_ENTER("WA_AGG_Init(diags=%p)\n", diags);

    api_mutex = WA_OSA_MutexCreate();
    if (!api_mutex)
    {
        WA_ERROR("WA_AGG_Init(): WA_OSA_MutexCreate() failed\n");
        goto err;
    }

    for (int i = 0; i < 2; i++)
    {
        /* mark the bank as dirty */
        agg_results[i].dirty = true;
        agg_results[i].diag_results = NULL;
        agg_results[i].start_time = 0;
        agg_results[i].end_time = 0;
        agg_results[i].client[0] = '\0';

        int count = 0;

        /* set defaults for each diag of the bank */
        for (const WA_DIAG_proceduresConfig_t *pDiag = diags; pDiag && pDiag->fnc; pDiag++)
        {
            if (strstr(pDiag->name, "_status"))
            {
                void *tmp = realloc(agg_results[i].diag_results, (count + 1) * sizeof(WA_AGG_DiagResult_t));
                if (tmp)
                    agg_results[i].diag_results = tmp;
                else
                {
                    WA_ERROR("WA_AGG_Init(): out of memory\n");
                    status = 1;
                    goto err;
                }

                agg_results[i].diag_results[count].diag = pDiag->name;
                agg_results[i].diag_results[count].timestamp = 0;
                agg_results[i].diag_results[count].result = DEFAULT_RESULT_VALUE;

                count++;
            }
        }

        agg_results[i].diag_count = count;
        status = 0;
    }

err:
    if (!status)
    {
        /* load most recent results from file */
        if (!load_results(AGG_RESULTS_FILE, &agg_results[0]))
        {
            WA_DBG("WA_AGG_Init(): successfully loaded results from file\n");
            agg_results[0].dirty = false;
        }
        else
        {
            /* this is not an error */
            WA_INFO("WA_AGG_Init(): previous results not available\n");
        }
    }
    else
        WA_AGG_Exit();

    WA_RETURN("WA_AGG_Init(): %d\n", status);

    return status;
}

int WA_AGG_Exit()
{
    int status = 0;

    WA_ENTER("WA_AGG_Exit()\n");

    /* release results memory */
    free(agg_results[0].diag_results);
    free(agg_results[1].diag_results);

    current_bank = -1;

    if (api_mutex)
    {
        if (WA_OSA_MutexDestroy(api_mutex))
            WA_ERROR("WA_AGG_Exit(): WA_OSA_MutexDestroy() failed\n");
    }

    WA_RETURN("WA_AGG_Exit(): %d\n", status);

    return status;
}

int WA_AGG_StartTestRun(const char *client, time_t timestamp)
{
    int status = 1;

    WA_ENTER("WA_AGG_StartTestRun(\"%s\", ...)\n", client);

    if (WA_OSA_MutexLock(api_mutex))
    {
        WA_ERROR("WA_AGG_StartTestRun(): WA_OSA_MutexLock() failed\n");
        t2_event_d("SYST_ERR_Mutexlockfail", 1);
        goto err;
    }

    /* pick up a bank to store the results */
    current_bank = (agg_results[0].dirty? 0 : 1);

    agg_results[current_bank].start_time = timestamp;
    strncpy(agg_results[current_bank].client, client, sizeof(agg_results[current_bank].client) - 1);
    status = 0;

    if (WA_OSA_MutexUnlock(api_mutex))
        WA_ERROR("WA_AGG_StartTestRun(): WA_OSA_MutexUnlock() failed\n");

err:
    WA_RETURN("WA_AGG_StartTestRun(): %d\n", status);

    return status;
}

int WA_AGG_FinishTestRun(time_t timestamp)
{
    int status = 1;

    WA_ENTER("WA_AGG_FinishTestRun(...)\n");

    if (WA_OSA_MutexLock(api_mutex))
    {
        WA_ERROR("WA_AGG_FinishTestRun(): WA_OSA_MutexLock() failed\n");
        t2_event_d("SYST_ERR_Mutexlockfail", 1);
        goto err;
    }

    if (current_bank != -1)
    {
        agg_results[current_bank].end_time = timestamp;

        if (writeTestResult)
        {
            /* save results to file */
            if (save_results(AGG_RESULTS_FILE, &agg_results[current_bank]))
                WA_ERROR("WA_AGG_FinishTestRun(): failed to save results file\n");
        }

        /* mark the bank as valid, force the other bank as dirty */
        agg_results[current_bank].dirty = false;
        agg_results[!current_bank].dirty = true;
        current_bank = -1;
        status = 0;
    }
    else
        WA_ERROR("WA_AGG_FinishTestRun(): test run not started\n");

    if (WA_OSA_MutexUnlock(api_mutex))
        WA_ERROR("WA_AGG_FinishTestRun(): WA_OSA_MutexUnlock() failed\n");

err:
    WA_RETURN("WA_AGG_FinishTestRun(): %d\n", status);

    return status;
}

int WA_AGG_SetTestResult(const char *diag_name, int result, time_t timestamp)
{
    int status = 1;

    WA_ENTER("WA_AGG_SetTestResult(%s, %i, ...)\n", diag_name, result);

    if (WA_OSA_MutexLock(api_mutex))
    {
        WA_ERROR("WA_AGG_SetTestResult(): WA_OSA_MutexLock() failed\n");
        t2_event_d("SYST_ERR_Mutexlockfail", 1);
        goto err;
    }

    if (current_bank != -1)
    {
        /* find the diag and store its result and timestamp */
        for (int i = 0; i < agg_results[current_bank].diag_count; i++)
        {
            if (!strcmp(agg_results[current_bank].diag_results[i].diag, diag_name))
            {
                agg_results[current_bank].diag_results[i].result = result;
                agg_results[current_bank].diag_results[i].timestamp = timestamp;
                status = 0;
                break;
            }
        }

        if (status)
            WA_DBG("WA_AGG_SetTestResult(): diag %s not found\n", diag_name);
    }
    else
        WA_DBG("WA_AGG_SetTestResult(): test run not started\n");

    if (WA_OSA_MutexUnlock(api_mutex))
        WA_ERROR("WA_AGG_SetTestResult(): WA_OSA_MutexUnlock() failed\n");

err:
    WA_RETURN("WA_AGG_SetTestResult(): %d\n", status);

    return status;
}

void WA_AGG_SetWriteTestResult(bool writeResult)
{
    writeTestResult = writeResult;
    WA_DBG("WA_DIAG_SetWriteTestResult(): %d\n", writeTestResult);
}

const WA_AGG_AggregateResults_t *WA_AGG_GetPreviousResults()
{
    const WA_AGG_AggregateResults_t *results = NULL;

    if (WA_OSA_MutexLock(api_mutex))
    {
        WA_ERROR("WA_AGG_GetPreviousResults(): WA_OSA_MutexLock() failed\n");
        t2_event_d("SYST_ERR_Mutexlockfail", 1);
        goto err;
    }

    /* return results of the non-dirty bank, if any */
    if (!agg_results[0].dirty)
        results = &agg_results[0];
    else if (!agg_results[1].dirty)
        results = &agg_results[1];
    else
        WA_DBG("WA_AGG_GetPreviousResults(): previous results not available\n");

    if (WA_OSA_MutexUnlock(api_mutex))
        WA_ERROR("WA_AGG_GetPreviousResults(): WA_OSA_MutexUnlock() failed\n");

err:
    return results;
}

int WA_AGG_Serialise(const WA_AGG_AggregateResults_t *results, json_t **out_json)
{
    int status = 1;

    WA_ENTER("WA_AGG_Serialise(%p, %p)\n", results, out_json);

    if (results && out_json)
    {
        char buf[32];
        json_t *jroot = json_object();
        json_t *jresults = json_object();

        json_object_set_new(jroot, "client", json_string(results->client));
        json_object_set_new(jroot, "start_time", json_string(WA_LOG_GetTimestampStr(results->start_time, buf, sizeof(buf))));
        json_object_set_new(jroot, "end_time", json_string(WA_LOG_GetTimestampStr(results->end_time, buf, sizeof(buf))));

        for (int i = 0; i < results->diag_count; i++)
        {
            json_t *jdiagresult = json_pack("{s:i,s:s}",
                                           "result", results->diag_results[i].result,
                                           "timestamp", WA_LOG_GetTimestampStr(results->diag_results[i].timestamp, buf, sizeof(buf)));

            json_object_set_new(jresults, results->diag_results[i].diag, jdiagresult);
        }

        json_object_set_new(jroot, "results", jresults);
        *out_json = jroot;

        status = 0;
    }
    else
        WA_ERROR("WA_AGG_Serialise(): invalid parameters\n");

    WA_RETURN("WA_AGG_Serialise(): %d\n", status);

    return status;
}

int WA_AGG_Deserialise(json_t *json, WA_AGG_AggregateResults_t *out_results)
{
    int status = 1;

    WA_ENTER("WA_AGG_Deserialise(%p, %p)\n", json, out_results);

    if (json && out_results)
    {
        char *client_str = NULL;
        char *timestamp_str = NULL;
        time_t timestamp;

        status = 10;
        if (json_unpack(json, "{s:s}", "client", &client_str) || !client_str)
            goto err;

        strncpy(out_results->client, client_str, sizeof(out_results->client) - 1);

        status = 11;
        if (json_unpack(json, "{s:s}", "start_time", &timestamp_str) || !timestamp_str)
            goto err;

        status = 12;
        if (WA_LOG_GetTimestampTime(timestamp_str, &timestamp))
            goto err;

        out_results->start_time = timestamp;

        status = 13;
        if (json_unpack(json, "{s:s}", "end_time", &timestamp_str) || !timestamp_str)
            goto err;

        status = 14;
        if (WA_LOG_GetTimestampTime(timestamp_str, &timestamp))
            goto err;

        out_results->end_time = timestamp;

        status = 15;
        json_t *results_json = NULL;
        if (json_unpack(json, "{s:o}", "results", &results_json) || !results_json)
            goto err;

        for (int i = 0; i < out_results->diag_count; i++)
        {
            status = 100+i;
            json_t *diag_result_json = NULL;
            if (json_unpack(results_json, "{s:o}", out_results->diag_results[i].diag, &diag_result_json) || !diag_result_json)
            {
                WA_ERROR("WA_AGG_Deserialise(): missing results for %s\n", out_results->diag_results[i].diag);
                goto err;
            }
            else
            {
                status = 200+i;
                int result;
                if (json_unpack(diag_result_json, "{s:s,s:i}", "timestamp", &timestamp_str, "result", &result) || !timestamp_str)
                    goto err;

                status = 300+i;
                if (WA_LOG_GetTimestampTime(timestamp_str, &timestamp))
                    goto err;

                out_results->diag_results[i].result = result;
                out_results->diag_results[i].timestamp = timestamp;
            }
        }

        status = 0;
err:
        if (status)
            WA_ERROR("WA_AGG_Deserialise(): invalid json data (%i)\n", status);
    }
    else
        WA_ERROR("WA_AGG_Serialise(): invalid parameters\n");

    WA_RETURN("WA_AGG_Deserialise(): %d\n", status);

    return status;
}

/*****************************************************************************
 * LOCAL FUNCTION DEFINITIONS
 *****************************************************************************/

static int load_results(const char *file, WA_AGG_AggregateResults_t *bank)
{
    int status = 1;

    WA_ENTER("load_results(%s, %p)\n", file, bank);

    json_t *json = json_load_file(file, 0, NULL);
    if (json)
    {
        if (!WA_AGG_Deserialise(json, bank))
        {
            WA_DBG("load_results(): json loaded successfully\n");
            status = 0;
        }
        else
            WA_ERROR("load_results(): deserialisation failed\n");

        json_decref(json);
    }
    else
        WA_DBG("load_results(): json_load_file() failed\n");

    WA_RETURN("load_results(): %d\n", status);

    return status;
}

static int save_results(const char *file, const WA_AGG_AggregateResults_t *bank)
{
    int status = 1;

    WA_ENTER("save_results(%s, %p)\n", file, bank);

    json_t *json = NULL;

    if (!WA_AGG_Serialise(bank, &json) && json)
    {
        FILE *f = fopen(file, "wb+");
        if (f)
        {
            if (!json_dumpf(json, f, 0))
            {
                fsync(fileno(f));
                WA_DBG("save_results(): json saved successfully\n");
                status = 0;
            }
            else
                WA_ERROR("save_results(): json_dump_file() failed\n");

            fclose(f);
        }
        else
            WA_ERROR("save_results(): failed to create file\n");

        json_decref(json);
    }
    else
        WA_ERROR("save_results(): serialisation failed\n");

    WA_RETURN("save_results(): %d\n", status);

    return status;
}

/* End of doxygen group */
/*! @} */

/* EOF */
