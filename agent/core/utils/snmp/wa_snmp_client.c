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
 * @brief Implementation of a simple SNMP client.
 */

/** @addtogroup WA_UTILS_SNMP
 *  @{
 */

/*****************************************************************************
 * CONFIGURATION DEFINITIONS
 *****************************************************************************/

#ifndef WA_UTILS_SNMP_APP_NAME
/** Application name, provided to the net-snmp library */
#define WA_UTILS_SNMP_APP_NAME "wa_snmp_app"
#endif

#ifndef WA_UTILS_SNMP_COMMUNITY
/** Community string to be passed to the SNMP server */
#define WA_UTILS_SNMP_COMMUNITY "public"
#endif

#define WA_SNMP_RETRY_COUNT     (5)
#define WA_SNMP_RETRY_DELAY     (100) /* ms */

/*****************************************************************************
 * STANDARD INCLUDE FILES
 *****************************************************************************/
#include <stdbool.h>
#include <unistd.h>

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include "wa_osa.h"
#include "wa_snmp_client.h"
#include "wa_debug.h"

/*****************************************************************************
 * GLOBAL VARIABLE DEFINITIONS
 *****************************************************************************/

/*****************************************************************************
 * LOCAL DEFINITIONS
 *****************************************************************************/

/*****************************************************************************
 * LOCAL TYPES
 *****************************************************************************/

typedef struct WaSnmpSessionData_tag
{
    struct snmp_session *session;
    struct snmp_pdu *response;
} WaSnmpSessionData_t, *WaSnmpSessionData;

typedef struct WaSnmpSingleResult_tag
{
    struct variable_list *var;
    WaSnmpSessionData_t sessionData;
} *WaSnmpSingleResult;

/*****************************************************************************
 * LOCAL FUNCTION PROTOTYPES
 *****************************************************************************/

static WaSnmpSingleResult waSnmpGetSingle(const char *server, const char * reqOidText, WA_UTILS_SNMP_ReqType_t reqType);

static void waSnmpSingleResultCleanup(WaSnmpSingleResult result);
static void waSnmpSingleResultDestroy(WaSnmpSingleResult result);

static void waSnmpSessionDataCleanup(WaSnmpSessionData sd);

static void *snmpMutex = NULL;
/*****************************************************************************
 * LOCAL VARIABLE DECLARATIONS
 *****************************************************************************/

/*****************************************************************************
 * FUNCTION DEFINITIONS
 *****************************************************************************/

int WA_UTILS_SNMP_Init()
{
    snmpMutex = WA_OSA_MutexCreate();
    if (!snmpMutex)
        WA_ERROR("WA_UTILS_SNMP_Init(): failed to allocate mutex\n");

    return snmpMutex ? 0 : -1;
}

int WA_UTILS_SNMP_Exit()
{
    return WA_OSA_MutexDestroy(snmpMutex);
}

bool WA_UTILS_SNMP_GetString(const char *server, const char * reqoid, char * buffer, size_t bufSize, WA_UTILS_SNMP_ReqType_t reqType)
{
    if (!server || !reqoid || !buffer)
    {
        WA_ERROR("WA_UTILS_SNMP_GetString(): Invalid parameters\n");
        return false;
    }

    if(!snmpMutex || WA_OSA_MutexLock(snmpMutex))
    {
        WA_ERROR("WA_UTILS_SNMP_GetString(): Failed to acquire mutex\n");
        return false;
    }

    WaSnmpSingleResult result = waSnmpGetSingle(server, reqoid, reqType);
    if (!result)
    {
        WA_ERROR("WA_UTILS_SNMP_GetString(): waSnmpGetSingle() failed\n");
        WA_OSA_MutexUnlock(snmpMutex);
        return false;
    }

    struct variable_list *var = result->var;

    size_t maxSize = var->val_len;
    if (maxSize + 1 > bufSize)
    {
        maxSize = bufSize - 1;
    }

    strncpy(buffer, (char*)var->val.string, maxSize);
    buffer[maxSize] = 0;

    WA_INFO("WA_UTILS_SNMP_GetString(%s, %s, 0x%08x, %i): returned '%s'\n", server, reqoid, (int)buffer, reqType, buffer);

    waSnmpSingleResultDestroy(result);
    WA_OSA_MutexUnlock(snmpMutex);

    return true;
}

