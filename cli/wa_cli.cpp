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

/**
 * @file wa_cli.c
 *
 * @brief This file contains main function for WA_CLI - hwselftest command line
 *        interface.
 */

/** @addtogroup WA_CLI
 *  @{
 */

/*****************************************************************************
 * STANDARD INCLUDE FILES
 *****************************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dlfcn.h>
#include <stdbool.h>
#include <unistd.h>

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/
#include "wa_wsclient.h"

using namespace hwselftest;

/*****************************************************************************
 * LOCAL DEFINITIONS
 *****************************************************************************/
#define HWSELFTESTCLI_NAME "hwselftestcli"
#define HWSELFTESTCLI_VERSION "0005"

#define cliprintf(str, ...) printf(HWSELFTESTCLI_NAME ": " str, ##__VA_ARGS__)
#define cliprintferr(str, ...) fprintf(stderr, HWSELFTESTCLI_NAME ": " str, ##__VA_ARGS__)

/*****************************************************************************
 * LOCAL TYPES
 *****************************************************************************/

/*****************************************************************************
 * LOCAL FUNCTION PROTOTYPES
 *****************************************************************************/
static bool selftest_execute(wa_wsclient *pInst);
#if WA_DEBUG
static bool selftest_results(wa_wsclient *pInst, char **out_results);
static bool selftest_capabilities(wa_wsclient *pInst, char **out_capabilities);
static bool selftest_enable(wa_wsclient *pInst, bool do_enable);
static bool selftest_periodic_enable(wa_wsclient *pInst, bool do_enable);
static bool selftest_periodic_frequency(wa_wsclient *pInst, const char *frequency);
static bool selftest_periodic_cpu_threshold(wa_wsclient *pInst, const char *threshold);
static bool selftest_periodic_dram_threshold(wa_wsclient *pInst, const char *threshold);
#endif /* WA_DEBUG */

/*****************************************************************************
 * LOCAL VARIABLE DECLARATIONS
 *****************************************************************************/
static int verbose_flag = 0;

/*****************************************************************************
 * FUNCTION DEFINITIONS
 *****************************************************************************/

