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
 * @file wa_main.c
 *
 * @brief This file contains main function that starts WE.
 */

/** @addtogroup WA_MAIN
 *  @{
 */

/*****************************************************************************
 * STANDARD INCLUDE FILES
 *****************************************************************************/
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/
#include "wa_comm.h"
#include "wa_comm_ws.h"
#include "wa_debug.h"
#include "wa_diag.h"
#include "wa_diag_hdd.h"
#include "wa_diag_sysinfo.h"
#include "wa_diag_prev_results.h"
#include "wa_diag_tuner.h"
#include "wa_diag_avdecoder_qam.h"
#include "wa_diag_hdmiout.h"
#include "wa_diag_ir.h"
#include "wa_diag_flash.h"
#include "wa_iarm.h"
#include "wa_diag_dram.h"
#include "wa_diag_mcard.h"
#include "wa_diag_moca.h"
#include "wa_diag_rf4ce.h"
#include "wa_diag_modem.h"
#include "wa_init.h"
#include "wa_json.h"
#include "wa_osa.h"
#include "wa_stest.h"
#include "wa_snmp_client.h"
#include "wa_log.h"

/*****************************************************************************
 * GLOBAL VARIABLE DEFINITIONS
 *****************************************************************************/
bool shouldQuit = false;

/*****************************************************************************
 * LOCAL DEFINITIONS
 *****************************************************************************/

#define CONFIG_FILE_NAME "hwselftest.conf"

#ifndef SYSCONFDIR
#define SYSCONFDIR "/etc/hwselftest"
#endif

#define INIT_SLEEP_USEC 10000 /* 10 millisec */
#define INIT_COUNT_MAX 100    /* 100 x 10 millisec = 1 second */

/*****************************************************************************
 * LOCAL TYPES
 *****************************************************************************/

/*****************************************************************************
 * LOCAL FUNCTION PROTOTYPES
 *****************************************************************************/

static json_t * loadConfigFile(const char * filename);
static json_t * loadConfig(const char * commandLineFilename);

/*****************************************************************************
 * LOCAL VARIABLE DECLARATIONS
 *****************************************************************************/
static WA_COMM_adaptersConfig_t adapters[] =
{
        {"comm_ws", WA_COMM_WS_Init, WA_COMM_WS_Exit, WA_COMM_WS_Callback, NULL, NULL},
        {NULL, NULL, NULL, NULL, NULL, NULL}
};

static WA_DIAG_proceduresConfig_t diags[] =
{
        {"sysinfo_info", NULL, NULL, WA_DIAG_SYSINFO_Info, NULL, NULL, NULL},
        {"hdd_status",  NULL, NULL, WA_DIAG_HDD_status, NULL, NULL, NULL },
        {"tuner_status", NULL, NULL, WA_DIAG_TUNER_status, NULL, NULL, NULL },
        {"avdecoder_qam_status", WA_DIAG_AVDECODER_QAM_init, NULL, WA_DIAG_AVDECODER_QAM_status, NULL, NULL, NULL },
        {"hdmiout_status", NULL, NULL, WA_DIAG_HDMIOUT_status, NULL, NULL, NULL },
        {"flash_status", NULL, NULL, WA_DIAG_FLASH_status, NULL, NULL, NULL },
        {"dram_status", NULL, NULL, WA_DIAG_DRAM_status, NULL, NULL, NULL },
        {"mcard_status", NULL, NULL, WA_DIAG_MCARD_status, NULL, NULL, NULL },
        {"moca_status", NULL, NULL, WA_DIAG_MOCA_status, NULL, NULL, NULL },
        {"rf4ce_status", NULL, NULL, WA_DIAG_RF4CE_status, NULL, NULL, NULL },
        {"ir_status", NULL, NULL, WA_DIAG_IR_status, NULL, NULL, NULL},
        {"modem_status", NULL, NULL, WA_DIAG_MODEM_status, NULL, NULL, NULL },
        {"previous_results", NULL, NULL, WA_DIAG_PREV_RESULTS_Info, NULL, NULL, NULL },
        /* END OF LIST */
        { fnc:NULL}
};

