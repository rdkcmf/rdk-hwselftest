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

/* rdk specific */
#include "wa_iarm.h"
#include "wa_mfr.h"

/* module interface */
#include "wa_diag_sysinfo.h"
#include "wa_diag_moca.h"
#include "wa_diag_modem.h"
#include "wa_diag_errcodes.h"
#include "wa_diag_tuner.h"

/*****************************************************************************
 * GLOBAL VARIABLE DEFINITIONS
 *****************************************************************************/

/*****************************************************************************
 * LOCAL DEFINITIONS
 *****************************************************************************/

#define MAX_LINE 100
#define DEV_FILE_PATH   "/tmp/.deviceDetails.cache"
#define STB_IP          "estb_ip="
#define SNMP_SERVER     "localhost"
#define OID_TIME_ZONE   "OC-STB-HOST-MIB::ocStbHostCardTimeZoneOffset"
#define OID_RECEIVER_ID "XcaliburClientMIB::xreReceiverId"
#define SI_PATH         "/si_cache_parser_121 /opt/persistent/si"
#define TMP_SI_PATH     "/si_cache_parser_121 /tmp/mnt/diska3/persistent/si"
#define OID_XCONF_VER   "FWDNLDMIB::swUpdateDownloadVersion"

/*****************************************************************************
 * LOCAL TYPES
 *****************************************************************************/
typedef struct {
    size_t size;
    char *value;
}SysinfoParam_t;

typedef enum
{
    WA_MOCA_INFO_RF_CHANNEL,
    WA_MOCA_INFO_NETWORK_CONTROLLER,
    WA_MOCA_INFO_TRANSMIT_RATE,
    WA_MOCA_INFO_NODE_SNR,
    WA_MOCA_INFO_MAX
} WA_MocaInfo_t;

/*****************************************************************************
 * LOCAL FUNCTION PROTOTYPES
 *****************************************************************************/
static json_t *sysinfo_Get();
static int getDateAndTime(char *date_time, size_t size);
static int getReceiverId(char *rev_id, size_t size);
static int xconfVerGet(char *xconf_ver, size_t size);
static int getMocaNetworkInfo(SysinfoParam_t *moca);

