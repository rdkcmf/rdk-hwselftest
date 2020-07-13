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
 * @file wa_diag.c
 *
 * @brief This file contains main function for comm module.
 */

/** @addtogroup WA_DIAG
 *  @{
 */

/*****************************************************************************
 * STANDARD INCLUDE FILES
 *****************************************************************************/
#include <stdbool.h>
#include <string.h>

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/
#include "jansson.h"
#include "wa_diag.h"
#include "wa_id.h"
#include "wa_init.h"
#include "wa_osa.h"
#include "wa_debug.h"
#include "wa_log.h"
#include "wa_agg.h"

/*****************************************************************************
 * GLOBAL VARIABLE DEFINITIONS
 *****************************************************************************/
void *WA_DIAG_IncomingQ = NULL;

/*****************************************************************************
 * LOCAL DEFINITIONS
 *****************************************************************************/
#define WA_DIAG_INCOME_Q_DEEP 10
#define COLLECTOR_Q_DEEP 10

#define WA_DIAG_INCOME_Q_EXIT_MSG "exit"

/*****************************************************************************
 * LOCAL TYPES
 *****************************************************************************/
struct WA_DIAG_procedureContext_tag;

typedef struct
{
    uint32_t id; /**< this procedure instance id given dynamically */
    uint32_t callerId; /**< id of the caller that issued creating this instance (a comm id) */
    void *taskHandle;
    struct WA_DIAG_procedureContext_tag *pContext;
    json_t *json; /**< startup json (a msg received) */
}WA_DIAG_procedureInstance_t;
WA_UTILS_LIST_DECLARE_LIST_TYPE(WA_DIAG_procedureInstance_t);

typedef struct WA_DIAG_procedureContext_tag
{
    const WA_DIAG_proceduresConfig_t *pConfig;
    void *handle;
    WA_UTILS_LIST(WA_DIAG_procedureInstance_t) instancesList;
}WA_DIAG_procedureContext_t;
WA_UTILS_LIST_DECLARE_LIST_TYPE(WA_DIAG_procedureContext_t);

typedef struct
{
    void *mutex;
    void *semCollector;
    WA_UTILS_ID_box_t idBox;
    WA_UTILS_LIST(WA_DIAG_procedureContext_t) proceduresList;
}WA_DIAG_procedures_t;


/**
 * @note Function is responsible for returning a json content
 *       that will be send back (or json_decref(*json), *json=NULL).
 */
typedef int (*WA_DIAG_LocalProcedure_t)(json_t **json);

typedef struct
{
    const char *name;
    WA_DIAG_LocalProcedure_t fnc;
}WA_DIAG_localProcedures_t;

/*****************************************************************************
 * LOCAL FUNCTION PROTOTYPES
 *****************************************************************************/
static void *DiagTask(void *p);
static WA_DIAG_procedureContext_t *FindContextByName(const char *name);
static int CreateProcedureInstance(char *name, json_t *json, uint32_t callerId);
static int DestroyProcedureInstance(WA_DIAG_procedureInstance_t *pInstance);
static int CleanupInstance(WA_DIAG_procedureInstance_t *pInstance);
static void *InstanceTask(void *p);
static void *InstancesCollectorTask(void *p);
static WA_DIAG_procedureInstance_t *FindInstanceById(uint32_t id);
static int DiagControl(json_t **json);
static int TestRunControl(json_t **json);

/*****************************************************************************
 * LOCAL VARIABLE DECLARATIONS
 *****************************************************************************/
static void *diagTaskHandle;

static void *collectorTaskHandle;
static void *collectorQ = NULL;

static const WA_DIAG_proceduresConfig_t *pProceduresConfig;

static WA_DIAG_procedures_t diagProcedures = {mutex:NULL};

static const WA_DIAG_localProcedures_t localProcedures[] =
{
        {"LOG", WA_LOG_Log},
        {"DIAG", DiagControl},
        {"TESTRUN", TestRunControl}
};

/*****************************************************************************
 * FUNCTION DEFINITIONS
 *****************************************************************************/

