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
 * @file wa_config.c
 *
 * @brief Implementation of agent configuration functionality.
 */

/** @addtogroup WA_CONFIG
 *  @{
 */

/*****************************************************************************
 * STANDARD INCLUDE FILES
 *****************************************************************************/
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/
#include "wa_diag.h"
#include "wa_log.h"
#include "wa_debug.h"
#include "wa_osa.h"

#include "wa_comm.h"
#include "wa_comm_ws.h"

#include "wa_diag.h"
#include "wa_diag_sysinfo.h"
#include "wa_diag_capabilities.h"
#include "wa_diag_prev_results.h"

#ifdef HAVE_DIAG_HDD
#include "wa_diag_hdd.h"
#endif
#ifdef HAVE_DIAG_SDCARD
#include "wa_diag_sdcard.h"
#endif
#ifdef HAVE_DIAG_TUNER
#include "wa_diag_tuner.h"
#endif
#ifdef HAVE_DIAG_AVDECODER_QAM
#include "wa_diag_avdecoder.h"
#endif
#ifdef HAVE_DIAG_HDMIOUT
#include "wa_diag_hdmiout.h"
#endif
#ifdef HAVE_DIAG_IR
#include "wa_diag_ir.h"
#endif
#ifdef HAVE_DIAG_FLASH
#include "wa_diag_flash.h"
#endif
#ifdef HAVE_DIAG_FLASH_XI6
#include "wa_diag_flash.h"
#endif
#ifdef HAVE_DIAG_DRAM
#include "wa_diag_dram.h"
#endif
#ifdef HAVE_DIAG_MCARD
#include "wa_diag_mcard.h"
#endif
#ifdef HAVE_DIAG_MOCA
#include "wa_diag_moca.h"
#endif
#ifdef HAVE_DIAG_RF4CE
#include "wa_diag_rf4ce.h"
#endif
#ifdef HAVE_DIAG_MODEM
#include "wa_diag_modem.h"
#endif
#ifdef HAVE_DIAG_BLUETOOTH
#include "wa_diag_bluetooth.h"
#endif
#ifdef HAVE_DIAG_WIFI
#include "wa_diag_wifi.h"
#endif

/*****************************************************************************
 * GLOBAL VARIABLE DEFINITIONS
 *****************************************************************************/

/*****************************************************************************
 * LOCAL DEFINITIONS
 *****************************************************************************/
#define CONFIG_FILE_NAME "hwselftest.conf"

#ifndef SYSCONFDIR_DEBUG
#define SYSCONFDIR_DEBUG "/opt/hwselftest"
#endif

#ifndef SYSCONFDIR
#define SYSCONFDIR "/etc/hwselftest"
#endif

/*****************************************************************************
 * LOCAL FUNCTION PROTOTYPES
 *****************************************************************************/
static json_t *loadConfigFile(const char * filename);
static json_t *loadConfig(const char * commandLineFilename);

/*****************************************************************************
 * LOCAL VARIABLE DECLARATIONS
 *****************************************************************************/
static json_t *configs = NULL;

static WA_COMM_adaptersConfig_t adapters[] =
{
    {"comm_ws", WA_COMM_WS_Init, WA_COMM_WS_Exit, WA_COMM_WS_Callback, NULL, NULL},
    /* END OF LIST */
    {NULL, NULL, NULL, NULL, NULL, NULL}
};

static WA_DIAG_proceduresConfig_t diags[] =
{
    {"sysinfo_info", NULL, NULL, WA_DIAG_SYSINFO_Info, NULL, NULL, NULL},
    {"capabilities_info", NULL, NULL, WA_DIAG_CAPABILITIES_Info, NULL, NULL, NULL },

#ifdef HAVE_DIAG_HDD
    {"hdd_status", NULL, NULL, WA_DIAG_HDD_status, NULL, NULL, NULL },
#endif
#ifdef HAVE_DIAG_SDCARD
    {"sdcard_status", NULL, NULL, WA_DIAG_SDCARD_status, NULL, NULL, NULL },
#endif
#ifdef HAVE_DIAG_FLASH
    {"flash_status", NULL, NULL, WA_DIAG_FLASH_status, NULL, NULL, NULL },
#endif
#ifdef HAVE_DIAG_FLASH_XI6
    {"flash_status", NULL, NULL, WA_DIAG_FLASH_status, NULL, NULL, NULL },
#endif
#ifdef HAVE_DIAG_DRAM
    {"dram_status", NULL, NULL, WA_DIAG_DRAM_status, NULL, NULL, NULL },
#endif
#ifdef HAVE_DIAG_HDMIOUT
    {"hdmiout_status", NULL, NULL, WA_DIAG_HDMIOUT_status, NULL, NULL, NULL },
#endif
#ifdef HAVE_DIAG_MCARD
    {"mcard_status", NULL, NULL, WA_DIAG_MCARD_status, NULL, NULL, NULL },
#endif
#ifdef HAVE_DIAG_IR
    {"ir_status", NULL, NULL, WA_DIAG_IR_status, NULL, NULL, NULL},
#endif
#ifdef HAVE_DIAG_RF4CE
    {"rf4ce_status", NULL, NULL, WA_DIAG_RF4CE_status, NULL, NULL, NULL },
#endif
#ifdef HAVE_DIAG_MOCA
    {"moca_status", NULL, NULL, WA_DIAG_MOCA_status, NULL, NULL, NULL },
#endif
#ifdef HAVE_DIAG_AVDECODER_QAM
    {"avdecoder_qam_status", WA_DIAG_AVDECODER_init, NULL, WA_DIAG_AVDECODER_status, NULL, NULL, NULL },
#endif
#ifdef HAVE_DIAG_TUNER
    {"tuner_status", NULL, NULL, WA_DIAG_TUNER_status, NULL, NULL, NULL },
#endif
#ifdef HAVE_DIAG_MODEM
    {"modem_status", NULL, NULL, WA_DIAG_MODEM_status, NULL, NULL, NULL },
#endif
#ifdef HAVE_DIAG_BLUETOOTH
    {"bluetooth_status", NULL, NULL, WA_DIAG_BLUETOOTH_status, NULL, NULL, NULL },
#endif
#ifdef HAVE_DIAG_WIFI
    {"wifi_status", NULL, NULL, WA_DIAG_WIFI_status, NULL, NULL, NULL },
#endif

    {"previous_results", NULL, NULL, WA_DIAG_PREV_RESULTS_Info, NULL, NULL, NULL },
    /* END OF LIST */
    {NULL, NULL, NULL, NULL, NULL, NULL}
};

