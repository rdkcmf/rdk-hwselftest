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

function ButtonGrid(collumns, objs) {
    var that = this;
    this.collumns = collumns;
    this.objs = objs;
    this.pos = -1;
    this.move('next');
} 

ButtonGrid.prototype.move = function(where) {
    var prev = this.pos;
    var i;
    var tmp;
    var orig;

    if(where == 'prev') {
        for(i = this.pos-1; (i >= 0) && (typeof this.objs[i] !== 'object'); --i);
        if(i < 0) {
            for(i = this.pos; (i < this.objs.length) && (typeof this.objs[i] !== 'object'); ++i);
        }
        if(i < this.objs.length) {
            this.pos = i;
        }
    } else if(where == 'next') {
        for(i = this.pos+1; (i < this.objs.length) && (typeof this.objs[i] !== 'object'); ++i);
        if(i == this.objs.length) {
            for(i = this.pos; (i >=0) && (typeof this.objs[i] !== 'object'); --i);
        }
        if(i >= 0) {
            this.pos = i;
        }
    } else if(typeof where === 'number') {
        if(where < 0) {
            tmp = this.pos;
            this.pos = -1;
            if(this.move('next') == -1) {
                this.pos = tmp;
            }
        } else if (where >= this.objs.length) {
            tmp = this.pos;
            this.pos = this.objs.length;
            if(this.move('prev') == this.objs.length) {
                this.pos = tmp;
            }
        } else {
            tmp = this.pos;
            this.pos = where -1;
            if(this.move('next') == this.objs.length) {
                this.pos = tmp;
            }
        }
    } else if(where === 'up') {
        orig = this.pos;
        if(this.pos >= this.collumns) {
            this.pos -= this.collumns - 1;
        }
        else {
            this.pos = 1;
        }

        tmp = this.pos;
        if((this.move('prev') === tmp) || (~~(this.pos / this.collumns) !== ~~(orig / this.collumns) - 1)) {
            this.pos = orig;
        }
    } else if(where === 'down') {
        orig = this.pos;
        if(this.pos + this.collumns < this.objs.length) {
            this.pos += this.collumns - 1;
        } else {
            this.pos = this.objs.length - 2;
        }

        tmp = this.pos;
        if((this.move('next') === tmp) || (~~(this.pos / this.collumns) !== ~~(orig / this.collumns) + 1)) {
            this.pos = orig;
        }
    }
    else if(where === 'left') {
        orig = this.pos;
        this.move('prev');
        if(~~(this.pos / this.collumns) !== ~~(orig / this.collumns)) {
            this.pos = orig;
        }
    } else if(where === 'right') {
        orig = this.pos;
        this.move('next');
        if(~~(this.pos / this.collumns) !== ~~(orig / this.collumns)) {
            this.pos = orig;
        }
    }

    if(this.pos != -1) {
        if((prev != -1) && (typeof this.objs[prev].className !== 'ButtonMarker')) {
            this.objs[prev].focus(false);
        }
        if(typeof this.objs[this.pos].className !== 'ButtonMarker') {
            this.objs[this.pos].focus(true);
        }
    }

    return this.pos;
};

ButtonGrid.prototype.click = function() {
    if((this.pos !== -1) && (typeof this.objs[this.pos].className !== 'ButtonMarker')) {
        var content = this.objs[this.pos].td.childNodes;
        if((typeof content[0] !== 'undefined') && (typeof content[0].click === 'function')) {
            content[0].click();
        }
    }
};

ButtonGrid.prototype.content = function() {
    if((this.pos !== -1) && (typeof this.objs[this.pos].className !== 'ButtonMarker')) {
        var content = this.objs[this.pos].td.childNodes;
        if(typeof content[0] !== 'undefined') {
            return content[0];
        }
    }
    return null;
};