int main(int argc, char * const argv[])
{
    enum {
        CMD_NONE,
        CMD_EXECUTE
#if WA_DEBUG
        ,
        CMD_ENABLE,
        CMD_DISABLE,
        CMD_RESULTS,
        CMD_CAPABILITIES,
        CMD_PERIODIC_ENABLE,
        CMD_PERIODIC_DISABLE,
        CMD_PERIODIC_FREQUENCY,
        CMD_PERIODIC_CPU_THRESHOLD,
        CMD_PERIODIC_DRAM_THRESHOLD
#endif /* WA_DEBUG */
    };

    int cmd = CMD_NONE;
    const char *cmd_param = NULL;
    bool status = true;
    void *wal_handle;
    void *rdkloggers_handle;

    if (argc == 1)
        cliprintf("use --help for help\n");

    /* handle cmd line switches */
    for (int i = 1; i < argc; i++)
    {
        if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--verbose"))
            verbose_flag = true;

        else if (!strcmp(argv[i], "--version"))
            cliprintf("%s\n", HWSELFTESTCLI_VERSION);

        else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help"))
        {
            printf(
                "usage: %s [options] command\n" \
                "\n" \
                "options:\n" \
                "    --version        - show %s version\n" \
                "    -v,--verbose     - enable verbose output mode\n" \
                "    -h,--help        - show this help message\n" \
                "\n" \
                "commands:\n" \
                "    execute          - executes the test suite\n"
#if WA_DEBUG
                "    enable           - enables hwselftest\n" \
                "    disable          - disables hwselftest\n" \
                "    results          - retrieves latest test results\n" \
                "    cap(abilitie)s   - retrieves agent capabilities\n"
                "    periodic-enable  - enables hwselftest periodic running\n" \
                "    periodic-disable - disables hwselftest periodic running\n" \
                "    periodic-freq N  - sets hwselftest periodic frequency to N minutes\n" \
                "    periodic-cpu N   - sets hwselftest periodic CPU threshold to N percent\n" \
                "    periodic-dram N  - sets hwselftest periodic DRAM threshold to N MB\n"
#endif /* WA_DEBUG */
                "\n",
                HWSELFTESTCLI_NAME, HWSELFTESTCLI_NAME);

            exit(EXIT_SUCCESS);
        }

        else if (!strcasecmp(argv[i], "execute"))
            cmd = CMD_EXECUTE;

#if WA_DEBUG
        else if (!strcasecmp(argv[i], "enable"))
            cmd = CMD_ENABLE;

        else if (!strcasecmp(argv[i], "disable"))
            cmd = CMD_DISABLE;

        else if (!strcasecmp(argv[i], "caps") || !strcasecmp(argv[i], "capabilities"))
            cmd = CMD_CAPABILITIES;

        else if (!strcasecmp(argv[i], "periodic-enable"))
            cmd = CMD_PERIODIC_ENABLE;

        else if (!strcasecmp(argv[i], "periodic-disable"))
            cmd = CMD_PERIODIC_DISABLE;

        else if (!strcasecmp(argv[i], "periodic-freq"))
        {
            cmd = CMD_PERIODIC_FREQUENCY;
            if (i + 1 < argc)
                cmd_param = argv[++i];
        }

        else if (!strcasecmp(argv[i], "periodic-cpu"))
        {
            cmd = CMD_PERIODIC_CPU_THRESHOLD;
            if (i + 1 < argc)
                cmd_param = argv[++i];
        }

        else if (!strcasecmp(argv[i], "periodic-dram"))
        {
            cmd = CMD_PERIODIC_DRAM_THRESHOLD;
            if (i + 1 < argc)
                cmd_param = argv[++i];
        }

        else if (!strcasecmp(argv[i], "results"))
            cmd = CMD_RESULTS;
#endif /* WA_DEBUG */

        else
        {
            cliprintferr("ERROR: unrecognised parameter: %s (use --help for help)\n", argv[i]);
            exit(EXIT_FAILURE);
        }
    }

    wa_wsclient *pInst = wa_wsclient::instance();
    if (pInst == NULL)
    {
        cliprintferr("ERROR: Failed to get wa_wsclient instance.\n");
        exit(EXIT_FAILURE);
    }

    switch (cmd)
    {
    case CMD_EXECUTE:
        status = selftest_execute(pInst);
        break;

#if WA_DEBUG
    case CMD_ENABLE:
        status = selftest_enable(pInst, true);
        break;

    case CMD_DISABLE:
        status = selftest_enable(pInst, false);
        break;

    case CMD_PERIODIC_ENABLE:
        status = selftest_periodic_enable(pInst, true);
        break;

    case CMD_PERIODIC_DISABLE:
        status = selftest_periodic_enable(pInst, false);
        break;

    case CMD_PERIODIC_FREQUENCY:
        status = selftest_periodic_frequency(pInst, cmd_param);
        break;

    case CMD_PERIODIC_CPU_THRESHOLD:
        status = selftest_periodic_cpu_threshold(pInst, cmd_param);
        break;

    case CMD_PERIODIC_DRAM_THRESHOLD:
        status = selftest_periodic_dram_threshold(pInst, cmd_param);
        break;

    case CMD_CAPABILITIES:
        {
            char *caps = NULL;
            status = selftest_capabilities(pInst, &caps);
            if (status && caps)
            {
                printf("Capabilities: %s\n", caps);
                free(caps);
            }
        }
        break;

    case CMD_RESULTS:
        {
            char *results = NULL;
            status = selftest_results(pInst, &results);
            if (status && results)
            {
                printf("Results:\n%s\n", results);
                free(results);
            }
        }
        break;
#endif /* WA_DEBUG */
    }

    return (status? EXIT_SUCCESS : EXIT_FAILURE);
}

/*****************************************************************************
 * LOCAL FUNCTION DEFINITIONS
 *****************************************************************************/
static bool selftest_execute(wa_wsclient *pInst)
{
    bool status;

    if(!pInst->is_enabled())
    {
        cliprintf("self test not scheduled, feature disabled\n");
        status = true;
    }
    else
    {
        status = pInst->execute_tests(true /* execute from cli */);
        if (status)
        {
            cliprintf("self test scheduled\n");
            status = pInst->wait();
            if(status)
                cliprintf("self test finished\n");
            else
                cliprintf("self test not finished\n");
        }
        else
            cliprintferr("ERROR: failed to schedule self test\n");
    }
    return status;
}

