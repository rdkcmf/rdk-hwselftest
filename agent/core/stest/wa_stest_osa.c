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
 * @file wa_stest_osa.c
 *
 * @brief This file contains OSA test functions.
 */

/** @addtogroup WA_STEST
 *  @{
 */

/*****************************************************************************
 * STANDARD INCLUDE FILES
 *****************************************************************************/
#include <string.h>

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/
#include "wa_debug.h"
#include "wa_osa.h"

/*****************************************************************************
 * GLOBAL VARIABLE DEFINITIONS
 *****************************************************************************/

/*****************************************************************************
 * LOCAL DEFINITIONS
 *****************************************************************************/

/*****************************************************************************
 * LOCAL TYPES
 *****************************************************************************/

/*****************************************************************************
 * LOCAL FUNCTION PROTOTYPES
 *****************************************************************************/

/*****************************************************************************
 * LOCAL VARIABLE DECLARATIONS
 *****************************************************************************/

/*****************************************************************************
 * FUNCTION DEFINITIONS
 *****************************************************************************/

static void *t1(void *p)
{
    int ms = (int)(uintptr_t)p;

    WA_ENTER("t1(p=%p)\n", p);

    WA_OSA_TaskSleep(ms);

    WA_RETURN("t1() exit\n");
    return p;
}

static int Qtest(void)
{
    int status = -1;
    void *q;
    int lenLow, lenHigh;
    WA_OSA_QMsg_t msg;
    unsigned int qprio;

    WA_ENTER("Qtest()\n");

    q = WA_OSA_QCreate(10, sizeof(WA_OSA_QMsg_t));
    if(q == NULL)
    {
        WA_ERROR("Qtest(): WA_OSA_QCreate() error\n");
        goto end;
    }

    strcpy(msg.string, "message low");
    lenLow = strlen(msg.string)+1;
    status = WA_OSA_QSend(q, &msg, lenLow, 0);
    if(status != 0)
    {
        WA_ERROR("Qtest(): WA_OSA_QSend(): %d\n", status);
    }

    strcpy(msg.string, "message high");
    lenHigh = strlen(msg.string)+1;
    status = WA_OSA_QSend(q, &msg, lenHigh, 7);
    if(status != 0)
    {
        WA_ERROR("Qtest(): WA_OSA_QSend(): %d\n", status);
    }

    strcpy(msg.string, "dummy string");

    status = WA_OSA_QReceive(q, &msg, sizeof(WA_OSA_QMsg_t), &qprio);
    if((status != lenHigh) || strcmp(msg.string, "message high"))
    {
        WA_ERROR("Qtest(): WA_OSA_QReceive(): %d\n", status);
    }
    else
    {
        WA_INFO("Qtest(): WA_OSA_QReceive(): \"%s\" prio=%u\n", msg.string, qprio);
    }
    status = WA_OSA_QReceive(q, &msg, sizeof(WA_OSA_QMsg_t), &qprio);
    if((status != lenLow) || strcmp(msg.string, "message low"))
    {
        WA_ERROR("Qtest(): WA_OSA_QReceive(): %d\n", status);
    }
    else
    {
        WA_INFO("Qtest(): WA_OSA_QReceive(): \"%s\" prio=%d\n", msg.string, qprio);
    }

    status = WA_OSA_QDestroy(q);
    if(status != 0)
    {
        WA_ERROR("Qtest(): WA_OSA_QDestroy(): %d\n", status);
    }
end:
    WA_RETURN("Qtest() exit\n");
    return status;
}

static int Tasktest(void)
{
    int status = 0;
    void *t;
    void *tretval;

    WA_ENTER("Tasktest()\n");

    t = WA_OSA_TaskCreate(NULL, 0, t1, (void *)(uintptr_t)1000, WA_OSA_SCHED_POLICY_RT, WA_OSA_TASK_PRIORITY_MAX);
    if(t == NULL)
    {
        WA_ERROR("Tasktest(): WA_OSA_TaskCreate(t1): error\n");
        goto end;
    }

    WA_INFO("Tasktest(): waiting t1\n");
    status = WA_OSA_TaskJoin(t, &tretval);
    if(status != 0)
    {
        WA_ERROR("Tasktest(): WA_OSA_TaskJoin(t1): error\n");
    }
    status = WA_OSA_TaskDestroy(t);
    if(status != 0)
    {
        WA_ERROR("Tasktest(): WA_OSA_TaskDestroy(t1): error\n");
    }

    WA_INFO("Tasktest(): waiting t1: %p\n", tretval);
end:
    WA_RETURN("Tasktest() exit\n");
    return status;
}

int WA_STEST_OSA_Run(void)
{
    int status = 0;

    WA_ENTER("WA_STEST_OSA_Run()\n");

    status = Tasktest();
    if(status != 0)
    {
        WA_ERROR("WA_STEST_OSA_Run(): Tasktest(): error\n");
        goto end;
    }
    WA_INFO("WA_STEST_OSA_Run(): Tasktest(): PASS\n");

    status = Qtest();
    if(status != 0)
    {
        WA_ERROR("WA_STEST_OSA_Run(): Qtest(): error\n");
    }

end:
    WA_RETURN("WA_STEST_OSA_Run():%d\n", status);
    return status;
}



/*****************************************************************************
 * LOCAL FUNCTIONS
 *****************************************************************************/

/* End of doxygen group */
/*! @} */

/* EOF */
