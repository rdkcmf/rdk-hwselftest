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
 * @file
 *
 * @brief Diagnostic functions for tuner - interface
 */

/** @addtogroup WA_DIAG_TUNER
 *  @{
 */

#ifndef WA_DIAG_TUNER_H
#define WA_DIAG_TUNER_H

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
#define BUFFER_LEN 256

/*****************************************************************************
 * EXPORTED TYPES
 *****************************************************************************/
typedef struct WA_DIAG_TUNER_TunerStatus_tag
{
        bool used;
        bool locked;
} WA_DIAG_TUNER_TunerStatus_t;

typedef struct WA_DIAG_TUNER_DocsisParams_tag
{
        char *DOCSIS_DwStreamChPwr;
        char *DOCSIS_UpStreamChPwr;
        char *DOCSIS_SNR;
} WA_DIAG_TUNER_DocsisParams_t;

typedef struct WA_DIAG_TUNER_QamParams_tag
{
        char *numLocked;
        char *QAM_ChPwr;
        char *QAM_SNR;
} WA_DIAG_TUNER_QamParams_t;

/*****************************************************************************
 * EXPORTED VARIABLES
 *****************************************************************************/

/*****************************************************************************
 * EXPORTED FUNCTIONS
 *****************************************************************************/
extern int WA_DIAG_TUNER_status(void* instanceHandle, void *initHandle, json_t **params);

bool WA_DIAG_TUNER_GetTunerStatuses(WA_DIAG_TUNER_TunerStatus_t * statuses, size_t statusCount, int * pNumLocked);
bool WA_DIAG_TUNER_GetDocsisParams(WA_DIAG_TUNER_DocsisParams_t * params);
bool WA_DIAG_TUNER_GetQamParams(WA_DIAG_TUNER_QamParams_t * params);

/*****************************************************************************
 * LOCAL FUNCTIONS
 *****************************************************************************/

#ifdef __cplusplus
}
#endif

#endif
