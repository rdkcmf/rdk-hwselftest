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
 * @brief Diagnostic functions for tuner - implementation
 */

/** @addtogroup WA_DIAG_TUNER
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
#include <time.h>

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/
#ifndef USE_TRM
#define USE_RMF
#endif

#include "wa_diag.h"
#include "wa_debug.h"
#include "wa_fileops.h"
#include "wa_snmp_client.h"

/* rdk specific */
#include "wa_sicache.h"
#include "wa_rmf.h"

#ifdef USE_TRM
#include "wa_trh.h"
#endif

/* module interface */
#include "wa_diag_tuner.h"

/* NGAN */
#include <sys/stat.h>
#include "wa_log.h"
#include "wa_iarm.h"
#include "pwrMgr.h"
#include "libIBus.h"
#include "libIARMCore.h"
#include "hostIf_tr69ReqHandler.h"

/*****************************************************************************
 * GLOBAL VARIABLE DEFINITIONS
 *****************************************************************************/

/*****************************************************************************
 * LOCAL DEFINITIONS
 *****************************************************************************/

/* For the reason unknown the snmp staus is not reliable, define this to use platform specific method.*/

#define HWSLFTST 1

#ifdef HWSLFTST
#define OID_TUNER_STATE "OC-STB-HOST-MIB::ocStbHostInBandTunerState"
#define OID_TUNER_FREQUENCY "OC-STB-HOST-MIB::ocStbHostInBandTunerFrequency"
#define OID_TUNER_MODULATION_MODE "OC-STB-HOST-MIB::ocStbHostInBandTunerModulationMode"
#define OID_TUNER_POWER "OC-STB-HOST-MIB::ocStbHostInBandTunerPower"
#define OID_TUNER_SNR "OC-STB-HOST-MIB::ocStbHostInBandTunerSNRValue"

#define OID_DOCSIS_DOWNSTREAMPOWER "DOCS-IF-MIB::docsIfDownChannelPower"
#define OID_DOCSIS_UPSTREAMPOWER "DOCS-IF-MIB::docsIfCmStatusTxPower"
#define OID_DOCSIS_SNR "DOCS-IF-MIB::docsIfSigQSignalNoise"
#define SNMP_SERVER_ESTB "localhost"
#define SNMP_SERVER_ECM "192.168.100.1"
#define TUNER_LOCKED 5
/* NGAN */
#define OID_TUNER_BER "OC-STB-HOST-MIB::ocStbHostInBandTunerBER"
#endif

#define USE_FRONTEND_PROCFS 1
#define USE_UNRELIABLE_PROCFS_WORKAROUND 1

#ifndef USE_FRONTEND_PROCFS
#define OID_TUNER_STATE "OC-STB-HOST-MIB::ocStbHostInBandTunerState"
#define OID_TUNER_FREQUENCY "OC-STB-HOST-MIB::ocStbHostInBandTunerFrequency"
#define OID_TUNER_MODULATION_MODE "OC-STB-HOST-MIB::ocStbHostInBandTunerModulationMode"
#define OID_TUNER_POWER "OC-STB-HOST-MIB::ocStbHostInBandTunerPower"
#define OID_TUNER_SNR "OC-STB-HOST-MIB::ocStbHostInBandTunerSNRValue"
#define OID_DOCSIS_DOWNSTREAMPOWER "DOCS-IF-MIB::docsIfDownChannelPower"
#define OID_DOCSIS_UPSTREAMPOWER "DOCS-IF-MIB::docsIfCmStatusTxPower"
#define OID_DOCSIS_SNR "DOCS-IF-MIB::docsIfSigQSignalNoise"

#define SNMP_SERVER_ESTB "localhost"
#else
#define HWSELFTEST_TUNERESULTS_FILE "/opt/logs/hwselftest.tuneresults" /* NGAN */
#define PROCFS_STATUS_FILE "/proc/brcm/frontend"
#endif

#define LINE_LEN 256

#define VIDEO_DECODER_STATUS_FILE "/proc/brcm/video_decoder"
#define TRANSPORT_STATUS_FILE     "/proc/brcm/transport"

#define DATA_NOT_AVAILABLE_STRING "DATA_NOT_AVAILABLE"

#define TUNE_RESPONSE_TIMEOUT 14000 /* in [ms] */
#define REQUEST_PATTERN "/vldms/tuner?ocap_locator=ocap://tune://frequency=%u_modulation=%u_pgmno=%u"
#define REQUEST_MAX_LENGTH 256
#define QAM_CH_PWR_ERROR -25.0
#define QAM_CH_PWR_ZERO 0.0

#ifdef USE_RMF
#define MAX_REQUEST 512
#define HTML_RESPONSE_TIMEOUT 1000 /* in [ms] */
#endif /* USE_RMF */

#ifdef USE_TRM
#define TRM_TUNER_RESERVATION_TIME              (1000) /* time to resevere the tuner for, ms */
#define TRM_TUNER_RESERVATION_TIMEOUT           (30000) /* time to wait for tuner reservation response, ms */
#define TRM_TUNER_RESERVATION_RELEASE_TIMEOUT   (TRM_TUNER_RESERVATION_TIME) /* time to wait for tuner release response, ms */
#define TRM_TUNER_SESSIONS_TIMEOUT              (120) /* max time for tunning all tuners on one locator, seconds */
#endif /* USE_TRM */

#define NUM_SI_ENTRIES 2

#define USE_SEVERAL_LOCK_PROBES 1
#ifdef USE_SEVERAL_LOCK_PROBES
#define LOCK_LOOP_TIME 200 /* in [ms] but less than 1s */
#define LOCK_LOOP_COUNT 15 /* gives 3s */
#endif
#define TMP_DATA_LEN 128
/*****************************************************************************
 * LOCAL TYPES
 *****************************************************************************/

#ifndef USE_FRONTEND_PROCFS
typedef enum TunerStateSnmp_tag
{
    TUNER_LOCKED = 5 // 5 is 'FOUNDQAM' status
} TunerStateSnmp_t;
#endif

typedef struct TuneSession_tag
{
    int socket; /**< Main socket + 1 */
} TuneSession_t;

/*****************************************************************************
 * LOCAL FUNCTION PROTOTYPES
 *****************************************************************************/

static unsigned int getNumberOfTuners(json_t * config);
static json_t * getTuneUrls(char* tuneData, int *frequency);
static int checkQamConnection(unsigned int tunersCount, json_t **pJsonInOut);
static int getTuneData(json_t *pJson, char *tuneData, size_t size);
static int verifyStandbyState();
static int saveDecoderTuneResults(VideoDecoder_t* hvd, int num_hvd, ParserBand_t* parserBand, int num_parser, PIDChannel_t* pidChannel, int num_pid, Frontend_t* frontendStatus);
static int saveTuneResults(Frontend_t* frontendStatus);
static int saveTuneFailureMsg(const char *errString);
static bool getBER(char* ber, size_t size, int frontend);
static void freeDecoderTransportData(VideoDecoder_t* hvd, ParserBand_t* parserBand, PIDChannel_t* pidChannel);

static bool startTuneSessions(void * instanceHandle,
        TuneSession_t * sessions,
        size_t sessionCount,
        const char * url,
        unsigned int currentPass,
        unsigned int totalPasses,
        WA_DIAG_TUNER_TunerStatus_t * statuses);

static bool closeTuneSessions(TuneSession_t * sessions, size_t sessionCount);

void SendSessionProgress(void * instanceHandle,
        unsigned int sessionsDone,
        unsigned int sessionCount,
        unsigned int passesDone,
        unsigned int passesCount);

#ifdef USE_RMF
static void WaitSessions(void* instanceHandle,
        TuneSession_t * sessions,
        size_t sessionCount,
        unsigned int currentPass,
        unsigned int totalPasses,
        WA_DIAG_TUNER_TunerStatus_t * statuses,
        size_t statusCount);

static int IssueRequest(uint32_t sourceIP, uint32_t serverIP, unsigned short int serverPort, const char *request);

static uint32_t FindNonexistentIf(uint32_t lastIP);
#endif /* USE_RMF */

/*****************************************************************************
 * LOCAL VARIABLE DECLARATIONS
 *****************************************************************************/

#ifdef USE_RMF
 static const unsigned int defaultStreamerPort = 8080; // in host byte order

/* IP Address 127.0.0.1 is reserved, use 127.0.0.2 */
 static const uint32_t firstLocalIP = 0x200007f; // in network byte order

 static const bool tuneAllAtOnce = true;
#endif /* USE_RMF */

 /*****************************************************************************
  * FUNCTION DEFINITIONS
  *****************************************************************************/

 /*****************************************************************************
  * LOCAL FUNCTIONS
  *****************************************************************************/

 static json_t * getTuneUrls(char* tuneData, int *frequency)
 {
     json_t * result;
     WA_UTILS_SICACHE_Entries_t *pEntries=NULL;
     int status, i, lucky, entries=0;
     char request[REQUEST_MAX_LENGTH];

     result = json_array();

     WA_UTILS_SICACHE_TuningSetTuneData(tuneData);

     lucky = WA_UTILS_SICACHE_TuningGetLuckyId();

     status = WA_UTILS_SICACHE_TuningRead(lucky >= NUM_SI_ENTRIES ? lucky+1 : NUM_SI_ENTRIES, &pEntries);

     if((lucky >= 0) && (status > lucky))
     {
         snprintf(request, REQUEST_MAX_LENGTH, REQUEST_PATTERN, pEntries[lucky].freq, pEntries[lucky].mod, pEntries[lucky].prog);
         WA_DBG("getTuneUrls(): lucky[%d], %s\n", lucky, request);
         json_array_append_new(result, json_string(request));
         *frequency = pEntries[lucky].freq;
         ++entries;
     }

     for(i=0; (entries < NUM_SI_ENTRIES) && (i < status); ++i)
     {
         if(WA_OSA_TaskCheckQuit())
         {
             WA_DBG("getTuneUrls(): test cancelled\n");
             break;
         }
         if(i != lucky)
         {
             snprintf(request, REQUEST_MAX_LENGTH, REQUEST_PATTERN, pEntries[i].freq, pEntries[i].mod, pEntries[i].prog);
             WA_DBG("getTuneUrls(): url[%d], %s\n", i, request);
             json_array_append_new(result, json_string(request));
             *frequency = pEntries[i].freq;
             ++entries;
         }
     }

     free(pEntries);
     return result;
 }

 static unsigned int getNumberOfTuners(json_t * config)
 {
     int result = 0;

     // Use value from test configuration, if provided
     if (config && (json_unpack(config, "{si}", "tuners_count", &result) == 0))
     {
         if (result < 0)
         {
             WA_ERROR("getNumberOfTuners(): Invalid tuners_count configuration (non-negative integer expected)\n");
             return 0;
         }
         WA_DBG("getNumberOfTuners(): Tuner count from test config: %i\n", result);
         return result;
     }

     if (config && (json_object_get(config, "tuners_count") != NULL))
     {
         WA_ERROR("getNumberOfTuners(): Invalid tune_count configuration (integer expected)\n");
         return 0;
     }

     // If number of tuners not provided in test config, check RMF configuration

     {
         unsigned int u;
         if (WA_UTILS_RMF_GetNumberOfTuners(&u))
         {
             WA_DBG("getNumberOfTuners(): Tuner count from RMF config: %u\n", u);
             return u;
         }
     }

     // If not provided in test config and not present in RMF config,
     // assume that there are no tuners to be tested.
     return 0;
 }

