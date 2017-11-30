#!/bin/bash
# Create memory and cpu control groups for hwselftest
# to limit DRAM and CPU usage during test execution.
#
HWST_MEM_RLIMIT_MBYTES=20
HWST_CPU_RLIMIT_PERCENT=5

HWST_MEM_CG="/sys/fs/cgroup/memory/hwst_mem"
HWST_CPU_CG="/sys/fs/cgroup/cpu/hwst_cpu"

# create_cg - create control group by absolute path
# args:
#   $1: cgroup absolute path
#
function create_cg() {

    if [ ! -d $1 ]; then
        echo "create_cg: group: $1 not found"
        mkdir $1

        if [ "$?" -eq 0 ]; then
            echo "create_cg: group: $1 created"
        else
            echo "create_cg: could not create $1 group, exiting"
            exit 1
        fi
    else
        echo "create_cg: group: $1 already exist"
    fi

}

# remove_cg - remove control group by absolute path
# args:
#    $1: cgroup absolute path
#
function remove_cg() {

    if [ -d $1 ]; then
        # echo "remove_cg: group: $1 found"

        rmdir $1

        if [ ! -d $1 ]; then
            echo "remove_cg: group: $1 removed"
        else
            echo "remove_cg: could not remove group, exiting"
            exit 2
        fi
    else
        echo "remove_cg: group: $1 not found"
    fi
}

# set_dram_limit - set runtime resident DRAM limit in MBytes
# args:
#    $1: cgroup absolute path
#    $2: DRAM limit in MBytes
#
function set_dram_limit() {

    if [ ! -d $1 ]; then
        echo "set_dram_limit: missing group $1, exiting"
        exit 3
    else
        OLD_LIMIT_B=`cat $1/memory.limit_in_bytes`
        OLD_LIMIT_MB=$(( $OLD_LIMIT_B / 1024 / 1024 ))
        # echo "set_dram_limit: group: $1: old limit: $OLD_LIMIT_MB MBytes"
        echo $(( $2 * 1024 * 1024 )) > $1/memory.limit_in_bytes
        NEW_LIMIT_B=`cat $1/memory.limit_in_bytes`
        NEW_LIMIT_MB=$(( $NEW_LIMIT_B / 1024 / 1024 ))
        echo "set_dram_limit: group: $1: new limit: $NEW_LIMIT_MB MBytes"
    fi
}

# set_cpu_limit - set runtime CPU occupation limit in %
# args:
#    $1: cgroup absolute path
#    $2: CPU limit in %
#
function set_cpu_limit() {

    if [ ! -d $1 ]; then
        echo "set_cpu_limit: missing group $1, exiting"
        exit 4
    else
        OLD_LIMIT_RAW=`cat $1/cpu.shares`
        OLD_LIMIT_PERCENT=$(( $OLD_LIMIT_RAW * 100 / 1024 ))
        # echo "set_cpu_limit: group: $1: old limit: $OLD_LIMIT_PERCENT %"
        echo $(( ($2 * 1024 + 99) / 100 )) > $1/cpu.shares
        NEW_LIMIT_RAW=`cat $1/cpu.shares`
        NEW_LIMIT_PERCENT=$(( $NEW_LIMIT_RAW * 100 / 1024 ))
        echo "set_cpu_limit: group: $1: new limit: $NEW_LIMIT_PERCENT %"
    fi
}

# cap_process_to_cg - include process in control group,
# args:
#    $1: pid of processs
#    $2: cgroup absolute path
function cap_process_to_cg() {

    # pid verified at the begining of script execution

    echo "Process pid: $1, cgroup: $2"
    echo $1 > $2/tasks
}

# Execution starts here:
#
if [ "$#" -ne 1 ]; then
    echo "Missing pid, exiting"
    exit 5
fi

if [ ! -d /proc/$1 ]; then
    echo "No such process, exiting"
    exit 6
fi

remove_cg $HWST_MEM_CG
remove_cg $HWST_CPU_CG

create_cg $HWST_MEM_CG
create_cg $HWST_CPU_CG

set_dram_limit $HWST_MEM_CG $HWST_MEM_RLIMIT_MBYTES
set_cpu_limit $HWST_CPU_CG $HWST_CPU_RLIMIT_PERCENT

cap_process_to_cg $1 $HWST_MEM_CG
cap_process_to_cg $1 $HWST_CPU_CG

