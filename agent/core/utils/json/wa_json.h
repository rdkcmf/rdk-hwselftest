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
 * @file wa_json.h
 */

/** @addtogroup WA_UTILS_JSON
 *  @{
 */

#ifndef WA_UTILS_JSON_H
#define WA_UTILS_JSON_H

/*****************************************************************************
 * STANDARD INCLUDE FILES
 *****************************************************************************/
#include <stdint.h>
#include <stdbool.h>

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/
#include "jansson.h"

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
 * @brief Validates json against json-rpc
 *
 * @param pJson pointer to there the newly created object is returned
 *
 * @retval 0 success (\c pJson is a \c json_t object, it must be applied to \n json_decref() by the caller)
 * @retval 1 validation error (\c pJson is a \c json_t object, it must be applied to \n json_decref() by the caller)
 * @retval -1 internal error (\c pJson is NULL)
 */
extern int WA_UTILS_JSON_RpcValidate(json_t **pJson);

/**
 * @brief Converts message to json and validates it against json-rpc
 *
 * @param msg incoming json string message
 * @param len number of characters to take into consideration
 * @param pJson pointer to there the newly created object is returned
 *
 * @retval 0 success (\c pJson is a \c json_t object, it must be applied to \n json_decref() by the caller)
 * @retval 1 validation error (\c pJson is a \c json_t object, it must be applied to \n json_decref() by the caller)
 * @retval -1 internal error (\c pJson is NULL)
 */
extern int WA_UTILS_JSON_RpcValidateTxt(const char *const msg, size_t len, json_t **pJson);

/**
 * @brief Initialize json.
 *
 * @retval 0 success
 * @retval -1 error
 */
extern int WA_UTILS_JSON_Init(void);

/**
 * @brief Sets a nested property of an object.
 *
 * The first parameters is the object whose nested property is to be modified.
 * The second argument is the value to be inserted into the object.
 * The third and subsequent arguments are names of the keys that define the path to the nested property (const char *).
 * The list is terminated by a single NULL pointer.
 *
 * If a single key name is provided, i.e.:
 *
 *   WA_UTILS_JSON_NestedSet(target, value, key, NULL);
 *
 * then it is roughly equivalent to:
 *
 *   json_object_set(target, key, value);
 *
 * If two key names are provided, i.e.:
 *
 *   WA_UTILS_JSON_NestedSet(target, value, childKey, key, NULL);
 *
 * then the @a target object is expected to contain an object associated with a key @a childkey
 * and that object's @a key property is then set to @a value.
 * If @a childKey does not exist in @a target an empty object is created.
 * If @a childKey exists in @a target but the value associated with it is not an object, function fails.
 *
 * If more key names are provided, each but the last one denotes a subsequent nested child object.
 */
extern bool WA_UTILS_JSON_NestedSet(json_t * object, json_t * value, const char * key, ...);

/**
 * @brief Like @a WA_UTILS_JSON_NestedSet, but decrefs @a value.
 */
extern bool WA_UTILS_JSON_NestedSetNew(json_t * object, json_t * value, const char * key, ...);

#ifdef __cplusplus
}
#endif

#endif /* _WA_UTILS_JSON_H_ */