char * configCheckDirectories[] =
{
    ".",
    SYSCONFDIR_DEBUG,
    SYSCONFDIR
};

/*****************************************************************************
 * FUNCTION DEFINITIONS
 *****************************************************************************/

int WA_CONFIG_Init(const char* filename)
{
    int status = 1;

    WA_ENTER("WA_CONFIG_Init()\n");

    if (!configs)
    {
        configs = loadConfig(filename);
        if (configs)
        {
            /* Fill in config fields in adapter definition table with json taken from configuration */
            json_t * adaptersConfig = json_object_get(configs, "adapters");
            if (adaptersConfig && json_is_object(adaptersConfig))
            {
                for (WA_COMM_adaptersConfig_t * adapter = &adapters[0]; adapter->name; adapter++)
                {
                    json_t * adapterConfig = json_object_get(adaptersConfig, adapter->name);
                    if (adapterConfig && json_is_object(adapterConfig))
                    {
                        adapter->config = adapterConfig;
                    }
                }
            }

            /* Fill in config fields in diag definition table with json taken from configuration */
            json_t * diagsConfig = json_object_get(configs, "diags");
            if (diagsConfig && json_is_object(diagsConfig))
            {
                for (WA_DIAG_proceduresConfig_t * diag = &diags[0]; diag->name; diag++)
                {
                    json_t * diagConfig = json_object_get(diagsConfig, diag->name);
                    if (diagConfig && json_is_object(diagConfig))
                    {
                        diag->config = diagConfig;
                    }
                }
            }

            status = 0;
        }
        else
            WA_ERROR("WA_CONFIG_Init(): Error during config loading.\n");
    }
    else
        WA_ERROR("WA_CONFIG_Init(): Already initialised.\n");

    WA_RETURN("WA_CONFIG_Init(): %d\n", status);

    return status;
}

int WA_CONFIG_Exit()
{
    int status = 0;

    WA_ENTER("WA_CONFIG_Exit()\n");

    if (configs)
    {
        json_decref(configs);
        configs = NULL;
    }

    WA_RETURN("WA_CONFIG_Exit(): %d\n", status);

    return status;
}

const WA_DIAG_proceduresConfig_t *WA_CONFIG_GetDiags()
{
    return diags;
}

const WA_COMM_adaptersConfig_t *WA_CONFIG_GetAdapters()
{
    return adapters;
}

/*****************************************************************************
 * LOCAL FUNCTION DEFINITIONS
 *****************************************************************************/

static json_t *loadConfigFile(const char * filename)
{
    json_error_t error;
    json_t *result;

    printf("HWST_DBG |Loading config file from: %s\n", filename);

    result = json_load_file(filename, 0, &error);
    if (!result)
    {
        WA_ERROR("Unable to parse configuration file \"%s\", position %i: %s\n", filename, error.position, error.text);
        fprintf(stderr, "HWST_DBG |Failed to open config file \"%s\"\n", filename);
        result = json_object();
    }

    return result;
}

static char *findConfigFile()
{
    const size_t baseNameSize = strlen(CONFIG_FILE_NAME);
    char *buf = NULL;
    size_t bufSize = 0;

    for (int i = 0; i < sizeof(configCheckDirectories) / sizeof(configCheckDirectories[0]); ++i)
    {
        const char * dir = configCheckDirectories[i];
        const size_t dirNameSize = strlen(dir);
        const size_t newSize = dirNameSize + baseNameSize + 2; // slash and terminating NUL

        if (newSize > bufSize)
        {
            char * newBuf = realloc(buf, newSize);
            if (newBuf)
            {
                buf = newBuf;
                bufSize = newSize;
            }
        }

        if (bufSize >= newSize)
        {
            snprintf(buf, bufSize, "%s/%s", dir,CONFIG_FILE_NAME);
        }

        printf("HWST_DBG |Looking for %s... ", buf);
        if (access(buf, F_OK) == 0)
        {
            printf("found.\n");
            return buf;
        }

        printf("not found.\n");
    }

    return NULL;
}

static json_t *loadConfig(const char *filename)
{
    json_t *result = NULL;
    char *toFree = NULL;

    if (!filename)
    {
        toFree = findConfigFile();
        filename = toFree;
    }

    if (filename)
        result = loadConfigFile(filename);
    else
        result = json_object();

    free(toFree);

    return result;
}

/* End of doxygen group */
/*! @} */

/* EOF */
