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

#include "hwst_diag_moca.hpp"
#include "hwst_comm.hpp"
#include "hwst_log.hpp"

using hwst::Comm;

namespace hwst {

DiagMoca::DiagMoca(const std::string &params_):
    Diag("moca_status", params_)
{
    presentationResult.insert(std::pair<int, const std::string>(MOCA_NO_CLIENTS, "WARNING"));
    presentationResult.insert(std::pair<int, const std::string>(MOCA_DISABLED, "WARNING"));

    presentationComment.insert(std::pair<int, const std::string>(MOCA_NO_CLIENTS, "No MoCA Network Found"));
    presentationComment.insert(std::pair<int, const std::string>(MOCA_DISABLED, "MoCA OFF"));
}

std::string DiagMoca::getPresentationName() const
{
    return "MoCA";
};

} // namespace hwst