/*****************************************************************************
 * LOCAL VARIABLE DECLARATIONS
 *****************************************************************************/

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
    char *rdkver;
    char *addr;
    char date_time[256];
    char rev_id[256];
    char xconf_ver[256] = "Not Available";
    char num_ch[256];
    char channels[256];
    FILE *fp;
    int nMocaItems;
    SysinfoParam_t moca_data[WA_MOCA_INFO_MAX];

    WA_ENTER("SYSINFO_Get()\n");

    memset(&params, 0, sizeof(params));
    memset(&moca_data, 0, sizeof(moca_data));

    if(WA_UTILS_IARM_Connect())
        return json;

    rdkver = WA_UTILS_FILEOPS_OptionFind("/version.txt", "imagename:");

    for (i = 0; i < sizeof(params)/sizeof(params[0]); i++)
    {
        if (WA_UTILS_MFR_ReadSerializedData(i, &params[i].size, &params[i].value) != 0)
            WA_ERROR("sysinfo_Get(): WA_UTILS_MFR_ReadSerializedData(%i) failed\n", i);
    }

    mocaNodeInfo = (char*)malloc(BUFFER_LENGTH * sizeof(char));
    if(!getUpnpResults())
    {
        snprintf(mocaNodeInfo, BUFFER_LENGTH, "Not Available");
        WA_ERROR("Home Network returned: %s\n", mocaNodeInfo);
    }
    WA_DBG("Home Network returned: %s\n", mocaNodeInfo);

    addr = WA_UTILS_FILEOPS_OptionFind(DEV_FILE_PATH, STB_IP);
    getDateAndTime(&date_time[0], sizeof(date_time));
    getReceiverId(&rev_id[0], sizeof(rev_id));
    xconfVerGet(&xconf_ver[0], sizeof(xconf_ver));

    snprintf(num_ch, sizeof(num_ch), "%s | grep \"SRCID\" | wc -l", SI_PATH);
    fp = popen(num_ch, "r");
    fscanf(fp, "%s", channels);
    fclose(fp);

    if(channels[0] == '\0' || channels[0] == '0') {
        WA_DBG("grep from %s failed\n", SI_PATH);
        snprintf(num_ch, sizeof(num_ch), "%s | grep \"SRCID\" | wc -l", TMP_SI_PATH);
        fp = popen(num_ch, "r");
        fscanf(fp, "%s", channels);
        fclose(fp);
        if(channels[0] == '\0') {
            WA_DBG("grep from %s failed\n", TMP_SI_PATH);
        }
    }

    /* MoCA */
    for (int i = 0; i < WA_MOCA_INFO_MAX; i++)
    {
        moca_data[i].value = (char*)malloc(sizeof(char) * 256);
    }

    nMocaItems = getMocaNetworkInfo(&moca_data[0]);
    if (nMocaItems != 0)
    {
        WA_ERROR("sysinfo_Get(): getMocaNetworkInfo failed to fetch %d items\n", nMocaItems);
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

    WA_DBG("IP ADDRESS returned: %s\n", addr);
    WA_DBG("getDateAndTime returned: %s\n", date_time);
    WA_DBG("getReceiverId returned: %s\n", rev_id);
    WA_DBG("xconfVerGet returned: %s\n", xconf_ver);
    WA_DBG("system(num_ch) returned: %s\n", channels);

    for (int i = 0; i < WA_MOCA_INFO_MAX; i++)
    {
        if (moca_data[i].value == NULL)
        {
            WA_ERROR("NULL pointer exception in moca_data %i\n", i);
        }

        WA_DBG("moca_data(%i) returned: %s\n", i, moca_data[i].value);
    }


    json = json_pack("{s:s,s:s,s:s,s:s,s:s,s:s,s:s,s:s,s:s,s:s,s:s,s:s,s:s,s:s,s:s,s:s,s:s,s:s,s:s,s:s,s:s}",
       "Vendor", (!params[WA_UTILS_MFR_PARAM_MANUFACTURER].size? "N/A" : params[WA_UTILS_MFR_PARAM_MANUFACTURER].value),
       "Model",  (!params[WA_UTILS_MFR_PARAM_MODEL].size? "N/A" : params[WA_UTILS_MFR_PARAM_MODEL].value),
       "Serial", (!params[WA_UTILS_MFR_PARAM_SERIAL].size? "N/A" : params[WA_UTILS_MFR_PARAM_SERIAL].value),
       "RDK",    (rdkver ? rdkver : "N/A"),
       "Ver", WA_VERSION,
       "xConf Version", xconf_ver,
       "Time Zone", date_time,
       "Receiver ID", rev_id,
       "eSTB IP", (!addr ? "N/A" : addr),
       "Number of Channels", channels,
       "Home Network", mocaNodeInfo,
       "MoCA RF Channel", (!moca_data[WA_MOCA_INFO_RF_CHANNEL].value ? "N/A" : moca_data[WA_MOCA_INFO_RF_CHANNEL].value),
       "MoCA NC", (!moca_data[WA_MOCA_INFO_NETWORK_CONTROLLER].value ? "N/A" : moca_data[WA_MOCA_INFO_NETWORK_CONTROLLER].value),
       "MoCA Bitrate", (!moca_data[WA_MOCA_INFO_TRANSMIT_RATE].value ? "N/A" : moca_data[WA_MOCA_INFO_TRANSMIT_RATE].value),
       "MoCA SNR", (!moca_data[WA_MOCA_INFO_NODE_SNR].value ? "N/A" : moca_data[WA_MOCA_INFO_NODE_SNR].value),
       "DOCSIS DwStrmPower", docsisParams.DOCSIS_DwStreamChPwr,
       "DOCSIS UpStrmPower", docsisParams.DOCSIS_UpStreamChPwr,
       "DOCSIS SNR", docsisParams.DOCSIS_SNR,
       "Video Tuner Locked", qamParams.numLocked,
       "Video Tuner Power", qamParams.QAM_ChPwr,
       "Video Tuner SNR", qamParams.QAM_SNR);

    if (json == NULL)
    {
        WA_ERROR("Error in JSON.. Ignoring all data \n");
    }

    free(rdkver);
    free(addr);

    for(i = 0; i < sizeof(params)/sizeof(params[0]); i++)
    {
       if(params[i].value != NULL)
       {
           free(params[i].value);
       }
    }

    for(i = 0; i < WA_MOCA_INFO_MAX; i++)
    {
       if(moca_data[i].value != NULL)
       {
           free(moca_data[i].value);
       }
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

    // free moca node info
    if (mocaNodeInfo != NULL)
        free(mocaNodeInfo);

    WA_UTILS_IARM_Disconnect();

    WA_RETURN("SYSINFO_Info(): %p\n", json);

    return json;
}

static int getDateAndTime(char *date_time, size_t size)
{
    WA_UTILS_SNMP_Resp_t val;

    val.type = WA_UTILS_SNMP_RESP_TYPE_LONG;

    if (!WA_UTILS_SNMP_GetNumber(SNMP_SERVER, OID_TIME_ZONE, &val, WA_UTILS_SNMP_REQ_TYPE_WALK))
    {
        WA_ERROR("TimeZone fetching, failed with %s.\n", OID_TIME_ZONE);

        return -1;
    }

    snprintf(date_time, size, "UTC %ld", val.data.l);

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
        WA_ERROR("xConf version fetching, failed.\n");

        return -1;
    }

    WA_RETURN("xConf Version: %s.\n", xconf_ver);

    return 0;
}

