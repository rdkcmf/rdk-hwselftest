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
 * @file wa_diag.h
 */

/** @addtogroup WA_DIAG
 *  @{
 */

#ifndef WA_DIAG_H
#define WA_DIAG_H

/*****************************************************************************
 * STANDARD INCLUDE FILES
 *****************************************************************************/

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/
#include "wa_diag_errcodes.h"
#include "wa_json.h"
#include "wa_osa.h"

#ifdef __cplusplus
extern "C"
{
#endif

/*****************************************************************************
 * EXPORTED DEFINITIONS
 *****************************************************************************/
#define WA_DIAG_MSG_ID_PREFIX 0x00020000

/*****************************************************************************
 * EXPORTED TYPES
 *****************************************************************************/

/** Adapter implementation side de-initialization for the diag procedure.
 *
 * @param initHandle a handle provided by \c WA_DIAG_ProcedureInit_t, otherwise WA_DIAG_proceduresConfig_t
 *
 * @retval 0 success.
 * @retval -1 error
 */
typedef int (*WA_DIAG_ProcedureExit_t)(void *initHandle);

/** A callback carrying message from system to diag procedure.
 * Carries startup params etc.
 *
 * @param initHandle a handle provided by \c WA_DIAG_ProcedureInit_t, otherwise WA_DIAG_proceduresConfig_t
 * @param json the message body
 * @param responseCookie the cookie that must be given to the \c WA_DIAG_Send()
 *
 * @retval 0 success.
 * @retval -1 error
 *
 * @note If the \c responseCookie is not NULL the response is required.
 */
typedef int (*WA_DIAG_Callback_t)(void *initHandle, json_t *json, void *responseCookie);


/** A diag diag routine, run in agent managed thread.
 *
 * @param instanceHandle a handle to provide back in \c WA_DIAG_Send()
 * @param initHandle a handle provided by \c WA_DIAG_ProcedureInit_t, otherwise WA_DIAG_proceduresConfig_t
 * @param json pointer to the the message body and for the response
 *
 * @retval 0 success.
 * @retval !=0 error
 *
 * @note Function in any case must consume the given json if it exists (so it must call json_decref(json)).
 * @note Function must update the \json parameter with data to be returned (null is allowed).
 */
typedef int (*WA_DIAG_ProcedureFnc_t)(void *instanceHandle, void *initHandle, json_t **json);

/** Content for registration of the diag procedures.
 * Provided in a form of array closed by NULLs in last entry.
 */
typedef struct WA_DIAG_proceduresConfig_t
{
    const char *name;
    void * (*initFnc)(struct WA_DIAG_proceduresConfig_t *); /**< the initialization function that will be called at the system startup */
    WA_DIAG_ProcedureExit_t exitFnc; /**< the de-initialization function that will be called at the system exit */
    WA_DIAG_ProcedureFnc_t fnc; /**< the procedure function that executes the diagnose */
    WA_DIAG_Callback_t callback; /** a callback function */
    json_t *config; /**< configuration given back to the above functions */
    const char *caps; /**< fnc capabilities */
}WA_DIAG_proceduresConfig_t;

/** Adapter implementation side initialization for the diag procedure.
 *
 * @param config configuration
 *
 * @returns handle to provide in \c WA_DIAG_ProcedureExit_t
 * @retval null error
 *
 * @note This function can build \c config->caps dynamically.
 */
typedef void * (*WA_DIAG_ProcedureInit_t)(struct WA_DIAG_proceduresConfig_t *config);

/*****************************************************************************
 * EXPORTED VARIABLES
 *****************************************************************************/
/** Handle to the DIAG incoming queue */
extern void *WA_DIAG_IncomingQ;

/*****************************************************************************
 * EXPORTED FUNCTIONS
 *****************************************************************************/

/** Communication function from diag module to the system.
 * Carries test status, progress, etc.
 *
 * @param instanceHandle a handle provided by \c WA_COMM_ProcedureRegister()
 * @param json the message body
 * @param responseCookie the cookie provided by the \c WA_DIAG_Callback_t, NULL otherwise
 *
 * @retval 0 success.
 * @retval -1 error
 */
extern int WA_DIAG_Send(void *instanceHandle, json_t *json, void *responseCookie);

/** Communication function from diag module to the system.
 * This is a wrapper for \c WA_DIAG_Send() for convenience when using string input
 *
 * @param instanceHandle a handle provided by \c WA_COMM_ProcedureRegister()
 * @param msg the message body as a string
 *
 * @retval 0 success.
 * @retval -1 error
 */
extern int WA_DIAG_SendStr(void *instanceHandle, char *msg, void *responseCookie);

/**
 * Communication function from diag module to the system.
 * This wrapper send the progress status.
 *
 * @param instanceHandle a handle provided by \c WA_COMM_ProcedureRegister()
 * @param progress a progress to report [1..100]
 */
extern void WA_DIAG_SendProgress(void* instanceHandle, int progress);

/**
 * Initialize the diag module.
 *
 * @param diags the array of diag procedures to attach to the system
 * @param responseCookie the cookie provided by the \c WA_DIAG_Callback_t, NULL otherwise
 *
 * @retval 0 success.
 * @retval -1 error
 */

extern int WA_DIAG_Init(const WA_DIAG_proceduresConfig_t *diags);

/**
 * Deinitialize the diag module.
 *
 * @retval 0 success.
 * @retval -1 error
 */
extern int WA_DIAG_Exit(void);

#ifdef __cplusplus
}
#endif

#endif /* _WA_DIAG_H_ */
