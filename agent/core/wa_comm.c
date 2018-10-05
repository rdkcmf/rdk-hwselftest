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
 * @file wa_comm.c
 *
 * @brief This file contains main function for comm module.
 */

/** @addtogroup WA_COMM
 *  @{
 */

/*****************************************************************************
 * STANDARD INCLUDE FILES
 *****************************************************************************/
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/
#include "wa_comm.h"
#include "wa_id.h"
#include "wa_init.h"
#include "wa_osa.h"
#include "wa_debug.h"

/*****************************************************************************
 * GLOBAL VARIABLE DEFINITIONS
 *****************************************************************************/
void *WA_COMM_IncomingQ = NULL;

/*****************************************************************************
 * LOCAL DEFINITIONS
 *****************************************************************************/
#define WA_COMM_INCOME_Q_DEEP 50

#define WA_COMM_INCOME_Q_EXIT_MSG "exit"

/*****************************************************************************
 * LOCAL TYPES
 *****************************************************************************/
typedef struct
{
    uint32_t id;
    void *cookie;
    const WA_COMM_adaptersConfig_t *pConfig;
}WA_COMM_adapterConnectionContext_t;
WA_UTILS_LIST_DECLARE_LIST_TYPE(WA_COMM_adapterConnectionContext_t);

typedef struct
{
    void *mutex;
    WA_UTILS_ID_box_t idBox;
    WA_UTILS_LIST(WA_COMM_adapterConnectionContext_t) adaptersList;
}WA_COMM_adapterConnections_t;

/*****************************************************************************
 * LOCAL FUNCTION PROTOTYPES
 *****************************************************************************/
static void *CommTask(void *p);

/*****************************************************************************
 * LOCAL VARIABLE DECLARATIONS
 *****************************************************************************/
static void *commTaskHandle;
static WA_COMM_adapterConnections_t commAdapterConnections = {mutex:NULL};

static const WA_COMM_adaptersConfig_t *pAdaptersConfig;
static WA_UTILS_LIST_t adaptersHandles;

/*****************************************************************************
 * FUNCTION DEFINITIONS
 *****************************************************************************/

