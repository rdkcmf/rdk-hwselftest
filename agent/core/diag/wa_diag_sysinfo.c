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
 * @file wa_diag_sysinfo.c
 *
 * @brief System Information - implementation
 */

/** @addtogroup WA_DIAG_SYSINFO
 *  @{
 */

/*****************************************************************************
 * STANDARD INCLUDE FILES
 *****************************************************************************/
#include <string.h>

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/

#include "wa_debug.h"
#include "wa_fileops.h"
#include "wa_version.h"
#include "wa_snmp_client.h"
#include "wa_log.h"
#include "sysMgr.h"
#include "xdiscovery.h"
#include "libIBus.h"
#include "libIARMCore.h"
#include "hostIf_tr69ReqHandler.h"

/* rdk specific */
#include "wa_iarm.h"
#include "wa_mfr.h"

/* module interface */
#include "wa_diag_sysinfo.h"
#ifdef HAVE_DIAG_MOCA
#include "wa_diag_moca.h"
#endif
#ifdef HAVE_DIAG_WIFI
#include "wa_diag_wifi.h"
#endif
#include "wa_diag_modem.h"
#include "wa_diag_errcodes.h"
#ifdef HAVE_DIAG_TUNER
#include "wa_diag_tuner.h"
#endif

#include "pwrMgr.h"
#include "rfcapi.h"
/*****************************************************************************
 * GLOBAL VARIABLE DEFINITIONS
 *****************************************************************************/

/*****************************************************************************
 * LOCAL DEFINITIONS
 *****************************************************************************/

#define MAX_LINE 100
#define DEV_FILE_PATH      "/tmp/.deviceDetails.cache"
#define BUFFER_LENGTH      512
#define MESSAGE_LENGTH     8192 * 4 /* On reference from xdiscovery.log which shows data length can be more than 5000 */ /* Increased the value 4 times because of DELIA-38611 */
#define DEVNAME_LENGTH     15
#define MAC_LENGTH         32
#define ESTB_MAC           "estb_mac="
#ifndef MEDIA_CLIENT
#define STB_IP             "estb_ip="
#define SNMP_SERVER        "localhost"
#define OID_TIME_ZONE      "OC-STB-HOST-MIB::ocStbHostCardTimeZoneOffset"
#define OID_XCONF_VER      "FWDNLDMIB::swUpdateDownloadVersion"
#define OID_RECEIVER_ID    "XcaliburClientMIB::xreReceiverId"
#define SI_PATH            "/si_cache_parser_121 /opt/persistent/si"
#define TMP_SI_PATH        "/si_cache_parser_121 /tmp/mnt/diska3/persistent/si"
#define DOCSIS_CONNECTING  "DOCSIS is Connecting"
#else
#ifdef HAVE_DIAG_WIFI
#define STB_IP             "boxIP="
#else
#define STB_IP             "moca_ip="
#endif
#define TR69_XCONF_VERSION "Device.DeviceInfo.X_RDKCENTRAL-COM_FirmwareToDownload"
#define TR69_RECEIVER_ID   "Device.X_COMCAST-COM_Xcalibur.Client.XRE.xreReceiverId"
#define THERMAL_PROTECTION_GROUP        "Thermal_Config"
#define TR69_CPU_TEMP                   "Device.DeviceInfo.X_RDKCENTRAL-COM.CPUTemp"
#define TEMPERATURE_HIGH_THRESHOLD      100 /* This value is from generic/iarmmgrs/generic/power/therm_mgr.c */
#define TEMPERATURE_CRITICAL_THRESHOLD  110 /* This value is from generic/iarmmgrs/generic/power/therm_mgr.c */
#endif

/*****************************************************************************
 * LOCAL TYPES
 *****************************************************************************/
typedef struct {
    size_t size;
    char *value;
}SysinfoParam_t;

#ifdef HAVE_DIAG_MOCA
typedef enum
{
    WA_MOCA_INFO_RF_CHANNEL,
    WA_MOCA_INFO_NETWORK_CONTROLLER,
    WA_MOCA_INFO_TRANSMIT_RATE,
    WA_MOCA_INFO_NODE_SNR,
    WA_MOCA_INFO_MAX
} WA_MocaInfo_t;
#endif

#ifdef HAVE_DIAG_WIFI
typedef enum
{
    WA_WIFI_SSID,
    WA_WIFI_SSID_MAC_ADDR,
    WA_WIFI_OPER_FREQ,
    WA_WIFI_SIGNAL_STRENGTH,
    WA_WIFI_INFO_MAX
} WA_WifiStat_t;
#endif

/*****************************************************************************
 * LOCAL FUNCTION PROTOTYPES
 *****************************************************************************/
static json_t *sysinfo_Get();
static int xconfVerGet(char *xconf_ver, size_t size);
static int getReceiverId(char *rev_id, size_t size);
static bool getUpnpResults();
#ifdef HAVE_DIAG_MOCA
static int getMocaNetworkInfo(SysinfoParam_t *moca);
#endif /* HAVE_DIAG_MOCA */
#ifdef HAVE_DIAG_WIFI
static int getWifiNetworkInfo(SysinfoParam_t *wifi);
#endif /* HAVE_DIAG_WIFI */
#ifndef MEDIA_CLIENT
static int getDateAndTime(char *date_time, size_t size);
#else
static int read_RFCProperty(const char* key);
static int temperatureGet(char *cpuTemp, size_t size);
int getSysInfoParam_IARM(char *param, char **value);
#endif /* MEDIA_CLIENT */

/*****************************************************************************
 * LOCAL VARIABLE DECLARATIONS
 *****************************************************************************/
char *mocaNodeInfo;
int  dstOffset = 0;
int  sysParamInt = 0;
char *sysParam;
char sysIARM[TR69HOSTIFMGR_MAX_PARAM_LEN];

