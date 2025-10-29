#
# Copyright (C) 2025, Tencent. All rights reserved.
# DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
#
# This code is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation. Tencent designates
# this particular file as subject to the "Classpath" exception as provided
# in the LICENSE file that accompanied this code.
#
# This code is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License version 2 for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#

################################################################################
# Setup OpenSSL for building the native crypto libraries.
################################################################################

AC_DEFUN_ONCE([LIB_SETUP_OPENSSL],
[
  AC_ARG_WITH([openssl], [AS_HELP_STRING([--with-openssl=DIR],
      [Specify absolute path to OpenSSL installation for crypto JNI])],
    [
      if test -d "$withval"; then
        KONA_OPENSSL_HOME="$withval"
        UTIL_FIXUP_PATH([KONA_OPENSSL_HOME])
        AC_MSG_CHECKING([for OpenSSL])
        AC_MSG_RESULT([$KONA_OPENSSL_HOME])
      else
        AC_MSG_ERROR([OpenSSL path '$withval' does not exist or is not a directory])
      fi
    ],
    [
      KONA_OPENSSL_HOME=""
    ]
  )

  AC_SUBST(KONA_OPENSSL_HOME)
])
