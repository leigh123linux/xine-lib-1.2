AC_DEFUN([XINE_XV_SUPPORT], [
    dnl With recent XFree86 or Xorg, dynamic linking is preferred!
    dnl Only dynamic linking is possible when using libtool < 1.4.0
    AC_ARG_WITH([xv-path],
                [AS_HELP_STRING([--with-xv-path=path], [where libXv is installed])])

    AC_ARG_ENABLE([static-xv],
                  [AS_HELP_STRING([--enable-static-xv], [Enable this to force linking against libXv.a])],
                  [test x"$enableval" != x"no" && xv_prefer_static="yes"], [xv_prefer_static="no"])
    case "$host_or_hostalias" in
        hppa*) xv_libexts="$acl_cv_shlibext" ;;
        *)
            if test x"$xv_prefer_static" = x"yes"; then  
                xv_libexts="$acl_cv_libext $acl_cv_shlibext"
            else
                xv_libexts="$acl_cv_shlibext $acl_cv_libext"
            fi
            ;;
    esac

    if test x"$no_x" != x"yes"; then
        PKG_CHECK_MODULES([XV], [xv], [have_xv=yes], [have_xv=no])
        if test x"$have_xv" = x"no"; then
            dnl No Xv package -- search for it
            for xv_libext in $xv_libexts; do
                xv_lib="libXv.$xv_libext"
                AC_MSG_CHECKING([for $xv_lib])
                for xv_try_path in "$with_xv_path" "$x_libraries" /usr/X11R6/lib /usr/lib; do
                    if test x"$xv_try_path" != x"" && test -f "$xv_try_path/$xv_lib"; then
                        case $xv_lib in
                            *.$acl_cv_libext)   xv_try_libs="$xv_try_path/$xv_lib" ;;
                            *.$acl_cv_shlibext) xv_try_libs="-L$xv_try_path -lXv" ;;
                        esac
                        ac_save_LIBS="$LIBS" LIBS="$xv_try_libs $X_PRE_LIBS $X_LIBS $X_EXTRA_LIBS $LIBS"
                        AC_LINK_IFELSE([AC_LANG_PROGRAM([[]], [[XvShmCreateImage()]])], [have_xv=yes], [])
                        LIBS="$ac_save_LIBS"
                        if test x"$have_xv" = x"yes"; then
                            AC_MSG_RESULT([$xv_try_path])
                            XV_LIBS="$xv_try_libs"
                            break
                        fi
                    fi
                done
                test x"$have_xv" = x"yes" && break
                AC_MSG_RESULT([no])
            done
        fi

        if test x"$have_xv" = x"yes"; then
            AC_DEFINE([HAVE_XV], 1, [Define this if you have libXv installed])
        fi
    fi
    AM_CONDITIONAL([HAVE_XV], [test x"$have_xv" = x"yes"])


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
