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
#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([HW Self Test], [0.1])
AM_INIT_AUTOMAKE([foreign subdir-objects])
AC_CONFIG_SRCDIR([agent/core/wa_comm.h])
AC_CONFIG_HEADERS([config.h])

LT_INIT

AC_ARG_WITH([wwwdir],
		[AS_HELP_STRING([--with-wwwdir=DIR], [target directory for WWW files])],
		[wwwdir=$withval],
		[wwwdir="/var/www/hwselftest"])
AC_SUBST([wwwdir], [$wwwdir])

AC_ARG_ENABLE([debug],
		[AS_HELP_STRING([--enable-debug], [turns on debug messages])],
		[if test "x$enableval" != xyes; then wa_debug=0; else wa_debug=1; fi],
		[wa_debug=0])
AC_SUBST([DEFAULT_WA_DEBUG], [$wa_debug])

AC_ARG_WITH([mfrvendor],
		[AS_HELP_STRING([--with-mfrvendor=VENDOR], [MFR library vendor name])],
		[mfrvendor=$withval],
		[])
AC_SUBST([MFRVENDOR], [$mfrvendor])

AC_ARG_WITH([mfrlib],
		[AS_HELP_STRING([--with-mfrlib=LIB], [MFR library file])],
		[mfrlib=$withval],
		[])
AC_SUBST([MFRLIB], [$mfrlib])

AM_CONDITIONAL([USE_MFRLIB], [test x$MFRLIB != x && test x$MFRVENDOR != x])


AC_ARG_ENABLE([agent-build],
        AS_HELP_STRING([--enable-agent-build],[enable agent build (default is no)]),
        [
          case "${enableval}" in
           yes) AGENT_ENABLE=true
                m4_syscmd([test -d agent])
                m4_if(m4_sysval,[0],[AC_CONFIG_FILES([agent/Makefile])])
                m4_if(m4_sysval,[0],[SUBDIRS_AGENT="agent"]) ;;
           no) AGENT_ENABLE=false;;
           esac
        ],
        [echo "agent build is disabled"])

AC_ARG_ENABLE([client-build],
        AS_HELP_STRING([--enable-client--build],[enable client build (default is no)]),
        [
          case "${enableval}" in
           yes) CLIENT_ENABLE=true
                m4_syscmd([test -d client])
                m4_if(m4_sysval,[0],[AC_CONFIG_FILES([client/Makefile])])
                m4_if(m4_sysval,[0],[SUBDIRS_CLIENT="client"]) ;;
           no) CLIENT_ENABLE=false;;
           esac
        ],
        [echo "client build is disabled"])

AC_ARG_ENABLE([tr69profile-build],
        AS_HELP_STRING([--enable-tr69profile-build],[enable tr69profile build (default is no)]),
        [
          case "${enableval}" in
           yes) TR69PROFILE_ENABLE=true
                m4_syscmd([test -d tr69profile])
                m4_if(m4_sysval,[0],[AC_CONFIG_FILES([tr69profile/Makefile])])
                m4_if(m4_sysval,[0],[SUBDIRS_TR69PROFILE="tr69profile"]) ;;
           no) TR69PROFILE_ENABLE=false;;
           esac
        ],
        [echo "tr69profile build is disabled"])

AC_ARG_ENABLE([cli-build],
        AS_HELP_STRING([--enable-cli-build],[enable CLI tool build (default is no)]),
        [
          case "${enableval}" in
           yes) CLI_ENABLE=true
                m4_syscmd([test -d cli])
                m4_if(m4_sysval,[0],[AC_CONFIG_FILES([cli/Makefile])])
                m4_if(m4_sysval,[0],[SUBDIRS_CLI="cli"]) ;;
           no) CLI_ENABLE=false;;
           esac
        ],
        [echo "cli build is disabled"])

AC_SUBST([COMPONENTS], ["${SUBDIRS_AGENT} ${SUBDIRS_CLIENT} ${SUBDIRS_TR69PROFILE} ${SUBDIRS_CLI}"])


# Select RF4CE test method...

