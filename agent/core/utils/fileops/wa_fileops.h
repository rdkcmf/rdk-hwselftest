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
 * @file wa_fileops.h
 *
 * @brief This file contains functions for reading options from file.
 */

/** @addtogroup WA_UTILS_FILEOPS
 *  @{
 */

#ifndef WA_UTILS_FILEOPS_H
#define WA_UTILS_FILEOPS_H


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

/*****************************************************************************
 * EXPORTED VARIABLES
 *****************************************************************************/

/*****************************************************************************
 * EXPORTED FUNCTIONS
 *****************************************************************************/
int WA_UTILS_FILEOPS_OptionSupported(const char *file, const char *mode, const char *option, const char *exp_value);
char *WA_UTILS_FILEOPS_OptionFind(const char *fname, const char *pattern);
char **WA_UTILS_FILEOPS_OptionFindMultiple(const char *fname, const char *pattern, int maxEntries);
void WA_UTILS_FILEOPS_OptionFindMultipleFree(char **multiple);
int WA_UTILS_FILEOPS_CheckCommandResult(const char *cmd, const char *searchString, const char *expValue);

/*****************************************************************************
 * LOCAL FUNCTIONS
 *****************************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* WA_UTILS_FILEOPSOD */

/* End of doxygen group */
/*! @} */

/* EOF */
