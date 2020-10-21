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
 * @file wa_diag_wan.c
 *
 * @brief Diagnostic functions for WAN - implementation
 */

/** @addtogroup WA_DIAG_WAN
 *  @{
 */

/*****************************************************************************
 * STANDARD INCLUDE FILES
 *****************************************************************************/

#define _GNU_SOURCE
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/

#include "wa_diag.h"
#include "wa_debug.h"
#include "wa_fileops.h"

/* rdk specific */
#include "wa_iarm.h"
#include "wa_json.h"
#include "libIBus.h"
#include "libIARMCore.h"
#include "sysMgr.h"
#include "xdiscovery.h"

/* module interface */
#include "wa_diag_wan.h"

/*****************************************************************************
 * GLOBAL VARIABLE DEFINITIONS
 *****************************************************************************/

/*****************************************************************************
 * LOCAL DEFINITIONS
 *****************************************************************************/

#define TR69_WAN_XRE_CONN_STATUS  "Device.X_COMCAST-COM_Xcalibur.Client.XRE.ConnectionTable.xreConnStatus"
#define TR69_WAN_RFC_PUBLIC_URL   "Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.hwHealthTestWAN.WANTestEndPointURL"

#define STR_MAX              256
#define MESSAGE_LENGTH       8192 * 4 /* On reference from xdiscovery.log which shows data length can be more than 5000 */ /* Increased the value 4 times because of DELIA-38611 */
#define NUM_PINGS            10
#define NUM_PINGS_SUCCESSFUL 8
#define CONNECTION_SUCCESS   1
#define CONNECTION_FAILED    0

/*****************************************************************************
 * LOCAL TYPES
 *****************************************************************************/

/*****************************************************************************
 * LOCAL FUNCTION PROTOTYPES
 *****************************************************************************/

int setReturnData(int status, json_t** param);
int gatewayConnection();
int comcastNetwork();
int publicNetwork();

/*****************************************************************************
 * LOCAL VARIABLE DECLARATIONS
 *****************************************************************************/

int var_size = STR_MAX;

/*****************************************************************************
 * FUNCTION DEFINITIONS
 *****************************************************************************/

/*****************************************************************************
 * LOCAL FUNCTIONS
 *****************************************************************************/
int setReturnData(int status, json_t **param)
{
    if(param == NULL)
        return status;

    switch (status)
    {
        case WA_DIAG_ERRCODE_SUCCESS:
            *param = json_string("Comcast and public network connections successful.");
            break;

        case WA_DIAG_ERRCODE_NO_GATEWAY_CONNECTION:
            *param = json_string("No X1 Gateway Connection.");
            break;

        case WA_DIAG_ERRCODE_NO_COMCAST_WAN_CONNECTION:
            *param = json_string("No Comcast WAN Connection.");
            break;

        case WA_DIAG_ERRCODE_NO_PUBLIC_WAN_CONNECTION:
            *param = json_string("No Public WAN Connection.");
            break;

        case WA_DIAG_ERRCODE_NO_WAN_CONNECTION:
            *param = json_string("No WAN Connection. Check Connection");
            break;

        case WA_DIAG_ERRCODE_CANCELLED:
            *param = json_string("Test cancelled.");
            break;

        case WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR:
            *param = json_string("Internal test error.");
            break;

        default:
            break;
    }

    return status;
}

