#!/bin/sh
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
        tr181Set -s -t s -v $periodicRunFrequency Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.hwHealthTest.PeriodicRunFrequency
        tr181Set -s -t s -v $cpuThreshold Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.hwHealthTest.cpuThreshold
        tr181Set -s -t s -v $dramThreshold Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.hwHealthTest.dramThreshold
        tr181Set -s -t s -v 1  Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.hwHealthTest.EnablePeriodicRun
    else
        echo "HW self Tests periodic run is disabled"
    fi
else
    echo "HW Self Tests are disabled"
fi
