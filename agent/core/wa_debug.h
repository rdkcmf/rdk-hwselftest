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
#include <stdio.h>

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/
#include "rdk_debug.h"
#include "stdlib.h"
#include "string.h"

/*****************************************************************************
 * EXPORTED DEFINITIONS
 *****************************************************************************/

/* Debug definitions. This will be enabled/disabled via debug.ini using 'LOG.RDK.HWST = ' */
#define WA_ENTER(fmt...)  RDK_LOG(RDK_LOG_TRACE1,  "LOG.RDK.HWST", "HWST_LOG |" fmt)
#define WA_RETURN(fmt...) RDK_LOG(RDK_LOG_TRACE1,  "LOG.RDK.HWST", "HWST_LOG |" fmt)
#define WA_ERROR(fmt...)  RDK_LOG(RDK_LOG_ERROR,   "LOG.RDK.HWST", "HWST_LOG |" fmt)
#define WA_WARN(fmt...)   RDK_LOG(RDK_LOG_WARN,    "LOG.RDK.HWST", "HWST_LOG |" fmt)
#define WA_INFO(fmt...)   RDK_LOG(RDK_LOG_INFO,    "LOG.RDK.HWST", "HWST_LOG |" fmt)
#define WA_DBG(fmt...)    RDK_LOG(RDK_LOG_DEBUG,   "LOG.RDK.HWST", "HWST_LOG |" fmt)

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

