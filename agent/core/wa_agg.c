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
#include "wa_diag_errcodes.h"

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
        agg_results[i].local_time = 0;
        agg_results[i].results_type[0] = '\0';
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
                agg_results[i].diag_results[count].diagResultsName = pDiag->nameInResults;

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

int WA_AGG_StartTestRun(const char *client, bool results_filter, time_t timestamp)
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
    snprintf(agg_results[current_bank].results_type, sizeof(agg_results[current_bank].results_type), "%s", results_filter ? "filtered" : "instant");
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
        agg_results[current_bank].local_time = timestamp;

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

                char info[512] = {'\0'};
                if (result != 0)
                {
                    switch(result)
                    {
                        case WA_DIAG_ERRCODE_FAILURE:
                            if (!strcmp("hdd_status", diag_name)) {
                                strcpy(info, "FAILED_Disk_Health_Status_Error");
                            }
                            else if (!strcmp("mcard_status", diag_name)) {
                                strcpy(info, "FAILED_Invalid_Card_Certification");
                            }
                            else if (!strcmp("rf4ce_status", diag_name)) {
                                strcpy(info, "FAILED_Paired_RCU_Count_Exceeded_Max_Value");
                            }
                            else if (!strcmp("avdecoder_qam_status", diag_name)) {
                                strcpy(info, "FAILED_Play_Status_Error");
                            }
                            else if (!strcmp("tuner_status", diag_name)) {
                                strcpy(info, "FAILED_Read_Status_File_Error");
                            }
                            else if (!strcmp("modem_status", diag_name)) {
                                strcpy(info, "FAILED_Gateway_IP_Not_Reachable");
                            }
                            else if (!strcmp("bluetooth_status", diag_name)) {
                                strcpy(info, "FAILED_Bluetooth_Not_Operational");
                            }
                            else if ((!strcmp("sdcard_status", diag_name)) || (!strcmp("sdcard_status", diag_name)) || (!strcmp("sdcard_status", diag_name))) {
                                strcpy(info, "FAILED_Memory_Verify_Error");
                            }
                            break;
                        case WA_DIAG_ERRCODE_NOT_APPLICABLE:
                            strcpy(info, "WARNING_Test_Not_Applicable");
                            break;
                        case WA_DIAG_ERRCODE_HDD_STATUS_MISSING:
                            strcpy(info, "WARNING_HDD_Test_Not_Run");
                            break;
                        case WA_DIAG_ERRCODE_HDMI_NO_DISPLAY:
                            strcpy(info, "WARNING_No_HDMI_detected._Verify_HDMI_cable_is_connected_on_both_ends_or_if_TV_is_compatible");
                            break;
                        case WA_DIAG_ERRCODE_HDMI_NO_HDCP:
                            strcpy(info, "WARNING_HDMI_authentication_failed._Try_another_HDMI_cable_or_check_TV_compatibility");
                            break;
                        case WA_DIAG_ERRCODE_MOCA_NO_CLIENTS:
                            strcpy(info, "WARNING_No_MoCA_Network_Found");
                            break;
                        case WA_DIAG_ERRCODE_MOCA_DISABLED:
                            strcpy(info, "WARNING_MoCA_OFF");
                            break;
                        case WA_DIAG_ERRCODE_SI_CACHE_MISSING:
                            strcpy(info, "WARNING_Missing_Channel_Map");
                            break;
                        case WA_DIAG_ERRCODE_TUNER_NO_LOCK:
                            strcpy(info, "WARNING_Lock_Failed_-_Check_Cable");
                            break;
                        case WA_DIAG_ERRCODE_TUNER_BUSY:
                            strcpy(info, "WARNING_One_or_more_tuners_are_busy._All_tuners_were_not_tested");
                            break;
                        case WA_DIAG_ERRCODE_AV_NO_SIGNAL:
                            strcpy(info, "WARNING_No_stream_data._Check_cable_and_verify_STB_is_provisioned_correctly");
                            break;
                        case WA_DIAG_ERRCODE_IR_NOT_DETECTED:
                            strcpy(info, "WARNING_IR_Not_Detected");
                            break;
                        case WA_DIAG_ERRCODE_CM_NO_SIGNAL:
                            strcpy(info, "WARNING_Lock_Failed_-_Check_Cable");
                            break;
                        case WA_DIAG_ERRCODE_RF4CE_NO_RESPONSE:
                            strcpy(info, "WARNING_RF_Input_Not_Detected_In_Last_10_Minutes");
                            break;
                        case WA_DIAG_ERRCODE_WIFI_NO_CONNECTION:
                            strcpy(info, "WARNING_No_Connection");
                            break;
                        case WA_DIAG_ERRCODE_AV_URL_NOT_REACHABLE:
                            strcpy(info, "WARNING_No_AV._URL_Not_Reachable_Or_Check_Cable");
                            break;
                        case WA_DIAG_ERRCODE_NON_RF4CE_INPUT:
                            strcpy(info, "WARNING_RF_Paired_But_No_RF_Input");
                            break;
                        case WA_DIAG_ERRCODE_RF4CE_CTRLM_NO_RESPONSE:
                            strcpy(info, "WARNING_RF_Controller_Issue");
                            break;
                        case WA_DIAG_ERRCODE_HDD_MARGINAL_ATTRIBUTES_FOUND:
                            strcpy(info, "WARNING_Marginal_HDD_Values");
                            break;
                        case WA_DIAG_ERRCODE_RF4CE_CHIP_DISCONNECTED:
                            strcpy(info, "FAILED_RF4CE_Chip_Fail");
                            break;
                        case WA_DIAG_ERRCODE_HDD_DEVICE_NODE_NOT_FOUND:
                            strcpy(info, "WARNING_HDD_Device_Node_Not_Found");
                            break;
                        case WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR:
                            strcpy(info, "WARNING_Test_Not_Run");
                            break;
                        case WA_DIAG_ERRCODE_CANCELLED:
                            strcpy(info, "WARNING_Test_Cancelled");
                            break;
                        case WA_DIAG_ERRCODE_CANCELLED_NOT_STANDBY:
                            strcpy(info, "WARNING_Test_Cancelled._Device_not_in_standby");
                            break;
                        case WA_DIAG_ERRCODE_NO_GATEWAY_CONNECTION:
                            strcpy(info, "WARNING_No_Local_Gateway_Connection");
                            break;
                        case WA_DIAG_ERRCODE_NO_COMCAST_WAN_CONNECTION:
                            strcpy(info, "WARNING_No_Comcast_WAN_Connection");
                            break;
                        case WA_DIAG_ERRCODE_NO_PUBLIC_WAN_CONNECTION:
                            strcpy(info, "WARNING_No_Public_WAN_Connection");
                            break;
                        case WA_DIAG_ERRCODE_NO_WAN_CONNECTION:
                            strcpy(info, "WARNING_No_WAN_Connection._Check_Connection");
                            break;
                        case WA_DIAG_ERRCODE_NO_ETH_GATEWAY_FOUND:
                            strcpy(info, "WARNING_No_Gateway_Discovered_via_Ethernet");
                            break;
                        case WA_DIAG_ERRCODE_NO_MW_GATEWAY_FOUND:
                            strcpy(info, "WARNING_No_Local_Gateway_Discovered");
                            break;
                        case WA_DIAG_ERRCODE_NO_ETH_GATEWAY_CONNECTION:
                            strcpy(info, "WARNING_No_Gateway_Response_via_Ethernet");
                            break;
                        case WA_DIAG_ERRCODE_NO_MW_GATEWAY_CONNECTION:
                            strcpy(info, "WARNING_No_Local_Gateway_Response");
                            break;
                        case WA_DIAG_ERRCODE_AV_DECODERS_NOT_ACTIVE:
                            strcpy(info, "WARNING_AV_Decoders_Not_Active");
                            break;
                        case WA_DIAG_ERRCODE_BLUETOOTH_INTERFACE_FAILURE:
                            strcpy(info, "FAILED_Bluetooth_Interfaces_Not_Found");
                            break;
                        case WA_DIAG_ERRCODE_FILE_WRITE_OPERATION_FAILURE:
                            strcpy(info, "FAILED_File_Write_Operation_Error");
                            break;
                        case WA_DIAG_ERRCODE_FILE_READ_OPERATION_FAILURE:
                            strcpy(info, "FAILED_File_Read_Operation_Error");
                            break;
                        case WA_DIAG_ERRCODE_EMMC_TYPEA_MAX_LIFE_EXCEED_FAILURE:
                            strcpy(info, "FAILED_Device_TypeA_Exceeded_Max_Life");
                            break;
                        case WA_DIAG_ERRCODE_EMMC_TYPEB_MAX_LIFE_EXCEED_FAILURE:
                            strcpy(info, "FAILED_Device_TypeB_Exceeded_Max_Life");
                            break;
                        case WA_DIAG_ERRCODE_EMMC_TYPEA_ZERO_LIFETIME_FAILURE:
                            strcpy(info, "FAILED_Device_TypeA_Returned_Invalid_Response");
                            break;
                        case WA_DIAG_ERRCODE_EMMC_TYPEB_ZERO_LIFETIME_FAILURE:
                            strcpy(info, "FAILED_Device_TypeB_Returned_Invalid_Response");
                            break;
                        case WA_DIAG_ERRCODE_MCARD_AUTH_KEY_REQUEST_FAILURE:
                            strcpy(info, "FAILED_Card_Auth_Key_Not_Ready");
                            break;
                        case WA_DIAG_ERRCODE_MCARD_HOSTID_RETRIEVE_FAILURE:
                            strcpy(info, "FAILED_Unable_To_Retrieve_Card_ID");
                            break;
                        case WA_DIAG_ERRCODE_MCARD_CERT_AVAILABILITY_FAILURE:
                            strcpy(info, "FAILED_Card_Certification_Not_Available");
                            break;
                        case WA_DIAG_ERRCODE_DEFAULT_RESULT_VALUE:
                        default:
                            if(result < 0) {
                                strcpy(info, "WARNING_Test_Not_Executed");
                            } else {
                                strcpy(info, "");
                            }
                            break;
                    }
                    strcpy (agg_results[current_bank].diag_results[i].diagResultMessage, info);
                }
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
        int finalResult = 0; /* all PASS */
        json_t *jroot = json_object();
        json_t *jresults = json_object();

        json_object_set_new(jroot, "client", json_string(results->client));
        json_object_set_new(jroot, "results_type", json_string(results->results_type));
