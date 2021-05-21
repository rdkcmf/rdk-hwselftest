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

/*****************************************************************************
 * STANDARD INCLUDE FILES
 *****************************************************************************/
#include <string>
#include <memory>
#include <cstring>
#include <sstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <fstream>
#include <unistd.h>
#include <algorithm>

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/
#include "DeviceInfo_hwHealthTest.h"
#include "hostIf_tr69ReqHandler.h"
#include "wa_wsclient.h"
#include "hwst_sched.hpp"
#include "jansson.h"
#include <rdk_debug.h>
#include <telemetry_busmessage_sender.h>

/*****************************************************************************
 * LOCAL DEFINITIONS
 *****************************************************************************/
#define FILE_CPP "DeviceInfo_hwHealthTest.cpp"

#define TUNE_TYPE_SOURCE_ID    "SRCID"
#define TUNE_TYPE_VCN          "VCN"
#define TUNE_TYPE_FREQ_PROG    "FREQ_PROG"
#define TUNE_RESULTS_FILE      "/opt/logs/hwselftest.tuneresults"
#define MAX_LEN                TR69HOSTIFMGR_MAX_PARAM_LEN
#define FILTER_FILES_PATH            "/opt/hwselftest"
#define FILTER_ENABLE_FILE           "/opt/hwselftest/.hwselftest_filter_enable"
#define FILTER_QUEUE_DEPTH_FILE      "/opt/hwselftest/.hwst_queuedepth"
#define RESULTS_FILTERED_ENABLE_FILE "/opt/hwselftest/.hwst_filteredResult_enable"
#define FILTER_BUFFER_FILE           "/opt/hwselftest/hwstresults.buffer"
#define FILTER_QUEUE_DEPTH_DEFAULT   20
#define FILTER_QUEUE_DEPTH_MAX       100

/*****************************************************************************
 * EXPORTED FUNCTIONS
 *****************************************************************************/

namespace {

void create_emptyStrResponse(HOSTIF_MsgData_t *buf)
{
    buf->paramValue[0] = '\0';
    buf->paramLen = 0;
    buf->paramtype = hostIf_StringType;
}

}

