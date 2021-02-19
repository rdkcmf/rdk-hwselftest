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
 * @brief Diagnostic functions for Cable Modem - implementation
 */

/** @addtogroup WA_DIAG_MODEM
 *  @{
 */

/*****************************************************************************
 * STANDARD INCLUDE FILES
 *****************************************************************************/

#include <stdbool.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <asm/types.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <ifaddrs.h>
#include <poll.h>
#include <sys/time.h>
#include <arpa/inet.h>

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/

#include "wa_diag.h"
#include "wa_debug.h"
#include "wa_fileops.h"
#include "wa_snmp_client.h"

/* module interface */
#include "wa_diag_modem.h"

/*****************************************************************************
 * GLOBAL VARIABLE DEFINITIONS
 *****************************************************************************/

/*****************************************************************************
 * LOCAL DEFINITIONS
 *****************************************************************************/
#define DEV_CONFIG_FILE_PATH  "/etc/device.properties"
#define FILE_MODE             "r"
#define MODEM_OPTION_STR      "ESTB_ECM_COMMN_IP="
#define MODEM_IP              "GATEWAY_IP="
#define STR_MAX 1256
#define OID_MODEM_STATUS "DOCS-IF3-MIB::docsIf3CmStatusValue"
#define OID_DOWN_WIDTH "DOCS-IF-MIB::docsIfDownChannelWidth"
#define OID_DOWN_MODULATION "DOCS-IF-MIB::docsIfDownChannelModulation"
#define OID_DOWN_INTERLEAVE "DOCS-IF-MIB::docsIfDownChannelInterleave"

#define STEP_TIME 5 /* in [s] */
#define TOTAL_STEPS 12
#define IOD_VALUE_OPERATIONAL 12

/*****************************************************************************
 * LOCAL TYPES
 *****************************************************************************/


/*****************************************************************************
 * LOCAL FUNCTION PROTOTYPES
 *****************************************************************************/
static bool modem_supported(void);
static char *get_modem_ip(void);
static int is_modem_operational(const char *snmp_server);
static int validate_modem_state(void* instanceHandle, const char *snmp_server);

/*****************************************************************************
 * LOCAL VARIABLE DECLARATIONS
 *****************************************************************************/

/*****************************************************************************
 * FUNCTION DEFINITIONS
 *****************************************************************************/

/*****************************************************************************
 * EXPORTED FUNCTIONS
 *****************************************************************************/

int WA_DIAG_MODEM_status(void* instanceHandle, void *initHandle, json_t **params)
{
    int status = WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR;
    char *modem_ip = NULL;
    char buf[STR_MAX];

    json_decref(*params); // not used
    *params = NULL;

    WA_ENTER("modem_status\n");

    /* IF modem_status test not applicable THEN */
    if(!modem_supported())
    {

        if(WA_OSA_TaskCheckQuit())
        {
            WA_DBG("modem_status: test cancelled\n");
        }
        else
        {
            *params = json_string("Not applicable.");
            status = WA_DIAG_ERRCODE_NOT_APPLICABLE;
        }

        goto end;
    }

    modem_ip = get_modem_ip();
    if(modem_ip != NULL)
    {
        snprintf(buf, STR_MAX, "ping -c 1 -W 1 %s &> /dev/null", modem_ip);
        if(system(buf) != 0)
        {
            *params = json_string("HW not accessible.");
            WA_ERROR("modem_status: ping failed to gateway ip %s\n", modem_ip);
            status = WA_DIAG_ERRCODE_FAILURE;
            goto end;
        }
    }
    else
    {
        if(WA_OSA_TaskCheckQuit())
        {
            WA_DBG("modem_status: get_modem_ip: test cancelled\n");
        }
        else
        {
            *params = json_string("Modem not configured.");
        }

        goto end;
    }

    status = validate_modem_state(instanceHandle, modem_ip);
    if(status > 0)
    {
        *params = json_string("HW not accessible.");
    }
    if(status < 0)
    {
        if(WA_OSA_TaskCheckQuit())
        {
            WA_DBG("modem_status: validate_modem_state: test cancelled\n");
        }
        else
        {
            *params = json_string("Modem not operational.");
        }
    }

end:

    free(modem_ip);
    if((status != 0) && WA_OSA_TaskCheckQuit())
    {
        json_decref(*params);
        *params = json_string("Test cancelled.");
        status = WA_DIAG_ERRCODE_CANCELLED;
    }
    WA_RETURN("modem_status %d\n", status);
    return status;
}


/*****************************************************************************
 * LOCAL FUNCTIONS
 *****************************************************************************/
/**
 * @brief Check if CM is supported.
 *
 * @retval true  supported
 * @retval false not supported or cancelled
 */
