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
 * @brief File storage verification functions - implementation
 */

/** @addtogroup WA_DIAG_FILE
 *  @{
 */

/*****************************************************************************
 * STANDARD INCLUDE FILES
 *****************************************************************************/

#define _GNU_SOURCE

#include <stdbool.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/

#include "wa_diag.h"
#include "wa_debug.h"

/*****************************************************************************
 * GLOBAL VARIABLE DEFINITIONS
 *****************************************************************************/

/*****************************************************************************
 * LOCAL DEFINITIONS
 *****************************************************************************/

/*****************************************************************************
 * LOCAL TYPES
 *****************************************************************************/

/*****************************************************************************
 * LOCAL FUNCTION PROTOTYPES
 *****************************************************************************/

static bool verifyPattern(const char * filename, char * buffer, char * compare, size_t totalSize, char * pattern, size_t patternSize);
static void setupBuffer(char * buffer, size_t totalSize, const char * pattern, size_t patternSize);
static bool storeBuffer(const char * filename, char * buffer, size_t totalSize);
static bool loadBuffer(const char * filename, char * buffer, size_t totalSize);

/*****************************************************************************
 * LOCAL VARIABLE DECLARATIONS
 *****************************************************************************/

/*****************************************************************************
 * FUNCTION DEFINITIONS
 *****************************************************************************/

/*****************************************************************************
 * EXPORTED FUNCTIONS
 *****************************************************************************/

int WA_DIAG_FileTest(const char * defaultFileName, size_t defaultTotalSize, json_t * jsonConfig, json_t ** pJsonOut)
{
    static const char constantPatterns[] = {0x55, 0xAA};

    int result = WA_DIAG_ERRCODE_FAILURE;
    char pattern[256];
    int totalSize = 0;
    const char * filename = NULL;

    if (!jsonConfig || (json_unpack(jsonConfig, "{si}", "filesize", &totalSize) != 0) || (totalSize <= 0))
    {
        totalSize = defaultTotalSize;
    }
    if (!jsonConfig || (json_unpack(jsonConfig, "{ss}", "filename", &filename) != 0))
    {
        filename = defaultFileName;
    }

    WA_DBG("Using file: %s, size: %i\n", (char*)filename, (int)totalSize);

    if (!filename || (totalSize <= 0))
    {
        if (pJsonOut)
        {
            WA_ERROR("Invalid arguments\n");
            *pJsonOut = json_string("Internal test error.");
        }
        return WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR;
    }

    char * compare = (char *)malloc(totalSize);
    char * buffer = (char *)malloc(totalSize);

    if (!buffer || !compare)
    {
        WA_ERROR("Unable to allocate buffers\n");
        goto free_exit;
    }

    {
        size_t i;
        for (i = 0; i < sizeof(constantPatterns) / sizeof(constantPatterns[0]); ++i)
        {
            if(WA_OSA_TaskCheckQuit())
            {
                result = WA_DIAG_ERRCODE_CANCELLED;
                goto free_exit;
            }

            memset(pattern, constantPatterns[i], sizeof(pattern));
            if (!verifyPattern(filename, buffer, compare, totalSize, pattern, sizeof(pattern)))
            {
                goto free_exit;
            }
        }
    }

    result = WA_DIAG_ERRCODE_SUCCESS;

free_exit:
    free(compare);
    free(buffer);

    unlink(filename);

    if(pJsonOut)
    {
        switch(result)
        {
            case WA_DIAG_ERRCODE_SUCCESS:
                *pJsonOut = json_string("Memory good.");
                break;

            case WA_DIAG_ERRCODE_FAILURE:
                *pJsonOut = json_string("Memory error.");
                break;

            case WA_DIAG_ERRCODE_CANCELLED:
                *pJsonOut = json_string("Test cancelled.");
                break;

            default:
                *pJsonOut = json_string("Internal test error.");
                break;
        }
    }

    WA_RETURN("Returning: %i\n", result);
    return result;
}

/*****************************************************************************
 * LOCAL FUNCTIONS
 *****************************************************************************/

static bool verifyPattern(const char * filename, char * buffer, char * compare, size_t totalSize, char * pattern, size_t patternSize)
{
    if(WA_OSA_TaskCheckQuit())
    {
        return false;
    }

    setupBuffer(buffer, totalSize, pattern, patternSize);

    if(WA_OSA_TaskCheckQuit())
    {
        return false;
    }

    if (!storeBuffer(filename, buffer, totalSize))
    {
        WA_DBG("Unable to write file\n");
        return false;
    }

    if(WA_OSA_TaskCheckQuit())
    {
        return false;
    }

    if (!loadBuffer(filename, compare, totalSize))
    {
        WA_DBG("Unable to read file\n");
        return false;
    }

    if(WA_OSA_TaskCheckQuit())
    {
        return false;
    }

    if (memcmp(buffer, compare, totalSize) != 0)
    {
        WA_DBG("Content mismatch\n");
        return false;
    }

    if(WA_OSA_TaskCheckQuit())
    {
        return false;
    }

    return true;
}

static void setupBuffer(char * buffer, size_t totalSize, const char * pattern, size_t patternSize)
{
    size_t pos = 0;
    size_t count = totalSize / patternSize;
    for (pos = 0; count > 0; count--, pos += patternSize)
    {
        if(WA_OSA_TaskCheckQuit())
        {
            return;
        }
        memcpy(buffer + pos, pattern, patternSize);
    }
    if (pos < totalSize)
    {
        memcpy(buffer + pos, pattern, totalSize - pos);
    }
}

static bool storeBuffer(const char * filename, char * buffer, size_t totalSize)
{
    int f = open(filename, O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR);
    if (f == -1)
    {
        WA_DBG("Unable to create file: %i (%s)\n", errno, strerror(errno));
        return false;
    }

    size_t processed = 0;
    while (processed < totalSize)
    {
        if(WA_OSA_TaskCheckQuit())
        {
            close(f);
            return false;
        }

        int rc = write(f, buffer + processed, totalSize - processed);
        if (rc <= 0)
        {
            WA_DBG("Unable to write: %i (%s)\n", errno, strerror(errno));
            close(f);
            return false;
        }
        processed += rc;
    }

    fsync(f);
    posix_fadvise(f, 0, 0, POSIX_FADV_DONTNEED);
    close(f);
    return true;
}

static bool loadBuffer(const char * filename, char * buffer, size_t totalSize)
{
    int f = open(filename, O_RDONLY);
    if (f == -1)
    {
        WA_DBG("Unable to open file: %i (%s)\n", errno, strerror(errno));
        return false;
    }

    size_t processed = 0;
    while (processed < totalSize)
    {
        if(WA_OSA_TaskCheckQuit())
        {
            close(f);
            return false;
        }

        int rc = read(f, buffer + processed, totalSize - processed);
        if (rc == -1)
        {
            WA_DBG("Unable to write: %i (%s)\n", errno, strerror(errno));
            close(f);
            return false;
        }
        else if (rc == 0)
        {
            WA_DBG("Unexpected end of input: %i (%s)\n", errno, strerror(errno));
            close(f);
            return false;
        }
        processed += rc;
    }

    close(f);
    return true;
}


/* End of doxygen group */
/*! @} */

