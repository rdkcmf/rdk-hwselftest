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
 * @file wa_diag_hdd.c
 *
 * @brief Diagnostic functions for HDD - implementation
 */

/** @addtogroup WA_DIAG_HDD
 *  @{
 */

/*****************************************************************************
 * STANDARD INCLUDE FILES
 *****************************************************************************/

#define _GNU_SOURCE
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/

#include "wa_diag.h"
#include "wa_debug.h"
#include "wa_fileops.h"

/* module interface */
#include "wa_diag_hdd.h"

/*****************************************************************************
 * GLOBAL VARIABLE DEFINITIONS
 *****************************************************************************/

/*****************************************************************************
 * LOCAL DEFINITIONS
 *****************************************************************************/
#define HDD_NODE                "/bin/mount | grep 'rtdev' | head -n1 | sed -e \"s|.*rtdev=||g\" -e \"s|,.*||g\""
#define SMART_OPTION_STR        "SMART support is"
#define SMART_ENABLED_STR       "Enabled"

#define HDD_OPTION_STR          "HDD_ENABLED"
#define HDD_AVAILABLE_STR       "true"

#define HDD_HEALTH_OPTION_STR   "SMART overall-health self-assessment test result"
#define HDD_HEALTH_OK_STR       "PASSED"

#define HDD_ATTRIBUTE_LIST_STR  "Vendor Specific SMART Attributes with Thresholds"
#define HDD_IN_THE_PAST_STR     "In_the_past"

#define DEV_CONFIG_FILE_PATH    "/etc/device.properties"
#define FILE_MODE               "r"

#define SMART_RAW_VALUE_LIMIT   500
#define LINE_LEN                256
#define DATA_LEN                32        /* /dev/sda1 */

/*****************************************************************************
 * LOCAL TYPES
 *****************************************************************************/

/*****************************************************************************
 * LOCAL FUNCTION PROTOTYPES
 *****************************************************************************/

static int disk_supported(void);
static int get_device_name(char *deviceName);
static int smart_enabled(const char* deviceName);
static int disk_health_status(const char* deviceName);
static int attribute_list_status(const char* deviceName);
static int get_smart_id_raw_value(char* attr_str, int *raw_val);
static int print_smart_data(smartAttributes_t *smart_data, int count);
static int setReturnData(int status, json_t** param);

/*****************************************************************************
 * LOCAL VARIABLE DECLARATIONS
 *****************************************************************************/
static char *smart_attr_ids[] = {"5", "187", "196", "197", "198"}; // Only selective smart attributes are checked for warnings

/*****************************************************************************
 * FUNCTION DEFINITIONS
 *****************************************************************************/

/*****************************************************************************
 * LOCAL FUNCTIONS
 *****************************************************************************/

static int disk_supported(void)
{
    return WA_UTILS_FILEOPS_OptionSupported(DEV_CONFIG_FILE_PATH, FILE_MODE, HDD_OPTION_STR, HDD_AVAILABLE_STR);
}

static int get_device_name(char *deviceName)
{
    FILE *fp;

    fp = popen(HDD_NODE, FILE_MODE);
    if (fp == NULL)
    {
        WA_ERROR("HDD Test, get_device_name(): Failed to get the device name '%s'\n", HDD_NODE);
        return -1;
    }

    fscanf(fp, "%s", deviceName);
    pclose(fp);

    WA_INFO("HDD Test, get_device_name(): %s\n", deviceName);

    return 0;
}

static int smart_enabled(const char* deviceName)
{
    char cmd[LINE_LEN];

    snprintf(cmd, sizeof(cmd), "smartctl --info %s", deviceName);

    return WA_UTILS_FILEOPS_CheckCommandResult(cmd, SMART_OPTION_STR, SMART_ENABLED_STR);
}

static int disk_health_status(const char* deviceName)
{
    char cmd[LINE_LEN];

    snprintf(cmd, sizeof(cmd), "smartctl --health %s", deviceName);

    return WA_UTILS_FILEOPS_CheckCommandResult(cmd, HDD_HEALTH_OPTION_STR, HDD_HEALTH_OK_STR);
}

