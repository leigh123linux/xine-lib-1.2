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
    default_enable_vcd=enable
    default_enable_vcdo=disable

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
            default_enable_vcdo=enable
            ;;
        linux*)
            default_enable_dvb=enable
            default_enable_v4l=enable
            default_enable_vcdo=enable
            ;;
        solaris*)
            default_enable_vcdo=enable
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
                     [AC_CHECK_HEADERS([libsmbclient.h], [have_samba=yes LIBSMBCLIENT_LIBS="-lsmbclient"])])
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
        have_v4l=yes
        AC_CHECK_HEADERS([linux/videodev.h linux/videodev2.h], , [have_v4l=no])
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
    if test x"$enable_vcd" != x"no"; then
        PKG_CHECK_MODULES([LIBCDIO], [libcdio >= 0.71])
        PKG_CHECK_MODULES([LIBVCDINFO], [libvcdinfo >= 0.7.23])
        AC_DEFINE([HAVE_VCDNAV], 1, [Define this if you use external libcdio/libvcdinfo])
    fi

    enable_vcdo=no
    test $default_enable_vcdo = enable && test x"$enable_vcd" != x"no" && enable_vcdo=yes

    AC_DEFINE([LIBCDIO_CONFIG_H], 1, [Get of rid system libcdio build configuration])
    AC_DEFINE([EXTERNAL_LIBCDIO_CONFIG_H], 1, [Get of rid system libcdio build configuration])
    AC_SUBST(LIBCDIO_CFLAGS)
    AC_SUBST(LIBCDIO_LIBS)
    AC_SUBST(LIBVCD_CFLAGS)
    AC_SUBST(LIBVCD_LIBS)
    AM_CONDITIONAL([ENABLE_VCD], [test x"$enable_vcd" != x"no"])
    AM_CONDITIONAL([ENABLE_VCDO], [test x"$enable_vcdo" != x"no"])
])
