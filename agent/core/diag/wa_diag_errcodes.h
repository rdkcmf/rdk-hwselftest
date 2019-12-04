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
 * @brief Unified error status codes.
 */

/** @addtogroup WA_DIAG
 *  @{
 */

#ifndef WA_DIAG_ERRCODES_H
#define WA_DIAG_ERRCODES_H

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
#define WA_DIAG_ERRCODE_FAILURE 1

#define WA_DIAG_ERRCODE_SUCCESS 0

/* generic codes */
#define WA_DIAG_ERRCODE_NOT_APPLICABLE      (-1)
#define WA_DIAG_ERRCODE_CANCELLED           (-2)
#define WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR (-3)

/* specific codes */
#define WA_DIAG_ERRCODE_HDD_STATUS_MISSING  (-100)
#define WA_DIAG_ERRCODE_HDMI_NO_DISPLAY     (-101)
#define WA_DIAG_ERRCODE_HDMI_NO_HDCP        (-102)
#define WA_DIAG_ERRCODE_MOCA_NO_CLIENTS     (-103)
#define WA_DIAG_ERRCODE_MOCA_DISABLED       (-104)
#define WA_DIAG_ERRCODE_SI_CACHE_MISSING    (-105)
#define WA_DIAG_ERRCODE_TUNER_NO_LOCK       (-106)
#define WA_DIAG_ERRCODE_AV_NO_SIGNAL        (-107)
#define WA_DIAG_ERRCODE_IR_NOT_DETECTED     (-108)
#define WA_DIAG_ERRCODE_CM_NO_SIGNAL        (-109)
#define WA_DIAG_ERRCODE_MCARD_CERT_INVALID  (WA_DIAG_ERRCODE_FAILURE)
#define WA_DIAG_ERRCODE_TUNER_BUSY          (-111)
#define WA_DIAG_ERRCODE_RF4CE_NO_RESPONSE   (-112)
#define WA_DIAG_ERRCODE_WIFI_NO_CONNECTION  (-113)

//#define DEFAULT_RESULT_VALUE (-200) /* Currently defined and used in wa_agg.c and hwst_diag_prev_results.cpp for initial value assignment */

/*****************************************************************************
 * EXPORTED TYPES
 *****************************************************************************/

/*****************************************************************************
 * EXPORTED VARIABLES
 *****************************************************************************/

/*****************************************************************************
 * EXPORTED FUNCTIONS
 *****************************************************************************/

/*****************************************************************************
 * LOCAL FUNCTIONS
 *****************************************************************************/

#ifdef __cplusplus
}
#endif

#endif
