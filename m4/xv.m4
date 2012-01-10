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
