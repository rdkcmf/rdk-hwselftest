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
 * @brief Diagnostic functions for RF4CE - implementation
 */

/** @addtogroup WA_DIAG_RF4CE
 *  @{
 */

/*****************************************************************************
 * STANDARD INCLUDE FILES
 *****************************************************************************/
#include <string.h>
#include <time.h>

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/
#include "wa_diag.h"
#include "wa_debug.h"
#include "wa_fileops.h"

/* rdk specific */
#include "wa_iarm.h"
#include "libIBus.h"
#include "libIARMCore.h"
#include "irMgr.h"

/* module interface */
#include "wa_diag_rf4ce.h"

/* enable RF APIs */
#define RF4CE_API          // select particular rf4ceMgr API (RF4CE_GENMSO_API, RF4CE_API, RF4CE_GPMSO_API)

#if defined(USE_RF4CEMGR)
#include "rf4ceMgr.h"
#endif

#if defined(USE_CTRLMGR)
#include "ctrlm_ipc.h"
#endif

/*****************************************************************************
 * GLOBAL VARIABLE DEFINITIONS
 *****************************************************************************/

/*****************************************************************************
 * LOCAL DEFINITIONS
 *****************************************************************************/
#define DEV_CONFIG_FILE_PATH         "/etc/device.properties"
#define FILE_MODE                    "r"

#define RF4CE_OPTION_STR              "RF4CE_CAPABLE"
#define RF4CE_AVAILABLE_STR           "true"

#define RF4CE_CHANNEL_MIN            (11)
#define RF4CE_CHANNEL_MAX            (26)

#define PREVIOUS_RF4CE_KEY_EVENT_MIN 10 /*minutes*/

/*****************************************************************************
 * LOCAL TYPES
 *****************************************************************************/
static int checkOnlyPairing = 0;

/*****************************************************************************
 * LOCAL FUNCTION PROTOTYPES
 *****************************************************************************/
static int rf4ce_supported(void);
#if defined(USE_RF4CEMGR)
static int checkRf4ceStatusViaRf4ceMgr();
#endif
#if defined(USE_CTRLMGR)
static int checkRf4ceChipStatus();
static int checkRf4ceStatusViaCtrlMgr();
#endif
#if defined(USE_RF4CEMGR) || defined(USE_CTRLMGR)
static int verifyRf4ceStatus(uint16_t paired, uint16_t max_paired, uint16_t channel);
static int checkRCULastKeyInfo();
#endif
static int checkRf4ceStatus(void);
static int setReturnData(int status, json_t **param);

/*****************************************************************************
 * LOCAL VARIABLE DECLARATIONS
 *****************************************************************************/

/*****************************************************************************
 * FUNCTION DEFINITIONS
 *****************************************************************************/

/*****************************************************************************
 * LOCAL FUNCTIONS
 *****************************************************************************/
/* -1 error
    0 not supported
    1 supported */
static int rf4ce_supported(void)
{
    return WA_UTILS_FILEOPS_OptionSupported(DEV_CONFIG_FILE_PATH, FILE_MODE, RF4CE_OPTION_STR, RF4CE_AVAILABLE_STR);
}

#if defined(USE_RF4CEMGR) || defined(USE_CTRLMGR)
/*  returns: 0 bad, 1 good */
static int verifyRf4ceStatus(uint16_t paired, uint16_t max_paired, uint16_t channel)
{
    int result = -1;

    WA_ENTER("verifyRf4ceStatus()\n");

    if (paired <= max_paired)
    {
        /* check number of paired remotes, no paired remotes is OK */
        if (paired > 0)
        {
            WA_DBG("verifyRf4ceStatus(): %i RCU(s) paired\n", paired);
            result = 1;
        }
    }
    else
    {
        result = 0;
        WA_ERROR("verifyRf4ceStatus(): invalid number of paired RCUs (%i)\n", paired);
    }

    if (checkOnlyPairing)
    {
        return result;
    }

    if (result == -1)
    {
        /* no paired RCUs found, at least check if the active RF channel number is within range */
        WA_DBG("verifyRf4ceStatus(): no paired RCUs found\n");

        result = ((channel >= RF4CE_CHANNEL_MIN) && (channel <= RF4CE_CHANNEL_MAX));

        if (result)
            WA_DBG("verifyRf4ceStatus(): correct active RF channel number (%i)\n", channel);
        else
            WA_ERROR("verifyRf4ceStatus(): invalid active RF channel number (%i)\n", channel);
    }

    WA_RETURN("verifyRf4ceStatus(): return code: %d\n", result);

    return result;
}