/* Return 1:good 0:failed -1:IARM errors */
int gatewayConnection()
{
    WA_ENTER("gatewayConnection()\n");

    int result = -1;
    IARM_Result_t iarm_result = IARM_RESULT_IPCCORE_FAIL;
    int is_connected = 0;

    IARM_Bus_SYSMGR_GetXUPNPDeviceInfo_Param_t *param = NULL;
    char upnpResults[MESSAGE_LENGTH+1];
    json_error_t jerror;
    json_t *json = NULL;

    iarm_result = IARM_Bus_IsConnected(_IARM_XUPNP_NAME, &is_connected);
    if (iarm_result != IARM_RESULT_SUCCESS)
    {
        WA_ERROR("gatewayConnection(): IARM_Bus_IsConnected('%s') failed\n", _IARM_XUPNP_NAME);
        return result;
    }

    if (!is_connected)
    {
        WA_ERROR("gatewayConnection(): XUPNP not available\n");
        return result;
    }

    iarm_result = IARM_Malloc(IARM_MEMTYPE_PROCESSLOCAL, sizeof(IARM_Bus_SYSMGR_GetXUPNPDeviceInfo_Param_t) + MESSAGE_LENGTH + 1, (void**)&param);
    if (iarm_result != IARM_RESULT_SUCCESS)
    {
        WA_ERROR("gatewayConnection(): IARM_Malloc() failed\n");
        return result;
    }

    if (param)
    {
        memset(param, 0, sizeof(*param));
        param->bufLength = MESSAGE_LENGTH;

        iarm_result = IARM_Bus_Call(_IARM_XUPNP_NAME, IARM_BUS_XUPNP_API_GetXUPNPDeviceInfo, (void *)param, sizeof(IARM_Bus_SYSMGR_GetXUPNPDeviceInfo_Param_t) + MESSAGE_LENGTH + 1);

        if (iarm_result != IARM_RESULT_SUCCESS)
        {
            WA_ERROR("gatewayConnection(): IARM_Bus_Call() returned %i\n", iarm_result);
            IARM_Free(IARM_MEMTYPE_PROCESSLOCAL, param);
            return result;
        }

        WA_DBG("gatewayConnection(): IARM_RESULT_SUCCESS\n");
        memcpy(upnpResults, ((char *)param + sizeof(IARM_Bus_SYSMGR_GetXUPNPDeviceInfo_Param_t)), param->bufLength);
        upnpResults[param->bufLength] = '\0';

        json = json_loadb(upnpResults, MESSAGE_LENGTH, JSON_DISABLE_EOF_CHECK, &jerror);
        if (!json)
        {
            WA_ERROR("gatewayConnection(): json_loads() error\n");
            json_decref(json);
            IARM_Free(IARM_MEMTYPE_PROCESSLOCAL, param);
            return result;
        }

        json_t *jarr = json_object_get(json, "xmediagateways");
        if (!jarr)
        {
            WA_ERROR("gatewayConnection(): Couldn't get the array of xmediagateways\n");
            json_decref(jarr);
            json_decref(json);
            IARM_Free(IARM_MEMTYPE_PROCESSLOCAL, param);
            return result;
        }

        for (size_t i = 0; i < json_array_size(jarr); i++)
        {
            json_t *jval = json_array_get(jarr, i);
            if (!jval)
            {
                WA_ERROR("gatewayConnection(): Not a json object in xmediagateways array for %i element\n", i);
                json_decref(jval);
                json_decref(jarr);
                json_decref(json);
                IARM_Free(IARM_MEMTYPE_PROCESSLOCAL, param);
                return result;
            }

            json_t *jparam_devType = json_object_get(jval, "DevType");
            char device[STR_MAX] = {'\0'};
            strcpy(device, json_string_value(jparam_devType));

            if (strstr(device, "XG") || strstr(device, "XB"))
            {
                json_t *jparam_IP = json_object_get(jval, "gatewayip");
                if (jparam_IP)
                {
                    char gtw_ip[STR_MAX] = {'\0'};
                    char buf[STR_MAX] = {'\0'};
                    strcpy(gtw_ip, json_string_value(jparam_IP));
                    snprintf(buf, var_size, "ping -c 1 -W 1 %s > /dev/null", gtw_ip);
                    if (system(buf) == 0)
                    {
                        WA_DBG("gatewayConnection(): Ping to local gateway %s with IP %s is successful.\n", device, gtw_ip);
                        result = CONNECTION_SUCCESS;
                        break;
                    }
                }
            }
        }

        if (result != CONNECTION_SUCCESS)
        {
            WA_DBG("gatewayConnection(): Unable to ping any local gateway.\n");
            result = CONNECTION_FAILED;
        }
    }

    IARM_Free(IARM_MEMTYPE_PROCESSLOCAL, param);
    WA_RETURN("gatewayConnection(): Returns with result %d.\n", result);

    return result;
}

