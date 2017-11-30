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
        core/diag/wa_diag_prev_results.c \
        core/diag/wa_diag_file.c \
        core/diag/wa_diag_dram.c \
        core/diag/wa_diag_flash.c \
        core/diag/wa_diag_hdd.c \
        core/diag/wa_diag_hdmiout.c \
        core/diag/wa_diag_ir.c \
        core/diag/wa_diag_mcard.c \
        core/diag/wa_diag_moca.c \
        core/diag/wa_diag_modem.c \
        core/diag/wa_diag_rf4ce.c \
        core/diag/wa_diag_tuner.c \
        core/diag/wa_diag_avdecoder_qam.cpp \
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
        core/wa_log.c \
        core/wa_agg.c

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

hwselftest_LDFLAGS = -ljansson -pthread -lrt -lnetsnmp -lds -ldshalcli -lIARMBus -lwebsockets -lhnsource -lmediaplayersink -lrmfbase -lrmfosal -lglib-2.0 -lrdkloggers


hwselftest_CFLAGS = \
        $(RF4CE_ENABLE_FLAG) \
        $(CTRLMGR_ENABLE_FLAG) \
        -std=gnu99 \
        -pthread \
        -Wall \
        -Werror

if USE_MFRLIB
    hwselftest_CPPFLAGS += -DLEGACY_MFR_VENDOR=$(MFRVENDOR)
    hwselftest_LDFLAGS += -l$(MFRLIB)
endif

#if USE_TRM
    hwselftest_SOURCES += core/utils/rdk/wa_trh.cpp
    hwselftest_CFLAGS += -DUSE_TRM
    hwselftest_LDFLAGS += -ltrm -ltrh
#endif
