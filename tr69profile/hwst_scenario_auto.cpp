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
#include "hwst_scenario_auto.hpp"
#include <string>
#include <sstream>

//#define HWST_DEBUG 1
#ifdef HWST_DEBUG
#include <unistd.h>
#define HWST_DBG(str) do {std::cout << str << std::endl;} while(false)
#else
#define HWST_DBG(str) do {} while(false)
#endif

using hwst::Diag;

namespace hwst {

ScenarioAuto::ScenarioAuto()
    : caps_elem(-1), caps_retrieved(false)
{
    HWST_DBG("ScenarioAuto");
}

ScenarioAuto::~ScenarioAuto()
{
    HWST_DBG("~ScenarioAuto");
}

bool ScenarioAuto::init(const std::vector<std::string>& diags, const std::string& param)
{
    caps_elem = addElement(DiagFactory::create("capabilities_info", param));
    if (caps_elem >= 0)
        HWST_DBG("added capabilities_info as: " + std::to_string(caps_elem));
    else
        HWST_DBG("error adding capabilities_info");

    // init all diags disabled and make sure capabilities is executed first
    if (ScenarioAll::init())
    {
        for (size_t i = 0; i < elements.size(); i++)
        {
            if (i != caps_elem)
            {
                elements[i].diag->setEnabled(false);
                if (!addDependency(i, caps_elem))
                    return false;
            }
        }

        return true;
    }
    else
        return false;
}

int ScenarioAuto::nextToRun(std::shared_ptr<Diag> &d)
{
    HWST_DBG("ScenarioAuto:nextToRun");
    if ((caps_elem != -1) && !caps_retrieved)
    {
        // re-enable diags that are present in capabililies info
        if (elements[caps_elem].diag->getStatus().state == Diag::finished)
        {
            std::lock_guard<std::recursive_mutex> elementsLock(elementsMutex);

            std::string item;
            std::istringstream ss(elements[caps_elem].diag->getStrStatus());
            while (getline(ss, item, ' '))
            {
                HWST_DBG("ScenarioAuto: enabling " + item);
                int e = getElement(item);
                if (e != -1)
                    elements[e].diag->setEnabled();
            }

            caps_retrieved = true;
        }
    }

    return ScenarioSet::nextToRun(d);
}

} // namespace hwst
