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

function ScrollText(container) {
    var timer = null;
    time = 3000;

    function textWidth(text) {
        var tmp = '<span style="display:none">' + text + '</span>';
        $('body').append(tmp);
        var width = $('body').find('span:last').width();
        $('body').find('span:last').remove();
        return width;
    }

    function tail() {
        //console.log("tail: " + container[0].innerText);
        timer = setTimeout(function(){console.log("tailTimer: " + container[0].innerText); start(); }, time);
        //console.log("tail timer: " + timer);
    }

    function stop() {
        //console.log("stop");
        if(timer !== null) {
            clearTimeout(timer);
            container.stop();
            //console.log("stop timer: " + timer);
            timer = null;
        }
        container.css('text-indent', '0px');
    }

    function set(text) {
        //console.log("set:" + text);
        stop();
        container.css('text-indent', '0px');
        container[0].innerHTML = text;
    }

    function start() {
        var shift = container.width() == 0 ? 0 :textWidth(container[0].innerText) - container.width();
        //console.log("start: " + shift + " " + container[0].innerText);
        container.css('text-indent', '0px');
        if(shift > 0) {
            //console.log("timer: " + container[0].innerText);
            timer = setTimeout(function() {
                //console.log("scrolltimer: " + shift);
                container.animate({
                    'textIndent': -shift
                }, shift * 30, 'linear', tail);
            }, time);
            //console.log("start timer: " + timer);
        }
    }

    this.stop = function() { stop(); };
    this.set = function(text) { set(text); };
    this.start = function() { start(); };
}
