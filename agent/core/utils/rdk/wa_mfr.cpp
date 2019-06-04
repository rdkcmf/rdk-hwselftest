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
 * @file wa_mfr.cpp
 *
 * @brief This file contains interface functions for manufacturer library.
 */

/** @addtogroup WA_UTILS_MFR
 *  @{
 */

/*****************************************************************************
 * STANDARD INCLUDE FILES
 *****************************************************************************/
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>


/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/
#include "wa_debug.h"
#include "wa_osa.h"
#include "wa_fileops.h"

/*****************************************************************************
 * RDK-SPECIFIC INCLUDE FILES
 *****************************************************************************/
#include "libIBus.h"
#include "libIARMCore.h"
#include "mfrMgr.h"
#include "wa_mfr.h"

#if defined(LEGACY_MFR_VENDOR)
/* To access legacy MFR API. */
#include "mfr_types.h"

#define MFR_API_CONCAT(x,y) x##y
#define MFR_API(vendor, api) MFR_API_CONCAT(vendor, _##api)

extern "C" {
VL_MFR_API_RESULT MFR_API(LEGACY_MFR_VENDOR, mfr_get_version)(VL_PLATFORM_VERSION_TYPE eVersionType, char **ppString);
}
#endif /* defined(LEGACY_MFR_VENDOR) */

/*****************************************************************************
 * GLOBAL VARIABLE DEFINITIONS
 *****************************************************************************/

/*****************************************************************************
 * LOCAL DEFINITIONS
 *****************************************************************************/
#define VENDOR_INFO_FILE "/tmp/mpeos_vendor_info.txt"

/*****************************************************************************
 * LOCAL FUNCTION PROTOTYPES
 *****************************************************************************/

/*****************************************************************************
 * LOCAL VARIABLE DECLARATIONS
 *****************************************************************************/

static mfrSerializedType_t paramMap[3] = {
    mfrSERIALIZED_TYPE_MANUFACTURER,
    mfrSERIALIZED_TYPE_MODELNAME,
    mfrSERIALIZED_TYPE_SERIALNUMBER };

#if defined(LEGACY_MFR_VENDOR)
static VL_PLATFORM_VERSION_TYPE paramMapVl[3] = {
    VL_PLATFORM_VERSION_TYPE_VENDOR_NAME,
    VL_PLATFORM_VERSION_TYPE_MODEL_NUMBER,
    VL_PLATFORM_VERSION_TYPE_MODEL_SERIAL_NO };
#endif

static const char *paramMapVendorInfoFile[3] =
{
    "Vendor Name%*[ ]:%*[ ]",
    "Model Name%*[ ]:%*[ ]",
    "Serial Number%*[ ]:%*[ ]"
};

/*****************************************************************************
 * FUNCTION DEFINITIONS
 *****************************************************************************/

/*****************************************************************************
 * LOCAL FUNCTIONS
 *****************************************************************************/

/*****************************************************************************
 * EXPORTED FUNCTIONS
 *****************************************************************************/

int WA_UTILS_MFR_ReadSerializedData(WA_UTILS_MFR_StbParams_t data, size_t* size, char** value)
{
    IARM_Result_t ret = IARM_RESULT_IPCCORE_FAIL;
    IARM_Bus_MFRLib_GetSerializedData_Param_t *param = NULL;

    if(size == NULL || value == NULL)
        return -1;

    *size = 0;
    *value = NULL;

    /* Try IARM MFR API first */
    IARM_Malloc(IARM_MEMTYPE_PROCESSLOCAL, sizeof(IARM_Bus_MFRLib_GetSerializedData_Param_t), (void**)&param);
    if (param)
    {
        param->type = paramMap[data];
        param->bufLen = MAX_SERIALIZED_BUF;

        ret = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME,
            IARM_BUS_MFRLIB_API_GetSerializedData,
            (void *)param, sizeof(IARM_Bus_MFRLib_GetSerializedData_Param_t));

        if (ret == IARM_RESULT_SUCCESS)
        {
            if(param->bufLen < 1)
            {
                WA_WARN("WA_UTILS_MFR_ReadSerializedData(): IARM_Bus_Call() returned empty data\n");
            }
            else
            {
                *size = param->bufLen + 1;
                *value = (char *)malloc(*size);
                if(*value == NULL)
                {
                    *size = 0;
                    ret = IARM_RESULT_IPCCORE_FAIL;
                }
                else
                {
                    (void)memcpy((void *)*value, (void *)param->buffer, *size - 1);
                    *(*value + param->bufLen) = '\0';
                    WA_INFO("Retrieved MFR value for param %i: %s\n", data, *value);
                }
            }
        }
        else
            WA_WARN("WA_UTILS_MFR_ReadSerializedData(): IARM_Bus_Call() returned %i\n", ret);

        IARM_Free(IARM_MEMTYPE_PROCESSLOCAL, param);
    }
    else
        WA_ERROR("WA_UTILS_MFR_ReadSerializedData(): IARM_Malloc() failed\n");

#if defined(LEGACY_MFR_VENDOR)
    /* Use legacy MFR library API */
    if (ret != IARM_RESULT_SUCCESS)
    {
        WA_INFO("WA_UTILS_MFR_ReadSerializedData(): Using legacy MFRLib interface\n");
        VL_MFR_API_RESULT vlret = VL_MFR_API_RESULT_FAILED;

        vlret = MFR_API(LEGACY_MFR_VENDOR, mfr_get_version) (paramMapVl[data], value);
        if (vlret == VL_MFR_API_RESULT_SUCCESS)
        {
            *size = strlen(*value);
            WA_INFO("WA_UTILS_MFR_ReadSerializedData(): Retrieved MFR value for param %i: %s\n", data, *value);
            ret = IARM_RESULT_SUCCESS;
        }
        else
            WA_ERROR("WA_UTILS_MFR_ReadSerializedData(): mfr_get_version() returned %i\n", vlret);
    }
#endif /* defined(LEGACY_MFR_VENDOR) */

    /* Sample vendor info file as a last resort */
    if (ret != IARM_RESULT_SUCCESS)
    {
        WA_INFO("WA_UTILS_MFR_ReadSerializedData(): Using %s file\n", VENDOR_INFO_FILE);
        *value = WA_UTILS_FILEOPS_OptionFind(VENDOR_INFO_FILE, paramMapVendorInfoFile[data]);
        if (*value)
        {
            *size = strlen(*value);
            WA_INFO("WA_UTILS_MFR_ReadSerializedData(): Retrieved MFR value for param %i: %s\n", data, *value);
            ret = IARM_RESULT_SUCCESS;
        }
        else
            WA_ERROR("WA_UTILS_MFR_ReadSerializedData(): param %i not found in info file\n", data);
    }

    return (ret == IARM_RESULT_SUCCESS ? 0 : -1);
}

/* End of doxygen group */
/*! @} */

/* EOF */