/*****************************************************************************
 * FUNCTION DEFINITIONS
 *****************************************************************************/
int WA_DIAG_SYSINFO_Info(void *instanceHandle, void * initHandle, json_t **params)
{
    WA_ENTER("WA_DIAG_SYSINFO_Info(instanceHandle=%p initHandle=%p params=%p)\n",
            instanceHandle, initHandle, params);

    json_decref(*params); //not used

    *params = sysinfo_Get();

    WA_RETURN("WA_DIAG_SYSINFO_Info(): %d\n", *params == NULL);

    return (*params == NULL);
}

/*****************************************************************************
 * LOCAL FUNCTIONS
 *****************************************************************************/

static json_t *sysinfo_Get()
{
    int i;
    json_t *json = NULL;
    SysinfoParam_t params[3];
#ifdef HAVE_DIAG_MOCA
    SysinfoParam_t moca_data[WA_MOCA_INFO_MAX];
#endif /* HAVE_DIAG_MOCA */
#ifdef HAVE_DIAG_WIFI
    SysinfoParam_t wifi_data[WA_WIFI_INFO_MAX];
#endif /* HAVE_DIAG_WIFI */
    char *rdkver;
    char *addr;
    char *mac_addr;
    char rev_id[256] = "Error Reading Value";
    char xconf_ver[256] = "Error Reading Value";
    char notAvailable[] = "N/A";
#ifndef MEDIA_CLIENT
    char date_time[256];
    char num_ch[256];
    char channels[256];
    FILE *fp;
#else
    char cpuTemp[256] = "Error Reading Value";
#endif /* MEDIA_CLIENT */

    WA_ENTER("SYSINFO_Get()\n");

    memset(&params, 0, sizeof(params));

    if (WA_UTILS_IARM_Connect())
        return json;

    rdkver = WA_UTILS_FILEOPS_OptionFind("/version.txt", "imagename:");

    for (i = 0; i < sizeof(params)/sizeof(params[0]); i++)
    {
        if (WA_UTILS_MFR_ReadSerializedData(i, &params[i].size, &params[i].value) != 0)
            WA_ERROR("sysinfo_Get(): WA_UTILS_MFR_ReadSerializedData(%i) failed\n", i);
    }

    mocaNodeInfo = (char*)malloc(BUFFER_LENGTH * sizeof(char));
    if (!getUpnpResults())
    {
        snprintf(mocaNodeInfo, BUFFER_LENGTH, "Not Available");
        WA_ERROR("Home Network returned: %s\n", mocaNodeInfo);
    }
    WA_DBG("Home Network returned: %s\n", mocaNodeInfo);

    addr = WA_UTILS_FILEOPS_OptionFind(DEV_FILE_PATH, STB_IP);
    mac_addr = WA_UTILS_FILEOPS_OptionFind(DEV_FILE_PATH, ESTB_MAC);
    getReceiverId(&rev_id[0], sizeof(rev_id));
    xconfVerGet(&xconf_ver[0], sizeof(xconf_ver));

    WA_DBG("IP ADDRESS returned: %s\n", addr);
    WA_DBG("getReceiverId returned: %s\n", rev_id);
    WA_DBG("xconfVerGet returned: %s\n", xconf_ver);

#ifdef HAVE_DIAG_MOCA
    /* MoCA */
    memset(&moca_data, 0, sizeof(moca_data));

    for (int i = 0; i < WA_MOCA_INFO_MAX; i++)
    {
        moca_data[i].value = (char*)malloc(sizeof(char) * 256);
        if (moca_data[i].value == NULL)
        {
            WA_ERROR("NULL pointer exception in moca_data %i\n", i);
            goto exit;
        }
        /* Copy default string as N/A */
        snprintf(moca_data[i].value, sizeof(notAvailable), "%s", notAvailable);

    }

    if (getMocaNetworkInfo(&moca_data[0]) != 0)
    {
        WA_ERROR("sysinfo_Get(): getMocaNetworkInfo failed to fetch MoCA items\n");
    }

#endif /* HAVE_DIAG_MOCA */

#ifdef HAVE_DIAG_WIFI
    /* WIFI */
    memset(&wifi_data, 0, sizeof(wifi_data));

    for (int i = 0; i < WA_WIFI_INFO_MAX; i++)
    {
        wifi_data[i].value = (char*)malloc(sizeof(char) * 256);
        if (wifi_data[i].value == NULL)
        {
            WA_ERROR("NULL pointer exception in wifi_data %i\n", i);
            goto exit;
        }
        /* Copy default string as N/A */
        snprintf(wifi_data[i].value, sizeof(notAvailable), "%s", notAvailable);
    }

    if (getWifiNetworkInfo(&wifi_data[0]) != 0)
    {
        WA_ERROR("sysinfo_Get(): getWifiNetworkInfo failed to fetch WiFi items\n");
    }
#endif /* HAVE_DIAG_WIFI */

#ifndef MEDIA_CLIENT
    getDateAndTime(&date_time[0], sizeof(date_time));
    snprintf(num_ch, sizeof(num_ch), "%s | grep \"SRCID\" | wc -l", SI_PATH);
    fp = popen(num_ch, "r");
    fscanf(fp, "%s", channels);
    pclose(fp);

    if (channels[0] == '\0' || channels[0] == '0') {
        WA_DBG("grep from %s failed\n", SI_PATH);
        snprintf(num_ch, sizeof(num_ch), "%s | grep \"SRCID\" | wc -l", TMP_SI_PATH);
        fp = popen(num_ch, "r");
        fscanf(fp, "%s", channels);
        pclose(fp);
        if (channels[0] == '\0') {
            WA_DBG("grep from %s failed\n", TMP_SI_PATH);
        }
    }

    /* Get Tuner param values */
    WA_DIAG_TUNER_DocsisParams_t docsisParams;
    docsisParams.DOCSIS_DwStreamChPwr = (char*)malloc(BUFFER_LEN * sizeof(char));
    snprintf(docsisParams.DOCSIS_DwStreamChPwr, BUFFER_LEN,"0 dBmV");
    docsisParams.DOCSIS_UpStreamChPwr = (char*)malloc(BUFFER_LEN * sizeof(char));
    snprintf(docsisParams.DOCSIS_UpStreamChPwr, BUFFER_LEN,"0 dBmV");
    docsisParams.DOCSIS_SNR = (char*)malloc(BUFFER_LEN * sizeof(char));
    snprintf(docsisParams.DOCSIS_SNR, BUFFER_LEN,"0 dB");

    WA_DIAG_TUNER_GetDocsisParams(&docsisParams);

    WA_DIAG_TUNER_QamParams_t qamParams;
    qamParams.numLocked = (char*)malloc(BUFFER_LEN * sizeof(char));
    snprintf(qamParams.numLocked, BUFFER_LEN,"0 of 0");
    qamParams.QAM_ChPwr = (char*)malloc(BUFFER_LEN * sizeof(char));
    snprintf(qamParams.QAM_ChPwr, BUFFER_LEN,"0 dBmV");
    qamParams.QAM_SNR = (char*)malloc(BUFFER_LEN * sizeof(char));
    snprintf(qamParams.QAM_SNR, BUFFER_LEN,"0 dB");

    WA_DIAG_TUNER_GetQamParams(&qamParams);

    WA_DBG("docsisParams.DOCSIS_DwStreamChPwr returned: %s\n", docsisParams.DOCSIS_DwStreamChPwr);
    WA_DBG("docsisParams.DOCSIS_UpStreamChPwr returned: %s\n", docsisParams.DOCSIS_UpStreamChPwr);
    WA_DBG("docsisParams.DOCSIS_SNR returned: %s\n", docsisParams.DOCSIS_SNR);
    WA_DBG("qamParams.numLocked returned: %s\n", qamParams.numLocked);
    WA_DBG("qamParams.QAM_ChPwr returned: %s\n", qamParams.QAM_ChPwr);
    WA_DBG("qamParams.QAM_SNR returned: %s\n", qamParams.QAM_SNR);

    WA_DBG("system(num_ch) returned: %s\n", channels);
    WA_DBG("getDateAndTime returned: %s\n", date_time);

    json = json_pack("{s:s,s:s,s:s,s:s,s:s,s:s,s:s,s:s,s:s,s:s,s:s,s:s,s:s,s:s,s:s,s:s,s:s,s:s,s:s,s:s,s:s,s:s}",
       "Vendor", (!params[WA_UTILS_MFR_PARAM_MANUFACTURER].size? "N/A" : params[WA_UTILS_MFR_PARAM_MANUFACTURER].value),
       "Model",  (!params[WA_UTILS_MFR_PARAM_MODEL].size? "N/A" : params[WA_UTILS_MFR_PARAM_MODEL].value),
       "Serial", (!params[WA_UTILS_MFR_PARAM_SERIAL].size? "N/A" : params[WA_UTILS_MFR_PARAM_SERIAL].value),
       "MAC", (!mac_addr ? "N/A" : mac_addr),
       "RDK", (rdkver ? rdkver : "N/A"),
       "Ver", WA_VERSION,
       "xConf Version", (xconf_ver[0] == '\0' ? rdkver : xconf_ver),
       "Time Zone", date_time,
       "Receiver ID", rev_id,
       "eSTB IP", (!addr ? "N/A" : addr),
       "Number of Channels", channels,
       "Home Network", mocaNodeInfo,
       "MoCA RF Channel", ((moca_data[WA_MOCA_INFO_RF_CHANNEL].value[0] == '\0') ? "N/A" : moca_data[WA_MOCA_INFO_RF_CHANNEL].value),
       "MoCA NC", ((moca_data[WA_MOCA_INFO_NETWORK_CONTROLLER].value[0] == '\0') ? "N/A" : moca_data[WA_MOCA_INFO_NETWORK_CONTROLLER].value),
       "MoCA Bitrate", ((moca_data[WA_MOCA_INFO_TRANSMIT_RATE].value[0] == '\0') ? "N/A" : moca_data[WA_MOCA_INFO_TRANSMIT_RATE].value),
       "MoCA SNR", ((moca_data[WA_MOCA_INFO_NODE_SNR].value[0] == '\0') ? "N/A" : moca_data[WA_MOCA_INFO_NODE_SNR].value),
       "Video Tuner Locked", qamParams.numLocked,
       "Video Tuner Power", qamParams.QAM_ChPwr,
       "Video Tuner SNR", qamParams.QAM_SNR,
       "DOCSIS DwStrmPower", docsisParams.DOCSIS_DwStreamChPwr,
       "DOCSIS UpStrmPower", docsisParams.DOCSIS_UpStreamChPwr,
       "DOCSIS SNR", docsisParams.DOCSIS_SNR);

    if (docsisParams.DOCSIS_BootState != 1)
    {
        json_object_set_new(json, "DOCSIS State", json_string(DOCSIS_CONNECTING));
        json_object_del(json, "DOCSIS DwStrmPower");
        json_object_del(json, "DOCSIS UpStrmPower");
        json_object_del(json, "DOCSIS SNR");
    }

    // free tuner params mallocs
    if (docsisParams.DOCSIS_DwStreamChPwr != NULL)
        free(docsisParams.DOCSIS_DwStreamChPwr);
    if (docsisParams.DOCSIS_UpStreamChPwr != NULL)
        free(docsisParams.DOCSIS_UpStreamChPwr);
    if (docsisParams.DOCSIS_SNR != NULL)
        free(docsisParams.DOCSIS_SNR);

    if (qamParams.numLocked != NULL)
        free(qamParams.numLocked);
    if (qamParams.QAM_ChPwr != NULL)
        free(qamParams.QAM_ChPwr);
    if (qamParams.QAM_SNR != NULL)
        free(qamParams.QAM_SNR);
#else
    temperatureGet(&cpuTemp[0], sizeof(cpuTemp));
    WA_DBG("State of CPU temperature: %s\n", cpuTemp);
#ifdef HAVE_DIAG_MOCA
    json = json_pack("{s:s,s:s,s:s,s:s,s:s,s:s,s:s,s:s,s:s,s:s,s:s,s:s,s:s,s:s,s:s}",
       "Vendor", (!params[WA_UTILS_MFR_PARAM_MANUFACTURER].size? "N/A" : params[WA_UTILS_MFR_PARAM_MANUFACTURER].value),
       "Model",  (!params[WA_UTILS_MFR_PARAM_MODEL].size? "N/A" : params[WA_UTILS_MFR_PARAM_MODEL].value),
       "Serial", (!params[WA_UTILS_MFR_PARAM_SERIAL].size? "N/A" : params[WA_UTILS_MFR_PARAM_SERIAL].value),
       "MAC", (!mac_addr ? "N/A" : mac_addr),
       "RDK", (rdkver ? rdkver : "N/A"),
       "Ver", WA_VERSION,
       "xConf Version", (xconf_ver[0] == '\0' ? rdkver : xconf_ver),
       "Receiver ID", rev_id,
       "eSTB IP", (!addr ? "N/A" : addr),
       "Home Network", mocaNodeInfo,
       "Device Temperature", cpuTemp,
       "MoCA RF Channel", ((moca_data[WA_MOCA_INFO_RF_CHANNEL].value[0] == '\0') ? "N/A" : moca_data[WA_MOCA_INFO_RF_CHANNEL].value),
       "MoCA NC", ((moca_data[WA_MOCA_INFO_NETWORK_CONTROLLER].value[0] == '\0') ? "N/A" : moca_data[WA_MOCA_INFO_NETWORK_CONTROLLER].value),
       "MoCA Bitrate", ((moca_data[WA_MOCA_INFO_TRANSMIT_RATE].value[0] == '\0') ? "N/A" : moca_data[WA_MOCA_INFO_TRANSMIT_RATE].value),
       "MoCA SNR", ((moca_data[WA_MOCA_INFO_NODE_SNR].value[0] == '\0') ? "N/A" : moca_data[WA_MOCA_INFO_NODE_SNR].value));

#endif /* HAVE_DIAG_MOCA */
#ifdef HAVE_DIAG_WIFI
    json = json_pack("{s:s,s:s,s:s,s:s,s:s,s:s,s:s,s:s,s:s,s:s,s:s,s:s,s:s,s:s,s:s}",
       "Vendor", (!params[WA_UTILS_MFR_PARAM_MANUFACTURER].size? "N/A" : params[WA_UTILS_MFR_PARAM_MANUFACTURER].value),
       "Model",  (!params[WA_UTILS_MFR_PARAM_MODEL].size? "N/A" : params[WA_UTILS_MFR_PARAM_MODEL].value),
       "Serial", (!params[WA_UTILS_MFR_PARAM_SERIAL].size? "N/A" : params[WA_UTILS_MFR_PARAM_SERIAL].value),
       "MAC", (!mac_addr ? "N/A" : mac_addr),
       "RDK", (rdkver ? rdkver : "N/A"),
       "Ver", WA_VERSION,
       "xConf Version", (xconf_ver[0] == '\0' ? rdkver : xconf_ver),
       "Receiver ID", rev_id,
       "eSTB IP", (!addr ? "N/A" : addr),
       "Home Network", mocaNodeInfo,
       "Device Temperature", cpuTemp,
       "WiFi SSID", ((wifi_data[WA_WIFI_SSID].value[0] == '\0') ? "N/A" : wifi_data[WA_WIFI_SSID].value),
       "WiFi SSID MAC", ((wifi_data[WA_WIFI_SSID_MAC_ADDR].value[0] == '\0') ? "N/A" : wifi_data[WA_WIFI_SSID_MAC_ADDR].value),
       "WiFi Op Frequency", ((wifi_data[WA_WIFI_OPER_FREQ].value[0] == '\0') ? "N/A" : wifi_data[WA_WIFI_OPER_FREQ].value),
       "WiFi Signal Strength", ((wifi_data[WA_WIFI_SIGNAL_STRENGTH].value[0] == '\0') ? "N/A" : wifi_data[WA_WIFI_SIGNAL_STRENGTH].value));

#endif /* HAVE_DIAG_WIFI */
#endif /* MEDIA_CLIENT */
    exit:
    if (json == NULL)
    {
        WA_ERROR("Error in JSON.. Ignoring all data \n");
    }

    free(rdkver);
    free(addr);
    free(mac_addr);

    for(i = 0; i < sizeof(params)/sizeof(params[0]); i++)
    {
       if (params[i].value != NULL)
       {
           free(params[i].value);
       }
    }

#ifdef HAVE_DIAG_MOCA
    for(i = 0; i < WA_MOCA_INFO_MAX; i++)
    {
       if (moca_data[i].value != NULL)
       {
           free(moca_data[i].value);
       }
    }
#endif /* HAVE_DIAG_MOCA */

#ifdef HAVE_DIAG_WIFI
    for(i = 0; i < WA_WIFI_INFO_MAX; i++)
    {
       if (wifi_data[i].value != NULL)
       {
           free(wifi_data[i].value);
       }
    }
#endif /* HAVE_DIAG_WIFI */

    // free moca node info
    if (mocaNodeInfo != NULL)
        free(mocaNodeInfo);

    WA_UTILS_IARM_Disconnect();

    WA_RETURN("SYSINFO_Info(): %p\n", json);

    return json;
}

