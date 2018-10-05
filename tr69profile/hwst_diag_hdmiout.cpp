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

#include "hwst_diag_hdmiout.hpp"
#include "hwst_comm.hpp"
#include "hwst_log.hpp"

using hwst::Comm;

namespace hwst {

DiagHdmiout::DiagHdmiout(const std::string &params_):
    Diag("hdmiout_status", params_)
{
    presentationResult.insert(std::pair<int, const std::string>(HDMI_NO_DISPLAY, "WARNING"));
    presentationResult.insert(std::pair<int, const std::string>(HDMI_NO_HDCP, "WARNING"));

    presentationComment.insert(std::pair<int, const std::string>(HDMI_NO_DISPLAY, "No HDMI detected. Verify HDMI cable is connected on both ends or if TV is compatible"));
    presentationComment.insert(std::pair<int, const std::string>(HDMI_NO_HDCP, "HDMI authentication failed. Try another HDMI cable or check TV compatibility"));
}

std::string DiagHdmiout::getPresentationName() const
{
    return "HDMI Output";
};

} // namespace hwst
