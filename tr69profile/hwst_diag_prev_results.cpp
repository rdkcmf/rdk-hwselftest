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
#include <fstream>

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
#define HWST_RESULTS_FILE  "/tmp/hwselftest.results"

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

/* Changed WEBPA GET Results format for COLBO-202 */
std::string DiagPrevResults::getStrStatus() const
{
    std::string results;

    std::ifstream results_file(HWST_RESULTS_FILE); /* Let the process reach here to read results file, so that the running tests finishes updating it by then  */
    if (results_file.is_open())
    {
        getline(results_file, results);
        results_file.close();
    }
    return results;
}

} // namespace hwst
