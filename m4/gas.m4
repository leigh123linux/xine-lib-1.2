dnl Copyright 2007 xine project
dnl Copyright 1996, 1997, 1998, 1999, 2000, 2001, 2003, 2004, 2005, 2006, 2007
dnl Free Software Foundation, Inc.
dnl Originally by Gordon Matzigkeit <gord@gnu.ai.mit.edu>, 1996
dnl
dnl This file is free software; the Free Software Foundation gives
dnl unlimited permission to copy and/or distribute it, with or without
dnl modifications, as long as this notice is preserved.

dnl AC_PROG_AS
dnl ----------
dnl find the pathname to the GNU or non-GNU assembler
AC_DEFUN([CC_PROG_AS],
[AC_ARG_WITH([gnu-as],
    [AS_HELP_STRING([--with-gnu-as],
	[assume the C compiler uses GNU as @<:@default=no@:>@])],
    [test "$withval" = no || with_gnu_as=yes],
    [with_gnu_as=no])
AC_REQUIRE([LT_AC_PROG_SED])dnl
AC_REQUIRE([AC_PROG_CC])dnl
AC_REQUIRE([AC_CANONICAL_HOST])dnl
AC_REQUIRE([AC_CANONICAL_BUILD])dnl
cc_prog=as
if test "$GCC" = yes; then
  # Check if gcc -print-prog-name=as gives a path.
  AC_MSG_CHECKING([for as used by $CC])
  case $host in
  *-*-mingw*)
    # gcc leaves a trailing carriage return which upsets mingw
    ac_prog=`($CC -print-prog-name=as) 2>&5 | tr -d '\015'` ;;
  *)
    ac_prog=`($CC -print-prog-name=as) 2>&5` ;;
  esac
  case $ac_prog in
    # Accept absolute paths.
    [[\\/]]* | ?:[[\\/]]*)
      re_direlt='/[[^/]][[^/]]*/\.\./'
      # Canonicalize the pathname of as
      ac_prog=`echo $ac_prog| $SED 's%\\\\%/%g'`
      while echo $ac_prog | grep "$re_direlt" > /dev/null 2>&1; do
	ac_prog=`echo $ac_prog| $SED "s%$re_direlt%/%"`
      done
      test -z "$AS" && AS="$ac_prog"
      ;;
  "")
    # If it fails, then pretend we aren't using GCC.
    ac_prog=as
    ;;
  *)
    # If it is relative, then search for the first as in PATH.
    with_gnu_as=unknown
    ;;
  esac
elif test "$with_gnu_as" = yes; then
  AC_MSG_CHECKING([for GNU as])
else
  AC_MSG_CHECKING([for non-GNU as])
fi
AC_CACHE_VAL(cc_cv_path_AS,
[if test -z "$AS"; then
  lt_save_ifs="$IFS"; IFS=$PATH_SEPARATOR
  for ac_dir in $PATH; do
    IFS="$lt_save_ifs"
    test -z "$ac_dir" && ac_dir=.
    if test -f "$ac_dir/$ac_prog" || test -f "$ac_dir/$ac_prog$ac_exeext"; then
      lt_cv_path_AS="$ac_dir/$ac_prog"
      # Check to see if the program is GNU as.  I'd rather use --version,
      # but apparently some variants of GNU as only accept -v.
      # Break only if it was the GNU/non-GNU as that we prefer.
      case `"$cc_cv_path_AS" -v 2>&1 </dev/null` in
      dnl Apple's assembler reports itself as GNU as 1.38;
      dnl but it doesn't provide the functions we need.
      *Apple*)
        test "$with_gnu_as" != yes && break
	;;
      *GNU* | *'with BFD'*)
	test "$with_gnu_as" != no && break
	;;
      *)
	test "$with_gnu_as" != yes && break
	;;
      esac
    fi
  done
  IFS="$lt_save_ifs"
else
  cc_cv_path_AS="$AS" # Let the user override the test with a path.
fi])
AS="$cc_cv_path_AS"
if test -n "$AS"; then
  AC_MSG_RESULT($AS)
else
  AC_MSG_RESULT(no)
fi
test -z "$AS" && AC_MSG_ERROR([no acceptable as found in \$PATH])
CC_PROG_AS_GNU
])


dnl AC_PROG_AS_GNU
dnl --------------
AC_DEFUN([CC_PROG_AS_GNU],
[AC_REQUIRE([AC_PROG_EGREP])dnl
AC_CACHE_CHECK([if the assembler ($AS) is GNU as], cc_cv_prog_gnu_as,
[# I'd rather use --version here, but apparently some GNU as's only accept -v.
case `$AS -v 2>&1 </dev/null` in
dnl Apple's assembler reports itself as GNU as 1.38;
dnl but it doesn't provide the functions we need.
*Apple*)
  cc_cv_prog_gnu_as=no
  ;;
*GNU* | *'with BFD'*)
  cc_cv_prog_gnu_as=yes
  ;;
*)
  cc_cv_prog_gnu_as=no
  ;;
esac])
with_gnu_as=$cc_cv_prog_gnu_as
])
