#!/bin/bash
##########################################################################
# If not stated otherwise in this file or this component's Licenses.txt
# file the following copyright and licenses apply:
#
# Copyright 2019 RDK Management
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
# Trigger hwselftest test execution

. /usr/bin/hwst_log.sh
source /lib/rdk/t2Shared_api.sh

/usr/bin/hwst_check_free_dram.sh $1
R1=$?

/usr/bin/hwst_check_free_cpu.sh $2
R2=$?

STAT=`/bin/systemctl show -p ActiveState hwselftest | sed 's/ActiveState=//g' 2>&1`
if [ "$STAT" = "active"  ] || [ "$STAT" = "activating" ]; then
    R3=1;
else
    R3=0;
fi

if [ $R1 -eq 0 -a $R2 -eq 0 -a $R3 -eq 0 ]; then
    n_log "[PTR] Test execution triggered."
    /usr/bin/hwselftestcli execute
else
    if [ $R3 -eq 1 ]; then
        n_log "[PTR] Multiple Connections Not Allowed."
        telemetryData="N,N,N,N,N,N,N,N,N,N,N,N,N,N,F-250" # WA_DIAG_ERRCODE_MULTIPLE_CONNECTIONS_NOT_ALLOWED
    elif [ $R1 -eq 1 ]; then
        telemetryData="N,N,N,N,N,N,N,N,N,N,N,N,N,N,F-254" # WA_DIAG_ERRCODE_DRAM_THRESHOLD_EXCEEDED
    elif [ $R2 -eq 1 ]; then
        telemetryData="N,N,N,N,N,N,N,N,N,N,N,N,N,N,F-255" # WA_DIAG_ERRCODE_CPU_THRESHOLD_EXCEEDED
    fi
    n_log "HwTestResult2: $telemetryData"
    t2ValNotify "hwtest2_split" "$telemetryData"

    n_log "[PTR] Test execution skipped, no resources."
fi
