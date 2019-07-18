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
 * @brief Diagnostic functions for MoCA - implementation
 */

/** @addtogroup WA_DIAG_MOCA
 *  @{
 */

/*****************************************************************************
 * STANDARD INCLUDE FILES
 *****************************************************************************/

#include <stdbool.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <asm/types.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <ifaddrs.h>
#include <poll.h>
#include <sys/time.h>

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/

#include "wa_diag.h"
#include "wa_debug.h"
#include "wa_fileops.h"
#include "wa_snmp_client.h"

/* rdk specific */
#include "wa_iarm.h"
#include "wa_json.h"
#include "libIBus.h"
#include "libIARMCore.h"
#include "sysMgr.h"
#include "xdiscovery.h"

/* module interface */
#include "wa_diag_moca.h"

/*****************************************************************************
 * GLOBAL VARIABLE DEFINITIONS
 *****************************************************************************/

/*****************************************************************************
 * LOCAL DEFINITIONS
 *****************************************************************************/

#define OID_MOCA11_IF_ENABLE_STATUS "MOCA11-MIB::mocaIfEnable"
#define OID_MOCA11_IF_NODES_COUNT "MOCA11-MIB::mocaIfNumNodes"
#define OID_MOCA11_IF_RX_PACKETS_COUNT "MOCA11-MIB::mocaIfRxPackets"
#define OID_MOCA11_IF_RF_CHANNEL_FREQUENCY "MOCA11-MIB::mocaIfRFChannel"
#define OID_MOCA11_IF_NETWORK_CONTROLLER "MOCA11-MIB::mocaIfNC"
#define OID_MOCA11_IF_TRANSMIT_RATE "MOCA11-MIB::mocaMeshTxRate"
#define OID_MOCA11_NODE_SNR "MOCA11-MIB::mocaNodeSNR"
#define OID_MOCA20_IF_ENABLE_STATUS "MOCA20-MIB::mocaIfEnable"
#define OID_MOCA20_IF_NODES_COUNT "MOCA20-MIB::mocaIfNumNodes"
#define OID_MOCA20_IF_RX_PACKETS_COUNT "MOCA20-MIB::mocaIfRxPackets"
#define OID_MOCA20_IF_RF_CHANNEL_FREQUENCY "MOCA20-MIB::mocaIfRFChannel"
#define OID_MOCA20_IF_NETWORK_CONTROLLER "MOCA20-MIB::mocaIfNC"
#define OID_MOCA20_IF_TRANSMIT_RATE "MOCA20-MIB::mocaMeshTxRate"
#define OID_MOCA20_NODE_SNR "MOCA20-MIB::mocaNodeSNR"

#define MOCA_TRUTH_VALUE_FALSE               (0)
#define MOCA_TRUTH_VALUE_TRUE                (1)

#define MOCA_IF_ENABLE_STATUS_MOCA_DISABLED  (MOCA_TRUTH_VALUE_FALSE)
#define MOCA_IF_ENABLE_STATUS_MOCA_ENABLED   (MOCA_TRUTH_VALUE_TRUE)

#define MOCA_IF_NODES_COUNT_MIN              (2) /* 0 - no clients,
                                                    2 - 1 client */

#define DEV_CONFIG_FILE_PATH         "/etc/device.properties"
#define FILE_MODE                    "r"

#define MOCA_OPTION_STR              "MOCA_INTERFACE"
#define MOCA_AVAILABLE_STR           "mca0"

#define SNMP_SERVER "localhost"

#define STEP_TIME 5 /* in [s] */
#define NODESCOUNT_STEPS 9
#define PACKETSCOUNT_STEPS 9
#define TOTAL_STEPS (NODESCOUNT_STEPS + PACKETSCOUNT_STEPS)

#define MOCA_GROUP_11 0
#define MOCA_GROUP_20 (MOCA20_OPT_IF_ENABLE_STATUS)
/*****************************************************************************
 * LOCAL TYPES
 *****************************************************************************/
