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
 * @file wa_vport.c
 *
 * @brief This file contains interface functions for rdk video output port module.
 */

/** @addtogroup WA_UTILS_VPORT
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
#include "wa_osa.h"
#include "wa_iarm.h"
#include "wa_mgr.h"
#include "wa_vport.h"
#include "wa_debug.h"

/*****************************************************************************
 * RDK-SPECIFIC INCLUDE FILES
 *****************************************************************************/
#include "host.hpp"
#include "videoOutputPort.hpp"
#include "videoOutputPortType.hpp"
#include "videoResolution.hpp"
#include "manager.hpp"

/*****************************************************************************
 * GLOBAL VARIABLE DEFINITIONS
 *****************************************************************************/

/*****************************************************************************
 * LOCAL DEFINITIONS
 *****************************************************************************/

/*****************************************************************************
 * LOCAL FUNCTION PROTOTYPES
 *****************************************************************************/

/*****************************************************************************
 * LOCAL VARIABLE DECLARATIONS
 *****************************************************************************/

/*****************************************************************************
 * FUNCTION DEFINITIONS
 *****************************************************************************/

/*****************************************************************************
 * LOCAL FUNCTIONS
 *****************************************************************************/

/*****************************************************************************
 * EXPORTED FUNCTIONS
 *****************************************************************************/

int WA_UTILS_VPORT_GetHdmiPortId(void)
{

    int id = WA_UTILS_VPORT_ID_UNKNOWN;

    device::List<device::VideoOutputPort> vPorts = device::Host::getInstance().getVideoOutputPorts();
    for (size_t i = 0; i < vPorts.size() && !WA_OSA_TaskCheckQuit(); i++)
    {
        device::VideoOutputPort &vPort = vPorts.at(i);

        if((vPort.getType() == dsVIDEOPORT_TYPE_HDMI) && 
           (vPort.isEnabled() == true))
        {
            id = vPort.getId();
            break;
        }
    }

    if(WA_OSA_TaskCheckQuit())
    {
        id = WA_UTILS_VPORT_ID_CHECK_CANCELLED;
    }
    return id;
}

int WA_UTILS_VPORT_IsDisplayConnected(int id)
{
    return device::Host::getInstance().getVideoOutputPort(id).isDisplayConnected();
}

int WA_UTILS_VPORT_IsHdcpEnabled(int id)
{
    int retCode = WA_UTILS_VPORT_HDCP_UNKNOWN;
    int status = dsHDCP_STATUS_INPROGRESS;

    device::VideoOutputPort port =  device::Host::getInstance().getVideoOutputPort(id);

    while (status == dsHDCP_STATUS_INPROGRESS && !WA_OSA_TaskCheckQuit())
    {
        status = port.getHDCPStatus();

        switch(status)
        {
            case dsHDCP_STATUS_AUTHENTICATED:
                retCode = WA_UTILS_VPORT_HDCP_ENABLED;
                break;

            default:
                retCode = WA_UTILS_VPORT_HDCP_DISABLED;
                break;
        }

        /* Sleep for 500 ms */
        WA_OSA_TaskSleep(500);
    }

    if(WA_OSA_TaskCheckQuit())
    {
        WA_DBG("WA_UTILS_VPORT_IsHdcpEnabled: CANCELLED\n");
        retCode = WA_UTILS_VPORT_HDCP_CHECK_CANCELLED;
    }

    return retCode;
}

/* End of doxygen group */
/*! @} */

/* EOF */
