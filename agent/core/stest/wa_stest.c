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
 * @file wa_stest.c
 *
 * @brief This file contains test functions.
 */

/** @addtogroup WA_STEST
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
#include "wa_osa.h"

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

/*****************************************************************************
 * LOCAL VARIABLE DECLARATIONS
 *****************************************************************************/

/*****************************************************************************
 * FUNCTION DEFINITIONS
 *****************************************************************************/

extern int WA_STEST_OSA_Run(void);
extern int WA_STEST_ID_Run(void);

int WA_STEST_Run(int i)
{
    int status = -1;
    
    (void) i;
    
    WA_ENTER("WA_STEST_Run()\n");
    
    status = WA_STEST_OSA_Run();
    if(status !=0)
    {
        WA_ERROR("WA_STEST_OSA_Run(): %d\n", status);
        goto end;
    }
    WA_INFO("WA_STEST_Run(): WA_STEST_OSA_Run(): PASS\n");
    
    status = WA_STEST_ID_Run();
    if(status !=0)
    {
        WA_ERROR("WA_STEST_ID_Run(): %d\n", status);
        goto end;
    }
    WA_INFO("WA_STEST_Run(): WA_STEST_ID_Run(): PASS\n");
end:
    WA_RETURN("WA_STEST_Run():%d\n", status);
    return status;
}



/*****************************************************************************
 * LOCAL FUNCTIONS
 *****************************************************************************/

/* End of doxygen group */
/*! @} */

/* EOF */
