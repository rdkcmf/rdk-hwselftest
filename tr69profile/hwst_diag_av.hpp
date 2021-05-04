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

#ifndef _HWST_DIAG_AV_
#define _HWST_DIAG_AV_

#include "hwst_diag.hpp"

namespace hwst {

class Comm;

class DiagAv: public Diag
{
    friend class hwst::Comm;

    enum {
        SI_CACHE_MISSING = -105,     /* Used only for debugging with '#define AVD_USE_RMF 1'; But the same error code is used in Tuner test */
        AV_NO_SIGNAL = -107,         /* Deprecated (DELIA-48787) - Used only for debugging with '#define AVD_USE_RMF 1' */
        AV_URL_NOT_REACHABLE = -114, /* Deprecated (DELIA-48787) - Used only for debugging with '#define AVD_USE_RMF 1' */
        AV_DECODERS_NOT_ACTIVE = -132
    };
public:
    DiagAv(const std::string &params_ = "");
    ~DiagAv() {}

private:
    std::string getPresentationName() const override;
};

} // namespace hwst

#endif // _HWST_DIAG_AV_
