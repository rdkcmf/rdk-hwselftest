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

#ifndef WA_SERVICE_H_
#define WA_SERVICE_H_

/*****************************************************************************
 * STANDARD INCLUDE FILES
 *****************************************************************************/
#include <string>

/*****************************************************************************
 * EXPORTED CLASSES
 *****************************************************************************/

 namespace hwselftest {

class wa_service final {
public:
    wa_service(const std::string& path, const std::string& name)
        : _name(name),
          _on_boot_sec(-1),
          _on_active_sec(-1),
          _on_unit_active_sec(-1),
          _on_unit_inactive_sec(-1),
          _systemctl("systemctl")
    {
        _is_timer = (_name.find(".timer") != std::string::npos);
        _path_name = path + "/" + name;
    }

    void set_description(const std::string &description) { _description = description; }
    void set_unit(const std::string &unit) { _unit = unit; }
    void set_on_boot_sec(int minutes) { _on_boot_sec = minutes; }
    void set_on_active_sec(int minutes) { _on_active_sec = minutes; }
    void set_on_unit_active_sec(int minutes) { _on_unit_active_sec = minutes; }
    void set_on_unit_inactive_sec(int minutes) { _on_unit_inactive_sec = minutes; }

    int create() const;
    int destroy() const;
    int exists() const;

    int start() const;
    int stop() const;
    int status() const;

private:
    std::string _systemctl;
    std::string _path_name;
    std::string _name;
    std::string _unit;
    std::string _description;
    int _on_boot_sec;
    int _on_active_sec;
    int _on_unit_active_sec;
    int _on_unit_inactive_sec;
    bool _is_timer;
};

} // namespace hwselftest

#endif /* WA_SERVICE_H_ */
