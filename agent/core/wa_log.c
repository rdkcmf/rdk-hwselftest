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
 * @brief Implementation of test results logging functionality.
 */

/** @addtogroup WA_LOG
 *  @{
 */

/*****************************************************************************
 * STANDARD INCLUDE FILES
 *****************************************************************************/
#define _GNU_SOURCE
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/
#include "wa_log.h"
#include "wa_debug.h"

/*****************************************************************************
 * LOCAL DEFINITIONS
 *****************************************************************************/
#define LOG_FILE_PATH "/opt/logs/hwselftest.log"

/*****************************************************************************
 * LOCAL FUNCTION PROTOTYPES
 *****************************************************************************/
static void saveLog(const char * msg);
static void saveRawLog(const char * msg);
static void writeLineToLog(const char * line);
static void prepareLogHeader(char * buffer, const char * mark, size_t bufferSize);

/*****************************************************************************
 * FUNCTION DEFINITIONS
 *****************************************************************************/

bool WA_LOG_Init()
{
    rdk_Error re;
    if ((re = rdk_logger_init("/etc/debug.ini")) != RDK_SUCCESS)
    {
        WA_ERROR("rdk_logger_init returned %i\n", (int)re);
        return false;
    }
    return true;
}

void WA_LOG_Client(bool raw, const char * format, ...)
{
    char * logmsg = NULL;
    va_list ap;

    va_start(ap, format);
    if (vasprintf(&logmsg, format, ap) == -1)
    {
        WA_ERROR("vasprintf failed, format: %s, error: %i %s\n", format, errno, strerror(errno));
    }
    else
    {
        if(raw)
        {
            saveRawLog(logmsg);
        }
        else
        {
            saveLog(logmsg);
        }
    }
    va_end(ap);
    free(logmsg);
}

int WA_LOG_Log(json_t **pJson)
{
    int status;
    json_t *jps;
    const char *msg;

    status = json_unpack(*pJson, "{s:o}", "params", &jps); //reference to jParams is NOT modified
    if(status != 0)
    {
        WA_INFO("WA_LOG_Log(): json_unpack() params undefined\n");
        goto end;
    }

    if(json_typeof(jps) != JSON_OBJECT)
    {
        WA_INFO("WA_LOG_Log(): params is not an object\n");
        status = -1;
        goto end;
    }

    msg = json_string_value(json_object_get(jps, "message"));
    if(msg != NULL)
    {
        WA_LOG_Client(false, "%s", msg);
        goto end;
    }
    msg = json_string_value(json_object_get(jps, "rawmessage"));
    if(msg != NULL)
    {
        WA_LOG_Client(true, "%s", msg);
        goto end;
    }

end:
    json_decref(*pJson);
    *pJson = NULL;

    return 0;
}

char *WA_LOG_GetTimestampStr(time_t time, char *buffer, int buffer_size)
{
    struct tm bdTime;
    gmtime_r(&time, &bdTime);
    strftime(buffer, buffer_size, "%F %T", &bdTime);
    return buffer;
}

int WA_LOG_GetTimestampTime(const char *buffer, time_t *time)
{
    struct tm bdTime;
    if (time && strptime(buffer, "%F %T", &bdTime))
    {
        *time = timegm(&bdTime);
        return 0;
    }
    else
        return 1;
}

/*****************************************************************************
 * LOCAL FUNCTIONS
 *****************************************************************************/

static void saveLog(const char * msg)
{
    char * buffer = NULL;
    static const int HEADER_MAX_LENGTH = 128;
    size_t msgLen = strlen(msg);

    char mark[] = "HWST_LOG |";
    size_t markLen = strlen(mark);

    buffer = (char*) malloc (markLen + msgLen + HEADER_MAX_LENGTH + 1 + 1);
    if (!buffer)
    {
        return;
    }

    prepareLogHeader(buffer, mark, HEADER_MAX_LENGTH);
    size_t hdrLen = strlen(buffer);
    memcpy(buffer + hdrLen, msg, msgLen);
    buffer[hdrLen + msgLen] = '\n';
    buffer[hdrLen + msgLen + 1] = 0;

    writeLineToLog(buffer);

    free(buffer);
}

static void saveRawLog(const char * msg)
{
    char * buffer = NULL;
    size_t msgLen = strlen(msg);

    buffer = (char*) malloc(msgLen + 1 + 1);
    if (!buffer)
    {
        return;
    }

    memcpy(buffer, msg, msgLen);
    buffer[msgLen] = '\n';
    buffer[msgLen + 1] = 0;

    writeLineToLog(buffer);

    free(buffer);
}

static void writeLineToLog(const char * line)
{
#ifdef HWST_LOG_TO_FILE
    int file = open(LOG_FILE_PATH, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (file == -1)
    {
        WA_ERROR("open(%s) returned %i (%s)\n", LOG_FILE_PATH, errno, strerror(errno));
        return;
    }
    write(file, line, strlen(line));
    close(file);
#else
    printf("%s\n", line);
#endif
}

static void prepareLogHeader(char * buffer, const char * mark, size_t bufferSize)
{
    time_t curTime;
    struct tm bdTime;
    size_t curSize = 0;

    size_t markLen = strlen(mark);

    memcpy(buffer, mark, markLen);

    curTime = time(0);
    gmtime_r(&curTime, &bdTime);
    curSize = strftime(buffer + markLen, bufferSize - markLen, "%F %T ", &bdTime);
    buffer[curSize + markLen] = 0;
}

/* End of doxygen group */
/*! @} */

/* EOF */
