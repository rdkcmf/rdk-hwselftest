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
 * @file wa_trh.c
 *
 * @brief This file contains interface functions tuner reservation helper.
 */

/** @addtogroup WA_UTILS_TRH
 *  @{
 */

/*****************************************************************************
 * STANDARD INCLUDE FILES
 *****************************************************************************/
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sys/time.h>

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/
#include "wa_osa.h"
#include "wa_debug.h"
#include "wa_trh.h"
#include "wa_trh_listener.h"

/*****************************************************************************
 * RDK-SPECIFIC INCLUDE FILES
 *****************************************************************************/
#include "TunerReservationHelper.h"

/*****************************************************************************
 * LOCAL DEFINITIONS
 *****************************************************************************/
#define MAX_TUNER_RESERVATION_HELPERS 6

/*****************************************************************************
 * LOCAL VARIABLE DECLARATIONS
 *****************************************************************************/
class TrhHandler
{
public:
    TrhHandler(const std::string &url)
        : locator(url)
    {
        listener = new reservation_listener_impl(url);
        helper = new TunerReservationHelper(listener);
    }

    ~TrhHandler()
    {
        delete helper;
        delete listener;
    }

    std::string locator;
    TunerReservationHelper *helper;
    reservation_listener_impl *listener;
};

/*****************************************************************************
 * EXPORTED FUNCTIONS
 *****************************************************************************/

int WA_UTILS_TRH_Init()
{
    int status = 0;


    return status;
}

int WA_UTILS_TRH_Exit()
{
    int status = 0;


    return status;
}

int WA_UTILS_TRH_ReserveTuner(const char *url, uint64_t when, uint64_t duration, int timeout_ms, void **out_handle)
{
    int status = 1;

    WA_ENTER("WA_UTILS_TRH_ReserveTuner(): url='%s', when=%llu, duration=%llu, timeout_ms=%i, out_handle=%p\n",
                url, when, duration, timeout_ms, out_handle);

    try
    {
        WA_DBG("WA_UTILS_TRH_ReserveTuner(): requesting tuner reservation for '%s' from TRM...\n", url);
        TrhHandler *handler = new TrhHandler(url);

        if (!when)
        {
            struct timeval timeNow;
            gettimeofday(&timeNow, NULL);
            when = (uint64_t)timeNow.tv_sec * 1000LL; /* reserve now */
        }

        if (handler->helper->reserveTunerForLive(url, when, duration, false /* async call */))
        {
            WA_DBG("WA_UTILS_TRH_ReserveTuner(): waiting for tuner reservation for '%s'...\n", url);

            handler->listener->wait_reserve(timeout_ms);
            status = !(handler->listener->get_status() == TRH_STATUS_RESERVATION_SUCCESS)
                       || (handler->listener->get_status() == TRH_STATUS_RELEASED);

            if (!status)
            {
                WA_DBG("WA_UTILS_TRH_ReserveTuner(): reservation successful for '%s'\n", url);

                if (out_handle)
                    *out_handle = handler;
            }
            else
            {
                if (handler->listener->get_status() == TRH_STATUS_RESERVATION_FAIL)
                    WA_ERROR("WA_UTILS_TRH_ReserveTuner(): reservation failure for '%s'\n", url);
                else
                {
                    WA_ERROR("WA_UTILS_TRH_ReserveTuner(): reservation timeout for '%s'\n", url);
                    status = 2;
                }
            }
        }
        else
            WA_ERROR("WA_UTILS_TRH_ReserveTuner(): failed to request tuner reservation\n");
    }
    catch (std::exception &e)
    {
        WA_ERROR("WA_UTILS_TRH_ReserveTuner(): failed to alloc reservation helper (%s)\n", e.what());
        status = 1;
    }

err:
    WA_RETURN("WA_UTILS_TRH_ReserveTuner(): status=%i\n", status);

    return status;
}

int WA_UTILS_TRH_ReleaseTuner(void *handle, int timeout_ms)
{
    int status = 1;

    WA_ENTER("WA_UTILS_TRH_ReleaseTuner(): handle=%p, timeout_ms=%i\n", handle, timeout_ms);

    if (handle)
    {
        TrhHandler *handler = reinterpret_cast<TrhHandler *>(handle);

        status = !(handler->listener->get_status() == TRH_STATUS_RESERVATION_RELEASE_SUCCESS)
                       || (handler->listener->get_status() == TRH_STATUS_RELEASED);
        if (status)
        {
            if (handler->helper->releaseTunerReservation())
            {
                WA_DBG("WA_UTILS_TRH_ReleaseTuner(): waiting for reservation release for '%s'...\n", handler->locator.c_str());

                handler->listener->wait_reserve_release(timeout_ms);
                status = !((handler->listener->get_status() == TRH_STATUS_RESERVATION_RELEASE_SUCCESS)
                           || (handler->listener->get_status() == TRH_STATUS_RELEASED));

                if (!status)
                    WA_DBG("WA_UTILS_TRH_ReleaseTuner(): reservation release successful for '%s'\n", handler->locator.c_str());
                else
                {
                    if (handler->listener->get_status() == TRH_STATUS_RESERVATION_RELEASE_FAIL)
                        WA_ERROR("WA_UTILS_TRH_ReleaseTuner(): reservation release failure for '%s'\n", handler->locator.c_str());
                    else
                    {
                        WA_ERROR("WA_UTILS_TRH_ReleaseTuner(): reservation release timeout for '%s'\n", handler->locator.c_str());
                        status = 2;
                    }
                }
            }
            else
                WA_ERROR("WA_UTILS_TRH_ReleaseTuner(): failed to request reservation release for '%s'\n", handler->locator.c_str());
        }
        else
            WA_DBG("WA_UTILS_TRH_ReleaseTuner(): reservation for '%s' already released\n", handler->locator.c_str());

        delete handler;

        WA_DBG("WA_UTILS_TRH_ReleaseTuner(): deallocated handle %p\n", handle);
    }
    else
        WA_ERROR("WA_UTILS_TRH_ReleaseTuner(): invalid reservation handle %p\n", handle);

err:
    WA_RETURN("WA_UTILS_TRH_ReleaseTuner(): status=%i\n", status);

    return status;
}

int WA_UTILS_TRH_WaitForTunerRelease(void *handle, int timeout_ms)
{
    int status = 1;

    WA_ENTER("WA_UTILS_TRH_WaitForTunerRelease(): handle=%p, timeout_ms=%i\n", handle, timeout_ms);

    if (handle)
    {
        TrhHandler *handler = reinterpret_cast<TrhHandler *>(handle);

        WA_DBG("WA_UTILS_TRH_WaitForTunerRelease(): waiting for tuner release for '%s'...\n", handler->locator.c_str());
        handler->listener->wait_release(timeout_ms);
        status = !(handler->listener->get_status() == TRH_STATUS_RELEASED);

        if (!status)
            WA_DBG("WA_UTILS_TRH_WaitForTunerRelease(): tuner release successful for '%s'\n", handler->locator.c_str());
        else
        {
            WA_ERROR("WA_UTILS_TRH_WaitForTunerRelease(): tuner release time out for '%s'\n", handler->locator.c_str());
            status = 2;
        }
    }
    else
        WA_ERROR("WA_UTILS_TRH_WaitForTunerRelease(): invalid reservation handle %p\n", handle);

err:
    WA_RETURN("WA_UTILS_TRH_WaitForTunerRelease(): status=%i\n", status);

    return status;
}

/* End of doxygen group */
/*! @} */

/* EOF */