int WA_DIAG_Init(const WA_DIAG_proceduresConfig_t *diags)
{
    int status = -1, s1;
    WA_DIAG_proceduresConfig_t *pConfig;
    void *iterator;
    void *procedureHandle;
    WA_DIAG_procedureContext_t *pContext = NULL;
    //    WA_UTILS_ID_t id; //This is for generating broadcast id

    WA_ENTER("WA_DIAG_Init(diags=%p)\n", diags);

    if(diagProcedures.mutex != NULL)
    {
        WA_ERROR("WA_DIAG_Init(): COMM already initialized\n");
        status = 0;
        goto end;
    }

    pProceduresConfig = diags;
    WA_UTILS_LIST_ClearList(diagProcedures.proceduresList);
    /*
    status = WA_UTILS_ID_GenerateUnsafe(&(diagProcedures.idBox), &id);
    if(status != 0)
    {
        WA_ERROR("CreateProcedureInstance(): WA_UTILS_ID_GenerateUnsafe(): %d\n", status);
        goto err_id;
    }
     */
    diagProcedures.mutex = WA_OSA_MutexCreate();
    if(diagProcedures.mutex == NULL)
    {
        WA_ERROR("WA_DIAG_Init(): WA_OSA_MutexCreate(): error\n");
        goto err_mutex;
    }

    collectorQ = WA_OSA_QCreate(COLLECTOR_Q_DEEP, sizeof(WA_DIAG_procedureInstance_t *));
    if(collectorQ == NULL)
    {
        WA_ERROR("WA_DIAG_Init(): WA_OSA_QCreate(collectorQ) error\n");
        goto err_collector_q;
    }

    collectorTaskHandle = WA_OSA_TaskCreate(NULL, 0, InstancesCollectorTask, NULL, WA_OSA_SCHED_POLICY_RT, WA_OSA_TASK_PRIORITY_MAX);
    if(collectorTaskHandle == NULL)
    {
        WA_ERROR("WA_DIAG_Init(): WA_OSA_TaskCreate(InstancesCollectorTask): error\n");
        goto err_collector_task;
    }


    WA_DIAG_IncomingQ = WA_OSA_QCreate(WA_DIAG_INCOME_Q_DEEP, sizeof(WA_OSA_Qjmsg_t));
    if(WA_DIAG_IncomingQ == NULL)
    {
        WA_ERROR("WA_DIAG_Init(): WA_OSA_QCreate(WA_DIAG_IncomingQ) error\n");
        goto income_q_err;
    }

    diagTaskHandle = WA_OSA_TaskCreate(NULL, 0, DiagTask, NULL, WA_OSA_SCHED_POLICY_RT, WA_OSA_TASK_PRIORITY_MAX);
    if(diagTaskHandle == NULL)
    {
        WA_ERROR("WA_DIAG_Init(): WA_OSA_TaskCreate(DiagTask): error\n");
        goto diag_task_err;
    }

    for(pConfig = (WA_DIAG_proceduresConfig_t *)pProceduresConfig;
            pConfig && pConfig->fnc;
            ++pConfig)
    {

        if((pConfig->name == NULL) || (strlen(pConfig->name) == 0) ||
                (pConfig->fnc == NULL))
        {
            WA_ERROR("WA_DIAG_Init(): invalid config params\n");
            goto err_adapters;
        }

        pContext = FindContextByName(pConfig->name);
        if(pContext != NULL)
        {
            WA_ERROR("WA_DIAG_Init(): name \"%s\" already registered\n", pConfig->name);
            goto err_adapters;
        }

        procedureHandle = (void *)pConfig;
        if(pConfig->initFnc != NULL)
        {
            procedureHandle = pConfig->initFnc(pConfig);
            if(procedureHandle == NULL)
            {
                WA_ERROR("WA_DIAG_Init(): initFnc(): error\n");
                goto err_adapters;
            }
        }

        pContext = (WA_DIAG_procedureContext_t *)WA_UTILS_LIST_AllocAddBack(&(diagProcedures.proceduresList),
                sizeof(WA_DIAG_procedureContext_t));
        if(pContext == NULL)
        {
            WA_ERROR("WA_DIAG_Init(): WA_UTILS_LIST_AllocAddBack() error\n");
            if(pConfig->exitFnc != NULL)
            {
                s1 = pConfig->exitFnc(procedureHandle);
                if(s1 != 0)
                {
                    WA_ERROR("WA_DIAG_Init(): exitFnc(): %d\n", s1);
                }
                goto err_adapters;
            }
        }

        pContext->handle = procedureHandle;
        pContext->pConfig = pConfig;
        WA_UTILS_LIST_ClearList(pContext->instancesList);
    }
    status = 0;
    goto end;

    err_adapters:
    for(iterator = WA_UTILS_LIST_FrontIterator(&(diagProcedures.proceduresList));
            iterator != WA_UTILS_LIST_NO_ELEM;
            iterator = WA_UTILS_LIST_NextIterator(&(diagProcedures.proceduresList), iterator))
    {
        pContext = (WA_DIAG_procedureContext_t *)WA_UTILS_LIST_DataAtIterator(&(diagProcedures.proceduresList), iterator);
        if(pContext->pConfig->exitFnc != NULL)
        {
            s1 = pContext->pConfig->exitFnc(pContext->handle);
            if(s1 != 0)
            {
                WA_ERROR("WA_DIAG_Init(): exitFnc(): %d\n", s1);
            }
        }
    }
    WA_UTILS_LIST_AllocPurge(&(diagProcedures.proceduresList));

    s1 = WA_OSA_QSend(WA_DIAG_IncomingQ,
            NULL,
            0,
            WA_OSA_Q_PRIORITY_MAX);
    if(s1 != 0)
    {
        WA_ERROR("WA_DIAG_Init(): WA_OSA_QSend(): %d\n", s1);
        status = -1;
    }
    s1 = WA_OSA_TaskSignalQuit(diagTaskHandle);
    if(s1 != 0)
    {
        WA_ERROR("WA_DIAG_Init(): WA_OSA_TaskSignal(DiagTask): error\n");
    }

    s1 = WA_OSA_TaskJoin(diagTaskHandle, NULL);
    if(s1 != 0)
    {
        WA_ERROR("WA_DIAG_Init(): WA_OSA_TaskJoin(DiagTask): error\n");
    }

    s1 = WA_OSA_TaskDestroy(diagTaskHandle);
    if(s1 != 0)
    {
        WA_ERROR("WA_DIAG_Init(): WA_OSA_TaskDestroy(DiagTask): error\n");
    }

    diag_task_err:
    s1 = WA_OSA_QDestroy(WA_DIAG_IncomingQ);
    if(s1 != 0)
    {
        WA_ERROR("WA_DIAG_Init():  WA_OSA_QDestroy(WA_DIAG_IncomingQ): %d\n", s1);
    }

    income_q_err:
    s1 = WA_OSA_QSend(collectorQ, NULL, 0, WA_OSA_Q_PRIORITY_MAX);
    if(s1 != 0)
    {
        WA_ERROR("WA_DIAG_Init(): WA_OSA_QSend(): %d\n", s1);
    }

    s1 = WA_OSA_TaskJoin(collectorTaskHandle, NULL);
    if(s1 != 0)
    {
        WA_ERROR("WA_DIAG_Init(): WA_OSA_TaskJoin(InstancesCollectorTask): error\n");
    }

    s1 = WA_OSA_TaskDestroy(collectorTaskHandle);
    if(s1 != 0)
    {
        WA_ERROR("WA_DIAG_Init(): WA_OSA_TaskDestroy(InstancesCollectorTask): error\n");
    }

    err_collector_task:
    s1 = WA_OSA_QDestroy(collectorQ);
    if(s1 != 0)
    {
        WA_ERROR("WA_DIAG_Init():  WA_OSA_QDestroy(collectorQ): %d\n", s1);
    }
    err_collector_q:
    s1 = WA_OSA_MutexDestroy(diagProcedures.mutex);
    if(s1 != 0)
    {
        WA_ERROR("WA_DIAG_Init(): WA_OSA_MutexDestroy(): %d\n", s1);
    }
    err_mutex:
    end:
    WA_RETURN("WA_DIAG_Init(): %d\n", status);
    return status;
}