static int checkQamConnection(unsigned int tunersCount, json_t **pJsonInOut)
{
    char oid[BUFFER_LEN];
    float avg_ChPwr = 0.0;

    WA_ENTER("Enter func=%s line=%d tunersCount = %d\n", __FUNCTION__, __LINE__, tunersCount);

    for (int tunerIndex = 0; tunerIndex < tunersCount; tunerIndex++)
    {
        if(WA_OSA_TaskCheckQuit())
        {
            WA_DBG("checkQamConnection(): test cancelled\n");
            return WA_DIAG_ERRCODE_CANCELLED;
        }

        /* Get QAM tuner state */
        int k = snprintf(oid, sizeof(oid), "%s.%i", OID_TUNER_STATE, tunerIndex + 1);
        if ((k >= sizeof(oid)) || (k < 0))
        {
            WA_ERROR("checkQamConnection(): Unable to generate OID for QAM tuner.\n");
            return WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR;
        }

        WA_UTILS_SNMP_Resp_t tunerResp;
        tunerResp.type = WA_UTILS_SNMP_RESP_TYPE_LONG;
        tunerResp.data.l = 0;
        if (!WA_UTILS_SNMP_GetNumber(SNMP_SERVER_ESTB, oid, &tunerResp, WA_UTILS_SNMP_REQ_TYPE_GET))
        {
            WA_ERROR("checkQamConnection(): Tuner %i failed to get QAM status\n", (int)tunerIndex);
            return WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR;
        }

        WA_INFO("checkQamConnection(): Tuner %i QAM state %i\n", (int)tunerIndex, (int)tunerResp.data.l);

        if(tunerResp.data.l == TUNER_LOCKED) // if one tuner is locked, we assume that tuners are fine and returning success
        {
           WA_INFO("checkQamConnection(): Tuners locked\n");
           *pJsonInOut = json_string("Tuners good.");
           return WA_DIAG_ERRCODE_SUCCESS;
        }

       /* Get QAM Down Stream Signal */
       k = snprintf(oid, sizeof(oid), "%s.%i", OID_TUNER_POWER, tunerIndex + 1);
       if ((k >= sizeof(oid)) || (k < 0))
       {
           WA_ERROR("checkQamConnection(): Unable to generate OID for QAM tuner.\n");
           return WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR;
       }

       tunerResp.type = WA_UTILS_SNMP_RESP_TYPE_LONG;
       tunerResp.data.l = 0;
       if (!WA_UTILS_SNMP_GetNumber(SNMP_SERVER_ESTB, oid, &tunerResp, WA_UTILS_SNMP_REQ_TYPE_GET))
       {
           WA_ERROR("checkQamConnection(): Tuner %i failed to get QAM Down Stream Signal\n", (int )tunerIndex);
           return WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR;
       }

       WA_INFO("checkQamConnection(): Tuner %i QAM_ChPwr %i\n", (int)tunerIndex, (int)tunerResp.data.l);
       avg_ChPwr = avg_ChPwr + (float) tunerResp.data.l;
   }

   avg_ChPwr = avg_ChPwr / tunersCount;
   avg_ChPwr = avg_ChPwr / 10; //TenthdBmV
   WA_DBG("func=%s line=%d, avg_ChPwr = %f\n", __FUNCTION__, __LINE__, avg_ChPwr);

   if (avg_ChPwr == QAM_CH_PWR_ZERO || avg_ChPwr < QAM_CH_PWR_ERROR)
   {
       WA_DBG("func=%s line=%d, Tuners not locked. Check cable; avg_ChPwr = %f\n", __FUNCTION__, __LINE__, avg_ChPwr);
       *pJsonInOut = json_string("No tuners locked. Check cable");
       return WA_DIAG_ERRCODE_TUNER_NO_LOCK;
   }

    WA_RETURN(" Exit func=%s line=%d\n", __FUNCTION__, __LINE__);
    return WA_DIAG_ERRCODE_FAILURE;
}

static int getTuneData(json_t *pJson, char *tuneData, size_t size)
{
    char* tuneValue = NULL;
    char* freq = NULL;
    char* prog = NULL;

    memset(tuneData, 0, size);

    if (pJson)
    {
        if (!json_unpack(pJson, "{ss}", "SRCID", &tuneValue))
        {
            snprintf(tuneData, size, "SRCID#%s", tuneValue);
            WA_DIAG_SetWriteTestResult(false);
        }
        else if (!json_unpack(pJson, "{ss}", "VCN", &tuneValue))
        {
            snprintf(tuneData, size, "VCN#%s", tuneValue);
            WA_DIAG_SetWriteTestResult(false);
        }
        else if (!json_unpack(pJson, "{ss}", "FREQ_PROG", &tuneValue))
        {
            if (strchr(tuneValue, ',') == NULL)
            {
                snprintf(tuneData, size, "Freq[%s]", tuneValue);
            }
            else
            {
                freq = strtok(tuneValue, ",");
                if (strcmp(freq, ""))
                {
                    prog = strtok(NULL, ",");
                }
                snprintf(tuneData, size, "Freq[%s]-Mode[0016]-Prog[%s]", freq, prog);
            }
            WA_DIAG_SetWriteTestResult(false);
        }
    }

    return 0;
}

#ifdef USE_RMF
/* About:
 * 1. The rmfstreamer does not stream multiple streams to clients at the same IP.
 * 2. The quamsrc will reuse the same tuner if the requested tuning string is already in use.
 * 2a. Only 'ocap://' prefix is compiled in, the 'tune://' is not.
 * 3. The rmfstreamer accepts multiple tune requests but they are handled one-by-one,
 *    thus next tuner is tuned only after previous tuner finished this process.
 *
 *
 * How to handle above:
 * 1. Since there is only one agent and it runs on local machine, multiple requests must be
 *    issued with different source IP each.
 * 2. Every tuning request must be different. The same frequency/modulation/sr is allowed by the
 *    physical string must differ by adding extra parameters.
 * 2a. Different 'ocap://' must be used for each request.
 * 3. Prior to read proper tuner status for all tuners, code must wait for responses from all tuners.
 *
 */

 static bool startTuneSessions(void * instanceHandle,
    TuneSession_t * sessions,
    size_t sessionCount,
    const char * urlBase,
    unsigned int currentPass,
    unsigned int totalPasses,
    WA_DIAG_TUNER_TunerStatus_t * statuses)
 {
     uint32_t localIP = firstLocalIP;
     uint32_t serverIP;
     struct timeval timeNow;

     bool result = false;

     serverIP = inet_addr(WA_UTILS_RMF_MEDIASTREAMER_IP);

     closeTuneSessions(sessions, sessionCount);

     if (sessionCount == 0)
     {
         goto end;
     }

     WA_INFO("startTuneSessions(): url base: %s\n", (char*)urlBase);

     size_t urlBaseLength = strlen(urlBase);
     size_t urlSuffixMaxLength = 50;
     char * url = malloc(urlBaseLength + urlSuffixMaxLength + 1);
     if (!url)
     {
         goto end;
     }

     strncpy(url, urlBase, urlBaseLength);
     char * urlSuffix = url + urlBaseLength;

     result = true;
     for (size_t sessionIndex = 0; sessionIndex < sessionCount; ++sessionIndex)
     {

         if(WA_OSA_TaskCheckQuit())
         {
             WA_DBG("startTuneSessions(): test cancelled #1\n");
             result = false;
             goto end_free_url;
         }

         gettimeofday(&timeNow, NULL);
         int k = snprintf(urlSuffix, urlSuffixMaxLength, "_%u%u", (unsigned int)timeNow.tv_sec, (unsigned int)timeNow.tv_usec);
         if ((k >= urlSuffixMaxLength) || ( k < 0))
         {
             WA_ERROR("startTuneSessions(): Unable to generate URL for tuner.\n");
             result = false;
             goto end_free_url;
         }

         localIP = FindNonexistentIf(localIP);

         WA_INFO("startTuneSessions(): session %i, fromIP:%08x url base: %s\n", (int)sessionIndex, localIP, (char*)url);

         //at first use 127.0.0.1 so it can be reused by av_play immediately after tuners
         sessions[sessionIndex].socket = IssueRequest(localIP, serverIP, htons(WA_UTILS_RMF_GetMediastreamerPort()), url) + 1;
         if(sessions[sessionIndex].socket < 0)
         {
             result = false;
         }

         if(WA_OSA_TaskCheckQuit())
         {
             WA_DBG("startTuneSessions(): test cancelled #2\n");
             result = false;
             goto end_free_url;
         }

         WA_DBG("startTuneSessions(): Returned socket fd: %i\n", (int)sessions[sessionIndex].socket);

         if (!tuneAllAtOnce)
         {
             if (sessions[sessionIndex].socket > 0)
             {
                 WaitSessions(instanceHandle,
                     sessions + sessionIndex,
                     1,
                     sessionIndex + currentPass * sessionCount,
                     totalPasses * sessionCount,
                     statuses,
                     sessionCount);

                 if(WA_OSA_TaskCheckQuit())
                 {
                     WA_DBG("startTuneSessions(): test cancelled #3\n");
                     result = false;
                     goto end_free_url;
                 }
             }
         }

         if(WA_OSA_TaskCheckQuit())
         {
             WA_DBG("startTuneSessions(): test cancelled #4\n");
             result = false;
             goto end_free_url;
         }
     }

     if (tuneAllAtOnce)
     {
         WaitSessions(instanceHandle, sessions, sessionCount, currentPass, totalPasses, statuses, sessionCount);
     }

     end_free_url:
     free(url);

     end:
     return result;
 }


 static bool closeTuneSessions(TuneSession_t * sessions, size_t sessionCount)
 {
     for (size_t sessionIndex = 0; sessionIndex < sessionCount; ++sessionIndex)
     {
         if(WA_OSA_TaskCheckQuit())
         {
             WA_DBG("closeTuneSessions: test cancelled\n");
             return false;
         }

         int socket = sessions[sessionIndex].socket - 1;
         WA_DBG("closeTuneSessions: TUNER[%d]:active:%d\n", sessionIndex, socket >= 0);
         if (socket >= 0)
         {
             shutdown(socket, SHUT_RDWR);
             close(socket);
             sessions[sessionIndex].socket = 0;
         }
     }
     return true;
 }

#if !WA_DEBUG
#define dump(x, y)
#else
 static void dump(const char * data, size_t count)
 {
     size_t i, j;

     for (i = 0; i < count; i += 16)
     {
         printf("HWST_DBG |");

         for (j = 0; j < 16; ++j)
         {
             if (i + j < count)
             {
                 printf("%02X ", data[i + j]);
             }
             else
             {
                 printf("   ");
             }
         }

         for (j = 0; j < 16; ++j)
         {
             if (i + j < count)
             {
                 char c = data[i + j];
                 if ((c >= 32) && (c != 127))
                 {
                     printf("%c", data[i + j]);
                 }
                 else
                 {
                     printf(".");
                 }
             }
             else
             {
                 printf(" ");
             }
         }
         printf("\n");
     }
 }
