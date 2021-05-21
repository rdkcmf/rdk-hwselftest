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
#include "hwst_comm.hpp"
#include "hwst_diag.hpp"
#include "hwst_ws.hpp"
#include "jansson.h"

//#define HWST_DEBUG 1
#ifdef HWST_DEBUG
#define HWST_DBG(str) do {std::cout << "HWST_DBG |" << str << std::endl;} while(false)
#else
#define HWST_DBG(str) do {} while(false)
#endif

using hwst::Ws;
using hwst::Diag;

namespace hwst {

Comm::Comm(cb_t cbConnected_, cb_t cbDisconnected_, cb_t cbReceived_):
    cbExtConnected(cbConnected_),
    cbExtDisconnected(cbDisconnected_),
    cbExtReceived(cbReceived_),
    connected(false)
{
    using namespace std::placeholders;

    ws = Ws::CreateInstance(std::bind(&Comm::cbConnected, this),
        std::bind(&Comm::cbDisconnected, this),
        std::bind(&Comm::cbReceived, this, _1));

    HWST_DBG("hwst_comm");
}

Comm::~Comm()
{
    HWST_DBG("~hwst_comm");
    cbDisconnected();
    disconnect();
}

int Comm::connect(std::string host, std::string port, int timeout)
{
    int status;
    HWST_DBG("comm-connect");
    status = ws->connect(host, port, timeout);
    return status;
}

void Comm::disconnect()
{
    HWST_DBG("comm-disconnect");
    ws->disconnect();
}

int Comm::sendRaw(std::string method, std::string params, std::string id)
{
    HWST_DBG("comm-sendRaw");

    std::string request;

    request = std::string("{\"jsonrpc\": \"2.0\", \"method\": \"") + method +
        std::string("\", \"params\":") + (params.empty() ? "[]" : params) + std::string(", \"id\": ") + id + std::string("}");
    HWST_DBG("request:" + request);

    return ws->send(request);
}

int Comm::send(std::shared_ptr<Diag> diag)
{
    HWST_DBG("comm-send-#1");
    int status = -1;
    std::map<unsigned int, std::shared_ptr<Diag>>::iterator it;
    std::string request;
    HWST_DBG("comm-send");

    {
        std::lock_guard<std::recursive_mutex> apiLock(apiMutex);
        for(int i=sendId++; byIdMap.count(sendId); ++sendId)
        {
            if(i == sendId)
                goto end;
        }
        auto result = byIdMap.insert(std::pair<unsigned int, std::shared_ptr<Diag>>(sendId,diag));
        if(result.second == false)
            goto end;
    }

    diag->setIssued();
    status = sendRaw(diag->name, diag->params, std::to_string(sendId));
    if(status != 0)
    {
        std::lock_guard<std::recursive_mutex> apiLock(apiMutex);
        byIdMap.erase(it);
    }
end:
    if(status != 0)
        diag->setError();

    return status;
}

void Comm::cbConnected()
{
    std::lock_guard<std::recursive_mutex> apiLock(apiMutex);

    connected = true;
    cbExtConnected();
    HWST_DBG("comm-cbConnected");
}

void Comm::cbDisconnected()
{
    std::lock_guard<std::recursive_mutex> apiLock(apiMutex);

    for (auto& m: byIdMap)
    {
        m.second->setError();
    }
    byIdMap.clear();

    for (auto& m: byHInstanceMap)
    {
        m.second->setError();
    }
    byHInstanceMap.clear();

    connected = false;

    cbExtDisconnected();
    HWST_DBG("comm-cbDisconnected");
}

void Comm::cbReceived(std::string msg)
{
    json_error_t jerror;
    int status;
    json_t *jId, *jResult, *jp;
    char *s;
    bool gotId = false;
    int id;
    std::string option;


    HWST_DBG("comm-cbReceived");

    std::unique_ptr<json_t, decltype(json_decref)*> json(json_loadb(msg.c_str(), msg.length(), JSON_DISABLE_EOF_CHECK, &jerror), json_decref);
    if(json == nullptr)
        goto end;

    /* check for "id" - although any type is allowed, this implementation is limited to
     * support only null and int
     */
    status = json_unpack(json.get(), "{s:n}", "id");
    if(status != 0)
    {
        HWST_DBG("json id is not null");
        status = json_unpack(json.get(), "{s:i}", "id", &id); //reference to jId is NOT modified
        if(status != 0)
        {
            HWST_DBG("json id is not int");
            goto end;
        }
        gotId = true;
    }

    /* check for "jsonrpc 2.0" */
    status = json_unpack(json.get(), "{ss}", "jsonrpc", &s); //s will be released when releasing json object
    if((status != 0) || strcmp(s, "2.0"))
    {
        HWST_DBG("not rpc 2.0 json");
        goto end;
    }
    HWST_DBG("valid json");

    /* check for "method" */
    if(Comm::handleMsgMethod(json) >= 0)
    {
        /* handled */
        goto end;
    }

    /* check for "result" */
    if(handleMsgResult(json) >=0 )
    {
        /* handled */
        goto end;
    }

    /* check for "error" */
    if(handleMsgError(json) >=0 )
    {
        /* handled */
        goto end;
    }

    HWST_DBG("invalid json");
end:
    HWST_DBG("comm::cbReceived#1");
    cbExtReceived();
    HWST_DBG("comm::cbReceived#2");
    return;
}

int Comm::handleMsgResult(std::unique_ptr<json_t, decltype(json_decref)*> &json)
{
    int status;
    char *s;
    int id;
    json_t *jResult;
    std::string option;
    std::string diag;
    std::map<unsigned int, std::shared_ptr<Diag>>::iterator it;
    std::unique_lock<std::recursive_mutex> apiLock(apiMutex, std::defer_lock);

    /* check for "result" */
    if(json_unpack(json.get(), "{so}", "result", &jResult) != 0) //s will be released when releasing json object
    {
        HWST_DBG("not a result json");;
        status = -1;
        goto end;
    }
    option = std::string(json_dumps(jResult, JSON_ENCODE_ANY));
    HWST_DBG("result json:" + option);

    /* result must have id */
    if(json_unpack(json.get(), "{s:i}", "id", &id) != 0) //reference to jId is NOT modified
    {
        HWST_DBG("json id is not int");
        status = 1;
        goto end;
    }

    /* check for "diag" */
    if(json_unpack(jResult, "{sn}", "diag") == 0)
    {
        HWST_DBG("diag:null");
        status = 2;
        apiLock.lock();
        it = byIdMap.find(id);
        if (it != byIdMap.end())
        {
            HWST_DBG("remove diag:" + it->second->name);
            it->second->setError();
            byIdMap.erase(it);
            status = 3;
        }
        apiLock.unlock();
    }
    else if(json_unpack(jResult, "{ss}", "diag", &s) == 0) //s will be released when releasing json object
    {
        diag = std::string(s);
        HWST_DBG("diag:" + diag);

        status = 4;
        if(diag[0] == '#')
        {
            status = 5;
            apiLock.lock();
            it = byIdMap.find(id);
            if (it != byIdMap.end())
            {
                HWST_DBG("move diag:" + it->second->name);
                status = 6;
                it->second->setProgress(0);
                if(!byHInstanceMap.count(diag))
                {
                    status = 7;
                    byHInstanceMap.insert(std::pair<std::string,std::shared_ptr<Diag>>(diag,it->second));
                    if(byHInstanceMap.count(std::string(s)))
                    {
                        HWST_DBG("new diag acquired");
                        byIdMap.erase(it);
                        status = 0;
                    }
                }
            }
            apiLock.unlock();
        }
    }
    else
    {
        HWST_DBG("no diag result");
        status = 7;
    }

end:
    return status;
}

int Comm::handleMsgError(std::unique_ptr<json_t, decltype(json_decref)*> &json)
{
    int status;
    char *s;
    int id;
    json_t *jError;
    std::string option;
    std::map<unsigned int, std::shared_ptr<Diag>>::iterator it;
    std::unique_lock<std::recursive_mutex> apiLock(apiMutex, std::defer_lock);

    /* check for "result" */
    if(json_unpack(json.get(), "{so}", "error", &jError) != 0) //s will be released when releasing json object
    {
        HWST_DBG("not an error json");
        status = -1;
        goto end;
    }

    option = std::string(json_dumps(jError, JSON_ENCODE_ANY));
    HWST_DBG("error json:" + option);

    /* result may have id */
    if(json_unpack(json.get(), "{s:i}", "id", &id) != 0) //reference to jId is NOT modified
    {
        HWST_DBG("json id is not int");
        status = 1;
        goto end;
    }

    status = 2;
    apiLock.lock();
    it = byIdMap.find(id);
    if (it != byIdMap.end())
    {
        HWST_DBG("remove diag:" + it->second->name);
        byIdMap.erase(it);
        status = 0;
    }
    apiLock.unlock();

end:
    return status;
}

int Comm::handleMsgMethod(std::unique_ptr<json_t, decltype(json_decref)*> &json)
{
    int status;
    char *m, *d, *dt;
    int id, progress, estatus, filterenable, fstatus;
    json_t *jParams, *jData;
    std::string method, diag;
    std::map<std::string, std::shared_ptr<Diag>>::iterator it;
    std::string data;
    std::unique_lock<std::recursive_mutex> apiLock(apiMutex, std::defer_lock);

    /* check for "method" */
    if(json_unpack(json.get(), "{ss}", "method", &m) != 0) //s will be released when releasing json object
    {
        /* for now ignore messages (methods with id=null) */
        HWST_DBG("not a method");
        status = -1;
        goto end;
    }

    method = std::string(m);
    HWST_DBG("method:" + method + ", json:" + std::string(json_dumps(json.get(), JSON_ENCODE_ANY)));

    /* id must exist, currently accept ony null (so messages) */
    if(json_unpack(json.get(), "{s:n}", "id") != 0)
    {
        HWST_DBG("json id is not null");
        status = 1;
        goto end;
    }

    if(method[0] == '#')
    {
        bool anyData = false;
        HWST_DBG("method: #");

        status = 2;
        /* check for "progress" */
        if(json_unpack(json.get(), "{si}", "progress", &progress) == 0)
        {
            HWST_DBG("progress:" + progress);
            anyData = true;
        }

        if(anyData)
        {
            status = 3;
            apiLock.lock();
            it = byHInstanceMap.find(method);
            if (it != byHInstanceMap.end())
            {
                HWST_DBG("found diag:" + it->second->name);
                it->second->setProgress(progress);
                status = 0;
            }
            apiLock.unlock();
        }
    }
    else if(method.compare("eod") == 0)
    {
        HWST_DBG("method: EOD");
        status = 4;
        /* check for "params" */
        if(json_unpack(json.get(), "{so}", "params", &jParams) != 0)
        {
            HWST_DBG("missing params");
            status = 5;
            goto end;
        }

        /* check for "status" */
        if(json_unpack(jParams, "{si}", "status", &estatus) != 0)
        {
            HWST_DBG("missing status");
            status = 6;
            goto end;
        }

        /* check for "filterenabled" */
        if(json_unpack(jParams, "{si}", "filterenabled", &filterenable) != 0)
        {
            HWST_DBG("missing filterenabled");
            status = 7;
            goto end;
        }

        /* check for "filterstatus" */
        if(json_unpack(jParams, "{si}", "filterstatus", &fstatus) != 0)
        {
            HWST_DBG("missing filterstatus");
            status = 8;
            goto end;
        }

        /* check for "data" */

        if(json_unpack(jParams, "{so}", "data", &jData) == 0) //jData will be released when releasing json object
        {
            data = std::string(json_dumps(jData, JSON_ENCODE_ANY));
            HWST_DBG("got data:");
        }

        if(json_unpack(jParams, "{ss}", "diag", &d) == 0) //d will be released when releasing json object
        {
            diag = std::string(d);
            HWST_DBG("diag:" + diag);

            status = 9;
            apiLock.lock();
            it = byHInstanceMap.find(diag);
            if (it != byHInstanceMap.end())
            {
                HWST_DBG("found diag:" + it->second->name);
                HWST_DBG("status:" + estatus);
                HWST_DBG("filterenable:" + std::to_string(filterenable));
                HWST_DBG("filterstatus:" + std::to_string(fstatus));
                HWST_DBG("data:");
                it->second->setFinished(estatus, data);
                it->second->setFilterStatus(filterenable, fstatus);
                HWST_DBG("setFinished");
                byHInstanceMap.erase(it);
                HWST_DBG("erased");
                status = 0;
            }
            apiLock.unlock();
        }
    }
    else
    {
        HWST_DBG("unknown method");
        status = 10;
    }

end:
    return status;
}

} // namespace hwst
