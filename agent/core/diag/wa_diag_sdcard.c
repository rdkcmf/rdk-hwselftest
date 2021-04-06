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
 * @brief Diagnostic functions for SD Card - implementation
 */

/** @addtogroup WA_DIAG_SDCARD
 *  @{
 */

/*****************************************************************************
 * STANDARD INCLUDE FILES
 *****************************************************************************/
#define _GNU_SOURCE
#include <string.h>
#include <stdio.h>

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/
#include "wa_diag.h"
#include "wa_debug.h"

/* rdk specific */
#include "wa_iarm.h"
#include "libIBus.h"
#include "libIARMCore.h"
#include "rdkStorageMgrTypes.h"

/* module interface */
#include "wa_diag_sdcard.h"

/*****************************************************************************
 * GLOBAL VARIABLE DEFINITIONS
 *****************************************************************************/

/*****************************************************************************
 * LOCAL DEFINITIONS
 *****************************************************************************/
#define IARM_BUS_STMGR_NAME     "rSTMgrBus" // Defined in rdkStorageMgr_iarm_private.h of storagemanager which is not accessible for other modules

/*****************************************************************************
 * LOCAL TYPES
 *****************************************************************************/


/*****************************************************************************
 * LOCAL FUNCTION PROTOTYPES
 *****************************************************************************/
static int checkTSBStatus();
static int checkTSBMaxMinutes();
static int setReturnData(int status, json_t **param);

/*****************************************************************************
 * LOCAL VARIABLE DECLARATIONS
 *****************************************************************************/

/*****************************************************************************
 * FUNCTION DEFINITIONS
 *****************************************************************************/
static int checkTSBStatus()
{
    IARM_Result_t iarm_result = IARM_RESULT_IPCCORE_FAIL;
    int ret = WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR;
    int is_connected = 0;
    eSTMGRTSBStatus status;

    iarm_result = IARM_Bus_IsConnected(IARM_BUS_STMGR_NAME, &is_connected);
    if (iarm_result != IARM_RESULT_SUCCESS || !is_connected)
    {
        WA_ERROR("checkTSBStatus(): IARM_Bus_IsConnected('%s') failed. Storage Manager not available.\n", IARM_BUS_STMGR_NAME);
        return ret;
    }

    iarm_result = IARM_Bus_Call(IARM_BUS_STMGR_NAME, "GetTSBStatus", (void *)&status, sizeof(status));
    if (iarm_result == IARM_RESULT_SUCCESS)
    {
        if (status == RDK_STMGR_TSB_STATUS_UNKNOWN) // Defined in rdkStorageMgrTypes.h whose decimal value is 64
        {
            WA_ERROR("checkTSBStatus(): TSBStatus=%i\n", status);
            ret = WA_DIAG_ERRCODE_SD_CARD_TSB_STATUS_FAILURE;
        }
        else
        {
            WA_DBG("checkTSBStatus(): TSBStatus is good (status=%i)\n", status);
            ret = WA_DIAG_ERRCODE_SUCCESS;
        }
    }
    else
        WA_ERROR("checkTSBStatus(): IARM_Bus_Call() returned %i\n", iarm_result);

    return ret;
}

static int checkTSBMaxMinutes()
{
    IARM_Result_t iarm_result = IARM_RESULT_IPCCORE_FAIL;
    int ret = WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR;
    int is_connected = 0;
    unsigned int minutes;

    iarm_result = IARM_Bus_IsConnected(IARM_BUS_STMGR_NAME, &is_connected);
    if (iarm_result != IARM_RESULT_SUCCESS || !is_connected)
    {
        WA_ERROR("checkTSBMaxMinutes(): IARM_Bus_IsConnected('%s') failed. Storage Manager not available.\n", IARM_BUS_STMGR_NAME);
        return ret;
    }

    iarm_result = IARM_Bus_Call(IARM_BUS_STMGR_NAME, "GetTSBMaxMinutes", (void *)&minutes, sizeof(minutes));
    if (iarm_result == IARM_RESULT_SUCCESS)
    {
        if (minutes == 0)
        {
            WA_ERROR("checkTSBMaxMinutes(): TSBMaxMinutes=%i\n", minutes);
            ret = WA_DIAG_ERRCODE_SD_CARD_ZERO_MAXMINUTES_FAILURE;
        }
        else
        {
            WA_DBG("checkTSBMaxMinutes(): TSBMaxMinutes is good (minutes=%i)\n", minutes);
            ret = WA_DIAG_ERRCODE_SUCCESS;
        }
    }
    else
        WA_ERROR("checkTSBMaxMinutes(): IARM_Bus_Call() returned %i\n", iarm_result);

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
            *param = json_string("SD Card good.");
            break;

        case WA_DIAG_ERRCODE_FAILURE:
            *param = json_string("SD Card bad.");
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

        case WA_DIAG_ERRCODE_SD_CARD_TSB_STATUS_FAILURE:
            *param = json_string("TSB Status Error");
            break;

        case WA_DIAG_ERRCODE_SD_CARD_ZERO_MAXMINUTES_FAILURE:
            *param = json_string("TSB Zero MaxMinutes");
            break;

        default:
            break;
    }

    return status;
}

/*****************************************************************************
 * EXPORTED FUNCTIONS
 *****************************************************************************/

int WA_DIAG_SDCARD_status(void* instanceHandle, void *initHandle, json_t **params)
{
    int status = WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR;
    json_t *config = NULL;

    json_decref(*params); // not used
    *params = NULL;

    WA_ENTER("sdcard_status\n");

    /* Use test config details if found */
    config = ((WA_DIAG_proceduresConfig_t*)initHandle)->config;
    if(!config)
    {
        WA_DBG("sdcard_status: config file not found.\n");
    }

    if(WA_OSA_TaskCheckQuit())
    {
        WA_DBG("sdcard_status: Test cancelled\n");
        return setReturnData(WA_DIAG_ERRCODE_CANCELLED, params);
    }

    if(WA_UTILS_IARM_Connect())
    {
        return setReturnData(WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR, params);
    }

    status = checkTSBStatus();

    if (status == WA_DIAG_ERRCODE_SUCCESS)
    {
        WA_DBG("sdcard_status: checkTSBStatus() passed\n");
        status = checkTSBMaxMinutes();
    }

    if(WA_UTILS_IARM_Disconnect())
    {
        WA_DBG("sdcard_status, WA_UTILS_IARM_Disconnect() Failed\n");
    }

    if(WA_OSA_TaskCheckQuit())
    {
        WA_DBG("sdcard_status: Test cancelled\n");
        return setReturnData(WA_DIAG_ERRCODE_CANCELLED, params);
    }

    WA_RETURN("sdcard_status: %d\n", status);
    return setReturnData(status, params);
}


/*****************************************************************************
 * LOCAL FUNCTIONS
 *****************************************************************************/

/* End of doxygen group */
/*! @} */