#endif

 static void WaitSessions(void* instanceHandle,
    TuneSession_t * sessions,
    size_t sessionCount,
    unsigned int currentPass,
    unsigned int totalPasses,
    WA_DIAG_TUNER_TunerStatus_t * statuses,
    size_t statusCount)
 {
     int i, k, status;
     struct pollfd *pfds;
     int timeout;
     struct timeval timeOfDayStart, timeOfDay;
     int numLocked;
     bool freqLocked = false;

     WA_INFO("WaitSessions(): %p\n", (void*)sessions);

     pfds = malloc(sizeof(struct pollfd) * sessionCount);
     if(pfds == NULL)
         return;

     for(i = 0, k=0; i < sessionCount; ++i)
     {

         if(WA_OSA_TaskCheckQuit())
         {
             WA_DBG("WaitSessions(): test cancelled #1\n");
             goto end;
         }

         if(sessions[i].socket <= 0)
         {
             ++k;
         }
         pfds[i].fd = sessions[i].socket - 1;
         pfds[i].events = POLLIN;
         //printf("HWST_DBG |Socket[%d]: %d\n", i, pfds[i].fd);
     }
     if(k == sessionCount)
     {
         WA_ERROR("WaitSessions(): No sockets\n");
         goto end;
     }

     if (tuneAllAtOnce)
     {
         if(k > 0)
         {
             SendSessionProgress(instanceHandle, k, sessionCount, currentPass, totalPasses);
         }
     }

     timeout = TUNE_RESPONSE_TIMEOUT*sessionCount;
     gettimeofday(&timeOfDayStart, NULL);
     do
     {
         if(WA_OSA_TaskCheckQuit())
         {
             WA_DBG("WaitSessions(): test cancelled #2\n");
             goto end;
         }

         status = poll(pfds, sessionCount, timeout);
         switch (status)
         {
         case -1:
             WA_ERROR("WaitSessions(): poll() error\n");
             goto end;
         case 0:
             WA_ERROR("WaitSessions(): poll() timeout\n");
             goto end;
         default:
             for(i=0, k=0; i<sessionCount; ++i)
             {
                 if(pfds[i].fd < 0)
                 {
                     ++k;
                     continue;
                 }
                 WA_DBG("WaitSessions(): event[%d]: %x\n", i, pfds[i].revents);
                 if(pfds[i].revents != 0)
                 {
                     {
                         char buf[MAX_REQUEST];
                         int rc = recv(pfds[i].fd, buf , MAX_REQUEST-1, 0);
                         WA_DBG("WaitSessions(): recv result: %i (errno: %i: %s)\n", (int)rc, (int)errno, (char*)strerror(errno));
                         fflush(stdout);
                         if(rc > 0)
                         {
                             dump(buf, rc);
                         }
                         fflush(stdout);
                         /* ignore response content */
                     }
                     pfds[i].fd = -1;
                     ++k;
                 }
                 /* ignore response content */
             }
             if (tuneAllAtOnce)
             {
                 if(k > 0)
                 {
                     SendSessionProgress(instanceHandle, k, sessionCount, currentPass, totalPasses);
                 }
             }
             if(k == sessionCount)
             {
                 WA_DBG("WaitSessions(): all events served\n");
                 goto end;
             }
             WA_DBG("WaitSessions(): Remaining tuners: %d\n", sessionCount-k);
             /* ignore response content */
             break;
         }

         gettimeofday(&timeOfDay, NULL);
         timeout = (TUNE_RESPONSE_TIMEOUT*sessionCount) - (timeOfDay.tv_sec - timeOfDayStart.tv_sec)*1000;

         WA_DIAG_TUNER_GetTunerStatuses(statuses, sessionCount, &numLocked, &freqLocked, "", NULL);
     }while((status > 0) && (timeout > 0));

     end:

     free(pfds);
     return;
 }

 /**
  * @param sourceIP local IP address to bind to, in network byte order
  * @param serverIP remote IP address to connect to, in network byte order
  * @param serverPort remote TCP port to connect to, in network byte order
  */
 static int IssueRequest(uint32_t sourceIP, uint32_t serverIP, unsigned short int serverPort, const char *request)
 {
     int s, on=1;
     int status = -1;
     struct sockaddr_in address;
     char buf[MAX_REQUEST];
     struct in_addr addr;
     struct pollfd pfd;

     WA_INFO("IssueRequest(): %s\n", (char*) request);

     if((s = socket(PF_INET,SOCK_STREAM, IPPROTO_TCP)) < 0)
     {
         WA_ERROR("IssueRequest(): socket() error: %i (%s)\n", (int)errno, strerror(errno));
         return -1;
     }

     if(WA_OSA_TaskCheckQuit())
     {
         WA_DBG("IssueRequest(): test cancelled #1\n");
         goto err;
     }

     if(setsockopt(s, SOL_SOCKET,  SO_REUSEADDR, (char *)&on, sizeof(on)) < 0)
     {
         WA_ERROR("IssueRequest(): setsockopt(SO_REUSEADDR) error: %i (%s)\n", (int)errno, strerror(errno));
         goto err;
     }

     if(WA_OSA_TaskCheckQuit())
     {
         WA_DBG("IssueRequest(): test cancelled #2\n");
         goto err;
     }

     if(setsockopt(s, SOL_IP, IP_TRANSPARENT, &on, sizeof (on)) < 0)
     {
         WA_ERROR("IssueRequest(): setsockopt(IP_TRANSPARENT) error\n");
         goto err;
     }

     if(WA_OSA_TaskCheckQuit())
     {
         WA_DBG("IssueRequest(): test cancelled #3\n");
         goto err;
     }

     address.sin_family = AF_INET;
     address.sin_addr.s_addr = sourceIP;
     address.sin_port = 0;
     if((bind(s,(struct sockaddr *)&address, sizeof(struct sockaddr_in))) < 0)
     {
         WA_ERROR("IssueRequest(): bind() error %i (%s)\n", (int)errno, strerror(errno));
         goto err;
     }

     if(WA_OSA_TaskCheckQuit())
     {
         WA_DBG("IssueRequest(): test cancelled #4\n");
         goto err;
     }

     address.sin_family = AF_INET;
     address.sin_addr.s_addr = serverIP;
     address.sin_port = serverPort;
     if (connect(s, (struct sockaddr *)&address , sizeof(address)) < 0)
     {
         WA_ERROR("IssueRequest(): connect() error: %i (%s)\n", (int)errno, strerror(errno));
         goto err;
     }

     if(WA_OSA_TaskCheckQuit())
     {
         WA_DBG("IssueRequest(): test cancelled #5\n");
         goto err;
     }

     addr.s_addr = serverIP;
     snprintf(buf, MAX_REQUEST, "GET %s HTTP/1.1\r\nHost: %s:%d\r\n\r\n",
             request,
             inet_ntoa(addr),
             ntohs(serverPort));
     WA_DBG("IssueRequest(): sending: [%s]\n", buf);

     if (send(s, buf,  strlen(buf), 0) < 0)
     {
         WA_ERROR("IssueRequest(): send() error: %i (%s)\n", (int)errno, strerror(errno));
         goto err;
     }

     if(WA_OSA_TaskCheckQuit())
     {
         WA_DBG("IssueRequest(): test cancelled #5\n");
         goto err;
     }

     /* wait for first response, expected 'HTML OK' */
     pfd.fd = s;
     pfd.events = POLLIN;
     status = poll(&pfd, 1, HTML_RESPONSE_TIMEOUT);
     switch (status)
     {
     case -1:
         WA_ERROR("IssueRequest(): poll() error: %i (%s)\n", (int)errno, strerror(errno));
         goto err;
     case 0:
         WA_ERROR("IssueRequest(): poll() timeout\n");
         status = 1;
         goto err;
     default:
     {
         int i = recv(s , buf , MAX_REQUEST-1, 0);
         WA_DBG("IssueRequest(): recv result: %i\n", (int)i);
         if(i > 0)
         {
             dump(buf, i);
         }
         /* ignore response content */
         goto end;
     }
     }

     err:
     close(s);
     s = -1;

     end:
     return s;
 }

 static uint32_t FindNonexistentIf(uint32_t lastIP)
 {
     int status;
     struct ifaddrs *ifas, *ifa;
     struct sockaddr_in *saIf;
     uint32_t ifIP = 0;
     uint32_t ip = ntohl(lastIP);

     status = getifaddrs(&ifas);
     if(status != 0)
     {
         WA_ERROR("getifaddrs() error: %i (%s)\n", (int)errno, strerror(errno));
         return 0;
     }

     for(++ip; ip < 0x7f0000FF; ++ip)
     {
         for(ifa = ifas; ifa; ifa=ifa->ifa_next)
         {
             if(WA_OSA_TaskCheckQuit())
             {
                 WA_DBG("FindNonexistentIf: test cancelled\n");
                 return 0;
             }

             if(ifa->ifa_addr && (ifa->ifa_addr->sa_family == AF_INET))
             {
                 saIf = (struct sockaddr_in *)ifa->ifa_addr;
                 ifIP = ntohl(saIf->sin_addr.s_addr);
                 if(ifIP == ip)
                 {
                     break;
                 }
             }
         }
         if(ifIP != ip)
         {
             goto end;
         }
     }
     ip = 0;
     end:
     freeifaddrs(ifas);
     return htonl(ip);
 }
#endif /* USE_RMF */

 void SendSessionProgress(void * instanceHandle,
         unsigned int sessionsDone,
         unsigned int sessionCount,
         unsigned int passesDone,
         unsigned int passesCount)
 {
     static const int max_progress = 90;

     unsigned int value = (sessionsDone * max_progress / sessionCount + passesDone * max_progress) / passesCount;

     //printf("HWST_DBG |Sending progress %i (%i, %i, %i, %i)\n", value, sessionsDone, sessionCount, passesDone, passesCount);

     WA_DIAG_SendProgress(instanceHandle, value);
 }

#ifdef USE_TRM
static bool startTuneSessions(void *instanceHandle,
    TuneSession_t * sessions,
    size_t sessionCount,
    const char *urlBase,
    unsigned int currentPass,
    unsigned int totalPasses,
    WA_DIAG_TUNER_TunerStatus_t *statuses)
{
    (void)statuses;
    bool result = false;

    WA_ENTER("startTuneSessions(instanceHandle=%p, sessions=%p, sessionCount=%u, urlBase='%s', currentPass=%u, totalPasses=%u, statuses=%p)\n",
                instanceHandle, sessions, sessionCount, urlBase, currentPass, totalPasses, statuses);

    if (!closeTuneSessions(sessions, sessionCount))
        WA_ERROR("startTuneSessions(): closeTuneSessions() failed\n");

    /* Unlikely, but might lengthen the url by 1 character when adjusting the frequency. */
    size_t max_url_len = strlen(urlBase) + 1 /* nul */ + 1;
    char *adj_url = malloc(max_url_len);
    if (!adj_url)
        goto end;

    int freq = 0, mod = 0, pgmo = 0;
    if (sscanf(urlBase, REQUEST_PATTERN, &freq, &mod, &pgmo) != 3)
    {
        WA_ERROR("startTuneSessions(): failed to parse URL\n");
        goto end;
    }

    time_t start_time = time(NULL);

    result = true;
    for (int i = 0; i < sessionCount; i++)
    {
        if (time(NULL) - start_time > TRM_TUNER_SESSIONS_TIMEOUT)
        {
            WA_ERROR("startTuneSessions(): sessions timeout!\n");
            WA_OSA_TaskSignalQuit(WA_OSA_TaskGet());
        }

        if (WA_OSA_TaskCheckQuit())
        {
            WA_DBG("startTuneSessions(): test cancelled #1\n");
            result = false;
            break;
        }

        /* For each reserve request adjust frequency slightly, so it's different to TRM, but close enough to still be lockable. */
        snprintf(adj_url, max_url_len, REQUEST_PATTERN, freq + i, mod, pgmo);
        if (!WA_UTILS_TRH_ReserveTuner(strstr(adj_url, "ocap://"), 0 /* now */, TRM_TUNER_RESERVATION_TIME, TRM_TUNER_RESERVATION_TIMEOUT, (int)sessionCount, (void **)&sessions[i].socket))
        {
            if (WA_OSA_TaskCheckQuit())
            {
                WA_DBG("startTuneSessions(): test cancelled #2\n");
                result = false;
                break;
            }

            if (WA_UTILS_TRH_WaitForTunerRelease((void *)sessions[i].socket, (2 * TRM_TUNER_RESERVATION_TIME)))
            {
                WA_ERROR("startTuneSessions(): tuner release timed out\n");
                result = false;
            }
            else
                WA_DBG("startTuneSessions(): tuner from reservation request %i successfully released\n", i);
        }
        else
        {
            WA_ERROR("startTuneSessions(): failed to issue reservation request %i\n", i);
            result = false;
        }

        SendSessionProgress(instanceHandle, i, sessionCount, currentPass, totalPasses);
    }

end:
    free(adj_url);

    WA_RETURN("startTuneSessions(): %d\n", result);

    return result;
}

