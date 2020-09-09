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
#include <cstdio>
#include <cstdlib>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/
#include "wa_service.h"

/*****************************************************************************
 * METHOD DEFINITIONS
 *****************************************************************************/

namespace hwselftest {

int wa_service::create() const
{
    FILE *f = fopen(_path_name.c_str(), "wb");
    if (f)
    {
        fprintf(f, "[Unit]\n");
        if (!_description.empty())
            fprintf(f, "Description=%s\n", _description.c_str());

        if (_is_timer)
        {
            fprintf(f, "\n[Timer]\n");

            if (_on_boot_sec != -1)
                fprintf(f, "OnBootSec=%im\n", _on_boot_sec);

            if (_on_active_sec != -1)
                fprintf(f, "OnActiveSec=%im\n", _on_active_sec);

            if (_on_unit_active_sec != -1)
                fprintf(f, "OnUnitActiveSec=%im\n", _on_unit_active_sec);

            if (_on_unit_inactive_sec != -1)
                fprintf(f, "OnUnitInactiveSec=%im\n", _on_unit_inactive_sec);

            if (!_unit.empty())
                fprintf(f, "Unit=%s\n", _unit.c_str());
        }

        fclose(f);
        return 0;
    }
    else
        return 1;
}

int wa_service::destroy() const
{
    return !!unlink(_path_name.c_str());
}

int wa_service::exists() const
{
    struct stat buffer;
    return !!stat(_path_name.c_str(), &buffer);
}

int wa_service::start() const
{
    std::string cmd = _systemctl + " start " + _name;
    return system(cmd.c_str());
}

int wa_service::stop() const
{
    std::string cmd = _systemctl + " stop " + _name;
    return system(cmd.c_str());
}

int wa_service::status() const
{
    std::string cmd = _systemctl + " status " + _name;
    return system(cmd.c_str());
}

} // namespace hwselftest
