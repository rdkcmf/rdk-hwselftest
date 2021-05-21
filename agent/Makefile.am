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

bin_PROGRAMS = hwselftest
dist_sysconf_DATA = ../platform/config/hwselftest.conf

hwselftest_SOURCES = \
        adaptation/os/linux/wa_osa.c \
        wa_main.c \
        core/diag/wa_diag_sysinfo.c \
        core/diag/wa_diag_capabilities.c \
        core/diag/wa_diag_prev_results.c \
        core/diag/wa_diag_filter.c \
        core/utils/fileops/wa_fileops.c \
        core/utils/id/wa_id.c \
        core/utils/json/wa_json.c \
        core/utils/list/wa_list_api.c \
        core/utils/snmp/wa_snmp_client.c \
        core/utils/rdk/wa_iarm.cpp \
        core/utils/rdk/wa_mfr.cpp \
        core/utils/rdk/wa_mgr.cpp \
        core/utils/rdk/wa_rmf.c \
        core/utils/rdk/wa_vport.cpp \
        core/utils/rdk/wa_sicache.c \
        core/comm/wa_comm_ws.c \
        core/wa_comm.c \
        core/wa_diag.c \
        core/wa_init.c \
        core/wa_config.c \
        core/wa_log.c \
        core/wa_agg.c

if HAVE_DIAG_FILE
    hwselftest_SOURCES += core/diag/wa_diag_file.c
endif
if HAVE_DIAG_HDD
    hwselftest_SOURCES += core/diag/wa_diag_hdd.c
endif
if HAVE_DIAG_SDCARD
    hwselftest_SOURCES += core/diag/wa_diag_sdcard.c
endif
if HAVE_DIAG_DRAM
    hwselftest_SOURCES += core/diag/wa_diag_dram.c
endif
if HAVE_DIAG_FLASH
   hwselftest_SOURCES += core/diag/wa_diag_flash.c
endif
if HAVE_DIAG_FLASH_XI6
   hwselftest_SOURCES += core/diag/wa_diag_flash.c
endif
if HAVE_DIAG_HDMIOUT
   hwselftest_SOURCES += core/diag/wa_diag_hdmiout.c
endif
if HAVE_DIAG_IR
   hwselftest_SOURCES += core/diag/wa_diag_ir.c
endif
if HAVE_DIAG_RF4CE
   hwselftest_SOURCES += core/diag/wa_diag_rf4ce.c
endif
if HAVE_DIAG_MODEM
   hwselftest_SOURCES += core/diag/wa_diag_modem.c
endif
if HAVE_DIAG_MCARD
   hwselftest_SOURCES += core/diag/wa_diag_mcard.c
endif
if HAVE_DIAG_MOCA
   hwselftest_SOURCES += core/diag/wa_diag_moca.c
endif
if HAVE_DIAG_TUNER
   hwselftest_SOURCES += core/diag/wa_diag_tuner.c
endif
if HAVE_DIAG_AVDECODER_QAM
   hwselftest_SOURCES += core/diag/wa_diag_avdecoder.cpp
endif
if HAVE_DIAG_BLUETOOTH
   hwselftest_SOURCES += core/diag/wa_diag_bluetooth.c
endif
if HAVE_DIAG_WIFI
    hwselftest_SOURCES += core/diag/wa_diag_wifi.c
endif
if HAVE_DIAG_WAN
    hwselftest_SOURCES += core/diag/wa_diag_wan.c
endif

WA_DEBUG ?= $(DEFAULT_WA_DEBUG)

hwselftest_CPPFLAGS = \
        -Icore -I$(srcdir)/core \
        -Icore/comm -I$(srcdir)/core/comm \
        -Icore/diag -I$(srcdir)/core/diag \
        -Icore/utils/fileops -I$(srcdir)/core/utils/fileops \
        -Icore/utils/id -I$(srcdir)/core/utils/id \
        -Icore/utils/json -I$(srcdir)/core/utils/json \
        -Icore/utils/list -I$(srcdir)/core/utils/list \
        -Icore/utils/snmp -I$(srcdir)/core/utils/snmp \
        -Icore/utils/rdk -I$(srcdir)/core/utils/rdk \
        -I=/usr/include/glib-2.0 -I=/usr/lib/glib-2.0/include \
        '-DSYSCONFDIR="$(sysconfdir)"' \
        -DWA_DEBUG=$(WA_DEBUG)

hwselftest_LDFLAGS = -ljansson -pthread -lrt -lnetsnmp -lds -ldshalcli -lIARMBus -lwebsockets -lhnsource -lmediaplayersink -lrmfbase -lrmfosal -lglib-2.0 -lrdkloggers -lbreakpadwrapper -lrfcapi

if USE_NEXUS
    hwselftest_LDFLAGS += -lnxclient
endif

hwselftest_CFLAGS = \
        $(RF4CE_ENABLE_FLAG) \
        $(CTRLMGR_ENABLE_FLAG) \
        $(DIAG_ENABLE_FLAGS) \
        -std=gnu99 \
        -pthread \
        -Wall \
        -Werror

if USE_MFRLIB
    hwselftest_CPPFLAGS += -DLEGACY_MFR_VENDOR=$(MFRVENDOR)
    hwselftest_LDFLAGS += -l$(MFRLIB)
endif

if USE_TRM
    hwselftest_SOURCES += core/utils/rdk/wa_trh.cpp
    hwselftest_CFLAGS += -DUSE_TRM
    hwselftest_LDFLAGS += -ltrm -ltrh
endif

hwselftest_CFLAGS += -DENABLE_THERMAL_PROTECTION
hwselftest_CPPFLAGS += -DENABLE_THERMAL_PROTECTION
