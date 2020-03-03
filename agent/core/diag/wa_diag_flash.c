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
#define DEV_CONFIG_FILE_PATH            "/etc/device.properties"
#define FILE_MODE                       "r"
#define EMMC_DEFAULT_MOUNT_PATH_STR     "/media/tsb"
#define EMMC_DEFAULT_TEST_FILE_STR      "diagsys-test-file"
#define EMMC_DEFAULT_TEST_FILE_SIZE     (512*1024)

#define TR69_EMMC_LIFE_ELAPSED_B        "Device.Services.STBService.1.Components.X_RDKCENTRAL-COM_eMMCFlash.LifeElapsedB"
#define TR69_EMMC_LIFE_ELAPSED_A        "Device.Services.STBService.1.Components.X_RDKCENTRAL-COM_eMMCFlash.LifeElapsedA"
#define EMMC_SLC1_PARTITION             "/dev/mmcblk0gp"
#define EMMC_PATTERN_STR                "TSB_MOUNT_PATH="
#define EMMC_MAX_DEVICE_LIFETIME        0x0A /* 90% - 100% device life time used */
#define EMMC_ZERO_DEVICE_LIFETIME       0x0
#endif

/*****************************************************************************
 * LOCAL TYPES
 *****************************************************************************/

/*****************************************************************************
 * LOCAL FUNCTION PROTOTYPES
 *****************************************************************************/
#ifdef HAVE_DIAG_FLASH_XI6
static int setReturnData(int status, json_t **param);
static int checkEMMCLifeLapseParam(char *param);
static int checkEMMCStorageLife();
#endif

/*****************************************************************************
 * LOCAL VARIABLE DECLARATIONS
 *****************************************************************************/
#ifndef HAVE_DIAG_FLASH_XI6
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

        case WA_DIAG_ERRCODE_EMMC_MAX_LIFE_EXCEED_FAILURE:
            *param = json_string("Device Exceeded Max Life.");
            break;

        case WA_DIAG_ERRCODE_NOT_APPLICABLE:
            *param = json_string("Not applicable.");
            break;

        case WA_DIAG_ERRCODE_CANCELLED:
            *param = json_string("Test cancelled.");
            break;

        case WA_DIAG_ERRCODE_EMMC_ZERO_LIFETIME_FAILURE:
            *param = json_string("Device Returned Invalid Response.");
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
    int result = WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR;

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
            return WA_DIAG_ERRCODE_EMMC_ZERO_LIFETIME_FAILURE;
        }

        result = (value <= EMMC_MAX_DEVICE_LIFETIME) ? WA_DIAG_ERRCODE_SUCCESS : WA_DIAG_ERRCODE_EMMC_MAX_LIFE_EXCEED_FAILURE;

        IARM_Free(IARM_MEMTYPE_PROCESSLOCAL, stMsgDataParam);

        return result;
    }

    WA_ERROR("checkEMMCLifeLapseParam(): IARM_Malloc('%s') failed\n", IARM_BUS_TR69HOSTIFMGR_NAME);
    return result;
}

static int checkEMMCStorageLife()
{
    int result = checkEMMCLifeLapseParam(TR69_EMMC_LIFE_ELAPSED_A);
    if (result == WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR)
    {
        WA_ERROR("checkEMMCStorageLife(): checkEMMCLifeLapseParam('%s') failed\n", TR69_EMMC_LIFE_ELAPSED_A);
        return result;
    }

    if (result == WA_DIAG_ERRCODE_EMMC_MAX_LIFE_EXCEED_FAILURE)
    {
        WA_ERROR("checkEMMCStorageLife(): checkEMMCLifeLapseParam('%s') eMMC device exceeded max life\n", TR69_EMMC_LIFE_ELAPSED_A);
        return result;
    }

    if (result == WA_DIAG_ERRCODE_EMMC_ZERO_LIFETIME_FAILURE)
    {
        WA_ERROR("checkEMMCStorageLife(): checkEMMCLifeLapseParam('%s') eMMC device zero life\n", TR69_EMMC_LIFE_ELAPSED_A);
        return result;
    }

    result = checkEMMCLifeLapseParam(TR69_EMMC_LIFE_ELAPSED_B);
    if (result == WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR)
    {
        WA_ERROR("checkEMMCStorageLife(): checkEMMCLifeLapseParam('%s') failed\n", TR69_EMMC_LIFE_ELAPSED_B);
        return result;
    }

    if (result == WA_DIAG_ERRCODE_EMMC_MAX_LIFE_EXCEED_FAILURE)
    {
        WA_ERROR("checkEMMCStorageLife(): checkEMMCLifeLapseParam('%s') eMMC device exceeded max life\n", TR69_EMMC_LIFE_ELAPSED_B);
        return result;
    }

    if (result == WA_DIAG_ERRCODE_EMMC_ZERO_LIFETIME_FAILURE)
    {
        WA_ERROR("checkEMMCStorageLife(): checkEMMCLifeLapseParam('%s') eMMC device zero life\n", TR69_EMMC_LIFE_ELAPSED_B);
        return result;
    }

    return result;
}
#endif