char * configCheckDirectories[] =
{
        ".",
        SYSCONFDIR
};

/*****************************************************************************
 * FUNCTION DEFINITIONS
 *****************************************************************************/

static int received = 0;
void sig_usr(int signo){
    if(signo==SIGUSR1)
    {
        received = 1;
    }
    return;
}

int main(int argc, char *argv[])
{
    int status;
    int port = -1;
    const char * configFileName = NULL;
    json_t * configs = NULL;
    int count_sleeps = 0;
    pid_t ppid, pid, sid;
    
    if (getenv("HW_TEST_SVC")==NULL)
    {
        fprintf(stderr, "Diagnostic agent can't be excuted out of service\n");
        exit(1);
    }

    /* Daemonize the service */

    signal(SIGUSR1,sig_usr);
    ppid = getpid(); /* get parent PID */
#if WA_DEBUG
    WA_DBG("parent PID = %d\n", ppid);
#endif
    pid = fork();
    if (pid == 0)
    {
        sid = setsid();
        if (sid < 0)
        {
            fprintf(stderr, "Failed to create new SID for child process\n");
            exit(1);
        }
    }
    else if (pid > 0)
    {
        /* Parent process successfully created child process */
#if WA_DEBUG
        WA_DBG("child PID = %d\n", pid);
#endif
        while(!received)
        {
            if(count_sleeps>INIT_COUNT_MAX)
            {
                fprintf(stderr, "Child process has not sent signal - initialization failed\n");
                exit(1);
            }
            usleep(INIT_SLEEP_USEC);
            count_sleeps++;
        }
#if WA_DEBUG
        WA_DBG("count_sleeps = %d\n", count_sleeps);
#endif
        /* Child process has sent signal - initialization complete */
        exit(0);
    }
    else
    {
        /* Failed to create child process */
        fprintf(stderr, "Failed to create child process, fork returned %d\n", pid);
        exit(1);
    }

    WA_ENTER("main(argc=%d)\n", argc);

    WA_LOG_Init();

    for (int argi = 1; argi < argc; argi++)
    {
        if (strncmp(argv[argi], "--config=", 9) == 0)
        {
            configFileName = argv[argi] + 9;
        }
#if WA_DEBUG
        /* allow specifying listening port only on debug builds */
        else
        {
            port = atoi(argv[argi]);
            if (port == 0)
                port = -1;
        }
#endif /* WA_DEBUG */
    }

    configs = loadConfig(configFileName);

    if (port != -1)
    {
        WA_UTILS_JSON_NestedSetNew(configs, json_integer(port + 1), "adapters", "comm_ws", "port", "");
    }

    /* Fill in config fields in adapter definition table with json taken from configuration */
    {
        json_t * adaptersConfig = json_object_get(configs, "adapters");
        if (adaptersConfig && json_is_object(adaptersConfig))
        {
            for (WA_COMM_adaptersConfig_t * adapter = &adapters[0]; adapter->name; adapter++)
            {
                json_t * adapterConfig = json_object_get(adaptersConfig, adapter->name);
                if (adapterConfig && json_is_object(adapterConfig))
                {
                    adapter->config = adapterConfig;
                }
            }
        }
    }

    /* Fill in config fields in diag definition table with json taken from configuration */
    {
        json_t * diagsConfig = json_object_get(configs, "diags");
        if (diagsConfig && json_is_object(diagsConfig))
        {
            for (WA_DIAG_proceduresConfig_t * diag = &diags[0]; diag->name; diag++)
            {
                json_t * diagConfig = json_object_get(diagsConfig, diag->name);
                if (diagConfig && json_is_object(diagConfig))
                {
                    diag->config = diagConfig;
                }
            }
        }
    }

    status = WA_OSA_Init();
    if(status != 0)
    {
        WA_ERROR("WA_OSA_Init():%d\n", status);
        goto end;
    }

    status = WA_UTILS_IARM_Init();
    if(status != 0)
    {
        WA_ERROR("WA_UTILS_IARM_Init():%d\n", status);
        goto err_wa;
    }

    status = WA_UTILS_SNMP_Init();
    if(status != 0)
    {
        WA_ERROR("WA_UTILS_SNMP_Init():%d\n", status);
        goto err_snmp;
    }
#ifdef WA_STEST
    WA_STEST_Run();
#else
    status = WA_INIT_Init(adapters, diags);
    if(status != 0)
    {
        WA_ERROR("WA_INIT_Init():%d\n", status);
        goto err_wa;
    }

#if WA_DEBUG
    WA_DBG("sending SIGUSR1\n");
#endif

    kill(ppid,SIGUSR1);  /* Send signal to parent process that initialization is complete */

    WA_INFO("WA_INIT_Init():Child process started\n");
    while(!shouldQuit)
        WA_OSA_TaskSleep(1000);

    WA_INIT_Exit();
#endif
    status = WA_UTILS_SNMP_Exit();
    if(status != 0)
    {
        WA_ERROR("WA_UTILS_SNMP_Exit():%d\n", status);
    }

    err_snmp:
    status = WA_UTILS_IARM_Term();
    if(status != 0)
    {
        WA_ERROR("WA_UTILS_IARM_Term():%d\n", status);
    }

    err_wa:
    WA_OSA_Exit();
    end:

    CLIENT_LOG("Agent exited");

    json_decref(configs);

    if (status)
        fprintf(stderr, "hwselftest agent failed\n");

    WA_RETURN("main():%d\n", status);
    return status;
}