static int checkRCULastKeyInfo()
{
    int result = -1;
    int time_diff = -1;
    IARM_Result_t iarm_result = IARM_RESULT_IPCCORE_FAIL;

    WA_ENTER("checkRCULastKeyInfo()\n");

    /* Get the last key press info when the pairing status returns success*/
    ctrlm_main_iarm_call_last_key_info_t *last_key_info_param = NULL;

    iarm_result = IARM_Malloc(IARM_MEMTYPE_PROCESSLOCAL, sizeof(*last_key_info_param), (void**)&last_key_info_param);
    if ((iarm_result == IARM_RESULT_SUCCESS) && last_key_info_param)
    {
        memset(last_key_info_param, 0, sizeof(*last_key_info_param));
        last_key_info_param->api_revision = CTRLM_MAIN_IARM_BUS_API_REVISION;

        WA_DBG("checkRCULastKeyInfo(): IARM_Bus_Call('%s', '%s', ...)\n", CTRLM_MAIN_IARM_BUS_NAME, CTRLM_MAIN_IARM_CALL_LAST_KEY_INFO_GET);
        iarm_result = IARM_Bus_Call(CTRLM_MAIN_IARM_BUS_NAME, CTRLM_MAIN_IARM_CALL_LAST_KEY_INFO_GET,
                                                    (void *)last_key_info_param, sizeof(*last_key_info_param));

        if (iarm_result == IARM_RESULT_SUCCESS)
        {
            if (last_key_info_param->result == CTRLM_IARM_CALL_RESULT_SUCCESS)
            {
                if ((last_key_info_param->controller_id != -1) && (last_key_info_param->source_type == IARM_BUS_IRMGR_KEYSRC_RF))
                {
                    time_t currentTime = time(NULL); // seconds
                    time_diff = ((currentTime/60) - (last_key_info_param->timestamp/(1000*60))); //converting last_key_info_param->timestamp from milliseconds to minutes
                    if (time_diff <= PREVIOUS_RF4CE_KEY_EVENT_MIN)
                    {
                        WA_INFO("checkRCULastKeyInfo(): Last RF4CE key event timestamp<%lldms>, used %i minutes back\n", last_key_info_param->timestamp, time_diff);
                        result = 1;
                    }
                    else
                    {
                        WA_WARN("checkRCULastKeyInfo(): RF4CE key event time difference is more than %i minutes\n", PREVIOUS_RF4CE_KEY_EVENT_MIN);
                        result = WA_DIAG_ERRCODE_RF4CE_NO_RESPONSE;
                    }
                }
                else
                {
                    WA_DBG("checkRCULastKeyInfo(): last_key_info returned by CtrlMgr is not from RF4CE, controller_id: %i\n", last_key_info_param->controller_id);
                    result = WA_DIAG_ERRCODE_NON_RF4CE_INPUT;
                }
            }
            else
            {
                WA_DBG("checkRCULastKeyInfo(): Invalid status returned by CtrlMgr: %i\n", last_key_info_param->result);
                result = WA_DIAG_ERRCODE_RF4CE_CTRLM_NO_RESPONSE;
            }
        }
        else
            WA_ERROR("checkRCULastKeyInfo(): IARM_Bus_Call() returned %i\n", iarm_result);

        IARM_Free(IARM_MEMTYPE_PROCESSLOCAL, last_key_info_param);
    }
    else
        WA_ERROR("checkRCULastKeyInfo(): IARM_Malloc() failed\n");

    WA_RETURN("checkRCULastKeyInfo(): return code: %d\n", result);

    return result;
}
#endif /* defined(USE_RF4CEMGR) || defined(USE_CTRLMGR) */