int WA_DIAG_FLASH_status(void* instanceHandle, void *initHandle, json_t **pJsonInOut)
{
    int result = WA_DIAG_ERRCODE_FAILURE;
    json_t *config;

    json_decref(*pJsonInOut);
    *pJsonInOut = NULL;

#ifdef HAVE_DIAG_FLASH_XI6
    char *mountPath = NULL;
    const char *filename = NULL;

    /* Use test config details if found */
    config = ((WA_DIAG_proceduresConfig_t*)initHandle)->config;
    if(config && (json_unpack(config, "{ss}", "filename", &filename)==0))
    {
        WA_DBG("flash_status: config filename = %s\n", filename);
    }
    else
    {
        WA_INFO("flash_status: config filename not found\n");
        filename = NULL;
    }

    if(WA_OSA_TaskCheckQuit())
    {
        WA_DBG("flash_status: Test cancelled\n");
        return setReturnData(WA_DIAG_ERRCODE_CANCELLED, pJsonInOut);
    }

    mountPath = WA_UTILS_FILEOPS_OptionFind(DEV_CONFIG_FILE_PATH, EMMC_PATTERN_STR);

    if(mountPath == NULL)
    {
        WA_INFO("flash_status: Mount path not found.\n");

        if(filename == NULL)
        {
            WA_ERROR("flash_status: Neither config filename nor Mount Path found.\n");
            return setReturnData(WA_DIAG_ERRCODE_NOT_APPLICABLE, pJsonInOut);
        }

        if (asprintf(&mountPath, EMMC_DEFAULT_MOUNT_PATH_STR) == -1)
        {
            WA_ERROR("flash_status: asprintf failed\n");
            return setReturnData(WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR, pJsonInOut);
        }
    }
    else
    {
        WA_DBG("flash_status: mountPath=%s\n", mountPath);
    }

    if(WA_OSA_TaskCheckQuit())
    {
        WA_DBG("flash_status: Test cancelled\n");
        free((void *)mountPath);
        return setReturnData(WA_DIAG_ERRCODE_CANCELLED, pJsonInOut);
    }

    char *defaultFilePath = NULL;
    int res = asprintf(&defaultFilePath, "%s/%s", mountPath, EMMC_DEFAULT_TEST_FILE_STR);
    free((void *)mountPath);
    if (res < 0)
    {
        WA_ERROR("flash_status: asprintf failed\n");
        return setReturnData(WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR, pJsonInOut);
    }
    WA_DBG("flash_status: defaultFilePath=%s\n", defaultFilePath);

    result = WA_DIAG_FileTest((const char *)defaultFilePath, EMMC_DEFAULT_TEST_FILE_SIZE, config, pJsonInOut);
    free((void *)defaultFilePath);

    if (result)
    {
        WA_ERROR("flash_status: eMMC error during file test in SLC2 partition.\n");
        return setReturnData(result, pJsonInOut);
    }

    char *mountPartition = NULL;
    mountPath = NULL;

    const char *file = "/etc/mtab";
    struct mntent *fs = NULL;
    FILE *fp = setmntent(file, "r");

    if (fp == NULL)
    {
        WA_ERROR("flash_status: could not open %s\n", file);
        return setReturnData(WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR, pJsonInOut);
    }

    while ((fs = getmntent(fp)) != NULL)
    {
        if (fs->mnt_fsname[0] != '/')   /* skip non-real file systems */
            continue;

        if (strncmp(fs->mnt_fsname, EMMC_SLC1_PARTITION, strlen(EMMC_SLC1_PARTITION)) == 0)
        {
            mountPartition = fs->mnt_fsname;
            mountPath = fs->mnt_dir;
            break;
        }
    }
    endmntent(fp);

    if (!mountPath)
    {
        WA_ERROR("flash_status: eMMC mount path %s not found in SLC1 partition %s\n", mountPath, mountPartition);
        return setReturnData(WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR, pJsonInOut);
    }

    WA_DBG("flash_status: eMMC mount partition=%s, path=%s\n", mountPartition, mountPath);

    res = asprintf(&defaultFilePath, "%s/%s", mountPath, EMMC_DEFAULT_TEST_FILE_STR);
    if (res < 0)
    {
        WA_ERROR("flash_status: asprintf failed for eMMC\n");
        return setReturnData(WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR, pJsonInOut);
    }
    WA_DBG("flash_status: eMMC SLC1 defaultFilePath=%s\n", defaultFilePath);

    result = WA_DIAG_FileTest((const char *)defaultFilePath, EMMC_DEFAULT_TEST_FILE_SIZE, config, pJsonInOut);
    free((void *)defaultFilePath);

    if (result)
    {
        WA_ERROR("flash_status: eMMC error during file test in SLC1 partition.\n");
        return setReturnData(result, pJsonInOut);
    }

    if (WA_UTILS_IARM_Connect())
    {
        WA_ERROR("flash_status: WA_UTILS_IARM_Connect() Failed \n");
        return setReturnData(WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR, pJsonInOut);
    }

    result = checkEMMCStorageLife();

    if(WA_UTILS_IARM_Disconnect())
    {
        WA_ERROR("flash_status: WA_UTILS_IARM_Disconnect() Failed \n");
    }

    WA_RETURN("flash_status: %d\n", result);
    return setReturnData(result, pJsonInOut);

#else

    int applicable;

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

    return result;

#endif
}

/*****************************************************************************
 * LOCAL FUNCTIONS
 *****************************************************************************/

/* End of doxygen group */
/*! @} */

