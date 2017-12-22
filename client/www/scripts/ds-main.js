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
var CLIENT_VERSION = "000c";
var client_name = "client ver. " + CLIENT_VERSION;
var screen1SysinfoTimeout = 3; //seconds, 0: disabled
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

/*
    GroupName1: {
    DiagName1: [ParamsSet1], [ParamsSet2], ...],
    DiagName2: [ParamsSet1], [ParamsSet2], ...]
    }
 */
var diagGroupsAll = {
    'Hard Drive': {'hdd_status': []},
    'Flash Memory': {'flash_status': []},
    'Dynamic RAM': {'dram_status': []},
    'HDMI Output': {'hdmiout_status': []},
    'Cable Card': {'mcard_status': []},
    'IR Remote Interface': {'ir_status': []},
    'RF Remote Interface': {'rf4ce_status': []},
    'MoCA': {'moca_status': []},
    'Audio/Video Decoder': {'avdecoder_qam_status': []},
    'Tuner': {'tuner_status': []},
    'Cable Modem': {'modem_status': []}
};

/* to be adjusted when reading capabilities */
var diagGroups = diagGroupsAll;

/* These numbers match the id-s assigned by buildOperationGroup().
   So first diag name present in diagGroups gets id=1 and so on.
 */
var diagOrder = {
    0: [1, 2, 3, 4, 5, 6, 7, 8, 9, 11],
    9: [10] //first av then tuners
};

var ds = null;

//window.onload = function() {
function dsOnload() {
    document.getElementById('version').innerHTML = "Ver. C" + CLIENT_VERSION;
    if(ds === null) {
        window.top.processKey = window.top.processKeyDummy;
        enableInit();
        ds = new Diagsys(wsEvent, dsCallbackInit);
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
    setCapabiliteisTimer(0);
    console.log("capabilitesMissing: timeout occured before the results arrived");
}

function prevResultsMissing() {
    ds.setCallback(null);
    setPrevResultsCheckTimer(0);
    setPrevResultsAvailable(false);
    console.log("prevResultsMissing: timeout occured before the results arrived");
    setInactivityTimer(screen1Timeout);
    enableStartbuttons();
}

function prevResultsPresent() {
    setPrevResultsAvailable(true);
}

function findIndexByDiagName(name,data) {
    if(typeof data !== 'object') {
        console.log("findIndexByDiagName: data is not an object!");
        return -1;
    }

    var index = -1;

    if((index = Object.keys(data).indexOf(name)) < 0) {
        console.log("findIndexByDiagName: index of" + name + " not found!");
        return -1;
    } else {
        //console.log("findIndexByDiagName: index of" + name + " is: " + index);
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
                console.log("readCapabailities: " + d + " available: " + capsDiags[cd]);
                availDiags[d] = allDiags[d];
                found = true;
                break;
                }
            }
        }
    }

    // consider no diags found as error; then return all diags
    if (!found) {
        console.log("error on capabilities reading, returning all diags");
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
    //console.log("readPreviousResults: raw data: " + JSON.stringify(data));
    //console.log("readPreviousResults: keys and values: " + "(" + Object.keys(data).length + " found" + ")");

    //console.log("readPrevResults: enter");
    if(typeof data !== 'object') {
        console.log("readPrevResults: exit, not an object data");
        setPrevResultsAvailable(false);
        return;
    }

    var index;
    var value;

    index = findIndexByDiagName("results_valid", data);
    if(index < 0) {
        //console.log("readPreviousResults: exit, results_valid index not found");
        setPrevResultsAvailable(false);
        return;
    }
    //console.log("readPreviousResults: results_valid index: " + index);

    value = Object.values(data)[index];
    if(value !== 0) {
        //console.log("readPreviousResults: exit, results_valid " + value + " (no previous results)");
        setPrevResultsAvailable(false);
        return;
    }
    //console.log("readPreviousResults: results_valid: " + value);

    index = findIndexByDiagName("start_time",data);
    if(index < 0) {
        //console.log("readPreviousResults: exit, start_time index not found");
        setPrevResultsAvailable(false);
        return;
    }
    var startTimeVal = Object.values(data)[index];
    //console.log("readPreviousResults: start_time index = " + index + ", value = " + startTimeVal);

    index = findIndexByDiagName("end_time",data);
    if(index < 0) {
        //console.log("readPreviousResults: end_time index not found");
        setPrevResultsAvailable(false);
        return;
    }
    var endTimeVal = Object.values(data)[index];
    document.getElementById('timestamp').innerHTML = endTimeVal + " UTC";
    //console.log("readPreviousResults: end_time index = " + index + ", value = " + endTimeVal);

    buildOperationGroup(diagGroups);
    tableCreateGroupProgress();
    tableCreateGroupInfos();

    for (var e in data.results) {
        elem = elemByElemName(e);
        if(elem === null) {
            console.log("readPrevResults: unknown diag: " + e);
            continue;
        }
        setPrevResult(elem, data.results[e].result);
    }
    if(!checkAllFinished()) {
        //console.log("readPrevResults: missing diags.");
        setPrevResultsAvailable(false);
        return;
    }

    setPrevResultsAvailable(true);

    //console.log("readPrevResults: exit");
}

