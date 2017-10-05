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
 * @file wa_diag_sysinfo.h
 *
 * @brief System Information - interface
 */

/** @addtogroup WA_DIAG_SYSINFO
 *  @{
 */

#ifndef WA_DIAG_SYSINFO_H
#define WA_DIAG_SYSINFO_H


/*****************************************************************************
 * STANDARD INCLUDE FILES
 *****************************************************************************/

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/
#include "wa_json.h"
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

/*****************************************************************************
 * EXPORTED FUNCTIONS
 *****************************************************************************/

/**
 * Read system information.
 *
 * @retval 0 success.
 * @retval 1 error
 */
extern int WA_DIAG_SYSINFO_Info(void* instanceHandle, void *initHandle, json_t **params);

/**
 * Returns platform specific information (it's a platform specific implementation).
 *
 * @returns Information object
 * @retval null on error
 */
extern json_t *SYSINFO_Get();

/*****************************************************************************
 * LOCAL FUNCTIONS
 *****************************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* WA_DIAG_SYSINFO_H */

/* End of doxygen group */
/*! @} */

/* EOF */
