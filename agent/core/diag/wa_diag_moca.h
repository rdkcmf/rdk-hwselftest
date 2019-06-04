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
 * @file wa_diag_mcard.h
 *
 * @brief Diagnostic functions for MoCA - interface
 */

/** @addtogroup WA_DIAG_MOCA
 *  @{
 */

#ifndef WA_DIAG_MOCA_H
#define WA_DIAG_MOCA_H


/*****************************************************************************
 * STANDARD INCLUDE FILES
 *****************************************************************************/

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/
#include "wa_json.h"
#include "wa_snmp_client.h"

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

/*****************************************************************************
 * EXPORTED FUNCTIONS
 *****************************************************************************/

/**
 * Check status of MoCA interface.
 *
 * @retval 0 success - MoCA enabled, interface up, client(s) present.
 * @retval 1 failure - MoCA disabled and/or interface down.
 * @retval 2 failure - MoCA enabled, iterface up, no client(s) found.
 * @retval -1 error - unknown device configuration / internal test error.
 */
extern int WA_DIAG_MOCA_status(void* instanceHandle, void *initHandle, json_t **params);

extern int getMocaIfRFChannelFrequency(int *group, int index, WA_UTILS_SNMP_Resp_t *value, WA_UTILS_SNMP_ReqType_t reqType);
extern int getMocaIfNetworkController(int group, int index, WA_UTILS_SNMP_Resp_t *value, WA_UTILS_SNMP_ReqType_t reqType);
extern int getMocaIfTransmitRate(int group, int index, WA_UTILS_SNMP_Resp_t *value, WA_UTILS_SNMP_ReqType_t reqType);
extern int getMocaNodeSNR(int group, int index, WA_UTILS_SNMP_Resp_t *value, WA_UTILS_SNMP_ReqType_t reqType);

/*****************************************************************************
 * LOCAL FUNCTIONS
 *****************************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* WA_DIAG_MCARD_H */

/* End of doxygen group */
/*! @} */

/* EOF */