int WA_COMM_Init(const WA_COMM_adaptersConfig_t *adapters)
{
    int status = -1, s1;
    WA_COMM_adaptersConfig_t *pAdapter;
    void *iterator;
    void *adapterHandle;

    WA_ENTER("WA_COMM_Init(adaptors=%p)\n", adapters);

    if(commAdapterConnections.mutex != NULL)
    {
        WA_WARN("WA_COMM_Init(): COMM already initialized\n");
        status = 0;
        goto end;
    }

    pAdaptersConfig = adapters;
    WA_UTILS_LIST_ClearList(commAdapterConnections.adaptersList);
    WA_UTILS_LIST_ClearList(adaptersHandles);

    commAdapterConnections.mutex = WA_OSA_MutexCreate();
    if(commAdapterConnections.mutex == NULL)
    {
        WA_ERROR("WA_COMM_Init(): WA_OSA_MutexCreate(): error\n");
        goto err_mutex;
    }

    WA_COMM_IncomingQ = WA_OSA_QCreate(WA_COMM_INCOME_Q_DEEP, sizeof(WA_OSA_Qjmsg_t));
    if(WA_COMM_IncomingQ == NULL)
    {
        WA_ERROR("WA_COMM_Init(): WA_OSA_QCreate(WA_COMM_IncomingQ) error\n");
        goto income_q_err;
    }

    commTaskHandle = WA_OSA_TaskCreate(NULL, 0, CommTask, NULL, WA_OSA_SCHED_POLICY_RT, WA_OSA_TASK_PRIORITY_MAX);
    if(commTaskHandle == NULL)
    {
        WA_ERROR("WA_COMM_Init(): WA_OSA_TaskCreate(CommTask): error\n");
        goto comm_task_err;
    }

    for(pAdapter = (WA_COMM_adaptersConfig_t *)pAdaptersConfig;
            pAdapter && pAdapter->initFnc && pAdapter->exitFnc && pAdapter->callback;
            ++pAdapter)
    {
        adapterHandle = pAdapter->initFnc(pAdapter);
        if(adapterHandle == NULL)
        {
            WA_ERROR("WA_COMM_Init(): initFnc(): error\n");
            goto err_adapters;
        }

        iterator = WA_UTILS_LIST_AddBack(&adaptersHandles, adapterHandle);
        if(iterator == NULL)
        {
            WA_ERROR("WA_COMM_Init(): WA_UTILS_LIST_AddBack(): error\n");
            s1 = pAdapter->exitFnc(adapterHandle);
            if(s1 != 0)
            {
                WA_ERROR("WA_COMM_Init(): exitFnc(): %d\n", s1);
            }
            goto err_adapters;
        }
    }
    status = 0;
    goto end;

    err_adapters:
    for(iterator = WA_UTILS_LIST_FrontIterator(&adaptersHandles), pAdapter = (WA_COMM_adaptersConfig_t *)pAdaptersConfig;
            iterator != WA_UTILS_LIST_NO_ELEM;
            iterator = WA_UTILS_LIST_NextIterator(&adaptersHandles, iterator), ++pAdapter)
    {
        adapterHandle = (WA_COMM_adaptersConfig_t *)(WA_UTILS_LIST_DataAtIterator(&adaptersHandles, iterator));
        if(pAdapter->exitFnc)
        {
            s1 = pAdapter->exitFnc(adapterHandle);
            if(s1 != 0)
            {
                WA_ERROR("WA_COMM_Init(): WA_COMM_AdaperExit_t(): %d\n", s1);
            }
        }
    }
    WA_UTILS_LIST_Purge(&adaptersHandles);
    comm_task_err:
    s1 = WA_OSA_QDestroy(WA_COMM_IncomingQ);
    if(s1 != 0)
    {
        WA_ERROR("WA_COMM_Init():  WA_OSA_QDestroy(WA_COMM_IncomingQ): %d\n", s1);
    }

    income_q_err:
    s1 = WA_OSA_MutexDestroy(commAdapterConnections.mutex);
    if(s1 != 0)
    {
        WA_ERROR("WA_COMM_Init(): WA_OSA_MutexDestroy(): %d\n", s1);
    }
    commAdapterConnections.mutex = NULL;
    err_mutex:
    end:
    WA_RETURN("WA_COMM_Init(): %d\n", status);
    return status;
}

int WA_COMM_Exit(void)
{
    int status = -1;
    const WA_COMM_adaptersConfig_t *pAdapter;
    void *iterator;
    void *adapterHandle;

    WA_ENTER("WA_COMM_Exit()\n");

    if(commAdapterConnections.mutex == NULL)
    {
        WA_ERROR("WA_COMM_Exit(): COMM not initialized\n");
        status = 0;
        goto end;
    }

    for(iterator = WA_UTILS_LIST_FrontIterator(&adaptersHandles), pAdapter = pAdaptersConfig;
            iterator != WA_UTILS_LIST_NO_ELEM;
            iterator = WA_UTILS_LIST_NextIterator(&adaptersHandles, iterator), ++pAdapter)
    {
        adapterHandle = (WA_COMM_adaptersConfig_t *)(WA_UTILS_LIST_DataAtIterator(&adaptersHandles, iterator));
        if(pAdapter->exitFnc)
        {
            status = pAdapter->exitFnc(adapterHandle);
            if(status != 0)
            {
                WA_ERROR("WA_COMM_Exit(): WA_COMM_AdaperExit_t(): %d\n", status);
            }
        }
    }

    WA_UTILS_LIST_Purge(&adaptersHandles);

    status = WA_OSA_QSend(WA_COMM_IncomingQ,
            NULL,
            0,
            WA_OSA_Q_PRIORITY_MAX);
    if(status != 0)
    {
        WA_ERROR("WA_COMM_Exit(): WA_OSA_QSend(): %d\n", status);
    }

    status = WA_OSA_TaskJoin(commTaskHandle, NULL);
    if(status != 0)
    {
        WA_ERROR("WA_COMM_Exit(): WA_OSA_TaskJoin(CommTask): error\n");
    }

    status = WA_OSA_TaskDestroy(commTaskHandle);
    if(status != 0)
    {
        WA_ERROR("WA_COMM_Exit(): WA_OSA_TaskDestroy(CommTask): error\n");
    }

    status = WA_OSA_QDestroy(WA_COMM_IncomingQ);
    if(status != 0)
    {
        WA_ERROR("WA_COMM_Exit():  WA_OSA_QDestroy(WA_COMM_IncomingQ): %d\n", status);
    }

    status = WA_OSA_MutexDestroy(commAdapterConnections.mutex);
    if(status != 0)
    {
        WA_ERROR("WA_COMM_Exit(): WA_OSA_MutexDestroy(): %d\n", status);
    }
    commAdapterConnections.mutex = NULL;
    end:
    WA_RETURN("WA_COMM_Exit(): %d\n", status);
    return status;
}

