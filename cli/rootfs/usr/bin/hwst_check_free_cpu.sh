#!/bin/bash
# Reading cumulative cpu usage from 'cpu ' row in /proc/stat
# Divide by number of cores to get avg per cpu load (normalize to 100%)
#
CPU_POWER_CONSUMED=0
CPU_POWER_CONSUMED_MAX=75

if [ "$#" -eq 1 ]; then
    CPU_POWER_CONSUMED_MAX=$1
    #echo "Maximum power consumption allowed: $CPU_POWER_CONSUMED_MAX %"
fi

STAT=`grep 'cpu ' /proc/stat`
# echo "/proc/stat: [$STAT]"

CORES=`cat /proc/stat|grep 'cpu'|grep -v 'cpu '|wc -l`
# echo "Number of cores: $CORES"

CPU_POWER_CONSUMED=`echo $STAT|awk '{cpu=($2+$4)*100/($2+$4+$5)} END {print cpu}'`
CPU_POWER_CONSUMED_INT=`echo $CPU_POWER_CONSUMED | awk '{rfcpu=sprintf("%.0f", $1)} END {print rfcpu}'`

# echo "CPU load: $CPU_POWER_CONSUMED_INT"
echo "`/bin/date -u "+%Y-%m-%d %H:%M:%S"` [PTR] CPU load: $CPU_POWER_CONSUMED_INT % (max allowed: $CPU_POWER_CONSUMED_MAX %)." >> /opt/logs/hwselftest.log

RESULT=$(( $CPU_POWER_CONSUMED_INT > $CPU_POWER_CONSUMED_MAX ))
if [ "$RESULT" -eq 1 ]; then
    echo "`/bin/date -u "+%Y-%m-%d %H:%M:%S"` [PTR] Maximum CPU load exceeded." >> /opt/logs/hwselftest.log
fi

exit $RESULT
