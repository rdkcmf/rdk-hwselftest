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

Diagsys.evType = {
        open:0,
        close:1,
        message:2,
        error:3
};

Diagsys.cbType = {
        status:0,
        progress:1,
        log:2,
        errLog:3,
        eod:4
};

Diagsys.cbStatus = {
        issued: 0,
        started: 1,
        error: 2
};

function Diagsys(wsEvent, callback)
{
    this.jsonId = 0;
    this.requests = {};
    this.instances = {};
    this.websocket = null;
    this.callback = callback;
    var that = this;

    this.wsOpenHandler = function (evt) {
        this.requests = {};
        this.instances = {};

        if (typeof wsEvent === 'function') {
            wsEvent(Diagsys.evType.open, evt);
        }
    };

    this.wsCloseHandler = function (evt) {
        if (typeof wsEvent === 'function') {
            wsEvent(Diagsys.evType.close, evt);
        }
    };

    this.wsErroreHandler = function (evt) {
        if (typeof wsEvent === 'function') {
            wsEvent(Diagsys.evType.error, evt);
        }
    };

    this.wsMessageHandler = function (evt) {
        if (typeof wsEvent === 'function')
        {
            if(wsEvent(Diagsys.evType.message, evt) === true) {
                return;  //skip if handled by the event callback
            }
        }

        try {
            json = JSON.parse(evt.data);
        }
        catch(e) {
            console.log("Invalid json.");
            return;
        }

        var keys = Object.keys(json);
        var vals = Object.values(json);
        var index;
        var idxId = 0;
        var diagId;
        var jsonId;
        var diag;
        var params;
        var method;
        var instance;
        var progress;
        var status;
        var log, errorLog;
        var data;
        var error;

        //console.log("keys: " + keys);
        if(((index = keys.indexOf("jsonrpc")) < 0) || (vals[index] !== "2.0")) {
            console.log("jsonrpc 2.0 missing.");
            return;
        }

        if((idxId = keys.indexOf("id")) < 0) {
            // Id must be present
            console.log("Id missing.");
            return;
        }

        jsonId = vals[idxId];
        //console.log("jsonId: " + jsonId + ", idxId:" + idxId);

        if((index = keys.indexOf("result")) >= 0) {
            //console.log("RESULT");
            if(jsonId === null) {
                console.log("Invalid json id (null)");
                return;
            }

            if(typeof that.requests[jsonId] === 'undefined') {
                console.log("Unknown reference.");
                return;
            }

            diagId = that.requests[jsonId].cookie;
            //console.log("result: " + index);
            //console.log("result for id: " + diagId);
            //console.log("result for diag: " + that.requests[jsonId].diag);

            params = vals[index];

            if((typeof params !== 'object') || ((index = Object.keys(params).indexOf("diag")) < 0)) {
                //console.log("Result missing diag param.");
                that.cb(Diagsys.cbType.status, that.requests[jsonId].cookie, Diagsys.cbStatus.error);
                delete that.requests[jsonId];
                return;
            }

            diag = Object.values(params)[index];
            //console.log("diag: " + diag);

            if((index = Object.keys(params).indexOf("message")) >= 0) {
                data = Object.values(params)[index];
                //console.log("error message: " + JSON.stringify(data));
                that.cb(Diagsys.cbType.errLog, that.requests[jsonId].cookie, data);
            }

            if(diag === null) {
                //console.log("Diag startup error.");
                that.cb(Diagsys.cbType.status, that.requests[jsonId].cookie, Diagsys.cbStatus.error);
                delete that.requests[jsonId];
                return;
            }

            if(typeof that.instances[diag] !== 'undefined') {
                //console.log("Response for already existing diag.");
                that.cb(Diagsys.cbType.status, that.requests[jsonId].cookie, Diagsys.cbStatus.error);
                delete that.requests[jsonId];
                return;
            }

            that.instances[diag] = that.requests[jsonId];
            delete that.requests[jsonId];
            that.cb(Diagsys.cbType.status, that.instances[diag].cookie, Diagsys.cbStatus.started);
            return;
        } else if((index = keys.indexOf("method")) >= 0) {
            //console.log("method: " + index);

            method = vals[index];
            //console.log("method: " + method);

            if(typeof (instance = that.instances[method]) !== 'undefined') {
                // diag instance
                //console.log("method for instance: " + instance.cookie + "[" + that.instances.diag + "]");

                if(jsonId !== null) {
                    // for now ignore messages (so methods with id=null), handle only
                    //console.log("method with id - ignoring");
                    return;
                }

                if((index = keys.indexOf("params")) >= 0) {
                    params = vals[index];
                    //console.log("params: " + params);

                    if(typeof params !== 'object') {
                        console.log("method with invalid params - ignore");
                        return;
                    }

                    if((index = Object.keys(params).indexOf("progress")) >= 0) {
                        progress = Object.values(params)[index];
                        //console.log("progress: " + progress);
                        that.cb(Diagsys.cbType.progress, instance.cookie, progress);
                    }

                    if((index = Object.keys(params).indexOf("log")) >= 0) {
                        log = Object.values(params)[index];
                        //console.log("log: " + log);
                        that.cb(Diagsys.cbType.log, instance.cookie, log);
                    }

                    if((index = Object.keys(params).indexOf("error")) >= 0) {
                        errorLog = Object.values(params)[index];
                        //console.log("errorLog: " + errorLog);
                        that.cb(Diagsys.cbType.errLog, instance.cookie, errorLog);
                    }

                    // ToDo: HANDLE SOME OTHER PARAMS HERE, e.g. hdd model info
                }
                return;
            } else if(method === "eod") {
                //console.log("eod");

                if(jsonId !== null) {
                    console.log("eod with id - ignoring");
                    return;
                }

                if((index = keys.indexOf("params")) < 0) {
                    console.log("eod missing params");
                    return;
                }

                params = vals[index];
                //console.log("params: " + params);

                if(typeof params !== 'object') {
                    console.log("eod with invalid params - ignore");
                    return;
                }

                if((index = Object.keys(params).indexOf("status")) < 0) {
                    console.log("eod - params missing status - ignore");
                    return;
                }
                status = Object.values(params)[index];
                //console.log("status: " + status);

                if((index = Object.keys(params).indexOf("diag")) < 0) {
                    console.log("eod - params missing diag - ignore");
                    return;
                }

                diag = Object.values(params)[index];
                //console.log("diag: " + diag);

                if(typeof (instance = that.instances[diag]) === 'undefined') {
                    console.log("eod - params invalid diag instance - ignore");
                    return;
                }

                //console.log("eod for instance: " + instance.cookie + "[" + instance.diag + "]");

                if((index = Object.keys(params).indexOf("data")) >= 0) {
                    data = Object.values(params)[index];
                    //console.log("data: " + JSON.stringify(data));

                    if(status === 0) {
                        that.cb(Diagsys.cbType.log, instance.cookie, data);
                    } else {
                        that.cb(Diagsys.cbType.errLog, instance.cookie, status, data);
                    }
                } else {
                    data = null;
                    if(status !== 0) {
                        that.cb(Diagsys.cbType.errLog, instance.cookie, status);
                    }
                }

                that.cb(Diagsys.cbType.eod, instance.cookie, status, data);
                delete that.instances[diag];
                return;
            }
        } else if((index = keys.indexOf("error")) >= 0) {
            //console.log("error response");
            if(jsonId !== null) {
                //console.log("error with id: " + jsonId);

                if(typeof that.requests[jsonId] !== 'undefined') {
                    diagId = that.requests[jsonId].cookie;
                    //console.log("error for diag: " + index);
                    delete that.requests[jsonId];
                    that.cb(Diagsys.cbType.status, diagId, Diagsys.cbStatus.error);

                    error = vals[index];
                    if((index = Object.keys(error).indexOf("code")) >= 0) {
                        status = Object.values(error)[index];
                        //console.log("error code: " + status);

                        if((index = Object.keys(error).indexOf("message")) < 0) {
                            console.log("missing error message");
                            return;
                        }
                        data = Object.values(error)[index];
                        //console.log("error message: " + JSON.stringify(data));
                        that.cb(Diagsys.cbType.errLog, diagId, status, data);
                    }
                    return;
                }
                return;
            }

            return;
        } else {
            console.log("UNKNOWN");
        }
    };

    this.cb = function(type, cookie, params) {
        if (typeof that.callback === 'function') {
            that.callback.apply(null, arguments);
        }
    };
}

