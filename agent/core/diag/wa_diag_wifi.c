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
 * @file wa_diag_wifi.c
 *
 * @brief Diagnostic functions for WIFI - implementation
 */

/** @addtogroup WA_DIAG_WIFI
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
#include "netsrvmgrIarm.h"

/* module interface */
#include "wa_diag_wifi.h"

/*****************************************************************************
 * GLOBAL VARIABLE DEFINITIONS
 *****************************************************************************/

/*****************************************************************************
 * LOCAL DEFINITIONS
 *****************************************************************************/
#define WIFI_OPTION_STR                     "WIFI_SUPPORT"
#define WIFI_AVAILABLE_STR                  "true"

#define DEV_CONFIG_FILE_PATH                "/etc/device.properties"
#define FILE_MODE                           "r"

#define IARM_BUS_WIFI_MGR_API_setEnabled    "setEnabled"
#define TR69_WIFI_ENABLE_STATUS             "Device.WiFi.SSID.1.Enable"
#define TR69_WIFI_OPER_STATUS               "Device.WiFi.SSID.1.Status"
#define TR69_WIFI_SSID                      "Device.WiFi.SSID.1.SSID"
#define TR69_WIFI_MAC_ADDR                  "Device.WiFi.SSID.1.MACAddress"
#define TR69_WIFI_OPER_FREQ                 "Device.WiFi.Radio.1.OperatingFrequencyBand"
#define TR69_WIFI_SIGNAL_STRENGTH           "Device.WiFi.EndPoint.1.Stats.SignalStrength"

/*****************************************************************************
 * LOCAL TYPES
 *****************************************************************************/

/*****************************************************************************
 * LOCAL FUNCTION PROTOTYPES
 *****************************************************************************/
static int disk_supported(void);
static int setReturnData(int status, json_t** param);
static int getWifiParam_IARM(char *param, char **value);
static int getWifiHwStatus_IARM();
static int getWifiOperStatus_IARM(char **result);

/*****************************************************************************
 * LOCAL VARIABLE DECLARATIONS
 *****************************************************************************/
int  wifiParamInt = 0;
bool wifiParamBool = 0;
char *wifiParam;

/*****************************************************************************
 * FUNCTION DEFINITIONS
 *****************************************************************************/

/*****************************************************************************
 * LOCAL FUNCTIONS
 *****************************************************************************/

static int disk_supported(void)
{
    return WA_UTILS_FILEOPS_OptionSupported(DEV_CONFIG_FILE_PATH, FILE_MODE, WIFI_OPTION_STR, WIFI_AVAILABLE_STR);
}

/* 1 - SSID enable status is true
   0 - SSID enable status is false, hence No Connection
  -1 - couldn't get enable status */
static int getWifiHwStatus_IARM()
{
    int ret = -1;
    char *value;
    bool enable;

    ret = getWifiParam_IARM(TR69_WIFI_ENABLE_STATUS, &value);

    enable = *(bool*)value; // Decoding in the same format as how Tr69HostIf encoded the data

    if (ret == 0)
    {
        WA_DBG("getWifiHwStatus_IARM(): WiFi Enable status = %d\n", enable);
        ret = enable ? 1 : 0;
    }

    WA_RETURN("getWifiHwStatus_IARM(): return value - %d\n", ret);
    return ret;
}