void *WA_COMM_Register(const WA_COMM_adaptersConfig_t *config, void *cookie)
{
    int status = -1, s1;
    WA_COMM_adapterConnectionContext_t *pContext = NULL;
    WA_UTILS_ID_t id;

    WA_ENTER("WA_COMM_Register(config=%p cookie=%p)\n", config, cookie);

    if(commAdapterConnections.mutex == NULL)
    {
        WA_ERROR("WA_COMM_Register(): COMM not initialized\n");
        goto end;
    }

    if(config == NULL)
    {
        WA_ERROR("WA_COMM_Register(): invalid parameter\n");
        goto end;
    }

    status = WA_OSA_MutexLock(commAdapterConnections.mutex);
    if(status != 0)
    {
        WA_ERROR("WA_COMM_Register(): WA_OSA_MutexLock(): %d\n", status);
        goto err_mutex;
    }

    status = WA_UTILS_ID_GenerateUnsafe(&(commAdapterConnections.idBox), &id);
    if(status != 0)
    {
        WA_ERROR("WA_COMM_Register(): WA_UTILS_ID_GenerateUnsafe(): %d\n", status);
        goto err_id;
    }

    pContext = (WA_COMM_adapterConnectionContext_t *)WA_UTILS_LIST_AllocAddBack(&(commAdapterConnections.adaptersList),
            sizeof(WA_COMM_adapterConnectionContext_t));
    if(pContext == WA_UTILS_LIST_NO_ELEM)
    {
        WA_ERROR("WA_COMM_Register(): WA_UTILS_LIST_AddBack() error\n");
        goto err_addlist;
    }

    pContext->pConfig = config;
    pContext->cookie = cookie;
    pContext->id = WA_COMM_MSG_ID_PREFIX | id;
    goto unlock;

    err_addlist:
    s1 = WA_UTILS_ID_ReleaseUnsafe(&(commAdapterConnections.idBox), id);
    if(s1 != 0)
    {
        WA_ERROR("WA_COMM_Register(): WA_UTILS_ID_ReleaseUnsafe(): %d\n", s1);
        status = -1;
    }

    err_id:
    unlock:
    s1 = WA_OSA_MutexUnlock(commAdapterConnections.mutex);
    if(s1 != 0)
    {
        WA_ERROR("WA_COMM_Register(): WA_OSA_MutexUnlock(): %d\n", s1);
        status = -1;
    }
    /*
    WA_INFO("WA_COMM_Register() - THIS IS FOR TEST PURPOSE ONLY\n");
    if(handle->callback != NULL)
    {
        handle->callback(handle->cookie, handle->caps);
    }
     */
    err_mutex:
    end:
    WA_RETURN("WA_COMM_Register(): %p\n", pContext);
    return pContext;
}

