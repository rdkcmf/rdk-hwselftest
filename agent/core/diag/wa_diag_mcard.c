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
 * @brief Diagnostic functions for M-Card - interface
 */

/** @addtogroup WA_DIAG_MCARD
 *  @{
 */

/*****************************************************************************
 * STANDARD INCLUDE FILES
 *****************************************************************************/

#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <stdlib.h>
#include <sys/types.h>
#include <asm/types.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <ifaddrs.h>
#include <poll.h>
#include <sys/time.h>

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/

#include "wa_diag.h"
#include "wa_debug.h"
#include "wa_snmp_client.h"

/* module interface */
#include "wa_diag_mcard.h"

/*****************************************************************************
 * GLOBAL VARIABLE DEFINITIONS
 *****************************************************************************/

/*****************************************************************************
 * LOCAL DEFINITIONS
 *****************************************************************************/
#define OID_MCARD_STATE "VIVIDLOGIC-MIB::VividlogicMcardAcHostId"
#define OID_MCARD_CERT_AVAILABLE "VIVIDLOGIC-MIB::VividlogicMcardbCertAvailable"
#define OID_MCARD_CERT_VERIFIED "VIVIDLOGIC-MIB::VividlogicMcardbCertValid"

#define SNMP_SUCCESS_STR "Yes"
#define SNMP_SUCCESS_STR_CERT_AVAILABLE (SNMP_SUCCESS_STR)
#define SNMP_SUCCESS_STR_CERT_VERIFIED  (SNMP_SUCCESS_STR)

#define OID_MCARD_AUTH_KEY_STATUS "OC-STB-HOST-MIB::ocStbHostCardCpAuthKeyStatus"
#define SNMP_MCARD_AUTH_KEY_READY 1

#define SNMP_SERVER "localhost"

/*****************************************************************************
 * LOCAL TYPES
 *****************************************************************************/
typedef enum {
    MCARD_OPT_ID_READ,
    MCARD_OPT_CERT_READ,
    MCARD_OPT_CERT_VALIDATE,
    MCARD_OPT_MAX,
} MCardOption_t;

/*****************************************************************************
 * LOCAL FUNCTION PROTOTYPES
 *****************************************************************************/
static int getMCardId(size_t size, char *value);
static int getMCardCert(size_t size, char *value);
static int checkMCardCert(size_t size, char *value);
static int getMCardOptionStatus(MCardOption_t opt, size_t size, char *value);
static int verifyOptionValue(MCardOption_t opt, char *value);
static int setReturnData(int status, json_t **param);

/*****************************************************************************
 * LOCAL VARIABLE DECLARATIONS
 *****************************************************************************/
char *MCardOptions[MCARD_OPT_MAX] = {
    OID_MCARD_STATE,
    OID_MCARD_CERT_AVAILABLE,
    OID_MCARD_CERT_VERIFIED
};

/*****************************************************************************
 * FUNCTION DEFINITIONS
 *****************************************************************************/

/*****************************************************************************
 * LOCAL FUNCTIONS
 *****************************************************************************/

static int getMCardOptionStatus(MCardOption_t opt, size_t size, char *value)
{
    int ret = -1;
    char oid[256];

    WA_ENTER("getMCardOptionStatus (%s).\n", MCardOptions[opt]);

    if(value == NULL)
    {
        WA_ERROR("getMCardOptionStatus, null pointer error.\n");
        return ret;
    }

    ret = snprintf(oid, sizeof(oid), "%s", MCardOptions[opt]);
    if ((ret >= sizeof(oid)) || (ret < 0))
    {
        WA_ERROR("getMCardOptionStatus, unable to generate OID.\n");
        return -1;
    }

    if(!WA_UTILS_SNMP_GetString(SNMP_SERVER, oid, value, size, WA_UTILS_SNMP_REQ_TYPE_WALK))
    {
        WA_ERROR("getMCardOptionStatus, option: %s, failed.\n", MCardOptions[opt]);
        return -1;
    }

    WA_RETURN("getMCardOptionStatus, option: %s, state: %s.\n", MCardOptions[opt], value);

    return verifyOptionValue(opt, value);
}

static int verifyOptionValue(MCardOption_t opt, char *value)
{
    int ret = -1;

    WA_ENTER("verifyOptionValue (%s).\n", MCardOptions[opt]);

    if(value == NULL)
    {
        WA_ERROR("verifyOptionValue: null pointer error.\n");
        return ret;
    }

    switch(opt)
    {
        case MCARD_OPT_ID_READ:
            ret = (value != NULL ? 1 : 0);
            break;

        case MCARD_OPT_CERT_READ:
            ret = (strcmp(value, SNMP_SUCCESS_STR_CERT_AVAILABLE) == 0 ? 1 : 0);
            break;

        case MCARD_OPT_CERT_VALIDATE:
            ret = (strcmp(value, SNMP_SUCCESS_STR_CERT_VERIFIED) == 0 ? 1 : 0);
            break;

        default:
            break;
    }

    WA_RETURN("verifyOptionValue (%s), return code: %d.\n", MCardOptions[opt], ret);

    return ret;
}