int getWifiParam_IARM(char *param, char **value)
{
    IARM_Result_t iarm_result = IARM_RESULT_IPCCORE_FAIL;
    int is_connected = 0;

    WA_ENTER("getWifiParam_IARM()\n");

    iarm_result = IARM_Bus_IsConnected(IARM_BUS_TR69HOSTIFMGR_NAME, &is_connected);
    if (iarm_result != IARM_RESULT_SUCCESS)
    {
        WA_ERROR("getWifiParam_IARM(): IARM_Bus_IsConnected('%s') failed\n", IARM_BUS_TR69HOSTIFMGR_NAME);
        return -1;
    }

    HOSTIF_MsgData_t *stMsgDataParam = NULL;

    iarm_result = IARM_Malloc(IARM_MEMTYPE_PROCESSLOCAL, sizeof(*stMsgDataParam), (void**)&stMsgDataParam);
    if ((iarm_result == IARM_RESULT_SUCCESS) && stMsgDataParam)
    {
        memset(stMsgDataParam, 0, sizeof(*stMsgDataParam));
        snprintf(stMsgDataParam->paramName, TR69HOSTIFMGR_MAX_PARAM_LEN, "%s", param);

        WA_DBG("getWifiParam_IARM(): IARM_Bus_Call('%s', '%s', %s)\n", IARM_BUS_TR69HOSTIFMGR_NAME, IARM_BUS_TR69HOSTIFMGR_API_GetParams, stMsgDataParam->paramName);
        iarm_result = IARM_Bus_Call(IARM_BUS_TR69HOSTIFMGR_NAME, IARM_BUS_TR69HOSTIFMGR_API_GetParams, (void *)stMsgDataParam, sizeof(*stMsgDataParam));

        if (iarm_result != IARM_RESULT_SUCCESS)
        {
            WA_ERROR("getWifiParam_IARM(): IARM_Bus_Call('%s') failed\n", IARM_BUS_TR69HOSTIFMGR_NAME);
            IARM_Free(IARM_MEMTYPE_PROCESSLOCAL, stMsgDataParam);
            return -1;
        }

        if(!strcmp(param, TR69_WIFI_SIGNAL_STRENGTH))
        {
            wifiParam = (char *)&stMsgDataParam->paramValue[0];
            wifiParamInt = *(int *)wifiParam;
            *value = (char *)&wifiParamInt;
        }
        else if(!strcmp(param, TR69_WIFI_ENABLE_STATUS))
        {
            wifiParam = (char *)&stMsgDataParam->paramValue[0];
            wifiParamBool = *(bool *)wifiParam;
            *value = (char *)&wifiParamBool;
        }
        else
        {
            strcpy(wifiIARM, stMsgDataParam->paramValue);
            *value = (char *)wifiIARM;
        }

        IARM_Free(IARM_MEMTYPE_PROCESSLOCAL, stMsgDataParam);
        return 0;
    }

    WA_ERROR("getWifiParam_IARM(): IARM_Malloc('%s') failed\n", IARM_BUS_TR69HOSTIFMGR_NAME);
    return -1;
}