static bool getUpnpResults()
{
    IARM_Result_t iarm_result = IARM_RESULT_IPCCORE_FAIL;
    int is_connected = 0;

    IARM_Bus_SYSMGR_GetXUPNPDeviceInfo_Param_t *param = NULL;
    char upnpResults[MESSAGE_LENGTH+1];
    json_error_t jerror;
    json_t *json = NULL;
    char tmp[BUFFER_LENGTH - DEVNAME_LENGTH] = {'\0'};

    iarm_result = IARM_Bus_IsConnected(_IARM_XUPNP_NAME, &is_connected);
    if (iarm_result != IARM_RESULT_SUCCESS)
    {
        WA_ERROR("getUpnpResults(): IARM_Bus_IsConnected('%s') failed\n", _IARM_XUPNP_NAME);
        return false;
    }

    if (!is_connected)
    {
        WA_ERROR("getUpnpResults(): XUPNP not available\n");
        return false;
    }

    iarm_result = IARM_Malloc(IARM_MEMTYPE_PROCESSLOCAL, sizeof(IARM_Bus_SYSMGR_GetXUPNPDeviceInfo_Param_t) + MESSAGE_LENGTH + 1, (void**)&param);
    if (iarm_result != IARM_RESULT_SUCCESS)
    {
        WA_ERROR("getUpnpResults(): IARM_Malloc() failed\n");
        return false;
    }

    if (param)
    {
        memset(param, 0, sizeof(*param));
        param->bufLength = MESSAGE_LENGTH;

        iarm_result = IARM_Bus_Call(_IARM_XUPNP_NAME, IARM_BUS_XUPNP_API_GetXUPNPDeviceInfo, (void *)param, sizeof(IARM_Bus_SYSMGR_GetXUPNPDeviceInfo_Param_t) + MESSAGE_LENGTH + 1);

        if (iarm_result != IARM_RESULT_SUCCESS)
        {
            WA_ERROR("getUpnpResults(): IARM_Bus_Call() returned %i\n", iarm_result);
            IARM_Free(IARM_MEMTYPE_PROCESSLOCAL, param);
            return false;
        }

        WA_DBG("getUpnpResults(): IARM_RESULT_SUCCESS\n");
        memcpy(upnpResults, ((char *)param + sizeof(IARM_Bus_SYSMGR_GetXUPNPDeviceInfo_Param_t)), param->bufLength);
        upnpResults[param->bufLength] = '\0';

        json = json_loadb(upnpResults, MESSAGE_LENGTH, JSON_DISABLE_EOF_CHECK, &jerror);
        if (!json)
        {
            WA_ERROR("getUpnpResults(): json_loads() error\n");
            json_decref(json);
            IARM_Free(IARM_MEMTYPE_PROCESSLOCAL, param);
            return false;
        }

        json_t *jarr = json_object_get(json, "xmediagateways");
        if (!jarr)
        {
            WA_ERROR("getUpnpResults(): Couldn't get the array of xmediagateways\n");
            json_decref(jarr);
            json_decref(json);
            IARM_Free(IARM_MEMTYPE_PROCESSLOCAL, param);
            return false;
        }

        for (size_t i = 0; i < json_array_size(jarr); i++)
        {
            json_t *jval = json_array_get(jarr, i);
            if (!jval)
            {
                WA_ERROR("getUpnpResults(): Not a json object in array for %i element\n", i);
                json_decref(jval);
                json_decref(jarr);
                json_decref(json);
                IARM_Free(IARM_MEMTYPE_PROCESSLOCAL, param);
                return false;
            }

            char param_Sno[MAC_LENGTH];
            char param_devName[DEVNAME_LENGTH + 1];
            json_t *jparam_devName = json_object_get(jval, "deviceName");

            /* If deviceName is not empty */
            if (strcmp(json_string_value(jparam_devName), ""))
            {
                strxfrm(param_devName, json_string_value(jparam_devName), DEVNAME_LENGTH); /* Get first 10 characters */
                param_devName[DEVNAME_LENGTH] = '\0';
                snprintf(mocaNodeInfo, BUFFER_LENGTH, "%s%s", tmp, param_devName);
            }
            else
            {
                json_t *jparam_devType = json_object_get(jval, "DevType");
                json_t *jparam_sno = json_object_get(jval, "sno");

                if (jparam_sno)
                {
                    strcpy(param_Sno, json_string_value(jparam_sno));
                }

                /* Take only last 4 characters of serial number */
                const int len = 4;
                char token[len];
                int digits = 0;
                int sno_length = strlen(param_Sno);

                for (int index = (sno_length - len); index < sno_length; index++)
                {
                    token[digits++] = param_Sno[index];
                }
                token[digits] = '\0';

                snprintf(mocaNodeInfo, BUFFER_LENGTH, "%s%s(%s)", tmp, json_string_value(jparam_devType), token);
                json_decref(jparam_devType);
                json_decref(jparam_sno);
            }

            strcpy(tmp, mocaNodeInfo);
            strcat(tmp, ", ");
            json_decref(jparam_devName);

            json_t *jparam_dayLightTime = json_object_get(jval, "usesdaylighttime");
            if(jparam_dayLightTime)
            {
                if(!strcmp("yes", json_string_value(jparam_dayLightTime)) && dstOffset == 0)
                {
                    char param_dstOffset[256];
                    json_t *jparam_dstOffset = json_object_get(jval, "dstoffset");
                    if(jparam_dstOffset)
                    {
                        strcpy(param_dstOffset, json_string_value(jparam_dstOffset));
                        dstOffset = atoi(param_dstOffset) / 60; /* convert offset value received in minutes to hours */ 
                        WA_DBG("DST off set = %d\n", dstOffset);
                    }
                }
            }
        }
    }

    IARM_Free(IARM_MEMTYPE_PROCESSLOCAL, param);

    return true;
}

