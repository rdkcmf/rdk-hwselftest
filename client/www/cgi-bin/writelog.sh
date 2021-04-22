##########################################################################
# If not stated otherwise in this file or this component's Licenses.txt
# file the following copyright and licenses apply:
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
##########################################################################
#

source /lib/rdk/t2Shared_api.sh

STR=`cat <&0`
date=`date +"%F %T"`
file="/opt/logs/hwselftest.log"

if [ "$STR" = "-250" ]; then
    STAT=`/bin/systemctl show -p ActiveState hwselftest | sed 's/ActiveState=//g' 2>&1`
    if [ "$STAT" = "active"  ] || [ "$STAT" = "activating" ]; then
        MSG="Multiple Connections Not Allowed."
        TELEMETRY="N,N,N,N,N,N,N,N,N,N,N,N,N,N,F-250" # WA_DIAG_ERRCODE_MULTIPLE_CONNECTIONS_NOT_ALLOWED
    fi
elif [ "$STR" = "-251" ]; then
    MSG="Websocket Connection Failure."
    TELEMETRY="N,N,N,N,N,N,N,N,N,N,N,N,N,N,F-251" # WA_DIAG_ERRCODE_WEBSOCKET_CONNECTION_FAILURE
fi

if [ ! -z "$MSG" ]; then
    echo "$date [UI] $MSG" >> $file
    echo "$date HwTestResult2: $TELEMETRY" >> $file
    t2ValNotify "hwtest2_split" "$TELEMETRY"
fi

echo "Content-Type: text/html"
echo ""
