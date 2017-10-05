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
 * @file wa_diag_hdd.c
 *
 * @brief Diagnostic functions for HDD - implementation
 */

/** @addtogroup WA_DIAG_HDD
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

/* module interface */
#include "wa_diag_hdd.h"

/*****************************************************************************
 * GLOBAL VARIABLE DEFINITIONS
 *****************************************************************************/

/*****************************************************************************
 * LOCAL DEFINITIONS
 *****************************************************************************/
#define SMART_OPTION_STR        "SMART support is"
#define SMART_ENABLED_STR       "Enabled"

#define HDD_OPTION_STR          "HDD_ENABLED"
#define HDD_AVAILABLE_STR       "true"

#define HDD_HEALTH_OPTION_STR   "SMART overall-health self-assessment test result"
#define HDD_HEALTH_OK_STR       "PASSED"

#define DEV_CONFIG_FILE_PATH    "/etc/device.properties"
#define DISK_INFO_FILE_PATH     "/opt/logs/diskinfo.log"
#define FILE_MODE "r"

/*****************************************************************************
 * LOCAL TYPES
 *****************************************************************************/

/*****************************************************************************
 * LOCAL FUNCTION PROTOTYPES
 *****************************************************************************/

static int disk_supported(void);
static int smart_enabled(const char* logfile);
static int disk_health_status(const char* logfile);
static int setReturnData(int status, json_t** param);

/*****************************************************************************
 * LOCAL VARIABLE DECLARATIONS
 *****************************************************************************/

/*****************************************************************************
 * FUNCTION DEFINITIONS
 *****************************************************************************/

/*****************************************************************************
 * LOCAL FUNCTIONS
 *****************************************************************************/

static int disk_supported(void)
{
    return WA_UTILS_FILEOPS_OptionSupported(DEV_CONFIG_FILE_PATH, FILE_MODE, HDD_OPTION_STR, HDD_AVAILABLE_STR);
}

static int smart_enabled(const char* logfile)
{
    return WA_UTILS_FILEOPS_OptionSupported(logfile, FILE_MODE, SMART_OPTION_STR, SMART_ENABLED_STR);
}

static int disk_health_status(const char* logfile)
{
    return WA_UTILS_FILEOPS_OptionSupported(logfile, FILE_MODE, HDD_HEALTH_OPTION_STR, HDD_HEALTH_OK_STR);
}

static int setReturnData(int status, json_t **param)
{
    if(param == NULL)
        return status;

    switch (status)
    {
        case WA_DIAG_ERRCODE_SUCCESS:
           *param = json_string("S.M.A.R.T. health status good.");;
           break;

        case WA_DIAG_ERRCODE_FAILURE:
           *param = json_string("S.M.A.R.T. health status error.");
           break;

        case WA_DIAG_ERRCODE_HDD_STATUS_MISSING:
            *param = json_string("S.M.A.R.T. health status unavailable.");
            break;

        case WA_DIAG_ERRCODE_NOT_APPLICABLE:
            *param = json_string("Not applicable.");
            break;

        case WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR:
            *param = json_string("Internal test error.");
            break;

        case WA_DIAG_ERRCODE_CANCELLED:
            *param = json_string("Test cancelled.");
            break;

        default:
            break;
    }

    return status;
}

/*****************************************************************************
 * EXPORTED FUNCTIONS
 *****************************************************************************/

int WA_DIAG_HDD_status(void *instanceHandle, void *initHandle, json_t **params)
{
    int ret = -1;
    json_t * jsonConfig = NULL;    
    const char * logfile = DISK_INFO_FILE_PATH;

    json_decref(*params); //not used
    *params = NULL;

    jsonConfig = ((WA_DIAG_proceduresConfig_t*)initHandle)->config;
    
    /* Use log file name from config if present */
    if (jsonConfig)
        json_unpack(jsonConfig, "{ss}", "logfile", &logfile);
    
    ret = disk_supported();
    if(ret < 0)
    {
        if(WA_OSA_TaskCheckQuit())
        {
            WA_DBG("disk_supported: test cancelled\n");
            return setReturnData(WA_DIAG_ERRCODE_CANCELLED, params);
        }

        WA_DBG("Device configuration unknown\n");
        return setReturnData(WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR, params);
    }
    else if(ret == 0)
    {
        WA_DBG("Device does not have hdd\n");
        return setReturnData(WA_DIAG_ERRCODE_NOT_APPLICABLE, params);
    }
    WA_DBG("HDD supported.\n");
    
    ret = smart_enabled(logfile);
    if(ret < 0 )
    {
        if(WA_OSA_TaskCheckQuit())
        {
            WA_DBG("smart_enabled: test cancelled\n");
            return setReturnData(WA_DIAG_ERRCODE_CANCELLED, params);
        }

        WA_DBG("S.M.A.R.T. health status missing\n");
        //even if SMART status unknown want to get latest available result
    }
    else if(ret == 0)
    {
        WA_DBG("S.M.A.R.T. currently disabled\n");
        //even if SMART disabled want to get latest available result
    }
    else
    {
        WA_DBG("S.M.A.R.T. currently enabled\n");
    }

    ret = disk_health_status(logfile);
    if(ret < 0)
    {
        if(WA_OSA_TaskCheckQuit())
        {
            WA_DBG("disk_health_status: test cancelled\n");
            return setReturnData(WA_DIAG_ERRCODE_CANCELLED, params);
        }

        WA_DBG("S.M.A.R.T. health status missing\n");
        return setReturnData(WA_DIAG_ERRCODE_HDD_STATUS_MISSING, params);
    }
    else if (ret == 0)
    {
        WA_DBG("Last S.M.A.R.T. health status bad\n");
        return setReturnData(WA_DIAG_ERRCODE_FAILURE, params);
    }

    WA_DBG("Last S.M.A.R.T. health status good\n");
    return setReturnData(WA_DIAG_ERRCODE_SUCCESS, params);
}

/* End of doxygen group */
/*! @} */

/* EOF */
