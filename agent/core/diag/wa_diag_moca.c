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
#include <math.h>

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

/* module interface */
#include "wa_diag_moca.h"

/*****************************************************************************
 * GLOBAL VARIABLE DEFINITIONS
 *****************************************************************************/

/*****************************************************************************
 * LOCAL DEFINITIONS
 *****************************************************************************/
#ifndef MEDIA_CLIENT
/* XG */
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
#define SNMP_SERVER "localhost"

#define STEP_TIME 5 /* in [s] */
#define NODESCOUNT_STEPS 9
#define PACKETSCOUNT_STEPS 9
#define TOTAL_STEPS (NODESCOUNT_STEPS + PACKETSCOUNT_STEPS)

#define MOCA_GROUP_11 0
#define MOCA_GROUP_20 (MOCA20_OPT_IF_ENABLE_STATUS)
#else
/* Xi */
#define TR69_MOCA_IF_ENABLE_STATUS "Device.MoCA.Interface.1.Enable"
#define TR69_MOCA_IF_RF_CHANNEL_FREQUENCY "Device.MoCA.Interface.1.CurrentOperFreq"
#define TR69_MOCA_IF_NETWORK_CONTROLLER "Device.MoCA.Interface.1.NetworkCoordinator"
#define TR69_MOCA_IF_MESH_TABLE_ENTRY "Device.MoCA.Interface.1.X_RDKCENTRAL-COM_MeshTableNumberOfEntries"
#define TR69_MOCA_IF_MESH_TABLE "Device.MoCA.Interface.1.X_RDKCENTRAL-COM_MeshTable."
#define TR69_MOCA_IF_TRANSMIT_RATE ".MeshPHYTxRate"
#define TR69_MOCA_IF_ASSOCIATED_DEVICE_ENTRY "Device.MoCA.Interface.1.AssociatedDeviceNumberOfEntries"
#define TR69_MOCA_IF_ASSOCIATED_DEVICE "Device.MoCA.Interface.1.AssociatedDevice."
#define TR69_MOCA_IF_RX_PACKETS_COUNT ".RxPackets"
#define TR69_MOCA_IF_NODE_SNR ".RxSNR"
#define HWST_RMH_MAX_MOCA_NODES 16 // Copied the macro as in /moca-hal/rmh_interface/rmh_type.h
#endif /* MEDIA_CLIENT */

/* XG and Xi */
#define DEV_CONFIG_FILE_PATH         "/etc/device.properties"
#define FILE_MODE                    "r"

#define MOCA_OPTION_STR              "MOCA_INTERFACE"
#define MOCA_AVAILABLE_STR           "mca0"

/*****************************************************************************
 * LOCAL TYPES
 *****************************************************************************/
#ifndef MEDIA_CLIENT
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
#endif /* MEDIA_CLIENT */

/*****************************************************************************
 * LOCAL FUNCTION PROTOTYPES
 *****************************************************************************/
static int moca_supported(const char *interface);
static int setReturnData(int status, json_t **param);

#ifndef MEDIA_CLIENT
static int getMocaOptionStatus(MocaOption_t opt, int index, WA_UTILS_SNMP_Resp_t *value,
    WA_UTILS_SNMP_ReqType_t reqType);
static int getMocaIfEnableStatus(int group, int index, WA_UTILS_SNMP_Resp_t *value,
    WA_UTILS_SNMP_ReqType_t reqType);
static int getMocaIfNodesCount(void* instanceHandle, int group, int index,
    WA_UTILS_SNMP_Resp_t *value, WA_UTILS_SNMP_ReqType_t reqType, int progress);
static int getMocaIfRxPacketsCount(void* instanceHandle, int group, int index,
    WA_UTILS_SNMP_Resp_t *value, WA_UTILS_SNMP_ReqType_t reqType, int progress);
static int verifyOptionValue(MocaOption_t opt,  WA_UTILS_SNMP_Resp_t *value);
int getMocaIfRFChannelFrequency(int *group, int index, WA_UTILS_SNMP_Resp_t *value, WA_UTILS_SNMP_ReqType_t reqType);
int getMocaIfNetworkController(int group, int index, WA_UTILS_SNMP_Resp_t *value, WA_UTILS_SNMP_ReqType_t reqType);
int getMocaIfTransmitRate(int group, int index, WA_UTILS_SNMP_Resp_t *value, WA_UTILS_SNMP_ReqType_t reqType);
int getMocaNodeSNR(int group, int index, WA_UTILS_SNMP_Resp_t *value, WA_UTILS_SNMP_ReqType_t reqType);
#else
static int getMocaIfMeshTable_IARM();
static int getMocaIfAssociatedDevice_IARM();
static int getMocaIfEnableStatus_IARM();
static int getMocaIfRxPacketsCount_IARM();
static int getMocaParam_IARM(char *param, char **value);
#endif /* MEDIA_CLIENT */

