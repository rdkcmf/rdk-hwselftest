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
 */

/** @addtogroup WA_LOG
 *  @{
 */

#ifndef WA_LOG_H_
#define WA_LOG_H_

/*****************************************************************************
 * STANDARD INCLUDE FILES
 *****************************************************************************/
#include <stdbool.h>
#include <time.h>

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/
#include <rdk_debug.h>
#include "wa_json.h"

#ifdef __cplusplus
extern "C"
{
#endif

/*****************************************************************************
 * EXPORTED DEFINITIONS
 *****************************************************************************/
#define RAW_MESSAGE 1
#define T2_MESSAGE  2 /* RDK-31352 To report events for Telemetry2.0 */
#define MESSAGE     3
#define CLIENT_LOG(format, ...) WA_LOG_Client(MESSAGE, format, ## __VA_ARGS__)

/*****************************************************************************
 * FUNCTION DEFINITIONS
 *****************************************************************************/
bool WA_LOG_Init();

void WA_LOG_Client(int level, const char * format, ...);

int WA_LOG_Log(json_t **pJson);

char *WA_LOG_GetTimestampStr(time_t time, char *buffer, int buffer_size);
int WA_LOG_GetTimestampTime(const char *buffer, time_t *time);


#ifdef __cplusplus
}
#endif

#endif

/* End of doxygen group */
/*! @} */