/* Return 1:Connected 0:Not Connected/any other -1:IARM errors */
int comcastNetwork()
{
    WA_ENTER("comcastNetwork()\n");

    IARM_Result_t iarm_result = IARM_RESULT_IPCCORE_FAIL;
    int is_connected = 0;
    int result = -1;

    iarm_result = IARM_Bus_IsConnected(IARM_BUS_TR69HOSTIFMGR_NAME, &is_connected);
    if (iarm_result != IARM_RESULT_SUCCESS)
    {
        WA_ERROR("comcastNetwork(): IARM_Bus_IsConnected('%s') failed\n", IARM_BUS_TR69HOSTIFMGR_NAME);
        return result;
    }

    HOSTIF_MsgData_t *stMsgDataParam = NULL;

    iarm_result = IARM_Malloc(IARM_MEMTYPE_PROCESSLOCAL, sizeof(*stMsgDataParam), (void**)&stMsgDataParam);
    if ((iarm_result == IARM_RESULT_SUCCESS) && stMsgDataParam)
    {
        memset(stMsgDataParam, 0, sizeof(*stMsgDataParam));
        snprintf(stMsgDataParam->paramName, TR69HOSTIFMGR_MAX_PARAM_LEN, "%s", TR69_WAN_XRE_CONN_STATUS);

        iarm_result = IARM_Bus_Call(IARM_BUS_TR69HOSTIFMGR_NAME, IARM_BUS_TR69HOSTIFMGR_API_GetParams, (void *)stMsgDataParam, sizeof(*stMsgDataParam));

        if (iarm_result != IARM_RESULT_SUCCESS)
        {
            WA_ERROR("comcastNetwork(): IARM_Bus_Call('%s') failed\n", IARM_BUS_TR69HOSTIFMGR_NAME);
            IARM_Free(IARM_MEMTYPE_PROCESSLOCAL, stMsgDataParam);
            return result;
        }

        char *xreStatus = (char *)&stMsgDataParam->paramValue[0];
        WA_DBG("comcastNetwork(): XRE connection status from %s is \"%s\"\n", TR69_WAN_XRE_CONN_STATUS, xreStatus);

        if (!strcasecmp(xreStatus, "Connected"))
            result = CONNECTION_SUCCESS;
        else
            result = CONNECTION_FAILED;

        IARM_Free(IARM_MEMTYPE_PROCESSLOCAL, stMsgDataParam);
    }
    else
        WA_ERROR("comcastNetwork(): IARM_Malloc() failed\n");

    WA_RETURN("comcastNetwork() returns : %d\n", result);

    return result;
}

/* Return 1:good 0:failed -1:IARM errors */
int publicNetwork()
{
    WA_ENTER("publicNetwork()\n");

    IARM_Result_t iarm_result = IARM_RESULT_IPCCORE_FAIL;
    int is_connected = 0;
    int result = -1;
    char buf[STR_MAX] = {'\0'};
    char host[STR_MAX] = {'\0'};

    iarm_result = IARM_Bus_IsConnected(IARM_BUS_TR69HOSTIFMGR_NAME, &is_connected);

    if (iarm_result == IARM_RESULT_SUCCESS)
    {
        HOSTIF_MsgData_t *stMsgDataParam = NULL;

        iarm_result = IARM_Malloc(IARM_MEMTYPE_PROCESSLOCAL, sizeof(*stMsgDataParam), (void**)&stMsgDataParam);
        if ((iarm_result == IARM_RESULT_SUCCESS) && stMsgDataParam)
        {
            memset(stMsgDataParam, 0, sizeof(*stMsgDataParam));
            snprintf(stMsgDataParam->paramName, TR69HOSTIFMGR_MAX_PARAM_LEN, "%s", TR69_WAN_RFC_PUBLIC_URL);

            iarm_result = IARM_Bus_Call(IARM_BUS_TR69HOSTIFMGR_NAME, IARM_BUS_TR69HOSTIFMGR_API_GetParams, (void *)stMsgDataParam, sizeof(*stMsgDataParam));

            if (iarm_result == IARM_RESULT_SUCCESS)
            {
                strcpy(host, &stMsgDataParam->paramValue[0]);
                WA_DBG("publicNetwork(): WAN URL from RFC - \"%s\"\n", host);
            }
            else
            {
                WA_ERROR("publicNetwork(): IARM_Bus_Call('%s') failed\n", IARM_BUS_TR69HOSTIFMGR_NAME);
                IARM_Free(IARM_MEMTYPE_PROCESSLOCAL, stMsgDataParam);
                return result;
            }

            IARM_Free(IARM_MEMTYPE_PROCESSLOCAL, stMsgDataParam);
        }
        else
        {
            WA_ERROR("publicNetwork(): IARM_Malloc('%s') failed\n", IARM_BUS_TR69HOSTIFMGR_NAME);
            return result;
        }
    }
    else
    {
        WA_ERROR("publicNetwork(): IARM_Bus_IsConnected('%s') failed\n", IARM_BUS_TR69HOSTIFMGR_NAME);
        return result;
    }

    if (!strcmp(host, ""))
    {
        WA_DBG("publicNetwork(): WAN URL from RFC is empty, public wan check is skipped\n");
        return result;
    }

    /* Get IPv6 for URL */
    char nslookup_command[STR_MAX] = {'\0'};
    char IPv6[STR_MAX] = {'\0'};
    FILE *fp;
    snprintf(nslookup_command, var_size, "nslookup %s | grep 'Address' | sed -n '2 p' | awk '{print $3}'", host);
    fp = popen(nslookup_command, "r");
    if (fp == NULL)
    {
        WA_DBG("publicNetwork(): Unable to fetch IPv6 for URL \"%s\" through 'nslookup'\n");
        return result;
    }

    if ((fgets(IPv6, sizeof(IPv6), fp) != NULL) && (strcmp(IPv6, "") != 0))
    {
        WA_DBG("publicNetwork(): IPv6 for given URL %s is %s\n", host, IPv6);
    }
    else
    {
        WA_DBG("publicNetwork(): Cannot resolve IPv6. Treating \"%s\" as invalid URL, skipping public wan test...\n", host);
        pclose(fp);
        return result;
    }

    pclose(fp);

    snprintf(buf, var_size, "ping -c 1 -W 1 %s > /dev/null", IPv6);

    int pingCount = 0;
    for (int i = 0; i < NUM_PINGS; i++)
    {
        if (system(buf) == 0)
        {
            pingCount++;
        }
    }

    if (pingCount >= NUM_PINGS_SUCCESSFUL)
    {
        WA_DBG("publicNetwork(): PING successful for IP %s\n", IPv6);
        result = CONNECTION_SUCCESS;
    }
    else
    {
        WA_DBG("publicNetwork(): PING to IP %s failed %d times\n", IPv6, (NUM_PINGS - pingCount));
        result = CONNECTION_FAILED;
    }

    WA_RETURN("publicNetwork() returns : %d\n", result);

    return result;
}

