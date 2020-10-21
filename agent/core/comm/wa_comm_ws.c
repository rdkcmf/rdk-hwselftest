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
 * @file
 *
 * @brief This file contains main function for websockets comm adaptor.
 */

/** @addtogroup WA_COMM_WS
 *  @{
 */

/*****************************************************************************
 * STANDARD INCLUDE FILES
 *****************************************************************************/
#include <string.h>
#include <stdbool.h>

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/
#include <libwebsockets.h>

#include "wa_comm_ws.h"
#include "wa_json.h"
#include "wa_osa.h"
#include "wa_debug.h"

/*****************************************************************************
 * GLOBAL VARIABLE DEFINITIONS
 *****************************************************************************/
extern void WA_MAIN_Quit(bool);

/*****************************************************************************
 * LOCAL DEFINITIONS
 *****************************************************************************/
#define WS_MAX_PAYLOAD 2048 /* Increased size due to DELIA-47381 */

/*****************************************************************************
 * LOCAL TYPES
 *****************************************************************************/
typedef struct
{
    bool taskRun;
    void *taskHandle;
    const WA_COMM_adaptersConfig_t *pConfig;
    struct lws_context *lwsContext;
    int id;
    int txTimeout;
}WA_COMM_WS_context_t;

typedef struct
{
    bool valid;
    struct lws *wsi;
    WA_COMM_WS_context_t *pContext;
    void *registrationHandle;
    void *conVarSend;
    char *txMsg;
    size_t txMsgSize;
    size_t txMsgWritten;
    bool rxMsgLeft;
}WA_COMM_WS_connection_t;
/*****************************************************************************
 * LOCAL FUNCTION PROTOTYPES
 *****************************************************************************/
static void *CommWsTask(void *p);
static int LwsCallback(struct lws *wsi, enum lws_callback_reasons reason, void *user,
          void *in, size_t len);

/*****************************************************************************
 * LOCAL VARIABLE DECLARATIONS
 *****************************************************************************/
static struct lws_protocols protocols[] =
{
    {/* first must be HTTP */
        "",
        LwsCallback,
        sizeof(WA_COMM_WS_connection_t),
        WS_MAX_PAYLOAD,
    },
    {
        NULL, NULL, 0
    }
};

static const struct lws_extension extensions[] =
{
    {
        "permessage-deflate",
        lws_extension_callback_pm_deflate,
        "permessage-deflate; client_no_context_takeover; client_max_window_bits"
    },
    {
        "deflate-frame",
        lws_extension_callback_pm_deflate,
        "deflate_frame"
    },
    { NULL, NULL, NULL /* terminator */ }
};

static int taskNo = 0;
static int connectionsNum = 0;

/*****************************************************************************
 * FUNCTION DEFINITIONS
 *****************************************************************************/

void * WA_COMM_WS_Init(WA_COMM_adaptersConfig_t *config)
{
    WA_COMM_WS_context_t *pContext = NULL;
    json_t *json;
    int port = 8003;
    int txTimeout = 3000;
    size_t maxPayload = WS_MAX_PAYLOAD;
    int status;
    struct lws_context_creation_info info;

    pContext = malloc(sizeof(WA_COMM_WS_context_t));
    if(pContext == NULL)
    {
        WA_ERROR("WA_COMM_WS_Init(): malloc(): error\n");
        goto err_malloc;
    }

    json = config->config;
    if (json)
    {
        if (!json_is_object(json))
        {
            WA_ERROR("WA_COMM_WS_Init(): configuration is not a JSON object\n");
        }
        else
        {
            if (json_object_get(json, "port"))
            {
                status = json_unpack(json, "{si}", "port", &port);
                if(status != 0)
                {
                    WA_ERROR("WA_COMM_WS_Init(): json_unpack() error\n");
                }
            }
            if (json_object_get(json, "max_payload"))
            {
                status = json_unpack(json, "{si}", "max_payload", &maxPayload);
                if(status != 0)
                {
                    WA_INFO("WA_COMM_WS_Init(): json_unpack() error\n");
                }
            }
            if (json_object_get(json, "tx_timeout"))
            {
                status = json_unpack(json, "{si}", "tx_timeout", &txTimeout);
                if(status != 0)
                {
                    WA_INFO("WA_COMM_WS_Init(): json_unpack() error\n");
                }
            }
        }
    }

    WA_INFO("WA_COMM_WS_Init(): port=%d max_payload=%zu tx_timeout=%d\n", port, maxPayload, txTimeout);

    protocols[0].rx_buffer_size = maxPayload;
    protocols[0].user = (void *)pContext;

    memset(&info, 0, sizeof info);
    info.port = port;
    info.iface = "lo";
    info.protocols = protocols;
    info.gid = -1;
    info.uid = -1;
    info.options = LWS_SERVER_OPTION_VALIDATE_UTF8;
    info.extensions = extensions;

    lws_set_log_level(0,NULL);
    pContext->lwsContext = lws_create_context(&info);

    if (pContext->lwsContext == NULL)
    {
        WA_ERROR("WA_COMM_WS_Init(): lws_create_context() error\n");
        goto err_lws;
    }

    pContext->id = taskNo;
    pContext->txTimeout = txTimeout;
    pContext->pConfig = config;
    pContext->taskRun = true;
    pContext->taskHandle = WA_OSA_TaskCreate(NULL, 0, CommWsTask, pContext, WA_OSA_SCHED_POLICY_RT, WA_OSA_TASK_PRIORITY_MAX);
    if(pContext->taskHandle == NULL)
    {
        WA_ERROR("WA_COMM_WS_Init(): WA_OSA_TaskCreate(CommWsTask): error\n");
        goto err_task;
    }

    ++taskNo;

    /* */
    goto end;

err_task:
    lws_context_destroy(pContext->lwsContext);
err_lws:
    free(pContext);
    pContext = NULL;
err_malloc:
end:
    WA_RETURN("WA_COMM_WS_Init(): %p\n", pContext);
    return (void *)pContext;
}