RF4CE_ENABLE_FLAG=" "
AC_ARG_ENABLE([rf4ce],
        AS_HELP_STRING([--enable-rf4ce],[enable RF4CE Support (default is no)]),
        [
          case "${enableval}" in
           yes) RF4CE_SUPPORT_ENABLED=true
                RF4CE_ENABLE_FLAG="-DUSE_RF4CEMGR";;
           no) RF4CE_SUPPORT_ENABLED=false AC_MSG_ERROR([RF4CE support is disabled]) ;;
          *) AC_MSG_ERROR([bad value ${enableval} for --enable-rf4ce ]) ;;
           esac
           ],
         [echo "RF4CE support is disabled"])
AM_CONDITIONAL([WITH_RF4CE_SUPPORT], [test x$RF4CE_SUPPORT_ENABLED = xtrue])
AC_SUBST(RF4CE_ENABLE_FLAG)

CTRLMGR_ENABLE_FLAG=" "
AC_ARG_ENABLE([ctrlmgr],
        AS_HELP_STRING([--enable-ctrlmgr],[enable CTRLMGR Support (default is no)]),
        [
          case "${enableval}" in
           yes) CTRLMGR_SUPPORT_ENABLED=true
                CTRLMGR_ENABLE_FLAG="-DUSE_CTRLMGR";;
           no) CTRLMGR_SUPPORT_ENABLED=false AC_MSG_ERROR([CTRLMGR support is disabled]) ;;
          *) AC_MSG_ERROR([bad value ${enableval} for --enable-ctrlmgr ]) ;;
           esac
           ],
         [echo "CTRLMGR support is disabled"])
AM_CONDITIONAL([WITH_CTRLMGR_SUPPORT], [test x$CTRLMGR_SUPPORT_ENABLED = xtrue])
AC_SUBST(CTRLMGR_ENABLE_FLAG)


# Select diags to compile...

DIAG_ENABLE_FLAGS=""

AC_ARG_WITH([diag-hdd],
        AS_HELP_STRING([--with-diag-hdd],[enable HDD test (default is no)]),
        AS_IF([test "x$enableval" = xyes], [DIAG_ENABLE_FLAGS+=" -DHAVE_DIAG_HDD" DIAG_HDD_ENABLE=true]),
        [echo "HDD test support is disabled"])
AM_CONDITIONAL([HAVE_DIAG_HDD], [test x$DIAG_HDD_ENABLE = xtrue])

AC_ARG_WITH([diag-dram],
        AS_HELP_STRING([--with-diag-hdd],[enable DRAM test (default is no)]),
        AS_IF([test "x$enableval" = xyes], [DIAG_ENABLE_FLAGS+=" -DHAVE_DIAG_DRAM" DIAG_DRAM_ENABLE=true]),
        [echo "DRAM test support is disabled"])
AM_CONDITIONAL([HAVE_DIAG_DRAM], [test x$DIAG_DRAM_ENABLE = xtrue])

AC_ARG_WITH([diag-flash],
        AS_HELP_STRING([--with-diag-flash],[enable FLASH test (default is no)]),
        AS_IF([test "x$enableval" = xyes], [DIAG_ENABLE_FLAGS+=" -DHAVE_DIAG_FLASH" DIAG_FLASH_ENABLE=true]),
        [echo "FLASH test support is disabled"])
AM_CONDITIONAL([HAVE_DIAG_FLASH], [test x$DIAG_FLASH_ENABLE = xtrue])

AC_ARG_WITH([diag-hdmiout],
        AS_HELP_STRING([--with-diag-hdmiout],[enable HDMIOUT test (default is no)]),
        AS_IF([test "x$enableval" = xyes], [DIAG_ENABLE_FLAGS+=" -DHAVE_DIAG_HDMIOUT" DIAG_HDMIOUT_ENABLE=true]),
        [echo "HDMIOUT test support is disabled"])
AM_CONDITIONAL([HAVE_DIAG_HDMIOUT], [test x$DIAG_HDMIOUT_ENABLE = xtrue])

AC_ARG_WITH([diag-ir],
        AS_HELP_STRING([--with-diag-ir],[enable IR test (default is no)]),
        AS_IF([test "x$enableval" = xyes], [DIAG_ENABLE_FLAGS+=" -DHAVE_DIAG_IR" DIAG_IR_ENABLE=true]),
        [echo "IR test support is disabled"])
AM_CONDITIONAL([HAVE_DIAG_IR], [test x$DIAG_IR_ENABLE = xtrue])

