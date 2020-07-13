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
 * @file wa_sicache.c
 *
 * @brief This file contains interface functions for rdk si cache.
 */

/** @addtogroup WA_UTILS_SICACHE
 *  @{
 */

/*****************************************************************************
 * STANDARD INCLUDE FILES
 *****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/
#include "wa_sicache.h"
#include "wa_debug.h"
#include "wa_osa.h"
#include "wa_fileops.h"

/*****************************************************************************
 * RDK-SPECIFIC INCLUDE FILES
 *****************************************************************************/

/*****************************************************************************
 * GLOBAL VARIABLE DEFINITIONS
 *****************************************************************************/

/*****************************************************************************
 * LOCAL DEFINITIONS
 *****************************************************************************/

#define PARSER_COMMAND "/usr/bin/si_cache_parser_121 %s %s"

#define MAX_LINE 256

#define PROPERTIES_FILE_PATH  "/etc/rmfconfig.ini"
#define SICACHE_ENTRY         "SITP.SI.CACHE.LOCATION="
#define SNSCACHE_ENTRY        "SITP.SNS.CACHE.LOCATION="

/*****************************************************************************
 * LOCAL FUNCTION PROTOTYPES
 *****************************************************************************/

/*****************************************************************************
 * LOCAL VARIABLE DECLARATIONS
 *****************************************************************************/
static int luckyId = -1;
static char* tuneParam = NULL;

/*****************************************************************************
 * FUNCTION DEFINITIONS
 *****************************************************************************/


/*****************************************************************************
 * EXPORTED FUNCTIONS
 *****************************************************************************/
int WA_UTILS_SICACHE_TuningGetLuckyId()
{
    WA_DBG("WA_UTILS_SICACHE_TuningGetLuckyId(): %d\n", luckyId >= 0 ? luckyId : -1);
    return luckyId >= 0 ? luckyId : -1;
}

void WA_UTILS_SICACHE_TuningSetLuckyId(int id)
{
    luckyId = id;
    WA_DBG("WA_UTILS_SICACHE_TuningSetLuckyId(): %d\n", luckyId);
}

void WA_UTILS_SICACHE_TuningSetTuneData(char *tuneData)
{
    tuneParam = NULL;
    if (tuneData && strcmp(tuneData, "") != 0)
    {
        tuneParam = tuneData;
    }
    WA_DBG("WA_UTILS_SICACHE_TuningSetTuneData(): %s\n", tuneParam);
}

int WA_UTILS_SICACHE_TuningRead(int numEntries, WA_UTILS_SICACHE_Entries_t **ppEntries)
{
    FILE *f;
    char line[MAX_LINE];
    int numFound = -1;
    char *c, *sicache=NULL, *snscache=NULL;
    bool skipFirst = true;

    WA_ENTER("WA_UTILS_SICACHE_TuningRead(numEntries=%d pEntries=%p)\n", numEntries, ppEntries);

    if((numEntries <= 0) || (ppEntries == NULL))
    {
        WA_ERROR("WA_UTILS_SICACHE_TuningRead() invalid parameter.\n");
        goto end;
    }

    sicache = WA_UTILS_FILEOPS_OptionFind(PROPERTIES_FILE_PATH, SICACHE_ENTRY);
    if(WA_OSA_TaskCheckQuit())
    {
        WA_DBG("WA_UTILS_SICACHE_TuningRead: WA_UTILS_FILEOPS_OptionFind: test cancelled\n");
        goto end;
    }

    if(sicache == NULL)
    {
        WA_ERROR("WA_UTILS_SICACHE_TuningRead() unable to find si cache path\n");
        goto end;
    }

    snscache = WA_UTILS_FILEOPS_OptionFind(PROPERTIES_FILE_PATH, SNSCACHE_ENTRY);
    if(WA_OSA_TaskCheckQuit())
    {
        WA_DBG("WA_UTILS_SICACHE_TuningRead: WA_UTILS_FILEOPS_OptionFind: test cancelled\n");
        goto end;
    }

    if(snscache == NULL)
    {
        WA_ERROR("WA_UTILS_SICACHE_TuningRead() unable to find sns cache path\n");
        goto end;
    }

    *ppEntries = malloc(numEntries * sizeof(WA_UTILS_SICACHE_Entries_t));
    if(*ppEntries == NULL)
    {
        WA_ERROR("WA_UTILS_SICACHE_TuningRead() malloc() error\n");
        goto end;
    }

    snprintf(line, MAX_LINE, PARSER_COMMAND, sicache, snscache);

    f = popen(line, "r");
    if(f == NULL)
    {
        WA_ERROR("WA_UTILS_SICACHE_TuningRead() popen() error\n");
        goto end;
    }

    if (tuneParam)
    {
        numEntries = 1;
        skipFirst = false;
    }

    numFound = 0;
    while(fgets(line, sizeof(line)-1, f) != NULL)
    {
        if(WA_OSA_TaskCheckQuit())
        {
            WA_DBG("WA_UTILS_SICACHE_TuningRead: fgets: test cancelled\n");
            break;
        }

        if (tuneParam)
        {
            c = strstr(line, tuneParam);
            if (!c)
                continue;
        }

        /* Example line:
         * RChannelVCN#000001-SRCID#000001-Name[(null)]-State-[4]-Freq[699000000]-Mode[0016]-Prog[00000003]
        */
        c = strstr(line, "]-Freq[");
        if(c)
        {
            if(sscanf(c+strlen("]-Freq["), "%u]-Mode[%u]-Prog[%u]", 
               &((*ppEntries)[numFound].freq),
               &((*ppEntries)[numFound].mod),
               &((*ppEntries)[numFound].prog)) == 3)
            {

                if (skipFirst)
                {
                    WA_DBG("WA_UTILS_SICACHE_TuningRead: skipping first SI entry\n");
                    skipFirst = false;
                    continue;
                }

                WA_DBG("WA_UTILS_SICACHE_TuningRead: SI#%d: freq=%d mod=%d prog=%d\n", numFound,
                       (*ppEntries)[numFound].freq,
                       (*ppEntries)[numFound].mod,
                       (*ppEntries)[numFound].prog);

                ++numFound;
                if(numFound == numEntries)
                {
                    break;
                }
            }
        }
    }

    pclose(f);
end:
    free(sicache);
    free(snscache);

    WA_RETURN("WA_UTILS_SICACHE_TuningRead(): %d\n", numFound);
    return numFound;
}

/*****************************************************************************
 * LOCAL FUNCTIONS
 *****************************************************************************/

/* End of doxygen group */
/*! @} */

/* EOF */
