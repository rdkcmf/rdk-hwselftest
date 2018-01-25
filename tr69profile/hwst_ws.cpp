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
#include <thread>
#include <mutex>
#include <new>

#include "hwst_ws.hpp"

#include <pthread.h>

//#define HWST_DEBUG 1
#ifdef HWST_DEBUG
#define HWST_DBG(str) do {std::cout << str << std::endl;} while(false)
#else
#define HWST_DBG(str) do {} while(false)
#endif

//#define USE_PRCTL_FOR_THREAD_NAME 1
#ifdef USE_PRCTL_FOR_THREAD_NAME
#include <sys/prctl.h> //for debug thread name
#endif

#define FLUSH_TIMEOUT 2000000

namespace hwst {

extern "C" {

noPollPtr npMutexCreate()
{
    return static_cast<noPollPtr>(new (std::nothrow) std::recursive_mutex());
}

void npMutexDestroy(noPollPtr m)
{
    if(m) delete static_cast<std::recursive_mutex*>(m);
}

void npMutexLock(noPollPtr m)
{
    if(m) static_cast<std::recursive_mutex*>(m)->lock();
}

void npMutexUnlock(noPollPtr m)
{
    if(m) static_cast<std::recursive_mutex*>(m)->unlock();
}

void npMessageCb(noPollCtx *ctx, noPollConn *conn, noPollMsg *msg, noPollPtr user_data)
{
    HWST_DBG("npMessageCb");
    const char *payload = (const char *)nopoll_msg_get_payload(msg);

    if(payload)
        Ws::cbReceived(payload);
    HWST_DBG("npMessageCb-END");
}

/* This callback is coming from two sources;
 * - the nopoll_loop_wait(), if closed externally
 * - nopoll_conn_close(), if closed intentionally
 */
void npClosedCb(noPollCtx *ctx, noPollConn *conn, noPollPtr user_data)
{
    HWST_DBG("npClosedCb");
    Ws::cbDisconnected();
    HWST_DBG("npClosedCb-END");
}

void npConnFree(noPollConn *conn)
{
    HWST_DBG("npConnFree");
    nopoll_conn_close(conn);
    HWST_DBG("npConnFree-END");
}

} // extern "C"

std::weak_ptr<Ws> Ws::instance;

std::shared_ptr<Ws> Ws::CreateInstance(cbConnected_t cbConnected_, cbDisconnected_t cbDisconnected_, cbReceived_t cbReceived_)
{
    HWST_DBG("Ws create");
    std::shared_ptr<Ws> ws = instance.lock();
    if(ws == nullptr)
    {
        ws.reset(new Ws(cbConnected_, cbDisconnected_, cbReceived_));
        Ws::instance = ws;
    }
    return ws;
}

Ws::Ws(cbConnected_t cbConnected_, cbDisconnected_t cbDisconnected_, cbReceived_t cbReceived_):
    cbExtConnected(cbConnected_),
    cbExtDisconnected(cbDisconnected_),
    cbExtReceived(cbReceived_),
    ctx(nullptr, nopoll_ctx_unref),
    conn(nullptr, nopoll_conn_close),
    connected(false)
{
    nopoll_thread_handlers(npMutexCreate,
	npMutexDestroy,
	npMutexLock,
	npMutexUnlock);

    ctx = ctx_t(nopoll_ctx_new(), nopoll_ctx_unref);
    if(ctx == nullptr)
        goto end;

end:
    HWST_DBG("hwst_ws");
}

Ws::~Ws()
{
    if(ctx == nullptr)
        goto end;

    disconnect();

end:
    HWST_DBG("~hwst_ws");
}

void Ws::npLoop(void)
{
#ifdef USE_PRCTL_FOR_THREAD_NAME
    prctl(PR_SET_NAME,"looper",0,0,0);
#endif
    HWST_DBG("starting loop");
    nopoll_loop_wait(ctx.get(), 0);
    HWST_DBG("loop finished");
}

void Ws::cbConnected()
{
    std::shared_ptr<Ws> ws = instance.lock();
    if(ws == nullptr)
        goto end;

    {
        std::lock_guard<std::recursive_mutex> apiLock(ws->apiMutex);
        HWST_DBG("wa::cbCconnected ENTER");
        ws->cbExtConnected();
    }

end:
    HWST_DBG("cbConnected");
    return;
}

void Ws::cbDisconnected()
{
    std::shared_ptr<Ws> ws = instance.lock();
    if(ws == nullptr)
        goto end;

    {
        std::unique_lock<std::mutex> condLock(ws->cbMutex);
        HWST_DBG("wa::cbDisconnect ENTER");
        ws->connected = false;
        ws->cbExtDisconnected();
    }

    HWST_DBG("cbDisconnected-notify");
    ws->cbCond.notify_all();
    HWST_DBG("cbDisconnected-finished");

end:
    return;
}

void Ws::cbReceived(std::string msg)
{
    std::shared_ptr<Ws> ws = instance.lock();
    if(ws == nullptr)
        goto end;

    {
        std::lock_guard<std::recursive_mutex> apiLock(ws->apiMutex);
        HWST_DBG("wa::cbReceive ENTER");
        ws->cbExtReceived(msg);
    }
end:
    return;
}

int Ws::connect(std::string host, std::string port, int timeout)
{
    std::lock_guard<std::recursive_mutex> apiLock(apiMutex);
    HWST_DBG("wa::connect ENTER");
    int status = -1;
    int npStatus;

    if(ctx == nullptr)
        goto end;

    if(conn != nullptr)
    {
        status = 0;
        goto end;
    }
    conn = conn_t(nopoll_conn_new(ctx.get(), host.c_str(), port.c_str(), NULL, NULL, NULL, NULL), nopoll_conn_close);
    if((conn == nullptr) || !nopoll_conn_is_ok (conn.get()))
        goto end;
    connected = true;

    nopoll_conn_set_on_close(conn.get(), npClosedCb, NULL);
    nopoll_conn_set_on_msg(conn.get(), npMessageCb, &cbExtReceived);
    /* This callback does not for for client. The client must poll
    nopoll_conn_set_on_ready(conn.get(), npReadyCb, &cbExtConnected);
    */
    if(timeout <= 0)
        timeout = 10; //as we go away from nopoll no separate thread for polling on_ready

    HWST_DBG("wa::connect set non-blocking");
    if(nopoll_conn_set_sock_block(nopoll_conn_socket(conn.get()), nopoll_false) == nopoll_false)
    {
        HWST_DBG("wa::connect cannot set non-blocking socket");
        goto end;
    }
    HWST_DBG("wa::connect wait ready");
    if(nopoll_conn_wait_until_connection_ready (conn.get(), timeout, &npStatus,  NULL) == nopoll_true)
    {
        HWST_DBG("wa::connect ready");
        status = 0;
        cbConnected();
        npThread = std::unique_ptr<std::thread>(new std::thread(&Ws::npLoop, this));
    }

end:
    HWST_DBG("wa::connect status:" + std::to_string(status));

    return status;
}

int Ws::send(std::string msg)
{
    HWST_DBG("wa::send #1");
    std::lock_guard<std::recursive_mutex> apiLock(apiMutex);
    HWST_DBG("wa::send");
    int status = -1;
    int length;

    if(!nopoll_conn_is_ready(conn.get()))
        goto end;

    length = nopoll_conn_send_text(conn.get(), msg.c_str(),msg.length());
    length = nopoll_conn_flush_writes (conn.get(), FLUSH_TIMEOUT, length);
    if(length == msg.length())
        status = 0;

    HWST_DBG("ws-send:" + std::to_string(status));
end:
    return status;
}

void Ws::disconnect()
{
    std::lock_guard<std::recursive_mutex> apiLock(apiMutex);
    HWST_DBG("wa::disconnect ENTER");
    if(conn == nullptr)
        goto end;

    nopoll_loop_stop(ctx.get());

    if(npThread != nullptr)
    {
        HWST_DBG("ws-disconnect-join");
        npThread->join();
        HWST_DBG("ws-disconnect-joined");
        npThread.reset();
    }

    HWST_DBG("ws-disconnect-close");

    nopoll_conn_close(conn.get());
    {
        std::unique_lock<std::mutex> condLock(cbMutex);
        HWST_DBG("ws-disconnect-waiting");
        cbCond.wait(condLock, [&]{return !connected;});
        HWST_DBG("ws-disconnect-confirmed");
    }
    conn.release(); //since nopoll_conn_close() is already done no .reset() must be called.

end:
    HWST_DBG("ws-disconnect");
}

} // namespace hwst
