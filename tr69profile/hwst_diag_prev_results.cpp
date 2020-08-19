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

#include <iostream>
#include <memory>
#include <sstream>
#include <string.h>

#include "hwst_diag_prev_results.hpp"
#include "hwst_comm.hpp"
#include "hwst_log.hpp"

#include "hwst_scenario.hpp"
#include "hwst_scenario_all.hpp"

#include "jansson.h"
#include "sysMgr.h"
#include "xdiscovery.h"
#include "libIBus.h"
#include "libIARMCore.h"
#include "hostIf_tr69ReqHandler.h"

//#define HWST_DEBUG 1
#ifdef HWST_DEBUG
#include <unistd.h>
#define HWST_DBG(str, ...) do { printf("HWST_DBG |" str, ##__VA_ARGS__); } while(0)
#else
#define HWST_DBG(str, ...) ((void)0)
#endif
#define DEFAULT_RESULT_VALUE -200
#define BUFFER_LENGTH      512
#define MESSAGE_LENGTH     8192 * 4 /* On reference from xdiscovery.log which shows data length can be more than 5000 */ /* Increased the value 4 times because of DELIA-38611 */

std::string timeZoneInfo;

namespace hwst {

DiagPrevResults::DiagPrevResults(const std::string &params_):
    Diag("previous_results", params_)
{
}

std::string DiagPrevResults::getPresentationName() const
{
    return "Previous Results";
};

std::string DiagPrevResults::getStrStatus() const
{
    bool status = false;
    std::string results;

    /* any error or missing diag results will discard all data */
    json_t *jresults = NULL;
    json_t *jroot = json_loads(this->status.data.c_str(), JSON_DISABLE_EOF_CHECK, NULL);
    if (jroot)
    {
        int valid = 1;
        bool passed = true;
        char *start_time = NULL;
        char *end_time = NULL;
        char *client = NULL;

        std::unique_ptr<Scenario> scenario = std::unique_ptr<Scenario>(new ScenarioAll());
        scenario->init();

        if (json_unpack(jroot, "{s:i}", "results_valid", &valid) || valid)
        {
            HWST_DBG("json: results not available\n");
            goto end;
        }

        if (json_unpack(jroot, "{s:s}", "client", &client) || !client)
        {
            HWST_DBG("json: no client\n");
            goto end;
        }

        if (json_unpack(jroot, "{s:s}", "start_time", &start_time) || !start_time)
        {
            HWST_DBG("json: no start date\n");
            goto end;
        }

        if (json_unpack(jroot, "{s:s}", "end_time", &end_time) || !end_time)
        {
            HWST_DBG("json: no end date\n");
            goto end;
        }

        if (localTimeZone())
        {
            results = timeZoneInfo + "\n";
        }
        else
        {
            results = std::string("Timezone: NA") + "\n";
        }

        results += start_time;
        results += " Test execution start, " + std::string(client) + "\n";

        if (!json_unpack(jroot, "{s:o}", "results", &jresults) && jresults)
        {
            for (auto const& e : scenario->elements)
            {
                json_t *jdiag = NULL;
                if (!json_unpack(jresults, "{s:o}", e.diag->getName().c_str(), &jdiag) && jdiag)
                {
                    int diag_status = -1;
                    char *timestamp = NULL;
                    if (!json_unpack(jdiag, "{s:s,s:i}", "timestamp", &timestamp, "result", &diag_status))
                    {
                        if (diag_status != DEFAULT_RESULT_VALUE)
                        {
                            e.diag->status.status = diag_status;
                            results += timestamp;
                            results += " " + e.diag->getStrStatus() + "\n";
                            if (!e.diag->getPresentationResult().compare("FAILED"))
                                passed = false;
                        }
                    }
                    else
                    {
                        HWST_DBG("json: invalid results for diag %s\n", e.diag->getName().c_str());
                        break;
                    }
                }
            }

            status = true;
        }
        else
            HWST_DBG("json: no results\n");

        results += end_time;
        results += " Test execution completed:" + std::string(passed? "PASSED" : "FAILED");

end:
        json_decref(jroot);
    }
    else
        HWST_DBG("json: root unpack error");

    return (status? results : "");
}

bool DiagPrevResults::localTimeZone() const
{
    IARM_Result_t iarm_result = IARM_RESULT_IPCCORE_FAIL;
    int is_connected = 0;

    IARM_Bus_SYSMGR_GetXUPNPDeviceInfo_Param_t *param = NULL;
    char upnpResults[MESSAGE_LENGTH+1];
    json_error_t jerror;
    json_t *json = NULL;
    char param_TZ[BUFFER_LENGTH];

    iarm_result = IARM_Bus_IsConnected(_IARM_XUPNP_NAME, &is_connected);
    if (iarm_result != IARM_RESULT_SUCCESS)
    {
        HWST_DBG("localTimeZone(): IARM_Bus_IsConnected('%s') failed\n", _IARM_XUPNP_NAME);
        return false;
    }

    if (!is_connected)
    {
        HWST_DBG("localTimeZone(): XUPNP not available\n");
        return false;
    }

    iarm_result = IARM_Malloc(IARM_MEMTYPE_PROCESSLOCAL, sizeof(IARM_Bus_SYSMGR_GetXUPNPDeviceInfo_Param_t) + MESSAGE_LENGTH + 1, (void**)&param);
    if (iarm_result != IARM_RESULT_SUCCESS)
    {
        HWST_DBG("localTimeZone(): IARM_Malloc() failed\n");
        return false;
    }

    if (param)
    {
        memset(param, 0, sizeof(*param));
        param->bufLength = MESSAGE_LENGTH;

        iarm_result = IARM_Bus_Call(_IARM_XUPNP_NAME, IARM_BUS_XUPNP_API_GetXUPNPDeviceInfo, (void *)param, sizeof(IARM_Bus_SYSMGR_GetXUPNPDeviceInfo_Param_t) + MESSAGE_LENGTH + 1);

        if (iarm_result != IARM_RESULT_SUCCESS)
        {
            HWST_DBG("localTimeZone(): IARM_Bus_Call() returned %i\n", iarm_result);
            IARM_Free(IARM_MEMTYPE_PROCESSLOCAL, param);
            return false;
        }

        HWST_DBG("localTimeZone(): IARM_RESULT_SUCCESS\n");
        memcpy(upnpResults, ((char *)param + sizeof(IARM_Bus_SYSMGR_GetXUPNPDeviceInfo_Param_t)), param->bufLength);
        upnpResults[param->bufLength] = '\0';

        json = json_loadb(upnpResults, MESSAGE_LENGTH, JSON_DISABLE_EOF_CHECK, &jerror);
        if (!json)
        {
            HWST_DBG("localTimeZone(): json_loads() error\n");
            json_decref(json);
            IARM_Free(IARM_MEMTYPE_PROCESSLOCAL, param);
            return false;
        }

        json_t *jarr = json_object_get(json, "xmediagateways");
        if (!jarr)
        {
            HWST_DBG("localTimeZone(): Couldn't get the array of xmediagateways\n");
            json_decref(jarr);
            json_decref(json);
            IARM_Free(IARM_MEMTYPE_PROCESSLOCAL, param);
            return false;
        }

        for (size_t i = 0; i < json_array_size(jarr) && !strcmp(param_TZ,""); i++)
        {
            json_t *jval = json_array_get(jarr, i);
            if (!jval)
            {
                HWST_DBG("localTimeZone(): Not a json object in array for %i element\n", i);
                json_decref(jval);
                json_decref(jarr);
                json_decref(json);
                IARM_Free(IARM_MEMTYPE_PROCESSLOCAL, param);
                return false;
            }

            json_t *jparam_timezone = json_object_get(jval, "timezone");

            /* If timezone is not empty */
            if (jparam_timezone)
            {
                json_t *jparam_rawoffset = json_object_get(jval, "rawoffset");

                /* If rawoffset is not empty */
                if (jparam_rawoffset)
                {
                    long rawOffset = 0;
                    long dstOffset = 0;
                    rawOffset = atoi(json_string_value(jparam_rawoffset));
                    json_t *jparam_dstNeed = json_object_get(jval, "usesdaylighttime");

                    /* If DST is applicable */
                    if (jparam_dstNeed)
                    {
                        if (!strcmp(json_string_value(jparam_dstNeed), "yes"));
                        {
                            json_t *jparam_dstOffset = json_object_get(jval, "dstsavings");
                            /* If DST offset is not empty */
                            if (jparam_dstOffset)
                            {
                                dstOffset = atoi(json_string_value(jparam_dstOffset));
                            }

                            json_decref(jparam_dstOffset);
                        }
                    }

                    json_decref(jparam_dstNeed);

                    long offset_millisec = rawOffset + dstOffset;
                    /* 3600000 milliseconds in an hour */
                    long offset_hr = offset_millisec / 3600000;
                    offset_millisec = offset_millisec - (3600000 * offset_hr);
                    /* 60000 milliseconds in a minute */
                    long offset_min = offset_millisec / 60000;

                    if (offset_min != 0)
                        sprintf(param_TZ, "%i:%i", offset_hr, offset_min);
                    else
                        sprintf(param_TZ, "%i", offset_hr);

                    HWST_DBG("localTimeZone(): param_TZ = %s\n", param_TZ);
                }

                json_decref(jparam_rawoffset);
            }

            json_decref(jparam_timezone);
        }
    }

    timeZoneInfo = "Timezone: UTC " + std::string(param_TZ);
    HWST_DBG("localTimeZone(): timeZoneInfo = %s\n", timeZoneInfo);

    IARM_Free(IARM_MEMTYPE_PROCESSLOCAL, param);

    return true;
}

} // namespace hwst
