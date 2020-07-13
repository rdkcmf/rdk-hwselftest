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

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/
#include "DeviceInfo_hwHealthTest.h"
#include "hostIf_tr69ReqHandler.h"
#include "wa_wsclient.h"
#include "jansson.h"
#include <rdk_debug.h>

/*****************************************************************************
 * LOCAL DEFINITIONS
 *****************************************************************************/
#define FILE_CPP "DeviceInfo_hwHealthTest.cpp"

#define TUNE_TYPE_SOURCE_ID    "SRCID"
#define TUNE_TYPE_VCN          "VCN"
#define TUNE_TYPE_FREQ_PROG    "FREQ_PROG"
#define TUNE_RESULTS_FILE      "/opt/logs/hwselftest.tuneresults"
#define MAX_LEN 256

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

} // namespace hwselftest
