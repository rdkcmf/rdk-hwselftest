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

#include "hwst_diag_prev_results.hpp"
#include "hwst_comm.hpp"
#include "hwst_log.hpp"

#include "hwst_scenario.hpp"
#include "hwst_scenario_all.hpp"

#include "jansson.h"

//#define HWST_DEBUG 1
#ifdef HWST_DEBUG
#include <unistd.h>
#define HWST_DBG(str, ...) do { printf("HWST_DBG |" str, ##__VA_ARGS__); } while(0)
#else
#define HWST_DBG(str, ...) ((void)0)
#endif

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

        results = start_time;
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
                        e.diag->status.status = diag_status;
                        results += timestamp;
                        results += " " + e.diag->getStrStatus() + "\n";
                        if (!e.diag->getPresentationResult().compare("FAILED"))
                            passed = false;
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

} // namespace hwst
