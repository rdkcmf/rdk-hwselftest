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
#
# Create memory and cpu control groups for hwselftest
# to limit DRAM and CPU usage during test execution.
#
HWST_MEM_RLIMIT_MBYTES=20
HWST_CPU_RLIMIT_PERCENT=5

HWST_MEM_CG="/sys/fs/cgroup/memory/hwst_mem"
HWST_CPU_CG="/sys/fs/cgroup/cpu/hwst_cpu"

. /usr/bin/hwst_log.sh
source /lib/rdk/t2Shared_api.sh

# quit - print error message and quit from running agent
# args:
#   $1: quit reason
#
function quit() {
    if [ $1 -eq 7 ]; then
        telemetryData="N,N,N,N,N,N,N,N,N,N,N,N,N,N,F-252" # WA_DIAG_ERRCODE_AGENT_INIT_FAILURE
    else
        telemetryData="N,N,N,N,N,N,N,N,N,N,N,N,N,N,F-253" # WA_DIAG_ERRCODE_RESOURCE_OPERATION_FAILURE
    fi
    n_log "HwTestResult2: $telemetryData"
    t2ValNotify "hwtest2_split" "$telemetryData"

    n_log "Runtime limits not applied, agent not started. ($1)"
    exit $1
}

# create_cg - create control group by absolute path
# args:
#   $1: cgroup absolute path
#
function create_cg() {

    if [ ! -d $1 ]; then
        # d_log "create_cg: group: $1 not found"
        mkdir $1

        if [ "$?" -eq 0 ]; then
            d_log "create_cg: group: $1 created"
        else
            n_log "create_cg: could not create $1 group, exiting"
            quit 1
        fi
    else
        d_log "create_cg: group: $1 already exist"
    fi
}

# remove_cg - remove control group by absolute path
# args:
#    $1: cgroup absolute path
#
function remove_cg() {

    if [ -d $1 ]; then
        # d_log "remove_cg: group: $1 found"

        rmdir $1

        if [ ! -d $1 ]; then
            d_log "remove_cg: group: $1 removed"
        else
            n_log "remove_cg: could not remove group, exiting"
            quit 2
        fi
    else
        d_log "remove_cg: group: $1 not found"
    fi
}

# set_dram_limit - set runtime resident DRAM limit in MBytes
# args:
#    $1: cgroup absolute path
#    $2: DRAM limit in MBytes
#
function set_dram_limit() {

    if [ ! -d $1 ]; then
        n_log "set_dram_limit: missing group $1, exiting hwselftest"
        quit 3
    else
        if [ ! -f $1/memory.limit_in_bytes ]; then
            n_log "set_dram_limit: missing file $1/memory.limit_in_bytes, exiting hwselftest"
            quit 3
        fi
        OLD_LIMIT_B=`cat $1/memory.limit_in_bytes`
        OLD_LIMIT_MB=$(( $OLD_LIMIT_B / 1024 / 1024 ))
        # d_log "set_dram_limit: group: $1: old limit: $OLD_LIMIT_MB MBytes"
        echo $(( $2 * 1024 * 1024 )) > $1/memory.limit_in_bytes
        NEW_LIMIT_B=`cat $1/memory.limit_in_bytes`
        NEW_LIMIT_MB=$(( $NEW_LIMIT_B / 1024 / 1024 ))
        d_log "set_dram_limit: group: $1: new limit: $NEW_LIMIT_MB MBytes"
    fi
}

# set_cpu_limit - set runtime CPU occupation limit in %
# args:
#    $1: cgroup absolute path
#    $2: CPU limit in %
#
function set_cpu_limit() {

    if [ ! -d $1 ]; then
        n_log "set_cpu_limit: missing group $1, exiting hwselftest"
        quit 4
    else
        if [ ! -f $1/cpu.shares ]; then
            n_log "set_cpu_limit: missing file $1/cpu.shares, exiting hwselftest"
            quit 3
        fi
        OLD_LIMIT_RAW=`cat $1/cpu.shares`
        OLD_LIMIT_PERCENT=$(( $OLD_LIMIT_RAW * 100 / 1024 ))
        # d_log "set_cpu_limit: group: $1: old limit: $OLD_LIMIT_PERCENT %"
        echo $(( ($2 * 1024 + 99) / 100 )) > $1/cpu.shares
        NEW_LIMIT_RAW=`cat $1/cpu.shares`
        NEW_LIMIT_PERCENT=$(( $NEW_LIMIT_RAW * 100 / 1024 ))
        d_log "set_cpu_limit: group: $1: new limit: $NEW_LIMIT_PERCENT %"
    fi
}

# cap_process_to_cg - include process in control group,
# args:
#    $1: pid of processs
#    $2: cgroup absolute path
function cap_process_to_cg() {

    # pid verified at the begining of script execution

    d_log "cap_process_to_cg: pid: $1, cgroup: $2"
    echo $1 > $2/tasks
}

# Execution starts here:
#
if [ "$#" -ne 1 ]; then
    n_log "Missing pid, exiting"
    quit 5
fi

if [ ! -d /proc/$1 ]; then
    n_log "No such process, exiting"
    quit 6
fi

remove_cg $HWST_MEM_CG
remove_cg $HWST_CPU_CG

create_cg $HWST_MEM_CG
create_cg $HWST_CPU_CG

set_dram_limit $HWST_MEM_CG $HWST_MEM_RLIMIT_MBYTES
set_cpu_limit $HWST_CPU_CG $HWST_CPU_RLIMIT_PERCENT

cap_process_to_cg $1 $HWST_MEM_CG
cap_process_to_cg $1 $HWST_CPU_CG

# copy latest results from persistent storage to dram
# where agent expects to find them, but only access
# persistent storage if box is in power ON mode
#
if [ ! -f /tmp/hwselftest.results ]; then
   pwr=`/QueryPowerState`
   d_log "hwst_agent_start: prev results not found, stb pwr: $pwr"
   if [ $pwr == "ON" ]; then
       if [ -f /opt/logs/hwselftest.results ]; then
           d_log "hwst_agent_start: prev results retrieved from /opt/logs"
           cp /opt/logs/hwselftest.results /tmp/
       fi
   fi
fi

[ -f /opt/debug.ini ] && ! grep BUILD_TYPE=prod /etc/device.properties > /dev/null && export DEBUGINIFILE=/opt/debug.ini

/usr/bin/hwselftest
exitCode=$?
if [ $exitCode -ne 0 ]; then
    n_log "Agent failed to start. ($exitCode)"
    quit 7
fi