#ifndef MEDIA_CLIENT
static int getDateAndTime(char *date_time, size_t size)
{
    WA_UTILS_SNMP_Resp_t val;

    val.type = WA_UTILS_SNMP_RESP_TYPE_LONG;

    if (!WA_UTILS_SNMP_GetNumber(SNMP_SERVER, OID_TIME_ZONE, &val, WA_UTILS_SNMP_REQ_TYPE_WALK))
    {
        WA_ERROR("TimeZone fetching, failed with %s.\n", OID_TIME_ZONE);

        return -1;
    }

    snprintf(date_time, size, "UTC %ld", val.data.l + dstOffset);

    WA_RETURN("TimeZone value: %s.\n", date_time);

    return 0;
}

static int getReceiverId(char *rev_id, size_t size)
{
    if (!WA_UTILS_SNMP_GetString(SNMP_SERVER, OID_RECEIVER_ID, rev_id, size, WA_UTILS_SNMP_REQ_TYPE_WALK))
    {
        WA_ERROR("Receiver ID fetching, failed.\n");

        return -1;
    }

    WA_RETURN("Receiver ID: %s.\n", rev_id);

    return 0;
}

static int xconfVerGet(char *xconf_ver, size_t size)
{
    if (!WA_UTILS_SNMP_GetString(SNMP_SERVER, OID_XCONF_VER, xconf_ver, size, WA_UTILS_SNMP_REQ_TYPE_WALK))
    {
        WA_DBG("xConf version fetching, failed.\n");
        xconf_ver[0] = '\0';

        return -1;
    }

    WA_RETURN("xConf Version: %s.\n", xconf_ver);

    return 0;
}

