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
 * @file wa_mfr.h
 *
 * @brief This file contains inteface functions for manufacturer library.
 */

/** @addtogroup WA_UTILS_MFR
 *  @{
 */

#ifndef WA_UTILS_MFR_H
#define WA_UTILS_MFR_H


/*****************************************************************************
 * STANDARD INCLUDE FILES
 *****************************************************************************/

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/
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
typedef enum {
    WA_UTILS_MFR_PARAM_MANUFACTURER,
    WA_UTILS_MFR_PARAM_MODEL,
    WA_UTILS_MFR_PARAM_SERIAL
} WA_UTILS_MFR_StbParams_t;

/*****************************************************************************
 * EXPORTED VARIABLES
 *****************************************************************************/

/*****************************************************************************
 * EXPORTED FUNCTIONS
 *****************************************************************************/

/**
 * Read serialized data item common hdd identity strings (s/n, fw, model).
 *
 * @retval 0 on success.
 * @retval -1 otherwise.
 */
extern int WA_UTILS_MFR_ReadSerializedData(WA_UTILS_MFR_StbParams_t data,
   size_t* size, char** value);

/*****************************************************************************
 * LOCAL FUNCTIONS
 *****************************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* WA_UTILS_MFR_H */

/* End of doxygen group */
/*! @} */

/* EOF */