typedef enum {
    MOCA11_OPT_IF_ENABLE_STATUS,
    MOCA11_OPT_IF_NODES_COUNT,
    MOCA11_OPT_IF_RX_PACKETS_COUNT,
    MOCA11_OPT_IF_RF_CHANNEL_FREQUENCY,
    MOCA11_OPT_IF_NETWORK_CONTROLLER,
    MOCA11_OPT_IF_TRANSMIT_RATE,
    MOCA11_OPT_NODE_SNR,
    MOCA20_OPT_IF_ENABLE_STATUS,
    MOCA20_OPT_IF_NODES_COUNT,
    MOCA20_OPT_IF_RX_PACKETS_COUNT,
    MOCA20_OPT_IF_RF_CHANNEL_FREQUENCY,
    MOCA20_OPT_IF_NETWORK_CONTROLLER,
    MOCA20_OPT_IF_TRANSMIT_RATE,
    MOCA20_OPT_NODE_SNR,
    MOCA_OPT_MAX,
} MocaOption_t;

/*****************************************************************************
 * LOCAL FUNCTION PROTOTYPES
 *****************************************************************************/
static int moca_supported(const char *interface);
static int getMocaOptionStatus(MocaOption_t opt, int index, WA_UTILS_SNMP_Resp_t *value,
    WA_UTILS_SNMP_ReqType_t reqType);
static int getMocaIfEnableStatus(int group, int index, WA_UTILS_SNMP_Resp_t *value,
    WA_UTILS_SNMP_ReqType_t reqType);
static int getMocaIfNodesCount(void* instanceHandle, int group, int index,
    WA_UTILS_SNMP_Resp_t *value, WA_UTILS_SNMP_ReqType_t reqType, int progress);
static int getMocaIfRxPacketsCount(void* instanceHandle, int group, int index,
    WA_UTILS_SNMP_Resp_t *value, WA_UTILS_SNMP_ReqType_t reqType, int progress);
static int verifyOptionValue(MocaOption_t opt,  WA_UTILS_SNMP_Resp_t *value);
static int setReturnData(int status, json_t **param);
int getMocaIfRFChannelFrequency(int *group, int index, WA_UTILS_SNMP_Resp_t *value, WA_UTILS_SNMP_ReqType_t reqType);
int getMocaIfNetworkController(int group, int index, WA_UTILS_SNMP_Resp_t *value, WA_UTILS_SNMP_ReqType_t reqType);
int getMocaIfTransmitRate(int group, int index, WA_UTILS_SNMP_Resp_t *value, WA_UTILS_SNMP_ReqType_t reqType);
int getMocaNodeSNR(int group, int index, WA_UTILS_SNMP_Resp_t *value, WA_UTILS_SNMP_ReqType_t reqType);

/*****************************************************************************
 * LOCAL VARIABLE DECLARATIONS
 *****************************************************************************/
char *MocaOptions[MOCA_OPT_MAX] = {
    OID_MOCA11_IF_ENABLE_STATUS,
    OID_MOCA11_IF_NODES_COUNT,
    OID_MOCA11_IF_RX_PACKETS_COUNT,
    OID_MOCA11_IF_RF_CHANNEL_FREQUENCY,
    OID_MOCA11_IF_NETWORK_CONTROLLER,
    OID_MOCA11_IF_TRANSMIT_RATE,
    OID_MOCA11_NODE_SNR,
    OID_MOCA20_IF_ENABLE_STATUS,
    OID_MOCA20_IF_NODES_COUNT,
    OID_MOCA20_IF_RX_PACKETS_COUNT,
    OID_MOCA20_IF_RF_CHANNEL_FREQUENCY,
    OID_MOCA20_IF_NETWORK_CONTROLLER,
    OID_MOCA20_IF_TRANSMIT_RATE,
    OID_MOCA20_NODE_SNR
};

/*****************************************************************************
 * FUNCTION DEFINITIONS
 *****************************************************************************/

/*****************************************************************************
 * LOCAL FUNCTIONS
 *****************************************************************************/
/* -1 error
    0 not supported
    1 supported */
static int moca_supported(const char *interface)
{
    return WA_UTILS_FILEOPS_OptionSupported(DEV_CONFIG_FILE_PATH, FILE_MODE, MOCA_OPTION_STR, interface);
}

