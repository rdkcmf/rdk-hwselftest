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

/*****************************************************************************
 * STANDARD INCLUDE FILES
 *****************************************************************************/
#include <string>
#include <fstream>

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/
#include "wa_settings.h"

/*****************************************************************************
 * METHOD DEFINITIONS
 *****************************************************************************/

namespace hwselftest {

bool wa_settings::load()
{
    std::ifstream file(_path);
    if (file.is_open())
    {
        std::string line;
        while (std::getline(file, line))
        {
            size_t delim = line.find('=');
            if (delim != std::string::npos)
                _settings.emplace(line.substr(0, delim), line.substr(delim + 1));
        }

        return (!_settings.empty());
    }

    return false;
}

bool wa_settings::save()
{
    std::ofstream file(_path, std::ios::trunc);
    if (file.is_open())
    {
        for (auto s : _settings)
            file << s.first << "=" << s.second << '\n';

        return true;
    }

    return false;
}

} // namespace hwselftest