#if defined(USE_RF4CEMGR)
/*  returns: -1 comm error, 0 bad, 1 good */
static int checkRf4ceStatusViaRf4ceMgr()
{
    IARM_Result_t iarm_result = IARM_RESULT_IPCCORE_FAIL;
    int is_connected = 0;
    int result = -1;

    WA_ENTER("checkRf4ceStatusViaRf4ceMgr()\n");

    /* Switch to the selected RF4CE API */
    #if defined (RF4CE_GENMSO_API)
        #define RFAPI_MGR_NAME        IARM_BUS_RFMGR_NAME
        #define RFAPI_REQUEST_NAME    IARM_BUS_RFMGR_MsgRequest
        #define RFAPI_REQUEST_PARAM_T IARM_Bus_RfMgr_MsgRequest_Param_t
        #define RFAPI_MSG_RFSTATUS_T  MSOBusAPI_RfStatus_t
        #define RFAPI_MSG_RFSTATUS_ID MSOBusAPI_MsgId_GetRfStatus
        #define RFAPI_MAX_REMOTES     (MSO_BUS_API_MAX_BINDED_REMOTES-1)
    #elif defined(RF4CE_API)
        #define RFAPI_MGR_NAME        IARM_BUS_RF4CEMGR_NAME
        #define RFAPI_REQUEST_NAME    IARM_BUS_RF4CEMGR_MsgRequest
        #define RFAPI_REQUEST_PARAM_T IARM_Bus_rf4ceMgr_MsgRequest_Param_t
        #define RFAPI_MSG_RFSTATUS_T  rf4ce_RfStatus_t
        #define RFAPI_MSG_RFSTATUS_ID rf4ce_MsgId_GetRfStatus
        #define RFAPI_MAX_REMOTES     (RF4CE_MAX_BINDED_REMOTES-1)
    #elif defined(RF4CE_GPMSO_API)
        #define RFAPI_MGR_NAME        IARM_BUS_GPMGR_NAME
        #define RFAPI_REQUEST_NAME    IARM_BUS_GPMGR_API_MsgRequest
        #define RFAPI_REQUEST_PARAM_T IARM_Bus_GpMgr_MsgRequest_Param_t
        #define RFAPI_MSG_RFSTATUS_T  gpMSOBusAPI_RfStatus_t
        #define RFAPI_MSG_RFSTATUS_ID gpMSOBusAPI_MsgId_GetRfStatus
        #define RFAPI_MAX_REMOTES     (GP_MSO_BUS_API_MAX_BINDED_REMOTES-1)
    #else
        #error "No RF4CEMGR API defined"
    #endif

    iarm_result = IARM_Bus_IsConnected(RFAPI_MGR_NAME, &is_connected);
    if (iarm_result != IARM_RESULT_SUCCESS)
        WA_DBG("checkRf4ceStatusViaRf4ceMgr(): IARM_Bus_IsConnected('%s') failed\n", RFAPI_MGR_NAME);

    if (is_connected)
    {
        RFAPI_REQUEST_PARAM_T *param = NULL;

        WA_DBG("checkRf4ceStatusViaRf4ceMgr(): attempting to verify RF interface via %s...\n", RFAPI_MGR_NAME);

        iarm_result = IARM_Malloc(IARM_MEMTYPE_PROCESSLOCAL, sizeof(*param), (void**)&param);
        if ((iarm_result == IARM_RESULT_SUCCESS) && param)
        {
            memset(param, 0, sizeof(*param));
            param->msg.msgId  = RFAPI_MSG_RFSTATUS_ID;
            param->msg.length = sizeof(RFAPI_MSG_RFSTATUS_T);
            param->msg.index = 0;

            WA_DBG("checkRf4ceStatusViaRf4ceMgr(): IARM_Bus_Call('%s', '%s', ...)\n", RFAPI_MGR_NAME, RFAPI_REQUEST_NAME);
            iarm_result = IARM_Bus_Call(RFAPI_MGR_NAME, RFAPI_REQUEST_NAME, (void *)param, sizeof(*param));
            if (iarm_result == IARM_RESULT_SUCCESS)
            {
                if ((param->msg.msgId == RFAPI_MSG_RFSTATUS_ID) && (param->msg.length == sizeof(RFAPI_MSG_RFSTATUS_T)))
                {
                    RFAPI_MSG_RFSTATUS_T *status = (RFAPI_MSG_RFSTATUS_T *)&param->msg.msg;
                    result = verifyRf4ceStatus(status->nbrBindRemotes, RFAPI_MAX_REMOTES, status->activeChannel.Number);
                }
                else
                    WA_ERROR("checkRf4ceStatusViaRf4ceMgr(): unexpected status retured by %s\n", RFAPI_MGR_NAME);
            }
            else
                WA_ERROR("checkRf4ceStatusViaRf4ceMgr(): IARM_Bus_Call() returned %i\n", iarm_result);

            IARM_Free(IARM_MEMTYPE_PROCESSLOCAL, param);
        }
        else
            WA_ERROR("checkRf4ceStatusViaRf4ceMgr(): IARM_Malloc() failed\n");
    }
    else
        WA_INFO("checkRf4ceStatusViaRf4ceMgr(): %s not available\n", RFAPI_MGR_NAME);

    #undef RFAPI_MGR_NAME
    #undef RFAPI_REQUEST_NAME
    #undef RFAPI_REQUEST_PARAM_T
    #undef RFAPI_MSG_RFSTATUS_T
    #undef RFAPI_MSG_RFSTATUS_ID
    #undef RFAPI_MAX_REMOTES

    WA_RETURN("checkRf4ceStatusViaRf4ceMgr(): return code: %d\n", result);

    return result;
}
#endif /* defined(USE_RF4CEMGR) */

