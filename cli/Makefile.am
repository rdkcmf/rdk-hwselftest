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

bin_PROGRAMS = hwselftestcli

PROFILE_DIR = $(top_srcdir)/tr69profile

AM_CXXFLAGS = -I$(PROFILE_DIR) -I$(PKG_CONFIG_SYSROOT_DIR)/usr/include/nopoll


hwselftestcli_SOURCES = \
    wa_cli.cpp

WA_DEBUG ?= $(DEFAULT_WA_DEBUG)

hwselftestcli_CXXFLAGS = $(AM_CXXFLAGS) -std=c++11 -DWA_DEBUG=$(WA_DEBUG)
hwselftestcli_LDFLAGS = $(AM_LDFLAGS) -pthread -lrt -ldl -ljansson -lnopoll
hwselftestcli_LDADD = ../tr69profile/libtr69ProfileHwSelfTest.la -lrdkloggers
