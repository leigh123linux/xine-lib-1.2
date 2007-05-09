dnl -----------------
dnl Video out plugins
dnl -----------------
AC_DEFUN([XINE_VIDEO_OUT_PLUGINS], [
    dnl Setup defaults for the target operating system.  For example, syncfb is
    dnl only ever available on Linux, so don't bother checking for it unless
    dnl explicitly requested to do so on other operating systems.
    dnl Notes:
    dnl - dha_kmod is Linux only, but disabled by default
    dnl - directx is Windows only
    dnl - dxr3 is Linux only
    dnl - Mac OS X video is Mac OS X only
    dnl - SyncFB is Linux only, but disabled by default
    dnl - Vidix is FreeBSD and Linux only

    default_enable_aalib=enable
    default_enable_dha_kmod=disable
    default_enable_directfb=disable
    default_enable_directx=disable
    default_enable_dxr3=disable
    default_enable_linuxfb=disable
    default_enable_macosx_video=disable
    default_enable_syncfb=disable
    default_enable_xinerama=enable
    default_enable_vidix=disable

    default_with_caca=with
    default_with_libstk=without
    default_with_sdl=with
    default_with_xcb=with

    case "$host_os" in
        cygwin* | mingw*)
            default_enable_directx=enable
            ;;

        darwin*)
            default_enable_macosx_video=enable
            ;;

        freebsd*)
            default_enable_vidix=enable
            ;;

        linux*)
            default_enable_dxr3=enable
            default_enable_linuxfb=enable
            default_enable_vidix=enable
            enable_linux=yes
            ;;
    esac


    dnl Video output plugins that depend on X11 support
    if test x$"no_x" != x"yes"; then
        dnl Xinerama
        AC_ARG_ENABLE([xinerama],
                      [AS_HELP_STRING([--enable-xinerama], [enable support for Xinerama])],
                      [], [test $default_enable_xinerama = disable && enable_xinerama=no])
        if test "x$enable_xinerama" != "xno"; then
            PKG_CHECK_MODULES([XINERAMA], [xinerama], [ac_have_xinerama=yes],
                              [AC_CHECK_LIB([Xinerama], [XineramaQueryExtension],
                                            [XINERAMA_LIBS="-lXinerama" ac_have_xinerama="yes"], [],
                                            [$X_LIBS $X_PRE_LIBS -lXext $X_EXTRA_LIBS])])
            if test "x$ac_have_xinerama" = "xyes"; then
                AC_DEFINE([HAVE_XINERAMA], 1, [Define this if you have libXinerama installed])
                X_LIBS="${X_LIBS} ${XINERAMA_LIBS}"
            fi
        else
            ac_have_xinerama=no
        fi
        AM_CONDITIONAL([HAVE_XINERAMA], [test x"$ac_have_xinerama" = x"yes"])

        dnl OpenGL, including GLut and/or GLU
        AM_PATH_OPENGL

        dnl xv
        XINE_XV_SUPPORT
    fi


    dnl Ascii-Art
    AC_ARG_ENABLE([aalib],
                  [AS_HELP_STRING([--enable-aalib], [enable support for AALIB])],
                  [], [test $default_enable_aalib = disable && enable_aalib=no])
    if test x"$enable_aalib" != x"no"; then
        AM_PATH_AALIB([1.4], [], [AC_MSG_RESULT([*** All of AALIB dependent parts will be disabled ***])])
    else
        no_aalib=yes
    fi
    AM_CONDITIONAL([HAVE_AA], [test x"$no_aalib" != x"yes"])


    dnl Color AsCii Art
    AC_ARG_WITH([caca],
                [AS_HELP_STRING([--without-caca], [Do not build CACA support])],
                [], [test $default_with_caca = without && with_caca=no])
    if test x"$with_caca" != x"no"; then
        PKG_CHECK_MODULES([CACA], [caca cucul], [have_caca="yes"], [have_caca="no"])
        if test x"$with_caca" = x"yes" && test x"$have_caca" = x"no"; then
            AC_MSG_ERROR([CACA support requested, but libcaca 0.99 not found])
        fi
    fi
    AM_CONDITIONAL([HAVE_CACA], [test x"$have_caca" = x"yes"])


    dnl dha (Linux only)
    AC_ARG_ENABLE([dha-kmod],
                  [AS_HELP_STRING([--enable-dha-kmod], [build Linux DHA kernel module])],
                  [], [test $default_enable_dha_kmod = disable && enable_dha_kmod=no])
    if test x"$enable_dha_kmod" != x"no"; then
        AC_ARG_WITH([linux-path],
                    [AS_HELP_STRING([--with-linux-path=PATH], [where the linux sources are located])],
                    [linux_path="$withval"], [linux_path="/usr/src/linux"])
        LINUX_INCLUDE="-I$linux_path/include"
        AC_SUBST(LINUX_INCLUDE)
        AC_CHECK_PROG([MKNOD], [mknod], [mknod], [no])
        AC_CHECK_PROG([DEPMOD], [depmod], [depmod], [no], ["$PATH:/sbin"])
    fi
    AM_CONDITIONAL([HAVE_LINUX], [test x"$enable_linux" = x"yes"])
    AM_CONDITIONAL([BUILD_DHA_KMOD], [test x"$enable_dha_kmod" != x"no"])


    dnl DirectFB
    AC_ARG_ENABLE([directfb],
                  [AS_HELP_STRING([--enable-directfb], [enable use of DirectFB])],
                  [], [test $default_enable_directfb = disable && enable_directfb=no])
    if test "x$enable_directfb" = "xyes"; then
        PKG_CHECK_MODULES([DIRECTFB], [directfb >= 0.9.22], [have_directfb="yes"], [have_directfb="no"])
    fi
    AM_CONDITIONAL([HAVE_DIRECTFB], [test x"$have_directfb" = x"yes"])


    dnl DirectX
    AM_PATH_DIRECTX


    dnl dxr3 / hollywood plus card
    AC_ARG_ENABLE([dxr3],
                  [AS_HELP_STRING([--enable-dxr3], [enable support for DXR3/HW+])],
                  [], [test $default_enable_dxr3 = disable && enable_dxr3=no])
    if test x"$enable_dxr3" != x"no"; then
        have_dxr3=yes
        AC_MSG_RESULT([*** checking for a supported mpeg encoder])
        AC_CHECK_LIB([fame], [fame_open],
                     [AC_CHECK_HEADERS([fame.h], [have_libfame=yes], [have_libfame=no])], [have_libfame=no])
        if test x"$have_libfame" = x"yes"; then
            have_encoder=yes
            AC_DEFINE([HAVE_LIBFAME], 1, [Define this if you have libfame mpeg encoder installed (fame.sf.net)])
            AM_PATH_LIBFAME([0.8.10],
                            [AC_DEFINE([HAVE_NEW_LIBFAME], 1, [Define this if you have libfame 0.8.10 or above])])
        fi
        AC_CHECK_LIB([rte], [rte_init],
                     [AC_CHECK_HEADERS([rte.h], [have_librte=yes], [have_librte=no])], [have_librte=no])
        if test x"$have_librte" = x"yes"; then
            have_encoder=yes
            AC_MSG_WARN([this will probably only work with rte version 0.4!])
            AC_DEFINE([HAVE_LIBRTE], 1, [Define this if you have librte mpeg encoder installed (zapping.sf.net)])
        fi
        if test "$have_encoder" = "yes"; then
            AC_MSG_RESULT([*** found one or more external mpeg encoders])
        else
            AC_MSG_RESULT([*** no external mpeg encoder found])
        fi
    else
        have_dxr3=no have_libfame=no have_librte=no have_encoder=no
    fi
    AM_CONDITIONAL([HAVE_DXR3], [test x"$have_dxr3" = x"yes"])
    AM_CONDITIONAL([HAVE_LIBFAME], [test x"$have_libfame" = x"yes"])
    AM_CONDITIONAL([HAVE_LIBRTE], [test x"$have_librte" = x"yes"])


    dnl LibSTK - http://www.libstk.net (project appears to be dead)
    AC_ARG_WITH([libstk],
                [AS_HELP_STRING([--with-libstk], [Build with STK surface video driver])],
                [], [test $default_with_libstk = without && with_libstk=no])
    if test x"$with_libstk" != x"no"; then
        PKG_CHECK_MODULES([LIBSTK], [libstk >= 0.2.0], [have_libstk=yes], [have_libstk=no])
        if test x"$with_libstk" = x"yes" && test x"$have_libstk" = x"no"; then
            AC_MSG_ERROR([libstk support requested, but libstk not found])
        fi
    fi
    AM_CONDITIONAL([HAVE_STK], [test x"$have_libstk" = x"yes"])


    dnl Linux framebuffer device
    AC_ARG_ENABLE([fb],
                  [AS_HELP_STRING([--enable-fb], [enable Linux framebuffer support])],
                  [], [test $default_enable_linuxfb = disable && enable_linuxfb=no])
    if test x"$enable_linuxfb" != x"no"; then
        AC_CHECK_HEADERS([linux/fb.h],
                         [AC_DEFINE([HAVE_FB], 1, [Define this if you have linux framebuffer support])
                          have_fb=yes])
    fi
    AM_CONDITIONAL([HAVE_FB], [test x"$have_fb" = x"yes"])


    dnl Mac OS X OpenGL video output
    dnl TODO: test could be much better, but there's not really much need
    AC_ARG_ENABLE([macosx-video],
                  [AS_HELP_STRING([--enable-macosx-video], [enable support for Mac OS X OpenGL video output])],
                  [have_macosx_video="$enableval"],
                  [test $default_enable_macosx_video = disable && have_macosx_video=no])
    AM_CONDITIONAL([HAVE_MACOSX_VIDEO], [test x"$have_macosx_video" != x"no"])


    dnl SDL
    AC_ARG_WITH([sdl],
                [AS_HELP_STRING([--without-sdl], [Build without SDL video output])],
                [], [test $default_with_sdl = without && with_sdl=no])
    if test "x$with_sdl" != "xno"; then
        PKG_CHECK_MODULES([SDL], [sdl], [have_sdl=yes], [have_sdl=no])
        if test x"$with_sdl" = x"yes" && test x"$have_sdl" = x"no"; then
            AC_MSG_ERROR([SDL support requested, but SDL not found])
        elif test x"$have_sdl" = x"yes"; then
            AC_DEFINE([HAVE_SDL], 1, [Define this if you have SDL installed])
        fi
    fi
    AM_CONDITIONAL([HAVE_SDL], [test x"$have_sdl" = x"yes"])


    dnl Solaris framebuffer device support (exists for more than just Solaris)
    AC_CHECK_HEADERS([sys/fbio.h], [ac_have_sunfb=yes], [ac_have_sunfb=no])
    if test x"$ac_have_sunfb" = x"yes"; then
        saved_CPPFLAGS="$CPPFLAGS" CPPFLAGS="$CPPFLAGS -I/usr/openwin/include"
        saved_LDFLAGS="$LDFLAGS" LDFLAGS="$LDFLAGS -L/usr/openwin/lib"
        AC_CHECK_LIB([dga], [XDgaGrabDrawable],
                     [AC_CHECK_HEADER([dga/dga.h],
                                      [SUNDGA_CFLAGS="-I/usr/openwin/include"
                                       SUNDGA_LIBS="-L/usr/openwin/lib -R/usr/openwin/lib -ldga"
                                       ac_have_sundga=yes])])
        CPPFLAGS="$saved_CPPFLAGS" LDFLAGS="$saved_LDFLAGS"
        AC_SUBST(SUNDGA_CPPFLAGS)
        AC_SUBST(SUNDGA_LIBS)
    fi
    AM_CONDITIONAL([HAVE_SUNDGA], [test x"$ac_have_sundga" = x"yes"])
    AM_CONDITIONAL([HAVE_SUNFB], [test x"$ac_have_sunfb" = x"yes"])


    dnl syncfb (Linux only)
    AC_ARG_ENABLE([syncfb],
                  [AS_HELP_STRING([--enable-syncfb], [enable support for syncfb (Linux only)])],
                  [], [test $default_enable_syncfb = disable && enable_syncfb=no])
    AM_CONDITIONAL([HAVE_SYNCFB], [test x"$enable_syncfb" != x"no"])


    dnl xcb
    AC_ARG_WITH([xcb],
                [AS_HELP_STRING([--without-xcb], [Doesn't build XCB video out plugins])],
                [], [test $default_with_xcb = without && with_xcb=no])
    if test x"$with_xcb" != x"no"; then
        PKG_CHECK_MODULES([XCB], [xcb-shape >= 1.0], [have_xcb="yes"], [have_xcb="no"])
        if test x$"have_xcb" = x"yes"; then
            PKG_CHECK_MODULES([XCBSHM], [xcb-shm], [have_xcbshm="yes"], [have_xcbshm="no"])
            PKG_CHECK_MODULES([XCBXV], [xcb-xv], [have_xcbxv="yes"], [have_xcbxv="no"])
        fi
    fi
    AM_CONDITIONAL([HAVE_XCB], [test x"$have_xcb" = x"yes"])
    AM_CONDITIONAL([HAVE_XCBSHM], [test x"$have_xcbshm" = x"yes"])
    AM_CONDITIONAL([HAVE_XCBXV], [test x"$have_xcbxv" = x"yes"])


    dnl vidix/libdha
    dnl Requires X11 or Linux framebuffer
    AC_ARG_ENABLE([vidix],
                  [AS_HELP_STRING([--enable-vidix], [enable support for Vidix])],
                  [], [test $default_enable_vidix = disable && enable_vidix=no])
    if test x"$enable_vidix" != x"no"; then
        if test x"$ac_cv_prog_AWK" = x"no"; then
            enable_vidix=no
        else
            if test x"$no_x" = x"yes" -o x"$have_fb" != x"yes"; then
                enable_vidix=no
            else
                case "$host_or_hostalias" in
                    i?86-*-linux* | k?-*-linux* | athlon-*-linux*) ;;
                    i?86-*-freebsd* | k?-*-freebsd* | athlon-*-freebsd*) ;;
                    *) enable_vidix="no" ;;
                esac
            fi
        fi
    fi
    AM_CONDITIONAL([HAVE_VIDIX], test x"$enable_vidix" != x"no")
])dnl XINE_VIDEO_OUT_PLUGINS