int WA_DIAG_Exit(void)
{
    int status = 0, s1;
    WA_DIAG_procedureContext_t *pContext;
    WA_DIAG_procedureInstance_t *pInstance;
    void *iterator;
    void *instanceIterator;

    WA_ENTER("WA_DIAG_Exit()\n");

    if(diagProcedures.mutex == NULL)
    {
        WA_ERROR("WA_DIAG_Exit(): COMM not initialized\n");
        status = -1;
        goto end;
    }

    s1 = WA_OSA_QSend(collectorQ, NULL, 0, WA_OSA_Q_PRIORITY_MAX);
    if(s1 != 0)
    {
        WA_ERROR("WA_DIAG_Exit(): WA_OSA_QSend(): %d\n", s1);
        status = -1;
    }

    s1 = WA_OSA_TaskJoin(collectorTaskHandle, NULL);
    if(s1 != 0)
    {
        WA_ERROR("WA_DIAG_Exit(): WA_OSA_TaskJoin(InstancesCollectorTask): error\n");
        status = -1;
    }

    s1 = WA_OSA_TaskDestroy(collectorTaskHandle);
    if(s1 != 0)
    {
        WA_ERROR("WA_DIAG_Exit(): WA_OSA_TaskDestroy(InstancesCollectorTask): error\n");
        status = -1;
    }

    for(iterator = WA_UTILS_LIST_FrontIterator(&(diagProcedures.proceduresList));
            iterator != WA_UTILS_LIST_NO_ELEM;
            iterator = WA_UTILS_LIST_NextIterator(&(diagProcedures.proceduresList), iterator))
    {
        pContext = (WA_DIAG_procedureContext_t *)WA_UTILS_LIST_DataAtIterator(&(diagProcedures.proceduresList), iterator);

        for(instanceIterator = WA_UTILS_LIST_FrontIterator(&(pContext->instancesList));
                instanceIterator != WA_UTILS_LIST_NO_ELEM;
                instanceIterator = WA_UTILS_LIST_NextIterator(&(pContext->instancesList), instanceIterator))
        {
            pInstance = (WA_DIAG_procedureInstance_t *)WA_UTILS_LIST_DataAtIterator(&(pContext->instancesList), instanceIterator);
            s1 = DestroyProcedureInstance(pInstance);
            if(s1 != 0)
            {
                WA_ERROR("WA_DIAG_Exit(): COMM not initialized\n");
                status = -1;
            }

        }

        if(pContext->pConfig->exitFnc != NULL)
        {
            s1 = pContext->pConfig->exitFnc(pContext->handle);
            if(s1 != 0)
            {
                WA_ERROR("WA_DIAG_Exit(): exitFnc(): %d\n", s1);
                status = -1;
            }
        }
    }

    WA_UTILS_LIST_AllocPurge(&(diagProcedures.proceduresList));

    s1 = WA_OSA_QSend(WA_DIAG_IncomingQ,
            NULL,
            0,
            WA_OSA_Q_PRIORITY_MAX);
    if(s1 != 0)
    {
        WA_ERROR("WA_DIAG_Init(): WA_OSA_QSend(): %d\n", s1);
        status = -1;
    }
    s1 = WA_OSA_TaskSignalQuit(diagTaskHandle);
    if(s1 != 0)
    {
        WA_ERROR("WA_DIAG_Exit(): WA_OSA_TaskSignal(DiagTask): error\n");
        status = -1;
    }

    s1 = WA_OSA_TaskJoin(diagTaskHandle, NULL);
    if(s1 != 0)
    {
        WA_ERROR("WA_DIAG_Exit(): WA_OSA_TaskJoin(DiagTask): error\n");
        status = -1;
    }

    s1 = WA_OSA_TaskDestroy(diagTaskHandle);
    if(s1 != 0)
    {
        WA_ERROR("WA_DIAG_Exit(): WA_OSA_TaskDestroy(DiagTask): error\n");
        status = -1;
    }

    s1 = WA_OSA_QDestroy(WA_DIAG_IncomingQ);
    if(s1 != 0)
    {
        WA_ERROR("WA_DIAG_Exit():  WA_OSA_QDestroy(WA_DIAG_IncomingQ): %d\n", status);
        status = -1;
    }

    s1 = WA_OSA_QDestroy(collectorQ);
    if(s1 != 0)
    {
        WA_ERROR("WA_DIAG_Init():  WA_OSA_QDestroy(collectorQ): %d\n", s1);
        status = -1;
    }

    s1 = WA_OSA_MutexDestroy(diagProcedures.mutex);
    if(s1 != 0)
    {
        WA_ERROR("WA_DIAG_Exit(): WA_OSA_MutexDestroy(): %d\n", s1);
        status = -1;
    }
    end:
    WA_RETURN("WA_DIAG_Exit(): %d\n", status);
    return status;
}


