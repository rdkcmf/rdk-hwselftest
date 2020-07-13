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
 * @file wa_trh.h
 *
 * @brief This file contains interface functions for tuner reservation helper.
 */

/** @addtogroup WA_UTILS_TRH
 *  @{
 */

#ifndef WA_UTILS_TRH_H
#define WA_UTILS_TRH_H


/*****************************************************************************
 * STANDARD INCLUDE FILES
 *****************************************************************************/
#include <stdint.h>

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

/*****************************************************************************
 * EXPORTED VARIABLES
 *****************************************************************************/

/*****************************************************************************
 * EXPORTED FUNCTIONS
 *****************************************************************************/

/**
 * @brief Initialises the TRH utility.
 *
 * @retval 0 on success.
 * @retval 1 on failure.
 */
int WA_UTILS_TRH_Init();

/**
 * @brief Deinitialises the TRH utility.
 *
 * @retval 0 on success.
 * @retval 1 on failure.
 */
int WA_UTILS_TRH_Exit();

/**
 * @brief Attempts to reserve a tuner via TRM.
 *
 * param url         OCAP URL to tune to.   
 * param when        Timestamp when to reserve the tuner (epoch, 0: now).
 * param duration    Duration for how long to reserve the tuner (ms).
 * param timeout_ms  Maximum time to wait for the reservation to happen (ms, 0: forever).
 * param tuner_count Total number of available tuners in the device.
 * param out_handle  Output: reservation handle.
 *
 * @retval 0 on success.
 * @retval 1 on failure.
 * @retval 2 on timeout.
 */
int WA_UTILS_TRH_ReserveTuner(const char *url, uint64_t when, uint64_t duration, int timeout_ms, int tuner_count, void **out_handle);

/**
 * @brief Attempts to release a tuner via TRM.
 *
 * @param handle      Handle of reservation to release.
 * @param timeout_ms  Maximum time to wait for the reservaion to be released (ms, 0: forever).
 *
 * @retval 0 on success.
 * @retval 1 on failure.
 * @retval 2 on timeout.
 */
int WA_UTILS_TRH_ReleaseTuner(void *handle, int timeout_ms);

/**
 * @brief Blocks the current thread until a tuner is released.
 *
 * @param handle      Handle of reservation to wait for.
 * @param timeout_ms  Maximum time to wait for the tuner to be released (ms, 0: forever).
 *
 * @retval 0 on success.
 * @retval 1 on failure.
 * @retval 2 on timeout.
 */
int WA_UTILS_TRH_WaitForTunerRelease(void *handle, int timeout_ms);

/*****************************************************************************
 * LOCAL FUNCTIONS
 *****************************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* WA_UTILS_TRH_H */

/* End of doxygen group */
/*! @} */

/* EOF */
