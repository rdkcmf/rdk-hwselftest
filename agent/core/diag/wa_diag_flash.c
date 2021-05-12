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
 * @brief Diagnostic functions for flash - implementation
 */

/** @addtogroup WA_DIAG_FLASH
 *  @{
 */

/*****************************************************************************
 * STANDARD INCLUDE FILES
 *****************************************************************************/
#define _GNU_SOURCE
#include <string.h>
#include <stdio.h>
#include <mntent.h>

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/
#include "wa_diag.h"
#include "wa_debug.h"
#include "wa_fileops.h"
#include "wa_diag_file.h"

/* module interface */
#include "wa_diag_flash.h"

/* rdk specific */
#include "wa_iarm.h"
#include "wa_json.h"
#include "libIBus.h"
#include "libIARMCore.h"

/*****************************************************************************
 * GLOBAL VARIABLE DEFINITIONS
 *****************************************************************************/

/*****************************************************************************
 * LOCAL DEFINITIONS
 *****************************************************************************/
#ifdef HAVE_DIAG_FLASH_XI6
#define TR181_XEMMC_FLASH               "Device.Services.STBService.1.Components.X_RDKCENTRAL-COM_eMMCFlash."
#define EMMC_PRE_EOL_STATE_EUDA         "PreEOLStateEUDA"
#define EMMC_PRE_EOL_STATE_SYSTEM       "PreEOLStateSystem"
#define EMMC_PRE_EOL_STATE_MLC          "PreEOLStateMLC"
#define EMMC_NORMAL_PRE_EOL_STATE       "Normal"

#define TR69_EMMC_LIFE_ELAPSED_B        "Device.Services.STBService.1.Components.X_RDKCENTRAL-COM_eMMCFlash.LifeElapsedB"
#define TR69_EMMC_LIFE_ELAPSED_A        "Device.Services.STBService.1.Components.X_RDKCENTRAL-COM_eMMCFlash.LifeElapsedA"
#define EMMC_MAX_DEVICE_LIFETIME        0x0A /* 90% - 100% device life time used */
#define EMMC_ZERO_DEVICE_LIFETIME       0x0
#define MAX_LIFE_EXCEED_FAILURE         -4
#define ZERO_LIFETIME_FAILURE           -5
#endif

/*****************************************************************************
 * LOCAL TYPES
 *****************************************************************************/
#ifdef HAVE_DIAG_FLASH_XI6
typedef enum {
    EUDA,
    SYSTEM,
    MLC,
    MAX_STATES
} PreEOLState_t;
#endif

/*****************************************************************************
 * LOCAL FUNCTION PROTOTYPES
 *****************************************************************************/
#ifdef HAVE_DIAG_FLASH_XI6
static int setReturnData(int status, json_t **param);
static int checkEMMCLifeLapseParam(char *param);
static int checkEMMCStorageLife();
static int checkEMMCPreEOLState(char* param);
#endif

/*****************************************************************************
 * LOCAL VARIABLE DECLARATIONS
 *****************************************************************************/
#ifdef HAVE_DIAG_FLASH_XI6
char *PreEOLState[MAX_STATES] = {
    EMMC_PRE_EOL_STATE_EUDA,
    EMMC_PRE_EOL_STATE_SYSTEM,
    EMMC_PRE_EOL_STATE_MLC
};
#else
static const size_t defaultTotalSize = 512 * 1024;
static const char * defaultFileName = "/mnt/nvram/diagsys-flash-test-file";
#endif
/*****************************************************************************
 * FUNCTION DEFINITIONS
 *****************************************************************************/

/*****************************************************************************
 * EXPORTED FUNCTIONS
 *****************************************************************************/

