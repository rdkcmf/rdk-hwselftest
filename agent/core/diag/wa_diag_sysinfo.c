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
 * @file wa_diag_sysinfo.c
 *
 * @brief System Information - implementation
 */

/** @addtogroup WA_DIAG_SYSINFO
 *  @{
 */

/*****************************************************************************
 * STANDARD INCLUDE FILES
 *****************************************************************************/
#include <string.h>

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/

#include "wa_debug.h"
#include "wa_fileops.h"
#include "wa_version.h"

/* rdk specific */
#include "wa_iarm.h"
#include "wa_mfr.h"

/* module interface */
#include "wa_diag_sysinfo.h"

/*****************************************************************************
 * GLOBAL VARIABLE DEFINITIONS
 *****************************************************************************/

/*****************************************************************************
 * LOCAL DEFINITIONS
 *****************************************************************************/

#define MAX_LINE 100

/*****************************************************************************
 * LOCAL TYPES
 *****************************************************************************/
typedef struct {
    size_t size;
    char *value;
}SysinfoParam_t;

/*****************************************************************************
 * LOCAL FUNCTION PROTOTYPES
 *****************************************************************************/
static json_t *sysinfo_Get();

/*****************************************************************************
 * LOCAL VARIABLE DECLARATIONS
 *****************************************************************************/

/*****************************************************************************
 * FUNCTION DEFINITIONS
 *****************************************************************************/
int WA_DIAG_SYSINFO_Info(void *instanceHandle, void * initHandle, json_t **params)
{
    WA_ENTER("WA_DIAG_SYSINFO_Info(instanceHandle=%p initHandle=%p params=%p)\n",
            instanceHandle, initHandle, params);

    json_decref(*params); //not used

    *params = sysinfo_Get();

    WA_RETURN("WA_DIAG_SYSINFO_Info(): %d\n", *params == NULL);

    return (*params == NULL);
}


/*****************************************************************************
 * LOCAL FUNCTIONS
 *****************************************************************************/

static json_t *sysinfo_Get()
{
    int i;
    json_t *json = NULL;
    SysinfoParam_t params[3];
    char *rdkver;

    WA_ENTER("SYSINFO_Get()\n");
    
    memset(&params, 0, sizeof(params));

    if(WA_UTILS_IARM_Connect())
        return json;
    
    rdkver = WA_UTILS_FILEOPS_OptionFind("/version.txt", "imagename:");

    for (i = 0; i < sizeof(params)/sizeof(params[0]); i++)
    {
        if (WA_UTILS_MFR_ReadSerializedData(i, &params[i].size, &params[i].value) != 0)
            WA_ERROR("sysinfo_Get(): WA_UTILS_MFR_ReadSerializedData(%i) failed\n", i);
    }
    
    json = json_pack("{s:s,s:s,s:s,s:s,s:s}",
       "Vendor", (!params[WA_UTILS_MFR_PARAM_MANUFACTURER].size? "N/A" : params[WA_UTILS_MFR_PARAM_MANUFACTURER].value),
       "Model",  (!params[WA_UTILS_MFR_PARAM_MODEL].size? "N/A" : params[WA_UTILS_MFR_PARAM_MODEL].value),
       "Serial", (!params[WA_UTILS_MFR_PARAM_SERIAL].size? "N/A" : params[WA_UTILS_MFR_PARAM_SERIAL].value),
       "RDK",    (rdkver ? rdkver : "N/A"),
       "AgentVersion", WA_VERSION);
       
    for(i = 0; i < sizeof(params)/sizeof(params[0]); i++)
       if(params[i].value != NULL)
           free(params[i].value);

    WA_UTILS_IARM_Disconnect();

    WA_RETURN("SYSINFO_Info(): %p\n", json);

    return json;
}



/* End of doxygen group */
/*! @} */

/* EOF */
