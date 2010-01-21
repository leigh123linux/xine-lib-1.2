dnl AC_C_ALWAYS_INLINE
dnl Define inline to something appropriate, including the new always_inline
dnl attribute from gcc 3.1
dnl Thanks to Michel LESPINASSE <walken@zoy.org>
dnl __inline__ "check" added by Darren Salt
AC_DEFUN([AC_C_ALWAYS_INLINE],
    [AC_C_INLINE
    if test x"$GCC" = x"yes" -a x"$ac_cv_c_inline" = x"inline"; then
        AC_MSG_CHECKING([for always_inline])
        SAVE_CFLAGS="$CFLAGS"
        CFLAGS="$CFLAGS -Wall -Werror"
        AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[inline __attribute__ ((__always_inline__)) void f (void);]])],
            [ac_cv_always_inline=yes],[ac_cv_always_inline=no])
        CFLAGS="$SAVE_CFLAGS"
        AC_MSG_RESULT([$ac_cv_always_inline])
        if test x"$ac_cv_always_inline" = x"yes"; then
            AH_TOP([
#ifdef inline
/* the strange formatting below is needed to prevent config.status from rewriting it */
#  undef \
     inline
#endif
            ])
            AC_DEFINE_UNQUOTED([inline],[inline __attribute__ ((__always_inline__))])
        fi
	ac_cv_c___inline__=''
    else
	# FIXME: test the compiler to see if it supports __inline__
	#	 instead of assuming that if it isn't gcc, it doesn't
	case "$ac_cv_c_inline" in
	    yes)
		ac_cv_c___inline__=inline
		;;
	    inline|__inline__)
		ac_cv_c___inline__=''
		;;
	    *)
		ac_cv_c___inline__="$ac_cv_c_inline"
		;;
	esac
    fi
    if test x"$ac_cv_c___inline__" != x; then
	AC_DEFINE_UNQUOTED([__inline__],[$ac_cv_c___inline__],[Define if the compiler doesn't recognise __inline__])
    fi
])

dnl
dnl Check for minimum version of libtool
dnl AC_PREREQ_LIBTOOL([MINIMUM VERSION],[ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND ]])
AC_DEFUN([AC_PREREQ_LIBTOOL],
  [
    lt_min_full=ifelse([$1], ,1.3.5,$1)
    lt_min=`echo $lt_min_full | sed -e 's/\.//g'`
    AC_MSG_CHECKING(for libtool >= $lt_min_full)
    lt_version="`grep '^VERSION' $srcdir/ltmain.sh | sed -e 's/VERSION\=//g;s/[[-.a-zA-Z]]//g'`"

    if test $lt_version -lt 100 ; then
      lt_version=`expr $lt_version \* 10`
    fi

    if test $lt_version -lt $lt_min ; then
      AC_MSG_RESULT(no)
      ifelse([$3], , :, [$3])
    fi
    AC_MSG_RESULT(yes)
    ifelse([$2], , :, [$2])
  ])

dnl
AC_DEFUN([AC_CHECK_LIRC],
  [AC_ARG_ENABLE(lirc,
     AS_HELP_STRING([--disable-lirc], [turn off LIRC support]),
     enable_lirc=$enableval, enable_lirc=yes)

  if test x"$enable_lirc" = xyes; then
     have_lirc=yes
     AC_REQUIRE_CPP
     AC_CHECK_LIB(lirc_client,lirc_init,
           AC_CHECK_HEADER(lirc/lirc_client.h, true, have_lirc=no), have_lirc=no)
     if test "$have_lirc" = "yes"; then

        if test x"$LIRC_PREFIX" != "x"; then
           lirc_libprefix="$LIRC_PREFIX/lib"
	   LIRC_INCLUDE="-I$LIRC_PREFIX/include"
        fi
        for llirc in $lirc_libprefix /lib /usr/lib /usr/local/lib; do
          AC_CHECK_FILE("$llirc/liblirc_client.a",
             LIRC_LIBS="$llirc/liblirc_client.a"
             AC_DEFINE(HAVE_LIRC),,)
        done
     else
         AC_MSG_RESULT([*** LIRC client support not available, LIRC support will be disabled ***]);
     fi
  fi

     AC_SUBST(LIRC_LIBS)
     AC_SUBST(LIRC_INCLUDE)
])


