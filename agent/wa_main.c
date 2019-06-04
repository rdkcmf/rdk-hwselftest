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
#include <semaphore.h>
#include <sys/time.h>
#include <errno.h>

#include "breakpad_wrapper.h"

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/
#include "wa_comm.h"
#include "wa_comm_ws.h"
#include "wa_debug.h"
#include "wa_diag.h"
#include "wa_iarm.h"
#include "wa_init.h"
#include "wa_json.h"
#include "wa_osa.h"
#include "wa_stest.h"
#include "wa_snmp_client.h"
#include "wa_log.h"
#include "wa_config.h"
#include "wa_version.h"

/*****************************************************************************
 * LOCAL DEFINITIONS
 *****************************************************************************/
#define CHILD_INIT_TIMEOUT 3000 /* 3s */
#define NO_CONNECTION_TIMEOUT 5000 /* 5s */

/*****************************************************************************
 * LOCAL TYPES
 *****************************************************************************/
typedef enum {
    quitStarting = 0,
    quitPrevent,
    quitQuit
}quit_t;

/*****************************************************************************
 * LOCAL VARIABLE DEFINITIONS
 *****************************************************************************/
static void *quitCondVar;
static quit_t quitState = quitStarting;
static void *childInitSem;

/*****************************************************************************
 * LOCAL FUNCTION PROTOTYPES
 *****************************************************************************/
static void sig_usr(int signo);

/*****************************************************************************
 * FUNCTION DEFINITIONS
 *****************************************************************************/

