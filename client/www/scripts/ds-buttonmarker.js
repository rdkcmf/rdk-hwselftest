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

function ButtonMarker(cssName)
{
    var that = this;
    var infocus = false;
    var css = cssName;

    function setCss(on)
    {
        infocus = on;
        that.table.className = css + " " + css + (on ? "-infocus" : "-nofocus") ;
    }

    this.focus = function(on){setCss(on);};

    this.table = document.createElement('table');
    this.focus(false);

    var tbody = document.createElement('tbody');
    var tr = document.createElement('tr');
    this.td = document.createElement('td');

    tr.appendChild(this.td);
    tbody.appendChild(tr);
    this.table.appendChild(tbody);
} 