int WA_DIAG_WAN_status(void *instanceHandle, void *initHandle, json_t **params)
{
    WA_ENTER("WA_DIAG_WAN_status(): Enters\n");

    if (WA_OSA_TaskCheckQuit())
    {
        WA_ERROR("WA_DIAG_WAN_status(): test cancelled\n");
        return setReturnData(WA_DIAG_ERRCODE_CANCELLED, params);
    }

    if (WA_UTILS_IARM_Connect())
    {
        WA_ERROR("WA_DIAG_WAN_status(): WA_UTILS_IARM_Connect() Failed \n");
        return setReturnData(WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR, params);
    }

#ifdef MEDIA_CLIENT
    int gatewayStatus = gatewayConnection();

    if (gatewayStatus != CONNECTION_SUCCESS)
    {
        if (gatewayStatus == CONNECTION_FAILED)
        {
            WA_ERROR("WA_DIAG_WAN_status(): Local Gateway connection failed\n");
            WA_UTILS_IARM_Disconnect();
            return setReturnData(WA_DIAG_ERRCODE_NO_GATEWAY_CONNECTION, params);
        }
        WA_UTILS_IARM_Disconnect();
        return setReturnData(WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR, params);
    }

    WA_DBG("WA_DIAG_WAN_status(): Local Gateway connection is success\n");
#endif

    int comcastWAN = comcastNetwork();
    int publicWAN = publicNetwork();
    int status;

    if (comcastWAN == CONNECTION_SUCCESS && publicWAN == CONNECTION_SUCCESS)
    {
        WA_DBG("WA_DIAG_WAN_status(): Comcast and public internet connections successful\n");
        status = WA_DIAG_ERRCODE_SUCCESS;
    }

    else if ((comcastWAN == CONNECTION_FAILED) && publicWAN)
    {
        WA_DBG("WA_DIAG_WAN_status(): Comcast connection failed and public internet connection skipped/successful\n");
        status = WA_DIAG_ERRCODE_NO_COMCAST_WAN_CONNECTION;
    }

    else if (comcastWAN && (publicWAN == CONNECTION_FAILED))
    {
        WA_DBG("WA_DIAG_WAN_status(): Comcast connection successful but public internet connection failed\n");
        status = WA_DIAG_ERRCODE_NO_PUBLIC_WAN_CONNECTION;
    }

    else if ((comcastWAN == CONNECTION_FAILED) && (publicWAN == CONNECTION_FAILED))
    {
        WA_DBG("WA_DIAG_WAN_status(): Comcast connection and public internet connections failed\n");
        status = WA_DIAG_ERRCODE_NO_WAN_CONNECTION;
    }

    else if ((comcastWAN == -1 || publicWAN == -1) && (comcastWAN == CONNECTION_SUCCESS || publicWAN == CONNECTION_SUCCESS))
    {
        WA_DBG("WA_DIAG_WAN_status(): One of Comcast and public WAN is success and other has ignorable error\n");
        status = WA_DIAG_ERRCODE_SUCCESS;
    }

    else
    {
        WA_ERROR("WA_DIAG_WAN_status(): Unable to finish WAN status due to dependent errors\n");
        status = WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR;
    }

    if(WA_UTILS_IARM_Disconnect())
    {
        WA_DBG("WA_DIAG_WAN_status(): WA_UTILS_IARM_Disconnect() Failed\n");
    }

    WA_RETURN("WA_DIAG_WAN_status(): return code: %d.\n", status);

    return setReturnData(status, params);
}