int WA_DIAG_Send(void *instanceHandle, json_t *json, void *responseCookie)
{
    int status = -1;
    WA_DIAG_procedureInstance_t *pInstance= (WA_DIAG_procedureInstance_t *)instanceHandle;
    WA_OSA_Qjmsg_t qjmsg;
    char tmp[10];

    WA_ENTER("WA_DIAG_Send(instanceHandle=%p json=%p responseCookie=%p)\n",
            instanceHandle, json, responseCookie);

    if(instanceHandle == NULL)
    {
        WA_ERROR("WA_DIAG_Send(): Invalid handle\n");
        goto end;
    }

    if(json == NULL)
    {
        WA_ERROR("WA_DIAG_Send(): invalid message\n");
        goto end;
    }
#if WA_DEBUG
    {
        char *msg;
        msg = json_dumps(json, JSON_ENCODE_ANY);
        if(!msg)
        {
            WA_ERROR("WA_DIAG_Send(): json_dumps(): error\n");
        }
        else
        {
            WA_INFO("WA_DIAG_Send(): json_dumps(): %s\n", msg);
            free(msg);
        }
    }
#else
    WA_INFO("WA_DIAG_Send(): WA_OSA_QReceive(): %p\n", qjmsg.json);
#endif

    qjmsg.from = pInstance->id;
    qjmsg.to = pInstance->callerId;
    snprintf(tmp, sizeof(tmp), "#%08x", pInstance->id);
    qjmsg.json = json_pack("{s:s,s:s,s:o,s:n}", "jsonrpc", "2.0", "method", tmp, "params", json, "id");

#if WA_DEBUG
    {
        char *msg;
        msg = json_dumps(qjmsg.json, JSON_ENCODE_ANY);
        if(!msg)
        {
            WA_ERROR("WA_DIAG_Send(): json_dumps(): error\n");
        }
        else
        {
            WA_INFO("WA_DIAG_Send(): json_dumps(): %s\n", msg);
            free(msg);
        }
    }
#else
    WA_INFO("WA_DIAG_Send(): WA_OSA_QReceive(): %p\n", qjmsg.json);
#endif
    if(!qjmsg.json)
    {
        WA_ERROR("WA_DIAG_Send(): json_pack() error\n");
        goto end;
    }
    status = WA_OSA_QTimedRetrySend(WA_INIT_IncomingQ,
            (const char * const)&qjmsg,
            sizeof(qjmsg),
            WA_OSA_Q_PRIORITY_MAX,
            WA_OSA_Q_SEND_TIMEOUT_DEFAULT,
            WA_OSA_Q_SEND_RETRY_DEFAULT);
    if(status != 0)
    {
        WA_ERROR("WA_DIAG_Send(): WA_OSA_QSend(): %d\n", status);
    }

    end:
    WA_RETURN("WA_DIAG_Send(): %d\n", status);
    return status;
}

int WA_DIAG_SendStr(void *instanceHandle, char *msg, void *responseCookie)
{
    int status = -1;
    json_t *jout;

    WA_ENTER("WA_DIAG_SendStr(instanceHandle=%p msg=%p[%s] , responseCookie=%p)\n",
            instanceHandle, msg, msg?msg:"", responseCookie);

    jout = json_string(msg);
    if(!jout)
    {
        WA_ERROR("WA_DIAG_SendStr(): json_string(): error\n");
    }
    else
    {
        status = WA_DIAG_Send(instanceHandle, jout, responseCookie);
        if(status != 0)
        {
            WA_ERROR("WA_DIAG_SendStr(): WA_DIAG_Send(): error\n");
            json_decref(jout);
        }
    }

    WA_RETURN("WA_DIAG_SendStr(): %d\n", status);
    return status;
}

void WA_DIAG_SendProgress(void* instanceHandle, int progress)
{
    json_t *jout;
    int status;

    WA_ENTER("WA_DIAG_SendProgress(instanceHandle=%p, progress=%d)\n",
            instanceHandle, progress);

    jout = json_pack("{s:i}", "progress", progress);
    if(jout == NULL)
    {
        WA_ERROR("WA_DIAG_SendProgress(): json_pack(): error\n");
    }
    else
    {
        status = WA_DIAG_Send(instanceHandle, jout, NULL);
        if(status != 0)
        {
            json_decref(jout);
            WA_ERROR("WA_DIAG_SendProgress(): WA_OSA_QSend(): %d\n", status);
        }
    }
}

void WA_DIAG_SetWriteTestResult(bool writeResult)
{
    WA_AGG_SetWriteTestResult(writeResult);
}

/*****************************************************************************
 * LOCAL FUNCTIONS
 *****************************************************************************/
