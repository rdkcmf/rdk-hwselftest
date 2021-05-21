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
 * @file wa_diag_filter.c
 *
 * @brief Results Filter - implementation
 */

/** @addtogroup WA_DIAG_FILTER
 *  @{
 */

/*****************************************************************************
 * STANDARD INCLUDE FILES
 *****************************************************************************/
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <telemetry_busmessage_sender.h>

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/
#include "wa_debug.h"
#include "wa_agg.h"
#include "wa_log.h"
#include "wa_json.h"
#include "wa_osa.h"
#include "wa_iarm.h"
#include "wa_diag_errcodes.h"

/* rdk specific */
#include "libIBus.h"
#include "libIARMCore.h"

/* module interface */
#include "wa_diag_filter.h"

/*****************************************************************************
 * PRE-PROCESSOR DEFINITIONS
 *****************************************************************************/
#define TR181_RESULT_FILTER                      "Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.hwHealthTest.ResultFilter."
#define TR181_RESULT_FILTER_ENABLE               "Enable"
#define TR181_RESULT_FILTER_QUEUE_DEPTH          "QueueDepth"
#define TR181_RESULT_FILTER_FILTER_PARAMS        "FilterParams"
#define TR181_RESULT_FILTER_RESULTS_FILTERED     "ResultsFiltered"

#define DEFAULT_RESULT_FILTER_PATH               "/opt/hwselftest/"
#define DEFAULT_RESULT_FILTER_BUFFER_FILE        "/opt/hwselftest/hwstresults.buffer"
#define TEMPORARY_RESULT_FILTER_BUFFER_FILE      "/opt/hwselftest/hwstresults.temp"

#define STRING_QDEPTH                            "QDEPTH"

#define MIN_DEFAULT_QUEUE_DEPTH                  20
#define MAX_DEFAULT_QUEUE_DEPTH                  100

/*****************************************************************************
 * GLOBAL VARIABLE DEFINITIONS
 *****************************************************************************/

/*****************************************************************************
 * LOCAL VARIABLE DECLARATIONS
 *****************************************************************************/
static void *buffer_mutex;
static filter resultFilter;

static bufferFile diags[] =
{
    {"HDD", "hdd_status", {0}, {0}},
    {"FLASH", "flash_status", {0}, {0}},
    {"SDCard", "sdcard_status", {0}, {0}},
    {"DRAM", "dram_status", {0}, {0}},
    {"HDMI", "hdmiout_status", {0}, {0}},
    {"CableCard", "mcard_status", {0}, {0}},
    {"RFR", "rf4ce_status", {0}, {0}},
    {"IRR", "ir_status", {0}, {0}},
    {"MOCA", "moca_status", {0}, {0}},
    {"AVDecoder", "avdecoder_qam_status", {0}, {0}},
    {"QAMTuner", "tuner_status", {0}, {0}},
    {"DOCSIS", "modem_status", {0}, {0}},
    {"BTLE", "bluetooth_status", {0}, {0}},
    {"WiFi", "wifi_status", {0}, {0}},
    {"WAN", "wan_status", {0}, {0}},

    /* end */
    {NULL, NULL, {0}, {0}}
};

/*****************************************************************************
 * LOCAL FUNCTION PROTOTYPES
 *****************************************************************************/
static int getResultFilterParam_IARM(char* param, char* value);
static int getResultFilterParams();
static int initResultFilterBufferFile();
static int createBuffer();
static int readBuffer(FILE *f, int depth);
static int assignFilterTypes();

/*****************************************************************************
 * LOCAL FUNCTIONS
 *****************************************************************************/
