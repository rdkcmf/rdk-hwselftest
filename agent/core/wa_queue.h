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
 * @file wa_queue.h
 */

/** @addtogroup WA_QUEUE
 *  @{
 */

#ifndef WA_QUEUE_H
#define WA_QUEUE_H

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

typedef void * WA_QUEUE_handle_t;

/*****************************************************************************
 * EXPORTED VARIABLES
 *****************************************************************************/

/*****************************************************************************
 * EXPORTED FUNCTIONS
 *****************************************************************************/

/**
 * Create queue with predefined deep.
 *
 * @param deep queue levels (0 for default)
 *
 * @retval 0  The queue was successfully created.
 * @retval -1 The queue creation failed.
 */

extern WA_QUEUE_handle_t WA_QUEUE_Create(int deep);


/**
 * Removes the queue.
 *
 * @retval 0  The queue was successfully destroyed.
 * @retval -1 The queue removal failed.
 */
extern int WA_QUEUE_Destroy(WA_QUEUE_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif /* _WA_QUEUE_H_ */

/* End of doxygen group */
/*! @} */

/* EOF */