static int getMCardId(size_t size, char *value)
{
    return getMCardOptionStatus(MCARD_OPT_ID_READ, size, value);
}

static int getMCardCert(size_t size, char *value)
{
    return getMCardOptionStatus(MCARD_OPT_CERT_READ, size, value);
}

static int checkMCardCert(size_t size, char *value)
{
    return getMCardOptionStatus(MCARD_OPT_CERT_VALIDATE, size, value);
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
           *param = json_string("MCard good.");
           break;

        case WA_DIAG_ERRCODE_FAILURE:
           *param = json_string("MCard bad.");
           break;

/* mapped to WA_DIAG_ERRCODE_FAILURE
        case WA_DIAG_ERRCODE_MCARD_CERT_INVALID:
            *param = json_string("MCard certificate not valid.");
            break;
*/

        case WA_DIAG_ERRCODE_NOT_APPLICABLE:
            *param = json_string("Not applicable.");
            break;

        case WA_DIAG_ERRCODE_CANCELLED:
            *param = json_string("Test cancelled.");
            break;

        case WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR:
            *param = json_string("Internal test error.");
            break;

        default:
            break;
    }

    return status;
}

int isCardInserted()
{
    int status = 0;
    long ready = 0;

    if (!WA_UTILS_SNMP_GetLong(SNMP_SERVER, OID_MCARD_AUTH_KEY_STATUS, &ready, WA_UTILS_SNMP_REQ_TYPE_WALK))
    {
      WA_ERROR("isCardInserted: SNMP read failed\n");
      status = -1;
    }
    else
    {
      status = (ready == SNMP_MCARD_AUTH_KEY_READY);
      WA_INFO("isCardInserted: card present: %s\n", (status? "YES" : "NO"));
    }

    return status;
}

/*****************************************************************************
 * EXPORTED FUNCTIONS
 *****************************************************************************/

int WA_DIAG_MCARD_status(void* instanceHandle, void *initHandle, json_t **params)
{
    int ret;
    char value[256];
    size_t size = sizeof(value);
    json_t *config;
    int applicable;

    json_decref(*params); // not used

    WA_ENTER("mcard_status\n");

    /* Determine if the test is applicable: */
    config = ((WA_DIAG_proceduresConfig_t*)initHandle)->config;
    if(config && !json_unpack(config, "{sb}", "applicable", &applicable))
    {
        if(!applicable)
        {
            WA_INFO("Device does not have MCard.\n");
            return setReturnData(WA_DIAG_ERRCODE_NOT_APPLICABLE, params);
        }
    }

    WA_DBG("MCard supported.\n");

    /* Check if card is inserted before doing the test */
    ret = isCardInserted();
    if (ret < 1)
    {
      WA_ERROR("mcard_status, isCardInserted, error code: %d\n", ret);
      return setReturnData(ret == 0? WA_DIAG_ERRCODE_FAILURE : WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR, params);
    }

    ret = getMCardId(size, &value[0]);
    if(ret < 1)
    {
        if(WA_OSA_TaskCheckQuit())
        {
            WA_DBG("getMCardId: test cancelled\n");
            return setReturnData(WA_DIAG_ERRCODE_CANCELLED, params);
        }

        WA_ERROR("mcard_status, getMCardId, error code: %d\n", ret);
        return setReturnData((ret == 0 ? WA_DIAG_ERRCODE_FAILURE : WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR), params);
    }

    WA_DBG("MCard id read successfully.\n");

    ret = getMCardCert(size, &value[0]);
    if(ret < 1)
    {
        if(WA_OSA_TaskCheckQuit())
        {
            WA_DBG("getMCardCert: test cancelled\n");
            return setReturnData(WA_DIAG_ERRCODE_CANCELLED, params);
        }

        WA_ERROR("mcard_status, getMCartCert, error code: %d\n", ret);
        return setReturnData((ret == 0 ? WA_DIAG_ERRCODE_MCARD_CERT_INVALID : WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR), params);
    }

    WA_DBG("MCard certificate present.\n");

    ret = checkMCardCert(size, &value[0]);
    if(ret < 1)
    {
        if(WA_OSA_TaskCheckQuit())
        {
            WA_DBG("checkMCardCert: test cancelled\n");
            return setReturnData(WA_DIAG_ERRCODE_CANCELLED, params);
        }

        WA_ERROR("mcard_status, checkMCardCert, error code: %d\n", ret);
        return setReturnData((ret == 0 ? WA_DIAG_ERRCODE_MCARD_CERT_INVALID : WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR), params);
    }

    WA_DBG("MCard certificate valid.\n");

    WA_RETURN("mcard_status, return code: %d.\n", WA_DIAG_ERRCODE_SUCCESS);

    return setReturnData(WA_DIAG_ERRCODE_SUCCESS, params);
}


/* End of doxygen group */
/*! @} */