static int getMocaOptionStatus(MocaOption_t opt, int index, WA_UTILS_SNMP_Resp_t *value,
    WA_UTILS_SNMP_ReqType_t reqType)
{
    int ret = -1;
    char oid[256];

    WA_UTILS_SNMP_Resp_t val;
    val.type = value->type;

    WA_ENTER("getMocaOptionStatus (opt: %s, index: %d, reqType: %d).\n", MocaOptions[opt], index, reqType);

    if(index == -1)
        ret = snprintf(oid, sizeof(oid), "%s", MocaOptions[opt]);
    else
        ret = snprintf(oid, sizeof(oid), "%s.%d", MocaOptions[opt], index);

    WA_DBG("getMocaOptionStatus(): oid: %s, snprintf ret: %d\n", oid, ret);

    if ((ret >= sizeof(oid)) || (ret < 0))
    {
        WA_ERROR("getMocaOptionStatus, unable to generate OID.\n");
        return -1;
    }

    WA_DBG("getMocaOptionStatus, oid generated: %s, oid len %d\n", oid, strlen(oid));

    if(!WA_UTILS_SNMP_GetNumber(SNMP_SERVER, oid, &val, reqType))
    {
        if(reqType == WA_UTILS_SNMP_REQ_TYPE_GET)
        {
            /* Try with walk request if get did not succeed */
            if(!WA_UTILS_SNMP_GetNumber(SNMP_SERVER, oid, &val, WA_UTILS_SNMP_REQ_TYPE_WALK))
            {
                WA_ERROR("getMocaOptionStatus, option: %s, failed for both get and walk query.\n", MocaOptions[opt]);
                return -1;
            }

            WA_INFO("getMocaOptionStatus, option: %s, failed for get but succeeded for walk query.\n", MocaOptions[opt]);

        }
        else
        {
            WA_ERROR("getMocaOptionStatus, option: %s, failed.\n", MocaOptions[opt]);
            return -1;
        }
    }

    switch(value->type)
    {
        case WA_UTILS_SNMP_RESP_TYPE_LONG:
            value->data.l = val.data.l;
            WA_DBG("getMocaOptionStatus, option: %s, state: %ld (%p).\n",
                MocaOptions[opt], value->data.l, &value->data.l);
            break;

        case WA_UTILS_SNMP_RESP_TYPE_COUNTER64:
            value->data.c64.high = val.data.c64.high;
            value->data.c64.low = val.data.c64.low;
            WA_DBG("getMocaOptionStatus, option: %s, state.low %ld state.high %ld (%p).\n",
                MocaOptions[opt], value->data.c64.high, value->data.c64.low, &value->data.c64);
            break;

        default:
            return -1;
    }

    return verifyOptionValue(opt, value);
}

static bool getMocaIfIndex(int group, int *index)
{
    int idx, ret;
    char oid[256];
    MocaOption_t opt = MOCA11_OPT_IF_ENABLE_STATUS + group;

    WA_ENTER("getMocaIfIndex\n");

    ret = snprintf(oid, sizeof(oid), "%s", MocaOptions[opt]);
    if ((ret >= sizeof(oid)) || (ret < 0))
    {
        WA_ERROR("getMocaIfIndex, unable to generate OID.\n");
        return false;
    }

    WA_DBG("getMocaIfIndex, generated OID: %s.\n", oid);

    if(!WA_UTILS_SNMP_FindIfIndex(SNMP_SERVER, oid, &idx))
    {
         WA_ERROR("getMocaIfIndex, index not found (oid: %s).\n", oid);
         return false;
    }

    WA_RETURN("getMocaIfIndex, option %s, index: %d.\n", MocaOptions[opt], idx);
    if(idx > 0)
    {
        WA_DBG("getMocaIfIndex, index grater than 0 (%d)\n", idx);
        *index = idx;
        return true;
    }
    WA_DBG("getMocaIfIndex, index not applicable (%d)\n", idx);
    return false;
}


static int getMocaIfEnableStatus(int group, int index, WA_UTILS_SNMP_Resp_t *value,
    WA_UTILS_SNMP_ReqType_t reqType)
{
    return getMocaOptionStatus(MOCA11_OPT_IF_ENABLE_STATUS + group, index, value, reqType);
}

