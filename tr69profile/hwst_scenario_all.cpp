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
#include "hwst_scenario_all.hpp"

#ifdef HWST_DEBUG
#include <unistd.h>
#define HWST_DBG(str) do {std::cout << str << std::endl;} while(false)
#else
#define HWST_DBG(str) do {} while(false)
#endif

using hwst::Diag;

namespace hwst {

ScenarioAll::ScenarioAll()
{
    HWST_DBG("ScenarioAll");
}

ScenarioAll::~ScenarioAll()
{
    HWST_DBG("~ScenarioAll");
}

bool ScenarioAll::init(const std::vector<std::string>& diags, const std::string& param)
{
    return ScenarioSet::init({"avdecoder_qam_status", "tuner_status", "dram_status", "flash_status",
                              "hdd_status", "hdmiout_status", "ir_status", "mcard_status", "moca_status",
                              "modem_status", "bluetooth_status", "rf4ce_status", "sdcard_status" }, param);
}

} // namespace hwst
