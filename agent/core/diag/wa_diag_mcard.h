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
 * @brief Diagnostic functions for M-Card - interface
 */

/** @addtogroup WA_DIAG_MCARD
 *  @{
 */

#ifndef WA_DIAG_MCARD_H
#define WA_DIAG_MCARD_H


/*****************************************************************************
 * STANDARD INCLUDE FILES
 *****************************************************************************/

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/
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
 * Read mcard id and verify certificate status.
 *
 * @retval 0 success - id read, certificate valid.
 * @retval 1 failure - could not read card id.
 * @retval 2 failure - invalid certificate.
 * @retval -1 error - unknown device configuration / internal test error.
 */
extern int WA_DIAG_MCARD_status(void* instanceHandle, void *initHandle, json_t **params);

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