static bool closeTuneSessions(TuneSession_t *sessions, size_t sessionCount)
{
    bool result = true;

    WA_ENTER("closeTuneSessions(sessions=%p, sessionCount=%u)\n", sessions, sessionCount);

    for (int i = 0; i < sessionCount; i++)
    {
        if (sessions[i].socket)
        {
            if (WA_UTILS_TRH_ReleaseTuner((void *)sessions[i].socket, TRM_TUNER_RESERVATION_RELEASE_TIMEOUT))
            {
               WA_ERROR("closeTuneSessions(): failed to free reservation %i\n", i);
               result = false; /* but continue anyway */
            }

            sessions[i].socket = 0;
        }
    }

    WA_RETURN("closeTuneSessions(): %d\n", result);

    return result;
}
#endif /* USE_TRM */

 /*****************************************************************************
  * EXPORTED FUNCTIONS
  *****************************************************************************/
bool WA_DIAG_TUNER_GetDocsisParams(WA_DIAG_TUNER_DocsisParams_t * params)
{
    char oid[BUFFER_LEN];
    WA_ENTER("Enter func=%s line=%d\n", __FUNCTION__, __LINE__);
    float flVar = 0.0;

    if(WA_OSA_TaskCheckQuit())
    {
        WA_DBG("GetDocsisParams: test cancelled\n");
        return false;
    }

    WA_UTILS_SNMP_Resp_t tunerResp;

    /* Get DOCSIS Down Stream Signal */
    int k = snprintf(oid, sizeof(oid), "%s.%i", OID_DOCSIS_DOWNSTREAMPOWER, 3 /* As per generic/diagnostics/generic/QAM_device/www/htmldiag/system_docsis.html */);
    if ((k >= sizeof(oid)) || ( k < 0))
    {
        WA_ERROR("Error Unable to generate OID for DOCSIS DwnStrmPwr.\n");
        return false;
    }

    tunerResp.type = WA_UTILS_SNMP_RESP_TYPE_LONG;
    tunerResp.data.l = 0;
    if (!WA_UTILS_SNMP_GetNumber(SNMP_SERVER_ECM, oid, &tunerResp, WA_UTILS_SNMP_REQ_TYPE_GET))
    {
        WA_ERROR("Error Tuner failed to get DOCSIS Down Stream Signal\n");
    }

    flVar = (float)tunerResp.data.l;
    snprintf(params->DOCSIS_DwStreamChPwr, BUFFER_LEN,"%.1f dBmV", flVar/10); //TenthdBmV
    WA_DBG("func=%s line=%d, DwStrChPwr=%s\n", __FUNCTION__, __LINE__, params->DOCSIS_DwStreamChPwr);

    /* Get DOCSIS Up Stream Signal */
    k = snprintf(oid, sizeof(oid), "%s.%i", OID_DOCSIS_UPSTREAMPOWER, 2 /* As per generic/diagnostics/generic/QAM_device/www/htmldiag/system_docsis.html */);
    if ((k >= sizeof(oid)) || ( k < 0))
    {
        WA_ERROR("Error Unable to generate OID for DOCSIS UpStrmPwr.\n");
        return false;
    }

    tunerResp.type = WA_UTILS_SNMP_RESP_TYPE_LONG;
    tunerResp.data.l = 0;
    if (!WA_UTILS_SNMP_GetNumber(SNMP_SERVER_ECM, oid, &tunerResp, WA_UTILS_SNMP_REQ_TYPE_GET))
    {
        WA_ERROR("Error Tuner failed to get DOCSIS Up Stream Signal\n");
    }

    flVar = (float)tunerResp.data.l;
    snprintf(params->DOCSIS_UpStreamChPwr, BUFFER_LEN,"%.1f dBmV", flVar/10); //TenthdBmV
    WA_DBG("func=%s line=%d, UpStrChPwr=%s\n", __FUNCTION__, __LINE__, params->DOCSIS_UpStreamChPwr);
    /* Get DOCSIS SNR */
    k = snprintf(oid, sizeof(oid), "%s.%i", OID_DOCSIS_SNR, 3 /* as per generic/diagnostics/generic/QAM_device/www/htmldiag/system_docsis.html */);
    if ((k >= sizeof(oid)) || ( k < 0))
    {
        WA_ERROR("Error Unable to generate OID for DOCSIS SNR.\n");
        return false;
    }

    tunerResp.type = WA_UTILS_SNMP_RESP_TYPE_LONG;
    tunerResp.data.l = 0;
    if (!WA_UTILS_SNMP_GetNumber(SNMP_SERVER_ECM, oid, &tunerResp, WA_UTILS_SNMP_REQ_TYPE_GET))
    {
        WA_ERROR("Error Tuner failed to get DOCSIS SNR\n");
    }

    flVar = (float)tunerResp.data.l;
    snprintf(params->DOCSIS_SNR, BUFFER_LEN,"%.1f dB", flVar/10); //TenthdB
    WA_RETURN("Exit func=%s line=%d, SNR=%s\n", __FUNCTION__, __LINE__, params->DOCSIS_SNR);

    return true;
}

bool WA_DIAG_TUNER_GetQamParams(WA_DIAG_TUNER_QamParams_t * params)
{
    char oid[BUFFER_LEN];
    WA_ENTER("Enter func=%s line=%d\n", __FUNCTION__, __LINE__);

    unsigned int numTuners;
    if (!WA_UTILS_RMF_GetNumberOfTuners(&numTuners))
    {
        WA_ERROR("func=%s line=%d numTuners = %d\n", __FUNCTION__, __LINE__, numTuners);
        return false;
    }

    WA_INFO("func=%s line=%d numTuners = %d\n", __FUNCTION__, __LINE__, numTuners);
    int LockedCount = 0;
    bool Locked[numTuners];
    float QAM_ChPwr[numTuners], min_ChPwr = 0.0, avg_ChPwr = 0.0, max_ChPwr = 0.0;
    float QAM_SNR[numTuners], min_SNR = 0.0, avg_SNR = 0.0, max_SNR = 0.0;

    char tempPWR[TMP_DATA_LEN] = {'\0'};
    char tempSNR[TMP_DATA_LEN] = {'\0'};

    for (size_t tunerIndex = 0; tunerIndex < numTuners; tunerIndex++)
    {
        if(WA_OSA_TaskCheckQuit())
        {
            WA_DBG("GetQamParams: test cancelled\n");
            return false;
        }

        int k = snprintf(oid, sizeof(oid), "%s.%i", OID_TUNER_STATE, tunerIndex + 1);
        if ((k >= sizeof(oid)) || ( k < 0))
        {
            WA_ERROR("Unable to generate OID for QAM tuner.\n");
            return false;
        }

        WA_UTILS_SNMP_Resp_t tunerResp;
        tunerResp.type = WA_UTILS_SNMP_RESP_TYPE_LONG;
        tunerResp.data.l = 0;
        if (!WA_UTILS_SNMP_GetNumber(SNMP_SERVER_ESTB, oid, &tunerResp, WA_UTILS_SNMP_REQ_TYPE_GET))
        {
            WA_ERROR("Tuner %i failed to get QAM status\n", (int)tunerIndex);
            return false;
        }

        WA_INFO("Tuner %i QAM state %i\n", (int)tunerIndex, (int)tunerResp.data.l);
        if(tunerResp.data.l == TUNER_LOCKED)
        {
            Locked[tunerIndex] = true;
            LockedCount++;

            WA_DBG("func=%s line=%d, QAM_locked[%d]=%i\n", __FUNCTION__, __LINE__, tunerIndex, Locked[tunerIndex]);

            /* Get QAM Down Stream Signal */
            k = snprintf(oid, sizeof(oid), "%s.%i", OID_TUNER_POWER, tunerIndex + 1);
            if ((k >= sizeof(oid)) || ( k < 0))
            {
                WA_ERROR("Unable to generate OID for QAM tuner.\n");
                return false;
            }

            tunerResp.type = WA_UTILS_SNMP_RESP_TYPE_LONG;
            tunerResp.data.l = 0;
            if (!WA_UTILS_SNMP_GetNumber(SNMP_SERVER_ESTB, oid, &tunerResp, WA_UTILS_SNMP_REQ_TYPE_GET))
            {
                WA_ERROR("Tuner %i failed to get QAM Down Stream Signal\n", (int)tunerIndex);
                return false;
            }

            WA_INFO("Tuner %i QAM_ChPwr %i\n", (int)tunerIndex, (int)tunerResp.data.l);
            QAM_ChPwr[tunerIndex] = (float)tunerResp.data.l;
            WA_DBG("func=%s line=%d, QAM_ChPwr=%f\n", __FUNCTION__, __LINE__, QAM_ChPwr[tunerIndex]);

            /* Get QAM SNR */
            k = snprintf(oid, sizeof(oid), "%s.%i", OID_TUNER_SNR, tunerIndex + 1);
            if ((k >= sizeof(oid)) || ( k < 0))
            {
                WA_ERROR("Unable to generate OID for QAM tuner.\n");
                return false;
            }

            tunerResp.type = WA_UTILS_SNMP_RESP_TYPE_LONG;
            tunerResp.data.l = 0;
            if (!WA_UTILS_SNMP_GetNumber(SNMP_SERVER_ESTB, oid, &tunerResp, WA_UTILS_SNMP_REQ_TYPE_GET))
            {
                WA_ERROR("Tuner %i failed to get QAM SNR\n", (int)tunerIndex);
                return false;
            }

            WA_INFO("Tuner %i QAM_SNR %i\n", (int)tunerIndex, (int)tunerResp.data.l);
            QAM_SNR[tunerIndex] = (float)tunerResp.data.l;
            WA_DBG("func=%s line=%d, QAM_SNR=%f\n", __FUNCTION__, __LINE__, QAM_SNR[tunerIndex]);
        }
        else
        {
            Locked[tunerIndex] = false;
        }
    }

    snprintf(params->numLocked, BUFFER_LEN,"%i of %i ", LockedCount, numTuners);

    if(LockedCount <= 3)
    {
        for(size_t tunerIndex = 0; tunerIndex < numTuners; tunerIndex++)
        {
            if(Locked[tunerIndex] == true)
            {
                snprintf(params->QAM_ChPwr, BUFFER_LEN,"%s%i: %.1f dBmV ", tempPWR, tunerIndex+1, QAM_ChPwr[tunerIndex]/10); //TenthdBmV
                strcpy(tempPWR, params->QAM_ChPwr);

                snprintf(params->QAM_SNR, BUFFER_LEN,"%s%i: %.1f dB ", tempSNR, tunerIndex+1, QAM_SNR[tunerIndex]/10); //TenthdB
                strcpy(tempSNR, params->QAM_SNR);
            }
        }
        WA_DBG("func=%s line=%d, params->QAM_ChPwr = %s params->QAM_SNR = %s \n", __FUNCTION__, __LINE__, params->QAM_ChPwr, params->QAM_SNR);
    }
    else //LockedCount > 3
    {
        min_ChPwr  = QAM_ChPwr[0]; // Load with first set of values to compare and swap for min and max values
        max_ChPwr  = QAM_ChPwr[0];
        min_SNR    = QAM_SNR[0];
        max_SNR    = QAM_SNR[0];
        avg_ChPwr  = QAM_ChPwr[0];
        avg_SNR    = QAM_SNR[0];

        for(size_t tunerIndex = 1; tunerIndex < numTuners; tunerIndex++)
        {
            if(Locked[tunerIndex] == false)
                continue;

            if(QAM_ChPwr[tunerIndex] < min_ChPwr)
            {
                min_ChPwr = QAM_ChPwr[tunerIndex];
            }
            if(QAM_ChPwr[tunerIndex] > max_ChPwr)
            {
                max_ChPwr = QAM_ChPwr[tunerIndex];
            }
            if(QAM_SNR[tunerIndex] < min_SNR)
            {
                min_SNR = QAM_SNR[tunerIndex];
            }
            if(QAM_SNR[tunerIndex] > max_SNR)
            {
                max_SNR = QAM_SNR[tunerIndex];
            }
            avg_ChPwr = avg_ChPwr + QAM_ChPwr[tunerIndex];
            avg_SNR = avg_SNR + QAM_SNR[tunerIndex];
        }
        avg_ChPwr = avg_ChPwr/numTuners;
        avg_SNR = avg_SNR/numTuners;

        snprintf(params->QAM_ChPwr, BUFFER_LEN,"MIN: %.1f, AVG: %.1f, MAX: %.1f dBmV", min_ChPwr/10, avg_ChPwr/10, max_ChPwr/10); //TenthdBmV
        snprintf(params->QAM_SNR, BUFFER_LEN,"MIN: %.1f, AVG: %.1f, MAX: %.1f dB", min_SNR/10, avg_SNR/10, max_SNR/10); //TenthdB
        WA_DBG("func=%s line=%d, params->QAM_ChPwr = %s params->QAM_SNR = %s \n", __FUNCTION__, __LINE__, params->QAM_ChPwr, params->QAM_SNR);
    }

    WA_RETURN(" Exit func=%s line=%d LockedCount=%d\n", __FUNCTION__, __LINE__, LockedCount);
    return true;
}