/*****************************************************************************
 * LOCAL VARIABLE DECLARATIONS
 *****************************************************************************/
#ifndef MEDIA_CLIENT
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
#endif /* MEDIA_CLIENT */

int  mocaParamInt = 0;
bool mocaParamBool = 0;
char *mocaParam;
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

#ifndef MEDIA_CLIENT
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
           if (opt <= MOCA11_OPT_NODE_SNR)
               WA_DBG("getMocaOptionStatus, option: %s, failed.\n", MocaOptions[opt]);
           else
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
            if (opt <= MOCA11_OPT_NODE_SNR)
                WA_DBG("getMocaOptionStatus, Received invalid value type \"%d\" for %s from snmp call\n", value->type, oid);
            else
                WA_ERROR("getMocaOptionStatus, Received invalid value type \"%d\" for %s from snmp call\n", value->type, oid);

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
                    if (opt == MOCA11_OPT_IF_ENABLE_STATUS)
                        WA_DBG("verifyOptionValue, Received invalid data \"%ld\" for MoCA enable status from snmp call\n", value->data.l);
                    else
                        WA_ERROR("verifyOptionValue, Received invalid data \"%ld\" for MoCA enable status from snmp call\n", value->data.l);

                    ret = -1;
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

#else
static int getMocaIfMeshTable_IARM()
{
    int num_entries = 0;
    // validating same as tr69hostif - MoCAInterface::get_MoCA_Mesh_NumberOfEntries
    int max_entries = (int)((int)pow(HWST_RMH_MAX_MOCA_NODES, 2) - (HWST_RMH_MAX_MOCA_NODES));
    char *value;

    if (getMocaParam_IARM(TR69_MOCA_IF_MESH_TABLE_ENTRY, &value) < 0)
    {
        WA_ERROR("getMocaIfMeshTable_IARM(): getMocaParam_IARM('%s') failed\n", TR69_MOCA_IF_MESH_TABLE_ENTRY);
        return -1;
    }

    num_entries = *(int*)value; // Decoding in the same format as how Tr69HostIf encoded the data

    if (num_entries > max_entries) // Sometimes we get junk values which is a huge number when MoCA cable is disconnected
    {
        WA_ERROR("getMocaIfMeshTable_IARM(): Received incorrect value %i for %s\n", num_entries, TR69_MOCA_IF_MESH_TABLE_ENTRY);
        num_entries = 0;
    }

    return num_entries;
}

static int getMocaIfAssociatedDevice_IARM()
{
    int num_devices = 0;
    char *value;

    if (getMocaParam_IARM(TR69_MOCA_IF_ASSOCIATED_DEVICE_ENTRY, &value) < 0)
    {
        WA_ERROR("getMocaIfAssociatedDevice_IARM(): getMocaParam_IARM('%s') failed\n", TR69_MOCA_IF_ASSOCIATED_DEVICE_ENTRY);
        return -1;
    }

    num_devices = *(int*)value; // Decoding in the same format as how Tr69HostIf encoded the data

    if (num_devices > HWST_RMH_MAX_MOCA_NODES) // Sometimes we get junk values which is a huge number when  MoCA cable is disconnected
    {
        WA_ERROR("getMocaIfAssociatedDevice_IARM(): Received incorrect value %i for %s\n", num_devices, TR69_MOCA_IF_ASSOCIATED_DEVICE_ENTRY);
        num_devices = 0;
    }

    return num_devices;
}

static int getMocaIfEnableStatus_IARM()
{
    int ret = -1;
    char *value;
    bool enable;

    ret = getMocaParam_IARM(TR69_MOCA_IF_ENABLE_STATUS, &value);

    enable = *(bool*)value; // Decoding in the same format as how Tr69HostIf encoded the data

    if (ret == 0)
    {
        ret = enable ? 1 : 0;
    }

    return ret;
}

static int getMocaIfRxPacketsCount_IARM()
{
    int ret = -1;
    int num_devices = 0;
    char *value;
    char data[256];

    if ((num_devices = getMocaIfAssociatedDevice_IARM()) == 0)
    {
        return num_devices;
    }

    for (int index = 1; index <= num_devices; index++)
    {
        snprintf(data, sizeof(data), "%s%i%s", TR69_MOCA_IF_ASSOCIATED_DEVICE, index, TR69_MOCA_IF_RX_PACKETS_COUNT);
        if (getMocaParam_IARM(data, &value) < 0)
        {
            WA_ERROR("getMocaIfRxPacketsCount_IARM(): getMocaParam_IARM('%s') failed\n", data);
            continue;
        }

        ret = 1;
    }

    return ret;
}