int main(int argc, char *argv[])
{
    int status, exitStatus, exitReason;
    const char * configFileName = NULL;
    struct sigaction sa_new, sa_old;
    pid_t ppid, pid, sid;

    WA_ENTER("main(argc=%d) [PARENT]\n", argc);

    /* Invoke the Breakpad exception handler registration interface */
    breakpad_ExceptionHandler();

    if (getenv("HW_TEST_SVC")==NULL)
    {
        fprintf(stderr, "hwselftest: diagnostic agent can't be excuted out of service\n");
        exit(1);
    }

    /* Daemonize the service */
    memset(&sa_new, 0, sizeof(sa_new));
    sigemptyset(&sa_new.sa_mask);
    sa_new.sa_handler  = sig_usr;
    if(sigaction(SIGUSR1, &sa_new, &sa_old) == -1)
    {
         WA_ERROR("hwselftest: failed install signal handler for SIGUSR1");
         exit(1);
    }

    ppid = getpid(); /* get parent PID */
    WA_DBG("parent PID = %d\n", ppid);

    childInitSem = malloc(sizeof(sem_t));
    if (!childInitSem || (sem_init(childInitSem, 0, 0) != 0))
    {
        fprintf(stderr, "hwselftest: can't create child init semaphore\n");
        exit(1);
    }

    pid = fork();
    if (pid == 0)
    {
        sid = setsid();
        if (sid < 0)
        {
            fprintf(stderr, "hwselftest: failed to create new SID for child process\n");
            exit(1);
        }
    }
    else if (pid > 0)
    {
        /* Parent process successfully created child process */
        WA_DBG("child PID = %d\n", pid);
        struct timespec absTime;
        struct timeval timeOfDay;
        int wait_status;

        gettimeofday(&timeOfDay, NULL);
        absTime.tv_nsec = timeOfDay.tv_usec * 1000 + (CHILD_INIT_TIMEOUT % 1000) * 1000000;
        absTime.tv_sec  = timeOfDay.tv_sec + (CHILD_INIT_TIMEOUT / 1000) + (absTime.tv_nsec / 1000000000);
        absTime.tv_nsec %= 1000000000;

        while ((wait_status = sem_timedwait(childInitSem, &absTime)) == -1 && (errno == EINTR))
            continue;

        if (wait_status == -1)
        {
            fprintf(stderr, "hwselftest: child process has not sent signal - initialization failed\n");
            exit(1);
        }

        /* Child process has sent signal - initialization complete */
        WA_INFO("main(): child process created, parent process exiting...\n");
        exit(0);
    }
    else
    {
        /* Failed to create child process */
        fprintf(stderr, "hwselftest: failed to create child process, fork returned %d\n", pid);
        exit(1);
    }

    WA_ENTER("main(argc=%d) [CHILD]\n", argc);

    if(sigaction(SIGUSR1, &sa_old, NULL) == -1)
    {
        WA_ERROR("hwselftest: failed to restore previous signal handler for SIGUSR1");
        exit(1);
    }

    WA_LOG_Init();

    for (int argi = 1; argi < argc; argi++)
    {
        if (strncmp(argv[argi], "--config=", 9) == 0)
        {
            configFileName = argv[argi] + 9;
        }
    }

    status = WA_OSA_Init();
    if(status != 0)
    {
        WA_ERROR("WA_OSA_Init():%d\n", status);
        exitReason = 1;
        goto end;
    }

    quitCondVar = WA_OSA_CondCreate();
    if(quitCondVar == NULL)
    {
        WA_ERROR("WA_OSA_CondCreate()\n");
        exitReason = 2;
        goto err_cond;
    }

    status = WA_CONFIG_Init(configFileName);
    if(status != 0)
    {
        WA_ERROR("WA_CONFIG_Init():%d\n", status);
        exitReason = 3;
        goto err_config;
    }

    status = WA_UTILS_IARM_Init();
    if(status != 0)
    {
        WA_ERROR("WA_UTILS_IARM_Init():%d\n", status);
        exitReason = 4;
        goto err_iarm;
    }

    status = WA_UTILS_SNMP_Init();
    if(status != 0)
    {
        WA_ERROR("WA_UTILS_SNMP_Init():%d\n", status);
        exitReason = 5;
        goto err_snmp;
    }

#ifdef WA_STEST
    WA_STEST_Run();
#else
    status = WA_INIT_Init(WA_CONFIG_GetAdapters(), WA_CONFIG_GetDiags());
    if(status != 0)
    {
        WA_ERROR("WA_INIT_Init():%d\n", status);
        exitReason = 6;
        goto err_init;
    }

    WA_DBG("sending SIGUSR1\n");
    kill(ppid,SIGUSR1);  /* Send signal to parent process that initialization is complete */

    WA_INFO("main(): child process started\n");
    WA_OSA_CondLock(quitCondVar);

    CLIENT_LOG("Agent started, ver. %s", WA_VERSION);

    exitReason = 0;

    if (getenv("HW_TEST_NO_CONN_INIT_TIMEOUT") != NULL)
        printf("hwselftest: diagnostic agent will not timeout if no connection\n");
    else
    {
        if(quitState == quitStarting)
            if(WA_OSA_CondTimedWait(quitCondVar, NO_CONNECTION_TIMEOUT) == 1) //timeout
            {
                WA_INFO("main(): no connection, quitting...\n");
                exitReason = 7;
                quitState = quitQuit;
            }
    }

    while(quitState != quitQuit)
        WA_OSA_CondWait(quitCondVar);

    WA_OSA_CondUnlock(quitCondVar);

    exitStatus = WA_INIT_Exit();
    if (exitStatus != 0)
    {
        WA_ERROR("WA_INIT_Exit(): error %d\n", exitStatus);
    }

err_init:
#endif /* WA_STEST */

    exitStatus = WA_UTILS_SNMP_Exit();
    if(exitStatus != 0)
    {
        WA_ERROR("WA_UTILS_SNMP_Exit(): error %d\n", exitStatus);
    }

err_snmp:
    exitStatus = WA_UTILS_IARM_Term();
    if(exitStatus != 0)
    {
        WA_ERROR("WA_UTILS_IARM_Term(): error %d\n", exitStatus);
    }

err_iarm:
    exitStatus = WA_CONFIG_Exit();
    if(exitStatus != 0)
    {
        WA_ERROR("WA_CONFIG_Exit(): error %d\n", exitStatus);
    }

err_config:
    exitStatus = WA_OSA_CondDestroy(quitCondVar);
    if(exitStatus != 0)
    {
        WA_ERROR("WA_OSA_CondDestroy(): error %d\n", exitStatus);
    }

err_cond:
    exitStatus = WA_OSA_Exit();
    if(exitStatus != 0)
        WA_ERROR("WA_OSA_Exit(): error %d\n", exitStatus);

end:
    if(exitReason)
        CLIENT_LOG("Agent exited (%d)", exitReason);
    else
        CLIENT_LOG("Agent exited");

    if (status)
        fprintf(stderr, "hwselftest: agent failed\n");

    WA_RETURN("main():%d\n", status);
    return status;
}

void WA_MAIN_Quit(bool quit)
{
    WA_ENTER("WA_MAIN_Quit(quit=%i)\n", quit);

    WA_OSA_CondLock(quitCondVar);
    quitState = quit ? quitQuit : quitPrevent;
    WA_OSA_CondSignal(quitCondVar);
    WA_OSA_CondUnlock(quitCondVar);

    WA_RETURN("WA_MAIN_Quit(): exit\n");
}

/*****************************************************************************
 * LOCAL FUNCTIONS
 *****************************************************************************/

static void sig_usr(int signo)
{
    if (signo == SIGUSR1)
        sem_post(childInitSem);
}

/* End of doxygen group */
/*! @} */

/* EOF */