#if WA_DEBUG
static bool selftest_results(wa_wsclient *pInst, char **out_results)
{
    bool status;

    if(!pInst->is_enabled())
    {
        cliprintf("test results not retrieved, feature disabled\n");
        status = true;
    }
    else
    {
        std::string result;
        status = pInst->get_results(result);
        if (status)
        {
            cliprintf("test results retrieved\n");
            status = true;

            if (out_results)
            {
                *out_results = strdup(result.c_str());
                if (!out_results)
                {
                    cliprintferr("ERROR: failed to allocate memory\n");
                    status = false;
                }
            }
        }
        else
            cliprintferr("ERROR: failed to get test results\n");
    }
    return status;
}

static bool selftest_capabilities(wa_wsclient *pInst, char **out_caps)
{
    bool status;

    std::string result;
    status = pInst->get_capabilities(result);
    if (status)
    {
        cliprintf("capabilities retrieved\n");

        if (out_caps)
        {
            *out_caps = strdup(result.c_str());
            if (!out_caps)
            {
                cliprintferr("ERROR: failed to allocate memory\n");
                status = false;
            }
        }
    }
    else
        cliprintferr("ERROR: failed to get capabilities\n");

    return status;
}

static bool selftest_enable(wa_wsclient *pInst, bool do_enable)
{
    bool status;

    status = pInst->enable(do_enable);
    if (status)
        cliprintf("self test %s\n", do_enable? "enabled" : "disabled");
    else
        cliprintferr("ERROR: failed to %s self test\n", do_enable? "enable" : "disable");

    return status;
}

static bool selftest_periodic_enable(wa_wsclient *pInst, bool do_enable)
{
    bool status;

    if(!pInst->is_enabled())
    {
        cliprintf("self test periodic run not %s, feature disabled\n", do_enable? "enabled" : "disabled");
        status = true;
    }
    else
    {
        status = pInst->enable_periodic(do_enable);
        if (status)
            cliprintf("self test periodic run %s\n", do_enable? "enabled" : "disabled");
        else
            cliprintferr("ERROR: failed to %s self test periodic run\n", do_enable? "enable" : "disable");
    }
    return status;
}

static bool selftest_periodic_frequency(wa_wsclient *pInst, const char *frequency)
{
    bool status;

    if(!pInst->is_enabled())
    {
        cliprintf("self test periodic run frequency not set, feature disabled\n");
        status = true;
    }
    else
    {
        bool invalidParam = false;
        int value = atoi(frequency ? frequency : "0");

        status = pInst->set_periodic_frequency(&invalidParam, value);
        if (status)
            cliprintf("self test periodic run frequency set to %s\n", frequency);
        else
            if(invalidParam)
                cliprintf("self test periodic run frequency not set, invalid parameter: '%i'\n", value);
            else
                cliprintferr("ERROR: failed to set self test periodic run frequency\n");
    }
    return status;
}

static bool selftest_periodic_cpu_threshold(wa_wsclient *pInst, const char *threshold)
{
    bool status;

    if(!pInst->is_enabled())
    {
        cliprintf("self test periodic run cpu threshold not set, feature disabled\n");
        status = true;
    }
    else
    {
        bool invalidParam = false;
        int value = atoi(threshold ? threshold : "0");

        status = pInst->set_periodic_cpu_threshold(&invalidParam, value);
        if (status)
            cliprintf("self test periodic run cpu threshold set to %s\n", threshold);
        else
            if(invalidParam)
                cliprintf("self test periodic run cpu threshold not set, invalid parameter: '%i%%'\n", value);
            else
                cliprintferr("ERROR: failed to set self test periodic run cpu threshold\n");
    }
    return status;
}

static bool selftest_periodic_dram_threshold(wa_wsclient *pInst, const char *threshold)
{
    bool status;

    if(!pInst->is_enabled())
    {
        cliprintf("self test periodic run dram threshold not set, feature disabled\n");
        status = true;
    }
    else
    {
        int value = atoi(threshold ? threshold : "0");

        status = pInst->set_periodic_dram_threshold(value);
        if (status)
            cliprintf("self test periodic run dram threshold set to %s\n", threshold);
        else
            cliprintferr("ERROR: failed to set self test periodic run dram threshold\n");
    }
    return status;
}
#endif /* WA_DEBUG */

/* End of doxygen group */
/*! @} */
