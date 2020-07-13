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
#include <sstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/
#include "wa_wsclient.h"
#include "wa_service.h"
#include "wa_settings.h"
#include "hwst_sched.hpp"
#include "hwst_log.hpp"


/*****************************************************************************
 * LOCAL DEFINITIONS
 *****************************************************************************/
//#define WA_DEBUG 1
#if WA_DEBUG
#define WA_DBG(f, ...) printf(f, ##__VA_ARGS__)
#else
#define WA_DBG(f, ...) (void)0
#endif

#define HWSELFTEST_WSCLIENT_VERSION "0011" /* Increased version number as part of RDK-28856 */


namespace {

const char *HWSELFTEST_WSCLIENT_REMOTE = "remote trigger ver. " HWSELFTEST_WSCLIENT_VERSION;
const char *HWSELFTEST_WSCLIENT_PERIODIC = "periodic trigger ver. " HWSELFTEST_WSCLIENT_VERSION;

const char *HWSELFTEST_AGENT_SERVER_ADDR = "127.0.0.1";
const char *HWSELFTEST_AGENT_SERVER_PORT = "8002";

const char *HWSELFTEST_ENABLE_FILE = "/tmp/.hwselftest_enable";
const char *HWSELFTEST_SETTINGS_FILE = "/tmp/.hwselftest_settings";

const int CONNECTION_TIMEOUT = 60; // sec /* Increased timeout value, fix for DELIA-42225  */
const int PREV_RESULTS_FETCH_TIMEOUT = 1000; // msec

const char *SYSTEMD_SERVICE_PATH = "/run/systemd/system";
const char *HWSELFTEST_RUNNER_TIMER = "hwselftest-runner.timer";
const char *HWSELFTEST_RUNNER_SERVICE = "hwselftest-runner.service";

const int EXECUTION_TIMEOUT = 180; // sec - should be less than MIN_PERIODIC_FREQUENCY
const int MIN_PERIODIC_FREQUENCY = 1; // minutes
const int DEFAULT_PERIODIC_FREQUENCY = 240; // minutes
const char *HWST_PERIODIC_FREQ_NAME = "HWST_PERIODIC_FREQ";

const int DEFAULT_CPU_THRESHOLD = 75; // percent
const char *HWST_CPU_THRESHOLD_NAME = "HWST_CPU_THRESHOLD";

const int DEFAULT_DRAM_THRESHOLD = 50; // MB
const char *HWST_DRAM_THRESHOLD_NAME = "HWST_DRAM_THRESHOLD";

const char *HWSELFTEST_TUNE_TYPE_FILE = "/tmp/.hwselftest_tunetype";
const char *HWSELFTEST_TUNE_RESULTS_FILE = "/opt/logs/hwselftest.tuneresults";

} // local namespeace


/*****************************************************************************
 * METHOD DEFINITIONS
 *****************************************************************************/