static void *DiagTask(void *p)
{
    int status, i;
    WA_OSA_Qjmsg_t qjmsg, qjmsgOut;
    unsigned int qprio;
    json_t *jId;
    char *method;
    WA_DIAG_procedureContext_t *pContext;

    WA_ENTER("DiagTask(p=%p)\n", p);

    status = WA_OSA_TaskSetName("WAdiag");
    if(status != 0)
    {
        WA_ERROR("DiagTask(): WA_OSA_TaskSetName(): %d\n", status);
        goto end;
    }

    while(1)
    {
        next:
        status = WA_OSA_QReceive(WA_DIAG_IncomingQ, (char * const)&qjmsg, sizeof(WA_OSA_Qjmsg_t), &qprio);
        if(status == 0)
        {
            WA_INFO("DiagTask(): WA_OSA_QReceive(): empty\n");
            break;
        }
        if(status != sizeof(WA_OSA_Qjmsg_t))
        {
            WA_ERROR("DiagTask(): WA_OSA_QReceive(): %d\n", status);
            goto end;
        }

#if WA_DEBUG
        {
            char *msg;
            msg = json_dumps(qjmsg.json, 0);
            if(!msg)
            {
                WA_ERROR("DiagTask(): json_dumps(): error\n");
            }
            else
            {
                WA_INFO("DiagTask(): WA_OSA_QReceive(): %s\n", msg);
                free(msg);
            }
        }
#else
        WA_INFO("DiagTask(): WA_OSA_QReceive(): %p\n", qjmsg.json);
#endif

        status = json_unpack(qjmsg.json, "{s:s}", "method", &method);
        if(status != 0)
        {
            WA_ERROR("DiagTask(): json_unpack() \"method\" missing\n");
            json_decref(qjmsg.json);
            continue;
        }

        status = json_unpack(qjmsg.json, "{s:o}", "id", &jId); //reference to jId is NOT modified
        if(status != 0)
        {
            WA_ERROR("DiagTask(): json_unpack() \"id\" missing\n");
            json_decref(qjmsg.json);
            continue;
        }

        if(json_typeof(jId) == JSON_NULL)
        {
            /* First use local handlers */
            for(i = 0; i < sizeof(localProcedures)/sizeof(WA_DIAG_localProcedures_t); ++i)
            {
                if(!strcmp(localProcedures[i].name, method))
                {
                    /* qjmsg.json is released inside the function called. */
                    localProcedures[i].fnc(&(qjmsg.json));
                    json_decref(qjmsg.json);
                    goto next;
                }
            }

            //if(method[0] != '#')
            {
                WA_ERROR("DiagTask(): id null for procedure call\n");
                /* according to the spec no response is allowed */
                json_decref(qjmsg.json);
                continue;
            }
            /* Consideration: Currently there is no need to send any information from
             *                client to a diag instance.
             *                If needed it should be handled here.
             */
        }

        /* Consideration: Currently the local handlers (grouped in localProcedures) are
         *                executed in response to notifications from client (so no reponse
         *                is returned).
         *                If there is a need to run them as methods (e.g to get a status of
         *                their execution) is should be handled here.
         */

        /* Consideration: Currently accept any message that it is known as coming from a clinet
         *                directed to a diag (as no more players are in the system).
         *                If there is a new player added (that can send/receive messages in system)
         *                or any filtering is needed it must be implemented here to make use of
         *                an ID that is carried by the messages.
         */
        /* Consideration: Currently there is no communication to particular instance (to its
         *                id in form as "#<instance>"). If this is needed it must be handled here.
         */

        WA_INFO("DiagTask(): method: %s\n", method);

        status = -1;
        qjmsgOut.json = NULL;

        //if(msg.h.to == WA_DIAG_MSG_ID_PREFIX)
        pContext = FindContextByName(method);
        if(pContext)
        {
            if(WA_UTILS_LIST_ElemCount(&(pContext->instancesList)) == 0)
            {
                status = CreateProcedureInstance(method, qjmsg.json, qjmsg.from);
                if(status != 0)
                {
                    WA_ERROR("DiagTask(): \"%s\" run error\n", method);
                    qjmsgOut.json = json_pack("{s:s,s:{s:n,s:s},s:O}", "jsonrpc", "2.0", "result", "diag", "message", "Cannot create instance", "id", jId);
                }
            }
            else
            {
                WA_INFO("DiagTask(): \"%s\" already in progress\n", method);
                qjmsgOut.json = json_pack("{s:s,s:{s:n,s:s},s:O}", "jsonrpc", "2.0", "result", "diag", "message", "Already in progress", "id", jId);
            }
        }
        else
        {
            WA_INFO("DiagTask(): \"%s\" diag unknown\n", method);
            qjmsgOut.json = json_pack("{s:s,s:{s:n,s:s},s:O}", "jsonrpc", "2.0", "result", "diag", "message", "Unknown method", "id", jId);
        }

        if(status != 0)
        {
            json_decref(qjmsg.json);

            if(qjmsgOut.json == NULL)
            {
                WA_ERROR("DiagTask(): json_object() error\n");
            }
            else
            {
                qjmsgOut.from = WA_DIAG_MSG_ID_PREFIX;
                qjmsgOut.to = qjmsg.from;
                status = WA_OSA_QTimedRetrySend(WA_INIT_IncomingQ,
                        (const char * const)&qjmsgOut,
                        sizeof(qjmsgOut),
                        WA_OSA_Q_PRIORITY_MAX,
                        WA_OSA_Q_SEND_TIMEOUT_DEFAULT,
                        WA_OSA_Q_SEND_RETRY_DEFAULT);
                if(status != 0)
                {
                    WA_ERROR("DiagTask(): WA_OSA_QSend(): %d\n", status);
                }
            }
        }

    }
    end:
    WA_RETURN("DiagTask(): %p\n", p);
    return p;
}

static void *InstancesCollectorTask(void *p)
{
    int status;
    unsigned int qprio;
    WA_DIAG_procedureInstance_t *pInstance;

    WA_ENTER("InstancesCollectorTask(p=%p)\n", p);

    status = WA_OSA_TaskSetName("WAcollector");
    if(status != 0)
    {
        WA_ERROR("InstancesCollectorTask(): WA_OSA_TaskSetName(): %d\n", status);
        goto end;
    }

    while(1)
    {
        status = WA_OSA_QReceive(collectorQ, (char * const)&pInstance, sizeof(pInstance), &qprio);
        if(status < 0)
        {
            WA_ERROR("InstancesCollectorTask(): WA_OSA_QReceive(): %d\n", status);
            goto end;
        }
        if(status == 0)
            break;

        WA_INFO("InstancesCollectorTask(): WA_OSA_QReceive(): %p\n", pInstance);

        status = WA_OSA_TaskJoin(pInstance->taskHandle, NULL);
        if(status != 0)
        {
            WA_ERROR("InstancesCollectorTask(): WA_OSA_TaskJoin(InstanceTask): error\n");
            status = -1;
        }

        status = WA_OSA_TaskDestroy(pInstance->taskHandle);
        if(status != 0)
        {
            WA_ERROR("InstancesCollectorTask(): WA_OSA_TaskDestroy(InstanceTask): error\n");
            status = -1;
        }

        status = CleanupInstance(pInstance);
        if(status != 0)
        {
            WA_ERROR("InstanceTask(): CleanupInstance(): %d\n", status);
        }
    }
    end:
    WA_RETURN("InstancesCollectorTask(): %p\n", p);
    return p;
}

static WA_DIAG_procedureContext_t *FindContextByName(const char *name)
{
    WA_DIAG_procedureContext_t *pContext = NULL;
    void *iterator;

    WA_ENTER("FindContextByName(name=%s)\n", name);

    for(iterator = WA_UTILS_LIST_FrontIterator(&(diagProcedures.proceduresList));
            iterator != WA_UTILS_LIST_NO_ELEM;
            iterator = WA_UTILS_LIST_NextIterator(&(diagProcedures.proceduresList), iterator))
    {
        pContext = (WA_DIAG_procedureContext_t *)(WA_UTILS_LIST_DataAtIterator(&(diagProcedures.proceduresList), iterator));
        if(strcmp(name, pContext->pConfig->name) == 0)
        {
            goto end;
        }
    }
    pContext = NULL;
    end:
    WA_ENTER("FindContextByName(): %p\n", pContext);
    return pContext;
}

