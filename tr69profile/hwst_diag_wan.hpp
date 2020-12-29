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

#ifndef _HWST_DIAG_WAN_
#define _HWST_DIAG_WAN_

#include "hwst_diag.hpp"

namespace hwst {

class Comm;

class DiagWan: public Diag
{
    friend class hwst::Comm;

    enum {
        NO_GATEWAY_CONNECTION     = -123,
        NO_COMCAST_WAN_CONNECTION = -124,
        NO_PUBLIC_WAN_CONNECTION  = -125,
        NO_WAN_CONNECTION         = -126,
        NO_ETH_GATEWAY_FOUND      = -128,
        NO_MW_GATEWAY_FOUND       = -129,
        NO_ETH_GATEWAY_CONNECTION = -130,
        NO_MW_GATEWAY_CONNECTION  = -131
    };
public:
    DiagWan(const std::string &params_ = "");
    ~DiagWan() {}

private:
    std::string getPresentationName() const override;
};

} // namespace hwst

#endif // _HWST_DIAG_WAN_
