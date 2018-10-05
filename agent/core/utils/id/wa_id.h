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
 * @file wa_id.h
 */

/** @addtogroup WA_UTILS_ID
 *  @{
 */

#ifndef WA_UTILS_ID_H
#define WA_UTILS_ID_H

/*****************************************************************************
 * STANDARD INCLUDE FILES
 *****************************************************************************/
#include <stdint.h>

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/
#include "wa_list_api.h"

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
#ifdef WA_STEST
/** Basic type for the ID */
typedef uint8_t WA_UTILS_ID_t;
#else
/** Basic type for the ID */
typedef uint16_t WA_UTILS_ID_t;
#endif

/** Id container that might be used individually by \c WA_UTILS_ID_GenerateUnsafe() */
typedef struct
{
    WA_UTILS_ID_t id;
    WA_UTILS_LIST_t list;
}WA_UTILS_ID_box_t;


/*****************************************************************************
 * EXPORTED VARIABLES
 *****************************************************************************/


/*****************************************************************************
 * EXPORTED FUNCTIONS
 *****************************************************************************/
/**
 * @brief Initialize thread-safe id.
 *
 * @retval 0  success
 * @retval !=0 error
 */
extern int WA_UTILS_ID_Init();

/**
 * @brief Uninitialize thread-safe id.
 *
 * @retval 0  success
 * @retval !=0 error
 */
extern int WA_UTILS_ID_Exit();

/**
 * @brief Generate thread-safe unique id.
 *
 * @param pId pointer where the new id will be stored
 *
 * @retval 0  success
 * @retval !=0 error
 */
extern int WA_UTILS_ID_Generate(WA_UTILS_ID_t *pId);

/**
 * @brief Release thread-safe unique id.
 *
 * @param id the id
 *
 * @retval 0  success
 * @retval !=0 error
 */
extern int WA_UTILS_ID_Release(WA_UTILS_ID_t id);

/**
 * @brief Generate thread-unsafe unique id.
 *
 * @param pBox pointer to the id box
 * @param pId pointer where the new id will be stored
 *
 * @retval 0  success
 * @retval !=0 error
 */
extern int WA_UTILS_ID_GenerateUnsafe(WA_UTILS_ID_box_t *pBox, WA_UTILS_ID_t *pId);

/**
 * @brief Release thread-unsafe unique id.
 *
 * @param pBox pointer to the id box
 * @param id the id
 *
 * @retval 0  success
 * @retval !=0 error
 */
extern int WA_UTILS_ID_ReleaseUnsafe(WA_UTILS_ID_box_t *pBox, WA_UTILS_ID_t id);

#ifdef __cplusplus
}
#endif

#endif /* _WA_UTILS_ID_H */
