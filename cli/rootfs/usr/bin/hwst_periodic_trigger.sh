#!/bin/bash
# Trigger hwselftest test execution

/usr/bin/hwst_check_free_dram.sh $1
R1=$?

/usr/bin/hwst_check_free_cpu.sh $2
R2=$?

if [ $R1 -eq 0 -a $R2 -eq 0 ]; then 
    /usr/bin/hwselftestcli execute
else 
    echo "`/bin/date -u "+%Y-%m-%d %H:%M:%S"` [PTR] Test execution skipped, no resources." >> /opt/logs/hwselftest.log
fi