bool WA_UTILS_SNMP_GetLong(const char *server, const char * reqoid, long * buffer, WA_UTILS_SNMP_ReqType_t reqType)
{
    if (!server || !reqoid || !buffer)
    {
        WA_ERROR("WA_UTILS_SNMP_GetLong(): Invalid parameters\n");
        return false;
    }

    if(!snmpMutex || WA_OSA_MutexLock(snmpMutex))
    {
        WA_ERROR("WA_UTILS_SNMP_GetLong(): Failed to acquire mutex\n");
        return false;
    }

    WaSnmpSingleResult result = waSnmpGetSingle(server, reqoid, reqType);
    if (!result)
    {
        WA_ERROR("WA_UTILS_SNMP_GetLong(): waSnmpGetSingle() failed\n");
        WA_OSA_MutexUnlock(snmpMutex);
        return false;
    }

    *buffer = *result->var->val.integer;
    WA_INFO("WA_UTILS_SNMP_GetLong(%s, %s, 0x%08x, %i): returned %li\n", server, reqoid, (int)buffer, reqType, *buffer);

    waSnmpSingleResultDestroy(result);
    WA_OSA_MutexUnlock(snmpMutex);

    return true;
}

/*****************************************************************************
 * LOCAL FUNCTIONS
 *****************************************************************************/

/**
 * Retrieves a single value associated with the OID specified from SNMP server.
 *
 * Starts net-snmp session, issues a synchronous SNMP GET request and returns the result.
 *
 * The result struct should be released by calling waSnmpSingleResultDestroy when it is no longer needed.
 */
static WaSnmpSingleResult waSnmpGetSingle(const char *server, const char * reqOidText, WA_UTILS_SNMP_ReqType_t reqType)
{
    WaSnmpSessionData_t sd;
    int retries = WA_SNMP_RETRY_COUNT;

    struct snmp_session session;
    struct snmp_pdu *pdu;

    oid reqOid[MAX_OID_LEN];
    size_t reqOidLength = MAX_OID_LEN;

    WA_ENTER("waSnmpGetSingle(%s, %s, %i)\n", server, reqOidText, reqType);

    init_snmp(WA_UTILS_SNMP_APP_NAME);

    snmp_sess_init(&session);
    session.peername = (char *)server;
    session.version = SNMP_VERSION_1;
    session.community = (u_char*)WA_UTILS_SNMP_COMMUNITY;
    session.community_len = strlen((char*)session.community);

    for (;;)
    {
        memset(&sd, 0, sizeof(sd));
        SOCK_STARTUP;

        sd.session = snmp_open(&session); /* establish the session */
        if (!sd.session)
        {
            WA_ERROR("waSnmpGetSingle(): snmp_open returned NULL\n");
            SOCK_CLEANUP;
            return NULL;
        }

        switch(reqType)
        {
        case WA_UTILS_SNMP_REQ_TYPE_GET:
            pdu = snmp_pdu_create(SNMP_MSG_GET);
            break;

        case WA_UTILS_SNMP_REQ_TYPE_WALK:
            pdu = snmp_pdu_create(SNMP_MSG_GETNEXT);
            break;

        default:
            WA_ERROR("waSnmpGetSingle(): invalid type (%i) requested\n", reqType);
            waSnmpSessionDataCleanup(&sd);
            return NULL;
        }

        get_node(reqOidText, reqOid, &reqOidLength);
        snmp_add_null_var(pdu, reqOid, reqOidLength);

        int rc = snmp_synch_response(sd.session, pdu, &sd.response);

        if ((rc != STAT_SUCCESS) || (sd.response->errstat != SNMP_ERR_NOERROR) || !sd.response->variables)
        {
            WA_ERROR("waSnmpGetSingle(): snmp_synch_response() returned %i\n", rc);
            waSnmpSessionDataCleanup(&sd);

            if (rc == STAT_TIMEOUT)
            {
                if (--retries)
                {
                    WA_INFO("waSnmpGetSingle(): snmp_synch_response() timed out, retrying (retries left: %i)\n", retries);
                    usleep(WA_SNMP_RETRY_DELAY * 1000);
                    continue;
                }

                WA_ERROR("waSnmpGetSingle(): exhausted retries, giving up...\n");
            }

            return NULL;
        }
        else
        {
            WA_INFO("waSnmpGetSingle(): snmp_synch_response() successful (retries: %i)\n", WA_SNMP_RETRY_COUNT - retries);
            break;
        }
    }

    WaSnmpSingleResult result = calloc(1, sizeof(*result));

    result->var = sd.response->variables;
    result->sessionData = sd;

    return result;
}

/**
 * Releases the resources associated with SNMP GET result and closes the associated net-snmp session, and destroys the result struct itself too.
 */
static void waSnmpSingleResultDestroy(WaSnmpSingleResult result)
{
    waSnmpSingleResultCleanup(result);
    free(result);
}

/**
 * Releases the resources associated with SNMP GET result and closes the associated net-snmp session.
 */
static void waSnmpSingleResultCleanup(WaSnmpSingleResult result)
{
    waSnmpSessionDataCleanup(&result->sessionData);
}

/**
 * Closes net-snmp session.
 */
static void waSnmpSessionDataCleanup(WaSnmpSessionData sd)
{
    if (sd->response)
    {
        snmp_free_pdu(sd->response);
        sd->response = NULL;
    }

    snmp_close(sd->session);
    sd->session = NULL;

    SOCK_CLEANUP;
}
