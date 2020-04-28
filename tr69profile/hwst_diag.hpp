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

#ifndef _HWST_DIAG_
#define _HWST_DIAG_

#include <string>
#include <map>
#include <memory>
#include <mutex>
#include <iostream>

namespace hwst {

class Comm;
class Scenario;
class DiagPrevResults;

class Diag
{
    friend class hwst::Comm;
    friend class hwst::Scenario;
    friend class hwst::DiagPrevResults;
    friend std::ostream& operator<<(std::ostream& os, const Diag& diag);
    friend bool operator== (const Diag &diag1, const Diag &diag2);
    friend bool operator!= (const Diag &diag1, const Diag &diag2);

public:
    enum state_t {
        disabled,
        enabled,
        issued,
        running,
        /* not used for now:
        cancelling,
        cancelled,*/
        finished,
        error
    };

    enum result_t {
        FAILURE = 1,
        SUCCESS = 0,
        /* generic codes */
        NOT_APPLICABLE = -1,
        CANCELLED = -2,
        INTERNAL_TEST_ERROR = -3,
        DEFAULT_RESULT_VALUE = -200
    };

    struct Status
    {
        state_t state;
        int status;
        int progress;
        std::string data;
        bool modified;
    };

    const std::string params;
    const std::string name;

    Diag(const std::string name_, const std::string &params_ = "");
    virtual ~Diag();
    virtual Status getStatus(bool markRead = false);
    virtual std::string getStrStatus() const;
    const std::string getName() const { return name; }
    const std::string getPresentationResult() const;
    const std::string getPresentationComment() const;
    void setEnabled(bool val = true);

protected:
    mutable std::recursive_mutex apiMutex;
    Status status;
    std::map<int, const std::string> presentationResult;
    std::map<int, const std::string> presentationComment;
    virtual std::string getPresentationName() const { return name; };

private:
    void setIssued();
    void setState(state_t s);
    void setProgress(int progress);
    void setFinished(int s, std::string data);
    void setError();
};

std::ostream& operator<< (std::ostream& stream, const Diag& diag);

} // namespace hwst

#endif // _HWST_DIAG_
