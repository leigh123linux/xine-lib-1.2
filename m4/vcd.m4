AC_DEFUN([VCD_SUPPORT], [
    AC_ARG_ENABLE(vcd, AS_HELP_STRING([--disable-vcd], [do not compile VCD plugin]),
                  enable_vcd=$enableval, enable_vcd=yes)

dnl Force build of both vcd plugins, for now.
dnl AC_ARG_ENABLE(vcdo, AS_HELP_STRING([--disable-vcdo], [do not compile old VCD plugin]),
dnl               enable_vcdo=$enableval, enable_vcdo=yes)
dnl
    enable_vcdo="yes"

    AC_ARG_WITH(internal-vcdlibs, AS_HELP_STRING([--with-internal-vcdlibs], [force using internal libcdio/libvcd/libvcdinfo]),
                [internal_vcdnav="$withval"], [internal_vcdnav="no"])

    if test "x$enable_vcd" = "xyes"; then
        dnl empty_array_size
        AC_MSG_CHECKING([how to create empty arrays])

        empty_array_size="xxx"
        AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[struct { int foo; int bar[]; } doo;]])],[empty_array_size=""],[])

        if test "x$empty_array_size" = "xxxx";then
            AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[struct { int foo; int bar[0]; } doo;]])],[empty_array_size="0"],[])
        fi

        if test "x$empty_array_size" = "xxxx"
        then
            AC_MSG_ERROR([compiler is unable to creaty empty arrays])
        else
            AC_DEFINE_UNQUOTED(EMPTY_ARRAY_SIZE, $empty_array_size, 
                               [what to put between the brackets for empty arrays])
            changequote(`,')
            msg="[${empty_array_size}]"
            changequote([,])
            AC_MSG_RESULT($msg)
        fi
        dnl empty_array_size

        if test "x$internal_vcdnav" = "xno" ; then
            PKG_CHECK_MODULES([LIBCDIO], [libcdio >= 0.71], [], [internal_vcdnav=yes])
            PKG_CHECK_MODULES([LIBVCDINFO], [libvcdinfo >= 0.7.23], [], [internal_vcdnav=yes])
            if test "x$internal_vcdnav" = "xyes"; then
                AC_MSG_RESULT([Use included libcdio/libvcdinfo support])
            fi
        fi

        dnl check twice, fallback is internal copy
        if test "x$internal_vcdnav" = "xno"; then
            AC_DEFINE([HAVE_VCDNAV], [1], [Define this if you use external libcdio/libvcdinfo])
        else
            AC_DEFINE_UNQUOTED(HOST_ARCH, "$host_os/$host_cpu", [host os/cpu identifier])

            AC_DEFINE(_DEVELOPMENT_, [], enable warnings about being development release)
            AC_HEADER_STDC
            AC_CHECK_HEADERS(sys/stat.h stdint.h glob.h inttypes.h stdbool.h)

            if test "x$ac_cv_header_stdint_h" != "xyes" 
            then
                AC_CHECK_SIZEOF(int, 4)
                AC_CHECK_SIZEOF(long, 4)
                AC_CHECK_SIZEOF(long long, 8)
            fi

            dnl ISOC99_PRAGMA
            AC_MSG_CHECKING([whether $CC supports ISOC99 _Pragma()])
            AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[_Pragma("pack(1)")]])],[
                    ISOC99_PRAGMA=yes 
                    AC_DEFINE(HAVE_ISOC99_PRAGMA, [], [Supports ISO _Pragma() macro])
                    ],[ISOC99_PRAGMA=no])
            AC_MSG_RESULT($ISOC99_PRAGMA)

            dnl
            dnl bitfield order
            dnl
            AC_MSG_CHECKING([bitfield ordering in structs])

            dnl basic compile test for all platforms
            AC_COMPILE_IFELSE([
int main() {
  struct { char bit_0:1, bit_12:2, bit_345:3, bit_67:2; }
#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ > 4)
  __attribute__((packed))
#endif
  bf = { 1,1,1,1 };
  switch (0) case 0: case sizeof(bf) == 1:;
  return 0;
}
], [], AC_MSG_ERROR([compiler doesn't support bitfield structs]))


            dnl run test
            AC_RUN_IFELSE([
int main() {
  struct { char bit_0:1, bit_12:2, bit_345:3, bit_67:2; }
#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ > 4)
  __attribute__((packed))
#endif
  bf = { 1,1,1,1 };
  if (sizeof (bf) != 1) return 1;
  return *((unsigned char*) &bf) != 0x4b; }
], bf_lsbf=1, [
            AC_RUN_IFELSE([
int main() {
  struct { char bit_0:1, bit_12:2, bit_345:3, bit_67:2; }
#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ > 4)
  __attribute__((packed))
#endif
 bf = { 1,1,1,1 };
 if (sizeof (bf) != 1) return 1;
 return *((unsigned char*) &bf) != 0xa5; }
], bf_lsbf=0, AC_MSG_ERROR([unsupported bitfield ordering]))
            ],
    [case "$host" in
        *-*-mingw32* | *-*-cygwin*)
            bf_lsbf=1
            ;;
        universal-*-darwin*)
            bf_lsbf=0
            ;;
        *)
            AC_MSG_RESULT([unknown])
            AC_MSG_ERROR([value of bitfield test isn't known for $host
*********************************************************************
Value of bitfield test can't be found out for cross-compiling and we
don't know its value for host "$host".

Because it's needed for VCD plugin, disable VCD by configure option
--disable-vcd or use external VCD library.
*********************************************************************])
    esac])

            if test "x$cross_compiling" = "xyes"; then
                TEXT=" (guessed)"
            else
                TEXT=""
            fi
            if test "x$bf_lsbf" = "x1"; then
                AC_MSG_RESULT(LSBF${TEXT})
                AC_DEFINE(BITFIELD_LSBF, [], [compiler does lsbf in struct bitfields])
            else
                AC_MSG_RESULT(MSBF${TEXT})
            fi

            AC_CHECK_HEADERS([errno.h fcntl.h \
                             stdbool.h  stdlib.h stdint.h stdio.h string.h \
                             strings.h linux/version.h sys/cdio.h sys/stat.h \
                             sys/types.h ])

            LIBCDIO_CFLAGS='-I$(top_srcdir)/src/input/vcd/libcdio'
            LIBCDIO_LIBS='$(top_builddir)/src/input/vcd/libcdio/libcdio.la'
            LIBISO9660_LIBS='$(top_builddir)/src/input/vcd/libcdio/libiso9660.la'
            LIBVCD_CFLAGS='-I$(top_srcdir)/src/input/vcd/libvcd'
            LIBVCD_LIBS='$(top_builddir)/src/input/vcd/libvcd/libvcd.la'
            LIBVCDINFO_LIBS='$(top_builddir)/src/input/vcd/libvcd/libvcdinfo.la'

            case $host_os in
                darwin*)
                    AC_CHECK_HEADERS(IOKit/IOKitLib.h CoreFoundation/CFBase.h, [have_iokit_h="yes"])
                    if test "x$have_iokit_h" = "xyes" ; then 
                        AC_DEFINE([HAVE_DARWIN_CDROM], [1], [Define 1 if you have Darwin OS X-type CD-ROM support])
                    fi
                    ;;
                linux*)
                    AC_CHECK_HEADERS(linux/version.h)
                    AC_CHECK_HEADERS(linux/cdrom.h, [have_linux_cdrom_h="yes"])
                    if test "x$have_linux_cdrom_h" = "xyes" ; then
                        AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[
#include <linux/cdrom.h>
struct cdrom_generic_command test;
int has_timeout=sizeof(test.timeout);]])],
                            [AC_DEFINE([HAVE_LINUX_CDROM_TIMEOUT], [1], [Define 1 if timeout is in cdrom_generic_command struct])],[])
                            AC_DEFINE([HAVE_LINUX_CDROM], [1], [Define 1 if you have Linux-type CD-ROM support])
                    fi
                    ;;
                bsdi*)
                    AC_CHECK_HEADERS(dvd.h, [have_bsdi_dvd_h="yes"])
                    if test "x$have_bsdi_dvd_h" = "xyes" ; then
                        AC_DEFINE([HAVE_BSDI_CDROM], [1], [Define 1 if you have BSDI-type CD-ROM support])
                    fi
                    ;;
                sunos*|sun*|solaris*)
                    AC_CHECK_HEADERS(sys/cdio.h)
                    AC_DEFINE([HAVE_SOLARIS_CDROM], [1], [Define 1 if you have Solaris CD-ROM support])
                    ;;
                cygwin*)
                    AC_DEFINE([CYGWIN], [1], [Define 1 if you are compiling using cygwin])
                    AC_DEFINE([HAVE_WIN32_CDROM], [1], [Define 1 if you have MinGW CD-ROM support])
                    LIBCDIO_LIBS="$LIBCDIO_LIBS -lwinmm"
                    LIBVCD_LIBS="$LIBVCD_LIBS -lwinmm"
                    ;;
                mingw*)
                    AC_DEFINE([MINGW32], [1], [Define 1 if you are compiling using MinGW])
                    AC_DEFINE([HAVE_WIN32_CDROM], [1], [Define 1 if you have MinGW CD-ROM support])
                    ;;
                freebsd4.*)
                    AC_DEFINE([HAVE_FREEBSD_CDROM], [1], [Define 1 if you have FreeBSD CD-ROM support])
                    ;;
                *)
                    AC_MSG_WARN(Don't have OS CD-reading support for ${host_os}...)
                    AC_MSG_WARN(Will use generic support.)
                    ;;
            esac
            AC_SUBST(LINUX_CDROM_TIMEOUT)
            AC_SUBST(HAVE_BSDI_CDROM)
            AC_SUBST(HAVE_DARWIN_CDROM)
            AC_SUBST(HAVE_FREEBSD_CDROM)
            AC_SUBST(HAVE_LINUX_CDROM)
            AC_SUBST(HAVE_SOLARIS_CDROM)
            AC_SUBST(HAVE_WIN32_CDROM)
            AC_SUBST(LINUX_CDROM_TIMEOUT)
            AC_SUBST(LIBVCD_SYSDEP)

            AC_CHECK_FUNCS( bzero memcpy )

            AC_CHECK_MEMBER([struct tm.tm_gmtoff],
                            [AC_DEFINE(HAVE_TM_GMTOFF, 1, [Define if struct tm has the tm_gmtoff member.])],
                            [], [#include <time.h>])
        fi
    fi

    AC_DEFINE([LIBCDIO_CONFIG_H], 1, [Get of rid system libcdio build configuration])
    AC_DEFINE([EXTERNAL_LIBCDIO_CONFIG_H], 1, [Get of rid system libcdio build configuration])
    AC_SUBST(LIBCDIO_CFLAGS)
    AC_SUBST(LIBCDIO_LIBS)
    AC_SUBST(LIBISO9660_LIBS)
    AC_SUBST(LIBVCD_CFLAGS)
    AC_SUBST(LIBVCD_LIBS)
    AC_SUBST(LIBVCDINFO_LIBS)
    AM_CONDITIONAL(HAVE_VCDNAV, [test "x$internal_vcdnav" = "xno"])
    AM_CONDITIONAL(ENABLE_VCD, [test "x$enable_vcd" = "xyes"])
])
