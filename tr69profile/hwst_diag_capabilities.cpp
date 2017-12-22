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

#include "hwst_diag_capabilities.hpp"
#include "hwst_comm.hpp"
#include "hwst_log.hpp"

#include "hwst_scenario.hpp"
#include "hwst_scenario_set.hpp"

#include "jansson.h"

//#define HWST_DEBUG 1
#ifdef HWST_DEBUG
#include <unistd.h>
#define HWST_DBG(str, ...) do { printf(str, ##__VA_ARGS__); } while(0)
#else
#define HWST_DBG(str, ...) ((void)0)
#endif

namespace hwst {

DiagCapabilities::DiagCapabilities(const std::string &params_):
    Diag("capabilities_info", params_)
{
}

std::string DiagCapabilities::getPresentationName() const
{
    return "Capabilities";
};

std::string DiagCapabilities::getStrStatus() const
{
    bool status = false;
    std::string caps;

    /* any error or missing diag results will discard all data */
    json_t *jroot = json_loads(this->status.data.c_str(), JSON_DISABLE_EOF_CHECK, NULL);
    if (jroot)
    {
        json_t *jdiags = json_object_get(jroot, "diags");
        if (jdiags)
        {
            for (size_t i = 0; i < json_array_size(jdiags); i++)
            {
                json_t *jval = json_array_get(jdiags, i);
                if (jval)
                {
                    const char *val = json_string_value(jval);
                    if (val)
                    {
                        caps += val;
                        caps += " ";
                    }
                }
            }

            status = true;
            HWST_DBG("json: diags found: %s\n", caps.c_str());
        }

end:
        json_decref(jroot);
    }
    else
        HWST_DBG("json: root unpack error\n");

    return (status? caps : "");
}

} // namespace hwst