static int getMocaParam_IARM(char *param, char **value)
{
    IARM_Result_t iarm_result = IARM_RESULT_IPCCORE_FAIL;
    int is_connected = 0;

    WA_ENTER("getMocaParam_IARM()\n");

    iarm_result = IARM_Bus_IsConnected(IARM_BUS_TR69HOSTIFMGR_NAME, &is_connected);
    if (iarm_result != IARM_RESULT_SUCCESS)
    {
        WA_ERROR("getMocaParam_IARM(): IARM_Bus_IsConnected('%s') failed\n", IARM_BUS_TR69HOSTIFMGR_NAME);
        return -1;
    }

    HOSTIF_MsgData_t *stMsgDataParam = NULL;

    iarm_result = IARM_Malloc(IARM_MEMTYPE_PROCESSLOCAL, sizeof(*stMsgDataParam), (void**)&stMsgDataParam);
    if ((iarm_result == IARM_RESULT_SUCCESS) && stMsgDataParam)
    {
        memset(stMsgDataParam, 0, sizeof(*stMsgDataParam));
        snprintf(stMsgDataParam->paramName, TR69HOSTIFMGR_MAX_PARAM_LEN, "%s", param);

        iarm_result = IARM_Bus_Call(IARM_BUS_TR69HOSTIFMGR_NAME, IARM_BUS_TR69HOSTIFMGR_API_GetParams, (void *)stMsgDataParam, sizeof(*stMsgDataParam));

        if (iarm_result != IARM_RESULT_SUCCESS)
        {
            WA_ERROR("getMocaParam_IARM(): IARM_Bus_Call('%s') failed\n", IARM_BUS_TR69HOSTIFMGR_NAME);
            IARM_Free(IARM_MEMTYPE_PROCESSLOCAL, stMsgDataParam);
            return -1;
        }

        if(!strcmp(param, TR69_MOCA_IF_ENABLE_STATUS))
        {
            mocaParam = (char *)&stMsgDataParam->paramValue[0];
            mocaParamBool = *(bool *)mocaParam;
            *value = (char *)&mocaParamBool;
        }
        else
        {
            mocaParam = (char *)&stMsgDataParam->paramValue[0];
            mocaParamInt = *(int *)mocaParam;
            *value = (char *)&mocaParamInt;
        }

        IARM_Free(IARM_MEMTYPE_PROCESSLOCAL, stMsgDataParam);

        return 0;
    }

    WA_ERROR("getMocaParam_IARM(): IARM_Malloc('%s') failed\n", IARM_BUS_TR69HOSTIFMGR_NAME);
    return -1;
}
#endif /* MEDIA_CLIENT */

/*****************************************************************************
 * EXPORTED FUNCTIONS
 *****************************************************************************/

int WA_DIAG_MOCA_status(void* instanceHandle, void *initHandle, json_t **params)
{
    int ret;
    json_t * jsonConfig = NULL;
    const char *interface = MOCA_AVAILABLE_STR;
#ifndef MEDIA_CLIENT
    WA_UTILS_SNMP_Resp_t value;
    int group = MOCA_GROUP_11;
    int ifIndex = -1;
#endif /* MEDIA_CLIENT */

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

    if(ret == 0)
    {
        WA_ERROR("Device does not have MoCA interface.\n");
        return setReturnData(WA_DIAG_ERRCODE_NOT_APPLICABLE, params);
    }
    WA_DBG("MoCA supported on device.\n");

#ifndef MEDIA_CLIENT
    value.type = WA_UTILS_SNMP_RESP_TYPE_LONG;
    ret = getMocaIfEnableStatus(group, ifIndex, &value, WA_UTILS_SNMP_REQ_TYPE_WALK);
    if(ret < 0)
    {
        group = MOCA_GROUP_20;
        ret = getMocaIfEnableStatus(group, ifIndex, &value, WA_UTILS_SNMP_REQ_TYPE_WALK);
    }
#else
    if (WA_UTILS_IARM_Connect())
    {
        WA_ERROR("WA_DIAG_MOCA_status(): WA_UTILS_IARM_Connect() Failed \n");
        return -1;
    }
    ret = getMocaIfEnableStatus_IARM();
#endif /* MEDIA_CLIENT */

    if(ret < 1)
    {
#ifdef MEDIA_CLIENT
        if(WA_UTILS_IARM_Disconnect())
        {
            WA_DBG("WA_DIAG_MOCA_status(): WA_UTILS_IARM_Disconnect() failed\n");
        }
#endif
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
            default:
                return setReturnData(WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR, params);
        }
    }

    WA_DBG("MoCA status read successfully.\n");

