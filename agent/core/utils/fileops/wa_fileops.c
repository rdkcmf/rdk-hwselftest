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
 * @file wa_fileops.c
 *
 * @brief This file contains functions for reading options from file.
 */

/** @addtogroup WA_FILEOPS
 *  @{
 */

/*****************************************************************************
 * STANDARD INCLUDE FILES
 *****************************************************************************/

#define _GNU_SOURCE
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/
#include "wa_fileops.h"
#include "wa_debug.h"
#include "wa_osa.h"

/*****************************************************************************
 * GLOBAL VARIABLE DEFINITIONS
 *****************************************************************************/

/*****************************************************************************
 * LOCAL DEFINITIONS
 *****************************************************************************/
#define LINE_LEN 256

/*****************************************************************************
 * LOCAL TYPES
 *****************************************************************************/

typedef struct {
    char *pos;
    char buff[LINE_LEN];
} buff;


/*****************************************************************************
 * LOCAL FUNCTION PROTOTYPES
 *****************************************************************************/
static char *get_line_with_str(FILE *file, const char *str_in, char *str_out, int line_len);
static char *get_last_line_with_str(FILE *file, const char *str_in, char *str_out, int line_len);

/*****************************************************************************
 * LOCAL VARIABLE DECLARATIONS
 *****************************************************************************/

/*****************************************************************************
 * FUNCTION DEFINITIONS
 *****************************************************************************/

/*****************************************************************************
 * LOCAL FUNCTIONS
 *****************************************************************************/

static char *get_line_with_str(FILE *file, const char *str_in, char *str_out, int line_len)
{
    char *pos = NULL, *hpos;
    int line_number = 0;

    if(file == NULL || str_in == NULL || str_out == NULL || line_len < 1)
    {
        return NULL;
    }

    while(fgets(str_out, line_len, file) != NULL && !WA_OSA_TaskCheckQuit())
    {
        line_number += 1;

        pos = (char *)strcasestr(str_out, str_in);
        if(pos != NULL)
        {
            hpos = (char *)strchr(str_out, '#');
            if((hpos == NULL) || (hpos > pos))
                return pos;
        }
    }

    return NULL;
}

static char *get_last_line_with_str(FILE *file, const char *str_in, char *str_out, int line_len)
{
    int id = 1;

    if(file == NULL || str_in == NULL || str_out == NULL || line_len < 1)
    {
        return NULL;
    }

    buff line_buff[2];
    (void)memset(line_buff, 0x00, sizeof(line_buff));

    do
    {
        id += 1;
        id %= 2;

        line_buff[id].pos = get_line_with_str(file, str_in, line_buff[id].buff, line_len);

    }while(line_buff[id].pos && !WA_OSA_TaskCheckQuit());

    if(WA_OSA_TaskCheckQuit())
    {
        return NULL;
    }

    id += 1;
    id %= 2;

    if(line_buff[id].pos)
    {
        memcpy(str_out, line_buff[id].buff, line_len);
        return line_buff[id].pos;
    }

    return NULL;
}

/*****************************************************************************
 * EXPORTED FUNCTIONS
 *****************************************************************************/

int WA_UTILS_FILEOPS_OptionSupported(const char *file, const char *mode, const char *option, const char *exp_value)
{
    FILE *fd;
    char *opt, *val;
    char buff[LINE_LEN];

    if(file == NULL || mode == NULL || option == NULL || exp_value == NULL)
    {
        return -1;
    }

    (void)memset(buff, 0x00, sizeof(buff));

    fd = fopen(file, mode);
    if(fd == NULL)
    {
        return -1;
    }

    opt = get_last_line_with_str(fd, option, buff, LINE_LEN);
    fclose(fd);

    if(WA_OSA_TaskCheckQuit())
    {
        WA_DBG("WA_UTILS_FILEOPS_OptionSupported: test cancelled\n");
        return -1;
    }

    if(opt)
    {
        val = strcasestr(buff, exp_value);
        if(val)
        {
            return 1;
        }
        else
        {
            return 0;
        }
    }

    return -1;
}

char *WA_UTILS_FILEOPS_OptionFind(const char *fname, const char *pattern)
{
    FILE* fd;
    char buf[LINE_LEN];
    char format[LINE_LEN];
    char result[LINE_LEN];
    char *line = NULL;

    if ((fd = fopen(fname,"r")) == NULL)
    {
        return NULL;
    }

    snprintf(format, LINE_LEN, "%s%%%ds", pattern, LINE_LEN);

    while(fgets(buf, LINE_LEN, fd) && !WA_OSA_TaskCheckQuit())
    {
        if (sscanf(buf, format, result) == 1)
        {
            line = strdup(result);
            break;
        }
    }

    if(WA_OSA_TaskCheckQuit())
    {
        WA_DBG("WA_UTILS_FILEOPS_OptionFind: test cancelled\n");
    }

    fclose(fd);

    return line;
}

char **WA_UTILS_FILEOPS_OptionFindMultiple(const char *fname, const char *pattern, int maxEntries)
{
    FILE* fd;
    char buf[LINE_LEN];
    char format[LINE_LEN];
    char result[LINE_LEN];
    char **multiple;
    int entries = 0;

    multiple = calloc((maxEntries+1), sizeof(char *));
    if(!multiple)
    {
        return NULL;
    }

    if ((fd = fopen(fname,"r")) == NULL)
    {
        return NULL;
    }

    snprintf(format, LINE_LEN, "%s%%%ds", pattern, LINE_LEN);
    while(fgets(buf, LINE_LEN, fd) && !WA_OSA_TaskCheckQuit())
    {
        if (sscanf(buf, format, result) == 1)
        {
            multiple[entries] = strdup(result);
            if((multiple[entries] == NULL) || (++entries == maxEntries))
            {
                break;
            }
        }
    }

    if(WA_OSA_TaskCheckQuit())
    {
        WA_DBG("WA_UTILS_FILEOPS_OptionFindMultiple: test cancelled\n");
    }

    fclose(fd);

    return multiple;
}

void WA_UTILS_FILEOPS_OptionFindMultipleFree(char **multiple)
{
    char **e;

    if(multiple == NULL)
    {
        return;
    }

    for(e=multiple; *e; ++e)
    {
        free(*e);
    }
    free(multiple);
}

/* End of doxygen group */
/*! @} */

/* EOF */
