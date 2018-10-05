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

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/

#include "wa_diag.h"
#include "wa_debug.h"

#include "wa_diag_file.h"

/* module interface */
#include "wa_diag_flash.h"

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

static const size_t defaultTotalSize = 512 * 1024;
static const char * defaultFileName = "/mnt/nvram/diagsys-flash-test-file";

/*****************************************************************************
 * FUNCTION DEFINITIONS
 *****************************************************************************/

/*****************************************************************************
 * EXPORTED FUNCTIONS
 *****************************************************************************/

int WA_DIAG_FLASH_status(void* instanceHandle, void *initHandle, json_t **pJsonInOut)
{
    int result = WA_DIAG_ERRCODE_FAILURE;
    json_t *config;
    int applicable;

    json_decref(*pJsonInOut);
    *pJsonInOut = NULL;

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

    /* Perform the test */
    result = WA_DIAG_FileTest(defaultFileName, defaultTotalSize,
        ((WA_DIAG_proceduresConfig_t*)initHandle)->config, pJsonInOut);

    return result;
}

/*****************************************************************************
 * LOCAL FUNCTIONS
 *****************************************************************************/

/* End of doxygen group */
/*! @} */