#ifdef HAVE_DIAG_FLASH_XI6
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
            *param = json_string("eMMC Card good.");
            break;

        case WA_DIAG_ERRCODE_FAILURE:
            *param = json_string("eMMC Card bad.");
            break;

        case WA_DIAG_ERRCODE_NOT_APPLICABLE:
            *param = json_string("Not applicable.");
            break;

        case WA_DIAG_ERRCODE_CANCELLED:
            *param = json_string("Test cancelled.");
            break;

        case WA_DIAG_ERRCODE_EMMC_TYPEA_MAX_LIFE_EXCEED_FAILURE:
            *param = json_string("Device Type A Exceeded Max Life.");
            break;

        case WA_DIAG_ERRCODE_EMMC_TYPEB_MAX_LIFE_EXCEED_FAILURE:
            *param = json_string("Device Type B Exceeded Max Life.");
            break;

        case WA_DIAG_ERRCODE_EMMC_TYPEA_ZERO_LIFETIME_FAILURE:
            *param = json_string("Device Type A Returned Invalid Response.");
            break;

        case WA_DIAG_ERRCODE_EMMC_TYPEB_ZERO_LIFETIME_FAILURE:
            *param = json_string("Device Type B Returned Invalid Response.");
            break;

        case WA_DIAG_ERRCODE_EMMC_PREEOL_STATE_FAILURE:
            *param = json_string("Pre EOL State Error.");
            break;

        case WA_DIAG_ERRCODE_FILE_WRITE_OPERATION_FAILURE:
            *param = json_string("File Write Operation Error.");
            break;

        case WA_DIAG_ERRCODE_FILE_READ_OPERATION_FAILURE:
            *param = json_string("File Read Operation Error.");
            break;

        case WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR:
            *param = json_string("Internal test error.");
            break;

        default:
            break;
    }

    return status;
}

static int checkEMMCLifeLapseParam(char *param)
{
    int is_connected = 0;
    char *data;
    int value;
    int result = -1;

    WA_ENTER("checkEMMCLifeLapseParam()\n");

    IARM_Result_t iarm_result = IARM_Bus_IsConnected(IARM_BUS_TR69HOSTIFMGR_NAME, &is_connected);
    if (iarm_result != IARM_RESULT_SUCCESS)
    {
        WA_ERROR("checkEMMCLifeLapseParam(): IARM_Bus_IsConnected('%s') failed\n", IARM_BUS_TR69HOSTIFMGR_NAME);
        return result;
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
            WA_ERROR("checkEMMCLifeLapseParam(): IARM_Bus_Call('%s') failed\n", IARM_BUS_TR69HOSTIFMGR_NAME);
            IARM_Free(IARM_MEMTYPE_PROCESSLOCAL, stMsgDataParam);
            return result;
        }

        // Decoding in the same format as how Tr69HostIf encoded the data
        data = (char*)&stMsgDataParam->paramValue[0];
        value = *(int*)data;

        if (value <= EMMC_ZERO_DEVICE_LIFETIME)
        {
            IARM_Free(IARM_MEMTYPE_PROCESSLOCAL, stMsgDataParam);
            return ZERO_LIFETIME_FAILURE;
        }

        result = (value <= EMMC_MAX_DEVICE_LIFETIME) ? WA_DIAG_ERRCODE_SUCCESS : MAX_LIFE_EXCEED_FAILURE;

        IARM_Free(IARM_MEMTYPE_PROCESSLOCAL, stMsgDataParam);

        return result;
    }

    WA_ERROR("checkEMMCLifeLapseParam(): IARM_Malloc('%s') failed\n", IARM_BUS_TR69HOSTIFMGR_NAME);
    return result;
}