static int getMocaIfNodesCount(void* instanceHandle, int group, int index,  WA_UTILS_SNMP_Resp_t *value,
    WA_UTILS_SNMP_ReqType_t reqType, int progress)
{
    int status;
    int step = 0;

    while(1)
    {
        ++step;

        status = getMocaOptionStatus(MOCA11_OPT_IF_NODES_COUNT + group, index, value, reqType);

        if(WA_OSA_TaskCheckQuit())
        {
            WA_DBG("getMocaIfNodesCount(): cancelled\n");
            status = -1;
            break;
        }

        if((status != 0) || (step > NODESCOUNT_STEPS))
        {
            break;
        }
        WA_DIAG_SendProgress(instanceHandle, ((progress + step) * 100)/(TOTAL_STEPS+1));

        sleep(STEP_TIME);

    }

    return status;
}

static int getMocaIfRxPacketsCount(void* instanceHandle, int group, int index, WA_UTILS_SNMP_Resp_t *value,
    WA_UTILS_SNMP_ReqType_t reqType, int progress)
{
    int status;
    int step = 0;

    while(1)
    {
        ++step;

        status = getMocaOptionStatus(MOCA11_OPT_IF_RX_PACKETS_COUNT + group, index, value, reqType);

        if(WA_OSA_TaskCheckQuit())
        {
            WA_DBG("getMocaIfRxPacketsCount(): cancelled\n");
            status = -1;
            break;
        }

        if((status != 0) || (step > PACKETSCOUNT_STEPS))
        {
            break;
        }
        WA_DIAG_SendProgress(instanceHandle, ((progress + step) * 100)/(TOTAL_STEPS+1));

        sleep(STEP_TIME);

    }

    return status;
}

int getMocaIfRFChannelFrequency(int *group, int index, WA_UTILS_SNMP_Resp_t *value, WA_UTILS_SNMP_ReqType_t reqType)
{
    int ret = -1;

    ret = getMocaOptionStatus(MOCA11_OPT_IF_RF_CHANNEL_FREQUENCY + (*group), index, value, reqType);

    if (ret < 0)
    {
        *group = MOCA_GROUP_20;
        ret = getMocaOptionStatus(MOCA11_OPT_IF_RF_CHANNEL_FREQUENCY + (*group), index, value, reqType);
    }

    return ret;
}

int getMocaIfNetworkController(int group, int index, WA_UTILS_SNMP_Resp_t *value, WA_UTILS_SNMP_ReqType_t reqType)
{
    return getMocaOptionStatus(MOCA11_OPT_IF_NETWORK_CONTROLLER + group, index, value, reqType);
}

int getMocaIfTransmitRate(int group, int index, WA_UTILS_SNMP_Resp_t *value, WA_UTILS_SNMP_ReqType_t reqType)
{
    return getMocaOptionStatus(MOCA11_OPT_IF_TRANSMIT_RATE + group, index, value, reqType);
}

int getMocaNodeSNR(int group, int index, WA_UTILS_SNMP_Resp_t *value, WA_UTILS_SNMP_ReqType_t reqType)
{
    return getMocaOptionStatus(MOCA11_OPT_NODE_SNR + group, index, value, reqType);
}