#if defined(USE_CTRLMGR)
/*  returns: -1 comm error, 0 bad, 1 good */
static int checkRf4ceStatusViaCtrlMgr()
{
    IARM_Result_t iarm_result = IARM_RESULT_IPCCORE_FAIL;
    int is_connected = 0;
    int result = -1;

    WA_ENTER("checkRf4ceStatusViaCtrlMgr()\n");

    iarm_result = IARM_Bus_IsConnected(CTRLM_MAIN_IARM_BUS_NAME, &is_connected);
    if (iarm_result != IARM_RESULT_SUCCESS || !is_connected)
    {
        WA_ERROR("checkRf4ceStatusViaCtrlMgr(): IARM_Bus_IsConnected('%s') failed. CtrlMgr not available.\n", CTRLM_MAIN_IARM_BUS_NAME);
        return WA_DIAG_ERRCODE_RF4CE_CTRLM_NO_RESPONSE;
    }

    /* Find out the network id of the RF4CE controller */
    ctrlm_main_iarm_call_status_t *ctrm_status_param = NULL;
    ctrlm_network_id_t rf4ce_network_id = -1;

    WA_DBG("checkRf4ceStatusViaCtrlMgr(): attempting to verify RF interface via CtrlMgr...\n");

    iarm_result = IARM_Malloc(IARM_MEMTYPE_PROCESSLOCAL, sizeof(*ctrm_status_param), (void**)&ctrm_status_param);
    if ((iarm_result == IARM_RESULT_SUCCESS) && ctrm_status_param)
    {
        memset(ctrm_status_param, 0, sizeof(*ctrm_status_param));
        ctrm_status_param->api_revision = CTRLM_MAIN_IARM_BUS_API_REVISION;

        WA_DBG("checkRf4ceStatusViaCtrlMgr(): IARM_Bus_Call('%s', '%s', ...)\n", CTRLM_MAIN_IARM_BUS_NAME, CTRLM_MAIN_IARM_CALL_STATUS_GET);
        iarm_result = IARM_Bus_Call(CTRLM_MAIN_IARM_BUS_NAME, CTRLM_MAIN_IARM_CALL_STATUS_GET,
                                   (void *)ctrm_status_param, sizeof(*ctrm_status_param));

        if (iarm_result == IARM_RESULT_SUCCESS)
        {
            if (ctrm_status_param->result == CTRLM_IARM_CALL_RESULT_SUCCESS)
            {
                for (int i = 0; i < ctrm_status_param->network_qty; i++)
                {
                    if (ctrm_status_param->networks[i].type == CTRLM_NETWORK_TYPE_RF4CE)
                    {
                        rf4ce_network_id = ctrm_status_param->networks[i].id;
                        WA_DBG("checkRf4ceStatusViaCtrlMgr(): RF4CE network supported by CtrlMgr, network id: %i\n", rf4ce_network_id);
                        break;
                    }
                }
            }
            else
            {
                WA_DBG("checkRf4ceStatusViaCtrlMgr(): unexpected status retured by CtrlMgr\n");
                result = WA_DIAG_ERRCODE_RF4CE_CTRLM_NO_RESPONSE;
            }
        }
        else
            WA_ERROR("checkRf4ceStatusViaCtrlMgr(): IARM_Bus_Call() returned %i\n", iarm_result);

        IARM_Free(IARM_MEMTYPE_PROCESSLOCAL, ctrm_status_param);
    }
    else
        WA_ERROR("checkRf4ceStatusViaCtrlMgr(): IARM_Malloc() failed\n");

    if (rf4ce_network_id != -1)
    {
        /* Have the network id, try to retrieve the controller status */
        ctrlm_main_iarm_call_network_status_t *network_status_param = NULL;

        iarm_result = IARM_Malloc(IARM_MEMTYPE_PROCESSLOCAL, sizeof(*network_status_param), (void**)&network_status_param);
        if ((iarm_result == IARM_RESULT_SUCCESS) && network_status_param)
        {
            memset(network_status_param, 0, sizeof(*network_status_param));
            network_status_param->api_revision = CTRLM_MAIN_IARM_BUS_API_REVISION;
            network_status_param->network_id = rf4ce_network_id;

            WA_DBG("checkRf4ceStatusViaCtrlMgr(): IARM_Bus_Call('%s', '%s', ...)\n", CTRLM_MAIN_IARM_BUS_NAME, CTRLM_MAIN_IARM_CALL_NETWORK_STATUS_GET);
            iarm_result = IARM_Bus_Call(CTRLM_MAIN_IARM_BUS_NAME, CTRLM_MAIN_IARM_CALL_NETWORK_STATUS_GET,
                                        (void *)network_status_param, sizeof(*network_status_param));

            if (iarm_result == IARM_RESULT_SUCCESS)
            {
                if (network_status_param->result == CTRLM_IARM_CALL_RESULT_SUCCESS)
                {
                    ctrlm_network_status_rf4ce_t *status = (ctrlm_network_status_rf4ce_t *)&network_status_param->status.rf4ce;
                    result = verifyRf4ceStatus(status->controller_qty, CTRLM_MAIN_MAX_BOUND_CONTROLLERS, status->rf_channel_active.number);
                }
                else
                {
                    WA_DBG("checkRf4ceStatusViaCtrlMgr(): invalid status retured by CtrlMgr\n");
                    result = WA_DIAG_ERRCODE_RF4CE_CTRLM_NO_RESPONSE;
                }
            }
            else
                WA_ERROR("checkRf4ceStatusViaCtrlMgr(): IARM_Bus_Call() returned %i\n", iarm_result);

            IARM_Free(IARM_MEMTYPE_PROCESSLOCAL, network_status_param);
        }
        else
            WA_ERROR("checkRf4ceStatusViaCtrlMgr(): IARM_Malloc() failed\n");
    }
    else
        WA_ERROR("checkRf4ceStatusViaCtrlMgr(): RF4CE network not supported by CtrlMgr\n");

    if (!checkOnlyPairing && result == 1)
    {
        result = checkRCULastKeyInfo();
    }

    WA_RETURN("checkRf4ceStatusViaCtrlMgr(): return code: %d\n", result);

    return result;
}