/************ Init page *************/

function wsEvent(type, evt) {
    //console.log("wsEvent(" + type + "," + evt + ")");

    switch(type) {
    case Diagsys.evType.message:
        //do not handle message here
        break;
    case Diagsys.evType.open:
        setSysinfoTimer(screen1SysinfoTimeout);
        ds.issue(1, "sysinfo_info");
        break;
    case Diagsys.evType.close:
        agentMissing();
        break;
    }
}

function dsCallbackInit(type, cookie, params) {
    //console.log("dsCallbackInit(" + type + "," + cookie + "," + params + ")");

    if(type === Diagsys.cbType.log) {
        tableCreateInit(params);
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
    //console.log("dsCallbackCapabilities(" + type + "," + cookie + "," + params + ")");

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
    //console.log("dsCallbackPrevResults(" + type + "," + cookie + "," + params + ")");

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
    var store = document.getElementById('devinfo');
    store.style.textAlign = 'center';
    store.innerHTML="WARNING:<br>No connection to agent, will exit in 3 seconds. Please retry after few minutes";
    logWrite("No connection to agent, will exit in 3 seconds.");
    setTimeout(uiExit, 3000);
}

function tableCreateInit(data) {
    if(typeof data !== 'object') {
        return;
    }
    var store = document.getElementById('devinfo');
    var tbl = document.createElement('table');
    tbl.id = 'devinfo_table';
    //tbl.style.width = '60%';
    tbl.className = 'frames';
    var tbdy = document.createElement('tbody');

    for (var i = 0; i < Object.keys(data).length; i++) {
        switch(Object.keys(data)[i]) {
        case "Vendor":
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
        case "AgentVersion":
            document.getElementById('version').innerHTML = "Ver. C" + CLIENT_VERSION + "A" + Object.values(data)[i];
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

/************ Startbutton page *************/

function dsRunPriv() {
    cancelled = false;
    setInactivityTimer(0);
    ds.notify("TESTRUN", { state: "start", client: client_name } );
    setInprogressTimer(screen2InprogressTimeout);

    disableStartbuttons();
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
    enableShowprevious();
}

function dsShow() {
    ds.notify("LOG", { message: "Show previous results, client ver. " + CLIENT_VERSION});
    dsShowPriv();
}

/************ startbuttons run or exit section *************/
var startSectionChoiceInitialized = false;
var startButtonMarkers = {};
var startButtonGrids = {};

function doBuildChoiceSection(b1,b1input,b2,b2input,markers,grids) {

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

    grids.resultFailed = new ButtonGrid(2, markers.resultFailed);
}

function startBuildChoiceSection() {

    if(startSectionChoiceInitialized) {
        return;
    }

    if(previousResultsAvailable) {
        doBuildChoiceSection("start_choice_soe_b1","soeBtnShow","start_choice_soe_b2","soeBtnExit",startButtonMarkers,startButtonGrids);
    } else {
        doBuildChoiceSection("start_choice_roe_b1","roeBtnRun","start_choice_roe_b2","roeBtnExit",startButtonMarkers,startButtonGrids);
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

        case KEY_LEFT:
	    setInactivityTimer(screen1Timeout);
	    startButtonGrids.resultFailed.move('left');
	    break;

	case KEY_RIGHT:
	    setInactivityTimer(screen1Timeout);
	    startButtonGrids.resultFailed.move('right');
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
            console.log("readPreviousResults: groupEntries empty");
            groupStatus[g].progress = 100;
            groupStatus[g].result = results.warning;
            continue;
        }
        for(var d in groupEntries) {
            var diagName = groupEntries[d];
            groupStatus[g].elems[d] = {};
            groupStatus[g].elems[d].name = diagName;
            groupStatus[g].elems[d].progress = 0;
            groupStatus[g].elems[d].log = [];
            groupStatus[g].elems[d].result = results.notrun;
            groupStatus[g].elems[d].status = -1;
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
    console.log("table create");
}

function getInfo(elemName, status, data) {
    var info = "";
    switch(status) {
    case DIAG_ERRCODE.FAILURE:
    case DIAG_ERRCODE.SUCCESS:
        info = "";
        break;
    case DIAG_ERRCODE.NOT_APPLICABLE:
        info = "Error. Not Applicable";
        break;
    case DIAG_ERRCODE.HDD_STATUS_MISSING:
        info = "Health Status Unavailable";
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
    case DIAG_ERRCODE.AV_NO_SIGNAL:
        info = "No stream data. Check cable and verify STB is provisioned correctly";
        break;
    case DIAG_ERRCODE.IR_NOT_DETECTED:
        info = "IR Not Detected";
        break;
/* currently mapped to DIAG_ERRCODE.FAILURE in wa_diag_errcodes.h
    case DIAG_ERRCODE.MCARD_CERT_INVALID:
        info = "";
        break; */
    case DIAG_ERRCODE.CM_NO_SIGNAL:
        info = "Lock Failed - Check Cable";
        break;
    case DIAG_ERRCODE.INTERNAL_TEST_ERROR:
    case DIAG_ERRCODE.CANCELLED:
    default:
        if(status < 0) {
            info = "Error. Please Rerun Test"
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
            group.img.src = "resources/icon-fail-18.png";
            warningInfo = getInfo(elem.name, elem.status, elem.data);
            if(warningInfo != "") {
                group.scroller.set(warningInfo);
                group.scroller.start();
            }
            break;
        }
        /* Only last element in warning message will be used */
        if(elem.result !== results.passed) {
            group.result = results.warning;
            textResult = "WARNING";
            group.img.src = "resources/icon-warning-18.png";
            warningInfo = getInfo(elem.name, elem.status, elem.data);
            if(warningInfo != "") {
                group.scroller.set(warningInfo);
                group.scroller.start();
            }
        }
    }

    group.img.style.visibility = "visible";

    if(!showPrevious) {
        logTextResult = textResult;
        if(warningInfo != "") {
            logTextResult += "_" + warningInfo.replace(/ /g, "_");
        }
        ds.notify("LOG", { message: "Test result: " + group.name + ":" + logTextResult });
    }
}

function setElemResult(elem, status, data) {
    elem.status = status;
    elem.data = data;
    switch(status) {
    case DIAG_ERRCODE.FAILURE:
        elem.result = results.error;
        break;
    case DIAG_ERRCODE.SUCCESS:
        elem.result = results.passed;
        break;
    case DIAG_ERRCODE.NOT_APPLICABLE:
    case DIAG_ERRCODE.CANCELLED:
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
    case DIAG_ERRCODE.CM_NO_SIGNAL:
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

function setPrevResult(elem, status, data) {
        setProgress(elem.id, 100);
        setElemResult(elem, status, data);

        var group = groupByElemId(elem.id);
        setGroupResult(group, true);
}

function setResult(id, status, data) {
    //console.log("setResult("+id+","+status+","+data+")");
    var elem = elemByElemId(id);
    if(elem === null) {
        return;
    }

    if(!cancelling) {
        setProgress(id, 100);
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

    setInprogressTimer(0);
    setInactivityTimer(screen2Timeout);
    // Simpler than waiting for all log message procedures to complete...
    setTimeout(function() {

        if(showPrevious) {
            ds.notify("LOG", { message: "Previous results overall status:" + textResult});
        } else {
            var timestamp = timeStamp();
            document.getElementById('timestamp').innerHTML = timestamp + " UTC";
            ds.notify("LOG", { rawmessage: timestamp + " Test execution completed:" + textResult});
            ds.notify("TESTRUN", { state: "finish" } );
        }

        switch(result) {
        case results.passed:
            if(showPrevious) {
                enableSuccessResultChoice();
            } else {
                enableSuccessResultNochoice();
            }
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

function dsCallbackRun(type, cookie, params, data) {
    //console.log("dsCallbackRun(" + type + "," + cookie + "," + params + ")");

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
            setResult(cookie, params, data);
        } else {
            setResult(cookie, params);
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
            if(elems[d].name === name)
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
    console.log("enableInprogress");
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
    console.log("enableShowprevious");

    document.getElementById('section_inprogress').style.display = 'block';
    setFinal(true);
    for(g in groupStatus) {
        groupStatus[g].scroller.start();
    }
}

function uiExit() {
    if(typeof(ServiceManager) == "undefined") {
        console.log("ServiceManager not loaded....");
        return;
    }

    if(ServiceManager.version) {
        ServiceManager.getServiceForJavaScript("System_10", function(service) { service.requestExitWebPage(); });
    } else {
        var systemService = ServiceManager.getServiceForJavaScript("systemService");
        if (systemService != null) {
            setTimeout( function() { systemService.requestExitWebPage(); }, 0);
        } else {
            console.log("systemService == null");
        }
    }
}

function dsExit() {
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

function logWrite(msg) {
    $.ajax({
        async: false,
        url: "cgi-bin/log.sh",
        cache: false,
        data: msg,
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
    doBuildChoiceSection("result_choice_b1","btnChoiceB1","result_choice_b2","btnChoiceB2",buttonMarkers,buttonGrids);
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
    document.getElementById('section_summary').style.display = 'none';
    document.getElementById('section_choice').style.display = 'none';
    document.getElementById('section_nochoice').style.display = 'none';
}