static int verifyOptionValue(MocaOption_t opt, WA_UTILS_SNMP_Resp_t *value)
{
    int ret = -1;

    WA_ENTER("verifyOptionValue (%s).\n", MocaOptions[opt]);

    if(value == NULL)
    {
       WA_ERROR("verifyOptionValue, null pointer error\n");
       return ret;
    }

    switch(opt)
    {
        case MOCA11_OPT_IF_ENABLE_STATUS:
        case MOCA20_OPT_IF_ENABLE_STATUS:
            switch (value->data.l)
            {
                case MOCA_IF_ENABLE_STATUS_MOCA_DISABLED:
                    ret = 0;
                    break;

                case MOCA_IF_ENABLE_STATUS_MOCA_ENABLED:
                    ret = 1;
                    break;

                default:
                    /* MoCA HW not accessible */
                    ret = -2;
                    break;
            }
            break;

        case MOCA11_OPT_IF_NODES_COUNT:
        case MOCA20_OPT_IF_NODES_COUNT:
            ret = (value->data.l >= MOCA_IF_NODES_COUNT_MIN ? 1 : 0);
            break;

        case MOCA11_OPT_IF_RX_PACKETS_COUNT:
        case MOCA20_OPT_IF_RX_PACKETS_COUNT:
            ret = ((value->data.c64.high > 0 || value->data.c64.low > 0) ? 1 : 0);
            break;

        case MOCA11_OPT_IF_RF_CHANNEL_FREQUENCY:
        case MOCA20_OPT_IF_RF_CHANNEL_FREQUENCY:
            ret = (value->data.l ? 1 : 0);
            break;
        case MOCA11_OPT_IF_NETWORK_CONTROLLER:
        case MOCA20_OPT_IF_NETWORK_CONTROLLER:
            ret = (value->data.l ? 1 : 0);
            break;

        case MOCA11_OPT_IF_TRANSMIT_RATE:
        case MOCA20_OPT_IF_TRANSMIT_RATE:
            ret = (value->data.l ? 1 : 0);
            break;

        case MOCA11_OPT_NODE_SNR:
        case MOCA20_OPT_NODE_SNR:
            ret = (value->data.l ? 1 : 0);
            break;

        default:
            break;
    }

    WA_RETURN("verifyOptionValue, option: %s, return code: %d.\n", MocaOptions[opt], ret);

    return ret;
}