/* Return - 0 Connected, <0 - errors */
static int checkRf4ceChipStatus()
{
    WA_ENTER("checkRf4ceChipStatus()\n");

    int is_connected = 0;
    int result = -1;
    IARM_Result_t iarm_result = IARM_RESULT_IPCCORE_FAIL;

    iarm_result = IARM_Bus_IsConnected(CTRLM_MAIN_IARM_BUS_NAME, &is_connected);
    if (iarm_result != IARM_RESULT_SUCCESS || !is_connected)
    {
        if (checkOnlyPairing == 1)
            WA_DBG("checkRf4ceChipStatus(): IARM_Bus_IsConnected('%s') failed. CtrlMgr not available.\n", CTRLM_MAIN_IARM_BUS_NAME);  
        else
            WA_ERROR("checkRf4ceChipStatus(): IARM_Bus_IsConnected('%s') failed. CtrlMgr not available.\n", CTRLM_MAIN_IARM_BUS_NAME);

        return WA_DIAG_ERRCODE_RF4CE_CTRLM_NO_RESPONSE;
    }

    /* Get Chip connection status */
    ctrlm_main_iarm_call_chip_status_t rf4ce_chip_status;

    memset((void*)&rf4ce_chip_status, 0, sizeof(rf4ce_chip_status));
    rf4ce_chip_status.api_revision = CTRLM_MAIN_IARM_BUS_API_REVISION;
    rf4ce_chip_status.network_id = 1;

    WA_DBG("checkRf4ceChipStatus(): IARM_Bus_Call('%s', '%s', ...)\n", CTRLM_MAIN_IARM_BUS_NAME, CTRLM_MAIN_IARM_CALL_CHIP_STATUS_GET);
    iarm_result = IARM_Bus_Call(CTRLM_MAIN_IARM_BUS_NAME, CTRLM_MAIN_IARM_CALL_CHIP_STATUS_GET, (void*)&rf4ce_chip_status, sizeof(rf4ce_chip_status));

    if (iarm_result == IARM_RESULT_SUCCESS)
    {
        if (rf4ce_chip_status.result == CTRLM_IARM_CALL_RESULT_SUCCESS)
        {
            unsigned char connection_status = rf4ce_chip_status.chip_connected;
            WA_DBG("checkRf4ceChipStatus(): RF4CE chip connection status is(int) : %u\n", (unsigned int)connection_status);
            if ((unsigned int)connection_status == 0) /* If disconnected */
            {
                if (checkOnlyPairing == 1)
                    WA_DBG("checkRf4ceChipStatus(): RF4CE Chip is not connected\n");
                else
                    WA_ERROR("checkRf4ceChipStatus(): RF4CE Chip is not connected\n");

                result = WA_DIAG_ERRCODE_RF4CE_CHIP_DISCONNECTED;
            }
            else
            {
                WA_DBG("checkRf4ceChipStatus(): RF4CE Chip is connected\n");
                result = 0; /* Continue to next step if status is connected */
            }
        }
        else if (rf4ce_chip_status.result == CTRLM_IARM_CALL_RESULT_ERROR_NOT_SUPPORTED)
        {
            WA_DBG("checkRf4ceChipStatus(): RF4CE Chip Connectivity is not supported on this platform\n");
            result = 0; /* Continue to next step if status is not supported (Samsung XG2V2 and CiscoXID) */
        }
        else
        {
            if (checkOnlyPairing == 1)
                WA_DBG("checkRf4ceChipStatus() CtrlMgr CHIP_STATUS_GET failed with result %d", (int)rf4ce_chip_status.result);
            else
                WA_ERROR("checkRf4ceChipStatus() CtrlMgr CHIP_STATUS_GET failed with result %d", (int)rf4ce_chip_status.result);

            result = WA_DIAG_ERRCODE_RF4CE_CTRLM_NO_RESPONSE;
        }
    }
    else
    {
#if (defined(DEVICE_CISCOXID) || defined(DEVICE_SAMSUNGXG2V2))

       WA_DBG("checkRf4ceChipStatus(): IARM_Bus_Call() failed with result %i\n", iarm_result);
       result = 0;

#else

        if (checkOnlyPairing == 1)
            WA_DBG("checkRf4ceChipStatus(): IARM_Bus_Call() failed with result %i, reported as RF4CE Chip Failed\n", iarm_result);
        else
            WA_ERROR("checkRf4ceChipStatus(): IARM_Bus_Call() failed with result %i, reported as RF4CE Chip Failed\n", iarm_result);

        result = WA_DIAG_ERRCODE_RF4CE_CHIP_DISCONNECTED;

#endif
    }

    WA_RETURN("checkRf4ceChipStatus(): return code: %d\n", result);

    return result;
}
#endif /* defined(USE_CTRLMGR) */

