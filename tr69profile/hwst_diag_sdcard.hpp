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

#ifndef _HWST_DIAG_SDCARD_
#define _HWST_DIAG_SDCARD_

#include "hwst_diag.hpp"

namespace hwst {

class Comm;

class DiagSdcard: public Diag
{
    friend class hwst::Comm;

    enum {
        SD_CARD_TSB_STATUS_FAILURE       = -222,
        SD_CARD_ZERO_MAXMINUTES_FAILURE  = -223
    };
public:
    DiagSdcard(const std::string &params_ = "");
    ~DiagSdcard() {}

private:
    std::string getPresentationName() const override;
};

} // namespace hwst

#endif // _HWST_DIAG_SDCARD_
