# AC_FIND_LIBXV_IMPL (LIB)
# -------------------------
#
AC_DEFUN([AC_PATH_LIBXV_IMPL],
[
  AC_MSG_CHECKING([for $1])
  if test -f "$xv_path/$1"; then
    AC_MSG_RESULT([found $1 in $xv_path])
    XV_LIBS="$1"
  else
    if test -f "/usr/lib/$1"; then
      AC_MSG_RESULT([found $1 in /usr/lib])
      XV_LIBS="$1"
    else
      AC_MSG_RESULT([$1 not found in $xv_path])
    fi
  fi
])

AC_DEFUN([AC_TEST_LIBXV],
[
  dnl -----------------------------------------------
  dnl   Testing installed Xv library
  dnl -----------------------------------------------
  AC_CHECK_LIB(Xv, XvShmCreateImage,
  [
     AC_DEFINE(HAVE_XV,
        1,
        [Define this if you have libXv installed])

     ac_have_xv="yes"
     case x$XV_LIBS in
      x*.a)
        AC_DEFINE(HAVE_XV_STATIC,
                1,
                [Define this if you have libXv.a])
        ac_have_xv_static="yes"
        XV_LIBS="$xv_path/$XV_LIBS"
        ;;
      x*.so)
        XV_LIBS=`echo $XV_LIBS | sed 's/^lib/-l/; s/\.so$//'`
        ;;
      *)
        AC_MSG_ERROR([sorry, I don't know about $XV_LIBS])
        ;;
     esac
    ],
     ,
  [$X_LIBS $X_PRE_LIBS -lXext $X_EXTRA_LIBS])

  dnl -----------------------------------------------
  dnl xine_check use Xv functions API.
  dnl -----------------------------------------------
  if test x$ac_have_xv = "xyes"; then
    EXTRA_X_LIBS="-L$xv_path $XV_LIBS -lXext"
    EXTRA_X_CFLAGS=""
  fi
  AC_SUBST(XV_LIBS)
  AC_SUBST(EXTRA_X_LIBS)
  AC_SUBST(EXTRA_X_CFLAGS)
])

# AC_PATH_LIBXV
# -------------------------
#
AC_DEFUN([AC_FIND_LIBXV],
[
  # Ensure that AC_PATH_XTRA is executed before this
  AC_REQUIRE([AC_PATH_XTRA])

  if test x$xv_path = x; then
    xv_path=/usr/X11R6/lib
  fi

  if test "x$xv_prefer_shared" = "xyes"; then  
    AC_PATH_LIBXV_IMPL([libXv.so])
  else
    AC_PATH_LIBXV_IMPL([libXv.a])
  fi
  
  # Try the other lib if prefered failed
  if test x$XV_LIBS = x; then
    if ! test "x$xv_prefer_shared" = "xyes"; then  
      AC_PATH_LIBXV_IMPL([libXv.so])
    else
      AC_PATH_LIBXV_IMPL([libXv.a])
    fi
  fi

  if ! test x$XV_LIBS = x; then
    AC_TEST_LIBXV
  fi
])

dnl ----------------------------------------------
dnl Check for Xv and XvMC support
dnl ----------------------------------------------