static int getMocaNetworkInfo(SysinfoParam_t *moca)
{
    int ret = 0;
    int index = -1;
    int group = 0; // 0 for MOCA_GROUP_11
    size_t size = 256;

    WA_UTILS_SNMP_Resp_t moca_data[WA_MOCA_INFO_MAX];
    WA_UTILS_SNMP_ReqType_t type = WA_UTILS_SNMP_REQ_TYPE_WALK;

    for (int i = 0; i < WA_MOCA_INFO_MAX; i++)
    {
        moca_data[i].type = WA_UTILS_SNMP_RESP_TYPE_LONG;
    }

    if (getMocaIfRFChannelFrequency(&group, index, &moca_data[WA_MOCA_INFO_RF_CHANNEL], type) < 0)
    {
        WA_ERROR("getMocaNetworkInfo(): getMocaIfRFChannelFrequency() failed. Group=%d\n", group);
        ret++;
    }

    snprintf(moca[WA_MOCA_INFO_RF_CHANNEL].value, size, "%ld MHz", moca_data[WA_MOCA_INFO_RF_CHANNEL].data.l);

    if (getMocaIfNetworkController(group, index, &moca_data[WA_MOCA_INFO_NETWORK_CONTROLLER], type) < 0)
    {
        WA_ERROR("getMocaNetworkInfo(): getMocaIfNetworkController() failed. Group=%d\n", group);
        ret++;
    }

    snprintf(moca[WA_MOCA_INFO_NETWORK_CONTROLLER].value, size, "%ld", moca_data[WA_MOCA_INFO_NETWORK_CONTROLLER].data.l);

    if (getMocaIfTransmitRate(group, index, &moca_data[WA_MOCA_INFO_TRANSMIT_RATE], type) < 0)
    {
        WA_ERROR("getMocaNetworkInfo(): getMocaIfTransmitRate() failed. Group=%d\n", group);
        ret++;
    }

    snprintf(moca[WA_MOCA_INFO_TRANSMIT_RATE].value, size, "%ld Mbps", moca_data[WA_MOCA_INFO_TRANSMIT_RATE].data.l);

    if (getMocaNodeSNR(group, index, &moca_data[WA_MOCA_INFO_NODE_SNR], type) < 0)
    {
        WA_ERROR("getMocaNetworkInfo(): getMocaNodeSNR() failed. Group=%d\n", group);
        ret++;
    }

    snprintf(moca[WA_MOCA_INFO_NODE_SNR].value, size, "%ld dB", moca_data[WA_MOCA_INFO_NODE_SNR].data.l);

    return ret;
}


/* End of doxygen group */
/*! @} */

/* EOF */
