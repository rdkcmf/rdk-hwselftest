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
 * @file wa_init.c
 *
 * @brief This file contains main function that initialize WE.
 */

/** @addtogroup WA_INIT
 *  @{
 */

/*****************************************************************************
 * STANDARD INCLUDE FILES
 *****************************************************************************/
#include <stdbool.h>

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/
#include "wa_comm.h"
#include "wa_diag.h"
#include "wa_id.h"
#include "wa_init.h"
#include "wa_osa.h"
#include "wa_debug.h"
#include "wa_agg.h"

/*****************************************************************************
 * GLOBAL VARIABLE DEFINITIONS
 *****************************************************************************/
void *WA_INIT_IncomingQ = NULL;

/*****************************************************************************
 * LOCAL DEFINITIONS
 *****************************************************************************/
#define WA_INIT_INCOME_Q_DEEP 10

#define WA_INIT_INCOME_Q_EXIT_MSG "exit"

#define WA_MSG_ID_SUFFIX_MASK 0x0000ffff
/*****************************************************************************
 * LOCAL TYPES
 *****************************************************************************/

/*****************************************************************************
 * LOCAL FUNCTION PROTOTYPES
 *****************************************************************************/
static void *AgentTask(void *p);

/*****************************************************************************
 * LOCAL VARIABLE DECLARATIONS
 *****************************************************************************/
static void *agentTaskHandle;

/*****************************************************************************
 * FUNCTION DEFINITIONS
 *****************************************************************************/

int WA_INIT_Init(WA_COMM_adaptersConfig_t * const adapters,
        WA_DIAG_proceduresConfig_t * const diags)
{
    int status = -1, s1;
    WA_ENTER("WA_INIT_Init(adapters=%p)\n", adapters);

    status = WA_UTILS_JSON_Init();
    if(status != 0)
    {
        WA_ERROR("WA_INIT_Init(): WA_UTILS_JSON_Init():%d\n", status);
        goto end;
    }

    status = WA_UTILS_ID_Init();
    if(status != 0)
    {
        WA_ERROR("WA_INIT_Init(): WA_UTILS_ID_Init():%d\n", status);
        goto id_err;
    }

    {
        WA_UTILS_ID_t id;
        WA_UTILS_ID_Generate(&id);
    }
    WA_INIT_IncomingQ = WA_OSA_QCreate(WA_INIT_INCOME_Q_DEEP, sizeof(WA_OSA_Qjmsg_t));
    if(WA_INIT_IncomingQ == NULL)
    {
        WA_ERROR("WA_INIT_Init(): WA_OSA_QCreate(WA_INIT_IncomingQ) error\n");
        goto income_q_err;
    }

    status = WA_COMM_Init(adapters);
    if(status != 0)
    {
        WA_ERROR("WA_INIT_Init(): WA_COMM_Init():%d\n", status);
        goto comm_err;
    }

    status = WA_DIAG_Init(diags);
    if(status != 0)
    {
        WA_ERROR("WA_INIT_Init(): WA_DIAG_Init():%d\n", status);
        goto diag_err;
    }

    status = WA_AGG_Init(diags);
    if(status != 0)
    {
        WA_ERROR("WA_INIT_Init(): WA_AGG_Init():%d\n", status);
        goto agg_err;
    }

    agentTaskHandle = WA_OSA_TaskCreate(NULL, 0, AgentTask, NULL, WA_OSA_SCHED_POLICY_RT, WA_OSA_TASK_PRIORITY_MAX);
    if(agentTaskHandle == NULL)
    {
        WA_ERROR("WA_INIT_Init(): WA_OSA_TaskCreate(AgentTask): error\n");
        goto agent_task_err;
    }

    /*  */

    goto end;

agent_task_err:
    s1 = WA_AGG_Exit();
    if(s1 != 0)
    {
        WA_ERROR("WA_INIT_Init(): WA_AGG_Exit():%d\n", s1);
    }

agg_err:
    s1 = WA_COMM_Exit();
    if(s1 != 0)
    {
        WA_ERROR("WA_INIT_Init(): WA_COMM_Exit():%d\n", s1);
    }

diag_err:
    s1 = WA_OSA_QDestroy(WA_DIAG_IncomingQ);
    if(s1 != 0)
    {
        WA_ERROR("WA_INIT_Init():  WA_OSA_QDestroy(WA_DIAG_IncomingQ): %d\n", s1);
    }

comm_err:
    s1 = WA_OSA_QDestroy(WA_INIT_IncomingQ);
    if(s1 != 0)
    {
        WA_ERROR("WA_INIT_Init():  WA_OSA_QDestroy(WA_INIT_IncomingQ): %d\n", s1);
    }

income_q_err:
    s1 = WA_UTILS_ID_Exit();
    if(s1 != 0)
    {
        WA_ERROR("WA_INIT_Init(): WA_UTILS_ID_Exit():%d\n", s1);
    }
id_err:
end:
    WA_RETURN("%d\n", status);
    return status;
}

