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
 * @file wa_diag_capabilities.h
 *
 * @brief Previous results - interface
 */

/** @addtogroup WA_DIAG_CAPABILITIES_H
 *  @{
 */

#ifndef WA_DIAG_CAPABILITIES_H
#define WA_DIAG_CAPABILITIES_H

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/
#include "wa_json.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * Provide agent capabilities.
 *
 * @retval 0 success.
 * @retval 1 error
 */
int WA_DIAG_CAPABILITIES_Info(void* instanceHandle, void *initHandle, json_t **params);

/*****************************************************************************
 * LOCAL FUNCTIONS
 *****************************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* WA_DIAG_CAPABILITIES_H */

/* End of doxygen group */
/*! @} */

/* EOF */
