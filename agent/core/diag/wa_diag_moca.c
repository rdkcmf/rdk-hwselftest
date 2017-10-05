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

/* module interface */
#include "wa_diag_moca.h"

/*****************************************************************************
 * GLOBAL VARIABLE DEFINITIONS
 *****************************************************************************/

/*****************************************************************************
 * LOCAL DEFINITIONS
 *****************************************************************************/

#define OID_MOCA_IF_ENABLE_STATUS "MOCA11-MIB::mocaIfEnable"
#define OID_MOCA_IF_NODES_COUNT "MOCA11-MIB::mocaIfNumNodes"
#define OID_MOCA_IF_RX_PACKETS_COUNT "MOCA11-MIB::mocaIfRxPackets"

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
/*****************************************************************************
 * LOCAL TYPES
 *****************************************************************************/
typedef enum {
    MOCA_OPT_IF_ENABLE_STATUS,
    MOCA_OPT_IF_NODES_COUNT,
    MOCA_OPT_IF_RX_PACKETS_COUNT,
    MOCA_OPT_MAX,
} MocaOption_t;

/*****************************************************************************
 * LOCAL FUNCTION PROTOTYPES
 *****************************************************************************/
static int moca_supported(const char *interface);
static int getMocaOptionStatus(MocaOption_t opt, long *value);
static int getMocaIfEnableStatus(long *value);
static int getMocaIfNodesCount(void* instanceHandle, long *value, int progress);
static int getMocaIfRxPacketsCount(void* instanceHandle, long *value, int progress);
static int verifyOptionValue(MocaOption_t opt, long *value);
static int setReturnData(int status, json_t **param);

/*****************************************************************************
 * LOCAL VARIABLE DECLARATIONS
 *****************************************************************************/
char *MocaOptions[MOCA_OPT_MAX] = {
    OID_MOCA_IF_ENABLE_STATUS,
    OID_MOCA_IF_NODES_COUNT,
    OID_MOCA_IF_RX_PACKETS_COUNT
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

static int getMocaOptionStatus(MocaOption_t opt, long *value)
{
    int ret = -1;
    char oid[256];

    WA_ENTER("getMocaOptionStatus (%s).\n", MocaOptions[opt]);

    ret = snprintf(oid, sizeof(oid), "%s", MocaOptions[opt]);
    if ((ret >= sizeof(oid)) || (ret < 0))
    {
        WA_ERROR("getMocaOptionStatus, unable to generate OID.\n");
        return -1;
    }

    if(!WA_UTILS_SNMP_GetLong(SNMP_SERVER, oid, value, WA_UTILS_SNMP_REQ_TYPE_WALK))
    {
        WA_ERROR("getMocaOptionStatus, option: %s, failed.\n", MocaOptions[opt]);
        return -1;
    }

    WA_RETURN("getMocaOptionStatus, option: %s, state: %ld (%p).\n", MocaOptions[opt], *value, value);

    return verifyOptionValue(opt, value);
}

static int getMocaIfEnableStatus(long *value)
{
    return getMocaOptionStatus(MOCA_OPT_IF_ENABLE_STATUS, value);
}

static int getMocaIfNodesCount(void* instanceHandle, long *value, int progress)
{
    int status;
    int step = 0;
    
    while(1)
    {
        ++step;

        status = getMocaOptionStatus(MOCA_OPT_IF_NODES_COUNT, value);

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

static int getMocaIfRxPacketsCount(void* instanceHandle, long *value, int progress)
{
    int status;
    int step = 0;
    
    while(1)
    {
        ++step;

        status = getMocaOptionStatus(MOCA_OPT_IF_RX_PACKETS_COUNT, value);

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

    return getMocaOptionStatus(MOCA_OPT_IF_RX_PACKETS_COUNT, value);
}

static int verifyOptionValue(MocaOption_t opt, long *value)
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
        case MOCA_OPT_IF_ENABLE_STATUS:
            switch (*value)
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

        case MOCA_OPT_IF_NODES_COUNT:
            ret = (*value >= MOCA_IF_NODES_COUNT_MIN ? 1 : 0);
            break;

        case MOCA_OPT_IF_RX_PACKETS_COUNT:
            ret = (*value >= 1 ? 1 : 0);
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
    long value;
    json_t * jsonConfig = NULL;    
    const char *interface = MOCA_AVAILABLE_STR;

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

    ret = getMocaIfEnableStatus(&value);
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

    ret = getMocaIfNodesCount(instanceHandle, &value, 0);
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

    // If nodes count grater than 1, still need to check if any packets received
    ret = getMocaIfRxPacketsCount(instanceHandle, &value, NODESCOUNT_STEPS);
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


/* End of doxygen group */
/*! @} */