static int CreateProcedureInstance(char *name, json_t *json, uint32_t callerId)
{
    int status = -1, s1;
    WA_DIAG_procedureContext_t *pContext = NULL;
    WA_DIAG_procedureInstance_t *pInstance;
    WA_UTILS_ID_t id;

    WA_ENTER("CreateProcedureInstance(name=%p[%s] json=%p callerId=0x%08x)\n",
            name, name?name:"", json, callerId);

    if(diagProcedures.mutex == NULL)
    {
        WA_ERROR("CreateProcedureInstance(): COMM not initialized\n");
        goto end;
    }

    if((name == NULL) || (strlen(name) == 0))
    {
        WA_ERROR("CreateProcedureInstance(): invalid parameter\n");
        goto end;
    }

    status = WA_OSA_MutexLock(diagProcedures.mutex);
    if(status != 0)
    {
        WA_ERROR("CreateProcedureInstance(): WA_OSA_MutexLock(): %d\n", status);
        goto err_mutex;
    }

    pContext = FindContextByName(name);
    if(pContext == NULL)
    {
        WA_ERROR("CreateProcedureInstance(): name \"%s\" unknown\n", name);
        pContext = NULL;
        goto err_notexists;
    }

    /* Consideration: Currently there is no limit of a number of created instances.
     *                This might be handled here and make use of the diag caps
     *                to define the limit number.
     */
    status = WA_UTILS_ID_GenerateUnsafe(&(diagProcedures.idBox), &id);
    if(status != 0)
    {
        WA_ERROR("CreateProcedureInstance(): WA_UTILS_ID_GenerateUnsafe(): %d\n", status);
        goto err_id;
    }

    pInstance = (WA_DIAG_procedureInstance_t *)WA_UTILS_LIST_AllocAddBack(&(pContext->instancesList),
            sizeof(WA_DIAG_procedureInstance_t));
    if(pInstance == WA_UTILS_LIST_NO_ELEM)
    {
        WA_ERROR("CreateProcedureInstance(): WA_UTILS_LIST_AllocAddBack() error\n");
        goto err_addlist;
    }

    /* Consideration: Currently there is no limit of a number of created tasks.
     *                This might be handled here and make use of the diag caps
     *                to define the limit number.
     */
    pInstance->pContext = pContext;
    pInstance->json = json;
    pInstance->callerId = callerId;
    pInstance->id = WA_DIAG_MSG_ID_PREFIX | id;
    pInstance->taskHandle = WA_OSA_TaskCreate(NULL, 0, InstanceTask, pInstance, WA_OSA_SCHED_POLICY_RT, WA_OSA_TASK_PRIORITY_MAX);
    if(pInstance->taskHandle == NULL)
    {
        WA_ERROR("CreateProcedureInstance(): WA_OSA_TaskCreate(InstanceTask): error\n");
        goto err_task;
    }

    goto unlock;

    err_task:
    WA_UTILS_LIST_AllocRemove(&(pContext->instancesList), pInstance);
    pInstance = NULL;

    err_addlist:
    s1 = WA_UTILS_ID_ReleaseUnsafe(&(diagProcedures.idBox), id);
    if(s1 != 0)
    {
        WA_ERROR("CreateProcedureInstance(): WA_UTILS_ID_ReleaseUnsafe(): %d\n", s1);
    }
    err_id:
    err_notexists:
    unlock:
    s1 = WA_OSA_MutexUnlock(diagProcedures.mutex);
    if(s1 != 0)
    {
        WA_ERROR("CreateProcedureInstance(): WA_OSA_MutexUnlock(): %d\n", s1);
    }
    err_mutex:
    end:
    WA_ENTER("CreateProcedureInstance(): %d\n", status);
    return status;
}

static int DestroyProcedureInstance(WA_DIAG_procedureInstance_t *pInstance)
{
    int status = -1, s1;

    WA_ENTER("DestroyProcedureInstance(pInstance=%p\n", pInstance);

    if(diagProcedures.mutex == NULL)
    {
        WA_ERROR("DestroyProcedureInstance(): COMM not initialized\n");
        goto end;
    }

    if((pInstance == NULL) || (pInstance->pContext == NULL))
    {
        WA_ERROR("DestroyProcedureInstance(): invalid parameter\n");
        goto end;
    }

    s1 = WA_OSA_TaskSignalQuit(pInstance->taskHandle);
    if(s1 != 0)
    {
        WA_ERROR("DestroyProcedureInstance(): WA_OSA_TaskSignal(InstanceTask): error\n");
        status = -1;
    }

    s1 = WA_OSA_TaskJoin(pInstance->taskHandle, NULL);
    if(s1 != 0)
    {
        WA_ERROR("DestroyProcedureInstance(): WA_OSA_TaskJoin(InstanceTask): error\n");
        status = -1;
    }

    s1 = WA_OSA_TaskDestroy(pInstance->taskHandle);
    if(s1 != 0)
    {
        WA_ERROR("DestroyProcedureInstance(): WA_OSA_TaskDestroy(InstanceTask): error\n");
        status = -1;
    }

    status = CleanupInstance(pInstance);
    if(status != 0)
    {
        WA_ERROR("DestroyProcedureInstance(): CleanupInstance(): %d\n", status);
    }
    end:
    WA_ENTER("CreateProcedureInstance(): %d\n", status);
    return status;
}

