dnl -------------
dnl Input Plugins
dnl -------------
AC_DEFUN([XINE_INPUT_PLUGINS], [
    dnl Setup defaults for the target operating system.  For example, v4l is
    dnl only ever available on Linux, so don't bother checking for it unless
    dnl explicitly requested to do so on other operating systems.
    dnl Notes:
    dnl - v4l is Linux only

    default_enable_gnomevfs=enable
    default_enable_samba=enable
    default_enable_v4l=disable
    default_enable_vcd=enable

    default_with_internal_vcdlibs=without

    case "$host_os" in
        linux*) default_enable_v4l=enable ;;
    esac


    dnl gnome-vfs
    AC_ARG_ENABLE([gnomevfs],
                  [AS_HELP_STRING([--disable-gnomevfs], [do not build gnome-vfs support])],
                  [], [test $default_enable_gnomevfs = disable && enable_gnomevfs=no])
    if test x"$enable_gnomevfs" != x"no"; then
        PKG_CHECK_MODULES([GNOME_VFS], [gnome-vfs-2.0], [no_gnome_vfs=no], [no_gnome_vfs=yes])
        if test x"$no_gnome_vfs" != x"yes"; then
            AC_DEFINE([HAVE_GNOME_VFS], 1, [Define this if you have gnome-vfs installed])
        fi
    else
        no_gnome_vfs=yes
    fi
    AM_CONDITIONAL([HAVE_GNOME_VFS], [test x"$no_gnome_vfs" != x"yes"])


    dnl libsmbclient
    AC_ARG_ENABLE([samba],
                  [AS_HELP_STRING([--disable-samba], [do not build Samba support])],
                  [], [test $default_enable_samba = disable && enable_samba=no])
    if test x"$enable_samba" != x"no"; then
        AC_CHECK_LIB([smbclient], [smbc_init],
                     [AC_CHECK_HEADERS([libsmbclient.h], [have_libsmbclient=yes
                                                          LIBSMBCLIENT_LIBS="-lsmbclient"])])
        AC_SUBST(LIBSMBCLIENT_LIBS)
    fi
    AM_CONDITIONAL([HAVE_LIBSMBCLIENT], [test x"$have_libsmbclient" = x"yes"])


    dnl video-for-linux (v4l)
    AC_ARG_ENABLE([v4l],
                  [AS_HELP_STRING([--disable-v4l], [do not build Video4Linux input plugin])],
                  [], [test $default_enable_v4l = disable && enable_v4l=no])
    if test x"$enable_v4l" != x"no"; then
        AC_CHECK_HEADERS([linux/videodev.h], [have_v4l=yes], [have_v4l=no])
        AC_CHECK_HEADERS([asm/types.h])
        if test x"$enable_v4l" = x"yes" && test x"$have_v4l" = x"no"; then
            AC_MSG_ERROR([Video4Linux support requested, but prerequisite headers not found.])
        fi
    fi
    AM_CONDITIONAL([HAVE_V4L], [test x"$have_v4l" = x"yes"])


    dnl cdrom ioctls (common for dvdnav and vcd)
    case "$host_os" in
        linux*)
            AC_CHECK_HEADERS([linux/cdrom.h],
                             [AC_DEFINE([HAVE_LINUX_CDROM], 1, [Define 1 if you have Linux-type CD-ROM support])
                              AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <linux/cdrom.h>]],
                                                                 [[struct cdrom_generic_command test; int has_timeout = sizeof(test.timeout);]])],
                                                [AC_DEFINE([HAVE_LINUX_CDROM_TIMEOUT], [1], [Define 1 if timeout is in cdrom_generic_command struct])])])
            ;;
    esac
    AC_CHECK_HEADERS([sys/dvdio.h sys/cdio.h sys/scsiio.h])
    AC_CACHE_CHECK([if cdrom ioctls are available], [am_cv_have_cdrom_ioctls],
                   [AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <sys/ioctl.h>]], [[CDROM_DRIVE_STATUS]])],
                                      [am_cv_have_cdrom_ioctls=yes],
                   [AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <sys/ioctl.h>]], [[CDIOCALLOW]])],
                                      [am_cv_have_cdrom_ioctls=yes],
                   [AC_EGREP_CPP([we_have_cdrom_ioctls],
                                 [#include <sys/ioctl.h>
                                  #ifdef HAVE_SYS_CDIO_H
                                  #  include <sys/cdio.h>
                                  #endif
                                  #ifdef HAVE_LINUX_CDROM_H
                                  #  include <linux/cdrom.h>
                                  #endif
                                  #if defined(CDROM_DRIVE_STATUS) || defined(CDIOCALLOW) || defined(CDROMCDXA)
                                  we_have_cdrom_ioctls
                                  #endif],
                                  [am_cv_have_cdrom_ioctls=yes], [am_cv_have_cdrom_ioctls=no])])])])
    have_cdrom_ioctls="$am_cv_have_cdrom_ioctls"
    if test x"$have_cdrom_ioctls" = x"yes"; then
        AC_DEFINE([HAVE_CDROM_IOCTLS], 1, [Define this if you have CDROM ioctls])
    fi
    AM_CONDITIONAL([HAVE_CDROM_IOCTLS], [test x"$have_cdrom_ioctls" = x"yes"])


    dnl dvdnav
    dnl REVISIT: Something doesn't feel right about this ... I'm not sure it works as intended
    AC_ARG_WITH([external-dvdnav],
                [AS_HELP_STRING([--with-external-dvdnav], [use external dvdnav library (not recommended)])],
                [external_dvdnav="$withval"], [no_dvdnav=yes external_dvdnav=no])
    if test "x$external_dvdnav" = "xyes"; then
        AM_PATH_DVDNAV([0.1.9],
                       [AC_DEFINE([HAVE_DVDNAV], 1, [Define this if you have a suitable version of libdvdnav])],
                       [AC_MSG_RESULT([*** no usable version of libdvdnav found, using internal copy ***])])
    else
        AC_MSG_RESULT([Using included DVDNAV support])
    fi
    AM_CONDITIONAL([HAVE_DVDNAV], [test x"$no_dvdnav" != x"yes"])


    dnl Video CD
    AC_ARG_ENABLE([vcd],
                  [AS_HELP_STRING([--disable-vcd], [do not compile VCD plugin])],
                  [], [test $default_enable_vcd = disable && enable_vcd=no])
    if test x"$enable_vcd" != x"no"; then
        AC_ARG_WITH([internal-vcdlibs],
                    [AS_HELP_STRING([--with-internal-vcdlibs], [force using internal libcdio/libvcd/libvcdinfo])],
                    [], [test $default_with_internal_vcdlibs = without && with_internal_vcdlibs=no])
        dnl check twice - fallback is to use internal vcdlibs
        if test x"$with_internal_vcdlibs" = x"no"; then
            PKG_CHECK_MODULES([LIBCDIO], [libcdio >= 0.71], [], [with_internval_vcdlibs=yes])
            PKG_CHECK_MODULES([LIBVCDINFO], [libvcdinfo >= 0.7.23], [], [with_internval_vcdlibs=yes])
            if test x"$with_internval_vcdlibs" = x"yes"; then
                AC_MSG_RESULT([Using included libcdio/libvcdinfo support])
            fi
        fi
        if test "x$with_internval_vcdlibs" = "xno"; then
            AC_DEFINE([HAVE_VCDNAV], 1, [Define this if you use external libcdio/libvcdinfo])
        else
            AC_DEFINE_UNQUOTED([HOST_ARCH], ["$host_os/$host_cpu"], [host os/cpu identifier])
            AC_DEFINE([_DEVELOPMENT_], [], [enable warnings about being development release])
            AC_CHECK_FUNCS([bzero memcpy])

            AC_CHECK_MEMBER([struct tm.tm_gmtoff],
                            [AC_DEFINE([HAVE_TM_GMTOFF], 1, [Define if struct tm has the tm_gmtoff member.])],
                            [], [#include <time.h>])

            dnl empty_array_size
            AC_MSG_CHECKING([how to create empty arrays])
            AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[struct { int foo; int bar[]; } baz]])],
                              [empty_array_size=""],
                              [AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[struct { int foo; int bar[0]; } baz]])],
                                                 [empty_array_size="0"],
                                                 [AC_MSG_ERROR([compiler is unable to create empty arrays])])])

            AC_DEFINE_UNQUOTED([EMPTY_ARRAY_SIZE], [$empty_array_size], [what to put between the brackets for empty arrays])
            AC_MSG_RESULT([[[$empty_array_size]]])

            dnl ISOC99_PRAGMA
            AC_MSG_CHECKING([whether $CC supports ISOC99 _Pragma()])
            AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[_Pragma("pack(1)")]])],
                              [ISOC99_PRAGMA=yes 
                               AC_DEFINE([HAVE_ISOC99_PRAGMA], [], [Supports ISO _Pragma() macro])],
                              [ISOC99_PRAGMA=no])
            AC_MSG_RESULT([$ISOC99_PRAGMA])

            AC_CHECK_HEADERS([errno.h fcntl.h glob.h stdbool.h])
            if test x"$ac_cv_header_stdint_h" != x"yes"; then
                AC_CHECK_SIZEOF([int], 4)
                AC_CHECK_SIZEOF([long], 4)
                AC_CHECK_SIZEOF([long long], 8)
            fi

            dnl
            dnl bitfield order
            dnl
            AC_MSG_CHECKING([bitfield ordering in structs])

            dnl basic compile test for all platforms
            AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[
    struct { char bit_0:1, bit_12:2, bit_345:3, bit_67:2; }
    #if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ > 4)
        __attribute__((packed))
    #endif
    bf = { 1,1,1,1 };
    switch (0) case 0: case sizeof(bf) == 1:;]])],
                              [], [AC_MSG_ERROR([compiler doesn't support bitfield structs])])


            dnl run test
            AC_RUN_IFELSE([[
    int main() {
        struct { char bit_0:1, bit_12:2, bit_345:3, bit_67:2; }
    #if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ > 4)
            __attribute__((packed))
    #endif
        bf = { 1,1,1,1 };
        if (sizeof (bf) != 1) return 1;
        return *((unsigned char*) &bf) != 0x4b;
    }]], [bf_lsbf=1], [
            AC_RUN_IFELSE([[
    int main() {
        struct { char bit_0:1, bit_12:2, bit_345:3, bit_67:2; }
    #if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ > 4)
            __attribute__((packed))
    #endif
        bf = { 1,1,1,1 };
        if (sizeof (bf) != 1) return 1;
        return *((unsigned char*) &bf) != 0xa5;
    }]], [bf_lsbf=0], [AC_MSG_ERROR([unsupported bitfield ordering])])],
            [case "$host" in
                *-*-mingw32* | *-*-cygwin* | i?86-* | k?-* | athlon-* | pentium*- | x86_64-*)
                    bf_lsbf=1
                    ;;
                universal-*-darwin*)
                    bf_lsbf=2
                    ;;
                powerpc-* | powerpc64-* | ppc-* | sparc*-* | mips-*)
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
                    ;;
            esac])

            if test "x$cross_compiling" = "xyes"; then
                TEXT=" (guessed)"
            else
                TEXT=""
            fi
            if test "x$bf_lsbf" = "x1"; then
                AC_MSG_RESULT([LSBF${TEXT}])
                AC_DEFINE([BITFIELD_LSBF], [], [compiler does lsbf in struct bitfields])
            else
                if test "x$bf_lsbf" = "x2"; then
                    AC_MSG_RESULT([indeterminate (universal build)])
                else
                    AC_MSG_RESULT([MSBF${TEXT}])
                fi
            fi

            dnl REVISIT: CFLAGS stuff here is wrong; it should be CPPFLAGS
            LIBCDIO_CFLAGS='-I$(top_srcdir)/src/input/vcd/libcdio'
            LIBCDIO_LIBS='$(top_builddir)/src/input/vcd/libcdio/libcdio.la'
            LIBISO9660_LIBS='$(top_builddir)/src/input/vcd/libcdio/libiso9660.la'
            LIBVCD_CFLAGS='-I$(top_srcdir)/src/input/vcd/libvcd'
            LIBVCD_LIBS='$(top_builddir)/src/input/vcd/libvcd/libvcd.la'
            LIBVCDINFO_LIBS='$(top_builddir)/src/input/vcd/libvcd/libvcdinfo.la'

            case "$host_os" in
                bsdi*)
                    AC_CHECK_HEADERS([dvd.h],
                                     [AC_DEFINE([HAVE_BSDI_CDROM], 1, [Define 1 if you have BSDI-type CD-ROM support])])
                    ;;
                cygwin*)
                    AC_DEFINE([CYGWIN], 1, [Define 1 if you are compiling using cygwin])
                    AC_DEFINE([HAVE_WIN32_CDROM], 1, [Define 1 if you have MinGW CD-ROM support])
                    LIBCDIO_LIBS="$LIBCDIO_LIBS -lwinmm"
                    LIBVCD_LIBS="$LIBVCD_LIBS -lwinmm"
                    ;;
                darwin*)
                    AC_CHECK_HEADERS([IOKit/IOKitLib.h CoreFoundation/CFBase.h],
                                     [AC_DEFINE([HAVE_DARWIN_CDROM], 1, [Define 1 if you have Darwin OS X-type CD-ROM support])])
                    ;;
                freebsd4.*)
                    AC_DEFINE([HAVE_FREEBSD_CDROM], 1, [Define 1 if you have FreeBSD CD-ROM support])
                    ;;
                linux*)
                    AC_CHECK_HEADERS([linux/version.h])
                    ;;
                mingw*)
                    AC_DEFINE([MINGW32], 1, [Define 1 if you are compiling using MinGW])
                    AC_DEFINE([HAVE_WIN32_CDROM], 1, [Define 1 if you have MinGW CD-ROM support])
                    ;;
                sunos*|sun*|solaris*)
                    AC_CHECK_HEADERS([sys/cdio.h],
                                     [AC_DEFINE([HAVE_SOLARIS_CDROM], 1, [Define 1 if you have Solaris CD-ROM support])])
                    ;;
                *)
                    AC_MSG_WARN([Don't have OS CD-reading support for ${host_os} ... Will use generic support.])
                    ;;
            esac

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
    AM_CONDITIONAL([HAVE_VCDNAV], [test x"$with_internval_vcdlibs" = x"no"])
    AM_CONDITIONAL([ENABLE_VCD], [test x"$enable_vcd" = x"yes"])
])