//      json_object_set_new(jroot, "start_time", json_string(WA_LOG_GetTimestampStr(results->start_time, buf, sizeof(buf)))); /* COLBO-202 */
        json_object_set_new(jroot, "local_time", json_string(WA_LOG_GetTimestampStr(results->local_time, buf, sizeof(buf))));

        for (int i = 0; i < results->diag_count; i++)
        {
            json_t *jdiagresult = NULL;
            if (results->diag_results[i].result == WA_DIAG_ERRCODE_SUCCESS)
            {
                jdiagresult = json_pack("{s:i}", "r", results->diag_results[i].result);
            }
            else if (results->diag_results[i].result == WA_DIAG_ERRCODE_DEFAULT_RESULT_VALUE) /* Test not exceuted due to pre-conditions */
            {
                continue;
            }
            else
            {
                if ((results->diag_results[i].result == WA_DIAG_ERRCODE_FAILURE) || (results->diag_results[i].result <= WA_DIAG_ERRCODE_BLUETOOTH_INTERFACE_FAILURE))
                    finalResult = 1;

                if (!strcmp(results->results_type, "filtered")) /* Skip the message if results are filtered */
                    jdiagresult = json_pack("{s:i}", "r", results->diag_results[i].result);
                else
                    jdiagresult = json_pack("{s:i,s:s}",
                                                   "r", results->diag_results[i].result,
                                                   "m", results->diag_results[i].diagResultMessage);
            }

            json_object_set_new(jresults, results->diag_results[i].diagResultsName, jdiagresult);
        }

        json_t *jdiagfinalresult = json_pack("{s:i}", "r", finalResult);
        json_object_set_new(jresults, "Final", jdiagfinalresult);
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
        char *results_type_str = NULL;
        char *timestamp_str = NULL;
        time_t timestamp;

        status = 10;
        if (json_unpack(json, "{s:s}", "client", &client_str) || !client_str)
            goto err;

        strncpy(out_results->client, client_str, sizeof(out_results->client) - 1);

        status = 11;
        if (json_unpack(json, "{s:s}", "results_type", &results_type_str) || !results_type_str)
            goto err;

        strncpy(out_results->results_type, results_type_str, sizeof(out_results->results_type) - 1);

