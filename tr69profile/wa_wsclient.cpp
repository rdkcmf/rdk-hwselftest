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

/*****************************************************************************
 * STANDARD INCLUDE FILES
 *****************************************************************************/
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>


/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/
#include "wa_wsclient.h"
#include "hwst_sched.hpp"
#include "hwst_log.hpp"


/*****************************************************************************
 * LOCAL DEFINITIONS
 *****************************************************************************/

#if WA_DEBUG
#define WA_DBG(f, ...) printf(f, ##__VA_ARGS__)
#else
#define WA_DBG(f, ...) (void)0
#endif

#define HWSELFTEST_WSCLIENT_VERSION "0003"

namespace {

const char *HWSELFTEST_WSCLIENT_REMOTE = "remote trigger ver. " HWSELFTEST_WSCLIENT_VERSION;

const char *HWSELFTEST_AGENT_SERVER_ADDR = "127.0.0.1";
const char *HWSELFTEST_AGENT_SERVER_PORT = "8002";

const char *HWSELFTEST_ENABLE_FILE = "/tmp/.hwselftest_enable";

const int CONNECTION_TIMEOUT = 1; // sec
const int PREV_RESULTS_FETCH_TIMEOUT = 500; // msec

} // local namespeace


/*****************************************************************************
 * METHOD DEFINITIONS
 *****************************************************************************/

namespace hwselftest {

wa_wsclient::wa_wsclient():
    _hwst_scheduler(new hwst::Sched(HWSELFTEST_AGENT_SERVER_ADDR, HWSELFTEST_AGENT_SERVER_PORT, CONNECTION_TIMEOUT))
{
}

wa_wsclient::~wa_wsclient()
{
    delete _hwst_scheduler;
}

/**
 * @brief This function is used to check if the hwselftest component is enabled.
 * @return Status of the hwselftest component
 * @retval true   Component enabeld
 * @retval false  Component disabled
 */
bool wa_wsclient::is_enabled() const
{
    // Check if the mark file does exist to enable this feature.
    struct stat buffer;
    return (stat(HWSELFTEST_ENABLE_FILE, &buffer) == 0);
}

/**
 * @brief This function is used to enable/disable the hwselftest component.
 * @return Status of the operation
 * @retval true   Operation succeeded
 * @retval false  Operation failed
 */
bool wa_wsclient::enable(bool enable)
{
    bool result = true;

    if (enable)
    {
        if (!is_enabled())
        {
            int fd = open(HWSELFTEST_ENABLE_FILE, O_RDWR | O_CREAT, S_IRUSR | S_IRGRP | S_IROTH);
            if (fd != -1)
                result = (close(fd) == 0);
            else
                result = false; // failed to create the file
        }
    }
    else
    {
        // If enabled (i.e. the mark file exists) then delete the mark file.
        if (is_enabled())
            result = (unlink(HWSELFTEST_ENABLE_FILE) == 0);
    }

    if (result)
    {
        if (is_enabled())
            log("Feature enabled.\n");
        else
            log("Feature disabled.\n");
    }

    return result;
}

/**
 * @brief This function is used to retrieve the most recent test results.
 * @param[out] result To be filled with test results string
 * @return Status of the operation
 * @retval true   Operation succeeded
 * @retval false  Operation failed
 */
bool wa_wsclient::get_results(std::string& results)
{
    int retval = false;

    if (is_enabled())
    {
        if (_hwst_scheduler)
        {
            int status = 0;
            std::string test_result;

            WA_DBG("wa_wsclient::get_results(): attempting to retrieve previous results from agent...\n");

            // ask agent for previous results...
            status = _hwst_scheduler->issue("previous_results");
            if (status == 0)
            {
                // wait for the results to arrive...
                int count = (PREV_RESULTS_FETCH_TIMEOUT / 100);
                do {
                    usleep(100 * 1000);
                    status = _hwst_scheduler->get(test_result);
                    count--;
                } while ((status != 1) && count);

                if (status == 1)
                {
                    WA_DBG("wa_wsclient::get_results(): retrieved results from agent\n");
                    _last_result = test_result;
                }
                else if (status == 0)
                    WA_DBG("wa_wsclient::get_results(): operation timed out\n");
                else
                    WA_DBG("wa_wsclient::get_results(): operation failed\n");
            }
            else if (status == 1)
                WA_DBG("wa_wsclient::get_results(): scheduler busy\n");
            else if (status == -1)
                WA_DBG("wa_wsclient::get_results(): scheduler failed\n");

            if (_last_result.empty())
            {
                // no results available
                WA_DBG("wa_wsclient::get_results(): returning 'not initiated'\n");
                _last_result = "Not Initiated";
            }

            retval = true; // will always return something
            results = _last_result;
        }
        else
            WA_DBG("wa_wsclient::get_results(): scheduler not avaialable\n");
    }
    else
    {
        WA_DBG("wa_wsclient::get_results(): service disabled\n");
        log("Feature disabled. Results request ignored.\n");
    }

    return retval;
}

/**
 * @brief This function is used to start at test run of the whole HW SelfTest test suite
 * @return Status of the operation
 * @retval true   Operation succeeded
 * @retval false  Operation failed
 */
bool wa_wsclient::execute_tests(int cookie)
{
    int retval = false;

    if (is_enabled())
    {
        if (_hwst_scheduler)
        {
            int status = _hwst_scheduler->issue("all", HWSELFTEST_WSCLIENT_REMOTE);

            switch(status)
            {
            case -1:
            default:
                WA_DBG("wa_wsclient::execute_tests(): scheduler failed (%i)\n", status);
                break;
            case 1:
                WA_DBG("wa_wsclient::execute_tests(): scheduler busy\n");
                break;
            case 0:
                WA_DBG("wa_wsclient::execute_tests(): successfully scheduled tests\n");
                retval = true;
                break;
            }
        }
        else
            WA_DBG("wa_wsclient::execute_tests(): scheduler not avaialable\n");
    }
    else
    {
        WA_DBG("wa_wsclient::execute_tests(): service disabled\n");
        log("Feature disabled. ExecuteTest request ignored.\n");
    }

    return retval;
}

void wa_wsclient::log(const std::string& message) const
{
    hwst::Log().toFile(hwst::Log().format(std::string("[TR69] ") + message));
}

/*****************************************************************************
 * STATIC METHOD DEFINITIONS
 *****************************************************************************/

wa_wsclient *wa_wsclient::instance()
{
    // guaranteed thread-safe by C++11
    static wa_wsclient wsclient_instance;
    return &wsclient_instance;
}

} // namespace hwselftest