static int checkEMMCStorageLife()
{
    int result = checkEMMCLifeLapseParam(TR69_EMMC_LIFE_ELAPSED_A);
    if (result == -1)
    {
        WA_ERROR("checkEMMCStorageLife(): checkEMMCLifeLapseParam('%s') failed\n", TR69_EMMC_LIFE_ELAPSED_A);
        return WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR;
    }

    if (result == MAX_LIFE_EXCEED_FAILURE)
    {
        WA_ERROR("checkEMMCStorageLife(): checkEMMCLifeLapseParam('%s') eMMC device exceeded max life\n", TR69_EMMC_LIFE_ELAPSED_A);
        return WA_DIAG_ERRCODE_EMMC_TYPEA_MAX_LIFE_EXCEED_FAILURE;
    }

    if (result == ZERO_LIFETIME_FAILURE)
    {
        WA_ERROR("checkEMMCStorageLife(): checkEMMCLifeLapseParam('%s') eMMC device zero life\n", TR69_EMMC_LIFE_ELAPSED_A);
        return WA_DIAG_ERRCODE_EMMC_TYPEA_ZERO_LIFETIME_FAILURE;
    }

    result = checkEMMCLifeLapseParam(TR69_EMMC_LIFE_ELAPSED_B);
    if (result == -1)
    {
        WA_ERROR("checkEMMCStorageLife(): checkEMMCLifeLapseParam('%s') failed\n", TR69_EMMC_LIFE_ELAPSED_B);
        return WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR;
    }

    if (result == MAX_LIFE_EXCEED_FAILURE)
    {
        WA_ERROR("checkEMMCStorageLife(): checkEMMCLifeLapseParam('%s') eMMC device exceeded max life\n", TR69_EMMC_LIFE_ELAPSED_B);
        return WA_DIAG_ERRCODE_EMMC_TYPEB_MAX_LIFE_EXCEED_FAILURE;
    }

    if (result == ZERO_LIFETIME_FAILURE)
    {
        WA_ERROR("checkEMMCStorageLife(): checkEMMCLifeLapseParam('%s') eMMC device zero life\n", TR69_EMMC_LIFE_ELAPSED_B);
        return WA_DIAG_ERRCODE_EMMC_TYPEB_ZERO_LIFETIME_FAILURE;
    }

    return result;
}

static int checkEMMCPreEOLState(char* param)
{
    IARM_Result_t iarm_result = IARM_RESULT_IPCCORE_FAIL;
    int status = WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR;
    int is_connected = 0;
    char* value;

    WA_ENTER("checkEMMCPreEOLState()\n");

    iarm_result = IARM_Bus_IsConnected(IARM_BUS_TR69HOSTIFMGR_NAME, &is_connected);
    if (iarm_result != IARM_RESULT_SUCCESS)
    {
        WA_ERROR("checkEMMCPreEOLState(): IARM_Bus_IsConnected('%s') failed\n", IARM_BUS_TR69HOSTIFMGR_NAME);
        return status;
    }

    HOSTIF_MsgData_t *stMsgDataParam = NULL;

    iarm_result = IARM_Malloc(IARM_MEMTYPE_PROCESSLOCAL, sizeof(*stMsgDataParam), (void**)&stMsgDataParam);
    if ((iarm_result == IARM_RESULT_SUCCESS) && stMsgDataParam)
    {
        memset(stMsgDataParam, 0, sizeof(*stMsgDataParam));
        snprintf(stMsgDataParam->paramName, TR69HOSTIFMGR_MAX_PARAM_LEN, "%s%s", TR181_XEMMC_FLASH, param);

        WA_DBG("checkEMMCPreEOLState(): IARM_Bus_Call('%s', '%s', %s)\n", IARM_BUS_TR69HOSTIFMGR_NAME, IARM_BUS_TR69HOSTIFMGR_API_GetParams, param);
        iarm_result = IARM_Bus_Call(IARM_BUS_TR69HOSTIFMGR_NAME, IARM_BUS_TR69HOSTIFMGR_API_GetParams, (void *)stMsgDataParam, sizeof(*stMsgDataParam));

        if (iarm_result != IARM_RESULT_SUCCESS)
        {
            WA_ERROR("checkEMMCPreEOLState(): IARM_Bus_Call('%s') failed\n", IARM_BUS_TR69HOSTIFMGR_NAME);
            IARM_Free(IARM_MEMTYPE_PROCESSLOCAL, stMsgDataParam);
            return status;
        }

        value = (char*)&stMsgDataParam->paramValue[0];
        if (strcmp(value, EMMC_NORMAL_PRE_EOL_STATE))
        {
            WA_ERROR("checkEMMCPreEOLState(): %s status is %s\n", param, value);
            status = WA_DIAG_ERRCODE_EMMC_PREEOL_STATE_FAILURE;
        }
        else
        {
            WA_DBG("checkEMMCPreEOLState(): %s status is %s\n", param, value);
            status = WA_DIAG_ERRCODE_SUCCESS;
        }

        IARM_Free(IARM_MEMTYPE_PROCESSLOCAL, stMsgDataParam);
    }

    WA_RETURN("checkEMMCPreEOLState(): %i\n", status);

    return status;
}
#endif

