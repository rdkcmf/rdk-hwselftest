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

#ifndef WA_SETTINGS_H_
#define WA_SETTINGS_H_

/*****************************************************************************
 * STANDARD INCLUDE FILES
 *****************************************************************************/
#include <string>
#include <map>

/*****************************************************************************
 * EXPORTED CLASSES
 *****************************************************************************/

namespace hwselftest {

class wa_settings final {
public:
    wa_settings(const std::string& path)
        : _path(path)
    {
        load();
    }

    bool load();
    bool save();

    template<class T> bool set(const std::string& key, const T& val) { _settings[key] = std::to_string(val); return save(); }
    const std::string& get(const std::string& key) { return _settings[key]; }

private:
    std::string _path;
    std::map<std::string, std::string> _settings;
};

template<> inline bool wa_settings::set<std::string>(const std::string& key, const std::string& val) { _settings[key] = val; return save(); }

}; // namespace hwselftest

#endif /* WA_SETTINGS_H_ */
