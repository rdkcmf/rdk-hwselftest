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

#include "hwst_diag_modem.hpp"
#include "hwst_comm.hpp"
#include "hwst_log.hpp"

using hwst::Comm;

namespace hwst {

DiagModem::DiagModem(const std::string &params_):
    Diag("modem_status", params_)
{
    presentationResult.insert(std::pair<int, const std::string>(CM_NO_SIGNAL, "WARNING"));

    presentationComment.insert(std::pair<int, const std::string>(CM_NO_SIGNAL, "Lock Failed - Check Cable"));
}

std::string DiagModem::getPresentationName() const
{
    return "Cable Modem";
};

} // namespace hwst
