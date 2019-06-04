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

#ifndef _HWST_SCHED_
#define _HWST_SCHED_

#include <condition_variable>
#include <string>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#include <map>

#define NUM_ELEMENTS 13
#define BUFFERLEN 256

namespace hwst {

class Comm;
class Diag;
class Scenario;

class Sched
{
public:
    enum state_t {
        idle,
        running,
        finished
    };

    Sched(std::string host, std::string port, int timeout);
    ~Sched();
    int issue(const std::vector<std::string>& jobs, const std::string& client = "");
    int get(std::string &result);


private:
    state_t state;
    std::mutex apiMutex;
    std::mutex diagsMutex;
    std::string host;
    std::string port;
    std::mutex cbMutex;
    std::condition_variable cbCond;
    bool cbHappened;
    int timeout;
    bool working;
    bool update;
    bool connected;
    char telemetryResults[NUM_ELEMENTS][BUFFERLEN];
    void cbConnected(void);
    void cbDisconnected(void);
    void cbUpdate(void);
    void worker(void);
    void telemetryLogInit(void);
    void telemetryLogStore(std::string);
    void telemetryLog(bool);
    std::unique_ptr<std::thread> thd;
    std::shared_ptr<Comm> comm;
    std::unique_ptr<Scenario> scenario;
    std::string summary;
    bool quiet;
};

} // namespace hwst

#endif // _HWST_SCHED_