namespace hwselftest {

wa_wsclient::wa_wsclient():
    _hwst_scheduler(new hwst::Sched(HWSELFTEST_AGENT_SERVER_ADDR, HWSELFTEST_AGENT_SERVER_PORT, CONNECTION_TIMEOUT)),
    _runner_service(SYSTEMD_SERVICE_PATH, HWSELFTEST_RUNNER_SERVICE),
    _runner_timer(SYSTEMD_SERVICE_PATH, HWSELFTEST_RUNNER_TIMER),
    _settings(HWSELFTEST_SETTINGS_FILE)
{
    _runner_timer.set_description("RDK Hardware Self Test periodic run timer");
    _runner_timer.set_unit(HWSELFTEST_RUNNER_SERVICE);

    if (_settings.get(HWST_PERIODIC_FREQ_NAME).empty() || (std::stoi(_settings.get(HWST_PERIODIC_FREQ_NAME)) < MIN_PERIODIC_FREQUENCY))
        _settings.set(HWST_PERIODIC_FREQ_NAME, DEFAULT_PERIODIC_FREQUENCY);

    if (_settings.get(HWST_CPU_THRESHOLD_NAME).empty() || (std::stoi(_settings.get(HWST_CPU_THRESHOLD_NAME)) > 100))
        _settings.set(HWST_CPU_THRESHOLD_NAME, DEFAULT_CPU_THRESHOLD);

    if (_settings.get(HWST_DRAM_THRESHOLD_NAME).empty())
        _settings.set(HWST_DRAM_THRESHOLD_NAME, DEFAULT_DRAM_THRESHOLD);

    WA_DBG("wa_wsclient::wa_wsclient(): cpu threshold: %s%%\n", _settings.get(HWST_CPU_THRESHOLD_NAME).c_str());
    WA_DBG("wa_wsclient::wa_wsclient(): dram threshold: %s MB\n", _settings.get(HWST_DRAM_THRESHOLD_NAME).c_str());
    WA_DBG("wa_wsclient::wa_wsclient(): periodic interval: %s min.\n", _settings.get(HWST_PERIODIC_FREQ_NAME).c_str());
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
    struct stat buffer;
    return (stat(HWSELFTEST_ENABLE_FILE, &buffer) == 0);
}

/**
 * @brief This function is used to enable/disable the hwselftest component.
 * @param[out] enable True to enable, False to disable.
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
        if (is_enabled())
        {
            enable_periodic(false, true);
            result = (unlink(HWSELFTEST_ENABLE_FILE) == 0);
        }
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

    if (_hwst_scheduler)
    {
        std::string test_results;

        WA_DBG("wa_wsclient::get_results(): attempting to retrieve previous results from agent...\n");

        if (execute("previous_results", test_results))
        {
            _last_result = test_results;
            WA_DBG("wa_wsclient::get_results(): retrieved last results from agent\n");
        }

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

    return retval;
}

/**
 * @brief This function is used to retrieve agent capabilities.
 * @param[out] caps String to be filled with capabilities info.
 * @return Status of the operation
 * @retval true   Operation succeeded
 * @retval false  Operation failed
 */
bool wa_wsclient::get_capabilities(std::string& caps)
{
    int retval = false;

    if (_hwst_scheduler)
    {
        WA_DBG("wa_wsclient::get_results(): attempting to agent capabilities...\n");

        // currently this only returns available diags list
        if (execute("capabilities_info", caps))
        {
            WA_DBG("wa_wsclient::get_capabilities(): successfully retrieved capabilities\n");
            retval = true;
        }
        else
            WA_DBG("wa_wsclient::get_capabilities(): failed to retrieve capabilities\n");

    }
    else
        WA_DBG("wa_wsclient::get_capabilities(): scheduler not avaialable\n");

    return retval;
}

/**
 * @brief This function is used to start at test run of the whole HW SelfTest test suite
 * @param[in] cli True if executed by CLI, False if by TR69 client.
 * @return Status of the operation
 * @retval true   Operation succeeded
 * @retval false  Operation failed
 */
bool wa_wsclient::execute_tests(bool cli)
{
    int retval = false;

    if (_hwst_scheduler)
    {
        WA_DBG("wa_wsclient::execute_tests(): attempting to execute tests...\n");
        int status = _hwst_scheduler->issue({}, (cli? HWSELFTEST_WSCLIENT_PERIODIC : HWSELFTEST_WSCLIENT_REMOTE));

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

    return retval;
}

/**
 * @brief This function is used to start only tuner test by issuing request to agent
 * @param[in] cli True if executed by CLI, False if by TR69 client.
 * @return Status of the operation
 * @retval true   Operation succeeded
 * @retval false  Operation failed
 */
bool wa_wsclient::execute_tune_test(bool cli, const std::string &param)
{
    int retval = false;

    if (_hwst_scheduler)
    {
        WA_DBG("wa_wsclient::execute_tune_test(): attempting to execute tune test...\n");
        int status = _hwst_scheduler->issue({"tuner_status"}, (cli? HWSELFTEST_WSCLIENT_PERIODIC : HWSELFTEST_WSCLIENT_REMOTE), param);

        switch(status)
        {
        case -1:
        default:
            WA_DBG("wa_wsclient::execute_tune_test(): scheduler failed (%i)\n", status);
            break;
        case 1:
            WA_DBG("wa_wsclient::execute_tune_test(): scheduler busy\n");
            break;
        case 0:
            WA_DBG("wa_wsclient::execute_tune_test(): successfully scheduled tune test\n");
            retval = true;
            break;
        }
    }
    else
        WA_DBG("wa_wsclient::execute_tune_test(): scheduler not avaialable\n");

    return retval;
}

/**
 * @brief Wait for the scheduler action to finish or timeout
 * @return Status of the operation
 * @retval false Scheduler not finished
 * @retval true  Scheduler finished
 */
bool wa_wsclient::wait()
{
    int status = 0;
    std::string test_result;
    int count = EXECUTION_TIMEOUT;
    bool result = false;

    if (_hwst_scheduler)
    {
        // wait for the tests to finish
        do {
            status = _hwst_scheduler->get(test_result);
            if(status != 0)
                break;
            usleep(1000000);
        } while (--count);

        if(status == 0)
            WA_DBG("wa_wsclient::wait(): operation timed out\n");

        result = status;
    }
    else
        WA_DBG("wa_wsclient::wait(): scheduler not avaialable\n");

    return result;
}

/**
 * @brief This function is used to enable/disable the hwselftest periodic run feature.
 * @param[in] enable True to enable periodic running, False to disable.
 * @param[in] destroy True remove the periodic service timer file.
 * @param[in] quite True to suppress logs.
 * @return Status of the operation
 * @retval true   Operation succeeded
 * @retval false  Operation failed
 */
bool wa_wsclient::enable_periodic(bool enable, bool destroy, bool quiet)
{
    int retval = false;

    _runner_timer.stop();

    if (enable)
    {
        int status = 0;

        if (_runner_timer.exists() != 0)
        {
            _runner_timer.set_on_unit_active_sec(DEFAULT_PERIODIC_FREQUENCY);
            _settings.set(HWST_PERIODIC_FREQ_NAME, DEFAULT_PERIODIC_FREQUENCY);

            status = _runner_timer.create();
            if (status != 0)
                WA_DBG("wa_wsclient::enable_periodic(): can't create runner timer file\n");
        }

        if (status == 0)
        {
            if (_runner_timer.start() == 0)
            {
                if (_runner_service.start() == 0)
                {
                    WA_DBG("wa_wsclient::enable_periodic(): periodic run enabled\n");

                    if (!quiet)
                    {
                        log("Periodic run enabled.\n");
                        log("Periodic run frequency is " + _settings.get(HWST_PERIODIC_FREQ_NAME) + " min.\n");
                        log("Periodic run CPU threshold is " + _settings.get(HWST_CPU_THRESHOLD_NAME) + " %.\n");
                        log("Periodic run DRAM threshold is " + _settings.get(HWST_DRAM_THRESHOLD_NAME) + " MB.\n");
                    }

                    retval = true;
                }
                else
                {
                    _runner_timer.stop();
                    WA_DBG("wa_wsclient::enable_periodic(): can't start runner service\n");
                }
            }
            else
                WA_DBG("wa_wsclient::enable_periodic(): can't start runner timer\n");
        }
    }
    else
    {
        retval = true;

        if (destroy)
            _runner_timer.destroy();

        WA_DBG("wa_wsclient::enable_periodic(): periodic run disabled\n");

        if (!quiet)
            log("Periodic run disabled.\n");
    }

    return retval;
}

/**
 * @brief This function is used to enable/disable the hwselftest periodic run feature.
 * @return Status of the operation
 * @retval true   Operation succeeded
 * @retval false  Operation failed
 */
bool wa_wsclient::set_periodic_frequency(bool *invalidParam, unsigned int frequency)
{
    int retval = false;

    if (frequency == 0)
        frequency = DEFAULT_PERIODIC_FREQUENCY;

    if (frequency >= MIN_PERIODIC_FREQUENCY)
    {
        _runner_timer.set_on_unit_active_sec(frequency);
        _settings.set(HWST_PERIODIC_FREQ_NAME, frequency);

        if (_runner_timer.create() == 0)
        {
            // if already running, then apply the new frequency immediately
            if (_runner_timer.status() == 0)
            {
                retval = enable_periodic(true, false, true);
                if (retval)
                    WA_DBG("wa_wsclient::set_periodic_frequency(): changed periodic frequency to %i min\n", frequency);
                else
                    WA_DBG("wa_wsclient::set_periodic_frequency(): failed to apply new periodic run frequency\n");
            }
            else
                retval = true;

            log("Periodic run frequency set to " + std::to_string(frequency) + " min.\n");
        }
        else
            WA_DBG("wa_wsclient::enable_periodic(): can't create timer file\n");
    }
    else
    {
        *invalidParam = true;
        WA_DBG("wa_wsclient::set_periodic_frequency(): invalid frequency value: '%i'\n", frequency);
    }

    return retval;
}

/**
 * @brief This function is used to set hwselftest CPU threshold.
 * @return Status of the operation
 * @retval true   Operation succeeded
 * @retval false  Operation failed
 */
bool wa_wsclient::set_periodic_cpu_threshold(bool *invalidParam, unsigned int threshold)
{
    int retval = false;

    if (threshold <= 100)
    {
        retval = _settings.set(HWST_CPU_THRESHOLD_NAME, threshold);
        if (retval)
        {
            WA_DBG("wa_wsclient::set_periodic_cpu_threshold(): changed periodic run cpu threshold to %i%%\n", threshold);
            log("Periodic run CPU threshold set to " + std::to_string(threshold) + "%.\n");
        }
        else
            WA_DBG("wa_wsclient::set_periodic_cpu_threshold(): failed to apply new periodic run cpu threshold\n");
    }
    else
    {
        *invalidParam = true;
        WA_DBG("wa_wsclient::set_periodic_cpu_threshold(): invalid threshold value: '%i%%'\n", threshold);
    }

    return retval;
}

/**
 * @brief This function is used to set hwselftest DRAM threshold.
 * @return Status of the operation
 * @retval true   Operation succeeded
 * @retval false  Operation failed
 */
bool wa_wsclient::set_periodic_dram_threshold(unsigned int threshold)
{
    int retval = false;

    retval = _settings.set(HWST_DRAM_THRESHOLD_NAME, threshold);
    if (retval)
    {
        WA_DBG("wa_wsclient::set_periodic_dram_threshold(): changed periodic run dram threshold to %i%%\n", threshold);
        log("Periodic run DRAM threshold set to " + std::to_string(threshold) + " MB.\n");
    }
    else
        WA_DBG("wa_wsclient::set_periodic_dram_threshold(): failed to apply new periodic run dram threshold\n");

    return retval;
}

/**
 * @brief This function is used to set tune type for hwselftest tuner test.
 * @return Status of the operation
 * @retval true   Operation succeeded
 * @retval false  Operation failed
 */
bool wa_wsclient::set_tune_type(unsigned int tune_type)
{
    bool result = true;
    size_t byte;
    std::string buf;

    if (tune_type <= FREQ_PROG)
    {
        int fd = open(HWSELFTEST_TUNE_TYPE_FILE, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IRGRP | S_IROTH);
        if (fd != -1)
        {
            buf = std::to_string(tune_type);
            byte = buf.length();
            write(fd, buf.c_str(), byte);
            result = (close(fd) == 0);

            /* Delete tune results file once tune type is received */
            struct stat buffer;
            if (stat(HWSELFTEST_TUNE_RESULTS_FILE, &buffer) == 0)
            {
                remove(HWSELFTEST_TUNE_RESULTS_FILE);
            }
        }
        else
            result = false; // failed to create the file
    }
    else
    {
        struct stat buffer;
        if (stat(HWSELFTEST_TUNE_TYPE_FILE, &buffer) == 0)
        {
            remove(HWSELFTEST_TUNE_TYPE_FILE);
        }
    }

    if (result)
    {
        if (tune_type <= FREQ_PROG)
            log("Tuner test enabled.\n");
        else
            log("Tuner test disabled.\n");
    }

    return result;
}

/**
 * @brief This function is used to get tune type for hwselftest tuner test.
 * @return Status of the operation
 * @retval true   Operation succeeded
 * @retval false  Operation failed
 */
bool wa_wsclient::get_tune_type(unsigned int* tune_type)
{
    bool result = true;
    char buf[5];
    size_t byte;
    ssize_t byte_read;

    int fd = open(HWSELFTEST_TUNE_TYPE_FILE, O_RDONLY, S_IRUSR | S_IRGRP | S_IROTH);
    if (fd != -1)
    {
        byte = sizeof(buf);
        byte_read = read(fd, buf, byte);
        if (byte_read > 0)
        {
            *tune_type = atoi(buf);
        }
        result = (close(fd) == 0);
    }
    else
        result = false; // failed to read the file

    if (*tune_type > 0 && *tune_type <= FREQ_PROG)
        log("Tuner test enabled.\n");
    else
        log("Tuner test disabled.\n");

    return result;
}

void wa_wsclient::log(const std::string& message) const
{
    hwst::Log().writeToLog(hwst::Log().format(std::string("[TR69] ") + message));
}

bool wa_wsclient::execute(const std::string& diag, std::string& result)
{
    int retval = false;

    if (_hwst_scheduler)
    {
        int status = 0;

        WA_DBG("wa_wsclient::execute(): attempting to execute diag %s...\n", diag.c_str());

        // ask agent for previous results...
        status = _hwst_scheduler->issue({diag});
        if (status == 0)
        {
            // wait for the results to arrive...
            int count = (PREV_RESULTS_FETCH_TIMEOUT / 100);
            do {
                usleep(100 * 1000);
                status = _hwst_scheduler->get(result);
                count--;
            } while ((status != 1) && count);

            if (status == 1)
            {
                retval = true;
                WA_DBG("wa_wsclient::execute(): retrieved result from agent\n");
            }
            else if (status == 0)
                WA_DBG("wa_wsclient::execute(): operation timed out\n");
            else
                WA_DBG("wa_wsclient::execute(): operation failed\n");
        }
        else if (status == 1)
            WA_DBG("wa_wsclient::execute(): scheduler busy\n");
        else if (status == -1)
            WA_DBG("wa_wsclient::execute(): scheduler failed\n");
    }
    else
        WA_DBG("wa_wsclient::execute(): scheduler not avaialable\n");

    return retval;
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
