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

#ifndef _HWST_SCENARIO_ALL_
#define _HWST_SCENARIO_ALL_

#include <string>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "hwst_scenario.hpp"

namespace hwst {

class ScenarioAll: public Scenario
{
public:
    ScenarioAll();
    ~ScenarioAll();
    virtual bool init(std::string options);
};

} // namespace hwst

#endif // _HWST_SCENARIO_ALL_