static int getResultFilterParam_IARM(char* param, char* value)
{
    IARM_Result_t iarm_result = IARM_RESULT_IPCCORE_FAIL;
    char *status;
    int is_connected = 0;
    int result = -1;

    WA_ENTER("getResultFilterParam_IARM()\n");

    iarm_result = IARM_Bus_IsConnected(IARM_BUS_TR69HOSTIFMGR_NAME, &is_connected);
    if (iarm_result != IARM_RESULT_SUCCESS)
    {
        WA_ERROR("getResultFilterParam_IARM(): IARM_Bus_IsConnected('%s') failed\n", IARM_BUS_TR69HOSTIFMGR_NAME);
        return result;
    }

    HOSTIF_MsgData_t *stMsgDataParam = NULL;

    iarm_result = IARM_Malloc(IARM_MEMTYPE_PROCESSLOCAL, sizeof(*stMsgDataParam), (void**)&stMsgDataParam);
    if ((iarm_result == IARM_RESULT_SUCCESS) && stMsgDataParam)
    {
        memset(stMsgDataParam, 0, sizeof(*stMsgDataParam));
        snprintf(stMsgDataParam->paramName, TR69HOSTIFMGR_MAX_PARAM_LEN, "%s%s", TR181_RESULT_FILTER, param);

        iarm_result = IARM_Bus_Call(IARM_BUS_TR69HOSTIFMGR_NAME, IARM_BUS_TR69HOSTIFMGR_API_GetParams, (void *)stMsgDataParam, sizeof(*stMsgDataParam));

        if (iarm_result != IARM_RESULT_SUCCESS)
        {
            WA_ERROR("getResultFilterParam_IARM(): IARM_Bus_Call('%s') failed\n", IARM_BUS_TR69HOSTIFMGR_NAME);
            IARM_Free(IARM_MEMTYPE_PROCESSLOCAL, stMsgDataParam);
            return result;
        }

        status = (char*)&stMsgDataParam->paramValue[0];
        strncpy(value, status, strlen(status));
        result = 0;

        WA_DBG("getResultFilterParam_IARM(): %s%s = \"%s\"\n", TR181_RESULT_FILTER, param, status);

        IARM_Free(IARM_MEMTYPE_PROCESSLOCAL, stMsgDataParam);
    }
    else
        WA_ERROR("getResultFilterParam_IARM(): IARM_Malloc() failed\n");

    WA_RETURN("getResultFilterParam_IARM() returns : %d\n", result);

    return result;
}

static int getResultFilterParams()
{
    char param_value[128];

    if (WA_UTILS_IARM_Connect())
    {
        WA_ERROR("getResultFilterParams(): WA_UTILS_IARM_Connect() Failed \n");
        return 0;
    }

    memset(param_value, 0, sizeof(param_value));
    if (getResultFilterParam_IARM(TR181_RESULT_FILTER_ENABLE, param_value) == 0)
    {
        if (strcmp(param_value, "true") == 0)
            resultFilter.enable = true;
        else
            resultFilter.enable = false;
    }

    WA_INFO("getResultFilterParams(): hwHealthTest.ResultFilter.Enable: %i\n", resultFilter.enable);

    memset(param_value, 0, sizeof(param_value));
    if (getResultFilterParam_IARM(TR181_RESULT_FILTER_QUEUE_DEPTH, param_value) == 0)
    {
        if (strcmp(param_value, "") != 0)
            resultFilter.queue_depth = atoi(param_value);
        else
            resultFilter.queue_depth = 0;
    }

    // Check the set the queue depth within the default range (1 to 100)
    resultFilter.queue_depth = (resultFilter.queue_depth == 0) ? MIN_DEFAULT_QUEUE_DEPTH : resultFilter.queue_depth;
    resultFilter.queue_depth = (resultFilter.queue_depth > MAX_DEFAULT_QUEUE_DEPTH) ? MAX_DEFAULT_QUEUE_DEPTH : resultFilter.queue_depth;

    WA_INFO("getResultFilterParams(): hwHealthTest.ResultFilter.QueueDepth: %i\n", resultFilter.queue_depth);

    memset(param_value, 0, sizeof(param_value));
    if (getResultFilterParam_IARM(TR181_RESULT_FILTER_FILTER_PARAMS, param_value) == 0)
        snprintf(resultFilter.filter_params, sizeof(resultFilter.filter_params), "%s", param_value);

    WA_INFO("getResultFilterParams(): hwHealthTest.ResultFilter.FilterParams: %s\n", resultFilter.filter_params);

    memset(param_value, 0, sizeof(param_value));
    if (getResultFilterParam_IARM(TR181_RESULT_FILTER_RESULTS_FILTERED, param_value) == 0)
    {
        if (strcmp(param_value, "true") == 0)
            resultFilter.results_filtered = true;
        else
            resultFilter.results_filtered = false;
    }

    WA_INFO("getResultFilterParams(): hwHealthTest.ResultFilter.ResultsFiltered: %i\n", resultFilter.results_filtered);

    if(WA_UTILS_IARM_Disconnect())
    {
        WA_DBG("getResultFilterParams(): WA_UTILS_IARM_Disconnect() Failed\n");
    }

    return 0;
}

