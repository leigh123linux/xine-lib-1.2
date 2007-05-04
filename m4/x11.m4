dnl ---------------------------------------------
dnl Checks for X11
dnl ---------------------------------------------

AC_DEFUN([XINE_X11_SUPPORT], [
    if test "x$with_x" != "xno"; then
        PKG_CHECK_MODULES([X], [x11 xext], , [
            AC_PATH_XTRA
   
            dnl Set xv_path if its not done already
            dnl we do it here before rewriting X_LIBS
            if test x$xv_path = x; then
     	        xv_path=`echo $X_LIBS | sed -e 's/\-L\(.*\)/\1/'`
            fi
     
            dnl ----------------------------------------------
            dnl Check for XShm support (required with X)
            dnl ----------------------------------------------

            if test "x$no_x" != "xyes"; then
                ac_save_CPPFLAGS="$CPPFLAGS"
                CPPFLAGS="$CPPFLAGS $X_CFLAGS"
                AC_CHECK_HEADERS([X11/extensions/XShm.h], [true],
  		   [AC_MSG_ERROR([XShm extension is required])])
	        AC_CHECK_LIB([Xext], [main], [true],
	           [AC_MSG_ERROR([libXext is required])], [$X_LIBS])
                CPPFLAGS="$ac_save_CPPFLAGS"
	        X_LIBS="$X_LIBS $X_PRE_LIBS -lX11 -lXext"
            fi
        ])
    else
        no_x="yes"
    fi

    if test "x$no_x" != "xyes"; then
        AC_DEFINE(HAVE_X11,1,[Define this if you have X11R6 installed])
    fi
    AM_CONDITIONAL(HAVE_X11, [test "x$no_x" != "xyes"])


    dnl ---------------------------------------------
    dnl Locate libraries needed for X health check
    dnl ---------------------------------------------

soname_script="/[[0-9]]$/! d; s%^.*/%%
t q
b
:q
q"
    x_lib_location="`ls -1 "${x_libraries:-/usr/local/lib}/libX11.so"* "${x_libraries:-/usr/lib}/libX11.so"* 2>/dev/null | sed -e \"${soname_script}\"`"
    AC_DEFINE_UNQUOTED([LIBX11_SO], "${x_lib_location:-libX11.so}", [The soname of libX11, needed for dlopen()])
    x_lib_location="`ls -1 "${x_libraries:-/usr/local/lib}/libXv.so"*  "${x_libraries:-/usr/lib}/libXv.so"*  2>/dev/null | sed -e \"${soname_script}\"`"
    AC_DEFINE_UNQUOTED([LIBXV_SO],  "${x_lib_location:-libXv.so}",  [The soname of libXv, needed for dlopen()])
])
