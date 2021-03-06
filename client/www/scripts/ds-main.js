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
var CLIENT_VERSION = "0011";
var client_name = "client ver. " + CLIENT_VERSION;
var screen1SysinfoTimeout = 20; //seconds, 0: disabled
var screen1Timeout = 300; //seconds, 0: disabled
var screen2InprogressTimeout = 180; //seconds, 0: disabled
var screen2Timeout = 600; //seconds, 0: disabled

var wsAddress = "ws://127.0.0.1:8002";

if(typeof Object.values !== 'function') {
    Object.values = function(object) {
        var values = [];
        for(var property in object) {
            values.push(object[property]);
        }
        return values;
    };
}

var sysinfoTimer = null;
var capabilitiesTimer = null;
var prevResultsCheckTimer = null;
var inactivityTimer = null;
var inprogressTimer = null;
var cancelling = false;
var previousResultsAvailable = false;

var json_store = {};

var telemetry_swap = {};
var telemetry_order = {
    'Hard Drive': ['HDD', 'N'],
    'Flash Memory': ['Flash', 'N'],
    'SD Card': ['SDCard', 'N'],
    'Dynamic RAM': ['DRAM', 'N'],
    'HDMI Output': ['HDMI', 'N'],
    'Cable Card': ['CableCard', 'N'],
    'RF Remote Interface': ['RFRemote', 'N'],
    'IR Remote Interface': ['IRremote', 'N'],
    'MoCA': ['MoCA', 'N'],
    'Audio/Video Decoder': ['AVDecoder', 'N'],
    'Video Tuner': ['QAMTuner', 'N'],
    'Cable Modem': ['CableModem', 'N'],
    'Bluetooth': ['BTLE', 'N'],
    'WiFi': ['WiFi', 'N'],
    'WAN': ['WAN', 'N']
};

/*
    GroupName1: {
    DiagName1: [ParamsSet1], [ParamsSet2], ...],
    DiagName2: [ParamsSet1], [ParamsSet2], ...]
    }
 */
var diagGroupsAll = {
    'Hard Drive': {'hdd_status': []},
    'SD Card': {'sdcard_status': []},
    'Flash Memory': {'flash_status': []},
    'Dynamic RAM': {'dram_status': []},
    'HDMI Output': {'hdmiout_status': []},
    'Cable Card': {'mcard_status': []},
    'IR Remote Interface': {'ir_status': []},
    'RF Remote Interface': {'rf4ce_status': []},
    'MoCA': {'moca_status': []},
    'Audio/Video Decoder': {'avdecoder_qam_status': []},
    'Video Tuner': {'tuner_status': []},
    'Cable Modem': {'modem_status': []},
    'Bluetooth': {'bluetooth_status': []},
    'WiFi': {'wifi_status': []},
    'WAN': {'wan_status': []}
};

var resultsFileDiag = {
    'hdd_status': 'HDD',
    'sdcard_status': 'SDCard',
    'flash_status': 'FLASH',
    'dram_status': 'DRAM',
    'hdmiout_status': 'HDMI',
    'mcard_status': 'CableCard',
    'ir_status': 'IRR',
    'rf4ce_status': 'RFR',
    'moca_status': 'MOCA',
    'avdecoder_qam_status': 'AVDecoder',
    'tuner_status': 'QAM',
    'modem_status': 'DOCSIS',
    'bluetooth_status': 'BTLE',
    'wifi_status': 'WiFi',
    'wan_status': 'WAN'
};

var resultsFilter = {"enable" : 0, "show" : 0, "type" : "instant"};

/* to be adjusted when reading capabilities */
var diagGroups = diagGroupsAll;

/* These numbers match the id-s assigned by buildOperationGroup().
   So first diag name present in diagGroups gets id=1 and so on.
 */
var diagOrder = {
    0: [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 13, 14, 15],
    10: [11] //first av then tuners
};

var ds = null;

//window.onload = function() {
function dsOnload() {
    document.getElementById('version').innerHTML = client_name;
    if(ds === null) {
        window.top.processKey = window.top.processKeyDummy;
        enableInit();
        ds = new Diagsys(wsEvent, dsCallbackInit);
        setSysinfoTimer(screen1SysinfoTimeout);
        ds.connect(wsAddress);
    }
}

//window.onbeforeunload = function() {
function dsOnbeforeunload() {
    if(ds !== null) {
        window.top.processKey = window.top.processKeyDummy;
        ds.disconnect();
        ds = null;
    }
}

function setSysinfoTimer(timeout) {
    if(sysinfoTimer != null) {
        clearTimeout(sysinfoTimer);
    }
    if(timeout > 0) {
        sysinfoTimer = setTimeout(agentMissing, timeout*1000);
    }
}

function setCapabilitiesTimer(timeout) {
    if(capabilitiesTimer != null) {
        clearTimeout(capabilitiesTimer);
    }
    if(timeout > 0) {
        capabilitiesTimer = setTimeout(capabilitiesMissing, timeout*1000);
    }
}

function setPrevResultsCheckTimer(timeout) {
    if(prevResultsCheckTimer != null) {
        clearTimeout(prevResultsCheckTimer);
    }
    if(timeout > 0) {
        prevResultsCheckTimer = setTimeout(prevResultsMissing, timeout*1000);
    }
}

function setInactivityTimer(timeout) {
    if(inactivityTimer != null) {
        clearTimeout(inactivityTimer);
    }
    if(timeout > 0) {
        inactivityTimer = setTimeout(dsExit, timeout*1000);
    }
}

function setInprogressTimer(timeout) {
    if(inprogressTimer != null) {
        clearTimeout(inprogressTimer);
    }
    if(timeout > 0) {
        inprogressTimer = setTimeout(cancellAll, timeout*1000);
    }
}

function setPrevResultsAvailable(value) {
    previousResultsAvailable = value;
    setPrevResultsAvailable = function (){};
}

function capabilitiesMissing() {
    ds.setCallback(null);
    setCapabilitiesTimer(0);
    dbgWrite("capabilitiesMissing: timeout occured before the results arrived");
}

function prevResultsMissing() {
    ds.setCallback(null);
    setPrevResultsCheckTimer(0);
    setPrevResultsAvailable(false);
    dbgWrite("prevResultsMissing: timeout occured before the results arrived");
    setInactivityTimer(screen1Timeout);
    enableStartbuttons();
}

function prevResultsPresent() {
    setPrevResultsAvailable(true);
}

function findIndexByDiagName(name,data) {
    if(typeof data !== 'object') {
        dbgWrite("findIndexByDiagName: data is not an object!");
        return -1;
    }

    var index = -1;

    if((index = Object.keys(data).indexOf(name)) < 0) {
        dbgWrite("findIndexByDiagName: index of" + name + " not found!");
        return -1;
    } else {
        //dbgWrite("findIndexByDiagName: index of" + name + " is: " + index);
        return index;
    }
}

function readCapabilities(caps, allDiags) {
    var found = false;
    var availDiags = {};

    capsDiags = caps["diags"];
    for(var cd in capsDiags) {
        var newEntry = true;
        for(var d in availDiags) {
            if (availDiags[d].hasOwnProperty(capsDiags[cd])) {
                newEntry = false;
                break;
            }
        }
        if(newEntry == true) {
            for(var d in allDiags) {
                if (allDiags[d].hasOwnProperty(capsDiags[cd])) {
                dbgWrite("readCapabilities: " + d + " available: " + capsDiags[cd]);
                availDiags[d] = allDiags[d];
                found = true;
                break;
                }
            }
        }
    }

    // consider no diags found as error; then return all diags
    if (!found) {
        dbgWrite("error on capabilities reading, returning all diags");
        availDiags = allDiags;
    }

    return availDiags;
}

function capabilitiesOrder(diags) {
    var newOrder = {0:[]};
    var i = 1;
    var avId=0;
    var qamId=0;

    for(var g in diags) {
        for(var d in diags[g]) {
            switch(d) {
            case 'tuner_status':
                qamId = i;
                break;
            case 'avdecoder_qam_status':
                avId = i;
            default:
                newOrder[0].push(i);
            }
            ++i;
        }
    }

    if(qamId != 0) {
        if(avId === 0) {
            newOrder[avId].push(qamId);
        } else {
            newOrder[avId] = [qamId];
        }
    }
    return newOrder;
}