int WA_COMM_Unregister(void *handle)
{
    int status = -1, s1;
    WA_COMM_adapterConnectionContext_t *pContext = (WA_COMM_adapterConnectionContext_t *)handle;

    WA_ENTER("WA_COMM_Unregister(handle=%p)\n", handle);

    if(commAdapterConnections.mutex == NULL)
    {
        WA_ERROR("WA_COMM_Unregister(): COMM not initialized\n");
        goto end;
    }

    if(handle == NULL)
    {
        WA_ERROR("WA_COMM_Unregister(): Invalid handle\n");
        goto end;
    }

    status = WA_OSA_MutexLock(commAdapterConnections.mutex);
    if(status != 0)
    {
        WA_ERROR("WA_COMM_Unregister(): WA_OSA_MutexLock(): %d\n", status);
        goto err_mutex;
    }

    status = WA_UTILS_ID_ReleaseUnsafe(&(commAdapterConnections.idBox),
            (WA_UTILS_ID_t)(pContext->id & ~WA_COMM_MSG_ID_PREFIX));
    if(status != 0)
    {
        WA_ERROR("WA_COMM_Unregister(): WA_UTILS_ID_ReleaseUnsafe(): %d\n", status);
    }

    WA_UTILS_LIST_AllocRemove(&(commAdapterConnections.adaptersList), handle);

    s1 = WA_OSA_MutexUnlock(commAdapterConnections.mutex);
    if(s1 != 0)
    {
        WA_ERROR("WA_COMM_Unregister(): WA_OSA_MutexUnlock(): %d\n", s1);
        status = -1;
    }

    err_mutex:
    end:
    WA_RETURN("WA_COMM_Unregister(): %d\n", status);
    return status;
}

int WA_COMM_Send(void *handle, json_t *json)
{
    int status = -1;
    WA_COMM_adapterConnectionContext_t *pContext = (WA_COMM_adapterConnectionContext_t *)handle;
    WA_OSA_Qjmsg_t qjmsg;

    WA_ENTER("WA_COMM_Send(handle=%p json=%p)\n", handle, json);

    if(commAdapterConnections.mutex == NULL)
    {
        WA_ERROR("WA_COMM_Send(): COMM not initialized\n");
        goto end;
    }

    status = WA_UTILS_JSON_RpcValidate(&json);
    if(status == -1)
    {
        WA_ERROR("WA_COMM_Send(): WA_UTILS_JSON_RpcValidate() error\n");
        goto end;
    }

    qjmsg.json = json;
    qjmsg.from = pContext->id;
    qjmsg.to = status ? pContext->id : 0;
    status = WA_OSA_QTimedRetrySend(status ? WA_COMM_IncomingQ : WA_INIT_IncomingQ,
            (const char * const)&qjmsg,
            sizeof(qjmsg),
            WA_OSA_Q_PRIORITY_MAX,
            WA_OSA_Q_SEND_TIMEOUT_DEFAULT,
            WA_OSA_Q_SEND_RETRY_DEFAULT);
    if(status != 0)
    {
        WA_ERROR("WA_COMM_Send(): WA_OSA_QSend(): %d\n", status);
    }

    end:
    WA_RETURN("WA_COMM_Send(): %d\n", status);
    return status;
}

int WA_COMM_SendTxt(void *handle, char *msg, size_t len)
{
    int status = -1;
    WA_COMM_adapterConnectionContext_t *pContext = (WA_COMM_adapterConnectionContext_t *)handle;
    WA_OSA_Qjmsg_t qjmsg;

    WA_ENTER("WA_COMM_Send(handle=%p msg=%p[%.*s] len=%zu)\n", handle, msg, msg?(int)len:0, msg?msg:"", len);

    if(commAdapterConnections.mutex == NULL)
    {
        WA_ERROR("WA_COMM_Send(): COMM not initialized\n");
        goto end;
    }

    if(handle == NULL)
    {
        WA_ERROR("WA_COMM_Send(): Invalid handle\n");
        goto end;
    }

    if(msg == NULL)
    {
        WA_ERROR("WA_COMM_Send(): invalid message\n");
        goto end;
    }

    /* general parsing check */
    status = WA_UTILS_JSON_RpcValidateTxt(msg, len, &(qjmsg.json));
    if(status < 0)
    {
        WA_ERROR("WA_COMM_Send(): WA_UTILS_JSON_RpcValidate() error\n");
        goto end;
    }

    qjmsg.from = pContext->id;
    qjmsg.to = status ? pContext->id : 0;
    status = WA_OSA_QTimedRetrySend(status ? WA_COMM_IncomingQ : WA_INIT_IncomingQ,
            (const char * const)&qjmsg,
            sizeof(qjmsg),
            WA_OSA_Q_PRIORITY_MAX,
            WA_OSA_Q_SEND_TIMEOUT_DEFAULT,
            WA_OSA_Q_SEND_RETRY_DEFAULT);
    if(status != 0)
    {
        WA_ERROR("WA_COMM_Send(): WA_OSA_QSend(): %d\n", status);
    }

    end:
    WA_RETURN("WA_COMM_Send(): %d\n", status);
    return status;
}

