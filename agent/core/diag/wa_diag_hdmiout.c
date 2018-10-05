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
 * @file wa_diag_hdmiout.c
 *
 * @brief Diagnostic functions for HDMI Output port - implementation
 */

/** @addtogroup WA_DIAG_HDMIOUT
 *  @{
 */

/*****************************************************************************
 * STANDARD INCLUDE FILES
 *****************************************************************************/
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/

#include "wa_diag.h"
#include "wa_debug.h"

/* rdk interfaces */

#include "wa_iarm.h"
#include "wa_mgr.h"
#include "wa_vport.h"

/* module interface */
#include "wa_diag_hdmiout.h"

/*****************************************************************************
 * RDK-SPECIFIC INCLUDE FILES
 *****************************************************************************/

/*****************************************************************************
 * GLOBAL VARIABLE DEFINITIONS
 *****************************************************************************/

/*****************************************************************************
 * LOCAL DEFINITIONS
 *****************************************************************************/
#define IARM_BUS_VPCLIENT_NAME "RDK_SelfTest_VPC"

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

/*****************************************************************************
 * LOCAL FUNCTIONS
 *****************************************************************************/

static int setReturnData(int status, json_t **param)
{
    if(param == NULL)
        return status;

    switch (status)
    {
        case WA_DIAG_ERRCODE_SUCCESS:
           *param = json_string("HDMI Output good.");
           break;

        case WA_DIAG_ERRCODE_HDMI_NO_DISPLAY:
           *param = json_string("HDMI cable not detected.");
           break;

        case WA_DIAG_ERRCODE_HDMI_NO_HDCP:
            *param = json_string("HDCP authentication issue.");
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

int WA_DIAG_HDMIOUT_status(void *instanceHandle, void *initHandle, json_t **params)
{
    int ret = -1;
    int hdcpState = -1;
    int portId = -1;

    json_decref(*params); //not used
    *params = NULL;

    if(WA_UTILS_IARM_Connect())
        return setReturnData(WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR, params);

    WA_UTILS_MGR_Init();

    portId = WA_UTILS_VPORT_GetHdmiPortId();

    if(portId == WA_UTILS_VPORT_ID_UNKNOWN)
    {
        ret = setReturnData(WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR, params);
        goto end;
    }

    if(portId == WA_UTILS_VPORT_ID_CHECK_CANCELLED)
    {
        WA_DBG("WA_UTILS_VPORT_GetHdmiPortId: test cancelled\n");
        ret = setReturnData(WA_DIAG_ERRCODE_CANCELLED, params);
        goto end;
    }

    if(WA_UTILS_VPORT_IsDisplayConnected(portId))
    {
        hdcpState = WA_UTILS_VPORT_IsHdcpEnabled(portId);
        switch(hdcpState)
        {
            case WA_UTILS_VPORT_HDCP_CHECK_CANCELLED:
                WA_DBG("WA_UTILS_VPORT_IsHdcpEnabled: test cancelled\n");
                ret = setReturnData(WA_DIAG_ERRCODE_CANCELLED, params);
                break;

            case WA_UTILS_VPORT_HDCP_ENABLED:
                ret = setReturnData(WA_DIAG_ERRCODE_SUCCESS, params);
                break;

            case WA_UTILS_VPORT_HDCP_DISABLED:
                ret = setReturnData(WA_DIAG_ERRCODE_HDMI_NO_HDCP, params);
                break;

            default:
                ret = setReturnData(WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR, params);
                break;
        }
    }
    else
    {
        ret = setReturnData(WA_DIAG_ERRCODE_HDMI_NO_DISPLAY, params);
    }

end:
    WA_UTILS_MGR_Term();

    if(WA_UTILS_IARM_Disconnect())
    {
        ret = setReturnData(WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR, params);
    }

    return ret;
}

/* End of doxygen group */
/*! @} */

/* EOF */