static int checkRf4ceStatus(void)
{
    int result = -1;

    WA_ENTER("checkRf4ceStatus()\n");

#if defined(USE_RF4CEMGR)
    result = checkRf4ceStatusViaRf4ceMgr();
#endif

#if defined(USE_CTRLMGR)
    /* Try CtrlMgr if Rf4ceMgr was not accessible */
    if (result == -1)
    {
        result = checkRf4ceChipStatus();
        if (result == 0)
            result = checkRf4ceStatusViaCtrlMgr();
    }
#endif

    WA_RETURN("checkRf4ceStatus(): return code: %d\n", result);

    return result;
}

static int setReturnData(int status, json_t **param)
{
    if(param == NULL)
    {
        WA_ERROR("setReturnData, null pointer error.\n");
        return status;
    }

    switch (status)
    {
    case WA_DIAG_ERRCODE_SUCCESS:
        *param = json_string("Rf4ce receiver good.");
        break;

    case WA_DIAG_ERRCODE_FAILURE:
        *param = json_string("Rf4ce receiver error.");
        break;

    case WA_DIAG_ERRCODE_NOT_APPLICABLE:
        *param = json_string("Not applicable.");
        break;

    case WA_DIAG_ERRCODE_CANCELLED:
        *param = json_string("Test cancelled.");
        break;

    case WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR:
        *param = json_string("Internal test error.");
        break;

    case WA_DIAG_ERRCODE_RF4CE_NO_RESPONSE:
        *param = json_string("RF input not detected.");
        break;

    case WA_DIAG_ERRCODE_NON_RF4CE_INPUT:
        *param = json_string("RF paired but no RF input.");
        break;

    case WA_DIAG_ERRCODE_RF4CE_CTRLM_NO_RESPONSE:
        *param = json_string("RF controller issue.");
        break;

    case WA_DIAG_ERRCODE_RF4CE_CHIP_DISCONNECTED:
        *param = json_string("RF4CE chip failed.");
        break;

    default:
        break;
    }

    return status;
}