function readPreviousResults(data) {
    //dbgWrite("readPreviousResults: raw data: " + JSON.stringify(data));
    //dbgWrite("readPreviousResults: keys and values: " + "(" + Object.keys(data).length + " found" + ")");

    //dbgWrite("readPrevResults: enter");
    if(typeof data !== 'object') {
        dbgWrite("readPrevResults: exit, not an object data");
        setPrevResultsAvailable(false);
        return;
    }

    var index;
    var value;

    index = findIndexByDiagName("results_valid", data);
    if(index < 0) {
        //dbgWrite("readPreviousResults: exit, results_valid index not found");
        setPrevResultsAvailable(false);
        return;
    }
    //dbgWrite("readPreviousResults: results_valid index: " + index);

    value = Object.values(data)[index];
    if(value !== 0) {
        //dbgWrite("readPreviousResults: exit, results_valid " + value + " (no previous results)");
        setPrevResultsAvailable(false);
        return;
    }
    //dbgWrite("readPreviousResults: results_valid: " + value);

    index = findIndexByDiagName("results_type", data);
    if(index < 0) {
        //dbgWrite("readPreviousResults: exit, results_type index not found");
        setPrevResultsAvailable(false);
        return;
    }
    //dbgWrite("readPreviousResults: results_type index: " + index);

    resultsFilter.type = Object.values(data)[index];
    //dbgWrite("readPreviousResults: results_type: " +  resultsFilter.type);

/*    index = findIndexByDiagName("start_time",data);
    if(index < 0) {
        //dbgWrite("readPreviousResults: exit, start_time index not found");
        setPrevResultsAvailable(false);
        return;
    }
    var startTimeVal = Object.values(data)[index];
    //dbgWrite("readPreviousResults: start_time index = " + index + ", value = " + startTimeVal);
*/
    index = findIndexByDiagName("local_time",data);
    if(index < 0) {
        //dbgWrite("readPreviousResults: local_time index not found");
        setPrevResultsAvailable(false);
        return;
    }
    var endTimeVal = Object.values(data)[index];
    var formatEndTime = endTimeVal.replace(/-/g, "/") + " UTC";
    var t_msec = Date.parse(formatEndTime);
    var local_time = new Date(t_msec);
    var dtime = timeStampUI(local_time);
    document.getElementById('timestamp').innerHTML = dtime + " " + timeZone() ;
    //dbgWrite("readPreviousResults: local_time index = " + index + ", value = " + endTimeVal);

    buildOperationGroup(diagGroups);
    tableCreateGroupProgress();
    tableCreateGroupInfos();

    for (var e in data.results) {
        elem = elemByElemName(e);
        if(elem === null) {
            dbgWrite("readPrevResults: unknown diag: " + e);
            continue;
        }
        setPrevResult(elem, data.results[e].r);
    }
    if(!checkAllFinished()) {
        //dbgWrite("readPrevResults: missing diags.");
        setPrevResultsAvailable(false);
        return;
    }

    setPrevResultsAvailable(true);

    //dbgWrite("readPrevResults: exit");
}

/************ Init page *************/

function wsEvent(type, evt) {
    //dbgWrite("wsEvent(" + type + "," + evt + ")");

    switch(type) {
    case Diagsys.evType.message:
        //do not handle message here
        break;
    case Diagsys.evType.open:
        ds.issue(1, "sysinfo_info");
        break;
    case Diagsys.evType.close:
        agentMissing();
        break;
    case Diagsys.evType.error:
        logFileWrite(DIAG_ERRCODE.WEBSOCKET_CONNECTION_FAILURE);
        break;
    }
}

function dsCallbackInit(type, cookie, params) {
    //dbgWrite("dsCallbackInit(" + type + "," + cookie + "," + params + ")");
    if(type === Diagsys.cbType.log) {
        tableCreateInit(params);
        tableCreateSystemInfo(params);
        tableCreateMocaInfo(params);
        tableCreateTunerInfo(params);
        tableCreateWiFiInfo(params);
        enableSystemData();
    }
    else if((type === Diagsys.cbType.eod) && (params === 0)) {
        setSysinfoTimer(0);
        ds.setCallback(null);
        ds.setCallback(dsCallbackCapabilities);
        setCapabilitiesTimer(screen1SysinfoTimeout);
        ds.issue(2, "capabilities_info");
    }
}

function dsCallbackCapabilities(type, cookie, params) {
    //dbgWrite("dsCallbackCapabilities(" + type + "," + cookie + "," + params + ")");

    if(type === Diagsys.cbType.log) {
        diagGroups = readCapabilities(params, diagGroupsAll);
        diagOrder = capabilitiesOrder(diagGroups);
    }
    else if((type === Diagsys.cbType.eod) && (params === 0)) {
        setCapabilitiesTimer(0);
        ds.setCallback(null);
        ds.setCallback(dsCallbackPrevResults);
        setPrevResultsCheckTimer(screen1SysinfoTimeout);
        ds.issue(2, "previous_results");
    }
}

function dsCallbackPrevResults(type, cookie, params) {
    //dbgWrite("dsCallbackPrevResults(" + type + "," + cookie + "," + params + ")");

    if(type === Diagsys.cbType.log) {
        readPreviousResults(params);
    }
    else if((type === Diagsys.cbType.eod) && (params === 0)) {
        //disableInit();
        setPrevResultsCheckTimer(0);
        ds.setCallback(null);
        setInactivityTimer(screen1Timeout);
        enableStartbuttons();
    }
}

function agentMissing() {
    ds.setCallback(null);
    ds.disconnect();
    var store = document.getElementById('devinfo');
    store.style.textAlign = 'center';
    store.innerHTML="WARNING:<br>Test Did Not Run - Please Retry";
    logWrite("Test Did Not Run - Please Retry.");
    logFileWrite(DIAG_ERRCODE.MULTIPLE_CONNECTIONS_NOT_ALLOWED); // Checking only for multiple connection issue and log the message into log file if it was a multiple connection
    setTimeout(uiExit, 3000);
}

function refreshData() {
    ds.setCallback(dsCallbackInit);
    ds.issue(1, "sysinfo_info");
    enableRefreshMessage();
}

