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
 * @file wa_iarm.cpp
 *
 * @brief This file contains interface functions for rdk iarm module.
 */

/** @addtogroup WA_UTILS_IARM
 *  @{
 */

/*****************************************************************************
 * STANDARD INCLUDE FILES
 *****************************************************************************/

#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>


/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/
#include "wa_debug.h"
#include "wa_osa.h"

/*****************************************************************************
 * RDK-SPECIFIC INCLUDE FILES
 *****************************************************************************/
#include "host.hpp"
#include "videoOutputPort.hpp"
#include "videoOutputPortType.hpp"
#include "videoResolution.hpp"
#include "manager.hpp"

#include "libIBus.h"
#include "dsMgr.h"
#include "wa_iarm.h"

/*****************************************************************************
 * GLOBAL VARIABLE DEFINITIONS
 *****************************************************************************/

/*****************************************************************************
 * LOCAL DEFINITIONS
 *****************************************************************************/

#define WA_UTILS_IARM_BUS_RDKSELFTEST_NAME "RDK_Self_Test"

typedef enum {
    WA_UTILS_IARM_IFCE_NOT_CONNECTED,
    WA_UTILS_IARM_IFCE_CONNECTED,
} WA_UTILS_IARM_LinkState_t;

typedef struct {
    void *mutex;
    WA_UTILS_IARM_LinkState_t state;
    unsigned count;
} WA_UTILS_IARM_ConnState_t;

/*****************************************************************************
 * LOCAL FUNCTION PROTOTYPES
 *****************************************************************************/

/*****************************************************************************
 * LOCAL VARIABLE DECLARATIONS
 *****************************************************************************/

static bool iarmInitialized;
static WA_UTILS_IARM_ConnState_t conn;

/*****************************************************************************
 * FUNCTION DEFINITIONS
 *****************************************************************************/

/*****************************************************************************
 * LOCAL FUNCTIONS
 *****************************************************************************/

/*****************************************************************************
 * EXPORTED FUNCTIONS
 *****************************************************************************/

int WA_UTILS_IARM_Init(void)
{
    char thName [WA_OSA_TASK_NAME_MAX_LEN];

    int getNameRes = WA_OSA_TaskGetName(thName);

    if(!iarmInitialized)
    {
        conn.mutex = WA_OSA_MutexCreate();

        if(!conn.mutex)
        {
            WA_ERROR("WA_UTILS_IARM_Init, WA_OSA_MutexCreate error!\n");
            return -1;
        }

        /*rename current thread to make threads created by arm bus distingushable */
        WA_OSA_TaskSetName("WAiarmbusifce");

        IARM_Result_t ret = IARM_Bus_Init(WA_UTILS_IARM_BUS_RDKSELFTEST_NAME);

        /*recover thread name as it was before*/
        if (getNameRes == 0)
        {
            WA_OSA_TaskSetName(thName);
        }
        else
        {
            /* use default value if for any reason we do not have initial value at this stage */
            WA_OSA_TaskSetName("hwselftest");
        }

        if(ret != IARM_RESULT_SUCCESS)
        {
            WA_ERROR("WA_UTILS_IARM_Init, IARM_Bus_Init, error code: %d\n", ret);

            int state = WA_OSA_MutexDestroy(conn.mutex);

            if(state)
            {
                WA_ERROR("WA_UTILS_IARM_Init, WA_OSA_MutexDestroy, error code: %d\n", state);
            }
        }
        else
        {
            iarmInitialized = true;
        }
    }

    return (iarmInitialized ? 0 : -1);
}

int WA_UTILS_IARM_Term(void)
{
    if(iarmInitialized)
    {

        IARM_Result_t ret = IARM_Bus_Term();
        if(ret != IARM_RESULT_SUCCESS)
        {
            WA_ERROR("DIAG_UTILS_IarmTerm: IARM_Bus_Term, error code: %d\n", ret);
            return -1;
        }

        if(conn.mutex)
        {
            int state = WA_OSA_MutexDestroy(conn.mutex);

            if(state)
            {
                WA_ERROR("DIAG_UTILS_IarmTerm: WA_OSA_MutexDestroy, error code: %d\n", state);
            }
            else
            {
                iarmInitialized = false;
            }
        }
    }

    return (iarmInitialized ? -1 : 0);
}

int WA_UTILS_IARM_Connect(void)
{
    int status = 0;

    if(!iarmInitialized)
    {
        return -1;
    }

    int state = WA_OSA_MutexLock(conn.mutex);
    if(state)
    {
        WA_ERROR("WA_UTILS_IARM_Connect: WA_OSA_MutexLock, error code: %d\n", state);
        return -1;
    }

    switch(conn.state)
    {
    case WA_UTILS_IARM_IFCE_NOT_CONNECTED:
    {
        IARM_Result_t ret = IARM_Bus_Connect();
        if(ret != IARM_RESULT_SUCCESS)
        {
            WA_ERROR("WA_UTILS_IARM_Connect: IARM_Bus_Connect, error code: %d\n", ret);
            status = -1;
            break;
        }

        conn.count = 0;
        conn.state = WA_UTILS_IARM_IFCE_CONNECTED;
    }

    case WA_UTILS_IARM_IFCE_CONNECTED:
    {
        conn.count += 1;
        break;
    }

    default:
        break;
    }

    state = WA_OSA_MutexUnlock(conn.mutex);
    if(state)
    {
        WA_ERROR("WA_UTILS_IARM_Connect: WA_OSA_MutexUnlock(): %d\n", state);
        return -1;
    }

    return status;
}

int WA_UTILS_IARM_Disconnect(void)
{
    int status = 0;

    if(!iarmInitialized)
    {
        return -1;
    }

    int state = WA_OSA_MutexLock(conn.mutex);
    if(state)
    {
        WA_ERROR("WA_UTILS_IARM_Disconnect: WA_OSA_MutexLock, error code: %d\n", state);
        return -1;
    }

    switch(conn.state)
    {
    case WA_UTILS_IARM_IFCE_CONNECTED:
    {
        if(conn.count > 0)
        {
            conn.count -= 1;
        }

        if(conn.count == 0)
        {

            IARM_Result_t ret = IARM_Bus_Disconnect();
            if(ret != IARM_RESULT_SUCCESS)
            {
                conn.count = 1;
                WA_ERROR("WA_UTILS_IARM_Disconnect: IARM_Bus_Disconnect, error code: %d\n", ret);
                status = -1;
            }
            else
            {
                conn.state = WA_UTILS_IARM_IFCE_NOT_CONNECTED;
            }
        }
        break;
    }

    case WA_UTILS_IARM_IFCE_NOT_CONNECTED:
    default:
        break;
    }

    state = WA_OSA_MutexUnlock(conn.mutex);
    if(state)
    {
        WA_ERROR("WA_UTILS_IARM_Disconnect: WA_OSA_MutexUnlock, error code: %d\n", state);
        return -1;
    }

    return status;
}

/* End of doxygen group */
/*! @} */

/* EOF */
