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

#ifndef _HWST_WS_
#define _HWST_WS_

#include <condition_variable>
#include <functional>
#include <thread>
#include <mutex>
#include <memory>

#include "nopoll.h"

namespace hwst {

class Ws
{
public:
    using cbConnected_t = std::function<void(void)>;
    using cbDisconnected_t = std::function<void(void)>;
    using cbReceived_t = std::function<void(std::string)>;

    /* Note: The nopoll patched implementation is buggy, it does not free allocated memory but also
     *       it does free the user provided pointer, which should not be touched, that causes crash.
     *       Because of that the Ws class is created as singleton, not passing any pointers
     *       into nopoll.
     */
    static std::shared_ptr<Ws> CreateInstance(cbConnected_t cbConnected_, cbDisconnected_t cbDisconnected_, cbReceived_t cbReceived_);
    ~Ws();
    int connect(std::string host, std::string port, int timeout);
    int send(std::string msg);
    void disconnect();
    static void cbConnected();
    static void cbDisconnected();
    static void cbReceived(std::string msg);

private:
    static std::weak_ptr<Ws> instance;
    Ws(cbConnected_t cbConnected_, cbDisconnected_t cbDisconnected_, cbReceived_t cbReceived_);
    std::recursive_mutex apiMutex;
    std::mutex cbMutex;
    std::condition_variable cbCond;
    bool connected;

    cbConnected_t cbExtConnected;
    cbDisconnected_t cbExtDisconnected;
    cbReceived_t cbExtReceived;

    typedef std::unique_ptr<noPollCtx, decltype(nopoll_ctx_unref)*> ctx_t;
    ctx_t ctx;
    
    typedef std::unique_ptr<noPollConn, decltype(nopoll_conn_close)*> conn_t;
    conn_t conn;

    std::unique_ptr<std::thread> npThread;
    void npLoop();
};

} // namespace hwst

#endif // _HWST_WS_
