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

#include "hwst_diag_mcard.hpp"
#include "hwst_comm.hpp"
#include "hwst_log.hpp"

using hwst::Comm;

namespace hwst {

DiagMcard::DiagMcard(const std::string &params_):
    Diag("mcard_status", params_)
{
    /* currently mapped to FAILURE
    presentationResult.insert(std::pair<int, const std::string>(MCARD_CERT_INVALID, "FAILED"));
    */
    presentationResult.insert(std::pair<int, const std::string>(MCARD_AUTH_KEY_REQUEST_FAILURE, "FAILED"));
    presentationResult.insert(std::pair<int, const std::string>(MCARD_HOSTID_RETRIEVE_FAILURE, "FAILED"));
    presentationResult.insert(std::pair<int, const std::string>(MCARD_CERT_AVAILABILITY_FAILURE, "FAILED"));

    presentationComment.insert(std::pair<int, const std::string>(FAILURE, "Invalid Card Certification"));
    presentationComment.insert(std::pair<int, const std::string>(MCARD_AUTH_KEY_REQUEST_FAILURE, "Card Auth Key Not Ready"));
    presentationComment.insert(std::pair<int, const std::string>(MCARD_HOSTID_RETRIEVE_FAILURE, "Unable To Retrieve Card ID"));
    presentationComment.insert(std::pair<int, const std::string>(MCARD_CERT_AVAILABILITY_FAILURE, "Card Certification Not Available"));
}

std::string DiagMcard::getPresentationName() const
{
    return "Cable Card";
};

} // namespace hwst