int WA_COMM_WS_Exit(void *handle)
{
    int status = -1;
    WA_COMM_WS_context_t *pContext = (WA_COMM_WS_context_t *)handle;

    WA_ENTER("WA_COMM_WS_Exit(handle=%p)\n", handle);

    pContext->taskRun = false;
    status = WA_OSA_TaskSignalQuit(pContext->taskHandle);
    if(status != 0)
    {
        WA_ERROR("WA_COMM_WS_Exit(): WA_OSA_TaskSignal(CommEthTask): error\n");
        goto end;
    }

    status = WA_OSA_TaskJoin(pContext->taskHandle, NULL);
    if(status != 0)
    {
        WA_ERROR("WA_COMM_WS_Exit(): WA_OSA_TaskJoin(CommEthTask): error\n");
        goto end;
    }

    status = WA_OSA_TaskDestroy(pContext->taskHandle);
    if(status != 0)
    {
        WA_ERROR("WA_COMM_WS_Exit(): WA_OSA_TaskDestroy(CommEthTask): error\n");
        goto end;
    }

    --taskNo;

    lws_context_destroy(pContext->lwsContext);

    free(pContext);
end:
    WA_RETURN("WA_COMM_WS_Exit(): %d\n", status);
    return status;
}

int WA_COMM_WS_Callback(void *cookie, json_t *json)
{
    char *msg, *wsMsg;
    int status = -1, s1;
    size_t s;
    WA_COMM_WS_connection_t *pConnection = (WA_COMM_WS_connection_t *)cookie;

    WA_ENTER("WA_COMM_WS_Callback(cookie=%p, json=%p)\n", cookie, json);

    msg = json_dumps(json, JSON_PRESERVE_ORDER | JSON_ENCODE_ANY);
    if(!msg)
    {
        WA_ERROR("WA_COMM_WS_Callback(): json_dumps(): error\n");
        goto end;
    }

    /* Wile sending the outgoing buffer must be pre-padded with LWS_PRE size. */
    s = strlen(msg);
    wsMsg = malloc(LWS_PRE + s);
    if(!wsMsg)
    {
        free(msg);
        WA_ERROR("WA_COMM_WS_Callback(): malloc(): error\n");
        goto end;
    }

    memcpy(&wsMsg[LWS_PRE], msg, s);
    free(msg);

    status = WA_OSA_CondLock(pConnection->conVarSend);
    if(status != 0)
    {
        free(wsMsg);
        WA_ERROR("WA_COMM_WS_Callback(): WA_OSA_CondLock(): %d\n", status);
        goto end;
    }
    if(pConnection->valid)
    {
        while(pConnection->txMsg != NULL)
        {
            status = WA_OSA_CondTimedWait(pConnection->conVarSend, pConnection->pContext->txTimeout);
            if((status != 0) || !pConnection->valid)
            {
                /* error or timeout */
                goto closing;
            }
        }

        pConnection->txMsgSize = s;
        pConnection->txMsgWritten = 0;
        pConnection->txMsg = wsMsg; //set this as the last operation so WA_OSA_Cond*() is not needed in LwsCallback()
        lws_callback_on_writable(pConnection->wsi);
    }
    else
    {
closing:
        /* closing */
        free(wsMsg);
        status = -1;
    }
    s1 = WA_OSA_CondUnlock(pConnection->conVarSend);
    if(s1 != 0)
    {
        free(wsMsg);
        WA_ERROR("WA_COMM_WS_Callback(): WA_OSA_CondLock(): %d\n", s1);
        status = -1;
        goto end;
    }

end:
    json_decref(json);

    WA_RETURN("WA_COMM_WS_Callback(): %d\n", status);
    return status;
}


