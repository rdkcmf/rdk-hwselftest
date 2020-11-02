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

#include "hwst_diag_hdd.hpp"
#include "hwst_comm.hpp"
#include "hwst_log.hpp"

using hwst::Comm;

namespace hwst {

DiagHdd::DiagHdd(const std::string &params_):
    Diag("hdd_status", params_)
{
    presentationResult.insert(std::pair<int, const std::string>(HDD_STATUS_MISSING, "WARNING"));
    presentationResult.insert(std::pair<int, const std::string>(HDD_MARGINAL_ATTRIBUTES_FOUND, "WARNING"));
    presentationResult.insert(std::pair<int, const std::string>(HDD_DEVICE_NODE_NOT_FOUND, "WARNING"));

    presentationComment.insert(std::pair<int, const std::string>(HDD_STATUS_MISSING, "HDD Test Not Run"));
    presentationComment.insert(std::pair<int, const std::string>(HDD_MARGINAL_ATTRIBUTES_FOUND, "Marginal HDD Values"));
    presentationComment.insert(std::pair<int, const std::string>(HDD_DEVICE_NODE_NOT_FOUND, "HDD Device Node Not Found"));
}

std::string DiagHdd::getPresentationName() const
{
    return "Hard Drive";
};

} // namespace hwst
