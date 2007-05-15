dnl -------------
dnl Input Plugins
dnl -------------
AC_DEFUN([XINE_INPUT_PLUGINS], [
    dnl Setup defaults for the target operating system.  For example, v4l is
    dnl only ever available on Linux, so don't bother checking for it unless
    dnl explicitly requested to do so on other operating systems.
    dnl Notes:
    dnl - dvb is Linux only
    dnl - v4l is Linux only

    default_enable_dvb=disable
    default_enable_gnomevfs=enable
    default_enable_samba=enable
    default_enable_v4l=disable
    default_enable_vcd=disable

    default_with_internal_vcdlibs=without

    case "$host_os" in
        cygwin* | mingw*)
            default_enable_gnomevfs=disable
            default_enable_samba=disable
            ;;
        darwin*)
            default_enable_gnomevfs=disable
            default_enable_samba=disable
            ;;
        freebsd*)
            default_enable_vcd=enable
            ;;
        linux*)
            default_enable_v4l=enable
            default_enable_vcd=enable
            ;;
        solaris*)
            default_enable_vcd=enable
            ;;
    esac

    dnl dvb
    AC_ARG_ENABLE([dvb],
                  [AS_HELP_STRING([--enable-dvb], [Enable support for the DVB plugin (Linux only)])],
                  [test x"$enableval" != x"no" && enable_dvb="yes"],
                  [test $default_enable_dvb = disable && enable_dvb="no"])
    if test x"$enable_dvb" != x"no"; then
        case "$host_os" in
            linux*) have_dvb=yes ;;
            *) have_dvb=no ;;
        esac
        if test x"$enable_dvb" = x"yes" && test x"$have_dvb" != x"yes"; then
            AC_MSG_ERROR([DVB support requested, but DVB not found])
        fi
    fi
    AM_CONDITIONAL([ENABLE_DVB], [test x"$have_dvb" = x"yes"])


    dnl gnome-vfs
    AC_ARG_ENABLE([gnomevfs],
                  [AS_HELP_STRING([--enable-gnomevfs], [Enable support for the Gnome-VFS plugin])],
                  [test x"$enableval" != x"no" && enable_gnomevfs="yes"],
                  [test $default_enable_gnomevfs = disable && enable_gnomevfs="no"])
    if test x"$enable_gnomevfs" != x"no"; then
        PKG_CHECK_MODULES([GNOME_VFS], [gnome-vfs-2.0], [have_gnomevfs=yes], [have_gnome_vfs=no])
        if test x"$enable_gnomevfs" = x"yes" && test x"$have_gnomevfs" != x"yes"; then
            AC_MSG_ERROR([Gnome-VFS support requested, but Gnome-VFS not found])
        fi
    fi
    AM_CONDITIONAL([ENABLE_GNOME_VFS], [test x"$have_gnomevfs" = x"yes"])


    dnl libsmbclient
    AC_ARG_ENABLE([samba],
                  [AS_HELP_STRING([--enable-samba], [Enable support for the Samba plugin])],
                  [test x"$enableval" != x"no" && enable_samba="yes"],
                  [test $default_enable_samba = disable && enable_samba="no"])
    if test x"$enable_samba" != x"no"; then
        AC_CHECK_LIB([smbclient], [smbc_init],
                     [AC_CHECK_HEADERS([libsmbclient.h], [have_libsmbclient=yes LIBSMBCLIENT_LIBS="-lsmbclient"])])
        AC_SUBST(LIBSMBCLIENT_LIBS)
        if test x"$enable_samba" = x"yes" && test x"$have_samba" != x"yes"; then
            AC_MSG_ERROR([Samba support requested, but Samba not found])
        fi
    fi
    AM_CONDITIONAL([ENABLE_LIBSMBCLIENT], [test x"$have_samba" = x"yes"])


    dnl video-for-linux (v4l)
    AC_ARG_ENABLE([v4l],
                  [AS_HELP_STRING([--enable-v4l], [Enable Video4Linux support])],
                  [test x"$enableval" != x"no" && enable_v4l="yes"],
                  [test $default_enable_v4l = disable && enable_v4l="no"])
    if test x"$enable_v4l" != x"no"; then
        AC_CHECK_HEADERS([linux/videodev.h], [have_v4l=yes], [have_v4l=no])
        AC_CHECK_HEADERS([asm/types.h])
        if test x"$enable_v4l" = x"yes" && test x"$have_v4l" != x"yes"; then
            AC_MSG_ERROR([Video4Linux support requested, but prerequisite headers not found.])
        fi
    fi
    AM_CONDITIONAL([ENABLE_V4L], [test x"$have_v4l" = x"yes"])


    dnl dvdnav
    dnl XXX: This could be cleaned up so that code does not have to ifdef so much
    AC_ARG_WITH([external-dvdnav],
                [AS_HELP_STRING([--with-external-dvdnav], [Use external dvdnav library (not recommended)])],
                [test x"$withval" != x"no" && with_external_dvdnav="yes"], [with_external_dvdnav="no"])
    if test x"$with_external_dvdnav" != x"no"; then
        ACX_PACKAGE_CHECK([DVDNAV], [0.1.9], [dvdnav-config],
                          [AC_DEFINE([HAVE_DVDNAV], 1, [Define this if you have a suitable version of libdvdnav])],
                          [AC_MSG_RESULT([*** no usable version of libdvdnav found, using internal copy ***])])
    else
        AC_MSG_RESULT([Using included DVDNAV support])
    fi
    AM_CONDITIONAL([WITH_EXTERNAL_DVDNAV], [test x"$with_external_dvdnav" != x"no"])


    dnl Video CD
    dnl XXX: This could be cleaned up so that code does not have it ifdef so much
    AC_ARG_ENABLE([vcd],
                  [AS_HELP_STRING([--enable-vcd], [Enable VCD (VideoCD) support])],
                  [test x"$enableval" != x"no" && enable_vcd="yes"],
                  [test $default_enable_vcd = disable && enable_vcd="no"])
    AC_ARG_WITH([internal-vcdlibs],
                [AS_HELP_STRING([--with-internal-vcdlibs], [force using internal libcdio/libvcd/libvcdinfo])],
                [test x"$withval" != x"no" && with_internal_vcdlibs="yes"],
                [test $default_with_internal_vcdlibs = without && with_internal_vcdlibs="no"])
    if test x"$enable_vcd" != x"no"; then
        dnl check twice - fallback is to use internal vcdlibs
        if test x"$with_internal_vcdlibs" = x"no"; then
            PKG_CHECK_MODULES([LIBCDIO], [libcdio >= 0.71], [], [with_internal_vcdlibs=yes])
            PKG_CHECK_MODULES([LIBVCDINFO], [libvcdinfo >= 0.7.23], [], [with_internal_vcdlibs=yes])
            if test x"$with_internal_vcdlibs" = x"yes"; then
                AC_MSG_RESULT([Using included libcdio/libvcdinfo support])
            fi
        fi
        if test x"$with_internal_vcdlibs" = x"no"; then
            AC_DEFINE([HAVE_VCDNAV], 1, [Define this if you use external libcdio/libvcdinfo])
        else
            AC_DEFINE_UNQUOTED([HOST_ARCH], ["$host_os/$host_cpu"], [host os/cpu identifier])
            AC_DEFINE([_DEVELOPMENT_], [], [enable warnings about being development release])

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
                    LIBVCD_LIBS="$LIBVCD_LIBS -framework CoreFoundation -framework IOKit"
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
    AM_CONDITIONAL([WITH_EXTERNAL_VCDLIBS], [test x"$with_internal_vcdlibs" = x"no"])
    AM_CONDITIONAL([ENABLE_VCD], [test x"$enable_vcd" != x"no"])
])
