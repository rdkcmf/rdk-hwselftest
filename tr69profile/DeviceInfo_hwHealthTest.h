/*
 * If not stated otherwise in this file or this component's Licenses.txt file the
 * following copyright and licenses apply:
 *
 * Copyright 2017 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

#ifndef DEVICEINFO_HWHEALTHTEST_H_
#define DEVICEINFO_HWHEALTHTEST_H_

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/
#include "hostIf_tr69ReqHandler.h"


/*****************************************************************************
 * EXPORTED FUNCTIONS
 *****************************************************************************/

namespace hwselftest {

/**
 * @brief set_Device_DeviceInfo_xOpsDeviceMgmt_hwHealthTest_Enable
 *
 * This method is used to enable/disable the hardware health test functionality.
 * with following TR-069 definition:
 *   Parameter Name: Device.DeviceInfo.X_RDKCENTRAL-COM_xOpsDeviceMgmt.hwHealthTest.Enable
 *   Data type: boolean - Setting this will enable/disable health test functionality.
 *
 * @retval true if it is successful.
 * @retval false if operation fails.
 */
bool set_Device_DeviceInfo_xOpsDeviceMgmt_hwHealthTest_Enable(const char *log_module, HOSTIF_MsgData_t *stMsgData);

/**
 * @brief set_Device_DeviceInfo_xOpsDeviceMgmt_hwHealthTest_ExecuteTest
 *
 * This method is used to trigger hardware health test on the STB.
 * with following TR-069 definition:
 *   Parameter Name: Device.DeviceInfo.X_RDKCENTRAL-COM_xOpsDeviceMgmt.hwHealthTest.ExecuteTest
 *   Data type: integer - Setting this parameter will schedule a new test run
 *
 * @retval true if it is successful.
 * @retval false if operation fails.
 */
bool set_Device_DeviceInfo_xOpsDeviceMgmt_hwHealthTest_ExecuteTest(const char *log_module, HOSTIF_MsgData_t *stMsgData);

/**
 * @brief get_Device_DeviceInfo_xOpsDeviceMgmt_hwHealthTest_Results
 *
 * This method is used to retrieve the most recent hardware health test results.
 * with following TR-069 definition:
 *   Parameter Name: Device.DeviceInfo.X_RDKCENTRAL-COM_xOpsDeviceMgmt.hwHealthTest.Results
 *
 * @retval true if it is successful.
 * @retval false if operation fails.
 */
bool get_Device_DeviceInfo_xOpsDeviceMgmt_hwHealthTest_Results(const char *log_module, HOSTIF_MsgData_t *stMsgData);

/**
 * @brief set_Device_DeviceInfo_xOpsDeviceMgmt_hwHealthTest_SetTuneType
 *
 * This method is used to set the tune type to run tuner test for NGAN.
 * with following TR-069 definition:
 *   Parameter Name: Device.DeviceInfo.X_RDKCENTRAL-COM_xOpsDeviceMgmt.hwHealthTest.SetTuneType
 *
 * @retval true if it is successful.
 * @retval false if operation fails.
 */
bool set_Device_DeviceInfo_xOpsDeviceMgmt_hwHealthTest_SetTuneType(const char *log_module, HOSTIF_MsgData_t *stMsgData);

/**
 * @brief set_Device_DeviceInfo_xOpsDeviceMgmt_hwHealthTest_ExecuteTuneTest
 *
 * This method is used to execute the tune test for NGAN.
 * with following TR-069 definition:
 *   Parameter Name: Device.DeviceInfo.X_RDKCENTRAL-COM_xOpsDeviceMgmt.hwHealthTest.ExecuteTuneTest
 *
 * @retval true if it is successful.
 * @retval false if operation fails.
 */
bool set_Device_DeviceInfo_xOpsDeviceMgmt_hwHealthTest_ExecuteTuneTest(const char *log_module, HOSTIF_MsgData_t *stMsgData);

/**
 * @brief get_Device_DeviceInfo_xOpsDeviceMgmt_hwHealthTestTune_TuneResults
 *
 * This method is used to retrieve the most recent hardware health tune test results.
 * with following TR-069 definition:
 *   Parameter Name: Device.DeviceInfo.X_RDKCENTRAL-COM_xOpsDeviceMgmt.hwHealthTestTune.TuneResults
 *
 * @retval true if it is successful.
 * @retval false if operation fails.
 */
bool get_Device_DeviceInfo_xOpsDeviceMgmt_hwHealthTestTune_TuneResults(const char *log_module, HOSTIF_MsgData_t *stMsgData);

/**
 * @brief set_Device_DeviceInfo_xOpsDeviceMgmt_hwHealthTest_EnablePeriodicRun
 *
 * This method is used to enable/disable the hardware health test periodic run functionality.
 * with following TR-069 definition:
 *   Parameter Name: Device.DeviceInfo.X_RDKCENTRAL-COM_xOpsDeviceMgmt.hwHealthTest.EnablePeriodicRun
 *   Data type: boolean - Setting this will enable/disable health test periodic run functionality.
 *
 * @retval true if it is successful.
 * @retval false if operation fails.
 */
bool set_Device_DeviceInfo_xOpsDeviceMgmt_hwHealthTest_EnablePeriodicRun(const char *log_module, HOSTIF_MsgData_t *stMsgData);

/**
 * @brief set_Device_DeviceInfo_xOpsDeviceMgmt_hwHealthTest_PeriodicRunFrequency
 *
 * This method is used to set the hardware health test periodic run frequency.
 * with following TR-069 definition:
 *   Parameter Name: Device.DeviceInfo.X_RDKCENTRAL-COM_xOpsDeviceMgmt.hwHealthTest.PeriodicRunFrequency
 *   Data type: uInt - periodic run frequency value.
 *
 * @retval true if it is successful.
 * @retval false if operation fails.
 */
bool set_Device_DeviceInfo_xOpsDeviceMgmt_hwHealthTest_PeriodicRunFrequency(const char *log_module, HOSTIF_MsgData_t *stMsgData);

/**
 * @brief set_Device_DeviceInfo_xOpsDeviceMgmt_hwHealthTest_CpuThreshold
 *
 * This method is used to set the hardware health test CPU threshold.
 * with following TR-069 definition:
 *   Parameter Name: Device.DeviceInfo.X_RDKCENTRAL-COM_xOpsDeviceMgmt.hwHealthTest.cpuThreshold
 *   Data type: unsignedInt - CPU threshold value.
 *
 * @retval true if it is successful.
 * @retval false if operation fails.
 */
bool set_Device_DeviceInfo_xOpsDeviceMgmt_hwHealthTest_CpuThreshold(const char *log_module, HOSTIF_MsgData_t *stMsgData);

/**
 * @brief set_Device_DeviceInfo_xOpsDeviceMgmt_hwHealthTest_DramThreshold
 *
 * This method is used to set the hardware health test DRAM threshold.
 * with following TR-069 definition:
 *   Parameter Name: Device.DeviceInfo.X_RDKCENTRAL-COM_xOpsDeviceMgmt.hwHealthTest.dramThreshold
 *   Data type: unsignedInt - DRAM threshold value.
 *
 * @retval true if it is successful.
 * @retval false if operation fails.
 */
bool set_Device_DeviceInfo_xOpsDeviceMgmt_hwHealthTest_DramThreshold(const char *log_module, HOSTIF_MsgData_t *stMsgData);

/**
 * @brief set_Device_DeviceInfo_xOpsDeviceMgmt_hwHealthTestWAN_WANEndPointURL
 *
 * This method is used to set the hardware health test WAN test case URL.
 * with following TR-069 definition:
 *   Parameter Name: Device.DeviceInfo.X_RDKCENTRAL-COM_xOpsDeviceMgmt.hwHealthTestWAN.WANTestEndPointURL
 *   Data type: string - URL to set.
 *
 * @retval true if it is successful.
 * @retval false if operation fails.
 */
bool set_Device_DeviceInfo_RFC_hwHealthTestWAN_WANEndPointURL(const char *log_module, HOSTIF_MsgData_t *stMsgData);

/**
 * @brief set_Device_DeviceInfo_xRDKCentralComRFC_hwHealthTest_ResultFilter_Enable
 *
 * This method is used to enable/disable the hardware health test result-filter functionality.
 * with following TR-069 definition:
 *   Parameter Name: Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.hwHealthTest.ResultFilter.Enable
 *   Data type: boolean - Setting this will enable/disable health test result-filter functionality.
 *
 * @retval true if it is successful.
 * @retval false if operation fails.
 */
bool set_Device_DeviceInfo_xRDKCentralComRFC_hwHealthTest_ResultFilter_Enable(const char *log_module, HOSTIF_MsgData_t *stMsgData);

/**
 * @brief set_Device_DeviceInfo_xRDKCentralComRFC_hwHealthTest_ResultFilter_QueueDepth
 *
 * This method is used to set the number of last results to be stored for hardware health test result-filter feature.
 * with following TR-069 definition:
 *   Parameter Name: Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.hwHealthTest.ResultFilter.QueueDepth
 *   Data type: unsignedInt - QueueDepth to set (<=100).
 *
 * @retval true if it is successful.
 * @retval false if operation fails.
 */
bool set_Device_DeviceInfo_xRDKCentralComRFC_hwHealthTest_ResultFilter_QueueDepth(const char *log_module, HOSTIF_MsgData_t *stMsgData);

/**
 * @brief set_Device_DeviceInfo_xRDKCentralComRFC_hwHealthTest_ResultFilter_FilterParams
 *
 * This method is used to set filter parameters to be applied for each component of hardware health test result-filter feature.
 * with following TR-069 definition:
 *   Parameter Name: Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.hwHealthTest.ResultFilter.FilterParams
 *   Data type: string - ordered list of comma separated values.
 *
 * @retval true if it is successful.
 * @retval false if operation fails.
 */
bool set_Device_DeviceInfo_xRDKCentralComRFC_hwHealthTest_ResultFilter_FilterParams(const char *log_module, HOSTIF_MsgData_t *stMsgData);

/**
 * @brief set_Device_DeviceInfo_xRDKCentralComRFC_hwHealthTest_ResultFilter_ResultsFiltered
 *
 * This method is used to enable the hardware health test filtered results to shown on UI/WEBPA/Resultsfile.
 * with following TR-069 definition:
 *   Parameter Name: Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.hwHealthTest.ResultFilter.ResultsFiltered
 *   Data type: boolean - Enable (True)/disable (False) health test filtered results display.
 *
 * @retval true if it is successful.
 * @retval false if operation fails.
 */
bool set_Device_DeviceInfo_xRDKCentralComRFC_hwHealthTest_ResultFilter_ResultsFiltered(const char *log_module, HOSTIF_MsgData_t *stMsgData);

void filterTelemetry();
} // namespace hwselftest

#endif /* DEVICEINFO_HWHEALTHTEST_H_ */
