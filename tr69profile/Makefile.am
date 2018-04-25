##########################################################################
# If not stated otherwise in this file or this component's Licenses.txt
# file the following copyright and licenses apply:
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
##########################################################################
#

SUBDIRS =

AM_CXXFLAGS = -I$(PKG_CONFIG_SYSROOT_DIR)/usr/include/nopoll

lib_LTLIBRARIES = libtr69ProfileHwSelfTest.la
libtr69ProfileHwSelfTest_la_SOURCES = \
    DeviceInfo_hwHealthTest.cpp \
    wa_wsclient.cpp \
    wa_service.cpp \
    wa_settings.cpp \
    hwst_comm.cpp \
    hwst_diag.cpp \
    hwst_diag_av.cpp \
    hwst_diag_dram.cpp \
    hwst_diag_flash.cpp \
    hwst_diag_hdd.cpp \
    hwst_diag_hdmiout.cpp \
    hwst_diag_ir.cpp \
    hwst_diag_mcard.cpp \
    hwst_diag_moca.cpp \
    hwst_diag_modem.cpp \
    hwst_diag_rf4ce.cpp \
    hwst_diag_sysinfo.cpp \
    hwst_diag_tuner.cpp \
    hwst_diag_bluetooth.cpp \
    hwst_diag_prev_results.cpp \
    hwst_diag_capabilities.cpp \
    hwst_diagfactory.cpp \
    hwst_log.cpp \
    hwst_scenario.cpp \
    hwst_scenario_set.cpp \
    hwst_scenario_all.cpp \
    hwst_scenario_auto.cpp \
    hwst_sched.cpp \
    hwst_ws.cpp

libtr69ProfileHwSelfTest_la_CXXFLAGS = $(AM_CXXFLAGS) -std=c++11
libtr69ProfileHwSelfTest_la_LDFLAGS = $(AM_LDFLAGS) -ljansson -lnopoll
