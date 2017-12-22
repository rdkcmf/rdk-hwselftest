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
 * @file wa_config.h
 *
 * @brief Interface of agent configuration functionality.
 *
 */

/** @addtogroup WA_CONFIG
 *  @{
 */

#ifndef WA_CONFIG_H
#define WA_CONFIG_H

/*****************************************************************************
 * STANDARD INCLUDE FILES
 *****************************************************************************/

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/
#include "wa_config.h"
#include "wa_diag.h"
#include "wa_comm.h"
#include "wa_json.h"

#ifdef __cplusplus
extern "C"
{
#endif

/*****************************************************************************
 * EXPORTED TYPES
 *****************************************************************************/

/*****************************************************************************
 * EXPORTED FUNCTIONS
 *****************************************************************************/

/**
 * @brief Initialises internal configuration structures.
 *
 * @returns Operation status.
 * @retval 0 for success, non-zero otherwise
 */
int WA_CONFIG_Init();

/**
 * @brief Frees internal configuration structures.
 *
 * @returns Operation status.
 * @retval 0 for success, non-zero otherwise
 */
int WA_CONFIG_Exit();

/**
 * @brief Retrieve diags.
 *
 * @returns Diags configuration.
 */
const WA_DIAG_proceduresConfig_t *WA_CONFIG_GetDiags();

/**
 * @brief Retrieve adapters.
 *
 * @returns Adapters configuration.
 */
const WA_COMM_adaptersConfig_t *WA_CONFIG_GetAdapters();

#ifdef __cplusplus
}
#endif

#endif /* WA_CONFIG_H */

/* EOF */
