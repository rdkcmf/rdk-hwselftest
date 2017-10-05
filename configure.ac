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

PAROUDS_ENABLE_FLAG=" "

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
        AS_HELP_STRING([--enable-client],[enable client build (default is no)]),
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
        [echo "tr60profile build is disabled"])

AC_ARG_ENABLE([parodus],
        AS_HELP_STRING([--enable-parodus],[enable Parodus Support (default is no)]),
        [
          case "${enableval}" in
           yes) PARODUS_SUPPORT_ENABLED=true
                PARODUS_ENABLE_FLAG="-DPARODUS_ENABLE";; 
           no) PARODUS_SUPPORT_ENABLED=false AC_MSG_ERROR([Parodus support is disabled]) ;;
          *) AC_MSG_ERROR([bad value ${enableval} for --enable-parodus ]) ;;
           esac
           ],
         [echo "Parodus support is disabled"])

AM_CONDITIONAL([WITH_PARODUS_SUPPORT], [test x$PARODUS_SUPPORT_ENABLED = xtrue])
     
AC_SUBST([COMPONENTS], ["${SUBDIRS_AGENT} ${SUBDIRS_CLIENT} ${SUBDIRS_TR69PROFILE}"])
AC_SUBST(PARODUS_ENABLE_FLAG)
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