static int attribute_list_status(const char* deviceName)
{
    FILE *fp;
    bool begin_parsing = false;
    char cmd[LINE_LEN];
    char str_out[LINE_LEN];
    char attr_id[DATA_LEN];
    char *id = NULL;
    char *tmp_string = NULL;
    int raw_value = 0;
    int count = 0;
    int str_len = 0;
    int ret = -1;
    int num_elements = sizeof(smart_attr_ids) / sizeof(char*); // Number of elements in the array calculated as per the actual elements
    smartAttributes_t smart_data[num_elements];

    WA_ENTER("HDD Test: attribute_list_status()\n");

    snprintf(cmd, sizeof(cmd), "smartctl --attributes %s", deviceName);

    fp = popen(cmd, FILE_MODE);
    if(fp == NULL)
    {
        WA_ERROR("HDD Test: attribute_list_status(): Failed to execute the command '%s'\n", cmd);
        return WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR;
    }

    // Going through the list of SMART attributes
    while (fgets(str_out, LINE_LEN, fp) != NULL && !WA_OSA_TaskCheckQuit())
    {
        begin_parsing = (!begin_parsing && strcasestr(str_out, HDD_ATTRIBUTE_LIST_STR) != NULL) ? true : begin_parsing;
        if (!begin_parsing)
            continue;

        str_len = strlen(str_out);
        tmp_string = (char*)malloc(str_len + 1);
        if (!tmp_string)
        {
            pclose(fp);
            WA_DBG("HDD Test: attribute_list_status(): Failed to allocate memory for temp_string\n");
            return WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR;
        }

        strncpy(tmp_string, str_out, str_len);
        memset(attr_id, 0, sizeof(attr_id));

        id = strtok(tmp_string, " ");
        if (id)
            snprintf(attr_id, sizeof(attr_id), "%s", id);
        else
            WA_DBG("HDD Test: attribute_list_status(): NULL value returned\n");

        free(tmp_string);
        tmp_string = NULL;

        if (strcmp(attr_id, "Vendor") == 0 || strcmp(attr_id, "ID#") == 0 || strcmp(attr_id, "") == 0)
            continue;

        WA_DBG("HDD Test: attribute_list_status(): attr_str_len: %i, attr_id: %s\n", str_len, attr_id);

        for (int index = 0; index < num_elements; index++)
        {
            raw_value = 0;
            ret = 0;

            if (strcmp(attr_id, smart_attr_ids[index]) == 0)
            {
                WA_DBG("HDD Test: attribute_list_status(): smart_attr_ids[index]: %s\n", smart_attr_ids[index]);
                ret = get_smart_id_raw_value(str_out, &raw_value); // Non-zero raw value is returned when the value is >10 or when failed in the past
            }

            if (ret == 0)
                continue;

            smart_data[count].attr_id = smart_attr_ids[index];
            smart_data[count].raw_value = raw_value;
            count++;
        }
    }

    pclose(fp);

    if (!begin_parsing)
    {
        WA_DBG("HDD Test: attribute_list_status(): Failed to find line with %s\n", HDD_ATTRIBUTE_LIST_STR);
        return WA_DIAG_ERRCODE_HDD_STATUS_MISSING;
    }

    if (count == 0)
    {
        WA_DBG("HDD Test: attribute_list_status(): SMART Attributes are good\n");
        return WA_DIAG_ERRCODE_SUCCESS;
    }

    print_smart_data(smart_data, count);

    WA_RETURN("HDD Test: Number of SMART Attributes failed: %i\n", count);

    return WA_DIAG_ERRCODE_HDD_MARGINAL_ATTRIBUTES_FOUND;
}

static int get_smart_id_raw_value(char* attr_str, int *raw_val)
{
    char *value = NULL;
    char *tmp_val;
    int attr_val;
    bool past_failure = false;

    WA_DBG("HDD Test: get_smart_id_raw_value(): attribute string: '%s'\n", attr_str);

    if (attr_str != NULL)
    {
        past_failure = (strstr(attr_str, HDD_IN_THE_PAST_STR) == NULL) ? false : true;

        tmp_val = strrchr(attr_str, ' ');
        value = tmp_val + 1;

        if (value)
        {
            attr_val = atoi(value);

            if (attr_val > SMART_RAW_VALUE_LIMIT || past_failure)
            {
                WA_DBG("HDD Test: get_smart_id_raw_value(): attribute raw value: '%i'\n", attr_val);
                *raw_val = attr_val;
                return 1;
            }
        }
        else
        {
            WA_ERROR("HDD Test: get_smart_id_raw_value(): Could not retrieve RAW value from SMART attribute data: '%s'\n", attr_str);
        }
    }
    else
    {
        WA_ERROR("HDD Test: get_smart_id_raw_value(): Empty SMART attribute data\n");
    }

    return 0;
}

