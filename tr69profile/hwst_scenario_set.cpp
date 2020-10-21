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

#include "hwst_diag.hpp"
#include "hwst_diagfactory.hpp"
#include "hwst_scenario_set.hpp"

//#define HWST_DEBUG 1
#ifdef HWST_DEBUG
#include <unistd.h>
#define HWST_DBG(str) do {std::cout << "HWST_DBG |" << str << std::endl;} while(false)
#else
#define HWST_DBG(str) do {} while(false)
#endif

using hwst::Diag;

namespace hwst {

ScenarioSet::ScenarioSet()
{
    HWST_DBG("ScenarioSet");
}

ScenarioSet::~ScenarioSet()
{
    HWST_DBG("~ScenarioSet");
}

bool ScenarioSet::init(const std::string& client, const std::vector<std::string>& diags, const std::string& param)
{
    /* create list of elements */
    for (auto diag : diags)
    {
        int elem = addElement(DiagFactory::create(diag, param));
        if (elem >= 0)
            HWST_DBG("added " + diag + " as: " + std::to_string(elem));
        else
            HWST_DBG("error adding " + diag);
    }

    /* create dependencies */
    int av = getElement("avdecoder_qam_status");
    int tuner = getElement("tuner_status");

    if ((av != -1) && (tuner != -1))
    {
        if (!addDependency(tuner, av))
            return false;

        HWST_DBG("added dependency: tuner after av");
    }

    return true;
}

} // namespace hwst
