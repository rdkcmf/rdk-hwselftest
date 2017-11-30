#!/bin/bash
# Old version of free
#   need compute available RAM as:
#   Total - used + buffer + cache
#
#   Example output of free -m:
#
#                total       used       free     shared    buffers     cached
#   Mem:          7983       2485       5498         23        567       1032
#   Swap:         8188          0       8188
#
#
# Newer version add "-/+ buffers/cache" row
#   we treat free value from this row as available memory count
#
#   Example output of free -m:
#
#                total       used       free     shared    buffers     cached
#   Mem:          7983       2487       5496         23        567       1032
#   -/+ buffers/cache:        887       7096
#   Swap:         8188          0       8188
#
#
# Latest version include direct estimation of available memory
#
#   Example output of free -m:
#
#                total        used        free      shared  buff/cache   available
#   Mem:          1504        1491          13           0         855      792
#   Swap:         2047           6        2041
#
# Latest is the most accurate method, if not available then fall back to others.
#
FREE_DRAM_MBYTES=0
FREE_DRAM_MBYTES_MIN=50

if [ "$#" -eq 1 ]; then
    FREE_DRAM_MBYTES_MIN=$1
    #echo "Required RAM: $FREE_DRAM_MBYTES_MIN MB"
fi

AVAILABLE=`free -m|grep available|wc -l`
PLUS_MINUS=`free -m|grep "+ buffer"|wc -l`

if [ x$AVAILABLE == x1 ]; then
    # echo "Found newest version of free, get 'available' memory"
    FREE_DRAM_MBYTES=`free -m|grep "Mem:"|tr -s " "|cut -d" " -f7`
elif [ x$PLUS_MINUS == x1 ]; then
    # echo "Found older version of free, using free count from '-/+ buffers/cache' row"
    FREE_DRAM_MBYTES=`free -m|grep "+ buffer"|tr -s " "|cut -d" " -f4`
else
    # echo "Found terribly old free version"
    FREE_DRAM_MBYTES=`free -m|grep "Mem:"|awk '{avail_mem=($2-$3+$6+$7) END {print avail_mem}'`
fi

# echo "Free DRAM: $FREE_DRAM_MBYTES MB"
echo "`/bin/date -u "+%Y-%m-%d %H:%M:%S"` [PTR] Free DRAM: $FREE_DRAM_MBYTES MB (min required: $FREE_DRAM_MBYTES_MIN MB)." >> /opt/logs/hwselftest.log

RESULT=$(( $FREE_DRAM_MBYTES < $FREE_DRAM_MBYTES_MIN ))
if [ "$RESULT" -eq 1 ]; then
    echo "`/bin/date -u "+%Y-%m-%d %H:%M:%S"` [PTR] Not enough DRAM." >> /opt/logs/hwselftest.log
fi

exit $RESULT