static int getMocaNetworkInfo(SysinfoParam_t *moca)
{
    int index = -1;
    int group = 0; // 0 for MOCA_GROUP_11
    size_t size = 256;

    WA_UTILS_SNMP_Resp_t moca_data[WA_MOCA_INFO_MAX];
    WA_UTILS_SNMP_ReqType_t type = WA_UTILS_SNMP_REQ_TYPE_WALK;

    for (int i = 0; i < WA_MOCA_INFO_MAX; i++)
    {
        moca_data[i].type = WA_UTILS_SNMP_RESP_TYPE_LONG;
    }

    if (getMocaIfRFChannelFrequency(&group, index, &moca_data[WA_MOCA_INFO_RF_CHANNEL], type) >= 0)
    {
        snprintf(moca[WA_MOCA_INFO_RF_CHANNEL].value, size, "%ld MHz", moca_data[WA_MOCA_INFO_RF_CHANNEL].data.l);
        WA_DBG("moca[WA_MOCA_INFO_RF_CHANNEL].value = %s\n", moca[WA_MOCA_INFO_RF_CHANNEL].value);
    }
    else
        WA_ERROR("getMocaNetworkInfo(): getMocaIfRFChannelFrequency() failed. Group=%d\n", group);

    if (getMocaIfNetworkController(group, index, &moca_data[WA_MOCA_INFO_NETWORK_CONTROLLER], type) >= 0)
    {
        snprintf(moca[WA_MOCA_INFO_NETWORK_CONTROLLER].value, size, "%ld", moca_data[WA_MOCA_INFO_NETWORK_CONTROLLER].data.l);
        WA_DBG("moca[WA_MOCA_INFO_NETWORK_CONTROLLER].value = %s\n", moca[WA_MOCA_INFO_NETWORK_CONTROLLER].value);
    }
    else
         WA_ERROR("getMocaNetworkInfo(): getMocaIfNetworkController() failed. Group=%d\n", group);

    if (getMocaIfTransmitRate(group, index, &moca_data[WA_MOCA_INFO_TRANSMIT_RATE], type) >= 0)
    {
        snprintf(moca[WA_MOCA_INFO_TRANSMIT_RATE].value, size, "%ld Mbps", moca_data[WA_MOCA_INFO_TRANSMIT_RATE].data.l);
        WA_DBG("moca[WA_MOCA_INFO_TRANSMIT_RATE].value = %s\n", moca[WA_MOCA_INFO_TRANSMIT_RATE].value);
    }
    else
        WA_ERROR("getMocaNetworkInfo(): getMocaIfTransmitRate() failed. Group=%d\n", group);

    if (getMocaNodeSNR(group, index, &moca_data[WA_MOCA_INFO_NODE_SNR], type) >= 0)
    {
        snprintf(moca[WA_MOCA_INFO_NODE_SNR].value, size, "%ld dB", moca_data[WA_MOCA_INFO_NODE_SNR].data.l);
        WA_DBG("moca[WA_MOCA_INFO_NODE_SNR].value = %s\n", moca[WA_MOCA_INFO_NODE_SNR].value);
    }
    else
        WA_ERROR("getMocaNetworkInfo(): getMocaNodeSNR() failed. Group=%d\n", group);

    return 0;
}
#else
static int getReceiverId(char *rev_id, size_t size)
{
    char *value;
    if (getSysInfoParam_IARM(TR69_RECEIVER_ID, &value))
    {
        WA_ERROR("Receiver ID fetching, failed.\n");

        return -1;
    }

    strcpy(rev_id, value);
    WA_RETURN("Receiver ID: %s\n", rev_id);
    return 0;
}

