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

static bool waSnmpFindIndexInOid(oid *input, size_t input_length, int *index);
static WaSnmpSingleResult waSnmpGetSingle(const char *server, const char * reqOidText, WA_UTILS_SNMP_ReqType_t reqType);

static void waSnmpSingleResultCleanup(WaSnmpSingleResult result);
static void waSnmpSingleResultDestroy(WaSnmpSingleResult result);

static void waSnmpSessionDataCleanup(WaSnmpSessionData sd);

static void *snmpMutex = NULL;
int MAX_CONTENT_LEN = 2048;
static const char *SNMPD_CONF_FILE = "/tmp/snmpd.conf";
static const char *SNMP_DELIMITER = " ";
static char communityString[128] = "public";
/*****************************************************************************
 * LOCAL VARIABLE DECLARATIONS
 *****************************************************************************/

/*****************************************************************************
 * FUNCTION DEFINITIONS
 *****************************************************************************/

void updateCommunityString(void) {
    FILE *snmpConfPtr = NULL;
    char *linePtr = NULL;
    char *tempCommString = NULL;
    int retVal = -1;
    snmpConfPtr = fopen( SNMPD_CONF_FILE, "r");
    if (NULL != snmpConfPtr) {
       retVal = getline( &linePtr, (size_t*) &MAX_CONTENT_LEN, snmpConfPtr);
       fclose(snmpConfPtr);
    }
    if ( -1 != retVal ) {
        if (linePtr){
            strtok(linePtr, SNMP_DELIMITER);
            strtok(NULL, SNMP_DELIMITER);
            strtok(NULL, SNMP_DELIMITER);
            tempCommString = strtok(NULL, SNMP_DELIMITER);
        }
    }
    if (tempCommString) {
        strncpy(communityString, tempCommString, strlen(tempCommString));
    }
    if(linePtr) {
        free(linePtr);
    }
    return;
}

int WA_UTILS_SNMP_Init()
{
    snmpMutex = WA_OSA_MutexCreate();
    if (!snmpMutex)
        WA_ERROR("WA_UTILS_SNMP_Init(): failed to allocate mutex\n");

    updateCommunityString();
    init_snmp(WA_UTILS_SNMP_APP_NAME);

    return snmpMutex ? 0 : -1;
}

