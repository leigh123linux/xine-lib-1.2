dnl Detection of the Pthread implementation flags and libraries
dnl Diego Pettenò <flameeyes-aBrp7R+bbdUdnm+yROfE0A@public.gmane.org> 2006-11-03
dnl
dnl CC_PTHREAD_FLAGS([ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
dnl This macro checks for the Pthread flags to use to build
dnl with support for PTHREAD_LIBS and PTHREAD_CFLAGS variables
dnl used in FreeBSD ports.
dnl
dnl This macro is released as public domain, but please mail
dnl to flameeyes@gmail.com if you want to add support for a
dnl new case, or if you're going to use it, so that there will
dnl always be a version available.
AC_DEFUN([CC_PTHREAD_FLAGS], [
  AC_REQUIRE([CC_CHECK_WERROR])
  AC_ARG_VAR([PTHREAD_CFLAGS], [C compiler flags for Pthread support])
  AC_ARG_VAR([PTHREAD_LIBS], [linker flags for Pthread support])

  dnl if PTHREAD_* are not set, default to -pthread (GCC)
  if test "${PTHREAD_CFLAGS-unset}" = "unset"; then
     case $host in
       *-hpux11*) PTHREAD_CFLAGS=""		;;
       *-darwin*) PTHREAD_CFLAGS=""		;;
       *-solaris*)
                  # Handle Studio compiler
                  CC_CHECK_CFLAGS([-mt], [PTHREAD_CFLAGS="-mt -D_REENTRANT"], [PTHREAD_CFLAGS="-D_REENTRANT"]);;
       *)	  PTHREAD_CFLAGS="-pthread"	;;
     esac
  fi
  if test "${PTHREAD_LIBS-unset}" = "unset"; then
     case $host in
       *-hpux11*) PTHREAD_LIBS="-lpthread"	;;
       *-darwin*) PTHREAD_LIBS=""		;;
       *-solaris*)
                  # Handle Studio compiler
                  CC_CHECK_CFLAGS([-mt], [PTHREAD_LIBS="-lpthread -lposix4 -lrt"], [PTHREAD_LIBS="-lpthread -lposix4 -lrt"]);;
       *)	  PTHREAD_LIBS="-pthread"	;;
     esac
  fi

  AC_CACHE_CHECK([if $CC supports Pthread],
    AS_TR_SH([cc_cv_pthreads]),
    [ac_save_CFLAGS="$CFLAGS"
     ac_save_LIBS="$LIBS"
     CFLAGS="$CFLAGS $cc_cv_werror $PTHREAD_CFLAGS"
     
     LIBS="$LIBS $PTHREAD_LIBS"
     AC_LINK_IFELSE(
       [AC_LANG_PROGRAM(
          [[#include <pthread.h>
	    void *fakethread(void *arg) { return NULL; }
	    pthread_t fakevariable;
	  ]],
          [[pthread_create(&fakevariable, NULL, &fakethread, NULL);]]
        )],
       [cc_cv_pthreads=yes],
       [cc_cv_pthreads=no])
     CFLAGS="$ac_save_CFLAGS"
     LIBS="$ac_save_LIBS"
    ])

  AC_SUBST([PTHREAD_LIBS])
  AC_SUBST([PTHREAD_CFLAGS])

  if test x$cc_cv_pthreads = xyes; then
    ifelse([$1], , [:], [$1])
  else
    ifelse([$2], , [:], [$2])
  fi
])

AC_DEFUN([CC_PTHREAD_RECURSIVE_MUTEX], [
    AC_REQUIRE([CC_PTHREAD_FLAGS])
    AC_MSG_CHECKING([for recursive mutex support in pthread])

    ac_save_LIBS="$LIBS"
    LIBS="$LIBS $PTHREAD_LIBS"
    AC_COMPILE_IFELSE(AC_LANG_SOURCE([#include <pthread.h>

int main() {
    pthread_mutexattr_t attr;
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    return 0;
}
        ]), [have_recursive_mutex=yes], [have_recursive_mutex=no])
    LIBS="$ac_save_LIBS"

    AC_MSG_RESULT([$have_recursive_mutex])

    if test x"$have_recursive_mutex" = x"yes"; then
        ifelse([$1], , [:], [$1])
    else
        ifelse([$2], , [:], [$2])
    fi
])