AC_ARG_WITH([diag-rf4ce],
        AS_HELP_STRING([--with-diag-rf4ce],[enable RF4CE test (default is no)]),
        AS_IF([test "x$enableval" = xyes], [DIAG_ENABLE_FLAGS+=" -DHAVE_DIAG_RF4CE" DIAG_RF4CE_ENABLE=true]),
        [echo "RF4CE test support is disabled"])
AM_CONDITIONAL([HAVE_DIAG_RF4CE], [test x$DIAG_RF4CE_ENABLE = xtrue])

AC_ARG_WITH([diag-modem],
        AS_HELP_STRING([--with-diag-modem],[enable MODEM test (default is no)]),
        AS_IF([test "x$enableval" = xyes], [DIAG_ENABLE_FLAGS+=" -DHAVE_DIAG_MODEM" DIAG_MODEM_ENABLE=true]),
        [echo "MODEM test support is disabled"])
AM_CONDITIONAL([HAVE_DIAG_MODEM], [test x$DIAG_MODEM_ENABLE = xtrue])

AC_ARG_WITH([diag-mcard],
        AS_HELP_STRING([--with-diag-mcard],[enable MCARD test (default is no)]),
        AS_IF([test "x$enableval" = xyes], [DIAG_ENABLE_FLAGS+=" -DHAVE_DIAG_MCARD" DIAG_MCARD_ENABLE=true]),
        [echo "MCARD test support is disabled"])
AM_CONDITIONAL([HAVE_DIAG_MCARD], [test x$DIAG_MCARD_ENABLE = xtrue])

AC_ARG_WITH([diag-moca],
        AS_HELP_STRING([--with-diag-moca],[enable MOCA test (default is no)]),
        AS_IF([test "x$enableval" = xyes], [DIAG_ENABLE_FLAGS+=" -DHAVE_DIAG_MOCA" DIAG_MOCA_ENABLE=true]),
        [echo "MOCA test support is disabled"])
AM_CONDITIONAL([HAVE_DIAG_MOCA], [test x$DIAG_MOCA_ENABLE = xtrue])

AC_ARG_WITH([diag-tuner],
        AS_HELP_STRING([--with-diag-tuner],[enable TUNER test (default is no)]),
        AS_IF([test "x$enableval" = xyes], [DIAG_ENABLE_FLAGS+=" -DHAVE_DIAG_TUNER" DIAG_TUNER_ENABLE=true]),
        [echo "TUNER test support is disabled"])
AM_CONDITIONAL([HAVE_DIAG_TUNER], [test x$DIAG_TUNER_ENABLE = xtrue])
AM_CONDITIONAL([USE_TRM], [test x$DIAG_TUNER_ENABLE = xtrue])

AC_ARG_WITH([diag-avdecoder-qam],
        AS_HELP_STRING([--with-diag-avdecoder-qam],[enable AVDECODER_QAM test (default is no)]),
        AS_IF([test "x$enableval" = xyes], [DIAG_ENABLE_FLAGS+=" -DHAVE_DIAG_AVDECODER_QAM" DIAG_AVDECODER_QAM_ENABLE=true]),
        [echo "AVDECODER_QAM test support is disabled"])
AM_CONDITIONAL([HAVE_DIAG_AVDECODER_QAM], [test x$DIAG_AVDECODER_QAM_ENABLE = xtrue])

AC_ARG_WITH([diag-bluetooth],
        AS_HELP_STRING([--with-diag-bluetooth],[enable BLUETOOTH test (default is no)]),
        AS_IF([test "x$enableval" = xyes], [DIAG_ENABLE_FLAGS+=" -DHAVE_DIAG_BLUETOOTH" DIAG_BLUETOOTH_ENABLE=true]),
        [echo "BLUETOOTH test support is disabled"])
AM_CONDITIONAL([HAVE_DIAG_BLUETOOTH], [test x$DIAG_BLUETOOTH_ENABLE = xtrue])

AM_CONDITIONAL([HAVE_DIAG_FILE], [test x$DIAG_FLASH_ENABLE = xtrue -o x$DIAG_DRAM_ENABLE = xtrue])

AC_SUBST([DIAG_ENABLE_FLAGS])


# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_RANLIB
AM_PROG_AR

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h fcntl.h malloc.h netinet/in.h stddef.h stdint.h stdlib.h string.h sys/ioctl.h sys/socket.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([gettimeofday inet_ntoa memset socket strchr strerror])

AC_CONFIG_FILES([Makefile])

AC_OUTPUT
