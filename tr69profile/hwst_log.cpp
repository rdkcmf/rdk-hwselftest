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

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <fstream>

#include "hwst_log.hpp"

//#define HWST_DEBUG 1
#ifdef HWST_DEBUG
#define HWST_DBG(str) do {std::cout << str << std::endl;} while(false)
#else
#define HWST_DBG(str) do {} while(false)
#endif

#define HWST_LOG_FILE "/opt/logs/hwselftest.log"

namespace hwst {

Log::Log()
{
    HWST_DBG("hwst_log()");
}

Log::~Log()
{
    HWST_DBG("~hwst_log()");
}

std::string Log::format(std::string text)
{
    std::chrono::system_clock::time_point tnow = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(tnow);
    struct tm now = *std::localtime(&t);
    std::ostringstream os;

    os << (now.tm_year + 1900) << "-" <<
        std::setfill('0') << std::setw(2) << (now.tm_mon + 1) << "-" <<
        std::setfill('0') << std::setw(2) << (now.tm_mday) << " " <<
        std::setfill('0') << std::setw(2) << (now.tm_hour) << ":" <<
        std::setfill('0') << std::setw(2) << (now.tm_min) << ":" <<
        std::setfill('0') << std::setw(2) << (now.tm_sec) << " " <<
        text;
    return os.str();;
}

void Log::toFile(std::string text)
{
    std::ofstream file(HWST_LOG_FILE, std::ofstream::out | std::ofstream::app);
    file << text;
}

} // namespace hwst