#ifndef MEDIA_CLIENT
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
#else
    ret = getMocaIfRxPacketsCount_IARM();
    if(WA_UTILS_IARM_Disconnect())
    {
        WA_DBG("WA_DIAG_MOCA_status(): WA_UTILS_IARM_Disconnect() Failed \n");
    }
#endif /* MEDIA_CLIENT */

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

#ifndef MEDIA_CLIENT
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

#else
int getMocaIfRFChannelFrequency_IARM(int *value)
{
    int ret = -1;
    char *data;

    ret = getMocaParam_IARM(TR69_MOCA_IF_RF_CHANNEL_FREQUENCY, &data);

    *value = *(int*)data; // Decoding in the same format as how Tr69HostIf encoded the data

    return ret;
}

int getMocaIfNetworkController_IARM(int *value)
{
    int ret = -1;
    char *data;

    ret = getMocaParam_IARM(TR69_MOCA_IF_NETWORK_CONTROLLER, &data);

    *value = *(int*)data; // Decoding in the same format as how Tr69HostIf encoded the data

    return ret;
}

int getMocaIfTransmitRate_IARM(char *value)
{
    int ret = -1;
    int phy_rate = 0;
    int num_entries = 0;
    int min = -1;
    int max = -1;
    char data[256];
    char *output;

    if ((num_entries = getMocaIfMeshTable_IARM()) == 0)
    {
        WA_ERROR("getMocaIfTransmitRate_IARM(): getMocaIfMeshTable_IARM(): Mesh table entries not found to fetch transmit rate.\n");
    }

    WA_DBG("getMocaIfTransmitRate_IARM(): getMocaIfMeshTable_IARM() returned %i entries\n", num_entries);

    for (int index = 1; index <= num_entries; index++)
    {
        snprintf(data, sizeof(data), "%s%i%s", TR69_MOCA_IF_MESH_TABLE, index, TR69_MOCA_IF_TRANSMIT_RATE);
        if ((ret = getMocaParam_IARM(data, &output)) < 0)
        {
            WA_ERROR("getMocaIfTransmitRate_IARM(): getMocaParam_IARM('%s') failed\n", data);
            continue;
        }

        phy_rate = *(int*)output; // Decoding in the same format as how Tr69HostIf encoded the data

        min = (phy_rate < min || min == -1) ? phy_rate : min;
        max = (phy_rate > max || max == -1) ? phy_rate : max;
    }

    snprintf(data, sizeof(data), "Min: %i Mbps, Max: %i Mbps", min, max);
    strcpy(value, data);

    return ret;
}

int getMocaNodeSNR_IARM(char *value)
{
    int ret = -1;
    int node_snr = 0;
    int num_devices = 0;
    int min = -1;
    int max = -1;
    char data[256];
    char *output;

    if ((num_devices = getMocaIfAssociatedDevice_IARM()) == 0)
    {
        WA_ERROR("getMocaNodeSNR_IARM: getMocaIfAssociatedDevice_IARM(): Associated devices not found to fetch transmit rate.\n");
    }

    WA_DBG("getMocaNodeSNR_IARM(): getMocaIfAssociatedDevice_IARM() returned %i devices\n", num_devices);

    for (int index = 1; index <= num_devices; index++)
    {
        snprintf(data, sizeof(data), "%s%i%s", TR69_MOCA_IF_ASSOCIATED_DEVICE, index, TR69_MOCA_IF_NODE_SNR);
        if ((ret = getMocaParam_IARM(data, &output)) < 0)
        {
            WA_ERROR("getMocaNodeSNR_IARM(): getMocaParam_IARM('%s') failed\n", data);
            continue;
        }

        node_snr = *(int*)output; // Decoding in the same format as how Tr69HostIf encoded the data

        min = (node_snr < min || min == -1) ? node_snr : min;
        max = (node_snr > max || max == -1) ? node_snr : max;
    }

    snprintf(data, sizeof(data), "Min: %i dB, Max: %i dB", min, max);
    strcpy(value, data);

    return ret;
}
#endif /* MEDIA_CLEINT */

/* End of doxygen group */
/*! @} */

