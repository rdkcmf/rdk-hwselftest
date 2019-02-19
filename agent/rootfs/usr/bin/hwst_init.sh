#!/bin/sh

enablePeriodicRun=`tr181Set Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.hwHealthTest.Enable 2>&1 > /dev/null`
if [ "$enablePeriodicRun" = "true" ]; then
    echo "HW Self Tests enabled"
    enablePeriodicRun=`tr181Set Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.hwHealthTest.EnablePeriodicRun 2>&1 > /dev/null`
    if [ "$enablePeriodicRun" = "true" ]; then
        echo "HW Self Tests periodic run enabled"
        periodicRunFrequency=`tr181Set Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.hwHealthTest.PeriodicRunFrequency 2>&1 > /dev/null`
        cpuThreshold=`tr181Set  Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.hwHealthTest.cpuThreshold 2>&1 > /dev/null`
        dramThreshold=`tr181Set Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.hwHealthTest.dramThreshold 2>&1 > /dev/null`
        echo "Enabling periodic run of HW self tests with $periodicRunFrequency seconds frequency with CPU Threshold = $cpuThreshold and DRAM threshold = $dramThreshold "
        tr181Set -s -t s -v 1  Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.hwHealthTest.Enable
        tr181Set -s -t s -v 1  Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.hwHealthTest.EnablePeriodicRun
        tr181Set -s -t s -v $periodicRunFrequency Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.hwHealthTest.PeriodicRunFrequency
        tr181Set -s -t s -v $cpuThreshold Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.hwHealthTest.cpuThreshold
        tr181Set -s -t s -v $dramThreshold Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.hwHealthTest.dramThreshold
    else
        echo "HW self Tests periodic run is disabled"
    fi
else
    echo "HW Self Tests are disabled"
fi