AC_DEFUN([XINE_XV_SUPPORT], [
    dnl With recent XFree86 or Xorg, dynamic linking is preferred!
    dnl Only dynamic linking is possible when using libtool < 1.4.0

    AC_ARG_WITH(xv-path, AS_HELP_STRING([--with-xv-path=path], [where libXv is installed]),
                xv_path="$withval",)

    AC_ARG_ENABLE([static-xv],
            AS_HELP_STRING([--enable-static-xv],[Enable this to force linking against libXv.a]))

    if test "x$enable_static_xv" = "xyes"; then
        xv_prefer_shared="no"
    else
        xv_prefer_shared="yes"
    fi

    if test "x$no_x" != "xyes"; then
        PKG_CHECK_MODULES([XV], [xv], [
            ac_have_xv="yes"
            AC_DEFINE([HAVE_XV], [1], [Define this if you have libXv installed])
        ], [AC_FIND_LIBXV])
    fi
    AM_CONDITIONAL(HAVE_XV, test "x$ac_have_xv" = "xyes")


    host_or_hostalias="$host"
    if test "$host_or_hostalias" = ""; then
        dnl user has called ./configure with a host parameter unknown to
        dnl config.sub; the canonical "$host" is empty
        dnl
        dnl Try the following switch with user's original host_alias 
        dnl input instead.
        dnl
        host_or_hostalias="$host_alias"
    fi

    case "$host_or_hostalias" in
        hppa*)
            if test "x$ac_have_xv_static" = "xyes"; then
                echo "warning: hppa linker - disabling static libXv"
                XV_LIBS="libXv.so"
            fi
            ;;

        ppc-*-linux* | powerpc-*)
            ppc_arch="yes"
            ;;

        *)
            ;;
    esac
    AM_CONDITIONAL(PPC_ARCH, test "x$ppc_arch" = "xyes")

    dnl
    dnl Check if we can enable the xxmc plugin.
    dnl

    AC_ARG_ENABLE([xvmc],
      AS_HELP_STRING([--disable-xvmc], [Disable XxMC and XvMC outplut plugins]) )

    if test "x$no_x" = "x" && test "x$enable_xvmc" != "xno"; then

        AC_ARG_WITH(xxmc-path, AS_HELP_STRING([--with-xxmc-path=path], [where libXvMC libraries for the
                xxmc plugin are  installed. Defalts to the default X library path.]),
                xxmc_path="$withval", xxmc_path="$x_libraries")
        AC_ARG_WITH(xxmc-lib, AS_HELP_STRING([--with-xxmc-lib=XXXX], [The name of the XvMC library 
                libXXXX.so for the xxmc plugin.]),xxmc_stub="$withval", 
                xxmc_stub="XvMCW")

        saved_libs="$LIBS"
        saved_CPPFLAGS="$CPPFLAGS"
        if test "x$x_includes" != "x"; then
            CPPFLAGS="$CPPFLAGS -I$x_includes"
        fi

        XXMC_LIBS="-L$xxmc_path -l$xxmc_stub"
        AC_MSG_CHECKING(whether to enable the xxmc plugin with vld extensions)
        AC_MSG_RESULT()
        dnl Check if vld "extended" XvMC is available
        if test "x$xxmc_stub" == "xXvMCW" && test "x$ac_have_xv" == "xyes"; then
            AC_CHECK_LIB($xxmc_stub, XvMCPutSlice,
                         ac_have_xxmc="yes",
                         [ac_have_xxmc="no"
                            AC_MSG_RESULT([*** Could not link with -l$xxmc_stub for vld extensions.])],
                         [-L$xxmc_path $X_LIBS $X_PRE_LIBS $XV_LIBS -lXext $X_EXTRA_LIBS])
        else
            if test "x$ac_have_xv" = "xyes"; then 
                AC_CHECK_LIB($xxmc_stub, XvMCPutSlice,
                             [ac_have_xxmc="yes"
                               XXMC_LIBS="$XXMC_LIBS -lXvMC"],
                             [ac_have_xxmc="no"
                               AC_MSG_RESULT([*** Could not link with -l$xxmc_stub -lXvMC for vld extensions.])],
                             [-L$xxmc_path -lXvMC $X_LIBS $X_PRE_LIBS $XV_LIBS -lXext $X_EXTRA_LIBS])
            else 
                ac_have_xxmc="no",
            fi  
        fi

        if test "x$ac_have_xxmc" = "xyes"; then
            AC_CHECK_HEADERS(X11/extensions/vldXvMC.h,
                [ac_have_vldxvmc_h="yes"],
                ac_have_vldxvmc="no",)
            if test "x$ac_have_vldxvmc_h" = "xyes"; then
                AC_DEFINE([HAVE_VLDXVMC], [1], 
                       [Define 1 if you have vldXvMC.h])
            fi
        fi
        dnl Try fallback to standard XvMC if vld failed
        if test "x$ac_have_xxmc" = "xno"; then
            if test "x$xxmc_stub" == "xXvMCW"; then
                  AC_CHECK_LIB($xxmc_stub, XvMCCreateContext,
                       ac_have_xxmc="yes",
                       [ac_have_xxmc="no"
                        AC_MSG_RESULT([*** Could not link with -l$xxmc_stub for standard XvMC.])],
                       [-L$xxmc_path $X_LIBS $X_PRE_LIBS $XV_LIBS -lXext $X_EXTRA_LIBS])
            else
                if test "x$ac_have_xv" = "xyes"; then 
                    AC_CHECK_LIB($xxmc_stub, XvMCCreateContext,
                                 [ac_have_xxmc="yes"
                                 XXMC_LIBS="$XXMC_LIBS -lXvMC"],
                                 [ac_have_xxmc="no"
                                 AC_MSG_RESULT([*** Could not link with -lXvMC for standard XvMC.])],
                                 [-L$xxmc_path -lXvMC $X_LIBS $X_PRE_LIBS $XV_LIBS -lXext $X_EXTRA_LIBS])
                else 
                    ac_have_xxmc="no",
                fi  
            fi
        fi
        if test "x$ac_have_xxmc" = "xyes"; then
            AC_CHECK_HEADERS(X11/extensions/XvMC.h,,
                 ac_have_xxmc="no",)
        fi
        if test "x$ac_have_xxmc" = "xyes"; then
            AC_DEFINE(HAVE_XXMC,1,[Define this to compile the xxmc plugin.])
            if test "x$ac_have_vldxvmc_h" = "xyes"; then
                AC_MSG_RESULT([*** Enabling xxmc plugin with vld extensions.])
            else
                AC_MSG_RESULT([*** Enabling xxmc plugin for standard XvMC *only*.])
            fi
        else
            AC_MSG_RESULT([*** Disabling xxmc plugin due to above errors.])
        fi
        LIBS="$saved_libs"
    fi
    AM_CONDITIONAL(HAVE_VLDXVMC, test "x$ac_have_vldxvmc_h" = "xyes")
    AM_CONDITIONAL(HAVE_XXMC, test "x$ac_have_xxmc" = "xyes")
    AC_SUBST(XXMC_LIBS)
	   
    dnl
    dnl Check if we can enable the xvmc plugin.
    dnl
    if test "x$no_x" = "x" && test "x$enable_xvmc" != "xno"; then

        AC_ARG_WITH(xvmc-path, AS_HELP_STRING([--with-xvmc-path=path], [where libXvMC libraries for the
                xvmc plugin are  installed. Defalts to the default X library path.]),
                xvmc_path="$withval", xvmc_path="$x_libraries")
        AC_ARG_WITH(xvmc-lib, AS_HELP_STRING([--with-xvmc-lib=XXXX], [The name of the XvMC library 
                libXXXX.so for the xvmc plugin.]),xvmc_stub="$withval", 
                xvmc_stub="XvMCW")
        saved_libs="$LIBS"
        XVMC_LIBS="-L$xvmc_path -l$xvmc_stub"
        AC_MSG_CHECKING(whether to enable the xvmc plugin)
        AC_MSG_RESULT()
        if test "x$xvmc_stub" == "xXvMCW"; then
            AC_CHECK_LIB($xvmc_stub, XvMCCreateContext,
                     ac_have_xvmc="yes",
                     [ac_have_xvmc="no"
                      AC_MSG_RESULT([*** Could not link with -l$xvmc_stub.])],
                     [-L$xvmc_path $X_LIBS $X_PRE_LIBS $XV_LIBS -lXext $X_EXTRA_LIBS])
        else
            if test "x$ac_have_xv" = "xyes"; then 
                AC_CHECK_LIB($xvmc_stub, XvMCCreateContext,
                           [ac_have_xvmc="yes"
                            XVMC_LIBS="$XVMC_LIBS -lXvMC"],
                           [ac_have_xvmc="no"
                            AC_MSG_RESULT([*** Could not link with -lXvMC.])],
                           [-L$xvmc_path -lXvMC $X_LIBS $X_PRE_LIBS $XV_LIBS -lXext $X_EXTRA_LIBS])
            else 
                ac_have_xvmc="no",
            fi  
        fi
        if test "x$ac_have_xvmc" = "xyes"; then
            AC_CHECK_HEADERS(X11/extensions/XvMC.h,,
                ac_have_xvmc="no",)
        fi
        if test "x$ac_have_xvmc" = "xyes"; then
            AC_DEFINE(HAVE_XVMC,1,[Define this if you have an XvMC library and XvMC.h installed.])
            AC_MSG_RESULT([*** Enabling old xvmc plugin.])
        else
            AC_MSG_RESULT([*** Disabling old xvmc plugin due to above errors.])
        fi
        CPPFLAGS="$saved_CPPFLAGS"
        LIBS="$saved_libs"
    fi
    AM_CONDITIONAL(HAVE_XVMC, test "x$ac_have_xvmc" = "xyes")
    AC_SUBST(XVMC_LIBS)
])
