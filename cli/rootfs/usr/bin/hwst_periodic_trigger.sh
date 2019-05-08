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

/usr/bin/hwst_check_free_dram.sh $1
R1=$?

/usr/bin/hwst_check_free_cpu.sh $2
R2=$?

if [ $R1 -eq 0 -a $R2 -eq 0 ]; then
    n_log "[PTR] Test execution triggered."
    /usr/bin/hwselftestcli execute
else
    n_log "[PTR] Test execution skipped, no resources."
fi
