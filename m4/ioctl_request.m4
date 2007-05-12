dnl Simple macro to find the type of the ioctl request parameter
dnl Copyright (c) 2007 xine project
dnl
dnl This program is free software; you can redistribute it and/or modify
dnl it under the terms of the GNU General Public License as published by
dnl the Free Software Foundation; either version 2, or (at your option)
dnl any later version.
dnl
dnl This program is distributed in the hope that it will be useful,
dnl but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
dnl GNU General Public License for more details.
dnl
dnl You should have received a copy of the GNU General Public License
dnl along with this program; if not, write to the Free Software
dnl Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
dnl 02110-1301, USA.
dnl
dnl As a special exception, the xine project, as copyright owner of the
dnl macro gives unlimited permission to copy, distribute and modify the
dnl configure scripts that are the output of Autoconf when processing the
dnl Macro. You need not follow the terms of the GNU General Public
dnl License when using or distributing such scripts, even though portions
dnl of the text of the Macro appear in them. The GNU General Public
dnl License (GPL) does govern all other use of the material that
dnl constitutes the Autoconf Macro.
dnl 
dnl This special exception to the GPL applies to versions of the
dnl Autoconf Macro released by the xine project. When you make and
dnl distribute a modified version of the Autoconf Macro, you may extend
dnl this special exception to the GPL to apply to your modified version as
dnl well.


dnl Usage AC_IOCTL_REQUEST
AC_DEFUN([AC_IOCTL_REQUEST], [
    AC_CACHE_CHECK([type of request parameter for ioctl()], [ac_cv_ioctl_request], [
        for ac_ioctl_request_type in "unsigned long" "int"; do
            AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <sys/ioctl.h>
                                              int ioctl(int fd, $ac_ioctl_request_type request, ...);]], [[]])],
                           [ac_cv_ioctl_request=$ac_ioctl_request_type], [])
        done
        if test x"$ac_cv_ioctl_request" = x""; then
            AC_MSG_ERROR([Unable to determine the type for ioctl() request parameter])
        fi
    ])
    AC_DEFINE_UNQUOTED([IOCTL_REQUEST_TYPE], [$ac_cv_ioctl_request], [Type of the request parameter for ioctl()])
])