bool WA_DIAG_TUNER_GetVideoDecoderData(VideoDecoder_t* hvd, int* num_hvd)
{
    FILE* fd;
    int hvd_index = 0;
    char buf[LINE_LEN];
    char format[LINE_LEN];
    char result[LINE_LEN];

    typedef enum
    {
        parseHVD = 0,
        parseIdx,
        parseStarted,
        parseTSM,
        parseDecode,
        parseDisplay
    } parseMode_t;

    parseMode_t parseMode = parseHVD;

    if ((fd = fopen(VIDEO_DECODER_STATUS_FILE, "r")) == NULL)
    {
        WA_ERROR("GetVideoDecoderData(): Cannot read the file %s\n", VIDEO_DECODER_STATUS_FILE);
        return false;
    }

    while(hvd_index < NUM_DECODERS && fgets(buf, LINE_LEN, fd))
    {
        switch (parseMode)
        {
        case parseHVD:
            /* HVD0: (84709000) general: 2MB secure: 7MB picture: 25MB watchdog:0 */
            /* HVD1: (cac7c800) general: 3MB secure: 8MB picture:  0MB watchdog:0 */
            snprintf(format, LINE_LEN, "HVD%%%ds", LINE_LEN);
            if (sscanf(buf, format, result) == 1)
            {
                WA_DBG("GetVideoDecoderData(): Found HVD%c\n", result[0]);
                parseMode = parseIdx;
            }
            break;
        case parseIdx:
            /* idx0(82cf8000): videoInput=8a2aaedc max=1920x1080p60 10 bit, MFD0 */
            /* This case is to ensure if expected data is available for the HVD found */
            snprintf(format, LINE_LEN, " idx%%%ds", LINE_LEN);
            if (sscanf(buf, format, result) == 1)
            {
                WA_DBG("GetVideoDecoderData(): Found idx%c\n", result[0]);
                parseMode = parseStarted;
            }
            else
            {
                snprintf(format, LINE_LEN, "HVD%%%ds", LINE_LEN);
                if (sscanf(buf, format, result) == 1)
                {
                    WA_DBG("GetVideoDecoderData(): Found immediate HVD%c\n", result[0]); // Negative case when HVD0 has no data and HVD1 is found
                }
                else
                {
                    WA_ERROR("GetVideoDecoderData(): Unrecognized data format in %s\n", VIDEO_DECODER_STATUS_FILE);
                    parseMode = -1; // Invalid data format is identified in the file
                }
            }
            break;
        case parseStarted:
            /* started=y: codec=5, pid=0x1983, pidCh=8fe98f80, stcCh=8727c400 */
            /* started=n */
            snprintf(format, LINE_LEN, " started=%%%ds", LINE_LEN);
            if (hvd && (sscanf(buf, format, result) == 1))
            {
                WA_DBG("GetVideoDecoderData(): Found started=%c\n", result[0]);
                if (result[0] == 'y')
                {
                    hvd[hvd_index].started = true;
                    parseMode = parseTSM;
                }
                else
                {
                    hvd[hvd_index].started = false;
                    parseMode = parseHVD; // Moving to next HVD
                    hvd_index++;
                }
            }
            break;
        case parseTSM:
            /* TSM: enabled pts=0x8c2269e7 pts_stc_diff=64 pts_offset=0x4366 errors=0 */
            if (hvd && (strstr(buf, "TSM") != NULL))
            {
                if (sscanf(buf, " TSM: enabled pts=%s pts_stc_diff=%s pts_offset=%s errors=%s",
                            hvd[hvd_index].tsm_data[0],
                            hvd[hvd_index].tsm_data[1],
                            hvd[hvd_index].tsm_data[2],
                            hvd[hvd_index].tsm_data[3]) != NUM_HVD_DATA)
                {
                    WA_ERROR("GetVideoDecoderData(), parseTSM: Failed to parse %s\n", buf);
                }
                parseMode = parseDecode;
            }
            break;
        case parseDecode:
            /* Decode: decoded=1288 drops=0 errors=0 overflows=0 */
            if (hvd && (strstr(buf, "Decode") != NULL))
            {
                if (sscanf(buf, " Decode: decoded=%i drops=%i errors=%i overflows=%i",
                            &(hvd[hvd_index].decode_data[0]),
                            &(hvd[hvd_index].decode_data[1]),
                            &(hvd[hvd_index].decode_data[2]),
                            &(hvd[hvd_index].decode_data[3])) != NUM_HVD_DATA)
                {
                    WA_ERROR("GetVideoDecoderData(), parseDecode: Failed to parse %s\n", buf);
                }
                parseMode = parseDisplay;
            }
            break;
        case parseDisplay:
            /* Display: displayed=1286 drops=0 errors=0 underflows=0 */
            if (hvd && (strstr(buf, "Display") != NULL))
            {
                if (sscanf(buf, " Display: displayed=%i drops=%i errors=%i underflows=%i",
                            &(hvd[hvd_index].display_data[0]),
                            &(hvd[hvd_index].display_data[1]),
                            &(hvd[hvd_index].display_data[2]),
                            &(hvd[hvd_index].display_data[3])) != NUM_HVD_DATA)
                {
                    WA_ERROR("GetVideoDecoderData(), parseDisplay: Failed to parse %s\n", buf);
                }
                parseMode = parseHVD;
                hvd_index++;
            }
            break;
        default:
            break;
        }
    }
    fclose(fd);

    *num_hvd = hvd_index;

    for (int index = 0; index < hvd_index; index++)
    {
        if (hvd[index].started == true)
        {
            WA_INFO("GetVideoDecoderData(): %s parsed successfully\n", VIDEO_DECODER_STATUS_FILE);
            return true;
        }
    }

    WA_DBG("GetVideoDecoderData(): Decoders not started\n");
    return false;
}