/*        status = 12;
        if (json_unpack(json, "{s:s}", "start_time", &timestamp_str) || !timestamp_str)
            goto err;

        status = 13;
        if (WA_LOG_GetTimestampTime(timestamp_str, &timestamp))
            goto err;

        out_results->start_time = timestamp;
*/
        status = 14;
        if (json_unpack(json, "{s:s}", "local_time", &timestamp_str) || !timestamp_str)
            goto err;

        status = 15;
        if (WA_LOG_GetTimestampTime(timestamp_str, &timestamp))
            goto err;

        out_results->local_time = timestamp;

        status = 16;
        json_t *results_json = NULL;
        if (json_unpack(json, "{s:o}", "results", &results_json) || !results_json)
            goto err;

        for (int i = 0; i < out_results->diag_count; i++)
        {
            status = 100+i;
            json_t *diag_result_json = NULL;
            if (json_unpack(results_json, "{s:o}", out_results->diag_results[i].diagResultsName, &diag_result_json) || !diag_result_json)
            {
                WA_DBG("WA_AGG_Deserialise(): No previous result for %s\n", out_results->diag_results[i].diagResultsName);
                continue;
            }
            else
            {
                status = 200+i;
                int result;
                char *message = NULL;
                if (json_unpack(diag_result_json, "{s:i, s:s}", "r", &result, "m", &message) || !message)
                {
                    if (json_unpack(diag_result_json, "{s:i}", "r", &result))
                        goto err;
                }

                out_results->diag_results[i].result = result;
                if (message != NULL)
                    strcpy (out_results->diag_results[i].diagResultMessage, message);
            }
        }

        status = 17;
        json_t *diag_finalresult_json = NULL;
        if (json_unpack(results_json, "{s:o}", "Final", &diag_finalresult_json) || !diag_finalresult_json)
        {
            WA_DBG("WA_AGG_Deserialise(): missing final result\n");
            goto err;
        }

        status = 18;
        int final_result;
        if (json_unpack(diag_finalresult_json, "{s:i}", "r", &final_result))
            goto err;

        status = 0;
err:
        if (status)
            WA_DBG("WA_AGG_Deserialise(): invalid json data (%i)\n", status);
    }
    else
        WA_DBG("WA_AGG_Serialise(): invalid parameters\n");

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
            WA_DBG("load_results(): deserialisation failed\n");

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
