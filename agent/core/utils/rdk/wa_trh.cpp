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
#include <typeinfo>

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/
#include "wa_osa.h"
#include "wa_debug.h"
#include "wa_json.h"
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
#define OUTPUT_LEN                    2040

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

class TestException: public std::exception
{
    public:
        TestException(int result, const char * text)
            : result(result)
            , text(text)
        {
        }
        ~TestException() throw()
        {
        }
        int getResult() const throw()
        {
            return result;
        }
        const char * what() const throw ()
        {
            return text.c_str();
        }
    private:
        int result;
        std::string text;
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

int WA_UTILS_TRH_ReserveTuner(const char *url, uint64_t when, uint64_t duration, int timeout_ms, int tuner_count, void **out_handle)
{
    int status = 1;
    int num_tuner = 0;
    int freeTuners = 0;
    char output[OUTPUT_LEN] = {'\0'};
    const char *tunerState;
    const char *key;
    json_t *json;
    json_t *allStates = NULL;
    json_t *trmResp = NULL;
    json_t *value;
    std::string tunerId;
    std::string tunerStates;

    WA_ENTER("WA_UTILS_TRH_ReserveTuner(): url='%s', when=%llu, duration=%llu, timeout_ms=%i, out_handle=%p\n",
                url, when, duration, timeout_ms, out_handle);

    try
    {
        WA_DBG("WA_UTILS_TRH_ReserveTuner(): requesting tuner reservation for '%s' from TRM...\n", url);
        TrhHandler *handler = new TrhHandler(url);

        // Get all tuner states before tuning to ensure there are free tuners
        if (handler->helper->getAllTunerStates(tunerStates) == false)
        {
            WA_ERROR("WA_UTILS_TRH_ReserveTuner(): Failed to get tuner states\n");
            delete handler;
            return TRH_STATUS_RESERVATION_FAIL;
        }

        WA_DBG("WA_UTILS_TRH_ReserveTuner(): Tuner states from TRM (success): '%s'\n", tunerStates.c_str());

        // To check if at-least one tuner is in Free state by checking all the tuners
        if (tunerStates.compare("") != 0)
        {
            snprintf(output, sizeof(output),"%s", tunerStates.c_str());
            json = json_loads((char*)output, JSON_DISABLE_EOF_CHECK, NULL);
            if (!json)
            {
                WA_ERROR("WA_UTILS_TRH_ReserveTuner(): TRM output json_loads() error\n");
                return TRH_STATUS_RESERVATION_FAIL;
            }

            status = json_unpack(json, "{so}", "getAllTunerStatesResponse", &trmResp);
            if (status || !trmResp)
            {
                WA_ERROR("WA_UTILS_TRH_ReserveTuner(): json_unpack(\"getAllTunerStatesResponse\") error\n");
                return TRH_STATUS_RESERVATION_FAIL;
            }

            status = json_unpack(trmResp, "{so}", "allStates", &allStates);
            if (status || !allStates)
            {
                WA_ERROR("WA_UTILS_TRH_ReserveTuner(): json_unpack(\"allStates\") error\n");
                return TRH_STATUS_RESERVATION_FAIL;
            }

            void *iter = json_object_iter(allStates);
            while(iter && num_tuner < MAX_TUNER_RESERVATION_HELPERS)
            {
                key = json_object_iter_key(iter);
                value = json_object_iter_value(iter);
                tunerState = json_string_value(value);

                if (strstr(key, "TunerId-") && !strcmp(tunerState, "Free"))
                {
                    freeTuners++;
                }

                num_tuner++;

                iter = json_object_iter_next(allStates, iter);
            }
        }

        if (freeTuners == 0)
        {
            WA_ERROR("WA_UTILS_TRH_ReserveTuner(): Could not get Free tuners\n");
            return TRH_STATUS_RESERVATION_FAIL;
        }

        WA_DBG("WA_UTILS_TRH_ReserveTuner(): Free tuners available: %i\n", freeTuners);

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
                throw TestException(TRH_STATUS_RESERVATION_FAIL, "Unable to Reserve Tuner");
                delete handler;
            }
        }
        else
        {
            WA_ERROR("WA_UTILS_TRH_ReserveTuner(): failed to request tuner reservation\n");
            throw TestException(TRH_STATUS_RESERVATION_FAIL, "Unable to Reserve Tuner");
            delete handler;
        }

    }
    catch (std::exception &e)
    {
        WA_ERROR("WA_UTILS_TRH_ReserveTuner(): failed to alloc reservation helper (%s)\n", e.what());
        status = 1;
    }

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

    }
    else

        WA_ERROR("WA_UTILS_TRH_ReleaseTuner(): invalid reservation handle %p\n", handle);

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

    WA_RETURN("WA_UTILS_TRH_WaitForTunerRelease(): status=%i\n", status);

    return status;
}

/* End of doxygen group */
/*! @} */

/* EOF */