bool WA_DIAG_TUNER_GetTransportData(ParserBand_t* parserBand, int* num_parser, PIDChannel_t* pidChannel, int* num_pid)
{
    FILE* fd;
    int parser_index = 0;
    int pid_index = 0;
    char buf[LINE_LEN];

    typedef enum
    {
        parseParserBand = 0,
        parsePidChannel
    } parseMode_t;

    parseMode_t parseMode = parseParserBand;

    if ((fd = fopen(TRANSPORT_STATUS_FILE, "r")) == NULL)
    {
        WA_ERROR("GetTransportData(): Cannot read the file %s\n", TRANSPORT_STATUS_FILE);
        return false;
    }

    while(fgets(buf, LINE_LEN, fd))
    {
        switch (parseMode)
        {
        case parseParserBand:
            /* parser band 0: source MTSIF 0x846a8400, enabled -, pid channels 8, cc errors 0, tei errors 0, length errors 0, RS overflows 0
             * parser band 1: source MTSIF 0x8479c900, enabled -, pid channels 5, cc errors 0, tei errors 0, length errors 0, RS overflows 0
             * . . .
             */
            if (parserBand && (strstr(buf, "parser band ") != NULL))
            {
                if (sscanf(buf, "%*[^,], %*[^,], %*[^,], cc errors %i, tei errors %i, length errors %i",
                            &(parserBand[parser_index].parser_band[0]),
                            &(parserBand[parser_index].parser_band[1]),
                            &(parserBand[parser_index].parser_band[2])) != NUM_PARSER_DATA)
                {
                    WA_ERROR("GetTransportData(), parserBand: Failed to parse %s\n", buf);
                }
                parser_index++;

                if (parser_index == NUM_PARSER_BANDS) // Maximum expected parser bands for which the malloc is done
                {
                    WA_DBG("GetTransportData(), parserBand: Max limit. Total elements: %i\n", parser_index);
                    parseMode = parsePidChannel;
                }
            }
            else
            {
                if (parser_index > 0)
                {
                    WA_DBG("GetTransportData(), parserBand: End of parsing. Total elements: %i\n", parser_index);
                    parseMode = parsePidChannel;
                }
            }
            break;
        case parsePidChannel:
            /* pidchannel 82fdf500: ch 86, parser 0, pid 0x0, 0 cc errors, 0 XC overflows */
            if (pidChannel && sscanf(buf, "pidchannel %s %*[^,], %*[^,], %*[^,], %i cc errors",
                    pidChannel[pid_index].pid_channel,
                    &(pidChannel[pid_index].cc_errors)) == NUM_PID_DATA)
            {
                int len = strlen(pidChannel[pid_index].pid_channel);
                pidChannel[pid_index].pid_channel[len-1] = '\0'; // To remove the extra ':' in the string
                pid_index++;

                if (pid_index == NUM_PID_CHANNELS) // Maximum expected pid channels for which the malloc is done
                {
                    WA_DBG("GetTransportData(), pidchannel: Max limit. Total elements: %i\n", pid_index);
                    parseMode = -1; // End of parsing
                }
            }
            else
            {
                if (pid_index > 0)
                {
                    WA_DBG("GetTransportData(), pidChannel: End of parsing. Total elements: %i\n", pid_index);
                    parseMode = -1; // End of parsing
                }
            }
            break;
        default:
            break;
        }
    }
    fclose(fd);

    *num_parser = parser_index;
    *num_pid = pid_index;

    if (*num_parser == 0 && *num_pid == 0)
    {
        WA_ERROR("GetTransportData(): No data collected from %s\n", TRANSPORT_STATUS_FILE);
        return false;
    }

    WA_INFO("GetTransportData(): %s parsed successfully\n", TRANSPORT_STATUS_FILE);
    return true;
}

 bool WA_DIAG_TUNER_GetTunerStatuses(WA_DIAG_TUNER_TunerStatus_t * statuses, size_t statusCount, int * pNumLocked, bool* freqLocked, char* freq, Frontend_t* frontendStatus)
 {
#ifndef USE_FRONTEND_PROCFS
     char oid[256];

     memset(statuses, 0, statusCount * sizeof(statuses[0]));

     size_t notLockedCount = statusCount;
     for (size_t tunerIndex = 0; tunerIndex < statusCount; tunerIndex++)
     {

         if(WA_OSA_TaskCheckQuit())
         {
             WA_DBG("GetTunerStatusses: test cancelled\n");
             return false;
         }

         int k = snprintf(oid, sizeof(oid), "%s.%i", OID_TUNER_STATE, tunerIndex + 1);
         if ((k >= sizeof(oid)) || ( k < 0))
         {
             WA_ERROR("Unable to generate OID for tuner.\n");
             return false;
         }

         WA_UTILS_SNMP_Resp_t tunerState;
         tunerState.type = WA_UTILS_SNMP_RESP_TYPE_LONG;
         tunerState.data.l = 0;
         if (!WA_UTILS_SNMP_GetNumber(SNMP_SERVER_ESTB, oid, &tunerState, WA_UTILS_SNMP_REQ_TYPE_GET))
         {
             WA_ERROR("Tuner %i failed to get status\n", (int)tunerIndex);
             return false;
         }

         statuses[tunerIndex].used = true;
         statuses[tunerIndex].locked = tunerState.data.l == TUNER_LOCKED;
         if (statuses[tunerIndex].locked)
         {
             notLockedCount--;
         }

         WA_INFO("Tuner %i state %i\n", (int)tunerIndex, (int)tunerState.data.l);
     }

     if (pAllLocked)
     {
         *pNumLocked = statusCount - notLockedCount;
     }

     return true;

#else /* USE_FRONTEND_PROCFS */
    FILE* fd;
    bool freqExists = false;
    char buf[LINE_LEN];
    char format[LINE_LEN];
    char result[LINE_LEN];
    char lockStatus[LINE_LEN];
    char SNR[LINE_LEN];
    char fecCorrected[LINE_LEN];
    char fecUncorrected[LINE_LEN];
    int frontend = 0;
    typedef enum {
        parseFe = 0,
        parseLock,
        parseFreq
    } parseMode_t;
    parseMode_t parseMode = parseFe;

    WA_DBG("GetTunerStatusses(): Freq: %s\n", freq);

    *pNumLocked = 0;
    if ((fd = fopen(PROCFS_STATUS_FILE,"r")) == NULL)
    {
        WA_ERROR("GetTunerStatusses(): Cannot get statuses.\n");
        return false;
    }
    while((frontend < statusCount) && fgets(buf, LINE_LEN, fd))
    {
        switch(parseMode)
        {
        case parseFe:
            /* pacexg1v3 : "frontend 0: ae49ea00, acquired y" */
            /* arrisxg1v3: "frontend 0: af2cfa00, acquired y" */
            /* arrisxg1v4: "frontend  0: cbabdc00:cb96a000, acquired y" */
            snprintf(format, LINE_LEN, "%%*[^,], acquired %%%ds", LINE_LEN);
            if (sscanf(buf, format, result) == 1)
            {
                WA_DBG("GetTunerStatusses(): Frontend[%d]:%s\n", frontend, result);
                if(result[0] == 'y')
                {
                    parseMode = (freq && strcmp(freq, "")) ? parseFreq : parseLock;
                    statuses[frontend].used = true;
                }
                else if(statuses[frontend].locked)
                {
                    WA_DBG("GetTunerStatusses(): RELEASE DETECTED[%d]\n", frontend);
                    ++(*pNumLocked);
                }
                if (result[0] == 'n')
                {
                    ++frontend;
                }
            }
            break;
        case parseLock:
            /* pacexg1v3 : "    lockStatus=locked, snr=0dB (est), fecCorrected=0, fecUncorrected=0" */
            /* arrisxg1v3: "    lockStatus=locked, snr=43dB (est), fecCorrected=0, fecUncorrected=0" */
            /* arrisxg1v4: "    lockStatus=locked, snr=0dB (est), fecCorrected=0, fecUncorrected=0" */
            snprintf(format, LINE_LEN, " lockStatus=%%%ds", LINE_LEN);
            if (sscanf(buf, format, result) == 1)
            {
                WA_DBG("GetTunerStatusses(): Lock[%d]:%s\n", frontend, result);
                if(!strcmp(result, "locked,"))
                {
                    *freqLocked = freqExists ? true : false;
                    statuses[frontend].locked = true;
                    ++(*pNumLocked);
                }
                else if(statuses[frontend].locked)
                {
                    WA_DBG("GetTunerStatusses(): UNLOCK DETECTED[%d]\n", frontend);
                    ++(*pNumLocked);
                }

                if (*freqLocked)
                {
                    snprintf(format, LINE_LEN, " lockStatus=%%s snr=%%s (est), fecCorrected=%%s fecUncorrected=%%s");
                    if (sscanf(buf, format, lockStatus, SNR, fecCorrected, fecUncorrected) == 4)
                    {
                        lockStatus[strlen(lockStatus)-1] = 0;
                        fecCorrected[strlen(fecCorrected)-1] = 0;
                        if (frontendStatus)
                        {
                            frontendStatus->frontend = frontend;
                            strncpy(frontendStatus->lock_status, lockStatus, sizeof(frontendStatus->lock_status));
                            strncpy(frontendStatus->snr, SNR, sizeof(frontendStatus->snr));
                            strncpy(frontendStatus->corrected, fecCorrected, sizeof(frontendStatus->corrected));
                            strncpy(frontendStatus->uncorrected, fecUncorrected, sizeof(frontendStatus->uncorrected));
                            getBER(frontendStatus->ber, sizeof(frontendStatus->ber), frontend);
                        }
                        else
                        {
                            WA_ERROR("GetTunerStatusses(): Error in storing frontend data\n");
                        }
                    }
                    else
                    {
                        WA_ERROR("GetTunerStatusses(): Unable to retrieve required metrics data from frontend file\n");
                        *freqLocked = false; // Setting it back to false when we don't get required data
                    }

                }
                parseMode = parseFe;
                ++frontend;
            }
            break;
        case parseFreq:
            /* freq=387000000 hz, modulation=Qam-256, annex=B */
            snprintf(format, LINE_LEN, " freq=%%%ds", LINE_LEN);
            if (sscanf(buf, format, result) == 1)
            {
                freqExists = strstr(buf, freq) ? true : false;
                parseMode = parseLock;
                WA_DBG("GetTunerStatusses(): freq[%s], freqExists:%i\n", freq, freqExists);
            }
            break;
        }

        if (freqExists && *freqLocked)
        {
            WA_DBG("GetTunerStatusses(): frontend=%i, freqLocked=%s, lockStatus=%s, snr=%s, fecCorrected=%s, fecUncorrected=%s\n", frontend, freq, lockStatus, SNR, fecCorrected, fecUncorrected);
            fclose(fd);
            return true;
        }
#if WA_DEBUG
        {
            int freq;
            snprintf(format, LINE_LEN, " freq=%%d");
            if (sscanf(buf, format, &freq) == 1)
            {
                WA_DBG("GetTunerStatusses(): freq=%d\n", freq/1000000);
            }
        }
#endif
        if(WA_OSA_TaskCheckQuit())
        {
            WA_DBG("GetTunerStatusses(): cancelled\n");
            fclose(fd);
            return false;
        }
    }
    fclose(fd);

#ifdef USE_UNRELIABLE_PROCFS_WORKAROUND
    if (frontend < statusCount)
    {
        *pNumLocked = 0;
        WA_WARN("GetTunerStatusses(): WARNING: broken status file?\n");
    }
#endif

    WA_DBG("GetTunerStatusses(): locked: %d/%d\n", *pNumLocked, statusCount);
    return true;
#endif /* USE_FRONTEND_PROCFS */
 }

 int WA_DIAG_TUNER_status(void* instanceHandle, void *initHandle, json_t **pJsonInOut)
 {
     int result = WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR;
     json_t * config = NULL;
     json_t * tuneUrls;
     char tuneData[128];
     char freq[64];
     int numLocked = 0;
     int frequency = 0;
     int retCode = 0;
     int numTestTuner = 0;
     int num_parser = 0;
     int num_pid = 0;
     int num_hvd = 0;
     bool standAloneTest = false;
     bool decoderData = false;
     bool freqLocked = false;
     VideoDecoder_t* hvdStatus = NULL;
     ParserBand_t* parserBand = NULL;
     PIDChannel_t* pidChannel = NULL;
     Frontend_t* frontendStatus = NULL;
#ifdef USE_SEVERAL_LOCK_PROBES
     int i;
     struct timespec sTime = {.tv_nsec=(LOCK_LOOP_TIME * 1000000), .tv_sec=0};
#endif

     // Read the input json before setting it to null, for ngan tune test and to decide whether it is ngan test or general test
     getTuneData(*pJsonInOut, &tuneData[0], sizeof(tuneData));
     standAloneTest = strcmp(tuneData, "") ? true : false;

     WA_DBG("%s(%d): Standalone Test: %i, Tune Data: %s\n", __FUNCTION__, __LINE__, standAloneTest, tuneData);

     // Clear the json for output data
     json_decref(*pJsonInOut);
     *pJsonInOut = NULL;

     if (standAloneTest) // NGAN feature
     {
         result = verifyStandbyState();
         if (result != WA_DIAG_ERRCODE_SUCCESS) // If result is success, continue to ngan tune test
         {
             *pJsonInOut = json_string("Device state not received.");
             CLIENT_LOG("HwTestTuneResult_telemetry: WARNING_DEVICE_NOT_IN_LIGHTSLEEP\n");
             WA_RETURN("WA_DIAG_TUNER_status(): NGAN test returns after power state check with result : %d\n", result); // Related Telemetry and Tuneresults are handled in verifyStandbyState()
             return result;
         }

         /* Get all the required video decoder information */
         /* These data structures are not utilized during normal hwselftest run */
         /* Memory allocation is done dynamically during standalone/NGAN test */
         hvdStatus = (VideoDecoder_t*) malloc(sizeof(VideoDecoder_t) * NUM_DECODERS); // Handle deallocations

         if (!hvdStatus)
         {
             WA_ERROR("WA_DIAG_TUNER_status(): Memory allocation failed for Video_decoder data");
             CLIENT_LOG("HwTestTuneResult_telemetry: WARNING_INTERNAL_TEST_ERROR\n");
             saveTuneFailureMsg("INTERNAL_TEST_ERROR");
             return WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR;
         }

         if (WA_DIAG_TUNER_GetVideoDecoderData(hvdStatus, &num_hvd))
         {
             decoderData = true;
             parserBand = (ParserBand_t*) malloc(sizeof(ParserBand_t) * NUM_PARSER_BANDS); // Handle deallocations
             pidChannel = (PIDChannel_t*) malloc(sizeof(PIDChannel_t) * NUM_PID_CHANNELS); // Handle deallocations

             if (!parserBand || !pidChannel)
             {
                 WA_ERROR("WA_DIAG_TUNER_status(): Memory allocation failed for Transport data");
                 CLIENT_LOG("HwTestTuneResult_telemetry: WARNING_INTERNAL_TEST_ERROR\n");
                 freeDecoderTransportData(hvdStatus, parserBand, pidChannel); // Deallocating hvdStatus, parserBand, pidChannel
                 saveTuneFailureMsg("INTERNAL_TEST_ERROR");
                 return WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR;
             }

             WA_DIAG_TUNER_GetTransportData(parserBand, &num_parser, pidChannel, &num_pid);

             if (num_parser == 0 && parserBand)
             {
                 free(parserBand);
                 parserBand = NULL;
             }

             if (num_pid == 0 && pidChannel)
             {
                 free(pidChannel);
                 pidChannel = NULL;
             }
         }

         if (!decoderData && hvdStatus)
         {
             // If code reaches here, it means NGAN phase 1 will be executed and the result will not contain decoder and transport information
             WA_INFO("WA_DIAG_TUNER_status(): Decoders are not running so the result will contain only frontend data if tune is successful\n");
             free(hvdStatus);
             hvdStatus = NULL;
         }
     }

     /* Determine if the test is applicable: */
     config = ((WA_DIAG_proceduresConfig_t*)initHandle)->config;
     unsigned int tunersCount = getNumberOfTuners(config);

     if (tunersCount <= 0)
     {
         WA_INFO("WA_DIAG_TUNER_status(): Not applicable (tuner count: %i)\n", (int)tunersCount);
         *pJsonInOut = json_string("Not applicable.");
         if (standAloneTest)
         {
             CLIENT_LOG("HwTestTuneResult_telemetry: FAILED_TUNER_NOT_AVAILABLE\n");
             saveTuneFailureMsg("TUNER_NOT_AVAILABLE");
         }

         return WA_DIAG_ERRCODE_NOT_APPLICABLE;
     }

     if (!decoderData) // The decoderData will be true only during NGAN test and when decoder values are fetched
     {
         /* To check if the cable is connected and if the tuners are locked */
         result = checkQamConnection(tunersCount, pJsonInOut);

         if (standAloneTest && (result == WA_DIAG_ERRCODE_TUNER_NO_LOCK))
         {
             CLIENT_LOG("HwTestTuneResult_telemetry: FAILED_TUNER_NOT_AVAILABLE\n");
             saveTuneFailureMsg("TUNER_NOT_AVAILABLE");
             return result;
         }

         if (!standAloneTest && (result != WA_DIAG_ERRCODE_FAILURE))
             return result;
     }

     tuneUrls = getTuneUrls(tuneData, &frequency);
     snprintf(freq, sizeof(freq),"%i", frequency);

     if(WA_OSA_TaskCheckQuit())
     {
         WA_DBG("WA_DIAG_TUNER_status(): test cancelled #1\n");
         *pJsonInOut = json_string("Test cancelled.");
         return WA_DIAG_ERRCODE_CANCELLED;
     }

     size_t urlCount = json_array_size(tuneUrls);
     if(urlCount == 0)
     {
         json_decref(tuneUrls);
         *pJsonInOut = json_string("Missing SICache.");
         if (standAloneTest)
         {
             CLIENT_LOG("HwTestTuneResult_telemetry: FAILED_TUNER_FAILED\n");
             saveTuneFailureMsg("TUNE_FAILED");
         }

         return WA_DIAG_ERRCODE_SI_CACHE_MISSING;
     }

     numTestTuner = 1; // Testing only one tuner is enough for tuner diagnostics with only one session
     TuneSession_t sessions[numTestTuner];
     memset(sessions, 0, sizeof(sessions));

     WA_DIAG_TUNER_TunerStatus_t statuses[tunersCount];
     memset(statuses, 0, sizeof(statuses));
     frontendStatus = (Frontend_t*) malloc(sizeof(Frontend_t)); // Needs deallocation

     if (decoderData) // NGAN Phase-2
     {
         if (!frontendStatus)
         {
             WA_ERROR("WA_DIAG_TUNER_status(): Memory allocation failed for Frontend data");
             CLIENT_LOG("HwTestTuneResult_telemetry: WARNING_INTERNAL_TEST_ERROR\n");
             saveTuneFailureMsg("INTERNAL_TEST_ERROR");
             freeDecoderTransportData(hvdStatus, parserBand, pidChannel); // Deallocating hvdStatus, parserBand, pidChannel
             return WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR;
         }

#ifndef USE_SEVERAL_LOCK_PROBES
         retCode = WA_DIAG_TUNER_GetTunerStatuses(statuses, tunersCount, &numLocked, &freqLocked, freq, frontendStatus);
#else
         for (i = 0; i < LOCK_LOOP_COUNT && !freqLocked; i++)
         {
             retCode = WA_DIAG_TUNER_GetTunerStatuses(statuses, tunersCount, &numLocked, &freqLocked, freq, frontendStatus);
             if (retCode && !freqLocked)
             {
                 WA_DBG("WA_DIAG_TUNER_status(): retrying status check for standalone tuner test (%i)...\n", i);
                 nanosleep(&sTime, NULL);
             }
         }
#endif /* USE_SEVERAL_LOCK_PROBES */
         if (!freqLocked && frontendStatus)
         {
             free(frontendStatus);
             frontendStatus = NULL;
         }
         retCode = saveDecoderTuneResults(hvdStatus, num_hvd, parserBand, num_parser, pidChannel, num_pid, frontendStatus);
         result = (retCode == 1) ? WA_DIAG_ERRCODE_FAILURE : WA_DIAG_ERRCODE_SUCCESS;
         freeDecoderTransportData(hvdStatus, parserBand, pidChannel);
         goto end;
     }

     WA_DBG("WA_DIAG_TUNER_status(): Tuning to url (tuner count: %i, tuners to test: %i, frequency: %s)\n", (int)tunersCount, numTestTuner, freq);

     for (size_t urlIndex = 0; (urlIndex < urlCount); ++urlIndex)
     {
         numLocked = 0;

         if(WA_OSA_TaskCheckQuit())
         {
             WA_DBG("WA_DIAG_TUNER_status(): test cancelled #2\n");
             result = WA_DIAG_ERRCODE_CANCELLED;
             break;
         }

        retCode = !standAloneTest ? WA_DIAG_TUNER_GetTunerStatuses(statuses, tunersCount, &numLocked, &freqLocked, "", NULL) : 0;
        if(retCode && (numLocked == tunersCount))
        {
            WA_INFO("WA_DIAG_TUNER_status(): All tuners initially locked.\n");
            result = WA_DIAG_ERRCODE_SUCCESS;
            break;
        }

         WA_INFO("WA_DIAG_TUNER_status(): Processing URL %zu.\n", (size_t)urlIndex);
         const char * url = json_string_value(json_array_get(tuneUrls, urlIndex));
         WA_DBG("WA_DIAG_TUNER_status(): URL[%d]: %s\n", urlIndex, url);
         if (!url)
         {
             WA_ERROR("WA_DIAG_TUNER_status(): URL %i is not a string\n", (int)urlIndex);
             continue;
         }

         retCode = startTuneSessions(instanceHandle,
             sessions,
             numTestTuner,
             url,
             urlIndex,
             urlCount,
             statuses);

         if(WA_OSA_TaskCheckQuit())
         {
             WA_DBG("WA_DIAG_TUNER_status(): test cancelled #3\n");
             result = WA_DIAG_ERRCODE_CANCELLED;
             break;
         }

         if(!retCode)
         {
             WA_ERROR("WA_DIAG_TUNER_status(): Could not start tune sessions for URL: %s\n", url);
         }

#ifndef USE_SEVERAL_LOCK_PROBES
         retCode = WA_DIAG_TUNER_GetTunerStatuses(statuses, tunersCount, &numLocked, &freqLocked, freq, frontendStatus);
#else
         for(i=0, retCode=true, numLocked=0;
             retCode && (i<LOCK_LOOP_COUNT) && (numLocked < tunersCount) && !WA_OSA_TaskCheckQuit() && !freqLocked; ++i)
         {
             retCode = WA_DIAG_TUNER_GetTunerStatuses(statuses, tunersCount, &numLocked, &freqLocked, freq, frontendStatus);

#if defined(USE_FRONTEND_PROCFS) && defined(USE_UNRELIABLE_PROCFS_WORKAROUND)
             if(retCode && (numLocked == 0))
#else
             if(retCode && (numLocked < tunersCount))
#endif
             {
                 WA_DBG("WA_DIAG_TUNER_status(): retrying status check (%i)...\n", i);
                 nanosleep(&sTime, NULL);
             }
         }
#endif /* USE_SEVERAL_LOCK_PROBES */

         closeTuneSessions(sessions, numTestTuner);

         WA_UTILS_SICACHE_TuningSetLuckyId(numLocked ? urlIndex : -1);

         if(!retCode)
         {
             if (standAloneTest)
             {
                 CLIENT_LOG("HwTestTuneResult_telemetry: FAILED_TUNER_FAILED\n");
                 saveTuneFailureMsg("TUNE_FAILED");
             }

             result = WA_DIAG_ERRCODE_FAILURE;
             break;
         }

         if ((standAloneTest && freqLocked) || (!standAloneTest && (numLocked >= tunersCount)))
         {
             WA_INFO("WA_DIAG_TUNER_status(): All tuners locked.\n");
             saveTuneResults(frontendStatus);
             result = WA_DIAG_ERRCODE_SUCCESS;
         }
         else
         {
             WA_WARN("WA_DIAG_TUNER_status(): Not all tuners locked, url: %s\n", (char*)url);
             if (standAloneTest)
             {
                 CLIENT_LOG("HwTestTuneResult_telemetry: FAILED_TUNER_FAILED\n");
                 saveTuneFailureMsg("TUNE_FAILED");
             }

             result = WA_DIAG_ERRCODE_TUNER_NO_LOCK;

#ifdef USE_TRM
             if (!standAloneTest && (numLocked == tunersCount - 1))
             {
                 WA_INFO("WA_DIAG_TUNER_status(): One tuner not locked, possible TRM tuner protection\n");
                 result = WA_DIAG_ERRCODE_TUNER_BUSY;
             }
#endif /* USE_TRM */
         }

         WA_DBG("WA_DIAG_TUNER_status(): URL[%d]: result=%d\n", urlIndex, result);
     }
end:
     if (frontendStatus)
     {
         free(frontendStatus);
         frontendStatus = NULL;
     }

     switch(result)
     {
     case WA_DIAG_ERRCODE_SUCCESS:
         *pJsonInOut = json_string("Tuners good.");
         break;

#ifdef USE_TRM
     case WA_DIAG_ERRCODE_TUNER_BUSY:
        /* fall-through */
#endif
     case WA_DIAG_ERRCODE_TUNER_NO_LOCK:
     {
         *pJsonInOut = json_array();
         for (int i = 0; i < tunersCount; ++i)
         {
             json_t * stateData = json_object();
             json_object_set_new(stateData, "lock", json_integer(statuses[i].locked));
             json_array_append_new(*pJsonInOut, stateData);
         }
         break;
     }

     case WA_DIAG_ERRCODE_FAILURE:
         *pJsonInOut = json_string("Some tuner(s) error.");
         break;

     case WA_DIAG_ERRCODE_CANCELLED:
         *pJsonInOut = json_string("Test cancelled.");
         break;

     default:
         *pJsonInOut = json_string("Internal test error.");
         break;
     }
     WA_DBG("WA_DIAG_TUNER_status(): finished: %s\n",
            *pJsonInOut == NULL ? "null" : json_string_value(*pJsonInOut));
     json_decref(tuneUrls);

     return result;
 }

