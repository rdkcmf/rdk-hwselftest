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
 * @file wa_vport.h
 *
 * @brief This file contains interface functions for rdk video output port module.
 */

/** @addtogroup WA_UTILS_VPORT
 *  @{
 */

#ifndef WA_UTILS_VPORT_H
#define WA_UTILS_VPORT_H


/*****************************************************************************
 * STANDARD INCLUDE FILES
 *****************************************************************************/

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/
#ifdef __cplusplus
extern "C"
{
#endif

#include "wa_json.h"

/*****************************************************************************
 * EXPORTED DEFINITIONS
 *****************************************************************************/
#define WA_UTILS_VPORT_ID_CHECK_CANCELLED   (-2)
#define WA_UTILS_VPORT_ID_UNKNOWN           (-1)

#define WA_UTILS_VPORT_HDCP_CHECK_CANCELLED (-2)
#define WA_UTILS_VPORT_HDCP_UNKNOWN         (-1)
#define WA_UTILS_VPORT_HDCP_ENABLED         (0)
#define WA_UTILS_VPORT_HDCP_DISABLED        (1)

/*****************************************************************************
 * EXPORTED TYPES
 *****************************************************************************/

/*****************************************************************************
 * EXPORTED VARIABLES
 *****************************************************************************/

/*****************************************************************************
 * EXPORTED FUNCTIONS
 *****************************************************************************/

/**
 * Find id of the first HDMI port on the list of VideoOutputPorts.
 *
 * @retval id on success.
 * @retval -1 otherwise.
 */
extern int WA_UTILS_VPORT_GetHdmiPortId(void);

/**
 * Check if there is any display connected to the VideoOutputPort.
 *
 * @retval 0 on success.
 * @retval 1 otherwise.
 */
extern int WA_UTILS_VPORT_IsDisplayConnected(int id);

/**
 * Check if HDCP is enabled for selected VideoOutputPort.
 *
 * @retval 0 on success.
 * @retval 1 otherwise.
 */
extern int WA_UTILS_VPORT_IsHdcpEnabled(int id);

/*****************************************************************************
 * LOCAL FUNCTIONS
 *****************************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* WA_UTILS_VPORT_H */

/* End of doxygen group */
/*! @} */

/* EOF */