static int xconfVerGet(char *xconf_ver, size_t size)
{
    char *value;
    if (getSysInfoParam_IARM(TR69_XCONF_VERSION, &value))
    {
        WA_ERROR("xConf version fetching, failed.\n");
        return -1;
    }

    strcpy(xconf_ver, value);
    WA_RETURN("xConf Version: %s\n", xconf_ver);
    return 0;
}

static int read_RFCProperty(const char* key)
{
    RFC_ParamData_t param;
    char valueBuf[16] = {'\0'};
    int dataLen;
    int threshold_value = 0;

    WDMP_STATUS status = getRFCParameter(THERMAL_PROTECTION_GROUP, key, &param);

    if (status == WDMP_SUCCESS)
    {
        dataLen = strlen(param.value);
        if ((param.value[0] == '"') && (param.value[dataLen-1] == '"'))
        {
            // remove quotes arround data
            strncpy (valueBuf, &param.value[1], dataLen-2);
            valueBuf[dataLen-2] = '\0';
        }
        else
        {
            strncpy (valueBuf, param.value, dataLen);
            valueBuf[dataLen] = '\0';
        }

        threshold_value = atoi(&valueBuf[0]);
        WA_DBG("read_RFCProperty() name = %s, type = %d, value = %s, status = %d\n", param.name, param.type, param.value, status);
    }
    else
    {
        WA_DBG("read_RFCProperty() property %s is not configured, Status %d  \n", key, status);
    }

    return threshold_value;
}

