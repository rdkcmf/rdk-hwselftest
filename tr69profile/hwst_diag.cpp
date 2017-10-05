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

#include <iostream>
#include <memory>
#include <sstream>
#include <utility>
#include <algorithm>

#include "hwst_diag.hpp"
#include "hwst_comm.hpp"

//#define HWST_DEBUG 1
#ifdef HWST_DEBUG
#define HWST_DBG(str) do {std::cout << str << std::endl;} while(false)
#else
#define HWST_DBG(str) do {} while(false)
#endif

using hwst::Comm;

namespace hwst {

Diag::Diag(const std::string name_, const std::string &params_):
    name(name_),
    params(params_),
    status{state: disabled}
{
    HWST_DBG("hwst_diag()");
    presentationResult.insert(std::pair<int, const std::string>(FAILURE, "FAILED"));
    presentationResult.insert(std::pair<int, const std::string>(SUCCESS, "PASSED"));
    presentationResult.insert(std::pair<int, const std::string>(NOT_APPLICABLE, "WARNING"));
    presentationResult.insert(std::pair<int, const std::string>(CANCELLED, "WARNING"));
    presentationResult.insert(std::pair<int, const std::string>(INTERNAL_TEST_ERROR, "WARNING"));

    presentationComment.insert(std::pair<int, const std::string>(INTERNAL_TEST_ERROR, "Error. Please Rerun Test"));
}

Diag::~Diag()
{
    HWST_DBG("~hwst_diag()");
}

std::ostream& operator<<(std::ostream& os, const Diag& diag)
{
    os << diag.getStrStatus();
    return os;
}

bool operator== (const Diag& diag1, const Diag& diag2)
{
    return ((diag1.name == diag2.name) &&
            (diag1.params == diag2.params));
}

bool operator!= (const Diag& diag1, const Diag& diag2)
{
    return !(diag1 == diag2);
}

Diag::Status Diag::getStatus(bool markRead)
{
    std::lock_guard<std::recursive_mutex> apiLock(apiMutex);

    Status s = status;
    if(markRead)
        status.modified = false;
    return s;
}

void Diag::setState(state_t s)
{
    std::lock_guard<std::recursive_mutex> apiLock(apiMutex);
    if(status.state != s)
        status.modified = true;
    status.state = s;
    HWST_DBG("DIAG:" + name + "/" + params + " state:" + std::to_string(s));
}

void Diag::setFinished(int s, std::string data)
{
    std::lock_guard<std::recursive_mutex> apiLock(apiMutex);

    HWST_DBG("setStatus:" + std::to_string(s) + " data:" + data);
    switch(status.state)
    {
    case running:
        setState(finished);
        status.status = s;
        status.data = data;
        if(s == 0)
            status.progress = 100;
        break;
    case disabled:
    case enabled:
    case issued:
    case finished:
    case error:
    default:
        break;
    }
}

void Diag::setError()
{
    std::lock_guard<std::recursive_mutex> apiLock(apiMutex);

    HWST_DBG("setError:");
    switch(status.state)
    {
    case issued:
    case running:
        setState(error);
        status.status = INTERNAL_TEST_ERROR;
        break;
    case disabled:
    case enabled:
    case finished:
    case error:
    default:
        break;
    }
}

void Diag::setProgress(int progress)
{
    std::lock_guard<std::recursive_mutex> apiLock(apiMutex);

    HWST_DBG("setProgress:" + progress);
    switch(status.state)
    {
    case issued:
    case running:
        setState(running);
        status.progress = progress;
        status.modified = true;
        break;
    case disabled:
    case enabled:
    case finished:
    case error:
    default:
        break;
    }
}

void Diag::setEnabled()
{
    std::lock_guard<std::recursive_mutex> apiLock(apiMutex);

    HWST_DBG("setEnabled:");
    switch(status.state)
    {
    case disabled:
        setState(enabled);
        break;

    case running:
    case enabled:
    case issued:
    case finished:
    case error:
    default:
        break;
    }
}

void Diag::setIssued()
{
    std::lock_guard<std::recursive_mutex> apiLock(apiMutex);

    HWST_DBG("setIssued:");
    switch(status.state)
    {
    case enabled:
        setState(issued);
        break;

    case running:
    case disabled:
    case issued:
    case finished:
    case error:
    default:
        break;
    }
}

const std::string Diag::getPresentationResult() const
{
    auto it = presentationResult.find(status.status);
    if(it == presentationResult.end())
        return presentationResult.at(status.status < 0 ? INTERNAL_TEST_ERROR : FAILURE);
    return (*it).second;
}

const std::string Diag::getPresentationComment() const
{
    auto it = presentationComment.find(status.status);
    if(it == presentationComment.end())
        return status.status < 0 ? presentationComment.at(INTERNAL_TEST_ERROR) : "";
    return (*it).second;
}

std::string Diag::getStrStatus() const
{
    std::lock_guard<std::recursive_mutex> apiLock(apiMutex);
    std::ostringstream os;
    std::string commentText = getPresentationComment();

    os << "Test result: " << getPresentationName() << ":" << getPresentationResult();
    if(!commentText.empty())
    {
        std::replace(commentText.begin(), commentText.end(), ' ', '_'); // replace all spaces with underscore
        os << "_" << commentText;
    }

    return os.str();
}

} // namespace hwst