static bool modem_supported(void)
{
    int status = false;
    char *addr;

    addr = WA_UTILS_FILEOPS_OptionFind(DEV_CONFIG_FILE_PATH, MODEM_OPTION_STR);

    if(WA_OSA_TaskCheckQuit())
    {
        free(addr);
        WA_DBG("modem_supported: test cancelled\n");
        return false;
    }

    if(addr == NULL)
        return false;

    status = strlen(addr) > 6 /* x.x.x.x */;
    free(addr);
    return status;
}

/**
 * @brief Get modem IP.
 *
 * @returns the modem IP
 * @retval null error or cancelled
 */
static char *get_modem_ip(void)
{
    char *addr;
    addr = WA_UTILS_FILEOPS_OptionFind(DEV_CONFIG_FILE_PATH, MODEM_IP);

    if(WA_OSA_TaskCheckQuit())
    {
        free(addr);
        WA_DBG("get_modem_ip: test cancelled\n");
        return NULL;
    }

    if(addr == NULL)
        return NULL;

    if(strlen(addr) <= 6 /* x.x.x.x */)
    {
        free(addr);
        return NULL;
    }

    return addr;
}

/**
 * @brief Check ig CM is operational
 *
 * @retval 0 is operational
 * @retval -1 not operational or cancelled
 * @retval 1 cannot access modem
 *
 */
static int is_modem_operational(const char *snmp_server)
{
    WA_UTILS_SNMP_Resp_t oper, width, mod, interleave;
    int status = WA_DIAG_ERRCODE_SUCCESS;

    oper.type = WA_UTILS_SNMP_RESP_TYPE_LONG;
    if(!WA_UTILS_SNMP_GetNumber(snmp_server, OID_MODEM_STATUS, &oper, WA_UTILS_SNMP_REQ_TYPE_WALK))
    {
        return WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR;
    }

    if(WA_OSA_TaskCheckQuit())
    {
        WA_DBG("is_modem_operational: modem status: test cancelled\n");
        return WA_DIAG_ERRCODE_CANCELLED;
    }

    width.type = WA_UTILS_SNMP_RESP_TYPE_LONG;
    if(!WA_UTILS_SNMP_GetNumber(snmp_server, OID_DOWN_WIDTH, &width, WA_UTILS_SNMP_REQ_TYPE_WALK))
    {
        return WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR;
    }

    if(WA_OSA_TaskCheckQuit())
    {
        WA_DBG("is_modem_operational: down_width: test cancelled\n");
        return WA_DIAG_ERRCODE_CANCELLED;
    }

    mod.type = WA_UTILS_SNMP_RESP_TYPE_LONG;
    if(!WA_UTILS_SNMP_GetNumber(snmp_server, OID_DOWN_MODULATION, &mod, WA_UTILS_SNMP_REQ_TYPE_WALK))
    {
        return WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR;
    }

    if(WA_OSA_TaskCheckQuit())
    {
        WA_DBG("is_modem_operational: down_modulation: test cancelled\n");
        return WA_DIAG_ERRCODE_CANCELLED;
    }

    interleave.type = WA_UTILS_SNMP_RESP_TYPE_LONG;
    if(!WA_UTILS_SNMP_GetNumber(snmp_server, OID_DOWN_INTERLEAVE, &interleave, WA_UTILS_SNMP_REQ_TYPE_WALK))
    {
        return WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR;
    }

    if(WA_OSA_TaskCheckQuit())
    {
        WA_DBG("is_modem_operational: down_interleave: test cancelled\n");
        return WA_DIAG_ERRCODE_CANCELLED;
    }

    if((oper.data.l != IOD_VALUE_OPERATIONAL) || (width.data.l == 0) || (mod.data.l == 1) || (interleave.data.l == 1))
    {
        status = WA_DIAG_ERRCODE_CM_NO_SIGNAL;
    }

    return status;
}

/**
 * @brief Periodic check of modem status with progress update.
 *
 * @retval 0 is operational
 * @retval -1 not operational or cancelled
 * @retval 1 cannot access modem
 *
 */
static int validate_modem_state(void* instanceHandle, const char *snmp_server)
{
    int step = 0;
    int status = WA_DIAG_ERRCODE_SUCCESS;

    while(1)
    {
        ++step;
        status = is_modem_operational(snmp_server);

        if(WA_OSA_TaskCheckQuit())
        {
            WA_DBG("validate_modem_state: test cancelled\n");
            status = WA_DIAG_ERRCODE_CANCELLED;
            break;
        }

        if((status >= 0) || (step > TOTAL_STEPS))
        {
            break;
        }
        WA_DIAG_SendProgress(instanceHandle, (step * 100)/(TOTAL_STEPS+1));

        sleep(STEP_TIME);
    }

    return status;
}

/* End of doxygen group */
/*! @} */