static int temperatureGet(char *cpuTemp, size_t size)
{
    int data;
    int high = 0, critical = 0;
    char *value;
    if (getSysInfoParam_IARM(TR69_CPU_TEMP, &value))
    {
        WA_ERROR("temperatureGet(): CPU temperature value fetching, failed.\n");
        return -1;
    }

    data = *(int*)value;
    WA_DBG("temperatureGet(): CPU temperature= %d\n", data);

    /* get threshold values from RFC API */
    RFC_ParamData_t param;
    bool isRFCEnabled = false;
    WDMP_STATUS status = getRFCParameter(THERMAL_PROTECTION_GROUP, "RFC_ENABLE_ThermalProtection", &param);

    if (status == WDMP_SUCCESS)
    {
        WA_DBG("temperatureGet() RFC_ENABLE_ThermalProtection value is %s  \n", param.value);
        if (!strcasecmp(param.value, "true") || !strcasecmp(param.value, "\"true\""))
        {
            isRFCEnabled = true;
            high = read_RFCProperty("RFC_DATA_ThermalProtection_DECLOCK_CONCERN_THRESHOLD");
            critical = read_RFCProperty("RFC_DATA_ThermalProtection_DECLOCK_CRITICAL_THRESHOLD");
            WA_DBG("temperatureGet(): Got current temperature thresholds using RFC - high: %d, critical: %d\n", high, critical);
        }
    }

    if (!isRFCEnabled || (high == 0) || (critical == 0))
    {
        WA_DBG("temperatureGet() Required RFC properties are not available with RFC_ENABLE_ThermalProtection status = %d  \n", status);
        /* Code to find high, critical threshold values through IARM */
        IARM_Bus_PWRMgr_GetTempThresholds_Param_t param;

        IARM_Result_t res = IARM_Bus_Call(IARM_BUS_PWRMGR_NAME, IARM_BUS_PWRMGR_API_GetTemperatureThresholds, (void *)&param, sizeof(param));

        if (res == IARM_RESULT_SUCCESS)
        {
            high = param.tempHigh;
            critical = param.tempCritical;
            WA_DBG("temperatureGet(): Got current temperature thresholds using IARM_Call - high: %d, critical: %d\n", high, critical);
        } /* End of Threshold values code */
        else
        {
            WA_ERROR("temperatureGet(): IARM_Bus_Call() for Get Temperature thresholds failed with result = %d\n", res);
        }
    }

    if ((high == 0) || (critical == 0))
    {
        WA_ERROR("temperatureGet(): Unable to get Temperature threshold values using RFC and IARM Bus call\n");
        return 0;
    }

    if (data >= critical)
    {
        strcpy(cpuTemp, "Critical");
    }
    else if (data >= high)
    {
        strcpy(cpuTemp, "High");
    }
    else
    {
        strcpy(cpuTemp, "Normal");
    }

    WA_RETURN("CPU temperature state: %s\n", cpuTemp);
    return 0;
}

#ifdef HAVE_DIAG_MOCA
static int getMocaNetworkInfo(SysinfoParam_t *moca)
{
    int value = 0;
    size_t size = 256;
    char data[size];

    if (getMocaIfRFChannelFrequency_IARM(&value) == 0)
    {
        snprintf(moca[WA_MOCA_INFO_RF_CHANNEL].value, size, "%i MHz", value );
        WA_DBG("moca[WA_MOCA_INFO_RF_CHANNEL].value = %s\n", moca[WA_MOCA_INFO_RF_CHANNEL].value);
    }
    else
        WA_ERROR("getMocaNetworkInfo(): getMocaIfRFChannelFrequency_IARM() failed\n");


    if (getMocaIfNetworkController_IARM(&value) == 0)
    {
        snprintf(moca[WA_MOCA_INFO_NETWORK_CONTROLLER].value, size, "%i", value);
        WA_DBG("moca[WA_MOCA_INFO_NETWORK_CONTROLLER].value = %s\n", moca[WA_MOCA_INFO_NETWORK_CONTROLLER].value);
    }
    else
        WA_ERROR("getMocaNetworkInfo(): getMocaIfNetworkController_IARM() failed\n");


    memset(&data, 0, sizeof(data));
    if (getMocaIfTransmitRate_IARM(&data[0]) == 0)
    {
        snprintf(moca[WA_MOCA_INFO_TRANSMIT_RATE].value, size, "%s", data);
        WA_DBG("moca[WA_MOCA_INFO_TRANSMIT_RATE].value = %s\n", moca[WA_MOCA_INFO_TRANSMIT_RATE].value);
    }
    else
        WA_ERROR("getMocaNetworkInfo(): getMocaIfTransmitRate_IARM() failed\n");


    memset(&data, 0, sizeof(data));
    if (getMocaNodeSNR_IARM(&data[0]) == 0)
    {
        snprintf(moca[WA_MOCA_INFO_NODE_SNR].value, size, "%s", data);
        WA_DBG("moca[WA_MOCA_INFO_NODE_SNR].value = %s\n", moca[WA_MOCA_INFO_NODE_SNR].value);
    }
    else
        WA_ERROR("getMocaNetworkInfo(): getMocaNodeSNR_IARM() failed\n");

    return 0;
}
#endif /* HAVE_DIAG_MOCA */