int WA_DIAG_FLASH_status(void* instanceHandle, void *initHandle, json_t **pJsonInOut)
{
    int result = WA_DIAG_ERRCODE_FAILURE;

    json_decref(*pJsonInOut);
    *pJsonInOut = NULL;

#ifdef HAVE_DIAG_FLASH_XI6

    if (WA_UTILS_IARM_Connect())
    {
        WA_ERROR("flash_status(): WA_UTILS_IARM_Connect() Failed \n");
        return WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR;
    }

    result = WA_DIAG_ERRCODE_SUCCESS; // Setting success initially before checking Pre EOL States
    for (PreEOLState_t index = 0; index < MAX_STATES; index++)
    {
        int status = checkEMMCPreEOLState(PreEOLState[index]);

        if (status == WA_DIAG_ERRCODE_EMMC_PREEOL_STATE_FAILURE)
            result = WA_DIAG_ERRCODE_EMMC_PREEOL_STATE_FAILURE; // If any one of the Pre EOL State is not normal, the test will fail
        else if (status == WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR && result != WA_DIAG_ERRCODE_EMMC_PREEOL_STATE_FAILURE)
            result = WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR; // If any one of the Pre EOL State IARM Call fails but for other's state is normal, test will return Internal Error
    }

    if(WA_OSA_TaskCheckQuit())
    {
        WA_DBG("flash_status: Test cancelled\n");
        result =  WA_DIAG_ERRCODE_CANCELLED;
    }

    if (result == WA_DIAG_ERRCODE_SUCCESS)
    {
        WA_INFO("flash_status(): eMMC Pre-EOL States are normal. Checking for life lapse for 2 partitions.\n");
        result = checkEMMCStorageLife();
    }

    if(WA_UTILS_IARM_Disconnect())
    {
        WA_DBG("flash_status(): WA_UTILS_IARM_Disconnect() failed\n");
    }

    WA_RETURN("flash_status: %d\n", result);
    return setReturnData(result, pJsonInOut);

#else

    int applicable;
    json_t *config;

    /* Determine if the test is applicable: */
    config = ((WA_DIAG_proceduresConfig_t*)initHandle)->config;
    if(config && !json_unpack(config, "{sb}", "applicable", &applicable))
    {
        if(!applicable)
        {
            WA_INFO("Not applicable\n");
            *pJsonInOut = json_string("Not applicable.");
            return WA_DIAG_ERRCODE_NOT_APPLICABLE;
        }
    }

    /* Perform the FLASH test */
    result = WA_DIAG_FileTest(defaultFileName, defaultTotalSize,
             ((WA_DIAG_proceduresConfig_t*)initHandle)->config, pJsonInOut);

    WA_RETURN("flash_status: returns \"%d\"\n", result);
    return result;

#endif
}

/*****************************************************************************
 * LOCAL FUNCTIONS
 *****************************************************************************/

/* End of doxygen group */
/*! @} */