static int CleanupInstance(WA_DIAG_procedureInstance_t *pInstance)
{
    int status = -1, s1;
    WA_DIAG_procedureContext_t *pContext = NULL;

    WA_ENTER("CleanupInstance(pInstance=%p\n", pInstance);

    if(diagProcedures.mutex == NULL)
    {
        WA_ERROR("CleanupInstance(): COMM not initialized\n");
        goto end;
    }

    if((pInstance == NULL) || (pInstance->pContext == NULL))
    {
        WA_ERROR("CleanupInstance(): invalid parameter\n");
        goto end;
    }

    status = WA_OSA_MutexLock(diagProcedures.mutex);
    if(status != 0)
    {
        WA_ERROR("CleanupInstance(): WA_OSA_MutexLock(): %d\n", status);
        goto end;
    }

    s1 = WA_UTILS_ID_ReleaseUnsafe(&(diagProcedures.idBox), pInstance->id & ~WA_DIAG_MSG_ID_PREFIX);
    if(s1 != 0)
    {
        WA_ERROR("CleanupInstance(): WA_UTILS_ID_ReleaseUnsafe(): %d\n", s1);
        status = -1;
    }
    json_decref(pInstance->json);

    pContext = (WA_DIAG_procedureContext_t *)pInstance->pContext;

    WA_UTILS_LIST_AllocRemove(&(pContext->instancesList), pInstance);

    s1 = WA_OSA_MutexUnlock(diagProcedures.mutex);
    if(s1 != 0)
    {
        WA_ERROR("CleanupInstance(): WA_OSA_MutexUnlock(): %d\n", s1);
        status = -1;
    }
    end:
    WA_ENTER("CleanupInstance(): %d\n", status);
    return status;
}

static void *InstanceTask(void *p)
{
    int status;
    char tmp[WA_OSA_TASK_NAME_MAX_LEN];
    WA_DIAG_procedureInstance_t *pInstance = (WA_DIAG_procedureInstance_t *)p;
    WA_DIAG_procedureContext_t *pContext;
    WA_OSA_Qjmsg_t qjmsg;
    json_t *jp, *jparams = NULL, *jId;
    time_t timestamp;
    char strtimestamp[32];

    WA_ENTER("InstanceTask(p=%p)\n", p);

    snprintf(tmp, WA_OSA_TASK_NAME_MAX_LEN, "WAdiag#%d", pInstance->id);
    status = WA_OSA_TaskSetName(tmp);
    if(status != 0)
    {
        WA_ERROR("InstanceTask(): WA_OSA_TaskSetName(): %d\n", status);
        goto end;
    }

    pContext = (WA_DIAG_procedureContext_t *)pInstance->pContext;

    /* "id" */
    status = json_unpack(pInstance->json, "{s:O}", "id", &jId); //reference to jId is NOT modified
    if(status != 0)
    {
        WA_ERROR("InstanceTask(): json_unpack() \"id\" missing\n");
        goto end;
    }

    qjmsg.from = pInstance->id;
    qjmsg.to = pInstance->callerId;

    /* "params" */
    status = json_unpack(pInstance->json, "{s:o}", "params", &jp); //reference to jParams is NOT modified
    if(status != 0)
    {
        WA_INFO("InstanceTask(): json_unpack() params undefined\n");
    }
    else
    {
        jparams = json_deep_copy(jp);
        if(!jparams)
        {
            WA_ERROR("InstanceTask(): json_deep_copy() error\n");
            qjmsg.json = json_pack("{s:s,s:{s:i,s:s},s:o}", "jsonrpc", "2.0", "error", "code", -32603, "message", "Internal error", "id", jId);
            status = WA_OSA_QTimedRetrySend(WA_INIT_IncomingQ,
                    (const char * const)&qjmsg,
                    sizeof(qjmsg),
                    WA_OSA_Q_PRIORITY_MAX,
                    WA_OSA_Q_SEND_TIMEOUT_DEFAULT,
                    WA_OSA_Q_SEND_RETRY_DEFAULT
            );
            if(status != 0)
            {
                WA_ERROR("InstanceTask(): WA_OSA_QSend(): %d\n", status);
            }
            goto end;
        }
    }

    snprintf(tmp, WA_OSA_TASK_NAME_MAX_LEN, "#%08x", pInstance->id);
    qjmsg.json = json_pack("{s:s,s:{s:s},s:o}", "jsonrpc", "2.0", "result", "diag", tmp, "id", jId);
    status = WA_OSA_QTimedRetrySend(WA_INIT_IncomingQ,
            (const char * const)&qjmsg,
            sizeof(qjmsg),
            WA_OSA_Q_PRIORITY_MAX,
            WA_OSA_Q_SEND_TIMEOUT_DEFAULT,
            WA_OSA_Q_SEND_RETRY_DEFAULT);
    if(status != 0)
    {
        WA_ERROR("InstanceTask(): WA_OSA_QSend(): %d\n", status);
    }

    status = pContext->pConfig->fnc(pInstance, pContext->handle, &jparams);
    if(status != 0)
    {
        WA_DBG("InstanceTask(): fnc(): %d\n", status);
    }

    timestamp = time(0);

    if (WA_AGG_SetTestResult(pContext->pConfig->name, status, timestamp))
        WA_WARN("InstanceTask(): WA_AGG_SetTestResult(): failed\n");

    WA_LOG_GetTimestampStr(timestamp, strtimestamp, sizeof(strtimestamp));

    if(jparams != NULL)
        qjmsg.json = json_pack("{s:s,s:s,s:{s:s,s:i,s:s,s:o},s:n}", "jsonrpc", "2.0", "method", "eod", "params", "diag", tmp, "status", status, "timestamp", strtimestamp, "data", jparams, "id");
    else
        qjmsg.json = json_pack("{s:s,s:s,s:{s:s,s:i,s:s},s:n}", "jsonrpc", "2.0", "method", "eod", "params", "diag", tmp, "status", status, "timestamp", strtimestamp, "id");

    status = WA_OSA_QTimedRetrySend(WA_INIT_IncomingQ,
            (const char * const)&qjmsg,
            sizeof(qjmsg),
            WA_OSA_Q_PRIORITY_MAX,
            WA_OSA_Q_SEND_TIMEOUT_DEFAULT,
            WA_OSA_Q_SEND_RETRY_DEFAULT);
    if(status != 0)
    {
        WA_ERROR("InstanceTask(): WA_OSA_QSend(): %d\n", status);
    }

    end:
    status = WA_OSA_QSend(collectorQ, (const char * const)&pInstance, sizeof(pInstance), WA_OSA_Q_PRIORITY_MAX);
    if(status != 0)
    {
        WA_ERROR("InstanceTask(): WA_OSA_QSend(): %d\n", status);
    }

    WA_RETURN("InstanceTask(): %p\n", p);
    return p;
}

