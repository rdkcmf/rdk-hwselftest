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
 * @file wa_diag_wifi.h
 *
 * @brief Diagnostic functions for WIFI - interface
 */

/** @addtogroup WA_DIAG_WIFI
 *  @{
 */

#ifndef WA_DIAG_WIFI_H
#define WA_DIAG_WIFI_H


/*****************************************************************************
 * STANDARD INCLUDE FILES
 *****************************************************************************/

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/
#include "wa_json.h"
#include "hostIf_tr69ReqHandler.h"
#include "wifiSrvMgrIarmIf.h"

#ifdef __cplusplus
extern "C"
{
#endif

/*****************************************************************************
 * EXPORTED DEFINITIONS
 *****************************************************************************/

/*****************************************************************************
 * EXPORTED TYPES
 *****************************************************************************/

/*****************************************************************************
 * EXPORTED VARIABLES
 *****************************************************************************/
char wifiIARM[TR69HOSTIFMGR_MAX_PARAM_LEN];
/*****************************************************************************
 * EXPORTED FUNCTIONS
 *****************************************************************************/

/**
 * WIFI SSID STATUS
 */
extern int WA_DIAG_WIFI_status(void* instanceHandle, void *initHandle, json_t **params);
int getWifiSSID_IARM(char **result);
int getWifiMacAddress_IARM(char **result);
int getWifiOperFrequency_IARM(char **result);
int getWifiSignalStrength_IARM(int *result);

/*****************************************************************************
 * LOCAL FUNCTIONS
 *****************************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* WA_DIAG_WIFI_H */

/* End of doxygen group */
/*! @} */

/* EOF */