namespace hwselftest {

bool set_Device_DeviceInfo_xOpsDeviceMgmt_hwHealthTest_Enable(const char *log_module, HOSTIF_MsgData_t *stMsgData)
{
    bool ret = false;

    RDK_LOG(RDK_LOG_DEBUG, log_module, "[%s:%s] attempting to enable/disable the hwselftest component...\n", FILE_CPP, __FUNCTION__);

    if(stMsgData == NULL)
    {
        RDK_LOG(RDK_LOG_ERROR, log_module,"[%s:%s] stMsgData is null pointer!\n", FILE_CPP, __FUNCTION__);
        return ret;
    }

    wa_wsclient *pInst = wa_wsclient::instance();
    if (pInst)
    {
        bool value = *(reinterpret_cast<bool *>(stMsgData->paramValue));
        ret = pInst->enable(value);

        if (ret)
            RDK_LOG(RDK_LOG_DEBUG, log_module, "[%s:%s] hwselftest %s successfully\n", FILE_CPP, __FUNCTION__, (value? "enabled" : "disabled"));
        else
            RDK_LOG(RDK_LOG_ERROR, log_module,"[%s:%s] failed to %s hwselftest\n", FILE_CPP, __FUNCTION__, (value? "enable" : "disable"));
    }
    else
        RDK_LOG(RDK_LOG_ERROR, log_module,"[%s:%s] failed to get wa_wsclient instance\n", FILE_CPP, __FUNCTION__);

    return ret;
}

bool set_Device_DeviceInfo_xOpsDeviceMgmt_hwHealthTest_ExecuteTest(const char *log_module, HOSTIF_MsgData_t *stMsgData)
{
    bool ret = false;

    RDK_LOG(RDK_LOG_DEBUG, log_module, "[%s:%s] attempting to execute tests...\n", FILE_CPP, __FUNCTION__);

    if(stMsgData == NULL)
    {
        RDK_LOG(RDK_LOG_ERROR, log_module,"[%s:%s] stMsgData is null pointer!\n", FILE_CPP, __FUNCTION__);
        return ret;
    }

    create_emptyStrResponse(stMsgData);
    stMsgData->faultCode = fcInternalError;

    wa_wsclient *pInst = wa_wsclient::instance();
    if (pInst)
    {
        if(!pInst->is_enabled())
        {
            RDK_LOG(RDK_LOG_DEBUG, log_module, "[%s:%s] tests not scheduled, feature disabled\n", FILE_CPP, __FUNCTION__);
            pInst->log("Feature disabled. ExecuteTest request ignored.\n");
            stMsgData->faultCode = fcNoFault;
        }
        else
        {
            ret = pInst->execute_tests(false /* execution from tr69 */);

            if (ret)
            {
                RDK_LOG(RDK_LOG_DEBUG, log_module, "[%s:%s] tests scheduled successfully\n", FILE_CPP, __FUNCTION__);
                stMsgData->faultCode = fcNoFault;
            }
            else
                RDK_LOG(RDK_LOG_ERROR, log_module,"[%s:%s] failed to schedule test execution\n", FILE_CPP, __FUNCTION__);
        }
    }
    else
        RDK_LOG(RDK_LOG_ERROR, log_module,"[%s:%s] failed to get wa_wsclient instance\n", FILE_CPP, __FUNCTION__);

    return true;
}

bool get_Device_DeviceInfo_xOpsDeviceMgmt_hwHealthTest_Results(const char *log_module, HOSTIF_MsgData_t *stMsgData)
{
    bool ret = false;

    RDK_LOG(RDK_LOG_DEBUG, log_module, "[%s:%s] attempting to retrieve test results...\n", FILE_CPP, __FUNCTION__);

    if(stMsgData == NULL)
    {
        RDK_LOG(RDK_LOG_ERROR, log_module,"[%s:%s] stMsgData is null pointer!\n", FILE_CPP, __FUNCTION__);
        return ret;
    }

    create_emptyStrResponse(stMsgData);
    stMsgData->faultCode = fcInternalError;

    wa_wsclient *pInst = wa_wsclient::instance();
    if (pInst)
    {
        if(!pInst->is_enabled())
        {
            RDK_LOG(RDK_LOG_DEBUG, log_module, "[%s:%s] test results not read, feature disabled\n", FILE_CPP, __FUNCTION__);
            pInst->log("Feature disabled. Results request ignored.\n");
            stMsgData->faultCode = fcNoFault;
        }
        else
        {
            std::string result;

            ret = pInst->get_results(result);

            if (ret)
            {
                RDK_LOG(RDK_LOG_DEBUG, log_module, "[%s:%s] test results retrieved successfully\n", FILE_CPP, __FUNCTION__);
                strncpy(stMsgData->paramValue, result.c_str(), TR69HOSTIFMGR_MAX_PARAM_LEN - 1)[TR69HOSTIFMGR_MAX_PARAM_LEN - 1] = '\0';
                stMsgData->paramLen = (result.length() > TR69HOSTIFMGR_MAX_PARAM_LEN - 1? TR69HOSTIFMGR_MAX_PARAM_LEN - 1 : result.length()) + 1;
                stMsgData->faultCode = fcNoFault;
            }
            else
                RDK_LOG(RDK_LOG_ERROR,log_module,"[%s:%s] failed to retrieve test results\n", FILE_CPP, __FUNCTION__);
        }
    }
    else
        RDK_LOG(RDK_LOG_ERROR, log_module,"[%s:%s] failed to get wa_wsclient instance\n", FILE_CPP, __FUNCTION__);

    return true;
}

bool set_Device_DeviceInfo_xOpsDeviceMgmt_hwHealthTest_SetTuneType(const char *log_module, HOSTIF_MsgData_t *stMsgData)
{
    bool ret = false;

    RDK_LOG(RDK_LOG_DEBUG, log_module, "[%s:%s] attempting to set tune type for the hwselftest tuner test...\n", FILE_CPP, __FUNCTION__);

    if(stMsgData == NULL)
    {
        RDK_LOG(RDK_LOG_ERROR, log_module,"[%s:%s] stMsgData is null pointer!\n", FILE_CPP, __FUNCTION__);
        return ret;
    }

    unsigned int value = *(reinterpret_cast<bool *>(stMsgData->paramValue));
    create_emptyStrResponse(stMsgData);
    stMsgData->faultCode = fcInternalError;

    wa_wsclient *pInst = wa_wsclient::instance();
    if (pInst)
    {
        if(!pInst->is_enabled())
        {
            RDK_LOG(RDK_LOG_DEBUG, log_module, "[%s:%s] tune type not set, feature disabled\n", FILE_CPP, __FUNCTION__);
            pInst->log("Feature disabled. SetTuneType request ignored.\n");
            stMsgData->faultCode = fcNoFault;
        }
        else
        {
            ret = pInst->set_tune_type(value);

            if (ret)
            {
                RDK_LOG(RDK_LOG_DEBUG, log_module, "[%s:%s] hwselftest tune type %s set successfully\n", FILE_CPP, __FUNCTION__, value);
                stMsgData->faultCode = fcNoFault;
            }
            else
                RDK_LOG(RDK_LOG_ERROR, log_module,"[%s:%s] failed to set tune type %s for hwselftest tuner test\n", FILE_CPP, __FUNCTION__, value);
        }
    }
    else
        RDK_LOG(RDK_LOG_ERROR, log_module,"[%s:%s] failed to get wa_wsclient instance\n", FILE_CPP, __FUNCTION__);

    return true;
}

bool set_Device_DeviceInfo_xOpsDeviceMgmt_hwHealthTest_ExecuteTuneTest(const char *log_module, HOSTIF_MsgData_t *stMsgData)
{
    bool ret = false;
    char input[32];
    char *param = NULL;
    json_t *json = NULL;
    unsigned int value = NO_TUNE;

    RDK_LOG(RDK_LOG_DEBUG, log_module, "[%s:%s] attempting to execute tuner test...\n", FILE_CPP, __FUNCTION__);

    if(stMsgData == NULL)
    {
        RDK_LOG(RDK_LOG_ERROR, log_module,"[%s:%s] stMsgData is null pointer!\n", FILE_CPP, __FUNCTION__);
        return ret;
    }

    sprintf(input,"%s", stMsgData->paramValue);

    create_emptyStrResponse(stMsgData);
    stMsgData->faultCode = fcInternalError;

    wa_wsclient *pInst = wa_wsclient::instance();
    if (pInst)
    {
        if(!pInst->is_enabled())
        {
            RDK_LOG(RDK_LOG_DEBUG, log_module, "[%s:%s] tests not scheduled, feature disabled\n", FILE_CPP, __FUNCTION__);
            pInst->log("Feature disabled. ExecuteTuneTest request ignored.\n");
            stMsgData->faultCode = fcNoFault;
        }
        else
        {
            if (!pInst->get_tune_type(&value))
            {
                RDK_LOG(RDK_LOG_ERROR, log_module,"[%s:%s] failed to get tune type for tuner test execution\n", FILE_CPP, __FUNCTION__);
                return true;
            }

            switch (value)
            {
            case NO_TUNE:
                break;
            case SRCID:
                json = json_pack("{s:s}", TUNE_TYPE_SOURCE_ID, input);
                break;
            case VCN:
                json = json_pack("{s:s}", TUNE_TYPE_VCN, input);
                break;
            case FREQ_PROG:
                json = json_pack("{s:s}", TUNE_TYPE_FREQ_PROG, input);
                break;
            default:
                break;
            }

            if (json && json_is_object(json))
            {
                param = json_dumps(json, JSON_ENCODE_ANY);
            }

            if (!param)
            {
                RDK_LOG(RDK_LOG_ERROR, log_module,"[%s:%s] failed to generate tune params for tuner test execution\n", FILE_CPP, __FUNCTION__);
                return true;
            }

            if (strcmp(param, "") == 0)
            {
                RDK_LOG(RDK_LOG_ERROR, log_module,"[%s:%s] Empty tune params received\n", FILE_CPP, __FUNCTION__);
                stMsgData->faultCode = fcInvalidParameterValue;
                free(param);
                return true;
            }
            ret = pInst->execute_tune_test(false /* execution from tr69 */, param);

            if (ret)
            {
                RDK_LOG(RDK_LOG_DEBUG, log_module, "[%s:%s] tuner test scheduled successfully\n", FILE_CPP, __FUNCTION__);
                stMsgData->faultCode = fcNoFault;
            }
            else
                RDK_LOG(RDK_LOG_ERROR, log_module,"[%s:%s] failed to schedule tuner test execution\n", FILE_CPP, __FUNCTION__);

            free(param);
        }

    }
    else
        RDK_LOG(RDK_LOG_ERROR, log_module,"[%s:%s] failed to get wa_wsclient instance\n", FILE_CPP, __FUNCTION__);

    return true;
}

bool get_Device_DeviceInfo_xOpsDeviceMgmt_hwHealthTestTune_TuneResults(const char *log_module, HOSTIF_MsgData_t *stMsgData)
{
    bool ret = false;

    RDK_LOG(RDK_LOG_DEBUG, log_module, "[%s:%s] attempting to retrieve tune test results...\n", FILE_CPP, __FUNCTION__);

    if(stMsgData == NULL)
    {
        RDK_LOG(RDK_LOG_ERROR, log_module,"[%s:%s] stMsgData is null pointer!\n", FILE_CPP, __FUNCTION__);
        return ret;
    }

    create_emptyStrResponse(stMsgData);
    stMsgData->faultCode = fcInternalError;

    wa_wsclient *pInst = wa_wsclient::instance();
    if (pInst)
    {
        if(!pInst->is_enabled())
        {
            RDK_LOG(RDK_LOG_DEBUG, log_module, "[%s:%s] tune test results not read, feature disabled\n", FILE_CPP, __FUNCTION__);
            pInst->log("Feature disabled. Tune Results request ignored.\n");
            stMsgData->faultCode = fcNoFault;
        }
        else
        {
            char read_results[MAX_LEN];
            char results_data[MAX_LEN] = {'\0'};
            sprintf(read_results, "cat %s 2>/dev/null", TUNE_RESULTS_FILE);
            FILE *p = popen(read_results, "r");

            if (p != NULL)
            {
                while(fgets(results_data, MAX_LEN, p) != NULL && results_data[0] != '\n')
                {
                    RDK_LOG(RDK_LOG_DEBUG, log_module, "[%s:%s] tune test results retrieved successfully\n", FILE_CPP, __FUNCTION__);
                }
                pclose(p);
            }

            if (!strcmp(results_data, ""))
            {
                strcpy(results_data, "RESULTS_NOT_AVAILABLE");
                RDK_LOG(RDK_LOG_ERROR,log_module,"[%s:%s] failed to retrieve tune test results\n", FILE_CPP, __FUNCTION__);
            }
            strncpy(stMsgData->paramValue, results_data, TR69HOSTIFMGR_MAX_PARAM_LEN - 1)[TR69HOSTIFMGR_MAX_PARAM_LEN - 1] = '\0';
            stMsgData->paramLen = (strlen(results_data) > TR69HOSTIFMGR_MAX_PARAM_LEN - 1? TR69HOSTIFMGR_MAX_PARAM_LEN - 1 : strlen(results_data)) + 1;
            stMsgData->faultCode = fcNoFault;
        }
    }
    else
        RDK_LOG(RDK_LOG_ERROR, log_module,"[%s:%s] failed to get wa_wsclient instance\n", FILE_CPP, __FUNCTION__);

    return true;
}

bool set_Device_DeviceInfo_xOpsDeviceMgmt_hwHealthTest_EnablePeriodicRun(const char *log_module, HOSTIF_MsgData_t *stMsgData)
{
    bool ret = false;

    RDK_LOG(RDK_LOG_DEBUG, log_module, "[%s:%s] attempting to enable/disable the hwselftest periodic run...\n", FILE_CPP, __FUNCTION__);

    if(stMsgData == NULL)
    {
        RDK_LOG(RDK_LOG_ERROR, log_module,"[%s:%s] stMsgData is null pointer!\n", FILE_CPP, __FUNCTION__);
        return ret;
    }

    bool value = *(reinterpret_cast<bool *>(stMsgData->paramValue));

    create_emptyStrResponse(stMsgData);
    stMsgData->faultCode = fcInternalError;

    wa_wsclient *pInst = wa_wsclient::instance();
    if (pInst)
    {
        if(!pInst->is_enabled())
        {
            RDK_LOG(RDK_LOG_DEBUG, log_module, "[%s:%s] hwselftest periodic run not %s, feature disabled\n", FILE_CPP, __FUNCTION__, (value? "enabled" : "disabled"));
            pInst->log("Feature disabled. EnablePeriodicRun request ignored.\n");
            stMsgData->faultCode = fcNoFault;
        }
        else
        {
            ret = pInst->enable_periodic(value);

            if (ret)
            {
                RDK_LOG(RDK_LOG_DEBUG, log_module, "[%s:%s] hwselftest periodic run %s request processed successfully\n", FILE_CPP, __FUNCTION__, (value? "enable" : "disable"));
                stMsgData->faultCode = fcNoFault;
            }
            else
                RDK_LOG(RDK_LOG_ERROR, log_module,"[%s:%s] failed to %s hwselftest periodic run\n", FILE_CPP, __FUNCTION__, (value? "enable" : "disable"));
        }
    }
    else
        RDK_LOG(RDK_LOG_ERROR, log_module,"[%s:%s] failed to get wa_wsclient instance\n", FILE_CPP, __FUNCTION__);

    return true;
}

bool set_Device_DeviceInfo_xOpsDeviceMgmt_hwHealthTest_PeriodicRunFrequency(const char *log_module, HOSTIF_MsgData_t *stMsgData)
{
    bool ret = false;

    RDK_LOG(RDK_LOG_DEBUG, log_module, "[%s:%s] attempting to set the hwselftest periodic run frequency...\n", FILE_CPP, __FUNCTION__);

    if(stMsgData == NULL)
    {
        RDK_LOG(RDK_LOG_ERROR, log_module,"[%s:%s] stMsgData is null pointer!\n", FILE_CPP, __FUNCTION__);
        return ret;
    }

    unsigned int value = *(reinterpret_cast<unsigned int *>(stMsgData->paramValue));

    create_emptyStrResponse(stMsgData);
    stMsgData->faultCode = fcInternalError;

    wa_wsclient *pInst = wa_wsclient::instance();
    if (pInst)
    {
        if(!pInst->is_enabled())
        {
            RDK_LOG(RDK_LOG_DEBUG, log_module, "[%s:%s] periodic run frequency not set, feature disabled\n", FILE_CPP, __FUNCTION__);
            pInst->log("Feature disabled. PeriodicRunFrequency request ignored.\n");
            stMsgData->faultCode = fcNoFault;
        }
        else
        {
            bool invalidParam = false;

            ret = pInst->set_periodic_frequency(&invalidParam, value);

            if (ret)
            {
                RDK_LOG(RDK_LOG_DEBUG, log_module, "[%s:%s] hwselftest periodic run frequency set to %i\n", FILE_CPP, __FUNCTION__, value);
                stMsgData->faultCode = fcNoFault;
            }
            else
            {
                if(invalidParam)
                {
                    RDK_LOG(RDK_LOG_DEBUG, log_module, "[%s:%s] periodic run frequency not set, invalid parameter value '%i'\n", FILE_CPP, __FUNCTION__, value);
                    stMsgData->faultCode = fcInvalidParameterValue;
                }
                else
                    RDK_LOG(RDK_LOG_ERROR, log_module,"[%s:%s] failed to set hwselftest periodic run frequency to %i\n", FILE_CPP, __FUNCTION__, value);
            }
        }
    }
    else
        RDK_LOG(RDK_LOG_ERROR, log_module,"[%s:%s] failed to get wa_wsclient instance\n", FILE_CPP, __FUNCTION__);

    return true;
}

bool set_Device_DeviceInfo_xOpsDeviceMgmt_hwHealthTest_CpuThreshold(const char *log_module, HOSTIF_MsgData_t *stMsgData)
{
    bool ret = false;

    RDK_LOG(RDK_LOG_DEBUG, log_module, "[%s:%s] attempting to set hwselftest CPU threshold...\n", FILE_CPP, __FUNCTION__);

    if(stMsgData == NULL)
    {
        RDK_LOG(RDK_LOG_ERROR, log_module,"[%s:%s] stMsgData is null pointer!\n", FILE_CPP, __FUNCTION__);
        return ret;
    }

    unsigned int value = *(reinterpret_cast<unsigned int *>(stMsgData->paramValue));

    create_emptyStrResponse(stMsgData);
    stMsgData->faultCode = fcInternalError;

    wa_wsclient *pInst = wa_wsclient::instance();
    if (pInst)
    {
        if(!pInst->is_enabled())
        {
            RDK_LOG(RDK_LOG_DEBUG, log_module, "[%s:%s] hwselftest CPU threshold not set, feature disabled\n", FILE_CPP, __FUNCTION__, value);
            pInst->log("Feature disabled. cpuThreshold request ignored.\n");
            stMsgData->faultCode = fcNoFault;
        }
        else
        {
            bool invalidParam = false;

            ret = pInst->set_periodic_cpu_threshold(&invalidParam, value);

            if (ret)
            {
                RDK_LOG(RDK_LOG_DEBUG, log_module, "[%s:%s] hwselftest CPU threshold set to %i\n", FILE_CPP, __FUNCTION__, value);
                stMsgData->faultCode = fcNoFault;
            }
            else
            {
                if(invalidParam)
                {
                    RDK_LOG(RDK_LOG_DEBUG, log_module, "[%s:%s] hwselftest CPU threshold not set, invalid parameter value '%i'\n",FILE_CPP, __FUNCTION__, value);
                    stMsgData->faultCode = fcInvalidParameterValue;
                }
                else
                    RDK_LOG(RDK_LOG_ERROR, log_module,"[%s:%s] failed to set CPU threshold to %i\n", FILE_CPP, __FUNCTION__, value);
            }
        }
    }
    else
        RDK_LOG(RDK_LOG_ERROR, log_module,"[%s:%s] failed to get wa_wsclient instance\n", FILE_CPP, __FUNCTION__);

    return true;
}

bool set_Device_DeviceInfo_xOpsDeviceMgmt_hwHealthTest_DramThreshold(const char *log_module, HOSTIF_MsgData_t *stMsgData)
{
    bool ret = false;

    RDK_LOG(RDK_LOG_DEBUG, log_module, "[%s:%s] attempting to set hwselftest DRAM threshold...\n", FILE_CPP, __FUNCTION__);

    if(stMsgData == NULL)
    {
        RDK_LOG(RDK_LOG_ERROR, log_module,"[%s:%s] stMsgData is null pointer!\n", FILE_CPP, __FUNCTION__);
        return ret;
    }

    unsigned int value = *(reinterpret_cast<unsigned int *>(stMsgData->paramValue));

    create_emptyStrResponse(stMsgData);
    stMsgData->faultCode = fcInternalError;

    wa_wsclient *pInst = wa_wsclient::instance();
    if (pInst)
    {
        if(!pInst->is_enabled())
        {
            RDK_LOG(RDK_LOG_DEBUG, log_module, "[%s:%s] hwselftest DRAM threshold not set, feature disabled\n", FILE_CPP, __FUNCTION__, value);
            pInst->log("Feature disabled. dramThreshold request ignored.\n");
            stMsgData->faultCode = fcNoFault;
        }
        else
        {
            ret = pInst->set_periodic_dram_threshold(value);

            if (ret)
            {
                RDK_LOG(RDK_LOG_DEBUG, log_module, "[%s:%s] hwselftest DRAM threshold set to %i\n", FILE_CPP, __FUNCTION__, value);
                stMsgData->faultCode = fcNoFault;
            }
            else
                RDK_LOG(RDK_LOG_ERROR, log_module,"[%s:%s] failed to set DRAM threshold to %i\n", FILE_CPP, __FUNCTION__, value);
         }
    }
    else
        RDK_LOG(RDK_LOG_ERROR, log_module,"[%s:%s] failed to get wa_wsclient instance\n", FILE_CPP, __FUNCTION__);

    return true;
}

bool set_Device_DeviceInfo_RFC_hwHealthTestWAN_WANEndPointURL(const char *log_module, HOSTIF_MsgData_t *stMsgData)
{
    bool ret = false;

    RDK_LOG(RDK_LOG_DEBUG, log_module, "[%s:%s] attempting to set WAN test URL for hwselftest...\n", FILE_CPP, __FUNCTION__);

    if(stMsgData == NULL)
    {
        RDK_LOG(RDK_LOG_ERROR, log_module,"[%s:%s] stMsgData is null pointer!\n", FILE_CPP, __FUNCTION__);
        return ret;
    }

    char URL[256] = {'\0'};
    strcpy(URL, stMsgData->paramValue);

    create_emptyStrResponse(stMsgData);

    RDK_LOG(RDK_LOG_DEBUG, log_module, "[%s:%s] hwselftest WAN test case URL set to %s\n", FILE_CPP, __FUNCTION__, URL);
    stMsgData->faultCode = fcNoFault;

    return true;
}

bool set_Device_DeviceInfo_xRDKCentralComRFC_hwHealthTest_ResultFilter_Enable(const char *log_module, HOSTIF_MsgData_t *stMsgData)
{
    bool ret = false;

    RDK_LOG(RDK_LOG_DEBUG, log_module, "[%s:%s] attempting to enable/disable the hwselftest result-filter feature...\n", FILE_CPP, __FUNCTION__);

    if(stMsgData == NULL)
    {
        RDK_LOG(RDK_LOG_ERROR, log_module,"[%s:%s] stMsgData is null pointer!\n", FILE_CPP, __FUNCTION__);
        return ret;
    }

    bool value = *(reinterpret_cast<bool *>(stMsgData->paramValue));
    wa_wsclient *pInst = wa_wsclient::instance();

    struct stat buffer;
    if (stat(FILTER_FILES_PATH, &buffer) != 0)
        mkdir(FILTER_FILES_PATH, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

    if (value)
    {
        int fd = open(FILTER_ENABLE_FILE, O_RDWR | O_CREAT, S_IRUSR | S_IRGRP | S_IROTH);
        if (fd != -1)
        {
            close(fd);
            ret = (stat(FILTER_ENABLE_FILE, &buffer) == 0);
        }
    }
    else
    {
        if(stat(FILTER_ENABLE_FILE, &buffer) == 0)  /* Delete file if present */
            ret = (unlink(FILTER_ENABLE_FILE) == 0);
        else
            ret = true;
        unlink(FILTER_BUFFER_FILE);
    }

    if (ret)
    {
        pInst->log("Filter feature " + (value ? std::string ("enabled") : std::string ("disabled")) + ".\n");
        RDK_LOG(RDK_LOG_DEBUG, log_module, "[%s:%s] hwselftest result-filter %s successfully\n", FILE_CPP, __FUNCTION__, (value? "enabled" : "disabled"));
    }
    else
        RDK_LOG(RDK_LOG_ERROR, log_module,"[%s:%s] failed to %s hwselftest result-filter\n", FILE_CPP, __FUNCTION__, (value? "enable" : "disable"));

    filterTelemetry();
    return ret;
}

bool set_Device_DeviceInfo_xRDKCentralComRFC_hwHealthTest_ResultFilter_QueueDepth(const char *log_module, HOSTIF_MsgData_t *stMsgData)
{
    bool ret = false;

    RDK_LOG(RDK_LOG_DEBUG, log_module, "[%s:%s] attempting to set hwselftest queue depth for result-filter...\n", FILE_CPP, __FUNCTION__);

    if(stMsgData == NULL)
    {
        RDK_LOG(RDK_LOG_ERROR, log_module,"[%s:%s] stMsgData is null pointer!\n", FILE_CPP, __FUNCTION__);
        return ret;
    }

    unsigned int value = *(reinterpret_cast<unsigned int *>(stMsgData->paramValue));

    create_emptyStrResponse(stMsgData);
    stMsgData->faultCode = fcInternalError;

    wa_wsclient *pInst = wa_wsclient::instance();

    struct stat buffer;
    if (stat(FILTER_FILES_PATH, &buffer) != 0)
        mkdir(FILTER_FILES_PATH, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

    std::ofstream file(FILTER_QUEUE_DEPTH_FILE, std::ios::trunc);
    if (file.is_open())
    {
        if (value > FILTER_QUEUE_DEPTH_MAX)
        {
            pInst->log("Queue depth " + std::to_string(value) + " is exceeding max value 100. Setting to the default max threshold.\n");
            value = FILTER_QUEUE_DEPTH_MAX;
        }
        if (value <= 0)
        {
            pInst->log("Queue depth " + std::to_string(value) + " is invalid. Setting to a default value 20.\n");
            value = FILTER_QUEUE_DEPTH_DEFAULT;
        }
        file << value <<'\n';
        file.flush();
    }
    pInst->log("Filter feature QueueDepth set to " + std::to_string(value) + ".\n");

    filterTelemetry();
    return true;
}

bool set_Device_DeviceInfo_xRDKCentralComRFC_hwHealthTest_ResultFilter_FilterParams(const char *log_module, HOSTIF_MsgData_t *stMsgData)
{
    bool ret = false;

    RDK_LOG(RDK_LOG_DEBUG, log_module, "[%s:%s] attempting to set hwselftest FilterParams for result-filter...\n", FILE_CPP, __FUNCTION__);

    if(stMsgData == NULL)
    {
        RDK_LOG(RDK_LOG_ERROR, log_module,"[%s:%s] stMsgData is null pointer!\n", FILE_CPP, __FUNCTION__);
        return ret;
    }

    wa_wsclient *pInst = wa_wsclient::instance();

    std::string filter_params = stMsgData->paramValue;
    pInst->log("Filter parameters given : " + filter_params + ".\n");

    /* Get the queue depth value */
    std::string queueDepth;
    std::ifstream depth(FILTER_QUEUE_DEPTH_FILE);
    getline(depth, queueDepth);
    queueDepth = (queueDepth.compare("") == 0) ? "0" : queueDepth;
    int qd = atoi(queueDepth.c_str());

    std::string filterParams_final = "";
    std::istringstream tokenstream(filter_params);
    std::string param;
    int i = 1;

    while (std::getline(tokenstream, param, ',') && i <= NUM_ELEMENTS)
    {
        /* Remove additional spaces from params */
        param.erase(remove_if(param.begin(), param.end(), isspace), param.end());

        int pos = -1;
        if (((pos = param.find("p")) != -1) || ((pos = param.find("P")) != -1))
        {
            std::string val = param.substr(pos+1);

            /* Check if Percentage filter value is valid */
            int per = atoi(val.c_str());
            if ((per > 100) || (per <= 0)) {
                param.replace(0,param.length(),"N");
                pInst->log("The percentage filter value for component " + std::to_string(i) + " is invalid. Skipping filter for it.\n");
            }
            else {
                std::string percentage_filter = "P" + std::to_string(per);
                param.replace(0,param.length(),percentage_filter);
            }
        }
        else if (((pos = param.find("s")) != -1) || ((pos = param.find("S")) != -1))
        {
            std::string val = param.substr(pos+1);

            /* Check if Sequence filter value is valid */
            int seq = atoi(val.c_str());
            if ((qd != 0)  && (seq > qd)) {
                std::string sequence_filter = "S" + std::to_string(qd);
                param.replace(0,param.length(),sequence_filter);
                pInst->log("The sequence filter value for component " + std::to_string(i) + " is greater than queueDepth. Hence defaulting it to queueDepth " + queueDepth + ".\n");
            }
            else if (seq <= 0)
            {
                param.replace(0,param.length(),"N");
                pInst->log("The sequence filter value for component " + std::to_string(i) + " is invalid. Skipping filter for it.\n");
            }
            else {
                std::string sequence_filter = "S" + std::to_string(seq);
                param.replace(0,param.length(),sequence_filter);
            }
        }
        else if (((pos = param.find("n")) != -1) || ((pos = param.find("N")) != -1))
        {
            param.replace(0,param.length(),"N");
        }
        else
        {
            param.replace(0,param.length(),"N");
            pInst->log("The filter value for component " + std::to_string(i) + " is invalid. Skipping filter for it.\n");
        }

        if (i == NUM_ELEMENTS)
            filterParams_final += param;
        else
            filterParams_final += param + ",";

        i++;
    }

    /* Set "N" for components, if filter params are not provided */
    if (i <= NUM_ELEMENTS)
    {
        while (i <= NUM_ELEMENTS)
        {
            if (i == NUM_ELEMENTS)
                filterParams_final += "N";
            else
                filterParams_final += "N" + std::string(",");
            i++;
        }
    }

    create_emptyStrResponse(stMsgData);
    stMsgData->faultCode = fcInternalError;

    pInst->log("HwTestFilterParams: " + filterParams_final + ".\n");
    t2_event_s("hwtestFilterParams_split", (char*)filterParams_final.c_str());

    return true;
}

bool set_Device_DeviceInfo_xRDKCentralComRFC_hwHealthTest_ResultFilter_ResultsFiltered(const char *log_module, HOSTIF_MsgData_t *stMsgData)
{
    bool ret = false;

    RDK_LOG(RDK_LOG_DEBUG, log_module, "[%s:%s] attempting to set hwselftest ResultsFiltered result-filter feature...\n", FILE_CPP, __FUNCTION__);

    if(stMsgData == NULL)
    {
        RDK_LOG(RDK_LOG_ERROR, log_module,"[%s:%s] stMsgData is null pointer!\n", FILE_CPP, __FUNCTION__);
        return ret;
    }

    unsigned int value = *(reinterpret_cast<unsigned int *>(stMsgData->paramValue));

    create_emptyStrResponse(stMsgData);
    stMsgData->faultCode = fcInternalError;

    wa_wsclient *pInst = wa_wsclient::instance();
    struct stat buffer;
    if (stat(FILTER_FILES_PATH, &buffer) != 0)
        mkdir(FILTER_FILES_PATH, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

    if (value)
    {
        int fd = open(RESULTS_FILTERED_ENABLE_FILE, O_RDWR | O_CREAT, S_IRUSR | S_IRGRP | S_IROTH);
        if (fd != -1)
        {
            close(fd);
            ret = (stat(RESULTS_FILTERED_ENABLE_FILE, &buffer) == 0);
        }
    }
    else
    {
        if(stat(RESULTS_FILTERED_ENABLE_FILE, &buffer) == 0)  /* Delete file if present */
            ret = (unlink(RESULTS_FILTERED_ENABLE_FILE) == 0);
        else
            ret = true;
    }

    if (ret)
    {
        pInst->log("Filter feature ResultsFiltered " + (value ? std::string ("enabled") : std::string ("disabled")) + ".\n");
        RDK_LOG(RDK_LOG_DEBUG, log_module, "[%s:%s] hwselftest ResultsFiltered parameter %s successfully\n", FILE_CPP, __FUNCTION__, (value? "enabled" : "disabled"));
    }
    else
        RDK_LOG(RDK_LOG_ERROR, log_module,"[%s:%s] failed to %s hwselftest ResultsFiltered parameter\n", FILE_CPP, __FUNCTION__, (value? "enable" : "disable"));

    filterTelemetry();
    return ret;
}

void filterTelemetry()
{
    int value;
    struct stat buffer;

    value = stat(FILTER_ENABLE_FILE, &buffer);
    std::string Enable = ((value == 0) ? std::string("true") : std::string("false"));

    std::string queueDepth;
    std::ifstream depth(FILTER_QUEUE_DEPTH_FILE);
    getline(depth, queueDepth);
    queueDepth = (queueDepth.compare("") == 0) ? "nil" : queueDepth;

    value = stat(RESULTS_FILTERED_ENABLE_FILE, &buffer);
    std::string resultsFilterd = ((value == 0) ? std::string("true") : std::string("false"));

    wa_wsclient *pInst = wa_wsclient::instance();
    std::string filterControlTelemetry = Enable + "," + queueDepth + "," + resultsFilterd;
    pInst->log("HwTestRFCFilterControl: " + filterControlTelemetry + "\n");
    t2_event_s("hwtestRFCFilterControl_split", (char*)filterControlTelemetry.c_str());
}
} // namespace hwselftest