dnl AC_LINUX_PATH(DEFAULT PATH)
AC_DEFUN([AC_LINUX_PATH],
  [AC_ARG_WITH(linux-path,
    AS_HELP_STRING([--with-linux-path=PATH], [where the linux sources are located]),
            linux_path="$withval", linux_path="$1")
  LINUX_INCLUDE="-I$linux_path/include"
])

dnl AC_CHECK_DXR3()
AC_DEFUN([AC_CHECK_DXR3],
[
  AC_ARG_ENABLE(dxr3,
    AS_HELP_STRING([--disable-dxr3], [do not build the DXR3/HW+ plugins]),
    enable_dxr3=$enableval, enable_dxr3=yes)
  if test x"$enable_dxr3" = xyes; then
    have_dxr3=yes
    AC_MSG_RESULT([*** checking for a supported mpeg encoder])
    have_encoder=no
    have_libfame=yes
    AC_CHECK_LIB(fame, fame_open,
      [AC_CHECK_HEADER(fame.h, true, have_libfame=no)], have_libfame=no)
    if test "$have_libfame" = "yes"; then
      AC_DEFINE(HAVE_LIBFAME)
      have_encoder=yes
    fi
    have_librte=yes
    AC_CHECK_LIB(rte, rte_init,
      [AC_CHECK_HEADER(rte.h, true, have_librte=no)], have_librte=no)
    if test "$have_librte" = "yes"; then
      AC_DEFINE(HAVE_LIBRTE)
      AC_MSG_WARN([this will probably only work with rte version 0.4!])
      have_encoder=yes
    fi
    if test "$have_encoder" = "yes"; then
      AC_MSG_RESULT([*** found one or more external mpeg encoders]);
    else
      AC_MSG_RESULT([*** no external mpeg encoder found]);
    fi
  else
    AC_MSG_RESULT([DXR3 plugins will not be built.])
    have_dxr3=no
  fi
])

