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
#include <algorithm>

#include <unistd.h>

#include "hwst_sched.hpp"
#include "hwst_scenario_set.hpp"
#include "hwst_scenario_auto.hpp"
#include "hwst_diag.hpp"
#include "hwst_diagfactory.hpp"
#include "hwst_diag_sysinfo.hpp"
#include "hwst_comm.hpp"
#include "hwst_log.hpp"

//#define HWST_DEBUG 1
#ifdef HWST_DEBUG
#define HWST_DBG(str) do {std::cout << "HWST_DBG |" << str << std::endl;} while(false)
#else
#define HWST_DBG(str) do {} while(false)
#endif

//#define USE_PRCTL_FOR_THREAD_NAME 1
#ifdef USE_PRCTL_FOR_THREAD_NAME
#include <sys/prctl.h> //for debug thread name
#endif

using hwst::Comm;

namespace hwst {

std::map<std::string, int> diagPool =
{

    {"hdd_status", 0},
    {"mcard_status", 1},
    {"rf4ce_status", 2},
    {"hdmiout_status", 3},
    {"moca_status", 4},
    {"modem_status", 5},
    {"flash_status", 6},
    {"dram_status", 7},
    {"avdecoder_qam_status", 8},
    {"tuner_status", 9},
    {"ir_status", 10},
    {"sdcard_status", 11},
    {"bluetooth_status", 12}
};

Sched::Sched(std::string host, std::string port, int timeout):
    host(host),
    port(port),
    timeout(timeout),
    working(false),
    state(idle)
{
    HWST_DBG("Sched");
    telemetryLogInit();
}

void Sched::telemetryLogInit()
{
    std::string telemetry ("NA");
    for(int i=0; i < NUM_ELEMENTS; i++)
    {
        telemetry.copy(telemetryResults[i], telemetry.length(), 0);
    }
    std::string RF_default ("RF_Not_Paired");
    std::string IR_default ("IR_NA_as_RF_Paired");
    std::string ne ("Not_Enabled");
    RF_default.copy(telemetryResults[2], RF_default.length(), 0); //this is for RF element
    telemetryResults[2][RF_default.length()] = '\0';
    IR_default.copy(telemetryResults[10], IR_default.length(), 0); //this is for IR element
    telemetryResults[10][IR_default.length()] = '\0';
    ne.copy(telemetryResults[11], ne.length(), 0); //this is for sdcard element
    telemetryResults[11][ne.length()] = '\0';
    ne.copy(telemetryResults[12], ne.length(), 0); //this is for bluetooth element
    telemetryResults[12][ne.length()] = '\0';
}

Sched::~Sched()
{
    HWST_DBG("~Sched");
    if(thd != nullptr)
    {
        {
            std::unique_lock<std::mutex> condLock(cbMutex);
            working = false;
            cbHappened = true;
        }
        cbCond.notify_all();
        thd->join();
        thd.reset();
    }
    if(comm != nullptr)
    {
        comm.reset();
    }
}

void Sched::worker(void)
{
#ifdef USE_PRCTL_FOR_THREAD_NAME
    prctl(PR_SET_NAME,"worker",0,0,0);
#endif

    HWST_DBG("Sched-loop-begin");
    while(working)
    {
        HWST_DBG("working");
        bool disconnect = false;
        bool doUpdate = false;
        bool doDisconnect = false;
        int status;
        {
            std::unique_lock<std::mutex> condLock(cbMutex);
            HWST_DBG("worker-waiting");
            if(!cbHappened)
                cbCond.wait(condLock, [&]{return cbHappened;});
            cbHappened = false;
            HWST_DBG("worker-notified");
            if(!connected)
            {
                doDisconnect = true;
                HWST_DBG("worker-DISCONNECTED");
            }
            else if(update)
            {
                /* update */
                doUpdate = update;
                HWST_DBG("worker-UPDATE");
            }
        }

        if(doDisconnect)
        {
            std::lock_guard<std::mutex> apiLock(apiMutex);
            working = false;
            state = finished;
        }
        else if(doUpdate)
        {
            std::lock_guard<std::mutex> apiLock(apiMutex);

            {
                std::lock_guard<std::recursive_mutex> elementsLock(scenario->elementsMutex);
                for(auto const& e: scenario->elements)
                {
                    Diag::Status s = e.diag->getStatus(true);
                    if(s.modified &&
                        ((s.state == Diag::error) ||
                        (s.state == Diag::finished)))
                    {
                        HWST_DBG("Running: " + e.diag->name + " finished");
                        std::string tmp = e.diag->getStrStatus();
                        if (!quiet && (tmp.find("Test result:") != std::string::npos))
                            comm->sendRaw("LOG", "{\"rawmessage\": \"" + Log().format(tmp) + "\"}", "null");
                        if (!summary.empty())
                            summary += "\n";
                        summary += tmp;
                        telemetryLogStore(e.diag->name);
                    }
                }
            }

            do
            {
                std::shared_ptr<Diag> diag;
                status = scenario->nextToRun(diag);
                HWST_DBG("nextToRun:" + std::to_string(status));
                switch(status)
                {
                case -1:
                    if(scenario->checkAllDone())
                    {
                        HWST_DBG("Running: all done");

                        if (!quiet)
                        {
                            bool passed = true;
                            {
                                std::lock_guard<std::recursive_mutex> elementsLock(scenario->elementsMutex);
                                for(auto const& e: scenario->elements)
                                {
                                    if(e.diag->getPresentationResult().compare("FAILED") == 0)
                                        passed = false;
                                }
                            }

                            std::string tmp = Log().format("Test execution completed:" + std::string(passed ? "PASSED" : "FAILED"));
                            summary += "\n";
                            summary += tmp;
                            comm->sendRaw("LOG", "{\"rawmessage\": \"" + tmp + "\"}", "null");
                            telemetryLog(passed);
                            comm->sendRaw("TESTRUN", "{\"state\": \"finish\"}", "null");
                        }

                        comm.reset();
                    }
                    break;
                case 0:
                    break;
                case 1:
                    HWST_DBG("Running: " + diag->getName());
                    comm->send(diag);
                    break;
                }
            }while(status == 1);
        }
    }

    HWST_DBG("Sched-loop-finished");
}

void Sched::telemetryLogStore(std::string diagName)
{
    for(auto const& e: scenario->elements)
    {
        if(e.diag->name.compare(diagName) == 0)
        {
            std::string result_value = e.diag->getPresentationResult();

            std::string errorText = e.diag->getPresentationComment();
            if(!errorText.empty())
            {
                std::replace(errorText.begin(), errorText.end(), ' ', '_');
                result_value.append("_");
                result_value.append(errorText);
            }
            auto it = diagPool.find(diagName);
            int index = it->second;
            result_value.copy(telemetryResults[index], result_value.length(), 0);
			telemetryResults[index][result_value.length()] = '\0';
            break;
        }
    }
}

void Sched::telemetryLog(bool testResult)
{
    std::string telemetry_log;
    telemetry_log.append("'");
    for(int i=0; i< NUM_ELEMENTS; i++)
	{
        if((std::string(telemetryResults[i])).compare("Not_Enabled") != 0)
		{
            telemetry_log.append(telemetryResults[i]);
            telemetry_log.append(", ");
        }
    }
    telemetry_log.append(std::string(testResult ? "PASSED" : "FAILED"));
    telemetry_log.append("'");
    std::string telemetry_result = Log().format("HwTestResult_telemetry: " + telemetry_log);
    comm->sendRaw("LOG", "{\"rawmessage\": \"" + telemetry_result + "\"}", "null");
    telemetryLogInit();
}

void Sched::cbConnected()
{
    {
        std::unique_lock<std::mutex> condLock(cbMutex);
        connected = true;
        update = true;
        cbHappened = true;
    }
    cbCond.notify_all();
    HWST_DBG("sched-cbConnected");
}

void Sched::cbDisconnected()
{
    {
        std::unique_lock<std::mutex> condLock(cbMutex);
        connected = false;
        cbHappened = true;
    }
    cbCond.notify_all();
    HWST_DBG("sched-cbDisconnected");
}

void Sched::cbUpdate()
{
    {
        std::unique_lock<std::mutex> condLock(cbMutex);
        update = true;
        cbHappened = true;
    }
    cbCond.notify_all();
    HWST_DBG("sched-cbUpdate");
}

int Sched::get(std::string &result)
{
    HWST_DBG("sched-GET#1");
    std::lock_guard<std::mutex> apiLock(apiMutex);
    int status = -1;
    result = "";

    HWST_DBG("sched-GET");
    switch(state)
    {
    case finished:
        HWST_DBG("sched-GET-finished");
        result = summary;
        status = 1;
        break;

    case running:
        HWST_DBG("sched-GET-running");
        status = 0;
        break;

    case idle:
        HWST_DBG("sched-GET-idle");
    default:
        break;
    }

    return status;
}

int Sched::issue(const std::vector<std::string>& jobs, const std::string& client)
{
    int status = -1;
    HWST_DBG("issue:" + jobs);

    switch(state)
    {
    case finished:
        HWST_DBG("Sched:finished");
        {
            std::unique_lock<std::mutex> condLock(cbMutex);
            working = false;
            cbHappened = true;
        }
        cbCond.notify_all();
        thd->join();
        thd.reset();
        comm.reset();
        state = idle;
        /* no break */
    case idle:
        HWST_DBG("Sched:idle");

        // suppress logging when running single job
        quiet = (jobs.size() == 1);

        if (!jobs.size())
            scenario = std::unique_ptr<Scenario>(new ScenarioAuto());
        else
            scenario = std::unique_ptr<Scenario>(new ScenarioSet());

        if (!scenario->init(jobs))
            break;

        HWST_DBG("Scenario created");

        connected = false;
        update = false;
        working = true;
        cbHappened = false;
        state = running;

        thd = std::unique_ptr<std::thread>(new std::thread(&Sched::worker, this));
        if(thd == nullptr)
        {
            state = idle;
            break;
        }

        comm = std::shared_ptr<Comm>(new Comm(std::bind(&Sched::cbConnected, this),
            std::bind(&Sched::cbDisconnected, this),
            std::bind(&Sched::cbUpdate, this)));

        if(comm->connect(host, port, timeout) != 0)
        {
            HWST_DBG("Could not connect");
            {
                std::unique_lock<std::mutex> condLock(cbMutex);
                working = false;
                cbHappened = true;
            }
            cbCond.notify_all();
            thd->join();
            thd.reset();
            comm.reset();
            state = idle;
            break;
        }

        summary = "";

        if (!quiet)
        {
            std::string tmp = Log().format("Test execution start");
            if (!client.empty())
            {
                tmp += ", ";
                tmp += client;
            }

            summary += tmp;
            comm->sendRaw("LOG", "{\"rawmessage\": \"" + tmp + "\"}", "null");
            comm->sendRaw("TESTRUN", "{\"state\": \"start\", \"client\": \"" + client + "\"}", "null");
        }
        HWST_DBG("Started");
        status = 0;
        break;
    case running:
        HWST_DBG("Sched:running");
        status = 1;
        break;
    }

end:
    if(status == -1)
        Log().writeToLog(Log().format("[TR69] No connection to agent.\n"));

    return status;
}

} // namespace hwst