function tableCreateInit(data) {
    if(typeof data !== 'object') {
        return;
    }

    data["Ver"] = "C" + CLIENT_VERSION + "A" + data["Ver"];
    json_store = data;

    var tmp = document.getElementById('devinfo_table');
    if (tmp && tmp.hasChildNodes() === true) {
        tmp.parentNode.removeChild(tmp);
    }

    var store = document.getElementById('devinfo');
    var tbl = document.createElement('table');
    tbl.id = 'devinfo_table';
    //tbl.style.width = '60%';
    tbl.className = 'frames';
    var tbdy = document.createElement('tbody');

    data["Model"] = data["Vendor"] + " " + data["Model"];

    var dataSerial = data["Serial"];
    data["Serial"] = data["Serial"] + "\xa0\xa0\xa0\xa0\xa0\xa0\xa0\xa0\xa0\xa0\xa0\xa0\xa0\xa0\xa0\xa0 MAC \xa0\xa0" + data["MAC"];

    for (var i = 0; i < Object.keys(data).length; i++) {
        switch(Object.keys(data)[i]) {
        case "Model":
        case "Serial":
        case "RDK":
            var tr = document.createElement('tr');
            //tr.style.border = '0';
            var td = document.createElement('td');
            td.style.textAlign = 'right';
            td.style.width = '26%';
            td.style.padding = '0 10px 0 0';
            td.appendChild(document.createTextNode(JSON.stringify(Object.keys(data)[i]).replace(/"/g, '')));
            tr.appendChild(td);

            td = document.createElement('td');
            td.style.textAlign = 'left';
            td.style.padding = '0 0 0 8px';
            td.style.textOverflow = 'ellipsis';
            td.style.overflow = 'hidden';
            td.appendChild(document.createTextNode(JSON.stringify(Object.values(data)[i]).replace(/"/g, '')));
            tr.appendChild(td);
            tbdy.appendChild(tr);
            break;
        case "Ver":
            document.getElementById('version').innerHTML = "Ver. " + Object.values(data)[i];
            break;
        }
    }
    tbl.appendChild(tbdy);
    store.appendChild(tbl);

    data["Serial"] = dataSerial;
}

function tableCreateSystemInfo(data) {
    if(typeof data !== 'object') {
        return;
    }

    var tmp = document.getElementById('sysinfo_table');
    if (tmp && tmp.hasChildNodes() === true) {
        tmp.parentNode.removeChild(tmp);
    }

    var store = document.getElementById('boxinfo');
    var tbl = document.createElement('table');
    tbl.id = 'sysinfo_table';
    tbl.className = 'infotable';
    tbl.style.width = '100%';
    var tbdy = document.createElement('tbody');

    for (var i = 0; i < Object.keys(data).length; i++) {
        switch(Object.keys(data)[i]) {
        case "Home Network":
        case "xConf Version":
        case "eSTB IP":
        case "Receiver ID":
        case "Number of Channels":
        case "Device Temperature":
        case "Time Zone":
            var tr = document.createElement('tr');
            tr.style.border = '5px';
            var td = document.createElement('td');
            td.style.textAlign = 'right';
            td.style.fontSize = 'smaller';
            td.style.padding = '0 10px 3px 0';
            td.style.width = '20%';
            td.appendChild(document.createTextNode(JSON.stringify(Object.keys(data)[i]).replace(/"/g, '')));
            tr.appendChild(td);

            td = document.createElement('td');
            td.style.textAlign = 'left';
            td.style.padding = '0 0 3px 8px';
            td.style.fontSize = 'smaller';
            td.appendChild(document.createTextNode(JSON.stringify(Object.values(data)[i]).replace(/"/g, '')));
            tr.appendChild(td);
            tbdy.appendChild(tr);
            break;
    }
    }
    tbl.appendChild(tbdy);
    store.appendChild(tbl);
}

function tableCreateMocaInfo(data) {
    if(typeof data !== 'object') {
        return;
    }

    var tmp = document.getElementById('mocainfo_table');
    if (tmp && tmp.hasChildNodes() === true) {
        tmp.parentNode.removeChild(tmp);
    }

    var store = document.getElementById('mocainfo');
    var tbl = document.createElement('table');
    tbl.id = 'mocainfo_table';
    tbl.className = 'infotable';
    var tbdy = document.createElement('tbody');

    for (var i = 0; i < Object.keys(data).length; i++) {
        switch(Object.keys(data)[i]) {
        case "MoCA RF Channel":
        case "MoCA NC":
        case "MoCA Bitrate":
        case "MoCA SNR":
            var tr = document.createElement('tr');
            tr.style.border = '5px';
            var td = document.createElement('td');
            td.style.textAlign = 'right';
            td.style.fontSize = 'smaller';
            td.style.padding = '0 10px 3px 0';
            td.style.width = '33%';
            td.appendChild(document.createTextNode(JSON.stringify(Object.keys(data)[i]).replace(/"/g, '')));
            tr.appendChild(td);

            td = document.createElement('td');
            td.style.textAlign = 'left';
            td.style.padding = '0 0 3px 8px';
            td.style.fontSize = 'smaller';
            td.appendChild(document.createTextNode(JSON.stringify(Object.values(data)[i]).replace(/"/g, '')));
            tr.appendChild(td);
            tbdy.appendChild(tr);
            break;
    }
    }
    tbl.appendChild(tbdy);
    store.appendChild(tbl);
}

function tableCreateTunerInfo(data) {
    if(typeof data !== 'object') {
        return;
    }

    var tmp = document.getElementById('tunerinfo_table');
    if (tmp && tmp.hasChildNodes() === true) {
        tmp.parentNode.removeChild(tmp);
    }

    var store = document.getElementById('tunerinfo');
    var tbl = document.createElement('table');
    tbl.id = 'tunerinfo_table';
    tbl.className = 'infotable';
    var tbdy = document.createElement('tbody');

    for (var i = 0; i < Object.keys(data).length; i++) {
        switch(Object.keys(data)[i]) {
        case "DOCSIS State":
        case "DOCSIS DwStrmPower":
        case "DOCSIS UpStrmPower":
        case "DOCSIS SNR":
        case "Video Tuner Locked":
        case "Video Tuner Power":
        case "Video Tuner SNR":
            var tr = document.createElement('tr');
            tr.style.border = '5px';
            var td = document.createElement('td');
            td.style.textAlign = 'right';
            td.style.fontSize = 'smaller';
            td.style.padding = '0 10px 3px 0';
            td.style.width = '33%';
            td.appendChild(document.createTextNode(JSON.stringify(Object.keys(data)[i]).replace(/"/g, '')));
            tr.appendChild(td);

            td = document.createElement('td');
            td.style.textAlign = 'left';
            td.style.padding = '0 0 3px 8px';
            td.style.fontSize = 'smaller';
            if (Object.keys(data)[i] == "DOCSIS State") {
                var img = document.createElement("img");
                img.src = "resources/icon-warning-18.png";
                td.appendChild(img);
                td.appendChild(document.createTextNode("\xa0\xa0\xa0\xa0"));
            }
            td.appendChild(document.createTextNode(JSON.stringify(Object.values(data)[i]).replace(/"/g, '')));
            tr.appendChild(td);
            tbdy.appendChild(tr);
            break;
    }
    }
    tbl.appendChild(tbdy);
    store.appendChild(tbl);
}

function tableCreateWiFiInfo(data) {
    if(typeof data !== 'object') {
        return;
    }

    var tmp = document.getElementById('wifi_info_table');
    if (tmp && tmp.hasChildNodes() === true) {
        tmp.parentNode.removeChild(tmp);
    }

    var store = document.getElementById('wifi_info');
    var tbl = document.createElement('table');
    tbl.id = 'wifi_info_table';
    tbl.className = 'infotable';
    var tbdy = document.createElement('tbody');

    for (var i = 0; i < Object.keys(data).length; i++) {
        switch(Object.keys(data)[i]) {
        case "WiFi SSID":
        case "WiFi SSID MAC":
        case "WiFi Op Frequency":
        case "WiFi Signal Strength":
            var tr = document.createElement('tr');
            tr.style.border = '5px';
            var td = document.createElement('td');
            td.style.textAlign = 'right';
            td.style.fontSize = 'smaller';
            td.style.padding = '0 10px 3px 0';
            td.style.width = '33%';
            td.appendChild(document.createTextNode(JSON.stringify(Object.keys(data)[i]).replace(/"/g, '')));
            tr.appendChild(td);

            td = document.createElement('td');
            td.style.textAlign = 'left';
            td.style.padding = '0 0 3px 8px';
            td.style.fontSize = 'smaller';
            td.appendChild(document.createTextNode(JSON.stringify(Object.values(data)[i]).replace(/"/g, '')));
            tr.appendChild(td);
            tbdy.appendChild(tr);
            break;
    }
    }
    tbl.appendChild(tbdy);
    store.appendChild(tbl);
}

function enableInit() {
    window.top.processKey = window.top.processKeyDummy;
    document.getElementById('section_init').style.display = 'block';
}

function disableInit() {
    document.getElementById('section_init').style.display = 'none';
}

function enableSystemData() {
    document.getElementById('system_data').style.display = 'block';
}

function disableSystemData() {
    document.getElementById('system_data').style.display = 'none';
}

function enableRefreshMessage() {
    document.getElementById('section_message').style.display = 'block';
}

function disableRefreshMessage() {
    document.getElementById('section_message').style.display = 'none';
}

/************ Startbutton page *************/

function dsRunPriv() {
    cancelled = false;
    telemetry_swap = telemetry_order;
    resultsFilter.type = "instant";
    setInactivityTimer(0);
    ds.notify("TESTRUN", { state: "start", client: client_name } );
    setInprogressTimer(screen2InprogressTimeout);

    disableStartbuttons();
    disableSystemData();
    enableInprogress();
}

function dsRun() {
    ds.notify("LOG", { message: "Test execution start, " + client_name });
    dsRunPriv();
}

function dsShowPriv() {
    cancelled = false;
    setInactivityTimer(0);
    setInprogressTimer(screen2InprogressTimeout);

    disableStartbuttons();
    disableSystemData();
    enableShowprevious();
}

function dsShow() {
    ds.notify("LOG", { message: "Show previous results, client ver. " + CLIENT_VERSION});
    dsShowPriv();
}

function showSystemData() {
    disableInprogress();
    prevResultsPresent();
    enableSystemData();
    enableStartbuttons();
    disableSummary();
}

/************ startbuttons run or exit section *************/
var startSectionChoiceInitialized = false;
var startButtonMarkers = {};
var startButtonGrids = {};

function doBuildChoiceSection(b1,b1input,b2,b2input,b3,b3input,markers,grids) {

    var store = document.getElementById(b1);
    markers.resultFailed = [];
    markers.resultFailed.push(new ButtonMarker('resultbuttons'));
    var input = document.getElementById(b1input);
    markers.resultFailed[0].td.appendChild(input);
    store.appendChild(markers.resultFailed[0].table);

    store = document.getElementById(b2);
    markers.resultFailed.push(new ButtonMarker('resultbuttons'));
    input = document.getElementById(b2input);
    markers.resultFailed[1].td.appendChild(input);
    store.appendChild(markers.resultFailed[1].table);

    store = document.getElementById(b3);
    markers.resultFailed.push(new ButtonMarker('resultbuttons'));
    input = document.getElementById(b3input);
    markers.resultFailed[2].td.appendChild(input);
    store.appendChild(markers.resultFailed[2].table);

    grids.resultFailed = new ButtonGrid(3, markers.resultFailed);
}

function startBuildChoiceSection() {

    if(startSectionChoiceInitialized) {
        return;
    }

    if(previousResultsAvailable) {
        doBuildChoiceSection("start_choice_soe_b1","soeBtnShow","start_choice_soe_b2","soeBtnRefresh","start_choice_soe_b3","soeBtnExit",startButtonMarkers,startButtonGrids);
    } else {
        doBuildChoiceSection("start_choice_roe_b1","roeBtnRun","start_choice_roe_b2","roeBtnRefresh","start_choice_roe_b3","roeBtnExit",startButtonMarkers,startButtonGrids);
    }

    startSectionChoiceInitialized = true;
}

function keyhandlerStartbutton(keyCode) {
    switch(keyCode) {
    case KEY_OK:
        var startBtn = startButtonGrids.resultFailed.content();
        if(startBtn !== null) {
                if(typeof startBtn.onclick == 'function') {
                    startBtn.onclick();
                }
            }
            break;

    case KEY_UP:
        setInactivityTimer(screen1Timeout);
        startButtonGrids.resultFailed.move('prev');
        break;

    case KEY_DN:
        setInactivityTimer(screen1Timeout);
        startButtonGrids.resultFailed.move('next');
        break;

    default:
        break;
    }
    keyCode=8;
}

function enableStartbuttons() {

    if(previousResultsAvailable) {
        document.getElementById('section_startbuttons_show_or_exit').style.display = 'block';
    } else {
        document.getElementById('section_startbuttons_run_or_exit').style.display = 'block';
    }

    document.getElementById('section_choice').style.display = 'none';

    disableRefreshMessage();
    startBuildChoiceSection();
    startButtonGrids.resultFailed.move(0);
    window.top.processKey = keyhandlerStartbutton;
}

function disableStartbuttons() {
    window.top.processKey = window.top.processKeyDummy;
    document.getElementById('section_startbuttons_run_or_exit').style.display = 'none';
    document.getElementById('section_startbuttons_show_or_exit').style.display = 'none';
}

/************ Testing page *************/

var results = {
        notrun: 0,
        running: 1,
        cancelling: 2,
        cancelled: 3,
        passed: 4,
        failed: 5,
        disabled: 6,
        pending: 7,
        error: 8,
        warning: 9
};

//var diagRunId = 0;
var groupStatus = {};

function purgeOperationGroup() {
    for(var g in groupStatus) {
        if(typeof g.bar !== 'undefined') {
            delete g.bar;
        }
        if(typeof g.img !== 'undefined') {
            delete g.img;
        }
        if(typeof g.scroller !== 'undefined') {
            g.scroller.stop();
            delete g.scroller;
        }
    }
    groupStatus = {};
}

function buildOperationGroup(source) {
    purgeOperationGroup();

    var grp = Object.keys(source);
    var diagId = 1;
    for (var g in source) {
        groupStatus[g] = {'progress':0, 'result': results.notrun, 'elems':[]};
        groupStatus[g].name = g;
        var groupEntries = Object.keys(source[g]);
        if(groupEntries.length === 0) {
            dbgWrite("readPreviousResults: groupEntries empty");
            groupStatus[g].progress = 100;
            groupStatus[g].result = results.warning;
            continue;
        }
        for(var d in groupEntries) {
            var diagName = groupEntries[d];
            groupStatus[g].elems[d] = {};
            groupStatus[g].elems[d].name = diagName;
            for (var r in resultsFileDiag)
            {
                if (r == diagName)
                    groupStatus[g].elems[d].resultsName = resultsFileDiag[r];
            }
            groupStatus[g].elems[d].progress = 0;
            groupStatus[g].elems[d].log = [];
            groupStatus[g].elems[d].result = results.notrun;
            groupStatus[g].elems[d].filterResult = results.notrun;
            groupStatus[g].elems[d].status = -1;
            groupStatus[g].elems[d].filterStatus = -200;
            groupStatus[g].elems[d].id = diagId;
            diagId++;

            if(typeof source[g][diagName] !== 'undefined') {
                groupStatus[g].elems[d].params = source[g][diagName];
            }
        }
    }
}

function tableCreateGroupProgress() {
    var store = document.getElementById("div_inprogress");
    store.innerHTML = "";
    var tbl = document.createElement('table');
    tbl.id = 'table';
    tbl.className = 'progress';
    //tbl.style.width = '80%';
    //tbl.setAttribute('border', '1px solid');
    var tbdy = document.createElement('tbody');

    var groups = Object.keys(groupStatus);
    var diagId = 0;
    for (var i = 0; i < groups.length; i++) {
        var tr = document.createElement('tr');
        var td = document.createElement('td');
        td.style.width = '50%';
        td.setAttribute('align', 'right');
        td.style.padding = '0 10px 0 0';
        td.appendChild(document.createTextNode(groups[i]));
        td.setAttribute('white-space', 'nowrap');
        tr.appendChild(td);

        td = document.createElement('td');
        //td.style.width = '50%';
        td.style.padding = '0 0 0 10px';
        groupStatus[groups[i]].bar = new Progressbar('progressbar');
        td.appendChild(groupStatus[groups[i]].bar.border);
        tr.appendChild(td);

        // Empty groups set status immediately
        if(groupStatus[groups[i]].progress === 100)
            groupStatus[groups[i]].bar.set(100);

        td = document.createElement('td');
        td.style.width = '8%';
        td.setAttribute('align', 'center');
        groupStatus[groups[i]].img = document.createElement("img");
        groupStatus[groups[i]].img.src = "resources/icon-success-18.png";
        if(groupStatus[groups[i]].progress !== 100)  {
            groupStatus[groups[i]].img.style.visibility = "hidden";
        } else {
            // Empty groups mark as warning
            groupStatus[groups[i]].img.src = "resources/icon-warning-18.png";
        }
        groupStatus[groups[i]].img.className = 'groupstatus';
        td.appendChild(groupStatus[groups[i]].img);
        tr.appendChild(td);

        tbdy.appendChild(tr);
    }
    tbl.appendChild(tbdy);
    store.appendChild(tbl);
}

function tableCreateGroupInfos() {
    var store = document.getElementById("div_infos");
    store.innerHTML = "";
    var tbl = document.createElement('table');
    tbl.id = 'table';
    tbl.className = 'infos';

    var tbdy = document.createElement('tbody');

    var groups = Object.keys(groupStatus);
    for (var i = 0; i < groups.length; i++) {
        var tr = document.createElement('tr');
        var td = document.createElement('td');
        td.innerHTML = "&nbsp;";
        groupStatus[groups[i]].info = td;
        groupStatus[groups[i]].scroller = new ScrollText($(groupStatus[groups[i]].info));
        tr.appendChild(td);
        tbdy.appendChild(tr);
    }
    tbl.appendChild(tbdy);
    store.appendChild(tbl);
    dbgWrite("table create");
}

function getInfo(elemName, status, data) {
    var info = "";
    switch(status) {
    case DIAG_ERRCODE.FAILURE:
        switch(elemName) {
        case "hdd_status":
            info = "Disk Health Status Error";
            break;
        case "mcard_status":
            info = "Invalid Card Certification";
            break;
        case "rf4ce_status":
            info = "Paired RCU Count Exceeded Max Value";
            break;
        case "avdecoder_qam_status":
            info = "Play Status Error";
            break;
        case "tuner_status":
            info = "Read Status File Error";
            break;
        case "modem_status":
            info = "Gateway IP Not Reachable";
            break;
        case "bluetooth_status":
            info = "Bluetooth Not Operational";
            break;
        case "sdcard_status":
        case "flash_status":
        case "dram_status":
            info = "Memory Verify Error";
            break;
        default:
            info = "";
            break;
        }
        break;
    case DIAG_ERRCODE.SUCCESS:
        info = "";
        break;
    case DIAG_ERRCODE.NOT_APPLICABLE:
        info = "Test Not Applicable";
        break;
    case DIAG_ERRCODE.HDD_STATUS_MISSING:
        info = "HDD Test Not Run";
        break;
    case DIAG_ERRCODE.HDMI_NO_DISPLAY:
        info = "No HDMI detected. Verify HDMI cable is connected on both ends or if TV is compatible";
        break;
    case DIAG_ERRCODE.HDMI_NO_HDCP:
        info = "HDMI authentication failed. Try another HDMI cable or check TV compatibility";
        break;
    case DIAG_ERRCODE.MOCA_NO_CLIENTS:
        info = "No MoCA Network Found";
        break;
    case DIAG_ERRCODE.MOCA_DISABLED:
        info = "MoCA OFF";
        break;
    case DIAG_ERRCODE.SI_CACHE_MISSING:
        info = "Missing Channel Map";
        break;
    case DIAG_ERRCODE.TUNER_NO_LOCK:
        info = "Lock Failed - Check Cable";
        break;
    case DIAG_ERRCODE.TUNER_BUSY:
        info = "One or more tuners are busy. All tuners were not tested";
        break;
    case DIAG_ERRCODE.AV_NO_SIGNAL: /* Deprecated (DELIA-48787) */
        info = "No stream data. Check cable and verify STB is provisioned correctly";
        break;
    case DIAG_ERRCODE.IR_NOT_DETECTED:
        info = "IR Not Detected";
        break;
    case DIAG_ERRCODE.CM_NO_SIGNAL:
        info = "Lock Failed - Check Cable";
        break;
    case DIAG_ERRCODE.RF4CE_NO_RESPONSE:
        info = "RF Input Not Detected In Last 10 Minutes";
        break;
    case DIAG_ERRCODE.WIFI_NO_CONNECTION:
        info = "No Connection";
        break;
    case DIAG_ERRCODE.AV_URL_NOT_REACHABLE: /* Deprecated (DELIA-48787) */
        info = "No AV. URL Not Reachable Or Check Cable";
        break;
    case DIAG_ERRCODE.NON_RF4CE_INPUT:
        info = "RF Paired But No RF Input";
        break;
    case DIAG_ERRCODE.RF4CE_CTRLM_NO_RESPONSE:
        info = "RF Controller Issue";
        break;
    case DIAG_ERRCODE.HDD_MARGINAL_ATTRIBUTES_FOUND:
        info = "Marginal HDD Values";
        break;
    case DIAG_ERRCODE.RF4CE_CHIP_DISCONNECTED:
        info = "RF4CE Chip Fail";
        break;
    case DIAG_ERRCODE.HDD_DEVICE_NODE_NOT_FOUND:
        info = "HDD Device Node Not Found";
        break;
    case DIAG_ERRCODE.INTERNAL_TEST_ERROR:
        info = "Test Not Run";
        break;
    case DIAG_ERRCODE.CANCELLED:
        info = "Test Cancelled";
        break;
    case DIAG_ERRCODE.CANCELLED_NOT_STANDBY:
        info = "Test Cancelled. Device not in standby";
        break;
    case DIAG_ERRCODE.NO_GATEWAY_CONNECTION:
        info = "No Local Gateway Connection";
        break;
    case DIAG_ERRCODE.NO_COMCAST_WAN_CONNECTION:
        info = "No Comcast WAN Connection";
        break;
    case DIAG_ERRCODE.NO_PUBLIC_WAN_CONNECTION:
        info = "No Public WAN Connection";
        break;
    case DIAG_ERRCODE.NO_WAN_CONNECTION:
        info = "No WAN Connection. Check Connection";
        break;
    case DIAG_ERRCODE.NO_ETH_GATEWAY_FOUND:
        info = "No Gateway Discovered via Ethernet";
        break;
    case DIAG_ERRCODE.NO_MW_GATEWAY_FOUND:
        info = "No Local Gateway Discovered";
        break;
    case DIAG_ERRCODE.NO_ETH_GATEWAY_CONNECTION:
        info = "No Gateway Response via Ethernet";
        break;
    case DIAG_ERRCODE.NO_MW_GATEWAY_CONNECTION:
        info = "No Local Gateway Response";
        break;
    case DIAG_ERRCODE.AV_DECODERS_NOT_ACTIVE:
        info = "AV Decoders Not Active";
        break;
    case DIAG_ERRCODE.BLUETOOTH_INTERFACE_FAILURE:
        info = "Bluetooth Interfaces Not Found";
        break;
    case DIAG_ERRCODE.FILE_WRITE_OPERATION_FAILURE:
        info = "File Write Operation Error";
        break;
    case DIAG_ERRCODE.FILE_READ_OPERATION_FAILURE:
        info = "File Read Operation Error";
        break;
    case DIAG_ERRCODE.EMMC_TYPEA_MAX_LIFE_EXCEED_FAILURE:
        info = "Device TypeA Exceeded Max Life";
        break;
    case DIAG_ERRCODE.EMMC_TYPEB_MAX_LIFE_EXCEED_FAILURE:
        info = "Device TypeB Exceeded Max Life";
        break;
    case DIAG_ERRCODE.EMMC_TYPEA_ZERO_LIFETIME_FAILURE:
        info = "Device TypeA Returned Invalid Response";
        break;
    case DIAG_ERRCODE.EMMC_TYPEB_ZERO_LIFETIME_FAILURE:
        info = "Device TypeB Returned Invalid Response";
        break;
    case DIAG_ERRCODE.MCARD_AUTH_KEY_REQUEST_FAILURE:
        info = "Card Auth Key Not Ready";
        break;
    case DIAG_ERRCODE.MCARD_HOSTID_RETRIEVE_FAILURE:
        info = "Unable To Retrieve Card ID";
        break;
    case DIAG_ERRCODE.MCARD_CERT_AVAILABILITY_FAILURE:
        info = "Card Certification Not Available";
        break;
    case DIAG_ERRCODE.SD_CARD_TSB_STATUS_FAILURE:
        info = "TSB Status Error";
        break;
    case DIAG_ERRCODE.SD_CARD_ZERO_MAXMINUTES_FAILURE:
        info = "TSB Zero MaxMinutes";
        break;
    case DIAG_ERRCODE.EMMC_PREEOL_STATE_FAILURE:
        info = "Pre EOL State Error";
        break;
    case DIAG_ERRCODE.DEFAULT_RESULT_VALUE:
    default:
        if(status < 0) {
            info = "Test Not Executed"
        } else {
            info = "";
        }
        break;
    }
    return info;
}

function setGroupResult(group, showPrevious) {
    var textResult = "PASSED";
    var warningInfo = "";

    group.result = results.passed;

    for(var e in group.elems) {
        elem = group.elems[e];
        if((elem.result === results.failed) || (elem.result === results.error)) {
            group.result = results.failed;
            textResult = "FAILED";
            warningInfo = getInfo(elem.name, elem.status, elem.data);
            break;
        }
        /* Only last element in warning message will be used */
        if(elem.result !== results.passed) {
            group.result = results.warning;
            textResult = "WARNING";
            warningInfo = getInfo(elem.name, elem.status, elem.data);
        }

    }

    setGroupUIResult(group);

    if(!showPrevious) {
        logTextResult = textResult;
        if(warningInfo != "") {
            logTextResult += "_" + warningInfo.replace(/ /g, "_");
        }
        ds.notify("LOG", { message: "Test result: " + group.name + ":" + logTextResult });
    }
}

function setGroupUIResult(group) {
    var warningInfo = "";

    for(var e in group.elems) {
        elem = group.elems[e];
        if ((resultsFilter.type === "filtered") || (resultsFilter.enable === 1 && resultsFilter.show === 1)) {
            if((elem.filterResult === results.failed) || (elem.filterResult === results.error) || (elem.filterResult !== results.passed)) {
                group.img.src = "resources/icon-fail-18.png";
            }
        }
        else {
            if((elem.result === results.failed) || (elem.result === results.error)) {
                group.img.src = "resources/icon-fail-18.png";
                warningInfo = getInfo(elem.name, elem.status, elem.data);
                if(warningInfo != "") {
                    group.scroller.set(warningInfo);
                    group.scroller.start();
                }
                break;
            }

            if(elem.result !== results.passed) {
                warningInfo = getInfo(elem.name, elem.status, elem.data);
                group.img.src = "resources/icon-warning-18.png";
                if(warningInfo != "") {
                    group.scroller.set(warningInfo);
                    group.scroller.start();
                }
            }
        }
    }
    group.img.style.visibility = "visible";
}

function telemetryLog() {
    var result_header = [];
    for (var t in telemetry_order) {
        result_header.push(telemetry_order[t][0]);
    }
    result_header.push("Result");
    result_header.toString();
    ds.notify("LOG", { message: "HwTestResultHeader: " + result_header });

    var result_store = [];
    for(var group in groupStatus) {
        var elems = groupStatus[group].elems;
        for(var e in elems) {
            var stat = elems[e].status;
            var result = elems[e].result;
            switch(result) {
                case results.error:
                    if (stat === DIAG_ERRCODE.FAILURE)
                        result_data = "F";
                    else
                        result_data = "F" + stat;
                    break;
                case results.passed:
                    result_data = "P";
                    break;
                case results.warning:
                    if (stat === DIAG_ERRCODE.DEFAULT_RESULT_VALUE)
                        result_data = "X";
                    else
                        result_data = "W" + stat;
                    break;
                default:
                    result_data = "X";
                    break;
            }
        }
        telemetry_order[group][1] = result_data;
    }

    for (var t in telemetry_order) {
        result_store.push(telemetry_order[t][1]);
    }
    var completeResult = "P";
    for(var g in groupStatus) {
        if(groupStatus[g].result === results.failed) {
            completeResult = "F";
            break;
        }
    }
    result_store.push(completeResult);
    result_store.toString();
    ds.notify("LOG", { message: "HwTestResult2: " + result_store });
    ds.notify("LOG", { telemetrymessage: "hwtest2_split " + result_store });

    result_store = [];
    telemetry_order = telemetry_swap;
}

function telemetryFilterLog() {
    var result_store = [];
    var completeResult = "P";

    for(var group in groupStatus) {
        var elems = groupStatus[group].elems;
        for(var e in elems) {
            var stat = elems[e].filterStatus;
            var filter_result = elems[e].filterResult;
            switch(filter_result) {
                case results.error:
                    result_data = "F";
                    completeResult = "F";
                    break;
                case results.passed:
                default:
                    result_data = "P";
                    break;
            }
        }
        telemetry_order[group][1] = result_data;
    }

    for (var t in telemetry_order) {
        result_store.push(telemetry_order[t][1]);
    }

    result_store.push(completeResult);
    result_store.toString();

    ds.notify("LOG", { message: "HwTestResultFilter: " + result_store });
    ds.notify("LOG", { telemetrymessage: "hwtestResultFilter_split " + result_store });

    result_store = [];
    telemetry_order = telemetry_swap;
}

function setElemResult(elem, status, data) {
    elem.status = status;
    elem.data = data;
    switch(status) {
    case DIAG_ERRCODE.FAILURE:
    case DIAG_ERRCODE.RF4CE_CHIP_DISCONNECTED:
    case DIAG_ERRCODE.BLUETOOTH_INTERFACE_FAILURE:
    case DIAG_ERRCODE.FILE_WRITE_OPERATION_FAILURE:
    case DIAG_ERRCODE.FILE_READ_OPERATION_FAILURE:
    case DIAG_ERRCODE.EMMC_TYPEA_MAX_LIFE_EXCEED_FAILURE:
    case DIAG_ERRCODE.EMMC_TYPEB_MAX_LIFE_EXCEED_FAILURE:
    case DIAG_ERRCODE.EMMC_TYPEA_ZERO_LIFETIME_FAILURE:
    case DIAG_ERRCODE.EMMC_TYPEB_ZERO_LIFETIME_FAILURE:
    case DIAG_ERRCODE.MCARD_AUTH_KEY_REQUEST_FAILURE:
    case DIAG_ERRCODE.MCARD_HOSTID_RETRIEVE_FAILURE:
    case DIAG_ERRCODE.MCARD_CERT_AVAILABILITY_FAILURE:
    case DIAG_ERRCODE.SD_CARD_TSB_STATUS_FAILURE:
    case DIAG_ERRCODE.SD_CARD_ZERO_MAXMINUTES_FAILURE:
    case DIAG_ERRCODE.EMMC_PREEOL_STATE_FAILURE:
        elem.result = results.error;
        break;
    case DIAG_ERRCODE.SUCCESS:
        elem.result = results.passed;
        break;
    case DIAG_ERRCODE.NOT_APPLICABLE:
    case DIAG_ERRCODE.CANCELLED:
    case DIAG_ERRCODE.CANCELLED_NOT_STANDBY:
    case DIAG_ERRCODE.INTERNAL_TEST_ERROR:
    case DIAG_ERRCODE.HDD_STATUS_MISSING:
    case DIAG_ERRCODE.HDMI_NO_DISPLAY:
    case DIAG_ERRCODE.HDMI_NO_HDCP:
    case DIAG_ERRCODE.MOCA_NO_CLIENTS:
    case DIAG_ERRCODE.MOCA_DISABLED:
    case DIAG_ERRCODE.SI_CACHE_MISSING:
    case DIAG_ERRCODE.TUNER_NO_LOCK:
    case DIAG_ERRCODE.TUNER_BUSY:
    case DIAG_ERRCODE.AV_NO_SIGNAL:
    case DIAG_ERRCODE.IR_NOT_DETECTED:
    case DIAG_ERRCODE.RF4CE_NO_RESPONSE:
    case DIAG_ERRCODE.CM_NO_SIGNAL:
    case DIAG_ERRCODE.AV_URL_NOT_REACHABLE:
    case DIAG_ERRCODE.NON_RF4CE_INPUT:
    case DIAG_ERRCODE.RF4CE_CTRLM_NO_RESPONSE:
    case DIAG_ERRCODE.HDD_MARGINAL_ATTRIBUTES_FOUND:
    case DIAG_ERRCODE.NO_GATEWAY_CONNECTION:
    case DIAG_ERRCODE.NO_COMCAST_WAN_CONNECTION:
    case DIAG_ERRCODE.NO_PUBLIC_WAN_CONNECTION:
    case DIAG_ERRCODE.NO_WAN_CONNECTION:
    case DIAG_ERRCODE.HDD_DEVICE_NODE_NOT_FOUND:
    case DIAG_ERRCODE.NO_ETH_GATEWAY_FOUND:
    case DIAG_ERRCODE.NO_MW_GATEWAY_FOUND:
    case DIAG_ERRCODE.NO_ETH_GATEWAY_CONNECTION:
    case DIAG_ERRCODE.NO_MW_GATEWAY_CONNECTION:
    case DIAG_ERRCODE.AV_DECODERS_NOT_ACTIVE:
    case DIAG_ERRCODE.DEFAULT_RESULT_VALUE:
        elem.result = results.warning;
        break;
/* currently mapped to DIAG_ERRCODE.FAILURE in wa_diag_errcodes.h
    case DIAG_ERRCODE.MCARD_CERT_INVALID:
        elem.result = results.error;
        break; */
    default:
        if(status < 0) {
            elem.result = results.warning;
        } else {
            elem.result = results.error;
        }
        break;
    }
}

function setElemFilterResult(elem, filterstatus)
{
    elem.filterStatus = filterstatus;

    switch(filterstatus) {
    case DIAG_ERRCODE.FAILURE:
        elem.filterResult = results.error;
        break;
    case DIAG_ERRCODE.SUCCESS:
    default:
        elem.filterResult = results.passed;
        break;
    }
}

function setPrevResult(elem, status, data) {
        setProgress(elem.id, 100);
        if (resultsFilter.type === "filtered")
            setElemFilterResult(elem, status);
        else
            setElemResult(elem, status, data);

        var group = groupByElemId(elem.id);
        setGroupResult(group, true);
}

function setResult(id, status, filterenable, filterstatus, showfilterResults, data) {
    //dbgWrite("setResult(id:"+id+",status:"+status+",data:"+data+",filterenable:"+filterenable+",filterstatus:"+filterstatus+",showfilterResults:"+showfilterResults+")");
    var elem = elemByElemId(id);
    if(elem === null) {
        return;
    }

    if(!cancelling) {
        setProgress(id, 100);

        if (filterenable === 1) { // Results Filter feature enabled
            resultsFilter.enable = 1;
            if (showfilterResults === 1)
                resultsFilter.show = 1;
            setElemFilterResult(elem, filterstatus);
        }
        setElemResult(elem, status, data);

        var group = groupByElemId(elem.id);
        setGroupResult(group, false);

        if(!runNextSet(id)) {
            if(checkAllFinished()) {
                setFinal(false);
                ds.setCallback(null);
            }
        }
    } else {
        setElemResult(elem, status, data);

        if(!checkNotFinished()) {
            ds.setCallback(null);
        }
    }
}

function timeStamp() {
    var d = new Date();

    return d.getUTCFullYear() + "-" +
           ("0" + (d.getUTCMonth() + 1)).slice(-2) + "-" +
           ("0" + d.getUTCDate()).slice(-2) + " " +
           ("0" + d.getUTCHours()).slice(-2) + ":" +
           ("0" + d.getUTCMinutes()).slice(-2) + ":" +
           ("0" + d.getUTCSeconds()).slice(-2);
}

function timeZone() {
    var d = new Date();
    var dt = d.toString();
    var zone = (dt.substring(dt.lastIndexOf("(") + 1, dt.lastIndexOf(")")));
    return zone;
}

function timeStampUI(d) {
    return d.getFullYear() + "-" +
           ("0" + (d.getMonth() + 1)).slice(-2) + "-" +
           ("0" + d.getDate()).slice(-2) + " " +
           ("0" + d.getHours()).slice(-2) + ":" +
           ("0" + d.getMinutes()).slice(-2) + ":" +
           ("0" + d.getSeconds()).slice(-2) ;
}

function setFinal(showPrevious) {
    var result = results.passed;
    var textResult = "PASSED";

    for(var g in groupStatus) {
        if(groupStatus[g].result === results.failed) {
            result = results.failed;
            textResult = "FAILED";
            break;
        }
        if(cancelling || (groupStatus[g].result != results.passed)) {
            result = results.warning;
        }
    }

    var resultUI = results.notrun;
    if ((resultsFilter.type === "filtered") || (resultsFilter.enable === 1 && resultsFilter.show === 1)) {
        document.getElementById('overallInstant').style.display = 'none';
        document.getElementById('overallFiltered').style.display = 'block';

        resultUI = results.passed;
        for(var g in groupStatus) {
            var elems = groupStatus[g].elems;
            for(var d in elems) {
                if ((elems[d].filterResult ===  results.error) || (elems[d].filterResult ===  results.failed)) {
                    resultUI = results.failed;
                    if(showPrevious)
                        textResult = "FAILED";
                }
            }
            if (resultUI === results.failed)
                break;
        }
    }
    else {
        document.getElementById('overallInstant').style.display = 'block';
        document.getElementById('overallFiltered').style.display = 'none';
    }

    if (resultUI !== results.notrun)
        result = resultUI;

    setInprogressTimer(0);
    setInactivityTimer(screen2Timeout);
    // Simpler than waiting for all log message procedures to complete...
    setTimeout(function() {

        if(showPrevious) {
            ds.notify("LOG", { message: "Previous results overall status:" + textResult});
        } else {
            var timestamp = timeStamp();
            var dt = new Date();
            var timestampUI = timeStampUI(dt);
            document.getElementById('timestamp').innerHTML = timestampUI + " " + timeZone();
            ds.notify("LOG", { rawmessage: "HWST_LOG |" + timestamp + " Test execution completed:" + textResult});

            ds.notify("LOG", { telemetrymessage: "SYST_INFO_HWTestOK" });
            if (textResult === "PASSED") {
                ds.notify("LOG", { telemetrymessage: "SYST_INFO_hwselftest_passed" });
            }

            ds.notify("TESTRUN", { state: "finish" } );
            telemetryLog();
            if (resultsFilter.enable == 1)
                telemetryFilterLog();
        }

        switch(result) {
        case results.passed:
            enableSuccessResultChoice();
            break;
        case results.warning:
            enableSuccessResultChoice();
            break;
        default:
            enableFailedResultChoice();
            break;
        }
    }, 500);
}

function cancellAll() {
    cancelling = true;
    for(g in groupStatus) {
        var elems = groupStatus[g].elems;
        var groupModified = false;
        for(var d in elems) {
            switch(elems[d].result) {
            case results.running:
                elems[d].result = results.cancelling;
                ds.cancell(elems[d].id);
            case results.notrun:
                elems[d].status = DIAG_ERRCODE.CANCELLED;
                elems[d].data = "Cancelled";
                setProgress(elems[d].id, 100);
                groupModified = true;
                break;
            }
        }
        if(groupModified) {
            setGroupResult(groupStatus[g], false);
        }
    }
    setFinal(false);
}


/*
function cancelItem(id) {
    var elem = elemByElemId(id);
    if(elem === null) {
        return;
    }

    if(elem.result !== results.running) {
        return;
    }

    ds.cancel(id);
}
*/

function setStarted(id) {
    var elem = elemByElemId(id);
    if(elem === null) {
        return;
    }
    elem.result = results.running;
}

function setProgress(id, progress) {
    var elem = elemByElemId(id);
    if(elem === null) {
        return;
    }

    if(progress < 0) {
        progress = 0;
    } else if(progress > 100) {
        progress = 100;
    }

    elem.progress = progress;
    var group = groupByElemId(id);
    group.progress = 0;
    for(var e in group.elems) {
        group.progress += group.elems[e].progress;
    }
    group.progress = ~~(group.progress / group.elems.length);
    group.bar.set(group.progress);
}

function dsCallbackRun(type, cookie, params, data, filterenable, filterparams, filtershowresults) {
    //dbgWrite("dsCallbackRun(type: " + type + ",cookie:" + cookie + ",params:" + params + ",filterenable:" + filterenable + ",filterparams:" + filterparams + ",filtershowresults:" + filtershowresults + ")");

    switch(type) {
    case Diagsys.cbType.log:
        break;
    case Diagsys.cbType.errLog:
        break;
    case Diagsys.cbType.progress:
        setProgress(cookie, params);
        break;
    case Diagsys.cbType.status:
        switch(params) {
        case Diagsys.cbStatus.issued:
            break;
        case Diagsys.cbStatus.started:
            setStarted(cookie);
            break;
        case Diagsys.cbStatus.error:
            setResult(cookie, -1);
            break;
        default:
            break;
        }
        break;
    case Diagsys.cbType.eod:
        if(isNaN(params)) {
            setResult(cookie, -1);
        }
        if((typeof data !== 'undefined') && (data !== null)) {
            setResult(cookie, params, filterenable, filterparams, filtershowresults, data);
        } else {
            setResult(cookie, params, filterenable, filterparams, filtershowresults);
        }
        break;
    default:
        break;
    }
}

function checkNotFinished() {
    for(g in groupStatus) {
        var elems = groupStatus[g].elems;
        for(var d in elems) {
            if(elems[d].result === results.cancelling) {
                return true;
            }
        }
    }
    return false;
}

function checkAllFinished() {
    for(g in groupStatus) {
        var elems = groupStatus[g].elems;
        for(var d in elems) {
            if(elems[d].progress !== 100)
                return false;
        }
    }
    return true;
}

function groupByElemId(id) {
    for(g in groupStatus) {
        var elems = groupStatus[g].elems;
        for(var d in elems) {
            if(elems[d].id === id)
                return groupStatus[g];
        }
    }
    return null;
}

function elemByElemId(id) {
    for(var g in groupStatus) {
        var elems = groupStatus[g].elems;
        for(d in elems) {
            if(elems[d].id === id)
                return elems[d];
        }
    }
    return null;
}

function elemByElemName(name) {
    for(var g in groupStatus) {
        var elems = groupStatus[g].elems;
        for(d in elems) {
            if(elems[d].resultsName === name)
                return elems[d];
        }
    }
    return null;
}

function runNextSet(lastId) {
    var status = false;

    if(typeof diagOrder[lastId] === 'undefined')
        return false;

    if(diagOrder[lastId].length == 0)
        return false;

    for(d in diagOrder[lastId]) {
        var elem = elemByElemId(diagOrder[lastId][d]);
        if(elem !== null) {
            status = true;
            //ToDo: consider setTimeout();
            ds.issue(elem.id, elem.name, elem.params);
        }
    }
    return status;
}

function enableInprogress() {
    ds.setCallback(null);
    buildOperationGroup(diagGroups);
    tableCreateGroupProgress();
    dbgWrite("enableInprogress");
    tableCreateGroupInfos();

    ds.setCallback(dsCallbackRun);

    document.getElementById('section_inprogress').style.display = 'block';
    if(!runNextSet(0)) {
        if(checkAllFinished()) {
            setFinal(false);
        }
    }
}

function disableInprogress() {
    document.getElementById('section_inprogress').style.display = 'none';
}

function enableShowprevious() {
    ds.setCallback(null);
    // In this case OperationGroup, Progress and Infos
    // are built in readPreviousResults() before we parse previous results
    dbgWrite("enableShowprevious");

    document.getElementById('section_inprogress').style.display = 'block';
    setFinal(true);
    for(g in groupStatus) {
        groupStatus[g].scroller.start();
    }
}

function uiExit() {

    if(typeof(ServiceManager) == "undefined") {
        dbgWrite("ServiceManager not loaded....");
        return;
    }

    if(ServiceManager.version) {
        ServiceManager.getServiceForJavaScript("System_10", function(service) { service.requestExitWebPage(); });
    } else {
        var systemService = ServiceManager.getServiceForJavaScript("systemService");
        if (systemService != null) {
            setTimeout( function() { systemService.requestExitWebPage(); }, 0);
        } else {
            dbgWrite("systemService == null");
        }
    }
}

function dsExit() {
    var json_store_string = JSON.stringify(json_store);
    ds.notify("LOG", { message: "HwSelfTest_SysInfo: " + json_store_string });

    ds.setCallback(null);
    cancelling = false;
    if(checkNotFinished())
    {
        agentKill();
        return;
    }
    uiExit();
}

function dsReRun() {
    disableResult();
    disableInprogress();

    ds.notify("LOG", { message: "Test execution restart"});

    ds.setCallback(null);
    cancelling = false;
    if(checkNotFinished())
    {
        agentKill();
        return;
    }
    dsRunPriv();
}

// To write logs into /opt/logs/hwselftest.log file
function logFileWrite(msg) {
    var str = msg.toString();
    var logMsg=(str);

    $.ajax({
        async: false,
        url: "cgi-bin/writelog.sh",
        timeout: 2000,
        cache: false,
        data: logMsg,
        dataType: "text",
        type: "POST"
    });
}

function logWrite(msg) {
    var timestamp = timeStamp();
    var logMsg=("HWST_LOG |" + timestamp + " [UI] " + msg);

    // equivalent of console.log("HWST_LOG |" + timestamp + " [UI] " + msg);
    // but logging under different systemd unit name
    $.ajax({
        async: false,
        url: "cgi-bin/log.sh",
        timeout: 2000,
        cache: false,
        data: logMsg,
        dataType: "text",
        type: "POST"
    });
}

function dbgWrite(msg) {
    var timestamp = timeStamp();
    var dbgMsg=("HWST_DBG |" + timestamp + " [UI] " + msg);

    // equivalent of console.log("HWST_DBG |" + timestamp + " [UI] " + msg);
    // but logging under different systemd unit name
    $.ajax({
        async: false,
        url: "cgi-bin/log.sh",
        timeout: 2000,
        cache: false,
        data: dbgMsg,
        dataType: "text",
        type: "POST"
    });
}

function agentKill() {
    $.ajax({
        async: true,
        url: "cgi-bin/agentkill.sh",
        timeout: 2000,
        cache: false
    });
}

/************ No choice buttons section *************/
function buildNochoiceSection() {

}

function keyhandlerNochoiceResult(keyCode) {
    if(keyCode === KEY_OK) {
        /*document.getElementById("btnPassB1").disabled = false;
        document.getElementById("btnPassB1").click();
        document.getElementById("btnPassB1").disabled = true;*/
        if(typeof document.getElementById("btnNochoiceB1").onclick == 'function') {
            document.getElementById("btnNochoiceB1").onclick();
        }
    }
}

/************ Choice buttons section *************/
var sectionChoiceInitialized = false
var buttonMarkers = {};
var buttonGrids = {};

function buildChoiceSection() {
    if(sectionChoiceInitialized) {
        return;
    }
    doBuildChoiceSection("result_choice_b1","btnChoiceB1","result_choice_b2","btnChoiceB2","result_choice_b3","btnChoiceB3",buttonMarkers,buttonGrids);
    sectionChoiceInitialized = true;
}

function keyhandlerChoiceResult(keyCode) {
    switch(keyCode) {
    case KEY_OK:
        //buttonGrids.resultFailed.click();
        var btn = buttonGrids.resultFailed.content();
        if(btn !== null) {
            /*btn.disabled = false;
            btn.click();
            btn.disabled = true;*/
            if(typeof btn.onclick == 'function') {
                btn.onclick();
            }
        }
        break;
    case KEY_LEFT:
        setInactivityTimer(screen2Timeout);
        buttonGrids.resultFailed.move('left');
        break;
    case KEY_RIGHT:
        setInactivityTimer(screen2Timeout);
        buttonGrids.resultFailed.move('right');
        break;
    default:
        break;
    }
    keyCode=8;
}

/************ Success result (no choice) page *************/
function enableSuccessResultNochoice() {
    document.getElementById('section_summary').style.display = 'block';
    document.getElementById('imgSummaryFail').style.display = 'none';
    document.getElementById('table_summary').classList.remove('overallresult-fail');
    document.getElementById('imgSummaryPass').style.display = 'block';
    document.getElementById('table_summary').classList.add('overallresult-pass');
    buildNochoiceSection();
    document.getElementById('section_nochoice').style.display = 'block';
    window.top.processKey = keyhandlerNochoiceResult;
}

/************ Success result (with choice) page *************/
function enableSuccessResultChoice() {
    document.getElementById('section_summary').style.display = 'block';
    document.getElementById('imgSummaryFail').style.display = 'none';
    document.getElementById('table_summary').classList.remove('overallresult-fail');
    document.getElementById('imgSummaryPass').style.display = 'block';
    document.getElementById('table_summary').classList.add('overallresult-pass');
    buildChoiceSection();
    buttonGrids.resultFailed.move(1);
    document.getElementById('section_choice').style.display = 'block';
    window.top.processKey = keyhandlerChoiceResult;
}

/************ Failed result (no choice) page *************/
function enableFailedResultNochoice() {
    document.getElementById('section_summary').style.display = 'block';
    document.getElementById('imgSummaryPass').style.display = 'none';
    document.getElementById('table_summary').classList.remove('overallresult-pass');
    document.getElementById('imgSummaryFail').style.display = 'block';
    document.getElementById('table_summary').classList.add('overallresult-fail');
    buildNochoiceSection();
    document.getElementById('section_choice').style.display = 'block';
    window.top.processKey = keyhandlerNochoiceResult;
}

/************ Failed result (with choice) page *************/
function enableFailedResultChoice() {
    document.getElementById('section_summary').style.display = 'block';
    document.getElementById('imgSummaryPass').style.display = 'none';
    document.getElementById('table_summary').classList.remove('overallresult-pass');
    document.getElementById('imgSummaryFail').style.display = 'block';
    document.getElementById('table_summary').classList.add('overallresult-fail');
    buildChoiceSection();
    buttonGrids.resultFailed.move(1);
    document.getElementById('section_choice').style.display = 'block';
    window.top.processKey = keyhandlerChoiceResult;
}


function disableResult() {
    window.top.processKey = window.top.processKeyDummy;
    disableSummary();
}

function disableSummary() {
    document.getElementById('section_summary').style.display = 'none';
    document.getElementById('section_choice').style.display = 'none';
    document.getElementById('section_nochoice').style.display = 'none';
}

function onAnimationTick() {
    /* This is dummy function to prevent log flooding as it is apparently is called repeatedly by the reciever */
}