dnl AC_CHECK_GENERATE_INTTYPES_H (INCLUDE-DIRECTORY)
dnl generate a default inttypes.h if the header file does not exist already
AC_DEFUN([AC_CHECK_GENERATE_INTTYPES],
    [AC_CHECK_HEADER([inttypes.h],,
        [if test ! -d $1; then mkdir $1; fi
        AC_CHECK_HEADER([stdint.h],
            [cat >$1/inttypes.h << EOF
#ifndef _INTTYPES_H
#define _INTTYPES_H
/* helper inttypes.h for people who do not have it on their system */

#include <stdint.h>
EOF
            ],
            [AC_COMPILE_CHECK_SIZEOF([char],[1])
            AC_COMPILE_CHECK_SIZEOF([short],[2])
            AC_COMPILE_CHECK_SIZEOF([int],[4])
            AC_COMPILE_CHECK_SIZEOF([long long],[8])
        cat >$1/inttypes.h << EOF
#ifndef _INTTYPES_H
#define _INTTYPES_H
/* default inttypes.h for people who do not have it on their system */
#if (!defined __int8_t_defined) && (!defined __BIT_TYPES_DEFINED__)
#define __int8_t_defined
typedef signed char int8_t;
typedef signed short int16_t;
typedef signed int int32_t;
#ifdef ARCH_X86
typedef signed long long int64_t;
#endif
#endif
#if (!defined __uint8_t_defined) && (!defined _LINUX_TYPES_H)
#define __uint8_t_defined
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
#ifdef ARCH_X86
typedef unsigned long long uint64_t;
#endif
#endif
EOF
            ])
        cat >>$1/inttypes.h << EOF

#ifdef WIN32
#  define PRI64_PREFIX "I64"
#else
#  define PRI64_PREFIX "l"
#endif

#ifndef PRId8
#  define PRId8 "d"
#endif
#ifndef PRId16
#  define PRId16 "d"
#endif
#ifndef PRId32
#  define PRId32 "d"
#endif
#ifndef PRId64
#  define PRId64 PRI64_PREFIX "d"
#endif

#ifndef PRIu8
#  define PRIu8 "u"
#endif
#ifndef PRIu16
#  define PRIu16 "u"
#endif
#ifndef PRIu32
#  define PRIu32 "u"
#endif
#ifndef PRIu64
#  define PRIu64 PRI64_PREFIX "u"
#endif

#ifndef PRIx8
#  define PRIx8 "x"
#endif
#ifndef PRIx16
#  define PRIx16 "x"
#endif
#ifndef PRIx32
#  define PRIx32 "x"
#endif
#ifndef PRIx64
#  define PRIx64 PRI64_PREFIX "x"
#endif

#ifndef PRIX8
#  define PRIX8 "X"
#endif
#ifndef PRIX16
#  define PRIX16 "X"
#endif
#ifndef PRIX32
#  define PRIX32 "X"
#endif
#ifndef PRIX64
#  define PRIX64 PRI64_PREFIX "X"
#endif

#ifndef PRIdFAST8
#  define PRIdFAST8 "d"
#endif
#ifndef PRIdFAST16
#  define PRIdFAST16 "d"
#endif
#ifndef PRIdFAST32
#  define PRIdFAST32 "d"
#endif
#ifndef PRIdFAST64
#  define PRIdFAST64 "d"
#endif

#ifndef PRIuFAST8
#  define PRIuFAST8 "u"
#endif
#ifndef PRIuFAST16
#  define PRIuFAST16 "u"
#endif
#ifndef PRIuFAST32
#  define PRIuFAST32 "u"
#endif
#ifndef PRIuFAST64
#  define PRIuFAST64 PRI64_PREFIX "u"
#endif

#ifndef PRIxFAST8
#  define PRIxFAST8 "x"
#endif
#ifndef PRIxFAST16
#  define PRIxFAST16 "x"
#endif
#ifndef PRIxFAST32
#  define PRIxFAST32 "x"
#endif
#ifndef PRIxFAST64
#  define PRIxFAST64 PRI64_PREFIX "x"
#endif

#ifndef SCNd8
#  define SCNd8 "hhd"
#endif
#ifndef SCNd16
#  define SCNd16 "hd"
#endif
#ifndef SCNd32
#  define SCNd32 "d"
#endif
#ifndef SCNd64
#  define SCNd64 PRI64_PREFIX "d"
#endif

#ifndef SCNu8
#  define SCNu8 "hhu"
#endif
#ifndef SCNu16
#  define SCNu16 "hu"
#endif
#ifndef SCNu32
#  define SCNu32 "u"
#endif
#ifndef SCNu64
#  define SCNu64 PRI64_PREFIX "u"
#endif

#ifndef PRIdMAX
#  define PRIdMAX PRId64
#endif
#ifndef PRIuMAX
#  define PRIuMAX PRIu64
#endif
#ifndef PRIxMAX
#  define PRIxMAX PRIx64
#endif
#ifndef SCNdMAX
#  define SCNdMAX SCNd64
#endif

#endif
EOF
        ])]
)


dnl AC_COMPILE_CHECK_SIZEOF (TYPE SUPPOSED-SIZE)
dnl abort if the given type does not have the supposed size
AC_DEFUN([AC_COMPILE_CHECK_SIZEOF],
    [AC_MSG_CHECKING(that size of $1 is $2)
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[switch (0) case 0: case (sizeof ($1) == $2):;]])],[],
        [AC_MSG_ERROR([can not build a default inttypes.h])])
    AC_MSG_RESULT([yes])])


