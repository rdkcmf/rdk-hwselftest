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
 * @file wa_trh_listener.h
 *
 * @brief This file contains listener implementation for tuner reservation helper.
 */

/** @addtogroup WA_UTILS_TRH_LISTENER_H
 *  @{
 */

#ifndef WA_UTILS_TRH_LISTENER_H
#define WA_UTILS_TRH_LISTENER_H

/*****************************************************************************
 * STANDARD INCLUDE FILES
 *****************************************************************************/
#include <exception>
#include <stdexcept>
#include <string>

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/
#include "TunerReservationHelper.h"
#include "wa_osa.h"
#include "wa_debug.h"

/*****************************************************************************
 * EXPORTED DEFINITIONS
 *****************************************************************************/

typedef enum
{
    TRH_STATUS_UNRESERVED,
    TRH_STATUS_RESERVATION_SUCCESS,
    TRH_STATUS_RESERVATION_FAIL,
    TRH_STATUS_RESERVATION_RELEASE_SUCCESS,
    TRH_STATUS_RESERVATION_RELEASE_FAIL,
    TRH_STATUS_RELEASED
} TRH_Status_t;

/*****************************************************************************
 * EXPORTED TYPES
 *****************************************************************************/

class reservation_listener_impl : public TunerReservationEventListener
{
public:
    reservation_listener_impl(const std::string& url);
    ~reservation_listener_impl();

    void wait_reserve(int timeout_ms = 0) const;
    void wait_reserve_release(int timeout_ms = 0) const;
    void wait_release(int timeout_ms = 0) const;
    TRH_Status_t get_status() const { return _status; };

    void reserveSuccess() /* override */;
    void reserveFailed() /* override */;
    void releaseReservationSuccess() /* override */;
    void releaseReservationFailed() /* override */;
    void tunerReleased() /*override */;

private:
    void _deinit();

    TRH_Status_t _status;
    std::string _locator;
    mutable void *_reserve_sem;
    mutable void *_reserve_release_sem;
    mutable void *_tuner_release_sem;
};


/*****************************************************************************
 * EXPORTED FUNCTIONS
 *****************************************************************************/
reservation_listener_impl::reservation_listener_impl(const std::string& url)
    : _locator(url)
{
    _status = TRH_STATUS_UNRESERVED;

    _reserve_sem = WA_OSA_SemCreate(0);
    _reserve_release_sem = WA_OSA_SemCreate(0);
    _tuner_release_sem = WA_OSA_SemCreate(0);

    if (!_reserve_sem || !_reserve_release_sem || !_tuner_release_sem)
    {
        _deinit();
        throw std::runtime_error("semaphore creation error");
    }
}

reservation_listener_impl::~reservation_listener_impl()
{
    _deinit();
}

void reservation_listener_impl::_deinit()
{
    if (_reserve_sem)
    {
        if (WA_OSA_SemDestroy(_reserve_sem))
            WA_ERROR("reservation_listener::_deinit(): WA_OSA_SemDestroy() failed\n");
    }

    if (_reserve_release_sem)
    {
        if (WA_OSA_SemDestroy(_reserve_release_sem))
            WA_ERROR("reservation_listener::_deinit(): WA_OSA_SemDestroy() failed\n");
    }

    if (_tuner_release_sem)
    {
        if (WA_OSA_SemDestroy(_tuner_release_sem))
            WA_ERROR("reservation_listener::_deinit(): WA_OSA_SemDestroy() failed\n");
    }
}

void reservation_listener_impl::wait_reserve(int timeout_ms) const
{
    WA_OSA_SemTimedWait(_reserve_sem, timeout_ms);
}

void reservation_listener_impl::wait_reserve_release(int timeout_ms) const
{
    WA_OSA_SemTimedWait(_reserve_release_sem, timeout_ms);
}

void reservation_listener_impl::wait_release(int timeout_ms) const
{
    WA_OSA_SemTimedWait(_tuner_release_sem, timeout_ms);
}

void reservation_listener_impl::reserveSuccess()
{
    WA_DBG("TRHListenerImpl::reserveSuccess(): reservation success for '%s'\n", _locator.c_str());
    _status = TRH_STATUS_RESERVATION_SUCCESS;
    WA_OSA_SemSignal(_reserve_sem);
}

void reservation_listener_impl::reserveFailed()
{
    WA_DBG("TRHListenerImpl::reserveFailed(): reservation failed for '%s'\n", _locator.c_str());
    _status = TRH_STATUS_RESERVATION_FAIL;
    WA_OSA_SemSignal(_reserve_sem);
}

void reservation_listener_impl::releaseReservationSuccess()
{
    WA_DBG("TRHListenerImpl::releaseReservationSuccess(): reservation release success for '%s'\n", _locator.c_str());
    _status = TRH_STATUS_RESERVATION_RELEASE_SUCCESS;
    WA_OSA_SemSignal(_reserve_release_sem);
}

void reservation_listener_impl::releaseReservationFailed()
{
    WA_DBG("TRHListenerImpl::releaseReservationFailed(): reservation release failed for '%s'\n", _locator.c_str());
    _status = TRH_STATUS_RESERVATION_RELEASE_FAIL;
    WA_OSA_SemSignal(_reserve_release_sem);
}

void reservation_listener_impl::tunerReleased()
{
    WA_DBG("TRHListenerImpl::tunerReleased(): tuner released for '%s'\n", _locator.c_str());
    _status = TRH_STATUS_RELEASED;
    WA_OSA_SemSignal(_tuner_release_sem);
    WA_OSA_SemSignal(_reserve_release_sem);
}

#endif /* WA_UTILS_TRH_LISTENER_H */

/* End of doxygen group */
/*! @} */

/* EOF */