static int setReturnData(int status, json_t **param)
{
    if(param == NULL)
    {
        WA_ERROR("setReturnData, null pointer error.\n");
        return status;
    }

    switch (status)
    {
        case WA_DIAG_ERRCODE_SUCCESS:
           *param = json_string("MoCA good.");
           break;

        case WA_DIAG_ERRCODE_FAILURE:
           *param = json_string("MoCA bad.");
           break;

        case WA_DIAG_ERRCODE_MOCA_DISABLED:
           *param = json_string("MoCA disabled.");
           break;

        case WA_DIAG_ERRCODE_MOCA_NO_CLIENTS:
           *param = json_string("MoCA no clients found.");
           break;

        case WA_DIAG_ERRCODE_NOT_APPLICABLE:
            *param = json_string("Not applicable.");
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

int WA_DIAG_MOCA_status(void* instanceHandle, void *initHandle, json_t **params)
{
    int ret;
    int ifIndex = -1;
    WA_UTILS_SNMP_Resp_t value;
    json_t * jsonConfig = NULL;
    const char *interface = MOCA_AVAILABLE_STR;
    int group = MOCA_GROUP_11;

    json_decref(*params); // not used
    *params = NULL;

    WA_ENTER("moca_status\n");

    jsonConfig = ((WA_DIAG_proceduresConfig_t*)initHandle)->config;

    /* Use interface name from config if present */
    if (jsonConfig)
        json_unpack(jsonConfig, "{ss}", "interface", &interface);

    ret = moca_supported(interface);
    if(ret < 0)
    {
        if(WA_OSA_TaskCheckQuit())
        {
            WA_DBG("moca_supported: test cancelled\n");
            return setReturnData(WA_DIAG_ERRCODE_CANCELLED, params);
        }

        WA_ERROR("Device configuration unknown.\n");
        return setReturnData(WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR, params);
    }
    else if(ret == 0)
    {
        WA_ERROR("Device does not have MoCA interface.\n");
        return setReturnData(WA_DIAG_ERRCODE_NOT_APPLICABLE, params);
    }
    WA_DBG("MoCA supported.\n");

    value.type = WA_UTILS_SNMP_RESP_TYPE_LONG;
    ret = getMocaIfEnableStatus(group, ifIndex, &value, WA_UTILS_SNMP_REQ_TYPE_WALK);
    if(ret < 0)
    {
        group = MOCA_GROUP_20;
        ret = getMocaIfEnableStatus(group, ifIndex, &value, WA_UTILS_SNMP_REQ_TYPE_WALK);
    }
    if(ret < 1)
    {

        if(WA_OSA_TaskCheckQuit())
        {
            WA_DBG("getMocaIfEnableStatus: test cancelled\n");
            return setReturnData(WA_DIAG_ERRCODE_CANCELLED, params);
        }

        WA_ERROR("moca_status, getMocaIfEnableStatus, error code: %d\n", ret);

        switch(ret)
        {
            case 0:
                return setReturnData(WA_DIAG_ERRCODE_MOCA_DISABLED, params);
            case -1:
                return setReturnData(WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR, params);
            case -2:
            default:
                return setReturnData(WA_DIAG_ERRCODE_FAILURE, params);
        }
    }

    WA_DBG("MoCA status read successfully.\n");

    (void)getMocaIfIndex(group, &ifIndex);
    WA_DBG("MoCA interface index: %d.\n", ifIndex);

    value.type = WA_UTILS_SNMP_RESP_TYPE_LONG;
    ret = getMocaIfNodesCount(instanceHandle, group, ifIndex, &value, WA_UTILS_SNMP_REQ_TYPE_GET, 0);
    if(ret < 1)
    {
        if(WA_OSA_TaskCheckQuit())
        {
            WA_DBG("getMocaIfNodesCount: test cancelled\n");
            return setReturnData(WA_DIAG_ERRCODE_CANCELLED, params);
        }

        WA_ERROR("moca_status, getMocaIfNodesCount, error code: %d\n", ret);
        return setReturnData((ret == 0 ? WA_DIAG_ERRCODE_MOCA_NO_CLIENTS : WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR), params);
    }

    WA_DBG("MoCA nodes count read successfully.\n");

    // If nodes count grater than 1, still need to check if any packets received
    if(group == MOCA_GROUP_20)
        value.type = WA_UTILS_SNMP_RESP_TYPE_COUNTER64;
    else
        value.type = WA_UTILS_SNMP_RESP_TYPE_LONG;

    ret = getMocaIfRxPacketsCount(instanceHandle, group, ifIndex, &value,
        WA_UTILS_SNMP_REQ_TYPE_GET, NODESCOUNT_STEPS);
    if(ret < 1)
    {
        if(WA_OSA_TaskCheckQuit())
        {
            WA_DBG("getMocaIfRxPacketsCount: test cancelled\n");
            return setReturnData(WA_DIAG_ERRCODE_CANCELLED, params);
        }

        WA_ERROR("moca_status, getMocaIfRxPacketsCount, error_code: %d\n", ret);
        return setReturnData((ret == 0 ? WA_DIAG_ERRCODE_MOCA_NO_CLIENTS : WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR), params);
    }

    WA_DBG("MoCA clients found.\n");

    WA_RETURN("moca_status, return code: %d.\n", WA_DIAG_ERRCODE_SUCCESS);

    return setReturnData(WA_DIAG_ERRCODE_SUCCESS, params);
}

bool getUpnpResults()
{
    IARM_Result_t iarm_result = IARM_RESULT_IPCCORE_FAIL;
    int is_connected = 0;

    IARM_Bus_SYSMGR_GetXUPNPDeviceInfo_Param_t *param = NULL;
    char upnpResults[MESSAGE_LENGTH+1];
    json_error_t jerror;
    json_t *json = NULL;
    char tmp[BUFFER_LENGTH] = {'\0'};

    iarm_result = IARM_Bus_IsConnected(_IARM_XUPNP_NAME, &is_connected);
    if (iarm_result != IARM_RESULT_SUCCESS)
    {
        WA_ERROR("getUpnpResults(): IARM_Bus_IsConnected('%s') failed\n", _IARM_XUPNP_NAME);
        return false;
    }

    if (!is_connected)
    {
        WA_ERROR("getUpnpResults(): XUPNP not available\n");
        return false;
    }

    iarm_result = IARM_Malloc(IARM_MEMTYPE_PROCESSLOCAL, sizeof(IARM_Bus_SYSMGR_GetXUPNPDeviceInfo_Param_t) + MESSAGE_LENGTH + 1, (void**)&param);
    if (iarm_result != IARM_RESULT_SUCCESS)
    {
        WA_ERROR("getUpnpResults(): IARM_Malloc() failed\n");
        return false;
    }

    /* Beyond this whenever we return due to failure/success, IAM_FREE must be called */
    if (param)
    {
        memset(param, 0, sizeof(*param));
        param->bufLength = MESSAGE_LENGTH;

        iarm_result = IARM_Bus_Call(_IARM_XUPNP_NAME, IARM_BUS_XUPNP_API_GetXUPNPDeviceInfo, (void *)param, sizeof(IARM_Bus_SYSMGR_GetXUPNPDeviceInfo_Param_t) + MESSAGE_LENGTH + 1);

        if (iarm_result != IARM_RESULT_SUCCESS)
        {
            WA_ERROR("getUpnpResults(): IARM_Bus_Call() returned %i\n", iarm_result);
            IARM_Free(IARM_MEMTYPE_PROCESSLOCAL, param);
            return false;
        }

        WA_DBG("getUpnpResults(): IARM_RESULT_SUCCESS\n");
        memcpy(upnpResults, ((char *)param + sizeof(IARM_Bus_SYSMGR_GetXUPNPDeviceInfo_Param_t)), param->bufLength);
        upnpResults[param->bufLength] = '\0';

        json = json_loadb(upnpResults, MESSAGE_LENGTH, JSON_DISABLE_EOF_CHECK, &jerror);
        if (!json)
        {
            WA_ERROR("getUpnpResults(): json_loads() error\n");
            json_decref(json);
            IARM_Free(IARM_MEMTYPE_PROCESSLOCAL, param);
            return false;
        }

        json_t *jarr = json_object_get(json, "xmediagateways");
        if (!jarr)
        {
            WA_ERROR("getUpnpResults(): Couldn't get the array of xmediagateways\n");
            json_decref(jarr);
            json_decref(json);
            IARM_Free(IARM_MEMTYPE_PROCESSLOCAL, param);
            return false;
        }

        for (size_t i = 0; i < json_array_size(jarr); i++)
        {
            json_t *jval = json_array_get(jarr, i);
            if (!jval)
            {
                WA_ERROR("getUpnpResults(): Not a json object in array for %i element\n", i);
                json_decref(jval);
                json_decref(jarr);
                json_decref(json);
                IARM_Free(IARM_MEMTYPE_PROCESSLOCAL, param);
                return false;
            }

            char param_Mac[BUFFER_LENGTH];
            char param_devName[BUFFER_LENGTH];
            json_t *jparam_devName = json_object_get(jval, "deviceName");

            /* If deviceName is not empty*/
            if (strcmp(json_string_value(jparam_devName), ""))
            {
                strxfrm(param_devName, json_string_value(jparam_devName), DEVNAME_LENGTH); /* Get first 10 characters */
                param_devName[DEVNAME_LENGTH] = '\0';
                snprintf(mocaNodeInfo, BUFFER_LENGTH, "%s%s", tmp, param_devName);
            }
            else
            {
                json_t *jparam_devType = json_object_get(jval, "DevType");
                json_t *jparam_gateway = json_object_get(jval, "isgateway");

                /* If DevType is a gateway*/
                if (!strcmp(json_string_value(jparam_gateway), "yes"))
                {
                    json_t *jparam_Mac = json_object_get(jval, "hostMacAddress");
                    strcpy(param_Mac, json_string_value(jparam_Mac));
                    json_decref(jparam_Mac);
                }
                else
                {
                    json_t *jparam_Mac = json_object_get(jval, "bcastMacAddress");
                    strcpy(param_Mac, json_string_value(jparam_Mac));
                    json_decref(jparam_Mac);
                }

                /* Take only last 4 characters of Macaddress */
                char* token = strtok(param_Mac, ":");
                for (int i = 0; i < 4; i++)
                {
                    token = strtok(NULL, ":");
                }
                char *octet = token;
                token = strtok(NULL, ":");

                snprintf(mocaNodeInfo, BUFFER_LENGTH, "%s%s(%s%s)", tmp, json_string_value(jparam_devType), octet, token);
                json_decref(jparam_devType);
                json_decref(jparam_gateway);
            }

            strcpy(tmp, mocaNodeInfo);
            strcat(tmp, ", ");
            json_decref(jparam_devName);
        }
    }

    IARM_Free(IARM_MEMTYPE_PROCESSLOCAL, param);

    return true;
}

/* End of doxygen group */
/*! @} */

