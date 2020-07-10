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
 * @file wa_diag_hdd.h
 *
 * @brief Diagnostic functions for HDD - interface
 */

/** @addtogroup WA_DIAG_HDD
 *  @{
 */

#ifndef WA_DIAG_HDD_H
#define WA_DIAG_HDD_H


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
typedef struct smartAttributes_t
{
	char *attr_id;
	int raw_value;
} smartAttributes_t;

/*****************************************************************************
 * EXPORTED VARIABLES
 *****************************************************************************/

/*****************************************************************************
 * EXPORTED FUNCTIONS
 *****************************************************************************/

/**
 * Read common hdd identity strings (s/n, fw, model).
 *
 * @retval 0 success - hdd S.M.A.R.T overall health status GOOD.
 * @retval 1 failure - hdd S.M.A.R.T overall health status NOT GOOD.
 * @retval 2 failure - hdd S.M.A.R.T overall health status unavailable.
 * @retval -1 error - unknown device configuration / internal test error.
 */
extern int WA_DIAG_HDD_status(void* instanceHandle, void *initHandle, json_t **params);

/*****************************************************************************
 * LOCAL FUNCTIONS
 *****************************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* WA_DIAG_HDD_H */

/* End of doxygen group */
/*! @} */

/* EOF */
