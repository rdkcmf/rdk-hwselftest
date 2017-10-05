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
 * @file wa_stest_osa.c
 *
 * @brief This file contains ID test functions.
 */

/** @addtogroup WA_STEST
 *  @{
 */

/*****************************************************************************
 * STANDARD INCLUDE FILES
 *****************************************************************************/

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/
#include "wa_debug.h"
#include "wa_id.h"

/*****************************************************************************
 * GLOBAL VARIABLE DEFINITIONS
 *****************************************************************************/

/*****************************************************************************
 * LOCAL DEFINITIONS
 *****************************************************************************/
#define PREPROCESSOR_ASSERT(x) (void)(sizeof(char[-!(x)]))

/*****************************************************************************
 * LOCAL TYPES
 *****************************************************************************/

/*****************************************************************************
 * LOCAL FUNCTION PROTOTYPES
 *****************************************************************************/

/*****************************************************************************
 * LOCAL VARIABLE DECLARATIONS
 *****************************************************************************/

/*****************************************************************************
 * FUNCTION DEFINITIONS
 *****************************************************************************/

int WA_STEST_ID_Run(void)
{
    int status = 0;
    int i;
    WA_UTILS_ID_t id;

    PREPROCESSOR_ASSERT((WA_UTILS_ID_t)(-1) == WA_UTILS_LIST_MAX_LIST_SIZE-1);

    WA_ENTER("WA_STEST_ID_Run()\n");

    status = WA_UTILS_ID_Init();
    if(status != 0)
    {
        WA_ERROR("WA_STEST_ID_Run(): WA_UTILS_ID_Init():%d\n", status);
        goto end;
    }

    WA_INFO("WA_STEST_ID_Run() step #1\n");
    for(i=1; i<=(WA_UTILS_ID_t)(-1); ++i)
    {
        status = WA_UTILS_ID_Generate(&id);
        if(status != 0)
        {
            WA_ERROR("WA_STEST_ID_Run(): WA_UTILS_ID_Generate():%d\n", status);
            goto end;
        }
        if(id != i)
        {
            WA_ERROR("WA_STEST_ID_Run(): invalid id:%d\n", id);
            status = -1;
            goto end;
        }
    }

    WA_INFO("WA_STEST_ID_Run() step #2\n");
    status = WA_UTILS_ID_Generate(&id);
    if(status != 0)
    {
        WA_ERROR("WA_STEST_ID_Run(): WA_UTILS_ID_Generate():%d\n", status);
        goto end;
    }
    if(id != 0)
    {
        WA_ERROR("WA_STEST_ID_Run(): invalid id:%d\n", id);
        status = -1;
        goto end;
    }

    WA_INFO("WA_STEST_ID_Run() step #3\n");
    status = WA_UTILS_ID_Generate(&id);
    if(status == 0)
    {
        WA_ERROR("WA_STEST_ID_Run(): should not generate id\n");
        status = -1;
        goto end;
    }

    WA_INFO("WA_STEST_ID_Run() step #4\n");
    WA_UTILS_ID_Release(2);

    WA_INFO("WA_STEST_ID_Run() step #5\n");
    status = WA_UTILS_ID_Generate(&id);
    if(status != 0)
    {
        WA_ERROR("WA_STEST_ID_Run(): WA_UTILS_ID_Generate():%d\n", status);
        goto end;
    }
    if(id != 2)
    {
        WA_ERROR("WA_STEST_ID_Run(): invalid id:%d\n", id);
        status = -1;
        goto end;
    }

    WA_INFO("WA_STEST_ID_Run() step #6\n");
    WA_UTILS_ID_Release(1);

    WA_INFO("WA_STEST_ID_Run() step #7\n");
    status = WA_UTILS_ID_Generate(&id);
    if(status != 0)
    {
        WA_ERROR("WA_STEST_ID_Run(): WA_UTILS_ID_Generate():%d\n", status);
        goto end;
    }
    if(id != 1)
    {
        WA_ERROR("WA_STEST_ID_Run(): invalid id:%d\n", id);
        status = -1;
        goto end;
    }

    status = WA_UTILS_ID_Exit();
    if(status != 0)
    {
        WA_ERROR("WA_INIT_Init(): WA_UTILS_ID_Exit():%d\n", status);
    }

end:
    WA_RETURN("WA_STEST_ID_Run():%d\n", status);
    return status;
}



/*****************************************************************************
 * LOCAL FUNCTIONS
 *****************************************************************************/

/* End of doxygen group */
/*! @} */

/* EOF */
