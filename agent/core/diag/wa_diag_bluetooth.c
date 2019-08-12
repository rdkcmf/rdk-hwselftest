/*
 * If not stated otherwise in this file or this component's Licenses.txt file the
 * following copyright and licenses apply:
 *
 * Copyright 2018 RDK Management
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
 * @brief Diagnostic functions for Bluetooth - implementation
 */

/** @addtogroup WA_DIAG_BLUETOOTH
 *  @{
 */

/*****************************************************************************
 * STANDARD INCLUDE FILES
 *****************************************************************************/

#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/

#include "wa_diag.h"
#include "wa_debug.h"
#include "wa_fileops.h"

/* module interface */
#include "wa_diag_bluetooth.h"

/*****************************************************************************
 * GLOBAL VARIABLE DEFINITIONS
 *****************************************************************************/

/*****************************************************************************
 * LOCAL DEFINITIONS
 *****************************************************************************/
#define DEV_CONFIG_FILE_PATH  "/etc/device.properties"
#define BLUETOOTH_OPTION_STR      "BLUETOOTH_ENABLED="
#define STR_MAX 256
#ifndef MEDIA_CLIENT
#define DIR_PATTERN "/sys/bus/platform/devices/*.serial/tty/ttyS*/hci*"
#else
#define DIR_PATTERN "/sys/class/bluetooth/hci*"
#endif
#define MAX_INTERFACES 3
#define MAX_DIR 100
#define IF_PREFIX "hci"

/*****************************************************************************
 * LOCAL TYPES
 *****************************************************************************/
typedef int hci_t[MAX_INTERFACES];

/*****************************************************************************
 * LOCAL FUNCTION PROTOTYPES
 *****************************************************************************/
static bool bluetooth_supported(void);

/*****************************************************************************
 * LOCAL VARIABLE DECLARATIONS
 *****************************************************************************/

/*****************************************************************************
 * FUNCTION DEFINITIONS
 *****************************************************************************/

/*****************************************************************************
 * EXPORTED FUNCTIONS
 *****************************************************************************/
extern int WA_DIAG_BLUETOOTH_GetInternalInterfaces(hci_t hci);
extern int WA_DIAG_BLUETOOTH_Verify(int hci);

int WA_DIAG_BLUETOOTH_status(void* instanceHandle, void *initHandle, json_t **params)
{
    int status = WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR;
    hci_t hci;
    int hciNum;
    int v, i;

    json_decref(*params); // not used
    *params = NULL;

    WA_ENTER("bluetooth_status\n");

    /* IF bluetooth_status test not applicable THEN */
    if(!bluetooth_supported())
    {

        if(WA_OSA_TaskCheckQuit())
        {
            WA_DBG("bluetooth_status: test cancelled\n");
        }
        else
        {
            *params = json_string("Not applicable.");
            status = WA_DIAG_ERRCODE_NOT_APPLICABLE;
        }

        goto end;
    }

    hciNum = WA_DIAG_BLUETOOTH_GetInternalInterfaces(hci);
    if(hciNum == 0)
    {
        *params = json_string("No bluetooth devices found.");
        status = WA_DIAG_ERRCODE_FAILURE;
        goto end;
    }

    status = WA_DIAG_ERRCODE_SUCCESS;
    i = hciNum;
    while(i--)
    {
        if(WA_OSA_TaskCheckQuit())
        {
            WA_DBG("bluetooth_status: test cancelled\n");
            break;
        }

        v = WA_DIAG_BLUETOOTH_Verify(hci[i]);
        if(v > 0)
        {
            *params = json_string("Bluetooth not operational.");
            status = WA_DIAG_ERRCODE_FAILURE;
            break;
        }
        else if(v < 0)
        {
            *params = json_string("HW not accessible.");
            status = WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR;
        }
        WA_DIAG_SendProgress(instanceHandle, ((hciNum - i)*100)/hciNum);
    }

end:
    if((status != 0) && WA_OSA_TaskCheckQuit())
    {
        json_decref(*params);
        *params = json_string("Test cancelled.");
        status = WA_DIAG_ERRCODE_CANCELLED;
    }
    WA_RETURN("bluetooth_status %d\n", status);
    return status;
}

/**
 * @brief Create list of internal bluetooth interfaces
 *
 * @returns number of interfaces found
 */
int WA_DIAG_BLUETOOTH_GetInternalInterfaces(hci_t hci)
{
    FILE *p;
    char path[MAX_DIR];
    int hciNum = 0;
    char *c;

    WA_ENTER("WA_DIAG_BLUETOOTH_GetInternalInterfaces()\n");

    p = popen("/bin/ls -d " DIR_PATTERN " 2>/dev/null", "r");
    if (p == NULL)
        goto end;

    while ((fgets(path, MAX_DIR-1, p) != NULL) && (hciNum < MAX_INTERFACES))
    {
        c = strrchr(path, '/');
        if((c != NULL) && (strncmp(c+1, IF_PREFIX, strlen(IF_PREFIX)) == 0))
            hci[hciNum++] = atoi(c+1+strlen(IF_PREFIX));
    }

    pclose(p);

end:
    WA_RETURN("WA_DIAG_BLUETOOTH_GetInternalInterfaces():%d\n", hciNum);
    return hciNum;
}

/**
 * @brief Verify bluetooth HW at given interface
 *
 * @retval 0 is operational
 * @retval -1 not operational or cancelled
 * @retval 1 cannot access bluettoth HW
 */
int WA_DIAG_BLUETOOTH_Verify(int hci)
{
    int status = 0;
    char buf[STR_MAX];

    WA_ENTER("WA_DIAG_BLUETOOTH_Verify(hci=%d)\n", hci);

    snprintf(buf, STR_MAX, "/usr/bin/hciconfig hci%d 2>/dev/null | grep -q \" *DOWN\" 2>/dev/null", hci);
    if(system(buf) == 0)
    {
        WA_DBG("hci%d is down\n", hci);
        status = -1;
    }
    else
    {
        snprintf(buf, STR_MAX, "/usr/bin/hciconfig hci%d version &>/dev/null", hci);
        if(system(buf) != 0)
        {
            WA_DBG("hci%d failure\n", hci);
            status = 1;
        }
    }

    WA_RETURN("WA_DIAG_BLUETOOTH_Verify(hci=%d):%d\n", hci, status);
    return status;
}

/*****************************************************************************
 * LOCAL FUNCTIONS
 *****************************************************************************/
/**
 * @brief Check if BT is supported.
 *
 * @retval true  supported
 * @retval false not supported or cancelled
 */
static bool bluetooth_supported(void)
{
    int status = false;
    char *addr;

    addr = WA_UTILS_FILEOPS_OptionFind(DEV_CONFIG_FILE_PATH, BLUETOOTH_OPTION_STR);

    if(WA_OSA_TaskCheckQuit())
    {
        free(addr);
        WA_DBG("bluetooth_supported: test cancelled\n");
        return false;
    }

    if(addr == NULL)
        return false;

    status = !strcmp(addr, "true");
    free(addr);
    return status;
}

/* End of doxygen group */
/*! @} */