/*****************************************************************************
 * EXPORTED FUNCTIONS
 *****************************************************************************/

int WA_DIAG_RF4CE_status(void* instanceHandle, void *initHandle, json_t **params)
{
    int ret;

    json_decref(*params); // not used
    *params = NULL;

    WA_ENTER("WA_DIAG_RF4CE_status\n");

    ret = rf4ce_supported();
    if(ret < 0)
    {
        if(WA_OSA_TaskCheckQuit())
        {
            WA_DBG("WA_DIAG_RF4CE_status: test cancelled\n");
            return setReturnData(WA_DIAG_ERRCODE_CANCELLED, params);
        }

        WA_ERROR("Device configuration unknown.\n");
        return setReturnData(WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR, params);
    }
    if(ret == 0)
    {
        WA_ERROR("Device does not have rf4ce interface.\n");
        return setReturnData(WA_DIAG_ERRCODE_NOT_APPLICABLE, params);
    }

    WA_DBG("Rf4ce supported.\n");

    if(WA_UTILS_IARM_Connect())
    {
        return setReturnData(WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR, params);
    }

    ret = checkRf4ceStatus();

    if(WA_UTILS_IARM_Disconnect())
    {
        WA_DBG("WA_DIAG_RF4CE_status, WA_UTILS_IARM_Disconnect() Failed\n");
    }

    if(ret < 0)
    {
        WA_DBG("WA_DIAG_RF4CE_status, checkRf4ceStatus, return code: %d\n", ret);

        if (ret == -1)
        {
            return setReturnData(WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR, params);
        }

        return setReturnData(ret, params);
    }

    WA_DBG("Rf4ce healthy.\n");

    WA_RETURN("WA_DIAG_RF4CE_status, return code: %d.\n", WA_DIAG_ERRCODE_SUCCESS);

    return setReturnData((ret == 1 ? WA_DIAG_ERRCODE_SUCCESS : WA_DIAG_ERRCODE_FAILURE), params);
}

int isRF4CEPaired()
{
    int ret = -1;

    if (WA_UTILS_IARM_Connect())
    {
        WA_ERROR("isRF4CEPaired(): WA_UTILS_IARM_Connect() Failed \n");
        return -1;
    }

    checkOnlyPairing = 1;

    ret = checkRf4ceStatus();

    checkOnlyPairing = 0;

    if(WA_UTILS_IARM_Disconnect())
    {
        WA_DBG("isRF4CEPaired(): WA_UTILS_IARM_Disconnect() Failed \n");
    }

    return ret;
}


/* End of doxygen group */
/*! @} */

