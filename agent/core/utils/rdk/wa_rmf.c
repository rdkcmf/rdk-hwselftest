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
 * @brief General RMF-related utilities - implementation.
 */

/** @addtogroup WA_UTILS_RMF
 *  @{
 */

/*****************************************************************************
 * STANDARD INCLUDE FILES
 *****************************************************************************/
#include <stdbool.h>

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/
#include "wa_rmf.h"
#include "wa_fileops.h"
#include "wa_debug.h"

/*****************************************************************************
 * RDK-SPECIFIC INCLUDE FILES
 *****************************************************************************/

/*****************************************************************************
 * GLOBAL VARIABLE DEFINITIONS
 *****************************************************************************/

/*****************************************************************************
 * LOCAL DEFINITIONS
 *****************************************************************************/
#define STREAMER_PORT_FILE      WA_UTILS_RMF_CONFIG_FILE
#define STREAMER_PORT_OPTION    "FEATURE.MRDVR.MEDIASTREAMER.STREAMING.PORT="
#define STREAMER_PORT_DEFAULT 8080

#define TUNER_COUNT_OPTION      "MPE.SYS.NUM.TUNERS="

/*****************************************************************************
 * LOCAL FUNCTION PROTOTYPES
 *****************************************************************************/

/*****************************************************************************
 * LOCAL VARIABLE DECLARATIONS
 *****************************************************************************/

/*****************************************************************************
 * FUNCTION DEFINITIONS
 *****************************************************************************/

int WA_UTILS_RMF_GetMediastreamerPort(void)
{
    int result = 0;

    char * optString = WA_UTILS_FILEOPS_OptionFind(STREAMER_PORT_FILE, STREAMER_PORT_OPTION);
    if (optString)
    {
        result = atoi(optString);
        WA_DBG("WA_UTILS_RMF_GetMediaserverPort(): Streamer port from RMF config: %i (%s)\n", result, optString);
        free(optString);

        if (result > 0)
        {
            return result;
        }
    }

    WA_DBG("WA_UTILS_RMF_GetMediaserverPort(): Using default streamer port: %i\n", STREAMER_PORT_DEFAULT);
    return STREAMER_PORT_DEFAULT;
}

bool WA_UTILS_RMF_GetNumberOfTuners(unsigned int * pResult)
{
    if (!pResult)
    {
        return false;
    }

    char * optString = WA_UTILS_FILEOPS_OptionFind(WA_UTILS_RMF_CONFIG_FILE, TUNER_COUNT_OPTION);
    if (optString)
    {
        int value = atoi(optString);
        if (value < 0)
        {
            value = 0;
        }
        WA_DBG("WA_UTILS_RMF_GetNumberOfTuners(): Tuner count from RMF config: %i (%s)\n", value, optString);
        free(optString);
        *pResult = value;
        return true;
    }

    return false;
}

/*****************************************************************************
 * EXPORTED FUNCTIONS
 *****************************************************************************/

/*****************************************************************************
 * LOCAL FUNCTIONS
 *****************************************************************************/

/* End of doxygen group */
/*! @} */

/* EOF */
