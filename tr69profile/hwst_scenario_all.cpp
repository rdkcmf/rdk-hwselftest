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

//#define HWST_DEBUG 1
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

bool ScenarioAll::init(std::string options)
{
    /* create list of elements */
#if 0
    int sysinfo = addElement(DiagFactory::create("sysinfo_info",""));
    if(!sysinfo < 0)
            return false;;
    HWST_DBG("added sysinfo as:" + std::to_string(sysinfo));
#else

    int hdd = addElement(DiagFactory::create("hdd_status"));
    if(!hdd < 0)
            return false;
    HWST_DBG("added hdd as:" + std::to_string(hdd));

    int flash = addElement(DiagFactory::create("flash_status"));
    if(!flash < 0)
            return false;
    HWST_DBG("added flssh as:" + std::to_string(flash));


    int hdmi = addElement(DiagFactory::create("hdmiout_status"));
    if(!hdmi < 0)
            return false;
    HWST_DBG("added hdmi as:" + std::to_string(hdmi));

    int mcard = addElement(DiagFactory::create("mcard_status"));
    if(!mcard < 0)
            return false;
    HWST_DBG("added mcard as:" + std::to_string(mcard));

    int ir = addElement(DiagFactory::create("ir_status"));
    if(!ir < 0)
            return false;
    HWST_DBG("added ir as:" + std::to_string(ir));

    int rf4ce = addElement(DiagFactory::create("rf4ce_status"));
    if(!rf4ce < 0)
            return false;
    HWST_DBG("added rf4ce as:" + std::to_string(rf4ce));

    int moca = addElement(DiagFactory::create("moca_status"));
    if(!moca < 0)
            return false;
    HWST_DBG("added moca as:" + std::to_string(moca));

    int av = addElement(DiagFactory::create("avdecoder_qam_status"));
    if(!av < 0)
            return false;
    HWST_DBG("added av as:" + std::to_string(av));

    int tuner = addElement(DiagFactory::create("tuner_status"));
    if(!tuner < 0)
            return false;
    HWST_DBG("added tuner as:" + std::to_string(tuner));

    int dram = addElement(DiagFactory::create("dram_status"));
    if(!dram < 0)
            return false;
    HWST_DBG("added dram as:" + std::to_string(dram));

    int modem = addElement(DiagFactory::create("modem_status"));
    if(!modem < 0)
            return false;
    HWST_DBG("added modem as:" + std::to_string(modem));

    /* create dependencies */
    if(!addDependency(tuner, av))
            return false;
    HWST_DBG("added dependency: tuner after av");
#endif
    return true;
}

} // namespace hwst
