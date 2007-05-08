dnl -----------------
dnl Video out plugins
dnl -----------------
AC_DEFUN([XINE_VIDEO_OUT_PLUGINS], [

dnl ---------------------------------------------
dnl Check for OpenGL & [GLut | GLU]
dnl ---------------------------------------------

AM_PATH_OPENGL()


dnl ---------------------------------------------
dnl Check for platform which supports syncfb
dnl ---------------------------------------------

AC_ARG_ENABLE([syncfb],
	AS_HELP_STRING([--disable-syncfb], [do not build syncfb plugin]))

case "$host_os" in
  *linux*) ;;
  *)
    if test "x$enable_syncfb" = "xyes"; then
       AC_MSG_ERROR([You cannot build SyncFB support on non-Linux systems.])
    fi
    enable_syncfb=no ;;
esac

AM_CONDITIONAL(HAVE_SYNCFB, test "x$enable_syncfb" != "xno")


XINE_XV_SUPPORT

dnl ---------------------------------------------
dnl Check for xcb
dnl ---------------------------------------------
AC_ARG_WITH([xcb], AS_HELP_STRING([--without-xcb], [Doesn't build XCB video out plugins]))

if test "x$with_xcb" != "xno"; then
  PKG_CHECK_MODULES([XCB], [xcb-shape >= 1.0], [have_xcb="yes"], [have_xcb="no"])
fi

AC_SUBST(XCB_CFLAGS)
AC_SUBST(XCB_LIBS)
AM_CONDITIONAL(HAVE_XCB, test "x$have_xcb" = "xyes" )


dnl ---------------------------------------------
dnl Check for xcb-shm
dnl ---------------------------------------------

if test "x$have_xcb" = "xyes"; then
  PKG_CHECK_MODULES([XCBSHM], [xcb-shm], [have_xcbshm="yes"], [have_xcbshm="no"])
fi

AC_SUBST(XCBSHM_CFLAGS)
AC_SUBST(XCBSHM_LIBS)
AM_CONDITIONAL(HAVE_XCBSHM, test "x$have_xcbshm" = "xyes" )


dnl ---------------------------------------------
dnl Check for xcb-xv
dnl ---------------------------------------------

if test "x$have_xcb" = "xyes"; then
  PKG_CHECK_MODULES([XCBXV], [xcb-xv], [have_xcbxv="yes"], [have_xcbxv="no"])
fi

AC_SUBST(XCBXV_CFLAGS)
AC_SUBST(XCBXV_LIBS)
AM_CONDITIONAL(HAVE_XCBXV, test "x$have_xcbxv" = "xyes" )


dnl ---------------------------------------------
dnl Checks for Xinerama extension
dnl ---------------------------------------------

AC_ARG_ENABLE([xinerama],
  AS_HELP_STRING([--disable-xinerama], [do not build Xinerama support]))

if test "x$enable_xinerama" != "xno"; then
   PKG_CHECK_MODULES([XINERAMA], [xinerama], [ac_have_xinerama=yes], [
      AC_CHECK_LIB(Xinerama, XineramaQueryExtension, 
                  [XINERAMA_LIBS="-lXinerama"
                   ac_have_xinerama="yes"],,
                  [$X_LIBS $X_PRE_LIBS -lXext $X_EXTRA_LIBS])
   ])
   if test "x$ac_have_xinerama" = "xyes"; then
      AC_DEFINE(HAVE_XINERAMA,1,[Define this if you have libXinerama installed])
      X_LIBS="${X_LIBS} ${XINERAMA_LIBS}"
   fi
else
  ac_have_xinerama=no
fi
dnl AM_CONDITIONAL(HAVE_XINERAMA, test "x$ac_have_xinerama" = "xyes")

 
dnl ---------------------------------------------
dnl Checks for Ascii-Art library
dnl ---------------------------------------------

AC_ARG_ENABLE([aalib],
  AS_HELP_STRING([--disable-aalib], [do not build AALIB support]),
  [with_aalib=$enableval], [with_aalib=yes])

if test "x$with_aalib" = "xyes"; then
  AM_PATH_AALIB(1.4,, AC_MSG_RESULT([*** All of AALIB dependent parts will be disabled ***]))
else
  no_aalib=yes
fi

AM_CONDITIONAL(HAVE_AA, test "x$no_aalib" != "xyes")

dnl ---------------------------------------------
dnl Checks for Color AsCii Art library
dnl ---------------------------------------------

AC_ARG_WITH([caca],
  AS_HELP_STRING([--without-caca], [Do not build CACA support]))

if test "x$with_caca" != "xno"; then
   PKG_CHECK_MODULES([CACA], [caca cucul], [have_caca="yes"], [have_caca="no"])
   if test "x$with_caca" = "xyes" && test "x$have_caca" = "xno"; then
      AC_MSG_ERROR([CACA support requested, but libcaca 0.99 not found])
   fi
fi

AM_CONDITIONAL([HAVE_CACA], [test "x$have_caca" = "xyes"])

dnl ---------------------------------------------
dnl Check solaris framebuffer device support
dnl ---------------------------------------------

AC_CHECK_HEADER(sys/fbio.h, ac_have_sunfb=yes,)
AM_CONDITIONAL(HAVE_SUNFB, [test "x$ac_have_sunfb" = "xyes"])


dnl ---------------------------------------------
dnl Check for Sun DGA
dnl ---------------------------------------------

saved_LDFLAGS="$LDFLAGS"
LDFLAGS="$LDFLAGS -L/usr/openwin/lib"
saved_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="$CPPFLAGS -I/usr/openwin/include"
AC_CHECK_LIB(dga, XDgaGrabDrawable, [
	AC_CHECK_HEADER(dga/dga.h, [
	        SUNDGA_CFLAGS="-I/usr/openwin/include"
		SUNDGA_LIBS="-L/usr/openwin/lib -R/usr/openwin/lib -ldga"
		ac_have_sundga=yes
	])
])
LDFLAGS="$saved_LDFLAGS"
CPPFLAGS="$saved_CPPFLAGS"
AM_CONDITIONAL(HAVE_SUNDGA, [test "x$ac_have_sundga" = "xyes"])
AC_SUBST(SUNDGA_CFLAGS)
AC_SUBST(SUNDGA_LIBS)


dnl ---------------------------------------------
dnl Check linux framebuffer device support
dnl ---------------------------------------------

AC_CHECK_HEADER(linux/fb.h,
                [AC_DEFINE(HAVE_FB,1,[Define this if you have linux framebuffer support])
                 have_fb=yes],)
AC_ARG_ENABLE(fb, AS_HELP_STRING([--disable-fb], [do not build linux framebuffer support]),
	      have_fb=$enableval)
AM_CONDITIONAL(HAVE_FB, [test "x$have_fb" = "xyes"])


dnl ---------------------------------------------
dnl Check whether to build Mac OS X video output driver
dnl ---------------------------------------------

MACOSX_VIDEO_SUPPORT




dnl ---------------------------------------------
dnl Check for DirectFB
dnl ---------------------------------------------
AC_ARG_ENABLE(directfb,
	AS_HELP_STRING([--enable-directfb], [enable use of DirectFB]),
		enable_directfb=$enableval,
		enable_directfb=no)

if test "x$enable_directfb" = "xyes"; then
  PKG_CHECK_MODULES([DIRECTFB], [directfb >= 0.9.22], [have_directfb="yes"], [have_directfb="no"])
fi

AC_SUBST(DIRECTFB_CFLAGS)
AC_SUBST(DIRECTFB_LIBS)
AM_CONDITIONAL(HAVE_DIRECTFB, test "x$have_directfb" = "xyes" )


dnl ---------------------------------------------
dnl check for SDL
dnl ---------------------------------------------

AC_ARG_WITH([sdl],
	AS_HELP_STRING([--without-sdl], [Build without SDL video output]))

if test "x$with_sdl" != "xno"; then
   PKG_CHECK_MODULES([SDL], [sdl], [have_sdl=yes], [have_sdl=no])
   if test "x$with_sdl" = "xyes" && test "x$have_sdl" = "xno"; then
      AC_MSG_ERROR([SDL support requested, but SDL not found])
   elif test "x$have_sdl" = "xyes"; then
      AC_DEFINE([HAVE_SDL], [1], [Define this if you have SDL installed])
   fi
fi

AM_CONDITIONAL([HAVE_SDL], [test "x$have_sdl" = "xyes"])

AC_SUBST([SDL_CFLAGS])
AC_SUBST([SDL_LIBS])

dnl ---------------------------------------------
dnl check for Libstk
dnl ---------------------------------------------

AC_ARG_WITH([libstk],
	AS_HELP_STRING([--with-libstk], [Build with STK surface video driver]))

if test "x$with_libstk" = "xyes"; then
   PKG_CHECK_MODULES([LIBSTK], [libstk >= 0.2.0], [have_libstk=yes], [have_libstk=no])
   if test "x$with_libstk" = "xyes" && test "x$have_libstk" = "xno"; then
      AC_MSG_ERROR([libstk support requested, but libstk not found])
   fi
fi

AM_CONDITIONAL([HAVE_STK], [test "x$have_libstk" = "xyes"])

dnl ---------------------------------------------
dnl check for DirectX
dnl ---------------------------------------------

AM_PATH_DIRECTX()


dnl ---------------------------------------------
dnl dxr3 / hollywood plus card
dnl ---------------------------------------------

case "$host_or_hostalias" in
  *-linux*)
    AC_CHECK_DXR3()
    if test "x$have_libfame" = "xyes" ; then
      AC_DEFINE_UNQUOTED(HAVE_LIBFAME,1,[Define this if you have libfame mpeg encoder installed (fame.sf.net)])
      AM_PATH_LIBFAME(0.8.10, 
        AC_DEFINE(HAVE_NEW_LIBFAME,1,[Define this if you have libfame 0.8.10 or above]))
    fi
    if test "x$have_librte" = "xyes" ; then
      AC_DEFINE_UNQUOTED(HAVE_LIBRTE,1,[Define this if you have librte mpeg encoder installed (zapping.sf.net)])
    fi
    ;;
  *)
    have_dxr3="no"
    have_libfame="no"
    have_librte="no"
    have_encoder="no"
    ;;
esac
AM_CONDITIONAL(HAVE_DXR3, test "x$have_dxr3" = "xyes")
AM_CONDITIONAL(HAVE_LIBFAME, test "x$have_libfame" = "xyes")
AM_CONDITIONAL(HAVE_LIBRTE, test "x$have_librte" = "xyes")


dnl ---------------------------------------------
dnl Vidix/libdha
dnl ---------------------------------------------

AC_LINUX_PATH(/usr/src/linux)
AC_SUBST([LINUX_INCLUDE])

AC_ARG_ENABLE(vidix, AS_HELP_STRING([--disable-vidix], [do not build vidix support]),
	      check_vidix=$enableval, check_vidix=yes)
AC_ARG_ENABLE(dha-kmod, AS_HELP_STRING([--enable-dha-kmod], [build DHA kernel module]),
	      enable_dha_kmod=$enableval,enable_dha_kmod=no)

enable_vidix="no"

AC_MSG_CHECKING(for vidix support)
if test "x$check_vidix" = "xyes" -a "x$ac_cv_prog_AWK" != "xno"; then
  if test "x$no_x" != "xyes" -o "x$have_fb" = "xyes"; then
    case "$host_or_hostalias" in
      i?86-*-linux* | k?-*-linux* | athlon-*-linux*)
        enable_vidix="yes"
        enable_linux="yes"
        ;;
      i?86-*-freebsd* | k?-*-freebsd* | athlon-*-freebsd*)
        enable_vidix="yes"
        enable_dha_kmod="no"
        ;;
      *)
        enable_dha_kmod="no"
        enable_vidix="no"
        ;;
    esac
  fi
fi
AC_MSG_RESULT($enable_vidix)

AC_MSG_CHECKING(for DHA linux kernel module build)
if test "x$enable_dha_kmod" = "xyes"; then
  AC_MSG_RESULT(yes)
else
  AC_MSG_RESULT(no)
fi

AM_CONDITIONAL(HAVE_VIDIX, test "x$enable_vidix" = "xyes")
AM_CONDITIONAL(HAVE_LINUX, test "x$enable_linux" = "xyes")
AM_CONDITIONAL(BUILD_DHA_KMOD, test "x$enable_dha_kmod" = "xyes")
AC_CHECK_PROG(MKNOD, mknod, mknod, no)
AC_CHECK_PROG(DEPMOD, depmod, depmod, no, "$PATH:/sbin")

])dnl XINE_VIDEO_OUT_PLUGINS