/*
return 0 : Continue with tune functionality
       1 : return FAILED_TUNER_NOT_AVAILABLE
      -3 : Failed to get power state "WARNING_Test_Not_Run"
*/
int verifyStandbyState()
{
    int state = WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR;
    int is_connected = 0;
    struct stat buffer;

    // Delete the existing tune results file before running tune test
    if (stat(HWSELFTEST_TUNERESULTS_FILE, &buffer) == 0)
    {
        if (remove(HWSELFTEST_TUNERESULTS_FILE))
        {
            WA_ERROR("Unable to delete tune results file\n");
            goto end;
        }
    }

    if (WA_UTILS_IARM_Connect())
    {
        WA_ERROR("verifyStandbyState() WA_UTILS_IARM_Connect() Failed \n");
        goto end;
    }

    IARM_Result_t iarm_result = IARM_Bus_IsConnected(IARM_BUS_TR69HOSTIFMGR_NAME, &is_connected);
    if (iarm_result != IARM_RESULT_SUCCESS)
    {
        WA_ERROR("verifyStandbyState(): IARM_Bus_IsConnected('%s') failed\n", IARM_BUS_TR69HOSTIFMGR_NAME);
        goto iarm_disconnect;
    }

    IARM_Bus_PWRMgr_GetPowerState_Param_t param;
    IARM_Result_t res = IARM_Bus_Call(IARM_BUS_PWRMGR_NAME, IARM_BUS_PWRMGR_API_GetPowerState,(void *)&param, sizeof(param));

    /* Query current Power state  */
    if (IARM_RESULT_SUCCESS == res)
    {
        switch (param.curState)
        {
        case IARM_BUS_PWRMGR_POWERSTATE_STANDBY:
        case IARM_BUS_PWRMGR_POWERSTATE_STANDBY_LIGHT_SLEEP:
            WA_DBG ("verifyStandbyState() Current power state is LIGHTSLEEP\n");
            state = WA_DIAG_ERRCODE_SUCCESS;
            break;
        default:
            WA_ERROR ("verifyStandbyState() Current power state is not in LIGHTSLEEP\n");
            state = WA_DIAG_ERRCODE_CANCELLED_NOT_STANDBY;
            break;
        }
    }
    else
    {
        WA_ERROR ("verifyStandbyState() Unable to get power state from IARM\n");
    }

iarm_disconnect:
    if (WA_UTILS_IARM_Disconnect())
    {
        WA_ERROR("verifyStandbyState(): WA_UTILS_IARM_Disconnect() Failed \n");
    }

end:
    // Print telemetry for failure cases
    if (state == WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR)
    {
        CLIENT_LOG("HwTestTuneResult_telemetry: WARNING_Test_Not_Run\n");
        saveTuneFailureMsg("TUNE_FAILED");
    }
    else if (state == WA_DIAG_ERRCODE_CANCELLED_NOT_STANDBY)
    {
        CLIENT_LOG("HwTestTuneResult_telemetry: FAILED_TUNER_NOT_AVAILABLE\n");
        saveTuneFailureMsg("TUNER_NOT_AVAILABLE");
    }

    return state;
}

