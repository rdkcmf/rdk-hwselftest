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
 * @brief Diagnostic functions for DRAM - implementation
 */

/** @addtogroup WA_DIAG_DRAM
 *  @{
 */

/*****************************************************************************
 * STANDARD INCLUDE FILES
 *****************************************************************************/
#define _GNU_SOURCE

#include <stdio.h>

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/

#include "wa_diag.h"
#include "wa_debug.h"
#include "wa_fileops.h"

#include "wa_diag_file.h"

/* module interface */
#include "wa_diag_dram.h"

/*****************************************************************************
 * GLOBAL VARIABLE DEFINITIONS
 *****************************************************************************/

/*****************************************************************************
 * LOCAL DEFINITIONS
 *****************************************************************************/

/*****************************************************************************
 * LOCAL TYPES
 *****************************************************************************/

/*****************************************************************************
 * LOCAL FUNCTION PROTOTYPES
 *****************************************************************************/

/*****************************************************************************
 * LOCAL VARIABLE DECLARATIONS
 *****************************************************************************/

static const size_t defaultTotalSize = 4 * 1024 * 1024;
static const char * defaultFileName = "diagsys-dram-test-file";
static const char * defaultRamDiskPath = "/tmp";

/*****************************************************************************
 * FUNCTION DEFINITIONS
 *****************************************************************************/

/*****************************************************************************
 * EXPORTED FUNCTIONS
 *****************************************************************************/

int WA_DIAG_DRAM_status(void* instanceHandle, void *initHandle, json_t **pJsonInOut)
{
    int result = WA_DIAG_ERRCODE_FAILURE;
    const char * ramDiskPath = NULL;
    json_t * config = NULL;
    char *filepath = NULL;
    int applicable;

    json_decref(*pJsonInOut);
    *pJsonInOut = NULL;

        /* Determine if the test is applicable: */
    config = ((WA_DIAG_proceduresConfig_t*)initHandle)->config;
    if(config && !json_unpack(config, "{sb}", "applicable", &applicable))
    {
        if(!applicable)
        {
            WA_INFO("dram_status: Not applicable\n");
            *pJsonInOut = json_string("Not applicable.");
            return WA_DIAG_ERRCODE_NOT_APPLICABLE;
        }
    }

    ramDiskPath = WA_UTILS_FILEOPS_OptionFind("/etc/include.properties", "RAMDISK_PATH=");

        if(WA_OSA_TaskCheckQuit())
        {
                free((void *)ramDiskPath);
                WA_DBG("dram_status: Test cancelled\n");
                *pJsonInOut = json_string("Test cancelled.");
                return WA_DIAG_ERRCODE_CANCELLED;
        }

    result = asprintf(&filepath, "%s/%s", ramDiskPath ? ramDiskPath : defaultRamDiskPath, defaultFileName);
    free((void *)ramDiskPath);
    if (result == -1)
    {
        *pJsonInOut = json_string("Internal test error.");
        return WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR;
    }

    result = WA_DIAG_FileTest((const char *)filepath, defaultTotalSize, ((WA_DIAG_proceduresConfig_t*) initHandle)->config, pJsonInOut);

    free(filepath);

    WA_RETURN("dram_status: returns \"%d\"\n", result);
    return result;
}

/*****************************************************************************
 * LOCAL FUNCTIONS
 *****************************************************************************/

/* End of doxygen group */
/*! @} */

