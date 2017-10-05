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


/*****************************************************************************
 * EXPORTED CLASSES
 *****************************************************************************/

namespace hwst { class Sched; }

namespace hwselftest {

class wa_wsclient final {
public:
    static wa_wsclient *instance();

    bool is_enabled() const;
    bool enable(bool toggle);

    bool execute_tests(int cookie);
    bool get_results(std::string &results);

private:
    wa_wsclient();
    ~wa_wsclient();

    wa_wsclient(wa_wsclient const &) = delete;
    void operator=(wa_wsclient const &) = delete;

    void log(const std::string& message) const;

    std::string _last_result;
    hwst::Sched *_hwst_scheduler;
};

} // namespace hwselftest

#endif /* WA_WSCLIENT_H_ */
