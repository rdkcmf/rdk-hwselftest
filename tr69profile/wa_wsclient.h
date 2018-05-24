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

#ifndef WA_WSCLIENT_H_
#define WA_WSCLIENT_H_

/*****************************************************************************
 * STANDARD INCLUDE FILES
 *****************************************************************************/
#include <string>
#include <vector>

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/
#include "wa_service.h"
#include "wa_settings.h"

/*****************************************************************************
 * EXPORTED CLASSES
 *****************************************************************************/

namespace hwst { class Sched; }

namespace hwselftest {

class wa_wsclient final {
public:
    static wa_wsclient *instance();

    bool is_enabled() const;
    bool enable(bool toggle = true);

    bool execute_tests(bool cli = false);
    bool get_results(std::string& results);
    bool get_capabilities(std::string& caps);
    bool wait();

    bool enable_periodic(bool toggle = true, bool destroy = false, bool quiet = false);
    bool set_periodic_frequency(bool *invalidParam, unsigned int frequency);
    bool set_periodic_cpu_threshold(bool *invalidParam, unsigned int threshold);
    bool set_periodic_dram_threshold(unsigned int threshold);

private:
    wa_wsclient();
    ~wa_wsclient();

    wa_wsclient(wa_wsclient const &) = delete;
    void operator=(wa_wsclient const &) = delete;

    void log(const std::string& message) const;
    bool execute(const std::string& diag, std::string& results);

    wa_service _runner_service;
    wa_service _runner_timer;

    wa_settings _settings;

    std::vector<std::string> _available_diags;
    std::string _last_result;
    hwst::Sched *_hwst_scheduler;
};

} // namespace hwselftest

#endif /* WA_WSCLIENT_H_ */
