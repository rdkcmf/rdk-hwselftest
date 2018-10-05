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
#include "wa_fileops.h"
#include "wa_diag_file.h"

/* module interface */
#include "wa_diag_sdcard.h"

/*****************************************************************************
 * GLOBAL VARIABLE DEFINITIONS
 *****************************************************************************/

/*****************************************************************************
 * LOCAL DEFINITIONS
 *****************************************************************************/
#define DEV_CONFIG_FILE_PATH            "/etc/device.properties"
#define FILE_MODE                       "r"
#define SD_CARD_PATTERN_STR             "SD_CARD_MOUNT_PATH="
#define SD_CARD_DEFAULT_MOUNT_PATH_STR  "/media/tsb"
#define SD_CARD_DEFAULT_TEST_FILE_STR   "diagsys-test-file"
#define SD_CARD_DEFAULT_TEST_FILE_SIZE  (512*1024)

/*****************************************************************************
 * LOCAL TYPES
 *****************************************************************************/


/*****************************************************************************
 * LOCAL FUNCTION PROTOTYPES
 *****************************************************************************/

static int setReturnData(int status, json_t **param);

/*****************************************************************************
 * LOCAL VARIABLE DECLARATIONS
 *****************************************************************************/

/*****************************************************************************
 * FUNCTION DEFINITIONS
 *****************************************************************************/

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
    const char *filename = NULL;
    char *mountPath = NULL;

    json_decref(*params); // not used
    *params = NULL;

    WA_ENTER("sdcard_status\n");

    /* Use test config details if found */
    config = ((WA_DIAG_proceduresConfig_t*)initHandle)->config;
    if(config && (json_unpack(config, "{ss}", "filename", &filename)==0))
    {
        WA_DBG("sdcard_status: config filename = %s\n", filename);
    }
    else
    {
        WA_INFO("sdcard_status: config filename not found\n");
        filename = NULL;
    }

    if(WA_OSA_TaskCheckQuit())
    {
        WA_DBG("sdcard_status: Test cancelled\n");
        return setReturnData(WA_DIAG_ERRCODE_CANCELLED, params);
    }

    mountPath = WA_UTILS_FILEOPS_OptionFind(DEV_CONFIG_FILE_PATH, SD_CARD_PATTERN_STR);
    if(mountPath == NULL)
    {
        WA_INFO("sdcard_status: Mount path not found.\n");

        if(filename == NULL)
        {
            WA_ERROR("sdcard_status: Neither config filename nor Mount Path found.\n");
            return setReturnData(WA_DIAG_ERRCODE_NOT_APPLICABLE, params);
        }

        if (asprintf(&mountPath, SD_CARD_DEFAULT_MOUNT_PATH_STR) == -1)
        {
            WA_ERROR("sdcard_status: asprintf failed\n");
            return setReturnData(WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR, params);
        }
    }
    else
    {
        WA_DBG("sdcard_status: mountPath=%s\n", mountPath);
    }

    if(WA_OSA_TaskCheckQuit())
    {
        WA_DBG("sdcard_status: Test cancelled\n");
        free((void *)mountPath);
        return setReturnData(WA_DIAG_ERRCODE_CANCELLED, params);
    }

    char *defaultFilePath = NULL;
    int res = asprintf(&defaultFilePath, "%s/%s", mountPath, SD_CARD_DEFAULT_TEST_FILE_STR);
    free((void *)mountPath);
    if (res == -1)
    {
        WA_ERROR("sdcard_status: asprintf failed\n");
        return setReturnData(WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR, params);
    }
    WA_DBG("sdcard_status: defaultFilePath=%s\n", defaultFilePath);

    status = WA_DIAG_FileTest((const char *)defaultFilePath, SD_CARD_DEFAULT_TEST_FILE_SIZE, config, params);
    free((void *)defaultFilePath);

    WA_RETURN("sdcard_status: %d\n", status);
    return setReturnData(status, params);
}


/*****************************************************************************
 * LOCAL FUNCTIONS
 *****************************************************************************/

/* End of doxygen group */
/*! @} */

