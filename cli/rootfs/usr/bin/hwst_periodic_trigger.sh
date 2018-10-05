#!/bin/bash
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