bool getBER(char* ber, size_t size, int frontend)
{
    char oid[BUFFER_LEN];
    int k = snprintf(oid, sizeof(oid), "%s.%i", OID_TUNER_BER, frontend + 1);
    if ((k >= sizeof(oid)) || (k < 0))
    {
        WA_ERROR("getBER(): Unable to generate OID for QAM tuner.\n");
        return false;
    }

    WA_UTILS_SNMP_Resp_t tunerResp;
    tunerResp.type = WA_UTILS_SNMP_RESP_TYPE_LONG;
    tunerResp.data.l = 0;

    if (!WA_UTILS_SNMP_GetNumber(SNMP_SERVER_ESTB, oid, &tunerResp, WA_UTILS_SNMP_REQ_TYPE_GET))
    {
        WA_ERROR("getBER(): Tuner %i failed to get QAM BER\n", frontend);
        return false;
    }

    WA_DBG("getBER(): Tuner %i QAM_BER = %i\n", frontend, (int)tunerResp.data.l);
    snprintf(ber, size, "%i", (int) tunerResp.data.l);

    return true;
}

int saveDecoderTuneResults(VideoDecoder_t* hvd, int num_hvd, ParserBand_t* parserBand, int num_parser, PIDChannel_t* pidChannel, int num_pid, Frontend_t* frontendStatus)
{
    int ret = -1;
    char buf[NUM_BYTES];
    json_t *json_frontend = NULL;
    json_t *json_array = NULL;
    json_t *json_parserBand = json_object();
    json_t *json_pidChannel = json_object();
    json_t *json_transport = json_object();
    json_t *json_hvd = json_object();
    json_t *json_decoder = json_object();
    json_t *json = NULL;

    WA_ENTER("saveDecoderTuneResults() Enter\n");

    if (frontendStatus)
    {
        json_frontend = json_pack("[sssss]", frontendStatus->snr, frontendStatus->corrected, frontendStatus->uncorrected, frontendStatus->lock_status, frontendStatus->ber);
    }
    else
    {
        json_frontend = json_pack("[s]", DATA_NOT_AVAILABLE_STRING);
        WA_ERROR("saveDecoderTuneResults(): frontend information not available\n");
    }

    if (parserBand)
    {
        for (int index = 0; index < num_parser; index++)
        {
            snprintf(buf, sizeof(buf), "%i", index);
            json_array = json_pack("[iii]", parserBand[index].parser_band[0], parserBand[index].parser_band[1], parserBand[index].parser_band[2]);
            json_object_set_new(json_parserBand, buf, json_array);
        }
        json_object_set_new(json_transport, "parserband", json_parserBand);
    }
    else
    {
        json_object_set_new(json_transport, "parserband", json_string(DATA_NOT_AVAILABLE_STRING));
        WA_ERROR("saveDecoderTuneResults(): parserband information not available\n");
    }

    if (pidChannel)
    {
        for (int index = 0; index < num_pid; index++)
        {
            json_object_set_new(json_pidChannel, pidChannel[index].pid_channel, json_integer(pidChannel[index].cc_errors));
        }
        json_object_set_new(json_transport, "pidchannel", json_pidChannel);
    }
    else
    {
        json_object_set_new(json_transport, "pidchannel", json_string(DATA_NOT_AVAILABLE_STRING));
        WA_ERROR("saveDecoderTuneResults(): pidchannel information not available\n");
    }

    if (hvd)
    {
        for (int index = 0; index < num_hvd; index++)
        {
            snprintf(buf, sizeof(buf), "HVD%i", index);

            json_array = json_pack("[ssss]", hvd[index].tsm_data[0], hvd[index].tsm_data[1], hvd[index].tsm_data[2], hvd[index].tsm_data[3]);
            json_object_set_new(json_hvd, "TSM", json_array);

            json_array = json_pack("[iiii]", hvd[index].decode_data[0], hvd[index].decode_data[1], hvd[index].decode_data[2], hvd[index].decode_data[3]);
            json_object_set_new(json_hvd, "Decode", json_array);

            json_array = json_pack("[iiii]", hvd[index].display_data[0], hvd[index].display_data[1], hvd[index].display_data[2], hvd[index].display_data[3]);
            json_object_set_new(json_hvd, "Display", json_array);

            json_object_set_new(json_decoder, buf, json_hvd);
        }
    }
    else
    {
        // Just to handle negative cases for future, at present code will never reach here
        json_object_set_new(json_decoder, "HVD", json_string(DATA_NOT_AVAILABLE_STRING));
        WA_ERROR("saveDecoderTuneResults(): video_deoder information not available\n");
    }

    json = json_pack("{s:o,s:o,s:o}", "Frontend", json_frontend, "Transport", json_transport, "VideoDecoder", json_decoder);

    /* Write result into tuneresults file */
    FILE *f = fopen(HWSELFTEST_TUNERESULTS_FILE, "wb+");
    if (f)
    {
        if (!json_dumpf(json, f, 0))
        {
            ret = 0;
            fsync(fileno(f));
            WA_INFO("saveDecoderTuneResults(): json saved to file successfully\n");
        }
        else
            WA_ERROR("saveDecoderTuneResults(): json_dump_file() failed\n");

        fclose(f);
    }
    else
        WA_ERROR("saveDecoderTuneResults(): failed to create file\n");

    json_decref(json_parserBand);
    json_decref(json_pidChannel);
    json_decref(json_transport);
    json_decref(json_hvd);
    json_decref(json_decoder);

    if (!frontendStatus)
    {
        CLIENT_LOG("HwTestTuneResult_telemetry: FAILED_TUNER_FAILED\n");
        return 1;
    }

    if (ret == 0)
    {
        /* Print result in log file if writing to results file is success */
        CLIENT_LOG("HwTestTuneResultHeader: SNR,fecCorrected,fecUncorrected,BER,lockStatus,decoderState");
        CLIENT_LOG("HwTestTuneResult_telemetry: %s,%s,%s,%s,%s,true", frontendStatus->snr, frontendStatus->corrected, frontendStatus->uncorrected, frontendStatus->ber, frontendStatus->lock_status);
    }
    else
    {
        CLIENT_LOG("HwTestTuneResult_telemetry: FAILED_RESULTS_NOT_AVAILABLE\n");
    }

    return ret;
}

/*Return 0 - All data is good and written to log file and results file
        -1 - Unable to get data
*/
int saveTuneResults(Frontend_t* frontendStatus)
{
    json_t *json = NULL;

    WA_ENTER("saveTuneResults() Enter\n");

    /* Create json string to write into tuneresults file */
    json = json_pack("{s:s,s:s,s:s,s:s,s:s}",
       "SNR", frontendStatus->snr,
       "Corrected", frontendStatus->corrected,
       "Uncorrected", frontendStatus->uncorrected,
       "BER", frontendStatus->ber,
       "RXLock", frontendStatus->lock_status);

    /* Write result into tuneresults file */
    FILE *f = fopen(HWSELFTEST_TUNERESULTS_FILE, "wb+");
    if (f)
    {
        if (!json_dumpf(json, f, 0))
        {
            fsync(fileno(f));

            /* Print result in log file if writing to results file is success */
            CLIENT_LOG("HwTestTuneResultHeader: SNR,fecCorrected,fecUncorrected,BER,lockStatus,decoderState");
            CLIENT_LOG("HwTestTuneResult_telemetry: %s,%s,%s,%s,%s,false", frontendStatus->snr, frontendStatus->corrected, frontendStatus->uncorrected, frontendStatus->ber, frontendStatus->lock_status);

            WA_INFO("saveTuneResults(): json saved to file successfully\n");
        }
        else
        {
            WA_ERROR("saveTuneResults(): json_dump_file() failed\n");
            fclose(f);
            goto end;
        }

        fclose(f);
    }
    else
    {
        WA_ERROR("saveTuneResults(): failed to create file\n");
        goto end;
    }
    return 0;

end:
    CLIENT_LOG("HwTestTuneResult_telemetry: FAILED_RESULTS_NOT_AVAILABLE\n");
    return -1;
}

static int saveTuneFailureMsg(const char *errString)
{
    FILE *f;

    f = fopen(HWSELFTEST_TUNERESULTS_FILE, "wb+");
    if (f)
    {
        if (strcmp(errString, ""))
        {
            fputs(errString, f);
            WA_INFO("saveTuneFailureMsg(): Tune failure message written to file successfully\n");
        }
        else
        {
            WA_ERROR("saveTuneFailureMsg(): No data written into file\n");
        }

        fclose(f);
    }
    else
    {
        WA_ERROR("saveTuneFailureMsg(): failed to create file\n");
    }

    return 0;
}

static void freeDecoderTransportData(VideoDecoder_t* hvd, ParserBand_t* parserBand, PIDChannel_t* pidChannel)
{
    if (hvd)
    {
        free(hvd);
        hvd = NULL;
    }

    if (parserBand)
    {
        free(parserBand);
        parserBand = NULL;
    }

    if (pidChannel)
    {
        free(pidChannel);
        pidChannel = NULL;
    }
}

 /* End of doxygen group */
 /*! @} */
