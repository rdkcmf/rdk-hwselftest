#!/bin/bash
#
# If not stated otherwise in this file or this component's license file the
# following copyright and licenses apply:
#
# Copyright 2017 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

FILE=/tmp/.hwselftest_dfltroute

# Input arguments Format: family interface destinationip gatewayip preferred_src metric add/delete
cmd=$7
gtwip=$4

if [ "$cmd" = "add" ]; then
    if ! grep -q "$gtwip" $FILE; then
        echo "$gtwip" >> $FILE
    fi
elif [ "$cmd" = "delete" ]; then
    sed -i "/$gtwip/d" $FILE
fi