#ifdef HAVE_DIAG_WIFI
static int getWifiNetworkInfo(SysinfoParam_t *wifi)
{
    int value = 0;
    char *data;
    size_t size = 256;

    /* Check for SSID */
    if (getWifiSSID_IARM(&data) == 0)
    {
        snprintf(wifi[WA_WIFI_SSID].value, size, "%s", data);
        WA_DBG("wifi[WA_WIFI_SSID].value = %s\n", wifi[WA_WIFI_SSID].value);
    }
    else
        WA_ERROR("getWiFiNetworkInfo(): SSID status failed\n");

    /* Check for MAC Address */
    if (getWifiMacAddress_IARM(&data) == 0)
    {
        snprintf(wifi[WA_WIFI_SSID_MAC_ADDR].value, size, "%s", data);
        WA_DBG("wifi[WA_WIFI_SSID_MAC_ADDR].value = %s\n", wifi[WA_WIFI_SSID_MAC_ADDR].value);
    }
    else
        WA_ERROR("getWiFiNetworkInfo(): WiFi Mac address fetching failed\n");

    /* Check for Operator frequency */
    if (getWifiOperFrequency_IARM(&data) == 0)
    {
        snprintf(wifi[WA_WIFI_OPER_FREQ].value, size, "%s", data);
        WA_DBG("wifi[WA_WIFI_OPER_FREQ].value = %s\n", wifi[WA_WIFI_OPER_FREQ].value);
    }
    else
        WA_ERROR("getWiFiNetworkInfo(): Operator frequency fetching failed\n");

    /* Check for Signal strength */
    if (getWifiSignalStrength_IARM(&value) == 0)
    {
        snprintf(wifi[WA_WIFI_SIGNAL_STRENGTH].value, size, "%i", value);
        WA_DBG("wifi[WA_WIFI_SIGNAL_STRENGTH].value = %s\n", wifi[WA_WIFI_SIGNAL_STRENGTH].value);
    }
    else
        WA_ERROR("getWiFiNetworkInfo(): Signal strength fetching failed\n");

    if (value == 0)
    {
        wifi[WA_WIFI_SSID].value[0] = '\0';
    }

    return 0;
}
#endif /* HAVE_DIAG_WIFI */

int getSysInfoParam_IARM(char *param, char **value)
{
    IARM_Result_t iarm_result = IARM_RESULT_IPCCORE_FAIL;
    int is_connected = 0;

    WA_ENTER("getSysInfoParam_IARM()\n");

    iarm_result = IARM_Bus_IsConnected(IARM_BUS_TR69HOSTIFMGR_NAME, &is_connected);
    if (iarm_result != IARM_RESULT_SUCCESS)
    {
        WA_ERROR("getSysInfoParam_IARM(): IARM_Bus_IsConnected('%s') failed\n", IARM_BUS_TR69HOSTIFMGR_NAME);
        return -1;
    }

    HOSTIF_MsgData_t *stMsgDataParam = NULL;

    iarm_result = IARM_Malloc(IARM_MEMTYPE_PROCESSLOCAL, sizeof(*stMsgDataParam), (void**)&stMsgDataParam);
    if ((iarm_result == IARM_RESULT_SUCCESS) && stMsgDataParam)
    {
        memset(stMsgDataParam, 0, sizeof(*stMsgDataParam));
        snprintf(stMsgDataParam->paramName, TR69HOSTIFMGR_MAX_PARAM_LEN, "%s", param);

        WA_DBG("getSysInfoParam_IARM(): IARM_Bus_Call('%s', '%s', %s)\n", IARM_BUS_TR69HOSTIFMGR_NAME, IARM_BUS_TR69HOSTIFMGR_API_GetParams, stMsgDataParam->paramName);
        iarm_result = IARM_Bus_Call(IARM_BUS_TR69HOSTIFMGR_NAME, IARM_BUS_TR69HOSTIFMGR_API_GetParams, (void *)stMsgDataParam, sizeof(*stMsgDataParam));

        if (iarm_result != IARM_RESULT_SUCCESS)
        {
            WA_ERROR("getSysInfoParam_IARM(): IARM_Bus_Call('%s') failed\n", IARM_BUS_TR69HOSTIFMGR_NAME);
            IARM_Free(IARM_MEMTYPE_PROCESSLOCAL, stMsgDataParam);
            return -1;
        }

        if (!strcmp(param, TR69_CPU_TEMP))
        {
            sysParam = (char *)&stMsgDataParam->paramValue[0];
            sysParamInt = *(int *)sysParam;
            *value = (char *)&sysParamInt;
        }
        else
        {
            strcpy(sysIARM, stMsgDataParam->paramValue);
            *value = (char *)sysIARM;
        }

        WA_DBG("getSysInfoParam_IARM() %s: %s\n", param, *value);

        IARM_Free(IARM_MEMTYPE_PROCESSLOCAL, stMsgDataParam);

        return 0;
    }

    WA_ERROR("getSysInfoParam_IARM(): IARM_Malloc('%s') failed\n", IARM_BUS_TR69HOSTIFMGR_NAME);
    return -1;
}
#endif /* MEDIA_CLIENT */

/* End of doxygen group */
/*! @} */

/* EOF */