static int initResultFilterBufferFile()
{
    FILE *f = NULL;
    struct stat buffer;
    char msg_buf[256];
    char out_buf[256];
    int status = -1;

    if (WA_OSA_MutexLock(buffer_mutex))
    {
        WA_ERROR("initResultFilterBufferFile(): WA_OSA_MutexLock() failed\n");
        t2_event_d("SYST_ERR_Mutexlockfail", 1);
        return status;
    }

    bool new_file = (stat(DEFAULT_RESULT_FILTER_BUFFER_FILE, &buffer) == 0) ? false : true;

    if (new_file)
    {
        if (stat(DEFAULT_RESULT_FILTER_PATH, &buffer) != 0)
            mkdir(DEFAULT_RESULT_FILTER_PATH, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

        WA_DBG("initResultFilterBufferFile(): Writing new buffer file\n");
        createBuffer();
    }
    else
    {
        f = fopen(DEFAULT_RESULT_FILTER_BUFFER_FILE, "r+");
        if (!f)
        {
            WA_ERROR("initResultFilterBufferFile(): fopen('%s') failed\n", DEFAULT_RESULT_FILTER_BUFFER_FILE);
            goto end;
        }

        int depth = 0;
        fgets(msg_buf, sizeof(msg_buf), f);
        snprintf(out_buf, sizeof(out_buf), "%s=%%d", STRING_QDEPTH);
        if (sscanf(msg_buf, out_buf, &depth) == 1)
        {
            // queue_depth is new value from recent config, depth is old value from buffer file
            if (resultFilter.queue_depth > depth)
            {
                WA_DBG("initResultFilterBufferFile(): RFC QueueDepth has been increased, old=%i, new=%i\n", depth, resultFilter.queue_depth);
                readBuffer(f, depth);
            }
            else if (resultFilter.queue_depth < depth)
            {
                WA_DBG("initResultFilterBufferFile(): RFC QueueDepth has been decreased, old=%i, new=%i\n", depth, resultFilter.queue_depth);
                readBuffer(f, depth);
            }
            else if (resultFilter.queue_depth == depth) // QDEPTH present in file and QueueDepth RFC values are same
            {
                WA_DBG("initResultFilterBufferFile(): RFC QueueDepth remains same, old=%i, new=%i\n", depth, resultFilter.queue_depth);
                readBuffer(f, depth);
            }
            else
            {
                WA_DBG("initResultFilterBufferFile(): RFC QueueDepth from file is not valid, old=%i, new=%i\n", depth, resultFilter.queue_depth);
                createBuffer();
            }
        }
        else
        {
            WA_DBG("initResultFilterBufferFile(): Creating fresh buffer file due to invalid data %s in buffer file %s.\n", msg_buf, DEFAULT_RESULT_FILTER_BUFFER_FILE);
            createBuffer();
        }

        fclose(f);
    }

    if (stat(TEMPORARY_RESULT_FILTER_BUFFER_FILE, &buffer) == 0)
    {
        remove(DEFAULT_RESULT_FILTER_BUFFER_FILE);
        rename(TEMPORARY_RESULT_FILTER_BUFFER_FILE, DEFAULT_RESULT_FILTER_BUFFER_FILE);
        WA_DBG("initResultFilterBufferFile(): Buffer file initialized with new RFC configuration\n");
    }

    status = assignFilterTypes();

end:
    if (WA_OSA_MutexUnlock(buffer_mutex))
        WA_ERROR("initResultFilterBufferFile(): WA_OSA_MutexUnlock() failed\n");

    return status;
}

static int createBuffer()
{
    FILE *tf = NULL;
    char out_buf[256];

    tf = fopen(TEMPORARY_RESULT_FILTER_BUFFER_FILE, "w");
    if (!tf)
    {
        WA_ERROR("createBuffer(): fopen('%s') failed\n", TEMPORARY_RESULT_FILTER_BUFFER_FILE);
        return -1;
    }

    // IMPORTANT: Make sure when a line is written into the buffer file, it contains '\n' which makes the next line to as a new line
    char buf[resultFilter.queue_depth];
    memset(buf, 'P', sizeof(buf)); // Setting dummy passes until queue_depth
    snprintf(out_buf, sizeof(out_buf), "%s=%i\n", STRING_QDEPTH, resultFilter.queue_depth);
    fputs(out_buf, tf);

    for (bufferFile *diag = &diags[0]; diag && diag->name; diag++)
    {
        strncpy(diag->result_history, buf, resultFilter.queue_depth);
        snprintf(out_buf, sizeof(out_buf), "%s=%s\n", diag->name, diag->result_history);
        fputs(out_buf, tf);
        WA_DBG("createBuffer(): New buffer file entry ('%s=%s')\n", diag->name, diag->result_history);
    }

    fclose(tf);

    return 0;
}

static int readBuffer(FILE *f, int depth)
{
    FILE *tf = NULL;
    char out_buf[256];
    char msg_buf[256];
    char *diag_name;
    char *diag_results;

    if (!f)
    {
        WA_ERROR("readBuffer(): fopen('%s') failed\n", DEFAULT_RESULT_FILTER_BUFFER_FILE);
        return -1;
    }

    tf = fopen(TEMPORARY_RESULT_FILTER_BUFFER_FILE, "w");
    if (!tf)
    {
        WA_ERROR("readBuffer(): fopen('%s') failed\n", TEMPORARY_RESULT_FILTER_BUFFER_FILE);
        return -1;
    }

    // IMPORTANT: Make sure when a line is written into the buffer file, it contains '\n' which makes the next line to as a new line
    char buf[resultFilter.queue_depth];
    memset(buf, 'P', sizeof(buf)); // Setting dummy passes until queue_depth
    snprintf(out_buf, sizeof(out_buf), "%s=%i\n", STRING_QDEPTH, resultFilter.queue_depth);
    fputs(out_buf, tf);

    while (fgets(msg_buf, sizeof(msg_buf), f))
    {
        diag_name = strtok(msg_buf, "=");
        if (!diag_name)
        {
            continue;
        }
        diag_results = strtok(NULL, "=");

        if (!diag_results || (diag_results && strcmp(diag_results, "") == 0)) // By any change the history of results not available setting dummy passes for the depth
        {
            WA_DBG("readBuffer(): Error in file('%s') containing empty results \n", DEFAULT_RESULT_FILTER_BUFFER_FILE);
            snprintf(out_buf, sizeof(out_buf), "%s", buf);
        }
        else
        {
            if (resultFilter.queue_depth > depth)
            {
                char temp_buf[256];
                int diff = resultFilter.queue_depth - depth;
                char add_buf[diff];

                memset(add_buf, 'P', sizeof(add_buf)); // Setting dummy passes until queue_depth
                snprintf(temp_buf, sizeof(temp_buf), "%s", diag_results);
                temp_buf[depth] = 0; // Removing the '\n' at the end of line read from file
                snprintf(out_buf, sizeof(out_buf), "%s%s", temp_buf, add_buf); // Storing dummy passes at the end which is oldest
            }
            else
            {
                snprintf(out_buf, sizeof(out_buf), "%s", diag_results);
            }
        }

        for (bufferFile *diag = &diags[0]; diag && diag->name; diag++)
        {
            if (strcmp(diag->name, diag_name) == 0)
            {
                strncpy(diag->result_history, out_buf, resultFilter.queue_depth);
                snprintf(out_buf, sizeof(out_buf), "%s=%s\n", diag->name, diag->result_history);
                fputs(out_buf, tf);
                WA_DBG("readBuffer(): Buffer file entry ('%s=%s')\n", diag->name, diag->result_history);
                break;
            }
        }
    }

    fclose(tf);

    return 0;
}

static int assignFilterTypes()
{
    char *filter_type;
    char filter_params[128];
    int len = 0;

    snprintf(filter_params, sizeof(filter_params), "%s", resultFilter.filter_params);
    WA_DBG("assignFilterTypes(): filter_params = %s\n", filter_params);

    // Remove white spaces in the string
    len = strlen(filter_params);
    for(int i = 0; i < len; i++)
    {
        if(filter_params[i] == ' ')
        {
            for(int j = i; j < len; j++)
            {
                filter_params[j] = filter_params[j + 1];
            }
            len--;
        }
    }

    filter_type = strtok(filter_params, ",");
    for (bufferFile *diag = &diags[0]; diag && diag->name; diag++)
    {
        if (filter_type)
        {
            snprintf(diag->filter_type, sizeof(diag->filter_type), "%s", filter_type);
            WA_DBG("assignFilterTypes(): diag = %s, filter_type = %s\n", diag->name, diag->filter_type);
        }
        filter_type = strtok(NULL, ",");
    }

    return 0;
}

/*****************************************************************************
 * FUNCTION DEFINITIONS
 *****************************************************************************/
int WA_FILTER_FilterInit()
{
    int status = 0;

    WA_ENTER("WA_FILTER_FilterInit()\n");

    buffer_mutex = WA_OSA_MutexCreate();
    if (!buffer_mutex)
    {
        WA_ERROR("WA_FILTER_FilterInit(): WA_OSA_MutexCreate() failed\n");
        status = -1;
    }

    WA_RETURN("WA_FILTER_FilterInit(): %d\n", status);
    return status;
}

int WA_FILTER_FilterExit()
{
    WA_ENTER("WA_FILTER_FilterExit()\n");

    if (buffer_mutex != NULL)
    {
        if (WA_OSA_MutexDestroy(buffer_mutex))
            WA_ERROR("WA_FILTER_FilterExit(): WA_OSA_MutexDestroy() failed\n");
    }

    WA_RETURN("WA_FILTER_FilterExit(): %d\n", 0);
    return 0;
}

int WA_FILTER_SetFilterBuffer()
{
    int status = -1;

    WA_ENTER("WA_FILTER_SetFilterBuffer()\n");

    status = getResultFilterParams(); // Always returns 0; This will continue normal tests to run even though filter buffer does not work due to internal IARM failures
    if (!resultFilter.enable)
    {
        WA_INFO("WA_FILTER_SetFilterBuffer(): Result Filter is disabled.\n");
        return 0; // Filter is disabled and normal test will continue
    }

    status = initResultFilterBufferFile();
    if (status != 0)
    {
         WA_ERROR("WA_FILTER_SetFilterBuffer(): initResultFilterBufferFile() failed\n");
    }

    WA_RETURN("WA_FILTER_SetFilterBuffer(): %d\n", status);

    return status;
}

int WA_FILTER_GetFilteredResult(const char *testDiag, int status)
{
    char *value;
    char status_result;
    char res_buf[256];
    int fail = 0;
    int limit = 0;
    int result = 0;
    int filter_status = (status == WA_DIAG_ERRCODE_FAILURE || status <= WA_DIAG_ERRCODE_BLUETOOTH_INTERFACE_FAILURE) ? WA_DIAG_ERRCODE_FAILURE : WA_DIAG_ERRCODE_SUCCESS; // Default or No Filter

    if (!resultFilter.enable)
    {
        WA_INFO("WA_FILTER_ResultToBufferFile(): Result filter feature disabled\n");
        return filter_status;
    }

    if (strcmp(resultFilter.filter_params, "") == 0)
    {
        WA_INFO("WA_FILTER_ResultToBufferFile(): Result filter params not set. Considering as NO FILTER.\n");
        return filter_status;
    }

    if (WA_OSA_MutexLock(buffer_mutex))
    {
        WA_ERROR("WA_FILTER_ResultToBufferFile(): WA_OSA_MutexLock() failed\n");
        t2_event_d("SYST_ERR_Mutexlockfail", 1);
        return filter_status;
    }

    for (bufferFile *diag = &diags[0]; diag && diag->diag_name; diag++)
    {
        if (!strstr(diag->diag_name, "_status")) // considering only the tests
        {
            continue;
        }

        int len = strlen(testDiag);
        if (strncmp(testDiag, diag->diag_name, len) == 0)
        {
            size_t leng = strlen(diag->result_history);
            status_result = (filter_status == WA_DIAG_ERRCODE_FAILURE) ? 'F' : 'P'; // Setting Fail or Pass based on the current test result
            snprintf(res_buf, sizeof(res_buf),"%c%s", status_result, diag->result_history); // Appending the result history with the current result added in beginning
            res_buf[strlen(res_buf) - 1] = '\0'; // Removing the oldest result in buffer which is at the end
            memset(&diag->result_history, 0, sizeof(diag->result_history));
            strncpy(diag->result_history, res_buf, leng);

            WA_DBG("WA_FILTER_GetFilteredResult(): res_buf from buffer file with current result: %s\n", res_buf);

            // filter_type must not have any spaces
            // Samples: P90 (90% failure) / S7 (7 failures in sequence)
            value = (diag->filter_type[1] != '\0') ? &diag->filter_type[1] : NULL;
            limit = value ? atoi(value) : 0;

            if ((diag->filter_type[0] == 'P' || diag->filter_type[0] == 'p') && (limit > 0 && limit <= 100)) // Percentage (1%-100%), otherwise No Filter 'N'
            {
                for (int i = 0; i < resultFilter.queue_depth; i++)
                {
                    if (res_buf[i] == '\0')
                        break;

                    if (res_buf[i] == 'F')
                        fail++;
                }

                result = (fail * 100) / resultFilter.queue_depth;
                filter_status = (result < limit) ? WA_DIAG_ERRCODE_SUCCESS : WA_DIAG_ERRCODE_FAILURE;
            }
            else if ((diag->filter_type[0] == 'S' || diag->filter_type[0] == 's') && (limit > 0)) // Sequence (must be greater than 0), otherwise No Filter 'N'
            {
                for (int i = 0; i < resultFilter.queue_depth; i++)
                {
                    if (res_buf[i] == '\0')
                        break;

                    if (res_buf[i] == 'F')
                        fail++;
                    else
                    {
                        result = (result < fail) ? fail : result;
                        fail = 0;
                    }
                }

                result = (result < fail) ? fail : result;
                limit = (limit > resultFilter.queue_depth) ? resultFilter.queue_depth : limit; // Validating the limit which must not exceed queue_depth
                filter_status = (result < limit) ? WA_DIAG_ERRCODE_SUCCESS : WA_DIAG_ERRCODE_FAILURE;
            }

            WA_INFO("WA_FILTER_GetFilteredResult(): diag: %s, filter_type: %c, limit: %i, status: %d, history: %s\n", diag->name, diag->filter_type[0], limit, filter_status, diag->result_history);
            break;
        }
    }

    if (WA_OSA_MutexUnlock(buffer_mutex))
        WA_ERROR("WA_FILTER_ResultToBufferFile(): WA_OSA_MutexUnlock() failed\n");

    return filter_status;
}

int WA_FILTER_DumpResultFilter()
{
    FILE *tf;
    struct stat buffer;
    char msg_buf[256];
    int status = -1;

    if (!resultFilter.enable)
    {
        WA_INFO("WA_FILTER_DumpResultFilter(): Result Filter is disabled.\n");
        return 0;
    }

    if (strcmp(resultFilter.filter_params, "") == 0)
    {
        WA_INFO("WA_FILTER_DumpResultFilter(): Result filter params is empty. Considering as NO FILTER.\n");
        return status;
    }

    if (WA_OSA_MutexLock(buffer_mutex))
    {
        WA_ERROR("WA_FILTER_DumpResultFilter(): WA_OSA_MutexLock() failed\n");
        t2_event_d("SYST_ERR_Mutexlockfail", 1);
        goto end;
    }

    tf = fopen(TEMPORARY_RESULT_FILTER_BUFFER_FILE, "w");
    if (!tf)
    {
        WA_ERROR("WA_FILTER_DumpResultFilter(): fopen('%s') failed\n", TEMPORARY_RESULT_FILTER_BUFFER_FILE);
        goto end;
    }

    // IMPORTANT: Make sure when a line is written into the buffer file, it contains '\n' which makes the next line to as a new line
    snprintf(msg_buf, sizeof(msg_buf), "%s=%i\n", STRING_QDEPTH, resultFilter.queue_depth);
    fputs(msg_buf, tf);

    for (bufferFile *diag = &diags[0]; diag && diag->name; diag++)
    {
        WA_DBG("WA_FILTER_DumpResultFilter(): %s=%s\n", diag->name, diag->result_history);
        snprintf(msg_buf, sizeof(msg_buf), "%s=%s\n", diag->name, diag->result_history);
        fputs(msg_buf, tf);
    }

    fclose(tf);

    if (stat(TEMPORARY_RESULT_FILTER_BUFFER_FILE, &buffer) == 0)
    {
        remove(DEFAULT_RESULT_FILTER_BUFFER_FILE);
        if (rename(TEMPORARY_RESULT_FILTER_BUFFER_FILE, DEFAULT_RESULT_FILTER_BUFFER_FILE) != -1)
        {
            status = 0;
            WA_DBG("WA_FILTER_DumpResultFilter(): Result filter buffer file updated with current results\n");
        }
    }

end:
    if (WA_OSA_MutexUnlock(buffer_mutex))
        WA_ERROR("WA_FILTER_DumpResultFilter(): WA_OSA_MutexUnlock() failed\n");

    return status;
}

bool WA_FILTER_IsFilterEnabled()
{
    return resultFilter.enable;
}

bool WA_FILTER_IsResultsFiltered()
{
    return (resultFilter.enable ? resultFilter.results_filtered : false);
}

/* End of doxygen group */
/*! @} */

/* EOF */
