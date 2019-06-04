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
#define PROCFS_STATUS_FILE "/proc/brcm/frontend"
#define LINE_LEN 256
#endif

#define TUNE_RESPONSE_TIMEOUT 14000 /* in [ms] */
#define REQUEST_PATTERN "/vldms/tuner?ocap_locator=ocap://tune://frequency=%u_modulation=%u_pgmno=%u"
#define REQUEST_MAX_LENGTH 256

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
static json_t * getTuneUrls();

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

 static json_t * getTuneUrls()
 {
     json_t * result;
     WA_UTILS_SICACHE_Entries_t *pEntries=NULL;
     int status, i, lucky, entries=0;
     char request[REQUEST_MAX_LENGTH];

     result = json_array();

     lucky = WA_UTILS_SICACHE_TuningGetLuckyId();

     status = WA_UTILS_SICACHE_TuningRead(lucky >= NUM_SI_ENTRIES ? lucky+1 : NUM_SI_ENTRIES, &pEntries);

     if((lucky >= 0) && (status > lucky))
     {
         snprintf(request, REQUEST_MAX_LENGTH, REQUEST_PATTERN, pEntries[lucky].freq, pEntries[lucky].mod, pEntries[lucky].prog);
         WA_DBG("getTuneUrls(): lucky[%d], %s\n", lucky, request);
         json_array_append_new(result, json_string(request));
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

         WA_DIAG_TUNER_GetTunerStatuses(statuses, sessionCount, &numLocked);
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
        if (!WA_UTILS_TRH_ReserveTuner(strstr(adj_url, "ocap://"), 0 /* now */, TRM_TUNER_RESERVATION_TIME, TRM_TUNER_RESERVATION_TIMEOUT, (void **)&sessions[i].socket))
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

    char tempPWR[BUFFER_LEN] = {'\0'};
    char tempSNR[BUFFER_LEN] = {'\0'};

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

 bool WA_DIAG_TUNER_GetTunerStatuses(WA_DIAG_TUNER_TunerStatus_t * statuses, size_t statusCount, int * pNumLocked)
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
    char buf[LINE_LEN];
    char format[LINE_LEN];
    char result[LINE_LEN];
    int frontend = 0;
    typedef enum {
        parseFe = 0,
        parseLock
    } parseMode_t;
    parseMode_t parseMode = parseFe;

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
                    parseMode = parseLock;
                    statuses[frontend].used = true;
                }
                else if(statuses[frontend].locked)
                {
                    WA_DBG("GetTunerStatusses(): RELEASE DETECTED[%d]\n", frontend);
                    ++(*pNumLocked);
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
                    statuses[frontend].locked = true;
                    ++(*pNumLocked);
                }
                else if(statuses[frontend].locked)
                {
                    WA_DBG("GetTunerStatusses(): UNLOCK DETECTED[%d]\n", frontend);
                    ++(*pNumLocked);
                }
                parseMode = parseFe;
                ++frontend;
            }
            break;
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
     int retCode = 0;
#ifdef USE_SEVERAL_LOCK_PROBES
     int i;
     struct timespec sTime = {.tv_nsec=(LOCK_LOOP_TIME * 1000000), .tv_sec=0};
#endif
     json_decref(*pJsonInOut);
     *pJsonInOut = NULL;

     /* Determine if the test is applicable: */

     config = ((WA_DIAG_proceduresConfig_t*)initHandle)->config;

     const unsigned int tunersCount = getNumberOfTuners(config);

     if (tunersCount <= 0)
     {
         WA_INFO("WA_DIAG_TUNER_status(): Not applicable (tuner count: %i)\n", (int)tunersCount);
         *pJsonInOut = json_string("Not applicable.");
         return WA_DIAG_ERRCODE_NOT_APPLICABLE;
     }

     tuneUrls = getTuneUrls();

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
         return WA_DIAG_ERRCODE_SI_CACHE_MISSING;
     }

     TuneSession_t sessions[tunersCount];
     memset(sessions, 0, sizeof(sessions));

     WA_DIAG_TUNER_TunerStatus_t statuses[tunersCount];
     memset(statuses, 0, sizeof(statuses));

     for (size_t urlIndex = 0; (urlIndex < urlCount) && (result != WA_DIAG_ERRCODE_SUCCESS); ++urlIndex)
     {
         int numLocked = 0;

         if(WA_OSA_TaskCheckQuit())
         {
             WA_DBG("WA_DIAG_TUNER_status(): test cancelled #2\n");
             result = WA_DIAG_ERRCODE_CANCELLED;
             break;
         }

        retCode = WA_DIAG_TUNER_GetTunerStatuses(statuses, tunersCount, &numLocked);
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
             tunersCount,
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
         retCode = WA_DIAG_TUNER_GetTunerStatuses(statuses, tunersCount, &numLocked);
#else
         for(i=0, retCode=true, numLocked=0;
             retCode && (i<LOCK_LOOP_COUNT) && (numLocked < tunersCount) && !WA_OSA_TaskCheckQuit(); ++i)
         {
             retCode = WA_DIAG_TUNER_GetTunerStatuses(statuses, tunersCount, &numLocked);

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

         closeTuneSessions(sessions, tunersCount);

         WA_UTILS_SICACHE_TuningSetLuckyId(numLocked ? urlIndex : -1);

         if(!retCode)
         {
             result = WA_DIAG_ERRCODE_FAILURE;
             break;
         }

         if (numLocked == tunersCount)
         {
             WA_INFO("WA_DIAG_TUNER_status(): All tuners locked.\n");
             result = WA_DIAG_ERRCODE_SUCCESS;
         }
         else
         {
             WA_WARN("WA_DIAG_TUNER_status(): Not all tuners locked, url: %s\n", (char*)url);
             result = WA_DIAG_ERRCODE_TUNER_NO_LOCK;

#ifdef USE_TRM
             if (numLocked == tunersCount - 1)
             {
                WA_INFO("WA_DIAG_TUNER_status(): One tuner not locked, possible TRM tuner protection\n");
                result = WA_DIAG_ERRCODE_TUNER_BUSY;
             }
#endif /* USE_TRM */
         }

         WA_DBG("WA_DIAG_TUNER_status(): URL[%d]: result=%d\n", urlIndex, result);
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


 /* End of doxygen group */
 /*! @} */
