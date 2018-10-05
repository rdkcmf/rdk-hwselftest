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

#ifndef _HWST_COMM_
#define _HWST_COMM_

#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <string>

#include "jansson.h"

namespace hwst {

class Diag;
class Ws;

class Comm
{
public:
    using cb_t = std::function<void(void)>;

    Comm(cb_t cbConnected_, cb_t cbDisconnected_, cb_t cbReceived_);
    ~Comm();
    int connect(std::string host, std::string port, int timeout);
    void disconnect();
    int send(std::shared_ptr<Diag> diag);
    int sendRaw(std::string method, std::string params, std::string id);
    void cbConnected();
    void cbDisconnected();
    void cbReceived(std::string msg);
    int handleMsgResult(std::unique_ptr<json_t, decltype(json_decref)*> &json);
    int handleMsgError(std::unique_ptr<json_t, decltype(json_decref)*> &json);
    int handleMsgMethod(std::unique_ptr<json_t, decltype(json_decref)*> &json);

private:
    cb_t cbExtConnected;
    cb_t cbExtDisconnected;
    cb_t cbExtReceived;

    std::recursive_mutex apiMutex;
    std::shared_ptr<Ws> ws;
    std::condition_variable cv;
    bool connected;
    unsigned int sendId;
    std::map<unsigned int, std::shared_ptr<Diag>> byIdMap;
    std::map<std::string, std::shared_ptr<Diag>> byHInstanceMap;
};

} // namespace hwst

#endif // _HWST_COMM_
