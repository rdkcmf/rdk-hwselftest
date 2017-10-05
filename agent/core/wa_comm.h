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
 * @file wa_comm.h
 */

/** @addtogroup WA_COMM
 *  @{
 */

#ifndef WA_COMM_H
#define WA_COMM_H

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
#define WA_COMM_MSG_ID_PREFIX 0x00010000

/*****************************************************************************
 * EXPORTED TYPES
 *****************************************************************************/

/** Adapter implementation side de-initialization for the comm adapter.
 *
 * @param handle a handle provided by \c WA_COMM_AdaperInit_t
 *
 * @retval 0 success.
 * @retval -1 error
 */
typedef int (*WA_COMM_AdapterExit_t)(void *handle);

/** A callback carrying message from system to comm module.
 *
 * @param cookie a cookie that is provided with \c WA_COMM_Register()
 * @param json the message body
 *
 * @retval 0 success
 * @retval -1 error
 *
 * @note In case of success this function is responsible for consuming the json
 *       (so it must call json_decref(json)). In case of error it must not do this.
 */
typedef int (*WA_COMM_Callback_t)(void *cookie, json_t *json);

/** Content for registration of the comm adapters.
 * Provided in a form of array closed by NULLs in last entry.
 */

typedef struct WA_COMM_adaptersConfig_t
{
    const char * name; /**< unique name of the adapter */
    void * (* initFnc)(struct WA_COMM_adaptersConfig_t *); /**< the initialization function that will be called at the system startup */
    WA_COMM_AdapterExit_t exitFnc; /**< the de-initialization function that will be called at the system exit */
    WA_COMM_Callback_t callback; /** a callback function */
    json_t *config; /**< cookie given back to the \c Fnc functions */
    const char *caps; /**< fnc capabilities */
}WA_COMM_adaptersConfig_t;


/** Adapter implementation side initialization for the comm adapter.
 *
 * @param config configuration that are provided with \c WA_COMM_adaptersConfig_t
 *
 * @returns handle to provide in \c WA_COMM_AdapterExit_t
 * @retval null error
 *
 * * @note This function can build \c config->caps dynamically.
 */
typedef void * (*WA_COMM_AdaperInit_t)(WA_COMM_adaptersConfig_t *config);


/*****************************************************************************
 * EXPORTED VARIABLES
 *****************************************************************************/
/** Handle to the COMM incoming queue */
extern void *WA_COMM_IncomingQ;

/*****************************************************************************
 * EXPORTED FUNCTIONS
 *****************************************************************************/

/** Comm module registration to the system.
 *
 * @param config configuration that are provided with \c WA_COMM_adaptersConfig_t
 * @param cookie a cookie that will be given back to the callback function
 *
 * @returns handle to provide in \c WA_COMM_Send()
 * @retval null error
 */
extern void *WA_COMM_Register(const WA_COMM_adaptersConfig_t *config, void *cookie);

/** Comm module unregistration from the system.
 *
 * @param handle a handle provided by \c WA_COMM_Register()
 *
 * @retval 0 success.
 * @retval -1 error
 */
extern int WA_COMM_Unregister(void *handle);

/** Communication function from comm module to the system.
 *
 * @param handle a handle provided by \c WA_COMM_Register()
 * @param json the message body
 *
 * @retval 0 success.
 * @retval -1 error
 */
extern int WA_COMM_Send(void *handle, json_t *json);

/** Communication function from comm module to the system using string.
 *
 * @param handle a handle provided by \c WA_COMM_Register()
 * @param msg the message body
 * @param len the buffer length
 *
 * @retval 0 success.
 * @retval -1 error
 */
extern int WA_COMM_SendTxt(void *handle, char *msg, size_t len);

/**
 * Initialize the comm module.
 *
 * @param adapters the array of comm adapters to attach to the system
 *
 * @retval 0 success.
 * @retval -1 error
 */
extern int WA_COMM_Init(WA_COMM_adaptersConfig_t * const adapters);

/**
 * Deinitialize the comm module.
 *
 * @retval 0 success.
 * @retval -1 error
 */
extern int WA_COMM_Exit(void);

#ifdef __cplusplus
}
#endif

#endif /* _WA_COMM_H_ */