int WA_UTILS_SNMP_Exit()
{
    snmp_shutdown(WA_UTILS_SNMP_APP_NAME);
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

bool WA_UTILS_SNMP_GetNumber(const char *server, const char *reqoid, WA_UTILS_SNMP_Resp_t *buffer, WA_UTILS_SNMP_ReqType_t reqType)
{
    bool ret_val;

    if (!server || !reqoid || !buffer)
    {
        WA_ERROR("WA_UTILS_SNMP_GetNumber(): Invalid parameters\n");
        return false;
    }

    if(!snmpMutex || WA_OSA_MutexLock(snmpMutex))
    {
        WA_ERROR("WA_UTILS_SNMP_GetNumber(): Failed to acquire mutex\n");
        return false;
    }

    WaSnmpSingleResult result = waSnmpGetSingle(server, reqoid, reqType);
    if (!result)
    {
        WA_ERROR("WA_UTILS_SNMP_GetNumber(): waSnmpGetSingle() failed\n");
        WA_OSA_MutexUnlock(snmpMutex);
        return false;
    }

    switch(buffer->type)
    {
	case WA_UTILS_SNMP_RESP_TYPE_LONG:

	    buffer->data.l = *result->var->val.integer;
            WA_INFO("WA_UTILS_SNMP_GetNumber(server: %s, oid: %s, long: 0x%08x, reqType: %i): returned %lu\n",
                server, reqoid, (int)buffer->data.l, reqType, *result->var->val.integer);
            ret_val = true;
            break;

        case WA_UTILS_SNMP_RESP_TYPE_COUNTER64:

            buffer->data.c64.high = result->var->val.counter64->high;
            buffer->data.c64.low = result->var->val.counter64->low;
            WA_INFO("WA_UTILS_SNMP_GetNumber(server: %s, oid: %s, counter64: len: %u, high: %lu, low: %lu]\n",
                 server, reqoid, result->var->val_len, result->var->val.counter64->high, result->var->val.counter64->low);
            ret_val = true;
            break;

        default:
            WA_ERROR("WA_UTILS_SNMP_GetNumber(): Invalid response type specified\n");
            ret_val = false;
            break;
    }

    waSnmpSingleResultDestroy(result);
    WA_OSA_MutexUnlock(snmpMutex);

    return ret_val;
}

bool WA_UTILS_SNMP_FindIfIndex(const char *server, const char *reqoid, int *ifIndex)
{
    int idx;
    bool ret_val;

    if (!server || !reqoid || !ifIndex)
    {
        WA_ERROR("WA_UTILS_SNMP_FindIfIndex(): Invalid parameters\n");
        return false;
    }

    if(!snmpMutex || WA_OSA_MutexLock(snmpMutex))
    {
        WA_ERROR("WA_UTILS_SNMP_FindIfIndex(): Failed to acquire mutex\n");
        return false;
    }

    WaSnmpSingleResult result = waSnmpGetSingle(server, reqoid, WA_UTILS_SNMP_REQ_TYPE_WALK);
    if (!result)
    {
        WA_ERROR("WA_UTILS_SNMP_FindIfIndex(): waSnmpGetSingle() failed\n");
        WA_OSA_MutexUnlock(snmpMutex);
        return false;
    }

    WA_DBG("WA_UTILS_SNMP_FindIfIndex(): response:\n");
    //print_objid(result->var->name, result->var->name_length);

    if(!waSnmpFindIndexInOid(result->var->name, result->var->name_length, &idx))
    {
        WA_ERROR("WA_UTILS_SNMP_FindIfIndex(): index not found\n");
        ret_val = false;
    }
    else
    {
        WA_DBG("WA_UTILS_SNMP_FindIfIndex(): index: %d\n", idx);
        *ifIndex = idx;
        ret_val = true;
    }

    waSnmpSingleResultDestroy(result);
    WA_OSA_MutexUnlock(snmpMutex);

    return ret_val;
}


/*****************************************************************************
 * LOCAL FUNCTIONS
 *****************************************************************************/
/**
 * Parses oid looking for ifIndex
 */
static bool waSnmpFindIndexInOid(oid *input, size_t input_length, int *index)
{
    int idx;
    char oid_str[MAX_OID_LEN];
    char buff[MAX_OID_LEN];

    //print_objid(input, input_length);

    snprint_objid(oid_str, sizeof(oid_str), input, input_length);

    if(sscanf(oid_str, "%[^.].%d", buff, &idx) != 2)
    {
        WA_ERROR("WA_UTILS_SNMP_waSnmpFindIndexInOid(): index not found\n");
        return false;
    }

    WA_DBG("WA_UTILS_SNMP_waSnmpFindIndexInOid(): buff: %s, index: %d\n", buff, idx);
    *index = idx;

    return true;
}


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
    struct variable_list *vars;

    oid reqOid[MAX_OID_LEN];
    size_t reqOidLength = MAX_OID_LEN;

    WA_ENTER("waSnmpGetSingle(%s, %s, %i)\n", server, reqOidText, reqType);

    snmp_sess_init(&session);
    session.peername = (char *)server;
    session.version = SNMP_VERSION_2c;
    session.community = (u_char*)communityString;
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

        reqOidLength = MAX_OID_LEN;
        if(get_node(reqOidText, reqOid, &reqOidLength))
        {
            WA_DBG("waSnmpGetSingle(): get_node: node correct: %s\n", reqOidText);
            print_objid(reqOid, reqOidLength);
        }
        else
        {
            WA_DBG("waSnmpGetSingle(): get_node: incorrect node: %s\n", reqOidText);
            waSnmpSessionDataCleanup(&sd);
            return NULL;
        }

        snmp_add_null_var(pdu, reqOid, reqOidLength);

        int rc = snmp_synch_response(sd.session, pdu, &sd.response);

        if ((rc != STAT_SUCCESS) || (sd.response->errstat != SNMP_ERR_NOERROR) || !sd.response->variables)
        {
            WA_ERROR("waSnmpGetSingle(): snmp_synch_response() returned %i, errstat: %li, vars: %s\n",
                rc, sd.response ? sd.response->errstat : -1, sd.response ? (sd.response->variables ? "ok" : "nok") : "");

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

    for(vars = sd.response->variables; vars; vars = vars->next_variable)
    {
        //print_objid(vars->name, vars->name_length);
        //print_value(vars->name, vars->name_length, vars);

        if(netsnmp_oid_is_subtree(reqOid, reqOidLength, vars->name, vars->name_length))
        {
            WA_ERROR("waSnmpGetSingle(): not corelated response, giving up...\n");
            waSnmpSessionDataCleanup(&sd);
            return NULL;
        }

        if(vars->val_len == 0)
        {
            WA_ERROR("waSnmpGetSingle(): empty response, giving up...\n");
            waSnmpSessionDataCleanup(&sd);
            return NULL;
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