dnl AM_CHECK_CDROM_IOCTLS ([ACTION-IF-YES], [ACTION-IF-NO])
dnl check for CDROM_DRIVE_STATUS in ioctl.h
AC_DEFUN([AM_CHECK_CDROM_IOCTLS],
         [AC_CACHE_CHECK([if cdrom ioctls are available],
	     [am_cv_have_cdrom_ioctls],
             [AC_EGREP_HEADER([CDROM_DRIVE_STATUS],[sys/ioctl.h],
	        am_cv_have_cdrom_ioctls=yes,
                [AC_EGREP_HEADER([CDIOCALLOW],[sys/ioctl.h],
		   am_cv_have_cdrom_ioctls=yes,
                   [AC_EGREP_CPP(we_have_cdrom_ioctls,[
#include <sys/ioctl.h>
#ifdef HAVE_SYS_CDIO_H
#  include <sys/cdio.h>
#endif
#ifdef HAVE_LINUX_CDROM_H
#  include <linux/cdrom.h>
#endif
#if defined(CDROM_DRIVE_STATUS) || defined(CDIOCALLOW) || defined(CDROMCDXA)
  we_have_cdrom_ioctls
#endif
],
                   am_cv_have_cdrom_ioctls=yes,
		   am_cv_have_cdrom_ioctls=no
          )])])])
          have_cdrom_ioctls=$am_cv_have_cdrom_ioctls
          if test "x$have_cdrom_ioctls" = xyes ; then
            ifelse([$1], , :, [$1])
          else
            ifelse([$2], , :, [$2])
          fi
])


dnl AC_CHECK_IP_MREQN
dnl check for struct ip_mreqn in netinet/in.h
AC_DEFUN([AC_CHECK_IP_MREQN],
         [AC_CACHE_CHECK([for ip_mreqn], [ac_cv_have_ip_mreqn],
		[AC_EGREP_HEADER([ip_mreqn],
			[netinet/in.h],
			[ac_cv_have_ip_mreqn=yes],
			[ac_cv_have_ip_mreqn=no])])
	  if test $ac_cv_have_ip_mreqn = yes; then
             AC_DEFINE([HAVE_IP_MREQN],1,[Define this if you have ip_mreqn in netinet/in.h])
	  fi
])


dnl AC_PROG_GMSGFMT_PLURAL
dnl ----------------------
dnl Validate the GMSGFMT program found by gettext.m4; reject old versions
dnl of GNU msgfmt that do not support the "msgid_plural" extension.
AC_DEFUN([AC_PROG_GMSGFMT_PLURAL],
 [dnl AC_REQUIRE(AM_GNU_GETTEXT)

  if test "$GMSGFMT" != ":"; then
    AC_MSG_CHECKING([for plural forms in GNU msgfmt])

    changequote(,)dnl We use [ and ] in in .po test input

    dnl If the GNU msgfmt does not accept msgid_plural we define it
    dnl as : so that the Makefiles still can work.
    cat >conftest.po <<_ACEOF
msgid "channel"
msgid_plural "channels"
msgstr[0] "canal"
msgstr[1] "canal"

_ACEOF
    changequote([,])dnl

    if $GMSGFMT -o /dev/null conftest.po >/dev/null 2>&1; then
      AC_MSG_RESULT(yes)
    else
      AC_MSG_RESULT(no)
      AC_MSG_RESULT(
	[found GNU msgfmt program is too old, it does not support plural forms; ignore it])
      GMSGFMT=":"
    fi
    rm -f conftest.po
  fi
])dnl AC_PROG_GMSGFMT_PLURAL


