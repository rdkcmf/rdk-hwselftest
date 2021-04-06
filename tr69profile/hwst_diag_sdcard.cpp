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

#include "hwst_diag_sdcard.hpp"
#include "hwst_comm.hpp"
#include "hwst_log.hpp"

using hwst::Comm;

namespace hwst {

DiagSdcard::DiagSdcard(const std::string &params_):
    Diag("sdcard_status", params_)
{
    presentationResult.insert(std::pair<int, const std::string>(SD_CARD_TSB_STATUS_FAILURE, "FAILED"));
    presentationResult.insert(std::pair<int, const std::string>(SD_CARD_ZERO_MAXMINUTES_FAILURE, "FAILED"));

    presentationComment.insert(std::pair<int, const std::string>(SD_CARD_TSB_STATUS_FAILURE, "TSB Status Error"));
    presentationComment.insert(std::pair<int, const std::string>(SD_CARD_ZERO_MAXMINUTES_FAILURE, "TSB Zero MaxMinutes"));
}

std::string DiagSdcard::getPresentationName() const
{
    return "SD Card";
};

} // namespace hwst
