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
#define WA_DIAG_ERRCODE_CANCELLED_NOT_STANDBY           (-4)

/* specific codes */
#define WA_DIAG_ERRCODE_HDD_STATUS_MISSING  (-100)
#define WA_DIAG_ERRCODE_HDMI_NO_DISPLAY     (-101)
#define WA_DIAG_ERRCODE_HDMI_NO_HDCP        (-102)
#define WA_DIAG_ERRCODE_MOCA_NO_CLIENTS     (-103)
#define WA_DIAG_ERRCODE_MOCA_DISABLED       (-104)
#define WA_DIAG_ERRCODE_SI_CACHE_MISSING    (-105)
#define WA_DIAG_ERRCODE_TUNER_NO_LOCK       (-106)
#define WA_DIAG_ERRCODE_AV_NO_SIGNAL        (-107) /* Deprecated (DELIA-48787) - Used only for debugging with '#define AVD_USE_RMF 1' */
#define WA_DIAG_ERRCODE_IR_NOT_DETECTED     (-108)
#define WA_DIAG_ERRCODE_CM_NO_SIGNAL        (-109)
#define WA_DIAG_ERRCODE_TUNER_BUSY          (-111)
#define WA_DIAG_ERRCODE_RF4CE_NO_RESPONSE   (-112)
#define WA_DIAG_ERRCODE_WIFI_NO_CONNECTION  (-113)
#define WA_DIAG_ERRCODE_AV_URL_NOT_REACHABLE (-114) /* Deprecated (DELIA-48787) - Used only for debugging with '#define AVD_USE_RMF 1' */

#define WA_DIAG_ERRCODE_NON_RF4CE_INPUT                 (-117)
#define WA_DIAG_ERRCODE_RF4CE_CTRLM_NO_RESPONSE         (-120)
#define WA_DIAG_ERRCODE_HDD_MARGINAL_ATTRIBUTES_FOUND   (-121)
#define WA_DIAG_ERRCODE_NO_GATEWAY_CONNECTION           (-123)
#define WA_DIAG_ERRCODE_NO_COMCAST_WAN_CONNECTION       (-124)
#define WA_DIAG_ERRCODE_NO_PUBLIC_WAN_CONNECTION        (-125)
#define WA_DIAG_ERRCODE_NO_WAN_CONNECTION               (-126)

#define WA_DIAG_ERRCODE_HDD_DEVICE_NODE_NOT_FOUND       (-127)
#define WA_DIAG_ERRCODE_NO_ETH_GATEWAY_FOUND            (-128)
#define WA_DIAG_ERRCODE_NO_MW_GATEWAY_FOUND             (-129)
#define WA_DIAG_ERRCODE_NO_ETH_GATEWAY_CONNECTION       (-130)
#define WA_DIAG_ERRCODE_NO_MW_GATEWAY_CONNECTION        (-131)
#define WA_DIAG_ERRCODE_AV_DECODERS_NOT_ACTIVE          (-132)

#define WA_DIAG_ERRCODE_DEFAULT_RESULT_VALUE            (-200) /* Also defined and used in wa_agg.c and hwst_diag_prev_results.cpp for initial value assignment */

/* Failure specific codes */
#define WA_DIAG_ERRCODE_BLUETOOTH_INTERFACE_FAILURE         (-211)
#define WA_DIAG_ERRCODE_FILE_WRITE_OPERATION_FAILURE        (-212)
#define WA_DIAG_ERRCODE_FILE_READ_OPERATION_FAILURE         (-213)
#define WA_DIAG_ERRCODE_EMMC_TYPEA_MAX_LIFE_EXCEED_FAILURE  (-214)
#define WA_DIAG_ERRCODE_EMMC_TYPEB_MAX_LIFE_EXCEED_FAILURE  (-215)
#define WA_DIAG_ERRCODE_EMMC_TYPEA_ZERO_LIFETIME_FAILURE    (-216)
#define WA_DIAG_ERRCODE_EMMC_TYPEB_ZERO_LIFETIME_FAILURE    (-217)
#define WA_DIAG_ERRCODE_MCARD_CERT_INVALID                  (WA_DIAG_ERRCODE_FAILURE)
#define WA_DIAG_ERRCODE_MCARD_AUTH_KEY_REQUEST_FAILURE      (-218)
#define WA_DIAG_ERRCODE_MCARD_HOSTID_RETRIEVE_FAILURE       (-219)
#define WA_DIAG_ERRCODE_MCARD_CERT_AVAILABILITY_FAILURE     (-220)
#define WA_DIAG_ERRCODE_RF4CE_CHIP_DISCONNECTED             (-221)
#define WA_DIAG_ERRCODE_SD_CARD_TSB_STATUS_FAILURE          (-222)
#define WA_DIAG_ERRCODE_SD_CARD_ZERO_MAXMINUTES_FAILURE     (-223)

/* App launch failure codes */
#define WA_DIAG_ERRCODE_MULTIPLE_CONNECTIONS_NOT_ALLOWED  (-250)
#define WA_DIAG_ERRCODE_WEBSOCKET_CONNECTION_FAILURE      (-251)
#define WA_DIAG_ERRCODE_AGENT_INIT_FAILURE                (-252)
#define WA_DIAG_ERRCODE_RESOURCE_OPERATION_FAILURE        (-253)
#define WA_DIAG_ERRCODE_DRAM_THRESHOLD_EXCEEDED           (-254)
#define WA_DIAG_ERRCODE_CPU_THRESHOLD_EXCEEDED            (-255)

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
