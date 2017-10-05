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
 * @file wa_init.h
 */

/** @addtogroup WA_INIT
 *  @{
 */

#ifndef WA_INIT_H
#define WA_INIT_H

/*****************************************************************************
 * STANDARD INCLUDE FILES
 *****************************************************************************/

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/
#include "wa_comm.h"
#include "wa_diag.h"

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
extern void *WA_INIT_IncomingQ;

/*****************************************************************************
 * EXPORTED FUNCTIONS
 *****************************************************************************/

/**
 * This function is called as a first thing during system
 * initialization. It is responsible for initializing
 * the agent internal data structures.
 *
 * @brief Initializes the agent.
 *
 * @param adapters the array of comm adapters to attach to the system
 * @param diags the array of diag procedures to attach to the system
 *
 * @retval 0  The agent was successfully initialized.
 * @retval -1 The agent initialization failed.
 */

extern int WA_INIT_Init(WA_COMM_adaptersConfig_t * const adapters,
        WA_DIAG_proceduresConfig_t * const diags);


/**
 * This function must be called during system uninitialization.
 * It is responsible for the uninitialization of the agent
 * internal data structures.
 *
 * @brief This function un-initializes the agent.
 *
 * @retval 0  The agent was successfully uninitialized.
 * @retval -1 The agent un-initialization failed.
 */
extern int WA_INIT_Exit(void);

#ifdef __cplusplus
}
#endif

#endif /* WA_INIT_H */
