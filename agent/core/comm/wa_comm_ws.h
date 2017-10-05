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
 * @file wa_comm_ws.h
 */

/** @addtogroup WA_COMM_WS
 *  @{
 */

#ifndef WA_COMM_WS_H
#define WA_COMM_WS_H

/*****************************************************************************
 * STANDARD INCLUDE FILES
 *****************************************************************************/

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/
#include "wa_comm.h"
#include "wa_json.h"

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
 * Initialize the comm ws module.
 *
 * @param config the configuration
 *
 * @returns handle to provide in \c WA_COMM_WS_Exit()
 * @retval null error
 */
extern void * WA_COMM_WS_Init(WA_COMM_adaptersConfig_t *config);

/**
 * Deinitialize the comm ws module.
 *
 * @param handle a handle provided by \c WA_COMM_WS_Init()
 *
 * @retval 0 success.
 * @retval -1 error
 */
extern int WA_COMM_WS_Exit(void *handle);

/**
 * A callback to receive message from agent.
 *
 * @param cookie a cookie that is provided with \c WA_COMM_Register()
 * @param json the message body
 *
 * @retval 0 success.
 * @retval -1 error
 */
extern int WA_COMM_WS_Callback(void *cookie, json_t *json);

#ifdef __cplusplus
}
#endif

#endif /* _WA_COMM_WS_H_ */