/*****************************************************************************
 * LOCAL FUNCTIONS
 *****************************************************************************/
static void *CommTask(void *p)
{
    int status;
    WA_OSA_Qjmsg_t qjmsg;
    unsigned int qprio;

    WA_ENTER("CommTask(p=%p)\n", p);

    status = WA_OSA_TaskSetName("WAcomm");
    if(status != 0)
    {
        WA_ERROR("CommTask(): WA_OSA_TaskSetName(): %d\n", status);
        goto end;
    }

    while(1)
    {
        status = WA_OSA_QReceive(WA_COMM_IncomingQ, (char * const)&qjmsg, sizeof(WA_OSA_Qjmsg_t), &qprio);
        if(status == 0)
        {
            WA_INFO("CommTask(): WA_OSA_QReceive(): empty\n");
            break;
        }
        if(status != sizeof(WA_OSA_Qjmsg_t))
        {
            WA_ERROR("CommTask(): WA_OSA_QReceive(): %d\n", status);
            goto end;
        }

#if WA_DEBUG
        {
            char *msg;
            msg = json_dumps(qjmsg.json, JSON_ENCODE_ANY);
            if(!msg)
            {
                WA_ERROR("CommTask(): json_dumps(): error\n");
            }
            else
            {
                WA_INFO("CommTask(): WA_OSA_QReceive(): %s\n", msg);
                free(msg);
            }
        }
#else
        WA_INFO("CommTask(): WA_OSA_QReceive(): %p\n", qjmsg.json);
#endif

        {
            WA_COMM_adapterConnectionContext_t *pContext;
            void *iterator;

            status = WA_OSA_MutexLock(commAdapterConnections.mutex);
            if(status != 0)
            {
                WA_ERROR("CommTask(): WA_OSA_MutexLock(): %d\n", status);
            }
            else
            {
                status = -1;
                for(iterator = WA_UTILS_LIST_FrontIterator(&commAdapterConnections.adaptersList);
                    iterator != WA_UTILS_LIST_NO_ELEM;
                    iterator = WA_UTILS_LIST_NextIterator(&commAdapterConnections.adaptersList, iterator))
                {
                    pContext = (WA_COMM_adapterConnectionContext_t *)(WA_UTILS_LIST_DataAtIterator(&commAdapterConnections.adaptersList, iterator));
                    if((pContext != NULL) && (pContext->id == qjmsg.to))
                    {
                        if(pContext->pConfig->callback != NULL)
                        {
                            status = pContext->pConfig->callback(pContext->cookie, qjmsg.json);
                            if(status != 0)
                            {
                                WA_ERROR("CommTask(): callback(): %d\n", status);
                            }
                        }
                        break;
                    }
                }

                if(status != 0)
                {
                    json_decref(qjmsg.json);//discard
                }

                status = WA_OSA_MutexUnlock(commAdapterConnections.mutex);
                if(status != 0)
                {
                    WA_ERROR("CommTask(): WA_OSA_MutexUnlock(): %d\n", status);
                }
            }
        }
    }
    end:
    WA_RETURN("CommTask(): %p\n", p);
    return p;
}


/* End of doxygen group */
/*! @} */

/* EOF */