static WA_DIAG_procedureInstance_t *FindInstanceById(uint32_t id)
{
    WA_DIAG_procedureContext_t *pContext;
    WA_DIAG_procedureInstance_t *pInstance;
    void *iterator;
    void *instanceIterator;

    WA_ENTER("FindInstanceById(id=0x%x)\n", id);

    for(iterator = WA_UTILS_LIST_FrontIterator(&(diagProcedures.proceduresList));
            iterator != WA_UTILS_LIST_NO_ELEM;
            iterator = WA_UTILS_LIST_NextIterator(&(diagProcedures.proceduresList), iterator))
    {
        pContext = (WA_DIAG_procedureContext_t *)WA_UTILS_LIST_DataAtIterator(&(diagProcedures.proceduresList), iterator);

        for(instanceIterator = WA_UTILS_LIST_FrontIterator(&(pContext->instancesList));
                instanceIterator != WA_UTILS_LIST_NO_ELEM;
                instanceIterator = WA_UTILS_LIST_NextIterator(&(pContext->instancesList), instanceIterator))
        {
            pInstance = (WA_DIAG_procedureInstance_t *)WA_UTILS_LIST_DataAtIterator(&(pContext->instancesList), instanceIterator);
            if(pInstance->id == id)
            {
                goto end;
            }
        }
    }
    pInstance = NULL;
    end:
    WA_RETURN("FindInstanceById(): %p\n", pInstance);
    return pInstance;
}

static int DiagControl(json_t **json)
{
    int status, s1;
    json_t *jps;
    char *method;
    uint32_t instanceId;
    WA_DIAG_procedureInstance_t *pInstance;

    WA_ENTER("DiagControl(json=%p)\n", json);

    status = json_unpack(*json, "{s:o}", "params", &jps); //reference to jParams is NOT modified
    if(status != 0)
    {
        WA_INFO("DiagControl(): json_unpack() params undefined\n");
        goto end;
    }

    if(json_typeof(jps) != JSON_OBJECT)
    {
        WA_INFO("DiagControl(): params is not an object\n");
        status = -1;
        goto end;
    }

    /* Currently handle only 'break' */
    status = json_unpack(jps, "{s:s}", "break", &method);
    if(status != 0)
    {
        WA_INFO("DiagControl(): json_unpack() incorrect syntax\n");
        goto end;
    }

    if(method[0] != '#')
    {
        WA_INFO("DiagControl(): incorrect instance\n");
        status = -1;
        goto end;
    }

    instanceId = (uint32_t)strtol(method+1, NULL, 16);

    status = WA_OSA_MutexLock(diagProcedures.mutex);
    if(status != 0)
    {
        WA_ERROR("DiagControl(): WA_OSA_MutexLock(): %d\n", status);
        goto end;
    }

    pInstance = FindInstanceById(instanceId);
    if(pInstance == NULL)
    {
        WA_INFO("DiagControl(): instance not found\n");
        status = -1;
        goto unlock;
    }

    status = WA_OSA_TaskSignalQuit(pInstance->taskHandle);
    if(status != 0)
    {
        WA_ERROR("DiagControl(): WA_OSA_TaskSignal(InstanceTask): error\n");
    }

    unlock:
    s1 = WA_OSA_MutexUnlock(diagProcedures.mutex);
    if(s1 != 0)
    {
        WA_ERROR("DiagControl(): WA_OSA_MutexUnlock(): %d\n", s1);
        status = -1;
    }
    end:
    json_decref(*json);
    *json = NULL;

    WA_RETURN("DiagControl(): %d\n", status);
    return status;
}

static int TestRunControl(json_t **json)
{
    int status;
    json_t *jps;
    char *testrun_status = NULL;
    char *testrun_client = NULL;

    WA_ENTER("TestRunControl(json=%p)\n", json);

    status = json_unpack(*json, "{s:o}", "params", &jps); //reference to jParams is NOT modified
    if(status != 0)
    {
        WA_INFO("TestRunControl(): json_unpack() params undefined\n");
        goto end;
    }

    if(json_typeof(jps) != JSON_OBJECT)
    {
        WA_INFO("TestRunControl(): params is not an object\n");
        status = -1;
        goto end;
    }

    status = json_unpack(jps, "{s:s}", "state", &testrun_status);
    if(status != 0)
    {
        WA_INFO("TestRunControl(): json_unpack() incorrect state syntax\n");
        goto end;
    }

    if (!strcasecmp(testrun_status, "start"))
    {
        status = json_unpack(jps, "{s:s}", "client", &testrun_client);
        if(status != 0)
        {
            WA_INFO("TestRunControl(): json_unpack() incorrect client syntax\n");
            goto end;
        }

        if (WA_AGG_StartTestRun(testrun_client, time(0)))
        {
            WA_ERROR("TestRunControl(): WA_AGG_StartTestRun failed\n");
            status = -1;
        }
    }
    else if (!strcasecmp(testrun_status, "finish"))
    {
        if (WA_AGG_FinishTestRun(time(0)))
        {
            WA_ERROR("TestRunControl(): WA_AGG_FinishTestRun failed\n");
            status = -1;
        }
    }
    else
    {
        WA_ERROR("TestRunControl(): invalid testrun status %s\n", testrun_status);
        status = -1;
    }

    end:
    json_decref(*json);
    *json = NULL;

    WA_RETURN("TestRunControl(): %d\n", status);
    return status;
}

/* End of doxygen group */
/*! @} */

/* EOF */
