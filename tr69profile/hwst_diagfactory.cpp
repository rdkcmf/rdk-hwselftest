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

#include "hwst_diagfactory.hpp"
#include "hwst_diag_sysinfo.hpp"
#include "hwst_diag_capabilities.hpp"
#include "hwst_diag_prev_results.hpp"
#include "hwst_diag_hdmiout.hpp"
#include "hwst_diag_mcard.hpp"
#include "hwst_diag_hdd.hpp"
#include "hwst_diag_moca.hpp"
#include "hwst_diag_tuner.hpp"
#include "hwst_diag_av.hpp"
#include "hwst_diag_modem.hpp"
#include "hwst_diag_flash.hpp"
#include "hwst_diag_dram.hpp"
#include "hwst_diag_ir.hpp"
#include "hwst_diag_rf4ce.hpp"

//#define HWST_DEBUG 1
#ifdef HWST_DEBUG
#define HWST_DBG(str) do {std::cout << str << std::endl;} while(false)
#else
#define HWST_DBG(str) do {} while(false)
#endif

using hwst::DiagSysinfo;

namespace hwst {

std::map<std::string, DiagFactory::creator_t> DiagFactory::pool =
{
    {"sysinfo_info", &creator<DiagSysinfo>},
    {"capabilities_info", &creator<DiagCapabilities>},
    {"previous_results", &creator<DiagPrevResults>},
    {"hdmiout_status", &creator<DiagHdmiout>},
    {"mcard_status", &creator<DiagMcard>},
    {"hdd_status", &creator<DiagHdd>},
    {"moca_status", &creator<DiagMoca>},
    {"ir_status", &creator<DiagIr>},
    {"rf4ce_status", &creator<DiagRf4ce>},
    {"tuner_status", &creator<DiagTuner>},
    {"avdecoder_qam_status", &creator<DiagAv>},
    {"modem_status", &creator<DiagModem>},
    {"flash_status", &creator<DiagFlash>},
    {"dram_status", &creator<DiagDram>}
};

std::unique_ptr<Diag> DiagFactory::create(std::string diag, std::string params)
{
    auto it = pool.find(diag);
    return std::unique_ptr<Diag>(it != pool.end() ? it->second(params) : new Diag(diag, params));
}

} // namespace hwst
