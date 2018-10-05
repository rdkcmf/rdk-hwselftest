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
 * @file wa_debug.h
 *
 */

#ifndef WA_DEBUG_H
#define WA_DEBUG_H

/** @addtogroup WA_DEBUG
 *  @{
 */

/*****************************************************************************
 * STANDARD INCLUDE FILES
 *****************************************************************************/

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/

/*****************************************************************************
 * EXPORTED DEFINITIONS
 *****************************************************************************/

/* Debug definitions. */
#if WA_DEBUG
#include <stdio.h>
#define WA_ENTER(fmt, ...) printf("HWST_DBG |" fmt, ## __VA_ARGS__)
#define WA_RETURN(fmt, ...) printf("HWST_DBG |" fmt, ## __VA_ARGS__)
#define WA_ERROR(fmt, ...) printf("HWST_DBG |" fmt, ## __VA_ARGS__)
#define WA_WARN(fmt, ...) printf("HWST_DBG |" fmt, ## __VA_ARGS__)
#define WA_INFO(fmt, ...) printf("HWST_DBG |" fmt, ## __VA_ARGS__)
#define WA_DBG(fmt, ...) printf("HWST_DBG |" fmt, ## __VA_ARGS__)
#else
#define WA_ENTER(...)
#define WA_RETURN(...)
#define WA_ERROR(...)
#define WA_WARN(...)
#define WA_INFO(...)
#define WA_DBG(...)
#endif

/*****************************************************************************
 * EXPORTED TYPES
 *****************************************************************************/

/*****************************************************************************
 * EXPORTED VARIABLES
 *****************************************************************************/

/*****************************************************************************
 * EXPORTED FUNCTIONS
 *****************************************************************************/

/* End of doxygen group */
/*! @} */

#endif /* WA_DEBUG_H */

