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
 * @brief Diagnostic functions for IR - implementation
 */

/** @addtogroup WA_DIAG_IR
 *  @{
 */

/*****************************************************************************
 * STANDARD INCLUDE FILES
 *****************************************************************************/

#define _GNU_SOURCE
#define _FILE_OFFSET_BITS 64

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <regex.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/

#include "wa_diag.h"
#include "wa_debug.h"
#include "wa_fileops.h"

/* module interface */
#include "wa_diag_ir.h"

/*****************************************************************************
 * GLOBAL VARIABLE DEFINITIONS
 *****************************************************************************/

/*****************************************************************************
 * LOCAL DEFINITIONS
 *****************************************************************************/
#define IR_LOG_FILE_NAME "/opt/logs/uimgr_log.txt"

#define PREVIOUS_PERIOD_CHECK_SEC   (10 * 60)
#define DELAY_BETWEEN_CHECKS_SEC    5
#define NUMBER_OF_CHECKS            7

/*****************************************************************************
 * LOCAL TYPES
 *****************************************************************************/

typedef enum LogFindResult_tag
{
    LFR_ERROR = 0,
    LFR_FAILURE,
    LFR_CANCELLED,
    LFR_FOUND,
    LFR_FOUND_CURRENT
} LogFindResult;

typedef struct LogFindResultWithLine_tag
{
    LogFindResult code;
    char * line;
} LogFindResultWithLine;

/*****************************************************************************
 * LOCAL FUNCTION PROTOTYPES
 *****************************************************************************/

/*****************************************************************************
 * LOCAL VARIABLE DECLARATIONS
 *****************************************************************************/

/*****************************************************************************
 * FUNCTION DEFINITIONS
 *****************************************************************************/

/*****************************************************************************
 * LOCAL FUNCTIONS
 *****************************************************************************/

static LogFindResult findLogEntryInBuffer(const char * buffer, regex_t *r, char ** pLastLog, json_t **params)
{
    LogFindResult result = LFR_FAILURE;
    int rc;
    size_t currentPos = 0;
    regmatch_t matchPositions[10];
    regmatch_t lastMatch = {};

    while (result != LFR_FOUND_CURRENT && result != LFR_CANCELLED)
    {
        if ((rc = regexec(r, buffer + currentPos, sizeof(matchPositions) / sizeof(matchPositions[0]), matchPositions, 0)) != 0)
        {
            WA_DBG("Regex not found\n");
            break;
        }

        result = LFR_FOUND;

        for (int i = 0; i < sizeof(matchPositions) / sizeof(matchPositions[0]); ++i)
        {
            if(WA_OSA_TaskCheckQuit())
            {
                WA_DBG("findLogEntryInBuffer: test cancelled\n");
                result = LFR_CANCELLED;
                break;
            }

            if (matchPositions[i].rm_so == -1)
            {
                break;
            }
            WA_DBG("Match[%i] (%i, %i): ", i, (int)matchPositions[i].rm_so, (int)matchPositions[i].rm_eo);
            for (regoff_t ro = matchPositions[i].rm_so; ro != matchPositions[i].rm_eo; ++ro)
            {
                WA_DBG("%c", buffer[currentPos + ro]);
            }
            WA_DBG("\n");
        }

        if(result == LFR_CANCELLED)
        {
            break;
        }

        struct tm bdt = {};

        if (strptime(buffer + currentPos + matchPositions[0].rm_so, "%Y %b %d %H:%M:%S.", &bdt))
        {
            time_t logTime = mktime(&bdt);
            time_t currentTime = time(NULL);
            int diff = logTime > currentTime ? logTime - currentTime : currentTime - logTime;
            if (diff < PREVIOUS_PERIOD_CHECK_SEC)
            {
                result = LFR_FOUND_CURRENT;
            }
            else
            {
                WA_DBG("Too old, diff: %i seconds\n", diff);
            }
        }

        lastMatch = matchPositions[0];
        lastMatch.rm_so += currentPos;
        lastMatch.rm_eo += currentPos;

        currentPos = lastMatch.rm_eo;
        WA_DBG("New currentPos: %i\n", (int)currentPos);
    }

    if (result == LFR_FOUND)
    {
        if (pLastLog)
        {
            size_t l = lastMatch.rm_eo - lastMatch.rm_so;
            char * s = (char*) malloc(l + 1);
            if (s)
            {
                memcpy(s, buffer + lastMatch.rm_so, l);
                s[l] = 0;
            }
            *pLastLog = s;
        }
    }

    return result;
}