Diagsys.prototype.connect = function(addr) {
    //console.log("CONNECT");
    if(this.websocket !== null) {
        return;
    }

    this.websocket = new WebSocket(addr);
    console.log(this.websocket);
    this.websocket.onopen = this.wsOpenHandler; 
    this.websocket.onclose = this.wsCloseHandler;
    this.websocket.onmessage = this.wsMessageHandler;
    this.websocket.onerror = this.wsErrorHandler;
};

Diagsys.prototype.disconnect = function() {
    if(this.websocket === null) {
        return;
    }

    this.websocket.onopen = function () {}; 
    this.websocket.onclose = function () {};
    this.websocket.onmessage = function () {};
    this.websocket.onerror = function () {};
    this.websocket.close();
    delete this.websocket;
};

Diagsys.prototype.setCallback = function(callback) {
    this.callback = callback;
};

Diagsys.prototype.issue = function(cookie, diag, params) {
    if(this.websocket === null) {
        return false;
    }
    
    if(cookie == null) {
        return Diagsys.prototype.notify(diag, params);
    }
    
    for(var i=0; typeof this.requests[this.jsonId] !== 'undefined'; ++i, ++this.jsonId) {
        if(i === Number.MAX_SAFE_INTEGER) {
            return false;
        }
    }

    //console.log("Issue(" + cookie + "," + diag + "," + params + ")" + "jsonId:" + this.jsonId);

    this.requests[this.jsonId] = {'cookie':cookie, 'diag':diag, 'params':params};

    this.cb(Diagsys.cbType.status, cookie, Diagsys.cbStatus.issued);

    if(typeof params !== 'undefined') {
        this.websocket.send('{"jsonrpc": "2.0", "method": "' + diag + '", "id": ' + this.jsonId + ', "params":' + JSON.stringify(params) + '}');
    } else {
        this.websocket.send('{"jsonrpc": "2.0", "method": "' + diag + '", "id": ' + this.jsonId + '}');
    }

    ++this.jsonId;

    return true;
};

Diagsys.prototype.notify = function(diag, params) {
    if(this.websocket === null) {
        return false;
    }

    if(typeof params !== 'undefined') {
        this.websocket.send('{"jsonrpc": "2.0", "method": "' + diag + '", "id": null, "params":' + JSON.stringify(params) + '}');
    } else {
        this.websocket.send('{"jsonrpc": "2.0", "method": "' + diag + '", "id": null}');
    }

    return true;
};

Diagsys.prototype.cancell = function(cookie) {
    if(this.websocket === null) {
        return false;
    }

    for(var i in this.instances) {
        if(this.instances[i].cookie === cookie) {
            this.websocket.send('{"jsonrpc": "2.0", "method": "DIAG", "id": null, "params": {"break": "' + i + '"}}');
            return true;
        }
    }
    return false;
};