/*****************************************************************************
 * LOCAL FUNCTIONS
 *****************************************************************************/

static void *CommWsTask(void *p)
{
    WA_COMM_WS_context_t *pContext = (WA_COMM_WS_context_t *)p;
    char taskName[WA_OSA_TASK_NAME_MAX_LEN];
    int status;

    WA_ENTER("CommWsTask(p=%p)\n", p);

    snprintf(taskName, WA_OSA_TASK_NAME_MAX_LEN, "WAcommws%d", pContext->id);
    status = WA_OSA_TaskSetName(taskName);
    if(status != 0)
    {
        WA_ERROR("CommWsTask(): WA_OSA_TaskSetName(): %d\n", status);
        goto end;
    }

    while(pContext->taskRun)
    {
        status = lws_service(pContext->lwsContext, 10);
        if(status < 0)
        {
            WA_ERROR("CommWsTask(): lws_service(): %d\n", status);
            goto end;
        }
    }
end:
    WA_RETURN("CommWsTask(): %p\n", p);
    return p;
}

static int LwsCallback(struct lws *wsi, enum lws_callback_reasons reason, void *user,
          void *in, size_t len)
{
    int status = 0, n;
    WA_COMM_WS_connection_t *pConnection;

    switch (reason)
    {
    case LWS_CALLBACK_WSI_CREATE:
        WA_INFO("LwsCallback(): LWS_CALLBACK_WSI_CREATE\n");
        WA_MAIN_Quit(false);
        ++connectionsNum;
        WA_INFO("LwsCallback(): LWS_CALLBACK_WSI_CREATE END\n");
        break;
    case LWS_CALLBACK_WSI_DESTROY:
        WA_INFO("LwsCallback(): LWS_CALLBACK_WSI_DESTROY\n");
        --connectionsNum;
        WA_INFO("LwsCallback(): LWS_CALLBACK_WSI_DESTROY END\n");
        break;
    case LWS_CALLBACK_FILTER_NETWORK_CONNECTION:
        WA_INFO("LwsCallback(): LWS_CALLBACK_FILTER_NETWORK_CONNECTION\n");
        if(connectionsNum)
        {
            WA_INFO("LwsCallback(): Multiple connections not allowed - dropping.\n");
            status = -1;
        }
        WA_INFO("LwsCallback(): LWS_CALLBACK_FILTER_NETWORK_CONNECTION END\n");
        break;

    case LWS_CALLBACK_ESTABLISHED:
        WA_INFO("LwsCallback(): LWS_CALLBACK_ESTABLISHED\n");
        pConnection = (WA_COMM_WS_connection_t *)user;

        pConnection->pContext = (WA_COMM_WS_context_t *)(lws_get_protocol(wsi)->user);
        pConnection->wsi = wsi;
        pConnection->valid = false;
        pConnection->txMsg = NULL;
        pConnection->rxMsgLeft = false;
        pConnection->conVarSend = WA_OSA_CondCreate();
        if(pConnection->conVarSend == NULL)
        {
            WA_ERROR("LwsCallback(): WA_OSA_CondCreate() error\n");
            status = -1;
            break; //ToDo: handle error signaling
        }

        pConnection->registrationHandle = WA_COMM_Register(pConnection->pContext->pConfig, (void *)pConnection);
        if(pConnection->registrationHandle == NULL)
        {
            WA_ERROR("LwsCallback(): WA_COMM_Register(): null\n");

            status = WA_OSA_CondDestroy(pConnection->conVarSend);
            if(status != 0)
            {
                WA_ERROR("LwsCallback() WA_OSA_CondDestroy(): %d\n", status);
                //ToDo: handle error signaling
            }
            status = -1;
            break;
        }

        pConnection->valid = true;
        WA_INFO("LwsCallback(): LWS_CALLBACK_ESTABLISHED END\n");
        break;

    case LWS_CALLBACK_CLOSED:
        WA_INFO("LwsCallback(): LWS_CALLBACK_CLOSED\n");
        pConnection = (WA_COMM_WS_connection_t *)user;
        if(pConnection->valid == false)
        {
            status = -1;
            break;
        }

        // signal pending callback to exit
        status = WA_OSA_CondLock(pConnection->conVarSend);
        if(status != 0)
        {
            WA_ERROR("LwsCallback(): WA_OSA_CondLock(): %d\n", status);
            break; //ToDo: handle error signaling
        }
        pConnection->valid = false; //flag pending callback should stop
        if(pConnection->txMsg != NULL)
        {
            free(pConnection->txMsg);
        }
        status = WA_OSA_CondSignalBroadcast(pConnection->conVarSend);
        if(status != 0)
        {
            WA_ERROR("LwsCallback(): WA_OSA_CondSignalBroadcast() %d\n", status);
            break; //ToDo: handle error signaling
        }
        status = WA_OSA_CondUnlock(pConnection->conVarSend);
        if(status != 0)
        {
            WA_ERROR("LwsCallback(): WA_OSA_CondUnlock() %d\n", status);
            break; //ToDo: handle error signaling
        }

        status = WA_COMM_Unregister(pConnection->registrationHandle);
        if(status != 0)
        {
            WA_ERROR("LwsCallback() WA_COMM_Unregister(): %d\n", status);
            break; //ToDo: handle error signaling
        }

        status = WA_OSA_CondDestroy(pConnection->conVarSend);
        if(status != 0)
        {
            WA_ERROR("LwsCallback() WA_OSA_CondDestroy(): %d\n", status);
            break; //ToDo: handle error signaling
        }
        WA_INFO("LwsCallback(): LWS_CALLBACK_CLOSED END\n");
        break;

    case LWS_CALLBACK_SERVER_WRITEABLE:
        WA_INFO("LwsCallback(): LWS_CALLBACK_SERVER_WRITEABLE\n");
        pConnection = (WA_COMM_WS_connection_t *)user;
        if(pConnection->valid == false)
        {
            status = -1;
            break;
        }

        if(pConnection->txMsg != NULL)
        {
            n = lws_write(wsi, (unsigned char *)(pConnection->txMsg + LWS_PRE + pConnection->txMsgWritten), pConnection->txMsgSize - pConnection->txMsgWritten, LWS_WRITE_TEXT);
            if(n < 0)
            {
                WA_ERROR("LwsCallback() lws_write(): %d\n", status);
                pConnection->txMsgWritten = pConnection->txMsgSize;
            }
            else
            {
                pConnection->txMsgWritten += n;
            }

            if(pConnection->txMsgWritten == pConnection->txMsgSize)
            {
                status = WA_OSA_CondLock(pConnection->conVarSend);
                if(status != 0)
                {
                    WA_ERROR("LwsCallback(): WA_OSA_CondLock() %d\n", status);
                    break; //ToDo: handle error signaling
                }

                free(pConnection->txMsg);
                pConnection->txMsg = NULL;

                status = WA_OSA_CondSignal(pConnection->conVarSend);
                if(status != 0)
                {
                    WA_ERROR("LwsCallback(): WA_OSA_CondSignal() %d\n", status);
                    break; //ToDo: handle error signaling
                }
                status = WA_OSA_CondUnlock(pConnection->conVarSend);
                if(status != 0)
                {
                    WA_ERROR("LwsCallback(): WA_OSA_CondUnlock() %d\n", status);
                    break; //ToDo: handle error signaling
                }
            }
            else
            {
                lws_callback_on_writable(wsi);
            }
        }
        WA_INFO("LwsCallback(): LWS_CALLBACK_SERVER_WRITEABLE END\n");
        break;

    case LWS_CALLBACK_RECEIVE:
        WA_INFO("LwsCallback(): LWS_CALLBACK_RECEIVE\n");
        pConnection = (WA_COMM_WS_connection_t *)user;
        if(pConnection->valid == false)
        {
            status = -1;
            break;
        }

        if(!pConnection->rxMsgLeft)
        {
            WA_DBG("LwsCallback(): rx:[%.*s] %zu\n", (int)len, (char *)in, len);
            WA_COMM_SendTxt(pConnection->registrationHandle, in, len);
        }
        else
        {
            WA_ERROR("LwsCallback(): RX overflow\n");
        }

        /* The message needs to be atomic (must fit into one buffer) */
        pConnection->rxMsgLeft = !!lws_remaining_packet_payload(wsi);
        WA_INFO("LwsCallback(): LWS_CALLBACK_RECEIVE END\n");
        break;

    default:
        break;
    }

    switch(reason)
    {
    case LWS_CALLBACK_CLOSED:
    case LWS_CALLBACK_WSI_DESTROY:
        WA_MAIN_Quit(true);
    default:
        break;
    }

    return status;
}
/* End of doxygen group */
/*! @} */