static LogFindResult findLogEntryInFile(const char * filename, regex_t *r, char ** pLastLog, json_t **params)
{
    static const size_t search_buffer_size = 16384;
    static const size_t max_line_size = 512;

    LogFindResult result = LFR_ERROR;

    if (pLastLog)
    {
        *pLastLog = NULL;
    }

    char buffer[search_buffer_size];
    FILE * file = NULL;
    size_t st;
    int rc;

    file = fopen(filename, "r");
    if (!file)
    {
        *params = json_string("Unable to open log file");
        goto end;
    }

    rc = fseeko(file, 0, SEEK_END);
    if (rc == -1)
    {
        WA_DBG("fseeko failed, error: %i %s\n", errno, strerror(errno));
        *params = json_string("Unable to get log file size");
        goto end;
    }
    off_t currentEnd = ftello(file);
    if (currentEnd == (off_t)-1)
    {
        WA_DBG("ftello failed, error: %i %s\n", errno, strerror(errno));
        *params = json_string("Unable to get log file size");
        goto end;
    }

    WA_DBG("Log file size: %lli\n", (long long)currentEnd);

    while ((currentEnd > 0) && (result != LFR_FOUND) && (result != LFR_FOUND_CURRENT) && (result != LFR_CANCELLED))
    {
        off_t currentPos = (sizeof(buffer) - 1 - max_line_size) <= currentEnd ? currentEnd - (sizeof(buffer) - 1 - max_line_size) : 0;
        WA_DBG("About to analyze at position %lli\n", (long long)currentPos);
        rc = fseeko(file, currentPos, SEEK_SET);
        if (rc == -1)
        {
            WA_DBG("fseeko failed, error: %i %s\n", errno, strerror(errno));
            *params = json_string("Unable to seek log file");
            goto end;
        }

        WA_DBG("Current position: %lli\n", (long long)ftello(file));

        st = fread(buffer, 1, sizeof(buffer) - 1, file);
        if (st < 0)
        {
            WA_DBG("fread failed, error: %i %s\n", errno, strerror(errno));
            *params = json_string("Unable to read log file");
            goto end;
        }
        buffer[st] = 0;

        WA_DBG("Read %lli bytes at %lli, starting search\n", (long long)st, (long long)currentPos);
        WA_DBG("Current position: %lli\n", (long long)ftello(file));

        result = findLogEntryInBuffer(buffer, r, pLastLog, params);
        if(result != LFR_CANCELLED)
        {
            currentEnd = currentPos;
        }
    }

    WA_DBG("File search completed, result: %i\n", result);

end:
    if (file)
    {
        fclose(file);
        file = NULL;
    }

    return result;
}

static int checkIRLogFile(void * instanceHandle, const char * filename, const char * logPattern, json_t ** params)
{
    int result;
    regex_t r;

    {
        char searchPattern[1024];
        int rc;

        snprintf(searchPattern, sizeof(searchPattern), "^(([^ ]* ){4}).*%s", logPattern);
        if ((rc = regcomp(&r, searchPattern, REG_NEWLINE | REG_EXTENDED)) != 0)
        {
            *params = json_string("Pattern compilation error.");
            return WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR;
        }
    }

    {
        LogFindResultWithLine currentResult;
        currentResult.code = findLogEntryInFile(filename, &r, &currentResult.line, params);
        WA_DBG("First search result: %i: %s\n", (int)currentResult.code, currentResult.line);
        for (int check = 1; (check < NUMBER_OF_CHECKS) && (currentResult.code != LFR_FOUND_CURRENT) && (currentResult.code != LFR_CANCELLED); check++)
        {
            sleep(DELAY_BETWEEN_CHECKS_SEC);

            LogFindResultWithLine previousResult = currentResult;

            currentResult.code = findLogEntryInFile(filename, &r, &currentResult.line, params);
            if(currentResult.code == LFR_CANCELLED)
            {
                break;
            }
            WA_DBG("Next search result: %i: %s\n", (int)currentResult.code, currentResult.line);
            if ((currentResult.code == LFR_FOUND) && (previousResult.code == LFR_FOUND))
            {
                if (currentResult.line && previousResult.line && strcmp(currentResult.line, previousResult.line) != 0)
                {
                    currentResult.code = LFR_FOUND_CURRENT;
                }
            }

            free(previousResult.line);

            WA_DIAG_SendProgress(instanceHandle, check * 100 / NUMBER_OF_CHECKS);
        }

        switch (currentResult.code)
        {
            case LFR_FOUND_CURRENT:
                *params = json_string("IR receiver good.");
                result = WA_DIAG_ERRCODE_SUCCESS;
                break;
            case LFR_FAILURE:
            case LFR_FOUND:
                *params = json_string("IR receiver no commands received for last 10min.");
                result = WA_DIAG_ERRCODE_IR_NOT_DETECTED;
                break;
            case LFR_CANCELLED:
                *params = json_string("Test cancelled.");
                result = WA_DIAG_ERRCODE_CANCELLED;
                break;
            default:
                *params = json_string("Internal test error.");
                result = WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR;
        }

        free(currentResult.line);
    }

    regfree(&r);

    return result;
}

/*****************************************************************************
 * EXPORTED FUNCTIONS
 *****************************************************************************/

/**
 * Searches the log file /var/log/uimgr_log.txt for an occurrence of the string
 * given as @a logpattern configuration parameters (a regular expression)
 * in the last 10 minutes.
 *
 * If such a string is found but not in the last 10 minutes, the search is
 * performed again after 10 seconds.
 *
 * The test is successful if the string was found with timestamp within last
 * 10 minutes, or if the two searches resulted in different string being found.
 */
int WA_DIAG_IR_status(void *instanceHandle, void *initHandle, json_t **params)
{
    json_t * jsonConfig = NULL;
    const char * logpattern = NULL;
    const char * logfile = IR_LOG_FILE_NAME;
    int applicable;

    json_decref(*params);
    *params = NULL;

    jsonConfig = ((WA_DIAG_proceduresConfig_t*)initHandle)->config;

    /* Determine if the test is applicable: */
    if(jsonConfig && !json_unpack(jsonConfig, "{sb}", "applicable", &applicable))
    {
        if(!applicable)
        {
            WA_INFO("Device does not have IR.\n");
            *params = json_string("Not applicable.");
            return WA_DIAG_ERRCODE_NOT_APPLICABLE;
        }
    }

    /* Use log file name from config if present */
    if (jsonConfig)
        json_unpack(jsonConfig, "{ss}", "logfile", &logfile);
    
    if (!jsonConfig || (json_unpack(jsonConfig, "{ss}", "logpattern", &logpattern) != 0))
    {
        WA_ERROR("Log pattern not provided\n");
        logpattern = NULL;
    }

    if (!logpattern)
    {
        *params = json_string("Log search pattern not provided.");
        return WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR;
    }

    return checkIRLogFile(instanceHandle, logfile, logpattern, params);
}

/* End of doxygen group */
/*! @} */

/* EOF */