static int setReturnData(int status, json_t **param)
{
    if(param == NULL)
        return status;

    switch (status)
    {
        case WA_DIAG_ERRCODE_SUCCESS:
           *param = json_string("WiFi interface is able to transmit and receive network traffic.");;
           break;

        case WA_DIAG_ERRCODE_FAILURE:
            *param = json_string("Couldn't fetch WiFi values.");
            break;

        case WA_DIAG_ERRCODE_WIFI_NO_CONNECTION:
            *param = json_string("WiFi connection is not established.");
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

/*****************************************************************************
 * EXPORTED FUNCTIONS
 *****************************************************************************/

int WA_DIAG_WIFI_status(void *instanceHandle, void *initHandle, json_t **params)
{
    int ret = -1;
    int hwStatus;
    char *connStatus;

    json_decref(*params); //not used
    *params = NULL;

    ret = disk_supported(); /* Check if the device supports WiFi Interface or not */
    if(ret < 0)
    {
        if(WA_OSA_TaskCheckQuit())
        {
            WA_ERROR("wifi_supported: test cancelled\n");
            return setReturnData(WA_DIAG_ERRCODE_CANCELLED, params);
        }

        WA_ERROR("WA_DIAG_WIFI_status(): Device configuration unknown\n");
        return setReturnData(WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR, params);
    }
    else if(ret == 0)
    {
        WA_ERROR("WA_DIAG_WIFI_status(): Device does not have wifi\n");
        return setReturnData(WA_DIAG_ERRCODE_NOT_APPLICABLE, params);
    }
    WA_INFO("WIFI supported.\n");

    if (WA_UTILS_IARM_Connect())
    {
        WA_ERROR("WA_DIAG_WIFI_status(): WA_UTILS_IARM_Connect() Failed \n");
        return -1;
    }

    hwStatus = getWifiHwStatus_IARM(); /* Check for Hardware status of the WiFi Interface */
    if(hwStatus == -1)
    {
        WA_ERROR("WA_DIAG_WIFI_status(): Unable to get enable value\n");
        ret = WA_UTILS_IARM_Disconnect();
        WA_DBG("WA_DIAG_WIFI_status(): WA_UTILS_IARM_Disconnect() status - %d\n", ret);
        return setReturnData(WA_DIAG_ERRCODE_FAILURE, params);
    }
    if(hwStatus == 0)
    {
        WA_INFO("WA_DIAG_WIFI_status(): Hardware setup is not enabled\n");
        ret = WA_UTILS_IARM_Disconnect();
        WA_DBG("WA_DIAG_WIFI_status(): WA_UTILS_IARM_Disconnect() status - %d\n", ret);
        return setReturnData(WA_DIAG_ERRCODE_WIFI_NO_CONNECTION, params);
    }

    WA_INFO("WiFi interface hardware status is good\n");

    ret = getWifiOperStatus_IARM(&connStatus); /* Check for Connection status of the WiFi Interface */
    if(ret == 0)
    {
        if(!strcmp(connStatus, "UP"))
        {
            WA_INFO("WiFi interface is able to transmit and receive network traffic\n");
            ret = WA_UTILS_IARM_Disconnect();
            WA_DBG("WA_DIAG_WIFI_status(): WA_UTILS_IARM_Disconnect() status - %d\n", ret);
            return setReturnData(WA_DIAG_ERRCODE_SUCCESS, params);
        }
    }

    WA_INFO("WiFi connection is not established\n");
    ret = WA_UTILS_IARM_Disconnect();
    WA_DBG("WA_DIAG_WIFI_status(): WA_UTILS_IARM_Disconnect() status - %d\n", ret);
    return setReturnData(WA_DIAG_ERRCODE_WIFI_NO_CONNECTION, params);

}

static int getWifiOperStatus_IARM(char **result)
{
    int ret = -1;
    ret = getWifiParam_IARM(TR69_WIFI_OPER_STATUS, result);
    return ret;
}

int getWifiSSID_IARM(char **result)
{
    int ret = -1;
    ret = getWifiParam_IARM(TR69_WIFI_SSID, result);
    return ret;
}

int getWifiMacAddress_IARM(char **result)
{
    int ret = -1;
    ret = getWifiParam_IARM(TR69_WIFI_MAC_ADDR, result);
    return ret;
}

int getWifiOperFrequency_IARM(char **result)
{
    int ret = -1;
    ret = getWifiParam_IARM(TR69_WIFI_OPER_FREQ, result);
    return ret;
}

int getWifiSignalStrength_IARM(int *result)
{
    int ret = -1;
    char *data;
    ret = getWifiParam_IARM(TR69_WIFI_SIGNAL_STRENGTH, &data);
    *result = *(int*)data;
    return ret;
}

/* Return -1:skip wifi test 1:execute  */
int isWiFiConfigured()
{
    WA_ENTER("isWiFiConfigured()\n");

    int result = -1;

    if (WA_UTILS_IARM_Connect())
    {
        WA_ERROR("isWiFiConfigured(): WA_UTILS_IARM_Connect() Failed \n");
        return result;
    }

    IARM_Result_t iarm_result = IARM_RESULT_IPCCORE_FAIL;
    IARM_BUS_NetSrvMgr_DefaultRoute_t param;
    iarm_result = IARM_Bus_Call(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETSRVMGR_API_getDefaultInterface, (void*)&param, sizeof(param));

    if (iarm_result != IARM_RESULT_SUCCESS)
    {
        WA_ERROR("isWiFiConfigured(): IARM_Bus_Call('%s') failed\n", IARM_BUS_NM_SRV_MGR_NAME);
        return result;
    }

    char defaultInterface[16];
    strcpy(defaultInterface, param.interface);
    WA_DBG("isWiFiConfigured(): Default interface is \"%s\" \n", defaultInterface);

    if (strstr(defaultInterface, "eth")) /* ANI (Active Network Interface) is Ethernet */
    {
        char *wifiStatus;
        if (getWifiOperStatus_IARM(&wifiStatus) == 0)
        {
            WA_DBG ("isWiFiConfigured(): WiFi Status is \"%s\"\n", wifiStatus);;

            if(!strcmp(wifiStatus, "UP"))
            {
                result = 1;
                WA_DBG("isWiFiConfigured(): Perform WiFi test, as WiFi Status is UP though ANI (Active Network Interface) is Ethernet\n");
            }
            else
                WA_DBG("isWiFiConfigured(): Skip WiFi test, as ANI (Active Network Interface) is ethernet and WiFi Status is %s\n", wifiStatus);
        }
    }
    else
    {
        result = 1;
        WA_DBG("isWiFiConfigured(): Perform WiFi test, as ANI (Active Network Interface) is not ethernet\n");
    }

    WA_UTILS_IARM_Disconnect();
    WA_RETURN("isWiFiConfigured(): Returns result %d\n", result);

    return result;
}

/* End of doxygen group */
/*! @} */

/* EOF */