# AC_PROG_LIBTOOL_SANITYCHECK
# ----------------------
# Default configuration of libtool on solaris produces non-working
# plugin modules, when gcc is used as compiler, and gcc does not
# use gnu-ld
AC_DEFUN([AC_PROG_LIBTOOL_SANITYCHECK],
 [dnl AC_REQUIRE(AC_PROG_CC)
  dnl AC_REQUIRE(AC_PROG_LD)
  dnl AC_REQUIRE(AC_PROG_LIBTOOL)

  case $host in
  *-*-solaris*)
    if test "$GCC" = yes && test "$with_gnu_ld" != yes; then
      AC_MSG_CHECKING([if libtool can build working modules])
      cat > conftest1.c <<_ACEOF
#undef NDEBUG
#include <assert.h>
int shlib_func(long long a, long long b) {
  assert(b);
  switch (a&3) {
  case 0: return a/b;
  case 1: return a%b;
  case 2: return (unsigned long long)a/b;
  case 3: return (unsigned long long)a%b;
  }
}
_ACEOF

      cat > conftest2.c <<_ACEOF
#include <dlfcn.h>
int main(){
  void *dl = dlopen(".libs/libconftest.so", RTLD_NOW);
  if (!dl) printf("%s\n", dlerror());
  exit(dl ? 0 : 1);
}
_ACEOF

      if ./libtool $CC -c conftest1.c >/dev/null 2>&1 && \
         ./libtool $CC -o libconftest.la conftest1.lo \
		 -module -avoid-version -rpath /tmp  >/dev/null 2>&1 && \
         ./libtool $CC -o conftest2 conftest2.c -ldl >/dev/null 2>&1
      then
        if ./conftest2 >/dev/null 2>&1; then
          AC_MSG_RESULT(yes)
        else
	  dnl typical problem: dlopen'ed module not self contained, because
	  dnl it wasn't linked with -lgcc
	  AC_MSG_RESULT(no)
	  if grep '^archive_cmds=.*$LD -G' libtool >/dev/null; then
            AC_MSG_CHECKING([if libtool can be fixed])

	    dnl first try to update gcc2's spec file to add the
	    dnl gcc3 -mimpure-text flag

	    libtool_specs=""

	    if $CC -dumpspecs | grep -- '-G -dy -z text' >/dev/null; then
	      $CC -dumpspecs | \
		  sed 's/-G -dy -z text/-G -dy %{!mimpure-text:-z text}/g' \
		  > gcc-libtool-specs
	      libtool_specs=" -specs=`pwd`/gcc-libtool-specs"
	    fi

	    sed -e "s,\$LD -G,\$CC${libtool_specs} -shared -mimpure-text,g" \
		-e 's/ -M / -Wl,-M,/' libtool >libtool-fixed
	    chmod +x libtool-fixed
            if ./libtool-fixed $CC -o libconftest.la conftest1.lo \
		    -module -avoid-version -rpath /tmp  >/dev/null 2>&1 && \
	       ./conftest2 >/dev/null 2>&1; then

	      dnl the fixed libtool works
	      AC_MSG_RESULT(yes)
	      mv -f libtool-fixed libtool

            else
	      AC_MSG_RESULT(no)
            fi
	  fi
        fi
      else
        AC_MSG_RESULT(no)
      fi
      rm -f conftest1.c conftest1.lo conftest1.o conftest2.c \
		libconftest.la conftest libtool-fixed
      rm -rf .libs
    fi ;;
  esac
])# AC_PROG_LIBTOOL_SANITYCHECK

dnl Check for the type of the third argument of getsockname
AC_DEFUN([AC_CHECK_SOCKLEN_T], [
  AC_MSG_CHECKING(for socklen_t)

  AC_CACHE_VAL(ac_cv_socklen_t, [
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <sys/types.h>
#include <sys/socket.h>
      ]], [[socklen_t a=0;
getsockname(0,(struct sockaddr*)0, &a);
      ]])],[ac_cv_socklen_t=socklen_t],[ac_cv_socklen_t=''])
    if test "x$ac_cv_socklen_t" = x; then
      AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <sys/types.h>
#include <sys/socket.h>]], [[
int a=0;
getsockname(0,(struct sockaddr*)0, &a);]])],
      [ac_cv_socklen_t=int],[ac_cv_socklen_t=size_t])
    fi
  ])

  AC_MSG_RESULT($ac_cv_socklen_t)
  if test "$ac_cv_socklen_t" != "socklen_t"; then
    AC_DEFINE_UNQUOTED(socklen_t, $ac_cv_socklen_t,
        [Define the real type of socklen_t])
  fi
])