int WA_INIT_Exit(void)
{
    int status = -1;
    WA_ENTER("WA_INIT_Exit()\n");

    status = WA_OSA_QSend(WA_INIT_IncomingQ,
            NULL,
            0,
            WA_OSA_Q_PRIORITY_MAX);
    if(status != 0)
    {
        WA_ERROR("WA_INIT_Exit(): WA_OSA_QSend(): %d\n", status);
    }

    status = WA_OSA_TaskJoin(agentTaskHandle, NULL);
    if(status != 0)
    {
        WA_ERROR("WA_INIT_Exit(): WA_OSA_TaskJoin(AgentTask): error\n");
    }

    status = WA_OSA_TaskDestroy(agentTaskHandle);
    if(status != 0)
    {
        WA_ERROR("WA_INIT_Exit(): WA_OSA_TaskDestroy(AgentTask): error\n");
    }

    status = WA_AGG_Exit();
    if(status != 0)
    {
        WA_ERROR("WA_INIT_Exit(): WA_AGG_Exit():%d\n", status);
    }

    status = WA_DIAG_Exit();
    if(status != 0)
    {
        WA_ERROR("WA_INIT_Exit(): WA_DIAG_Exit():%d\n", status);
    }

    status = WA_COMM_Exit();
    if(status != 0)
    {
        WA_ERROR("WA_INIT_Exit(): WA_COMM_Exit():%d\n", status);
    }

    status = WA_OSA_QDestroy(WA_INIT_IncomingQ);
    if(status != 0)
    {
        WA_ERROR("WA_INIT_Exit():  WA_OSA_QDestroy(WA_INIT_IncomingQ): %d\n", status);
    }

    status = WA_UTILS_ID_Exit();
    if(status != 0)
    {
        WA_ERROR("WA_INIT_Exit(): WA_UTILS_ID_Exit():%d\n", status);
    }

    WA_RETURN("%d\n", 0);
    return 0;
}



/*****************************************************************************
 * LOCAL FUNCTIONS
 *****************************************************************************/
extern bool shouldQuit;
static void *AgentTask(void *p)
{
    int status;
    WA_OSA_Qjmsg_t qjmsg;
    unsigned int qprio;
    char *s;

    WA_ENTER("AgentTask(p=%p)\n", p);

    status = WA_OSA_TaskSetName("WAagent");
    if(status != 0)
    {
        WA_ERROR("AgentTask(): WA_OSA_TaskSetName(): %d\n", status);
        goto end;
    }

    while(1)
    {
        status = WA_OSA_QReceive(WA_INIT_IncomingQ, (char * const)&qjmsg, sizeof(WA_OSA_Qjmsg_t), &qprio);
        if(status == 0)
        {
            WA_INFO("AgentTask(): WA_OSA_QReceive(): empty\n");
            break;
        }
        if(status != sizeof(WA_OSA_Qjmsg_t))
        {
            WA_ERROR("AgentTask(): WA_OSA_QReceive(): %d\n", status);
            goto end;
        }

#ifdef WA_DEBUG
        {
            char *msg;
            msg = json_dumps(qjmsg.json, JSON_ENCODE_ANY);
            if(!msg)
            {
                WA_ERROR("AgentTask(): json_dumps(): error\n");
            }
            else
            {
                WA_INFO("AgentTask(): WA_OSA_QReceive(): %s\n", msg);
                free(msg);
            }
        }
#else
        WA_INFO("AgentTask(): WA_OSA_QReceive(): %p\n", qjmsg.json);
#endif
        /* Consideration: Add handling messages to particular instances:
         * - if message is with "id" store this json as a reference till response is sent,
         *   then deliver to instance with cookie to this json
         * - if notification, just deliver to instance with NULL cookie
         * */
        {
            WA_INFO("AgentTask() - THIS IS FOR TEST PURPOSE ONLY\n");

            switch(qjmsg.from & ~WA_MSG_ID_SUFFIX_MASK)
            {
            case WA_COMM_MSG_ID_PREFIX:
                WA_INFO("AgentTask() msg from WA_COMM_MSG_ID_PREFIX\n");

                status = json_unpack(qjmsg.json, "{ss}", "method", &s); //s will be released when releasing json object
                if((status == 0) && (strcmp(s,"exit") == 0))
                {
                    WA_INFO("AgentTask() got exit message.\n");
                    json_decref(qjmsg.json);
                       shouldQuit = true;
                       break;
                }
                status = WA_OSA_QTimedRetrySend(WA_DIAG_IncomingQ,
                        (const char * const)&qjmsg,
                        sizeof(WA_OSA_Qjmsg_t),
                        WA_OSA_Q_PRIORITY_MAX,
                        WA_OSA_Q_SEND_TIMEOUT_DEFAULT,
                        WA_OSA_Q_SEND_RETRY_DEFAULT);
                if(status != 0)
                {
                    json_decref(qjmsg.json);
                    WA_ERROR("AgentTask(): WA_OSA_QSend(): %d\n", status);
                }
                break;

            case WA_DIAG_MSG_ID_PREFIX:
                WA_INFO("AgentTask() msg from WA_DIAG_MSG_ID_PREFIX\n");
                status = WA_OSA_QTimedRetrySend(WA_COMM_IncomingQ,
                        (const char * const)&qjmsg,
                        sizeof(WA_OSA_Qjmsg_t),
                        WA_OSA_Q_PRIORITY_MAX,
                        WA_OSA_Q_SEND_TIMEOUT_DEFAULT,
                        WA_OSA_Q_SEND_RETRY_DEFAULT);
                if(status != 0)
                {
                    json_decref(qjmsg.json);
                    WA_ERROR("AgentTask(): WA_OSA_QSend(): %d\n", status);
                }
                break;
            default:
                json_decref(qjmsg.json);
                WA_INFO("AgentTask() unknown sender: 0x%0x\n", qjmsg.from);
                break;
            }
        }
    }
end:
    WA_RETURN("AgentTask(): %p\n", p);
    return p;
}

/* End of doxygen group */
/*! @} */

/* EOF */