static int print_smart_data(smartAttributes_t *smart_data, int count)
{
    char *msg;
    json_t *json = json_object();

    for (int index = 0; index < count; index++)
    {
        json_object_set_new(json, smart_data[index].attr_id, json_integer(smart_data[index].raw_value));
    }

    msg = json_dumps(json, JSON_ENCODE_ANY);

    if (msg)
    {
        // We are printing the telemetry of marginal attributes in json format and it means HDD test is not passed.
        WA_ERROR("HWHeathTestSMART: %s\n", msg);
        free(msg);
    }
    else
    {
        WA_ERROR("HDD Test: json_dumps(): Error while printing jsmart_data\n");
    }

    return 0;
}

static int setReturnData(int status, json_t **param)
{
    if(param == NULL)
        return status;

    switch (status)
    {
        case WA_DIAG_ERRCODE_SUCCESS:
           *param = json_string("S.M.A.R.T. health status good.");;
           break;

        case WA_DIAG_ERRCODE_FAILURE:
           *param = json_string("S.M.A.R.T. health status error.");
           break;

        case WA_DIAG_ERRCODE_HDD_STATUS_MISSING:
            *param = json_string("HDD Test Not Run");
            break;

        case WA_DIAG_ERRCODE_HDD_MARGINAL_ATTRIBUTES_FOUND:
            *param = json_string("Marginal HDD Values");
            break;

        case WA_DIAG_ERRCODE_HDD_DEVICE_NODE_NOT_FOUND:
            *param = json_string("HDD Device Node Not Found");
            break;

        case WA_DIAG_ERRCODE_NOT_APPLICABLE:
            *param = json_string("Not applicable.");
            break;

        case WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR:
            *param = json_string("Internal test error.");
            break;

        case WA_DIAG_ERRCODE_CANCELLED:
            *param = json_string("Test cancelled.");
            break;

        default:
            break;
    }

    return status;
}

/*****************************************************************************
 * EXPORTED FUNCTIONS
 *****************************************************************************/

int WA_DIAG_HDD_status(void *instanceHandle, void *initHandle, json_t **params)
{
    int ret = -1;
    char deviceName[DATA_LEN];

    json_decref(*params); //not used
    *params = NULL;

    ret = disk_supported();
    if (ret < 0)
    {
        if(WA_OSA_TaskCheckQuit())
        {
            WA_DBG("disk_supported: test cancelled\n");
            return setReturnData(WA_DIAG_ERRCODE_CANCELLED, params);
        }

        WA_DBG("Device configuration unknown\n");
        return setReturnData(WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR, params);
    }

    if (ret == 0)
    {
        WA_DBG("Device does not have hdd\n");
        return setReturnData(WA_DIAG_ERRCODE_NOT_APPLICABLE, params);
    }

    WA_DBG("HDD supported.\n");

    memset(deviceName, 0, sizeof(deviceName));
    if (get_device_name(deviceName))
    {
        WA_DBG("HDD_status: Device node cannot be retrieved\n");
        return setReturnData(WA_DIAG_ERRCODE_HDD_DEVICE_NODE_NOT_FOUND, params);
    }

    ret = smart_enabled(deviceName);
    if(ret < 0 )
    {
        if(WA_OSA_TaskCheckQuit())
        {
            WA_DBG("smart_enabled: Test cancelled\n");
            return setReturnData(WA_DIAG_ERRCODE_CANCELLED, params);
        }

        WA_DBG("S.M.A.R.T. health status missing\n");
        //even if SMART status unknown want to get latest available result
    }
    else if(ret == 0)
    {
        WA_DBG("S.M.A.R.T. currently disabled\n");
        //even if SMART disabled want to get latest available result
    }
    else
    {
        WA_DBG("S.M.A.R.T. currently enabled\n");
    }

    ret = disk_health_status(deviceName);
    if(ret < 0)
    {
        if(WA_OSA_TaskCheckQuit())
        {
            WA_DBG("disk_health_status: test cancelled\n");
            return setReturnData(WA_DIAG_ERRCODE_CANCELLED, params);
        }

        WA_DBG("S.M.A.R.T. health status missing\n");
        return setReturnData(WA_DIAG_ERRCODE_HDD_STATUS_MISSING, params);
    }
    else if (ret == 0)
    {
        WA_DBG("Last S.M.A.R.T. health status bad\n");
        return setReturnData(WA_DIAG_ERRCODE_FAILURE, params);
    }

    WA_DBG("Last S.M.A.R.T. health status good\n");

    ret = attribute_list_status(deviceName);

    return setReturnData(ret, params);
}

/* End of doxygen group */
/*! @} */

/* EOF */
