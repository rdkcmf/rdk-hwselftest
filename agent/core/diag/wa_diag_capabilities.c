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
 * @file wa_diag_capabilities.c
 *
 * @brief Agent capabilities - implementation
 */

/** @addtogroup WA_DIAG_PREV_RESULTS
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
#include "wa_diag_capabilities.h"
#include "wa_json.h"
#include "wa_config.h"
#include "wa_diag_rf4ce.h"

/*****************************************************************************
 * LOCAL DEFINITIONS
 *****************************************************************************/

/*****************************************************************************
 * FUNCTION DEFINITIONS
 *****************************************************************************/
int WA_DIAG_CAPABILITIES_Info(void *instanceHandle, void *initHandle, json_t **params)
{
    WA_ENTER("WA_DIAG_CAPABILITIES_Info(instanceHandle=%p, initHandle=%p, params=%p)\n",
             instanceHandle, initHandle, params);

    int status = 1;
    int rf4ce = 1;

    json_decref(*params); //not used

    const WA_DIAG_proceduresConfig_t *diags = WA_CONFIG_GetDiags();

    if (isRF4CEPaired() == -1)
    {
        rf4ce = 0;
    }

    if (diags)
    {
        json_t *jroot = json_object();
        if (jroot)
        {
            json_t *jdiagsarray = json_array();
            if (jdiagsarray)
            {
                for (const WA_DIAG_proceduresConfig_t *diag = &diags[0]; diag && diag->name; diag++)
                {
                    if (strstr(diag->name, "_status")) // list only tests
                    {
                        if ((rf4ce && !strcmp(diag->name, "ir_status")) || (!rf4ce && !strcmp(diag->name, "rf4ce_status")))
                        {
                            continue;
                        }
                        json_array_append_new(jdiagsarray, json_string(diag->name));
                    }
                }

                json_object_set_new(jroot, "diags", jdiagsarray);
                (*params) = jroot;
                status = 0;
            }
            else
                json_decref(jroot);
        }

        if (status)
            WA_ERROR("WA_DIAG_CAPABILITIES_Info(): JSON allocation failure.\n");
    }
    else
        WA_ERROR("WA_DIAG_CAPABILITIES_Info(): WA_CONFIG_GetDiags() failed.\n");

    WA_RETURN("WA_DIAG_CAPABILITIES_Info(): %d\n", status);

    return status;
}

/* End of doxygen group */
/*! @} */

/* EOF */