/*****************************************************************************
 * LOCAL FUNCTIONS
 *****************************************************************************/

static json_t * loadConfigFile(const char * filename)
{
    json_error_t error;
    json_t * result;
    printf("Loading config file from: %s\n", filename);
    result = json_load_file(filename, 0, &error);
    if (!result)
    {
        WA_ERROR("Unable to parse configuration file \"%s\", position %i: %s\n", filename, error.position, error.text);
        fprintf(stderr, "Failed to open config file \"%s\"\n", filename);
        result = json_object();
    }
    return result;
}

static char * findConfigFile()
{
    const size_t baseNameSize = strlen(CONFIG_FILE_NAME);
    char * buf = NULL;
    size_t bufSize = 0;

    for (int i = 0; i < sizeof(configCheckDirectories) / sizeof(configCheckDirectories[0]); ++i)
    {
        const char * dir = configCheckDirectories[i];
        const size_t dirNameSize = strlen(dir);
        const size_t newSize = dirNameSize + baseNameSize + 2; // slash and terminating NUL
        if (newSize > bufSize)
        {
            char * newBuf = realloc(buf, newSize);
            if (newBuf)
            {
                buf = newBuf;
                bufSize = newSize;
            }
        }
        if (bufSize >= newSize)
        {
            strncpy(buf, dir, dirNameSize);
            buf[dirNameSize] = '/';
            strncpy(buf + dirNameSize + 1, CONFIG_FILE_NAME, baseNameSize);
            buf[dirNameSize + 1 + baseNameSize] = 0;
        }
        printf("Looking for %s... ", buf);
        if (access(buf, F_OK) == 0)
        {
            printf("found.\n");
            return buf;
        }
        printf("not found.");
    }

    return NULL;
}

static json_t * loadConfig(const char * filename)
{
    json_t * result = NULL;
    char * toFree = NULL;

    if (!filename)
    {
        toFree = findConfigFile();
        filename = toFree;
    }

    if (filename)
    {
        result = loadConfigFile(filename);
    }
    else
    {
        result = json_object();
    }

    free(toFree);

    return result;
}

/* End of doxygen group */
/*! @} */

/* EOF */
