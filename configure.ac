AC_PREREQ(2.59)

dnl Note that autoconf/autoheader/automake cache using autom4te, so version.sh
dnl will only be run if configure.ac has changed.  This must be done before
dnl AC_INIT so that XINE_VERSION_SPEC, which is an m4 macro, is available.
m4_esyscmd([./version.sh])

dnl Initialize autoconf, autoheader, and automake
AC_INIT([xine-lib], XINE_VERSION_SPEC, [xine-bugs@lists.sourceforge.net])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([src/xine-engine/xine.c])
AC_CONFIG_LIBOBJ_DIR([lib])
AC_CONFIG_HEADERS([config.h])
AM_MAINTAINER_MODE

AH_TOP([#ifndef __XINE_LIB_CONFIG_H__
#define __XINE_LIB_CONFIG_H__ 1
])
AH_BOTTOM([#ifdef ASMALIGN_1SLN
# define ASMALIGN(ZEROBITS) ".align " #ZEROBITS "\n\t"
#else
# define ASMALIGN(ZEROBITS) ".align 1<<" #ZEROBITS "\n\t"
#endif

/* include internal system specific header */
#include "os_internal.h"

#endif  /* __XINE_LIB_CONFIG_H__ */
])

test x"$prefix"      = x"NONE" && prefix="${ac_default_prefix}"
test x"$exec_prefix" = x"NONE" && exec_prefix='${prefix}'

dnl Use features of autoconf 2.61, but stay compatible with older versions.
if test x"$datarootdir" = x""; then
   datarootdir='${datadir}'
   AC_SUBST(datarootdir)
fi

if test x"$docdir" = x""; then
   docdir='${datarootdir}/doc/${PACKAGE}'
   AC_SUBST(docdir)
fi

if test x"$htmldir" = x""; then
   htmldir='${docdir}'
   AC_SUBST(htmldir)
fi


dnl -------------------------
dnl Setup version information
dnl -------------------------

dnl Do not change these manually; they come from running ./version.sh when
dnl autoconf runs.  This must all be done after AC_INIT is done, but running
dnl the version.sh script must be done before AC_INIT.
XINE_MAJOR=XINE_VERSION_MAJOR
AC_SUBST(XINE_MAJOR)
AC_DEFINE_UNQUOTED([XINE_MAJOR], [$XINE_MAJOR], [xine major version number])
XINE_MINOR=XINE_VERSION_MINOR
AC_SUBST(XINE_MINOR)
AC_DEFINE_UNQUOTED([XINE_MINOR], [$XINE_MINOR], [xine minor version number])
XINE_SUB=XINE_VERSION_SUB
AC_SUBST(XINE_SUB)
AC_DEFINE_UNQUOTED([XINE_SUB],   [$XINE_SUB],   [xine sub version number])

XINE_LT_CURRENT=__XINE_LT_CURRENT
AC_SUBST(XINE_LT_CURRENT)
XINE_LT_REVISION=__XINE_LT_REVISION
AC_SUBST(XINE_LT_REVISION)
XINE_LT_AGE=__XINE_LT_AGE
AC_SUBST(XINE_LT_AGE)

LIBNAME="libxine${XINE_MAJOR}"
AC_SUBST(LIBNAME)
AC_DEFINE_UNQUOTED([XINE_TEXTDOMAIN], "$LIBNAME", [catalog message text domain])

dnl Always set flags to begin with -g so that debug information will be
dnl included.  In release bulids, this can be stripped out later if desired.
dnl More importantly, it prevents autoconf from initializing the defaults to
dnl include -O2, which is not desirable in a debug build, and it messes with
dnl other optimizations that we'll want to be setting ourselves later.
ASFLAGS="-g $ASFLAGS"
CFLAGS="-g $CFLAGS"
CPPFLAGS="-D_REENTRANT -DXINE_COMPILE $CPPFLAGS"
LDFLAGS="-g $LDFLAGS"
OBJCFLAGS="-g $OBJCFLAGS"

AC_SUBST(ASFLAGS)


dnl ------------------------------
dnl Build modes: debug and profile
dnl ------------------------------

AC_ARG_ENABLE([debug],
              [AS_HELP_STRING([--enable-debug], [build with debugging code enabled])],
              [], [enable_debug="no"])
if test x"$enable_debug" != x"no"; then
    CPPFLAGS="-DDEBUG $CPPFLAGS"
else
    CPPFLAGS="-DNDEBUG $CPPFLAGS"
fi
AM_CONDITIONAL([DEBUG_BUILD], [test x"$enable_debug" != x"no"])

AC_ARG_ENABLE([profiling],
              [AS_HELP_STRING([--enable-profiling], [build with profiling code enabled])],
              [], [enable_profiling="no"])
if test x"$enable_profiling" != x"no"; then
    CFLAGS="-pg $CFLAGS"
    OBJCFLAGS="-pg $OBJCFLAGS"
    LDFLAGS="-pg $LDFLAGS"
fi
AM_CONDITIONAL([PROFILING_BUILD], [test x"$enable_profiling" != x"no"])


dnl --------------
dnl Build features
dnl --------------

AC_ARG_ENABLE([altivec],
              [AS_HELP_STRING([--disable-altivec], [do not use assembly codes for Motorola 74xx CPUs])],
              [], [enable_altivec="yes"])

AC_ARG_ENABLE([vis],
	      [AS_HELP_STRING([--disable-vis], [do not use assembly codes for Sun UltraSPARC CPUs])],
              [], [enable_vis="yes"])

AC_ARG_ENABLE([ipv6],
              [AS_HELP_STRING([--enable-ipv6], [enable use of IPv6])],
              [if test x"$enableval" != x"no"; then
                   AC_DEFINE([ENABLE_IPV6], 1, [Enable this when IPv6 is requested])
               fi])

AC_ARG_ENABLE([antialiasing],
              [AS_HELP_STRING([--enable-antialiasing], [enable font antialiasing])],
              [if test x"$enableval" != x"no"; then
                   AC_DEFINE([ENABLE_ANTIALIASING], 1, [Define this to 1 to enable font antialising.])    
               fi])


dnl ----------------------------------------------
dnl Cross compilation, Mac OS X Universal Binaries
dnl ----------------------------------------------

AC_CANONICAL_BUILD
AC_CANONICAL_HOST

dnl check for Mac OS X universal binary support early, because certain flags
dnl must be set prior to looking for cc/libtool, etc.
MACOSX_UNIVERSAL_BINARIES

dnl Check to see if $host is empty.  If it is, try $host_alias instead.
dnl If $host is empty, it's because the user has run ./configure with a host
dnl parameter unknown to config.sub.  This used to be set in xv handling, but
dnl it's also used in a bunch of other places unrelated to Xwindows support,
dnl so if X is disabled, other things could go badly.
host_or_hostalias="$host"
test x"$host_or_hostalias" = x"" && host_or_hostalias="$host_alias"


dnl -------------------
dnl checks for programs
dnl -------------------

AC_PROG_CC
AC_PROG_OBJC
AM_PROG_AS

AC_PROG_MAKE_SET
AC_PROG_EGREP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_AWK

AC_CHECK_TOOL([STRINGS], [strings], [false])

AC_GNU_SOURCE
AC_ISC_POSIX

PKG_PROG_PKG_CONFIG

dnl Check for documentation formatting tools
AC_CHECK_PROG([SGMLTOOLS], [sgmltools], [sgmltools], [no])
AM_CONDITIONAL([HAVE_SGMLTOOLS], [test x"$SGMLTOOLS" != x"no"])

AC_CHECK_PROG([FIG2DEV], [fig2dev], [fig2dev], [no])
AM_CONDITIONAL([HAVE_FIG2DEV], [test x"$FIG2DEV" != x"no"])


dnl libtool
m4_undefine([AC_PROG_F77])
m4_defun([AC_PROG_F77],[])

AC_DISABLE_STATIC
AC_LIBTOOL_DLOPEN
AC_LIBTOOL_WIN32_DLL
AC_PATH_MAGIC
AC_PROG_LIBTOOL
AC_PROG_LIBTOOL_SANITYCHECK

dnl REVISIT:
dnl Nothing seems to use LIBTOOL_DEPS
dnl src/video_out/libdha and src/video_out/vidix both use STATIC -- why?
AC_SUBST(LIBTOOL_DEPS)
if ${CONFIG_SHELL} ./libtool --features | grep "enable static" >/dev/null; then
    STATIC="-static"
else
    STATIC=
fi
AC_SUBST(STATIC)


dnl --------------------
dnl checks for libraries
dnl --------------------

AM_ICONV
AC_ARG_ENABLE([iconvtest],
              [AS_HELP_STRING([--disable-iconvtest], [don't require iconv library])])
if test x"$enable_iconvtest" != x"no"; then
    if test x"$am_cv_func_iconv" != x"yes"; then
        AC_MSG_ERROR([
****************************************************************
* iconv library not found. It's necessary for proper           *
* manipulation with texts so xine requires it as default.      *
*                                                              *
* You need to install iconv library or to specify prefix       *
* by option --with-libiconv-prefix.                            *
*                                                              *
* If you don't want iconv support use the option               *
* --disable-iconvtest.                                         *
****************************************************************
])
    fi
fi

AM_GNU_GETTEXT([external])
AC_PROG_GMSGFMT_PLURAL

AC_CHECK_LIB([c], [dlopen], [DYNAMIC_LD_LIBS=""],
             [AC_CHECK_LIB([dl], [dlopen], [DYNAMIC_LD_LIBS="-ldl"],
                           [AC_MSG_CHECKING(for dlopen under win32)
                            AC_LANG_PUSH([C])
                            ac_save_CPPFLAGS="$CPPFLAGS" CPPFLAGS="-I${srcdir}/win32/include $CPPFLAGS"
                            ac_save_LIBS="$LIBS" LIBS="$LIBS -lkernel32"
                            AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <stddef.h>
                                                                 #include <dlfcn.h>]], [[dlopen(NULL, 0)]])],
                                              [DYNAMIC_LD_LIBS="-lkernel32"
                                               AC_MSG_RESULT([yes])], [AC_MSG_RESULT([no])])
                            CPPFLAGS="$ac_save_CPPFLAGS" LIBS="$ac_save_LIBS"
                            AC_LANG_POP([C])], [AC_MSG_ERROR([dynamic linker needed])])])
AC_SUBST([DYNAMIC_LD_LIBS])

AC_ARG_WITH([external-libxdg-basedir],
            [AS_HELP_STRING([--with-external-libxdg-basedir], [use external copy of libxdg-basedir])])

if test x"$with_external_libxdg_basedir" = x"yes"; then
    PKG_CHECK_MODULES([XDG_BASEDIR], [libxdg-basedir >= 0.1.3])
else
    XDG_BASEDIR_CPPFLAGS='-I$(top_srcdir)/contrib/libxdg-basedir'
    XDG_BASEDIR_LIBS='$(top_builddir)/contrib/libxdg-basedir/libxdg-basedir.la'
    XDG_BASEDIR_DEPS='$(top_builddir)/contrib/libxdg-basedir/libxdg-basedir.la'
fi

AC_SUBST([XDG_BASEDIR_CPPFLAGS])
AC_SUBST([XDG_BASEDIR_LIBS])
AC_SUBST([XDG_BASEDIR_DEPS])

AM_CONDITIONAL([EXTERNAL_LIBXDG_BASEDIR], [test x"$with_external_libxdg_basedir" = x"yes"])

dnl Test for socket and network support library
AC_CHECK_LIB([socket], [socket],        [NET_LIBS="-lsocket $NET_LIBS"])
AC_CHECK_LIB([nsl],    [gethostbyname], [NET_LIBS="-lnsl $NET_LIBS"])
AC_CHECK_LIB([resolv], [hstrerror],     [NET_LIBS="-lresolv $NET_LIBS"])
AC_SUBST(NET_LIBS)

AC_CHECK_LIB([rt], [clock_getres],
             [RT_LIBS="-lrt"
              AC_DEFINE(HAVE_POSIX_TIMERS, 1, [Define this if you have POSIX timers.])], 
             [AC_MSG_RESULT([*** no POSIX timers available.])])
AC_SUBST(RT_LIBS)

AC_CHECK_LIB([posix4], [sched_get_priority_min])

AC_CHECK_LIB([kstat], [kstat_open],
             [KSTAT_LIBS=-lkstat
              AC_DEFINE([HAVE_KSTAT], 1, [Define this if you have kernel statistics available via kstat interface])])
AC_SUBST(KSTAT_LIBS)

AC_ARG_WITH([zlib-prefix], 
            [AS_HELP_STRING([--with-zlib-prefix=PREFIX], [path to zlib compression library])],
            [if test x"$withval" != x"no"; then
                 ZLIB_CPPFLAGS="-I$withval/include"
                 ZLIB_LIBS="-L$withval/lib"
             fi])

AC_CHECK_LIB([z], [gzsetparams],
             [ZLIB_LIBS="$ZLIB_LIBS -lz"
              ac_save_CPPFLAGS="$CPPFLAGS" CPPFLAGS="$CPPFLAGS $ZLIB_CPPFLAGS"
              AC_CHECK_HEADER([zlib.h], [have_zlib=yes], [have_zlib=no])
              CPPFLAGS="$ac_save_CPPFLAGS"],
             [have_zlib=no],
             ["$ZLIB_LIBS"])

test x"$have_zlib" != x"yes" && AC_MSG_ERROR(zlib needed)
AC_SUBST(ZLIB_CPPFLAGS)
AC_SUBST(ZLIB_LIBS)


dnl -----------------------
dnl checks for header files
dnl -----------------------

AC_HEADER_STDC

dnl REVISIT: should we not be using AC_FUNC_ALLOCA for this?
AC_CHECK_HEADERS([alloca.h])

AC_CHECK_HEADERS([assert.h byteswap.h dirent.h execinfo.h libgen.h malloc.h netdb.h ucontext.h])
AC_CHECK_HEADERS([sys/ioctl.h sys/mixer.h sys/mman.h sys/param.h sys/times.h])


dnl ----------------
dnl checks for types
dnl ----------------

AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_CHECK_SIZEOF(long)
dnl AC_CHECK_TYPES([ptrdiff_t])
dnl For systems without inttypes.h would be needed extend generated replacement.
AC_CHECK_GENERATE_INTTYPES([include])
AM_CONDITIONAL([GENERATED_INTTYPES_H], [test "x$ac_cv_header_inttypes_h" != x"yes"])
AC_CHECK_TYPE(ssize_t, :, AC_DEFINE(ssize_t, __int64, [define ssize_t to __int64 if it's missing in default includes]))

AC_CHECK_SOCKLEN_T
AC_IOCTL_REQUEST

dnl ---------------------
dnl checks for structures
dnl ---------------------


dnl -----------------------------------
dnl checks for compiler characteristics
dnl -----------------------------------

if test x"$GCC" = x"yes"; then
    GCC_VERSION="`"$CC" -dumpversion`"
    GCC_VERSION_MAJOR="`echo "$GCC_VERSION" | cut -d '.' -f1`"
    GCC_VERSION_MINOR="`echo "$GCC_VERSION" | cut -d '.' -f2`"
    GCC_VERSION_PATCHLEVEL="`echo "$GCC_VERSION" | cut -d '.' -f3`"
fi
AC_SUBST(GCC_VERSION)
AC_SUBST(GCC_VERSION_MAJOR)
AC_SUBST(GCC_VERSION_MINOR)
AC_SUBST(GCC_VERSION_PATCHLEVEL)

CC_CHECK_WERROR

CC_PTHREAD_FLAGS([], [AC_MSG_ERROR([Pthread support is needed])])
CC_PTHREAD_RECURSIVE_MUTEX([], [AC_MSG_ERROR([recursive mutex support is needed - please report])])

dnl REVISIT: AC_C_ALWAYS_INLINE removal allows ffmpeg to be more widely buildable
AC_C_BIGENDIAN
AC_C_CONST
AC_C_INLINE
AC_C_RESTRICT

dnl ASM ALIGN is power of two ?
dnl src/post/planar
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[asm(".align 3");]])],
                  [AC_DEFINE([ASMALIGN_1SLN], [1],
                             [define if '.align n' means alignment to (1 << n) - byte boundaries])])

CC_ATTRIBUTE_ALIGNED

CC_ATTRIBUTE_PACKED([XINE_PACKED='__attribute__((packed))'],
                    [AC_MSG_WARN([Your compiler doesn't support __attribute__((packed)); xine might not work as expected.])])
AC_DEFINE_UNQUOTED([XINE_PACKED], [$XINE_PACKED], [Mark a structure as being packed])

CC_ATTRIBUTE_MALLOC

CC_ATTRIBUTE_VISIBILITY([protected],
                        [visibility_export="protected"],
                        [CC_ATTRIBUTE_VISIBILITY([default], [visibility_export="default"])])

if test x"$visibility_export" != x""; then
    CC_FLAG_VISIBILITY([VISIBILITY_FLAG="-fvisibility=hidden"
                        EXPORTED='__attribute__((visibility("default")))'])
fi
AC_DEFINE_UNQUOTED([EXPORTED], [$EXPORTED], [Mark a symbol as being exported if visibility is changed])
AC_SUBST([VISIBILITY_FLAG])

CC_ATTRIBUTE_SENTINEL
CC_ATTRIBUTE_FORMAT
CC_ATTRIBUTE_FORMAT_ARG

CC_CHECK_CFLAGS([-pipe], [miscflags="$miscflags -pipe"])

dnl Set warning flags for warnings that we do not want to skip.  In all cases,
dnl these warnings should be enabled.  Set these into CFLAGS and OBJCFLAGS
dnl later after all testing is done.

CC_CHECK_CFLAGS([-Wall], [warnflags="$warnflags -Wall"])

CC_CHECK_CFLAGS([-Wformat=2], [wformat="-Wformat=2"],
                [CC_CHECK_CFLAGS([-Wformat], [wformat="-Wformat"])])
if test x"$wformat" != x""; then
    CC_CHECK_CFLAGS([-Wno-format-zero-length], [wformat="$wformat -Wno-format-zero-length"])
fi
CC_CHECK_CFLAGS([-Wmissing-format-attribute], [wformat="$wformat -Wmissing-format-attribute"])
warnflags="$warnflags $wformat"

dnl WARNING: This warning flag, if set into CFLAGS now, can break some autoconf tests.
CC_CHECK_CFLAGS([-Werror-implicit-function-declaration], [warnflags="$warnflags -Werror-implicit-function-declaration"])

CC_CHECK_CFLAGS([-Wstrict-aliasing=2], [warnflags="$warnflags -Wstrict-aliasing=2"],
                [CC_CHECK_CFLAGS([-Wstrict-aliasing], [warnflags="$warnflags -Wstrict-aliasing"])])

CC_CHECK_CFLAGS([-Wchar-subscripts],      [warnflags="$warnflags -Wchar-subscripts"])
CC_CHECK_CFLAGS([-Wmissing-declarations], [warnflags="$warnflags -Wmissing-declarations"])
CC_CHECK_CFLAGS([-Wmissing-prototypes],   [warnflags="$warnflags -Wmissing-prototypes"])

dnl Some combinations of gcc and glibc produce useless warnings on memset when
dnl compiling with -Wpointer-arith, so check for this first.
AC_MSG_CHECKING([for sane -Wpointer-arith])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <string.h>]], [[int a; memset(&a, 0, sizeof(int))]])],
                  [warnflags="$warnflags -Wpointer-arith"
                   AC_MSG_RESULT([yes])], [AC_MSG_RESULT([no])])

dnl FreeBSD (et al.) does not complete linking for shared objects when pthreads
dnl are requested, as different implementations are present; to avoid problems
dnl use -Wl,-z,defs only for those platforms not behaving this way.
case "$host_or_hostalias" in
    *-freebsd*) ;;

    dnl check if we are using the cygwin, mingw or cygwin with mno-cygwin mode
    dnl in which case we are actually dealing with a mingw32 compiler
    dnl This cannot be done until AC_PROG_EGREP and AC_PROG_CC are both done.
    *-*-mingw* | *-*-cygwin*)
        case "$host_or_hostalias" in
            *-*-mingw32*)
                WIN32_SYS=mingw32
                ;;
            *-*-cygwin*)
                AC_EGREP_CPP([yes],
                             [#ifdef WIN32
                              yes
                              #endif],
                             [WIN32_SYS=mingw32], [WIN32_SYS=cygwin])
                ;;
        esac

        if test "$WIN32_SYS" = "mingw32"; then
            WIN32_CPPFLAGS='-I$(top_srcdir)/win32/include'
            LIBS="-lwinmm -lwsock32 $LIBS"
            GOOM_LIBS="-liberty"
            AC_SUBST(GOOM_LIBS)
            AC_SUBST(WIN32_CPPFLAGS)
        fi
        LDFLAGS="-no-undefined $LDFLAGS"
        ;;

    *)
        CC_CHECK_LDFLAGS([-Wl,-z,defs], [NOUNDEF="-Wl,-z,defs"])
        AC_SUBST([NOUNDEF])
        ;;
esac

AM_CONDITIONAL([WIN32], [test x"$WIN32_SYS" = x"mingw32"])

dnl No optimization at all.  For gcc, this is the optimization level.
O0_CFLAGS="-O0"

dnl Lowest level of optimization.  For gcc, this enables:
dnl     -fdefer-pop -fdelayed-branch -fguess-branch-probability -fcprop-registers
dnl     -floop-optimize -fif-conversion -fif-conversion2 -ftree-ccp -ftree-dce
dnl     -ftree-dce -ftree-dominator-opts -ftree-dse -ftree-ter -ftree-lrs
dnl     -ftree-sra -ftree-copyrename -ftree-fre -ftree-ch -fmerge-constants
dnl On platforms where -fomit-frame-pointer does not interfere with debugging,
dnl it is also enabled by -O1.
O1_CFLAGS="-O1"

dnl Middle level of optimization.  For gcc, this enables -O1 and:
dnl     -fthread-jumps -fcrossjumping -foptimize-sibling-calls -fcse-follow-jumps
dnl     -fcse-skip-blocks -fgcse -fgcse-lm -fexpensive-optimizations
dnl     -fstrength-reduce -frerun-cse-after-loop -frerun-loop-opt -fcaller-saves
dnl     -fforce-mem -fpeephole2 -fschedule-insns -fschedule-insns2
dnl     -fsched-interblock -fsched-spec -fregmove -fstrict-aliasing
dnl     -fdelete-null-pointer-checks -freorder-blocks -freorder-functions
dnl     -funit-at-a-time -falign-functions -falign-jumps -falign-loops
dnl     -falign-labels -ftree-pre
dnl Note that Apple's version of gcc differs slightly, because it does not enable
dnl     -fstrict-aliasing -freorder-blocks -fsched-interblock
O2_CFLAGS="-O2"

dnl Highest level of optimization.  For gcc, this enables -O2 and:
dnl     -finline-functions -funswitch-loops -fgcse-after-reload
O3_CFLAGS="-O3"

dnl gcc 3.3.5 (at least) is known to be buggy wrt optimization with
dnl -finline-functions, so use -fno-inline-functions for gcc < 3.4.0
if test x"$GCC" = x"yes"; then
    if test "$GCC_VERSION_MAJOR" -lt 3; then
        O3_CFLAGS="$O3_CFLAGS -fno-inline-functions"
    else
        if test "$GCC_VERSION_MAJOR" -eq 3 -a "$GCC_VERSION_MINOR" -lt 4; then
            O3_CFLAGS="$O3_CFLAGS -fno-inline-functions"
        fi
    fi
fi

CC_CHECK_CFLAGS([-ffast-math],               [optflags="$optflags -ffast-math"])
CC_CHECK_CFLAGS([-fexpensive-optimizations], [optflags="$optflags -fexpensive-optimizations"])

case "$host_or_hostalias" in
    alphaev56-*)    cpuflags="-mcpu=ev56 -mieee $cpuflags" ;;
    alpha*)         cpuflags="-mieee $cpuflags" ;;

    armv4l-*-linux*)
        cpuflags="-mcpu=strongarm1100 -ffast-math -fsigned-char $cpuflags"
        DEFAULT_OCFLAGS='$(O2_CFLAGS)'
        ;;

    sparc*-*-linux*)
        case "`uname -m`" in
            sparc)      cpuflags="-mcpu=supersparc -mtune=supersparc" ;;
            sparc64)    cpuflags="-mcpu=ultrasparc -mtune=ultrasparc" ;;
        esac
        test x"$enable_vis" != x"no" && has_vis=yes
        AC_DEFINE([ARCH_SPARC], [], [Define this if you're running SPARC architecture])
        ;;

    sparc-*-solaris*)
        if test x"$GCC" = x"yes"; then
            case "`uname -m`" in
                sun4c) cpuflags="-mcpu=v7 -mtune=supersparc" ;;
                sun4m) cpuflags="-mcpu=v8 -mtune=supersparc" ;;
                sun4u)
                    if test x"$GCC_VERSION_MAJOR" -lt 3; then
                        # -mcpu=ultrasparc triggers a GCC 2.95.x compiler bug
                        # when compiling video_out.c:
                        #   gcc: Internal compiler error: program cc1 got fatal signal 11
                        # avoid -mcpu=ultrasparc with gcc 2.*
                        cpuflags="-mcpu=v8 -mtune=ultrasparc"
                    else
                        cpuflags="-mcpu=ultrasparc -mtune=ultrasparc"
                    fi
                    ;;
            esac
            if test "$GCC_VERSION_MAJOR" -ge 3; then
                test x$"enable_vis" != x"no" && has_vis=yes
            fi
        else
            case "`uname -m`" in
                sun4c) cpuflags="-xarch=v7" ;;
                sun4m) cpuflags="-xarch=v8" ;;
                sun4u) cpuflags="-xarch=v8plusa" ;;
            esac
            O1_CFLAGS="-fast -xCC"
            O2_CFLAGS="$O1_CFLAGS"
            O3_CFLAGS="$O1_CFLAGS"
        fi
        AC_DEFINE([ARCH_SPARC], [], [Define this if you're running SPARC architecture])
        ;;

    x86_64-*)
        arch_x86=yes
        AC_DEFINE([ARCH_X86_64], [], [Define this if you're running x86 architecture 64 bits])
        ;;

    *-*-darwin*)
        case "$host_or_hostalias" in
            i386-* | universal-* | x86_64-*)
                arch_x86=yes
                ;;
            ppc* | powerpc*)
                dnl avoid ppc compilation crash
                AS="$CC"

                AC_CHECK_HEADER([altivec.h], [], [enable_altivec=no])
                if test x"$enable_altivec" != x"no"; then
                    AC_DEFINE([ENABLE_ALTIVEC], [], [Define this if you want to use altivec on PowerPC CPUs])
                    cpuflags="$cpuflags -faltivec -maltivec"
                    LIBMPEG2_CFLAGS="$LIBMPEG2_CFLAGS -force_cpusubtype_ALL"
                fi
                ;;
        esac

        enable_impure_text=yes
        HOST_OS_DARWIN=1
        dnl HOST_OS_DARWIN is used by a bunch of difference stuff, including
        dnl libdvdnav, libmpeg2, and xine itself (xine-engine, xine-utils)
        AC_DEFINE([HOST_OS_DARWIN], 1, [Define this if built on Mac OS X/Darwin])
        dnl CONFIG_DARWIN is used by ffmpeg, so anything that pulls ffmpeg
        dnl headers needs to have this set.
        AC_DEFINE([CONFIG_DARWIN],  1, [Define this if built on Mac OS X/Darwin])
        CPPFLAGS="-D_INTL_REDIRECT_MACROS $CPPFLAGS"
        ;;

    ppc-*-linux* | powerpc-*) 
        dnl avoid ppc compilation crash
        AS="$CC"
        AC_DEFINE([ARCH_PPC], [], [Define this if you're running PowerPC architecture])

        AC_CHECK_HEADER([altivec.h], [], [enable_altivec=no])
        
        if test x"$enable_altivec" != x"no"; then
            AC_DEFINE([ENABLE_ALTIVEC], [], [Define this if you want to use altivec on PowerPC CPUs])
            cpuflags="$cpuflags -maltivec"
        fi
        ;;

    i?86-* | k?-* | athlon-* | pentium*)
        arch_x86=yes
        enable_impure_text=yes

        case "$host_or_hostalias" in
            *-*-cygwin* | *-*-mingw32*)
                CC_CHECK_CFLAGS([-fno-omit-frame-pointer], [W32_NO_OPTIMIZE="$W32_NO_OPTIMIZE -fno-omit-frame-pointer"])
                CC_CHECK_CFLAGS([-fno-inline-functions],   [W32_NO_OPTIMIZE="$W32_NO_OPTIMIZE -fno-inline-functions"])
                CC_CHECK_CFLAGS([-fno-rename-registers],   [W32_NO_OPTIMIZE="$W32_NO_OPTIMIZE -fno-rename-registers"])
                AC_SUBST(W32_NO_OPTIMIZE)
                case "$host_or_hostalias" in
                    *-*-cygwin*) LIBS="$LIBS @INTLLIBS@ -lkernel32" ;;
                esac
                ;;
        esac

        AC_DEFINE([ARCH_X86_32], [], [Define this if you're running x86 architecture 32 bits])

        if test x"$GCC" = x"yes" -o x"${CC##*/}" = x"icc"; then
            if test x"$GCC" = x"yes"; then
                # GCC optimizations
                CC_CHECK_CFLAGS([-mtune=i386], [sarchopt="-mtune"],
                    [CC_CHECK_CFLAGS([-mcpu=i386], [sarchopt="-mcpu"],
                        [CC_CHECK_CFLAGS([-march=i386], [sarchopt="-march"], [sarchopt="no"])])])
                if test "$sarchopt" != "no"; then
                    case "$host_or_hostalias" in
                        i386-*)         archopt_val="i386" ;;
                        i486-*)         archopt_val="i486" ;;
                        i586-*)         archopt_val="pentium" ;;
                        pentium-mmx-*)  archopt_val="pentium-mmx" ;;
                        k6-2* | k6-3-*) archopt_val="k6-2" ;;
                        k6-*)           archopt_val="k6" ;;
                        pentium3-*)     archopt_val="pentium3" ;;
                        pentium4-*)     archopt_val="pentium4" ;;

                        athlon-4-* | athlon-xp-* | althon-mp-*) archopt_val="athlon-4" ;;
                        k7-* | athlon-tbird-* | athlon-*)       archopt_val="athlon" ;;

                        pentiumpro-* | pentium2-* | i686-*)
                            archopt_val="pentiumpro"
                            if test x"$cross_compiling" != x"yes"; then
                                if test -f /proc/cpuinfo; then
                                    modelname=`cat /proc/cpuinfo | grep "model\ name\	:" | sed -e 's/ //g' | cut -d ':' -f2`
                                    case "$modelname" in
                                        *Athlon* | *Duron* | *K7*)
                                            dnl Special check for k7 cpu CC support
                                            CC_CHECK_CFLAGS([$sarchopt=athlon],
                                                            [archopt_val="athlon"], [archopt_val="i686"])
                                            ;;
                                        VIAEzra)
                                            archopt_val="c3"
                                            ;;
                                    esac
                                fi
                            fi
                            ;;
                    esac
                    test x$"archopt_val" != x"" && cpuflags="$cpuflags $sarchopt=$archopt_val"
                fi
            else
                # Intel optimizations
                O3_CFLAGS="$O3_CFLAGS -unroll -ipo -ipo_obj"
            fi
        fi
        ;;
esac

if test "x$has_vis" = "xyes"; then
    AC_DEFINE([ENABLE_VIS], [], [Define this if you have Sun UltraSPARC CPU])
    case "$cpuflags" in
        *-mcpu=*) ;;
        *) cpuflags="$cpuflags -mcpu=v9" ;;
    esac
fi
AM_CONDITIONAL([ENABLE_VIS], test x"$has_vis" = x"yes")

dnl Allow turning off/on of optimizations.  By default, optimizations are
dnl enabled if --enable-debug is not specified.  Even with --enable-debug,
dnl optimizations can be enabled by explicitly specifying --enable-optimizations
AC_ARG_ENABLE([optimizations],
              [AS_HELP_STRING([--disable-optimizations], [Don't try to guess what optimization to enable])],
              [], [test x"$enable_debug" != x"no" && enable_optimizations="no"])
if test x"$enable_optimizations" = x"no"; then
    O1_CFLAGS="$O0_CLFAGS"
    O2_CFLAGS="$O0_CFLAGS"
    O3_CFLAGS="$O0_CFLAGS"
else
    O1_CFLAGS="$O1_CFLAGS $optflags $cpuflags"
    O2_CFLAGS="$O2_CFLAGS $optflags $cpuflags"
    O3_CFLAGS="$O3_CFLAGS $optflags $cpuflags"

    dnl For multi-pass compilation: never when cross-compiling
    if test x"$cross_compiling" != x"yes"; then
        if test x"$GCC" = x"yes"; then
            CC_CHECK_CFLAGS([-fprofile-arcs], [CC_CHECK_CFLAGS([-fbranch-probabilities],
                            [PASS1_CFLAGS="-fprofile-arcs $PASS1_CFLAGS"
                             PASS2_CFLAGS="-fbranch-probabilities $PASS2_CFLAGS"])])
        else
            pass1flags="-prof_dir \$(PWD)/\$(top_builddir)/ -prof_genx"
            pass2flags="-prof_dir \$(PWD)/\$(top_builddir)/ -prof_use"
            CC_CHECK_CFLAGS(["$pass1flags"], [CC_CHECK_CFLAGS(["$pass2flags"],
                            [PASS1_CFLAGS="$pass1flags $PASS1_CFLAGS"
                             PASS2_CFLAGS="$pass2flags $PASS2_CFLAGS"])])
        fi
    fi
fi
AC_SUBST(O0_CFLAGS)
AC_SUBST(O1_CFLAGS)
AC_SUBST(O2_CFLAGS)
AC_SUBST(O3_CFLAGS)

AC_SUBST(PASS1_CFLAGS)
AC_SUBST(PASS2_CFLAGS)

test x"$DEFAULT_OCFLAGS" = x"" && DEFAULT_OCFLAGS='$(O3_CFLAGS)'
AC_SUBST(DEFAULT_OCFLAGS)

if test x"$arch_x86" = x"yes"; then
     AC_DEFINE([ARCH_X86], [], [Define this if you're running x86 architecture])
     AC_DEFINE([HAVE_MMX], [], [Define this if you can compile MMX asm instructions])
fi
AM_CONDITIONAL([ARCH_X86], test x"$arch_x86" = x"yes")
AM_CONDITIONAL([HAVE_MMX], test x"$arch_x86" = x"yes")
AM_CONDITIONAL([HOST_OS_DARWIN], test x"$HOST_OS_DARWIN" = x"1")

if test x"$enable_impure_text" = x"yes"; then
    case "$host_or_hostalias" in
        *solaris*)
            if test "$GCC" = yes; then
                IMPURE_TEXT_LDFLAGS="-mimpure-text"
            else
                IMPURE_TEXT_LDFLAGS="-z textoff"
            fi
            ;;
        *darwin*)
            IMPURE_TEXT_LDFLAGS="-Wl,-read_only_relocs,warning"
            ;;
    esac
fi
AC_SUBST(IMPURE_TEXT_LDFLAGS)


dnl ----------------------------
dnl checks for library functions
dnl ----------------------------

dnl NLS: src/input/mms.c src/input/vcd, xine-utils
AC_CHECK_FUNCS([nl_langinfo])

dnl src/libfaad
AC_CHECK_DECL(lrintf,[
	AC_DEFINE([HAVE_LRINTF],    [1], [Define this if the 'lrintf' function is declared in math.h])
	AC_DEFINE([_ISOC9X_SOURCE], [1], [Define this if you are ISO C9X compliant])
      ],,[
#define _ISOC9X_SOURCE
#include <math.h>
])

dnl contrib/libdca, src/video_out/vidix/drivers/mach64_vid.c
AC_CHECK_FUNCS([memalign])

dnl src/input/input_file.c
AC_ARG_ENABLE([mmap],
              AS_HELP_STRING([--enable-mmap], [Enable mmap() file loading (default: no)]))
if test x"$enable_mmap" = x"yes"; then
    AC_CHECK_FUNCS([mmap])
fi

AC_CHECK_FUNCS([vsscanf sigaction sigset getpwuid_r nanosleep lstat memset readlink strchr va_copy])

AC_CHECK_FUNCS([snprintf _snprintf], [have_required_function="yes"])
               test x"$have_required_function" != x"yes" && AC_MSG_ERROR([required function not found])
AC_CHECK_FUNCS([vsnprintf _vsnprintf], [have_required_function="yes"])
               test x"$have_required_function" != x"yes" && AC_MSG_ERROR([required function not found])
AC_CHECK_FUNCS([strcasecmp _stricmp], [have_required_function="yes"])
               test x"$have_required_function" != x"yes" && AC_MSG_ERROR([required function not found])
AC_CHECK_FUNCS([strncasecmp _strnicmp], [have_required_function="yes"])
               test x"$have_required_function" != x"yes" && AC_MSG_ERROR([required function not found])

AC_FUNC_FSEEKO

AC_REPLACE_FUNCS([asprintf basename gettimeofday setenv strndup strpbrk strsep strtok_r timegm unsetenv])

AC_LIBSOURCE([hstrerror.c])
AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <netdb.h>]], [[hstrerror(0)]])],
               [AC_DEFINE([HAVE_HSTRERROR], 1, [Define to 1 if you have 'hstrerror' in <netdb.h>])],
               [AC_LIBOBJ([hstrerror])])

AC_LIBSOURCE([dirent_msvc.c])
AC_CHECK_FUNC([opendir],
              [AC_DEFINE([HAVE_OPENDIR], 1, [Define to 1 if you have 'opendir' function])],
              [if test x"$WIN32_SYS" = x"mingw32"; then
                   AC_LIBOBJ([dirent_msvc])
               else
                   AC_MSG_ERROR([dirent is needed (opendir, readdir, ...)])
               fi])


dnl --------------------------
dnl checks for system services
dnl --------------------------

XINE_X11_SUPPORT

AC_SYS_LARGEFILE


dnl -------
dnl Plugins
dnl -------

XINE_AUDIO_OUT_PLUGINS
XINE_VIDEO_OUT_PLUGINS
XINE_INPUT_PLUGINS
























dnl ---------------------------------------------
dnl mpeg2lib and ffmpeg stuff
dnl ---------------------------------------------

AC_SUBST(LIBMPEG2_CFLAGS)

AC_ARG_WITH([external-ffmpeg], AS_HELP_STRING([--with-external-ffmpeg], [use external ffmpeg library]))

case "x$with_external_ffmpeg" in
   xyes)
      PKG_CHECK_MODULES([FFMPEG], [libavcodec >= 51.20.0])
      ;;
   xsoft)
      with_external_ffmpeg=yes
      PKG_CHECK_MODULES([FFMPEG], [libavcodec >= 51.20.0], [],
			[AC_MSG_RESULT(no); with_external_ffmpeg=no])
      ;;
esac
if test "x$with_external_ffmpeg" = "xyes"; then
   PKG_CHECK_MODULES([FFMPEG_POSTPROC], [libpostproc])
   AC_SUBST([FFMPEG_CFLAGS])
   AC_SUBST([FFMPEG_LIBS])
   AC_SUBST([FFMPEG_POSTPROC_CFLAGS])
   AC_SUBST([FFMPEG_POSTPROC_LIBS])
   AC_DEFINE([HAVE_FFMPEG], [1], [Define this if you have ffmpeg library])
   
   AC_MSG_NOTICE([
*********************************************************************
xine is configured with external ffmpeg.

This requires the same version of ffmpeg what is included in xine and
you should know what you do. If some problems occur, please try to
use internal ffmpeg.
*********************************************************************])
else
  AC_MSG_RESULT([using included ffmpeg])
fi
AM_CONDITIONAL(HAVE_FFMPEG, test "x$with_external_ffmpeg" = "xyes")


AC_ARG_ENABLE([ffmpeg_uncommon_codecs],
	AS_HELP_STRING([--disable-ffmpeg-uncommon-codecs], [don't build uncommon ffmpeg codecs]))
AC_ARG_ENABLE([ffmpeg_popular_codecs],
	AS_HELP_STRING([--disable-ffmpeg-popular-codecs], [don't build popular ffmpeg codecs]))

AM_CONDITIONAL([FFMPEG_DISABLE_UNCOMMON_CODECS], [test "x$enable_ffmpeg_uncommon_codecs" = "xno"])
AM_CONDITIONAL([FFMPEG_DISABLE_POPULAR_CODECS], [test "x$enable_ffmpeg_popular_codecs" = "xno"])

LIBMPEG2_CFLAGS=""

AC_ARG_ENABLE([mlib],
	AS_HELP_STRING([--disable-mlib], [do not build Sun mediaLib support]))

AC_ARG_ENABLE([mlib-lazyload],
	AS_HELP_STRING([--enable-mlib-lazyload], [check for Sun mediaLib at runtime]))

if test "x$enable_mlib" != xno; then
    if test "x$MLIBHOME" = x; then
	mlibhome=/opt/SUNWmlib
    else
	mlibhome="$MLIBHOME"
    fi

    AC_CHECK_LIB(mlib, mlib_VideoAddBlock_U8_S16,
	[ saved_CPPFLAGS="$CPPFLAGS"
	  CPPFLAGS="$CPPFLAGS -I$mlibhome/include"
	  AC_CHECK_HEADER(mlib_video.h,
	       [ if test "x$enable_mlib_lazyload" = xyes; then
		     if test "$GCC" = yes; then
			 MLIB_LIBS="-L$mlibhome/lib -Wl,-z,lazyload,-lmlib,-z,nolazyload"
		     else
			 MLIB_LIBS="-L$mlibhome/lib -z lazyload -lmlib -z nolazyload"
		     fi
		     AC_DEFINE(MLIB_LAZYLOAD,1,[Define this if you want to load mlib lazily])
		 else
		     MLIB_LIBS="-L$mlibhome/lib -lmlib"
		 fi
		 MLIB_CFLAGS="-I$mlibhome/include"
		 LIBMPEG2_CFLAGS="$LIBMPEG2_CFLAGS $MLIB_CFLAGS" 
		 LIBFFMPEG_CFLAGS="$LIBFFMPEG_CFLAGS $MLIB_CFLAGS"
		 AC_DEFINE(HAVE_MLIB,1,[Define this if you have mlib installed])
		 AC_DEFINE(LIBMPEG2_MLIB,1,[Define this if you have mlib installed])
		 ac_have_mlib=yes
	       ],)
	  CPPFLAGS="$saved_CPPFLAGS"
	], , -L$mlibhome/lib)
fi
AM_CONDITIONAL(HAVE_MLIB, test "x$ac_have_mlib" = "xyes")
AC_SUBST(MLIB_LIBS)
AC_SUBST(MLIB_CFLAGS)

dnl ---------------------------------------------
dnl Ogg/Vorbis libs.
dnl ---------------------------------------------

AC_ARG_WITH([vorbis],
	AS_HELP_STRING([--without-vorbis], [Build without Vorbis audio decoder]))

if test "x$with_vorbis" != "xno"; then
   PKG_CHECK_MODULES([VORBIS], [ogg vorbis], [have_vorbis=yes], [have_vorbis=no])
   if test "x$with_vorbis" = "xyes" && test "x$have_vorbis" = "xno"; then
      AC_MSG_ERROR([Vorbis support requested, but libvorbis not found])
   fi
fi
AM_CONDITIONAL([HAVE_VORBIS], [test "x$have_vorbis" = "xyes"])

AC_SUBST([VORBIS_CFLAGS])
AC_SUBST([VORBIS_LIBS])

dnl ---------------------------------------------
dnl Ogg/Theora libs.
dnl ---------------------------------------------

AC_ARG_WITH([theora],
	AS_HELP_STRING([--without-theora], [Build without Theora video decoder]))

if test "x$with_theora" != "xno"; then
   PKG_CHECK_MODULES([THEORA], [ogg theora], [have_theora=yes], [have_theora=no])
   if test "x$with_theora" = "xyes" && test "x$have_theora" = "xno"; then
      AC_MSG_ERROR([Theora support requested, but libtheora not found])
   elif test "x$have_theora" = "xyes"; then
      AC_DEFINE([HAVE_THEORA], [1], [Define this if you have theora])
   fi
fi
AM_CONDITIONAL([HAVE_THEORA], [test "x$have_theora" = "xyes"])

AC_SUBST([THEORA_CFLAGS])
AC_SUBST([THEORA_LIBS])

dnl ---------------------------------------------
dnl Ogg/Speex libs.
dnl ---------------------------------------------
AC_ARG_WITH([speex],
	AS_HELP_STRING([--without-speex], [Build without Speex audio decoder]))

if test "x$with_speex" != "xno"; then
   PKG_CHECK_MODULES([SPEEX], [ogg speex], [have_speex=yes], [have_speex=no])
   if test "x$with_speex" = "xyes" && test "x$have_speex" = "xno"; then
      AC_MSG_ERROR([Speex support requested, but libspeex not found])
   elif test "x$have_speex" = "xyes"; then
      AC_DEFINE([HAVE_SPEEX], [1], [Define this if you have speex])
   fi
fi
AM_CONDITIONAL([HAVE_SPEEX], [test "x$have_speex" = "xyes"])

AC_SUBST([SPEEX_CFLAGS])
AC_SUBST([SPEEX_LIBS])

dnl ---------------------------------------------
dnl check for libFLAC
dnl ---------------------------------------------

AC_ARG_WITH([libflac],
  AS_HELP_STRING([--with-libflac], [build libFLAC-based decoder and demuxer]))

have_libflac="no"
if test "x$with_libflac" = "xyes"; then
  AM_PATH_LIBFLAC([have_libflac="yes"])
fi

AM_CONDITIONAL([HAVE_LIBFLAC], [test "x$have_libflac" = "xyes"])

dnl ---------------------------------------------
dnl External version of a52dec
dnl ---------------------------------------------

AC_ARG_ENABLE(a52dec, AS_HELP_STRING([--disable-a52dec], [Disable support for a52dec decoding library (default: enabled)]),
              [enable_a52dec="$enableval"], [enable_a52dec="yes"])
AC_ARG_WITH(external-a52dec, AS_HELP_STRING([--with-external-a52dec], [use external a52dec library (not recommended)]),
            [external_a52dec="$withval"], [external_a52dec="no"])

have_a52="no"

if test "x$enable_a52dec" = "xno"; then
  AC_MSG_RESULT([a52dec support disabled])
elif test "x$external_a52dec" = "xyes"; then
  have_a52="yes"
  AC_CHECK_HEADERS([a52dec/a52.h a52dec/a52_internal.h],, have_a52="no",
[
  #ifdef HAVE_SYS_TYPES_H
  # include <sys/types.h>
  #endif
  #ifdef HAVE_INTTYPES_H
  # include <inttypes.h>
  #endif
  #ifdef HAVE_STDINT_H
  # include <stdint.h>
  #endif

  #include <a52dec/a52.h>
])
  SAVE_LIBS="$LIBS"
  AC_CHECK_LIB([a52], [a52_init],, have_a52="no", [-lm])
  LIBS="$SAVE_LIBS"

  if test "x$have_a52" = "xno"; then
    AC_MSG_RESULT([*** no usable version of a52dec found, using internal copy ***])
  fi
else
  AC_MSG_RESULT([Use included a52dec support])
fi

AM_CONDITIONAL(A52, test "x$enable_a52dec" = "xyes")
AM_CONDITIONAL(EXTERNAL_A52DEC, test "x$have_a52" = "xyes")

dnl ---------------------------------------------
dnl External version of libmad
dnl ---------------------------------------------

AC_ARG_ENABLE(mad, AS_HELP_STRING([--disable-mad], [Disable support for MAD decoding library (default: enabled)]),
              [enable_libmad="$enableval"], [enable_libmad="yes"])
AC_ARG_WITH(external-libmad, AS_HELP_STRING([--with-external-libmad], [use external libmad library (not recommended)]),
            [external_libmad="$withval"], [external_libmad="no"])

have_mad="no"

if test "x$enable_libmad" = "xno"; then
  AC_MSG_RESULT([libmad support disabled])
elif test "x$external_libmad" = "xyes"; then
  PKG_CHECK_MODULES(LIBMAD, [mad], have_mad=yes, have_mad=no)
  AC_CHECK_HEADERS([mad.h])
  AC_SUBST(LIBMAD_LIBS)
  AC_SUBST(LIBMAD_CFLAGS)
  if test "x$have_mad" = "xno"; then
    AC_MSG_RESULT([*** no usable version of libmad found, using internal copy ***])
  fi
else
  AC_MSG_RESULT([Use included libmad support])
  case "$host_or_hostalias" in
    i?86-* | k?-* | athlon-* | pentium*-)
      AC_DEFINE(FPM_INTEL,1,[Define to select libmad fixed point arithmetic implementation])
      ;;
    x86_64-*)
      AC_DEFINE(FPM_64BIT,1,[Define to select libmad fixed point arithmetic implementation])
      ;;
    ppc-* | powerpc-*) 
      AC_DEFINE(FPM_PPC,1,[Define to select libmad fixed point arithmetic implementation])
      ;;
    sparc*-*)
      if test "$GCC" = yes; then
        AC_DEFINE(FPM_SPARC,1,[Define to select libmad fixed point arithmetic implementation])
      else
        AC_DEFINE(FPM_64BIT,1,[Define to select libmad fixed point arithmetic implementation])
      fi
      ;;
    mips-*)
      AC_DEFINE(FPM_MIPS,1,[Define to select libmad fixed point arithmetic implementation])
      ;;
    alphaev56-* | alpha* | ia64-* | hppa*-linux-*)
      AC_DEFINE(FPM_64BIT,1,[Define to select libmad fixed point arithmetic implementation])
      ;;
    arm*-*)
      AC_DEFINE(FPM_ARM,1,[Define to select libmad fixed point arithmetic implementation])
      ;;
    *)
      AC_DEFINE(FPM_DEFAULT,1,[Define to select libmad fixed point arithmetic implementation])
      ;;
  esac
fi

AM_CONDITIONAL(MAD, test "x$enable_libmad" = "xyes")
AM_CONDITIONAL(EXTERNAL_LIBMAD, test "x$have_mad" = "xyes")

dnl ---------------------------------------------
dnl External libmpcdec support
dnl ---------------------------------------------

AC_ARG_ENABLE([musepack], AS_HELP_STRING([--disable-musepack], [Disable support for MusePack decoding (default: enabled)]))
AC_ARG_WITH([external-libmpcdec], AS_HELP_STRING([--with-external-libmpcdec], [Use external libmpc library]))

if test "x$enable_musepack" = "xno"; then
   AC_MSG_RESULT([musepack support disabled])
elif test "x$with_external_libmpcdec" = "xyes"; then
   AC_CHECK_LIB([mpcdec], [mpc_decoder_decode], [have_mpcdec=yes])
   AC_CHECK_HEADERS([mpcdec/mpcdec.h], , [have_mpcdec=no])
   if test "x$have_mpcdec" != "xyes"; then
      AC_MSG_ERROR([Unable to find mpcdec])
   fi
   MPCDEC_LIBS="-lmpcdec"
   MPCDEC_CFLAGS=""
else
   AC_MSG_RESULT([Use included libmusepack])
   MPCDEC_CFLAGS='-I$(top_srcdir)/contrib/libmpcdec'
   MPCDEC_LIBS='$(top_builddir)/contrib/libmpcdec/libmpcdec.la'
   MPCDEC_DEPS='$(top_builddir)/contrib/libmpcdec/libmpcdec.la'
fi

AC_SUBST(MPCDEC_LIBS)
AC_SUBST(MPCDEC_DEPS)
AC_SUBST(MPCDEC_CFLAGS)

AM_CONDITIONAL([MUSEPACK], [test "x$enable_musepack" != "xno"])
AM_CONDITIONAL([EXTERNAL_MPCDEC], [test "x$have_mpcdec" = "xyes"])

dnl ---------------------------------------------
dnl MNG libs.
dnl ---------------------------------------------

AC_ARG_ENABLE([mng],
  AS_HELP_STRING([--disable-mng], [do not build mng support]),
  [with_mng=$enableval], [with_mng=yes])

if test "x$with_mng" = "xyes"; then
  AC_CHECK_LIB(mng, mng_initialize,
	[ AC_CHECK_HEADER(libmng.h,
		[ have_libmng=yes
		  MNG_LIBS="-lmng" ], 
		AC_MSG_RESULT([*** All libmng dependent parts will be disabled ***]))],
	AC_MSG_RESULT([*** All libmng dependent parts will be disabled ***]))
  AC_SUBST(MNG_LIBS)
else
  have_libmng=no
fi
AM_CONDITIONAL(HAVE_LIBMNG, test "x$have_libmng" = "xyes")

dnl ---------------------------------------------
dnl MagickWand API of Imagemagick.
dnl ---------------------------------------------

AC_ARG_WITH([imagemagick],
	AS_HELP_STRING([--without-imagemagick], [Build without ImageMagick image decoder]))

if test "x$with_imagemagick" != "xno"; then
   PKG_CHECK_MODULES([WAND], [Wand], [have_imagemagick=yes], [have_imagemagick=no])
   if test "x$with_imagemagick" = "xyes" && test "x$have_imagemagick" = "xno"; then
      AC_MSG_ERROR([ImageMagick support requested, but Wand not found])
   elif test "x$have_imagemagick" = "xyes"; then
      AC_DEFINE([HAVE_WAND], [1], [Define this if you have ImageMagick installed])
   fi
fi

AM_CONDITIONAL([HAVE_WAND], [test "x$have_imagemagick" = "xyes"])
AC_SUBST(WAND_CFLAGS)
AC_SUBST(WAND_LIBS)

dnl ---------------------------------------------
dnl freetype2 lib.
dnl ---------------------------------------------
AC_ARG_WITH([freetype],
	AS_HELP_STRING([--with-freetype], [Build with FreeType2 library]))

if test "x$with_freetype" = "xyes"; then
   PKG_CHECK_MODULES([FT2], [freetype2], [have_freetype=yes], [have_freetype=no])
   if test "x$have_freetype" = "xno"; then
      AC_MSG_ERROR([FreeType2 support requested but FreeType2 library not found])
   elif test "x$have_freetype" = "xyes"; then
      AC_DEFINE([HAVE_FT2], [1], [Define this if you have freetype2 library])
   fi
fi
AC_SUBST([FT2_CFLAGS])
AC_SUBST([FT2_LIBS])

dnl ---------------------------------------------
dnl fontconfig
dnl ---------------------------------------------
AC_ARG_WITH([fontconfig],
	AS_HELP_STRING([--with-fontconfig], [Build with fontconfig library]))

if test "x$with_fontconfig" = "xyes"; then
   if test "x$have_freetype" != "xyes"; then
      AC_MSG_ERROR([fontconfig support requested, but FreeType2 not enabled.])
   fi

   PKG_CHECK_MODULES([FONTCONFIG], [fontconfig], [have_fontconfig=yes], [have_fontconfig=no])
   if test "x$have_fontconfig" = "xno"; then
      AC_MSG_ERROR([fontconfig support requested but fontconfig library not found])
   elif test "x$have_fontconfig" = "xyes"; then
      AC_DEFINE([HAVE_FONTCONFIG], [1], [Define this if you have fontconfig library])
   fi
fi
AC_SUBST([FONTCONFIG_CFLAGS])
AC_SUBST([FONTCONFIG_LIBS])


dnl ---------------------------------------------
dnl gdk-pixbuf support
dnl ---------------------------------------------

AC_ARG_ENABLE([gdkpixbuf],
   AS_HELP_STRING([--disable-gdkpixbuf], [do not build gdk-pixbuf support]))

if test "x$enable_gdkpixbuf" != "xno"; then
  PKG_CHECK_MODULES(GDK_PIXBUF, gdk-pixbuf-2.0,
                no_gdkpixbuf=no,
		no_gdkpixbuf=yes)
  AC_SUBST(GDK_PIXBUF_CFLAGS)
  AC_SUBST(GDK_PIXBUF_LIBS)
  if test "x$no_gdkpixbuf" != "xyes"; then
    AC_DEFINE(HAVE_GDK_PIXBUF,1,[Define this if you have gdk-pixbuf installed])
  else
    AC_MSG_RESULT(*** All of the gdk-pixbuf dependent parts will be disabled ***)
  fi
else
  no_gdkpixbuf=yes
fi
AM_CONDITIONAL(HAVE_GDK_PIXBUF, test "x$no_gdkpixbuf" != "xyes")

dnl ---------------------------------------------
dnl ASF build can be optional
dnl ---------------------------------------------

AC_ARG_ENABLE([asf], AS_HELP_STRING([--disable-asf], [do not build ASF demuxer]))
AM_CONDITIONAL(BUILD_ASF, test "x$enable_asf" != "xno")


dnl ---------------------------------------------
dnl FAAD build can be optional
dnl ---------------------------------------------

AC_ARG_ENABLE([faad], AS_HELP_STRING([--disable-faad], [do not build FAAD decoder]))
AM_CONDITIONAL(BUILD_FAAD, test "x$enable_faad" != "xno")

dnl ---------------------------------------------
dnl Optional and external libdts
dnl ---------------------------------------------

AC_ARG_ENABLE(dts, AS_HELP_STRING([--disable-dts], [Disable support for DTS decoding library (default: enabled)]),
              [enable_libdts="$enableval"], [enable_libdts="yes"])
AC_ARG_WITH(external-libdts, AS_HELP_STRING([--with-external-libdts], [use external libdts/libdca library (not recommended)]),
            [external_libdts="$withval"], [external_libdts="no"])

have_dts="no"

if test "x$enable_libdts" = "xno"; then
  AC_MSG_RESULT([libdts support disabled])
elif test "x$external_libdts" = "xyes"; then
  PKG_CHECK_MODULES(LIBDTS, [libdts], have_dts=yes, have_dts=no)
  if test "x$have_dts" = "xno"; then
    AC_MSG_RESULT([*** no usable version of libdts found, using internal copy ***])
  fi
else
  AC_MSG_RESULT([Use included libdts support])
  LIBDTS_CFLAGS='-I$(top_srcdir)/contrib/libdca/include'
  LIBDTS_DEPS='$(top_builddir)/contrib/libdca/libdca.la'
  LIBDTS_LIBS='$(top_builddir)/contrib/libdca/libdca.la'
fi

AC_SUBST(LIBDTS_LIBS)
AC_SUBST(LIBDTS_DEPS)
AC_SUBST(LIBDTS_CFLAGS)

AM_CONDITIONAL(DTS, test "x$enable_libdts" = "xyes")
AM_CONDITIONAL(EXTERNAL_LIBDTS, test "x$have_dts" = "xyes")

dnl ---------------------------------------------
dnl libmodplug support 
dnl ---------------------------------------------
AC_ARG_ENABLE([modplug],
  AS_HELP_STRING([--enable-modplug], [Enable modplug support]) )

if test "x$enable_modplug" != "xno"; then
  PKG_CHECK_MODULES([LIBMODPLUG], [libmodplug >= 0.7],
    AC_DEFINE([HAVE_MODPLUG], 1, [define this if you have libmodplug installed]),
    [enable_modplug=no])
fi

AC_SUBST(LIBMODPLUG_CFLAGS)
AC_SUBST(LIBMODPLUG_LIBS)
dnl AM_CONDITIONAL(HAVE_MODPLUG, [test "x$have_modplug" = x"yes"])

dnl ---------------------------------------------
dnl Wavpack library
dnl ---------------------------------------------
AC_ARG_WITH([wavpack],
   AS_HELP_STRING([--with-wavpack], [Enable Wavpack decoder (requires libwavpack)]) )

if test "x$with_wavpack" = "xyes"; then
   PKG_CHECK_MODULES([WAVPACK], [wavpack], [have_wavpack=yes])
fi

AM_CONDITIONAL([HAVE_WAVPACK], [test "x$have_wavpack" = "xyes"])


dnl --------------------------------------------
dnl Real binary codecs support
dnl --------------------------------------------

AC_ARG_ENABLE([real-codecs],
	AS_HELP_STRING([--disable-real-codecs], [Disable Real binary codecs support]))
AC_ARG_WITH([real-codecs-path],
	AS_HELP_STRING([--with-real-codecs-path=dir], [Specify directory for Real binary codecs]), [
		AC_DEFINE_UNQUOTED([REAL_CODEC_PATH], ["$withval"], [Specified path for Real binary codecs])
	])

dnl On some systems, we cannot enable Real codecs support to begin with.
dnl This includes Darwin, that uses Mach-O rather than ELF.
case $host_or_hostalias in
     *-darwin*) enable_real_codecs="no" ;;
esac

if test "x$enable_real_codecs" != "xno"; then
   dnl For those that have a replacement, break at the first one found
   AC_CHECK_SYMBOLS([__environ _environ environ], [break], [need_weak_aliases=yes])
   AC_CHECK_SYMBOLS([stderr __stderrp], [break], [need_weak_aliases=yes])

   dnl For these there are no replacements
   AC_CHECK_SYMBOLS([___brk_addr __ctype_b])

   if test "x$need_weak_aliases" = "xyes"; then
      CC_ATTRIBUTE_ALIAS(, [AC_MSG_ERROR([You need weak aliases support for Real codecs on your platform])])
   fi
fi

AM_CONDITIONAL([ENABLE_REAL], [test "x$enable_real_codecs" != "xno"])

dnl ---------------------------------------------
dnl For win32 libraries location, needed by libw32dll.
dnl ---------------------------------------------

AC_ARG_WITH([w32-path],
  AS_HELP_STRING([--with-w32-path=path], [location of Win32 binary codecs]),
  [w32_path="$withval"], [w32_path="/usr/lib/codecs"])
AC_SUBST(w32_path)

AC_ARG_ENABLE([w32dll],
  AS_HELP_STRING([--disable-w32dll], [Disable Win32 DLL support]),
  , [enable_w32dll=$with_gnu_as])

case $host_or_hostalias in
   *-mingw* | *-cygwin)
     enable_w32dll="no" ;;
   i?86-* | k?-* | athlon-* | pentium*-)
     if test "x$enable_w32dll" != "xno"; then
	CC_PROG_AS
     fi
     test "x$enable_w32dll" = "x" && \
       enable_w32dll="$with_gnu_as"
     ;;
   *)
     enable_w32dll="no" ;;
esac

if test "x$enable_w32dll" = "xyes" && \
   test "x$with_gnu_as" = "xno"; then

   AC_MSG_ERROR([You need GNU as to enable Win32 codecs support])
fi

AM_CONDITIONAL(HAVE_W32DLL, test "x$enable_w32dll" != "xno")


dnl ---------------------------------------------
dnl XINE_ROOTDIR does not work if architecture independent files are 
dnl installed to another place than architecture dependent files !!!
dnl ---------------------------------------------

dnl
dnl installation directories and directories relative to prefix
dnl
dnl Note:
dnl   use AC_DEFINE for runtime
dnl   use AC_SUBST for installation
dnl

makeexpand () {
  local i
  local j
  i="$1"
  while test "$i" != "$j"; do j="$i"; eval i="$j"; done
  echo "$i"
}

XINE_PLUGINDIR="$libdir/xine/plugins/$XINE_MAJOR.$XINE_MINOR.$XINE_SUB"
XINE_FONTDIR="${datadir}/xine/libxine$XINE_MAJOR/fonts"
XINE_LOCALEDIR="${datadir}/locale"
XINE_REL_PLUGINDIR="`makeexpand "$XINE_PLUGINDIR"`"
XINE_REL_PLUGINDIR="`makeexpand "$XINE_REL_PLUGINDIR" | sed -e "s,^${prefix}/,,"`"
XINE_REL_FONTDIR="`makeexpand "$XINE_FONTDIR" | sed -e "s,^${prefix}/,,"`"
XINE_REL_LOCALEDIR="`makeexpand "$XINE_LOCALEDIR" | sed -e "s,^${prefix}/,,"`"
if test "x$WIN32_SYS" = "xmingw32" -o "x$WIN32_SYS" = "xcygwin"; then
  dnl polish paths (MinGW runtime accepts both \ and / anyway)
  XINE_REL_PLUGINDIR="`echo "$XINE_REL_PLUGINDIR" | sed -e 's/\\//\\\\\\\\/g'`"
  XINE_REL_FONTDIR="`echo "$XINE_REL_FONTDIR" | sed -e 's/\\//\\\\\\\\/g'`"
  XINE_REL_LOCALEDIR="`echo "$XINE_REL_LOCALEDIR" | sed -e 's/\\//\\\\\\\\/g'`"
  dnl prefix in xine-config
  XINE_CONFIG_PREFIX="\$(cd \$(dirname \$0)/..; pwd)"
  dnl installation directories (in xine-config)
  XINE_PLUGINPATH="$XINE_CONFIG_PREFIX/$XINE_REL_PLUGINDIR"
  XINE_FONTPATH="$XINE_CONFIG_PREFIX/$XINE_REL_FONTDIR"
  XINE_LOCALEPATH="$XINE_CONFIG_PREFIX/$XINE_REL_LOCALEDIR"
  dnl runtime directories
  AC_DEFINE(XINE_PLUGINDIR,[xine_get_plugindir()],[Define this to plugins directory location])
  AC_DEFINE(XINE_FONTDIR,[xine_get_fontdir()],[Define this to osd fonts dir location])
  AC_DEFINE(XINE_LOCALEDIR,[xine_get_localedir()],[Path where catalog files will be.])
else
  dnl prefix in xine-config
  XINE_CONFIG_PREFIX="`makeexpand "${prefix}"`"
  dnl directories from xine-config and runtime directories
  XINE_PLUGINPATH="`makeexpand "$XINE_PLUGINDIR"`"
  XINE_FONTPATH="`makeexpand "$XINE_FONTDIR"`"
  XINE_LOCALEPATH="`makeexpand "$XINE_LOCALEDIR"`"
  dnl defining runtime directories
  AC_DEFINE_UNQUOTED(XINE_PLUGINDIR,"$XINE_PLUGINPATH",[Define this to plugins directory location])
  AC_DEFINE_UNQUOTED(XINE_FONTDIR,"$XINE_FONTPATH",[Define this to osd fonts dir location])
  AC_DEFINE_UNQUOTED(XINE_LOCALEDIR, "$XINE_LOCALEPATH",[Path where catalog files will be.])
fi
AC_DEFINE_UNQUOTED(XINE_REL_PLUGINDIR,"$XINE_REL_PLUGINDIR",[Define this to plugin directory relative to execution prefix])
AC_DEFINE_UNQUOTED(XINE_REL_FONTDIR,"$XINE_REL_FONTDIR",[Define this to font directory relative to prefix])
AC_DEFINE_UNQUOTED(XINE_REL_LOCALEDIR,"$XINE_REL_LOCALEDIR",[Define this to font directory relative to prefix])
AC_SUBST(XINE_CONFIG_PREFIX)
AC_SUBST(XINE_PLUGINPATH)
AC_SUBST(XINE_FONTPATH)
AC_SUBST(XINE_LOCALEPATH)
AC_SUBST(XINE_PLUGINDIR)
AC_SUBST(XINE_FONTDIR)
AC_SUBST(XINE_LOCALEDIR)

dnl Where aclocal m4 files should be installed
XINE_ACFLAGS="-I `makeexpand "${datarootdir}/aclocal"`"
AC_DEFINE_UNQUOTED(XINE_ACFLAGS, "$XINE_ACFLAGS", [Path where aclocal m4 files will be.])
AC_SUBST(XINE_ACFLAGS)

dnl Where architecture independent data (e.g. logo) will/should be installed
XINE_DATADIR="`makeexpand "${datarootdir}/xine"`"
AC_SUBST(XINE_DATADIR)

dnl Where scripts will/should be installed.
eval XINE_SCRIPTPATH="$XINE_DATADIR/xine/scripts"
AC_SUBST(XINE_SCRIPTPATH)


dnl ---------------------------------------------
dnl Get where .m4 should be installed.
dnl ---------------------------------------------

dnl if test "x${ACLOCAL_DIR+set}" != xset; then
dnl   case "`id`" in
dnl     uid=0\(* )
dnl       AC_MSG_CHECKING(for aclocal directory)
dnl       if (aclocal --version) < /dev/null > /dev/null 2>&1; then
dnl         ACLOCAL_DIR="`eval $ACLOCAL --print-ac-dir`"
dnl         AC_MSG_RESULT($ACLOCAL_DIR)
dnl       else
dnl         ACLOCAL_DIR="${prefix}/share/aclocal"
dnl         AC_MSG_RESULT(none - will be installed in $ACLOCAL_DIR)
dnl       fi
dnl       escapedprefix="`echo $prefix | sed -e 's/\\//\\\\\//g'`"
dnl       ACLOCAL_DIR="`echo $ACLOCAL_DIR|sed -e 's/^'$escapedprefix/'\${prefix}'/`"
dnl       ;;
dnl   esac
dnl fi
AC_SUBST(ACLOCAL_DIR)
AM_CONDITIONAL([INSTALL_M4],[test "x$ACLOCAL_DIR" != "x"])


dnl this is an internal function we should not use, but
dnl as long as neither autoconf nor automake offer an A[CM]_PROG_OBJC
dnl check we will have to call it
_AM_DEPENDENCIES([OBJC])

AM_CONDITIONAL([BUILD_DMX_IMAGE], [test "x$have_imagemagick" = "xyes" -o "x$no_gdkpixbuf" != "xyes"])

dnl ---------------------------------------------
dnl some include paths ( !!! DO NOT REMOVE !!! )
dnl ---------------------------------------------

INCLUDES='-I$(top_srcdir) -I$(top_builddir)/include -I$(top_srcdir)/include -I$(top_srcdir)/src -I$(top_builddir)/src/xine-engine -I$(top_srcdir)/src/xine-engine -I$(top_srcdir)/src/xine-utils $(INTLDIR) -I$(top_builddir)/src/input -I$(top_srcdir)/src/input $(WIN32_CPPFLAGS) -I$(top_builddir)/lib -I$(top_srcdir)/lib'
AC_SUBST(INCLUDES)


dnl Common cflags for all platforms
CFLAGS="$UNIVERSAL_CFLAGS \$(MULTIPASS_CFLAGS) $miscflags $warnflags $CFLAGS"
LDFLAGS="$UNIVERSAL_LDFLAGS $LDFLAGS"
OBJCFLAGS="$UNIVERSAL_CFLAGS $miscflags $warnflags $OBJCFLAGS"


dnl Some informations about xine-lib compilation for xine-config
XINE_BUILD_CC="`$CC -v 2>&1 | tail -1 2>/dev/null`"
XINE_BUILD_OS="`uname -s -r -m`"
XINE_BUILD_DATE="`date \"+%a %d %b %Y %T\"`"
AC_SUBST(XINE_BUILD_CC)
AC_SUBST(XINE_BUILD_OS)
AC_SUBST(XINE_BUILD_DATE)


dnl ---------------------------------------------
dnl Output configuration files
dnl ---------------------------------------------

AC_CONFIG_FILES([
Makefile
doc/Makefile
doc/man/Makefile
doc/man/en/Makefile
doc/hackersguide/Makefile
doc/faq/Makefile
doc/Doxyfile
contrib/Makefile
contrib/libdca/Makefile
contrib/libmpcdec/Makefile
contrib/libxdg-basedir/Makefile
include/Makefile
include/xine.h
lib/Makefile
m4/Makefile
m4/gettext/Makefile
misc/Makefile
misc/SlackBuild
misc/build_rpms.sh
misc/fonts/Makefile
misc/libxine.pc
misc/relchk.sh
misc/xine-config
misc/xine-lib.spec
po/Makefile.in
src/Makefile
src/audio_out/Makefile
src/combined/Makefile
src/demuxers/Makefile
src/dxr3/Makefile
src/input/Makefile
src/input/libdvdnav/Makefile
src/input/dvb/Makefile
src/input/librtsp/Makefile
src/input/libreal/Makefile
src/input/vcd/Makefile
src/input/vcd/libcdio/Makefile
src/input/vcd/libcdio/cdio/Makefile
src/input/vcd/libcdio/MSWindows/Makefile
src/input/vcd/libcdio/image/Makefile
src/input/vcd/libvcd/Makefile
src/input/vcd/libvcd/libvcd/Makefile
src/liba52/Makefile
src/libfaad/Makefile
src/libfaad/codebook/Makefile
src/libffmpeg/Makefile
src/libmad/Makefile
src/libmpeg2/Makefile
src/libspudec/Makefile
src/libspucc/Makefile
src/libspucmml/Makefile
src/libspudvb/Makefile
src/libsputext/Makefile
src/libw32dll/Makefile
src/libw32dll/wine/Makefile
src/libw32dll/DirectShow/Makefile
src/libw32dll/dmo/Makefile
src/libw32dll/qtx/Makefile
src/libw32dll/qtx/qtxsdk/Makefile
src/libxinevdec/Makefile
src/libxineadec/Makefile
src/libxineadec/gsm610/Makefile
src/libxineadec/nosefart/Makefile
src/libreal/Makefile
src/post/Makefile
src/post/planar/Makefile
src/post/goom/Makefile
src/post/mosaico/Makefile
src/post/visualizations/Makefile
src/post/audio/Makefile
src/post/deinterlace/Makefile
src/post/deinterlace/plugins/Makefile
src/video_out/Makefile
src/video_out/libdha/Makefile
src/video_out/libdha/bin/Makefile
src/video_out/libdha/kernelhelper/Makefile
src/video_out/libdha/oth/Makefile
src/video_out/libdha/sysdep/Makefile
src/video_out/macosx/Makefile
src/video_out/vidix/Makefile
src/video_out/vidix/drivers/Makefile
src/xine-utils/Makefile
src/xine-engine/Makefile
win32/Makefile
win32/include/Makefile])
AC_CONFIG_COMMANDS([default],[[chmod +x ./misc/SlackBuild ./misc/build_rpms.sh ./misc/relchk.sh]],[[]])
AC_OUTPUT

dnl ---------------------------------------------
dnl Work around a suspected bug in libtool:
dnl
dnl Remove excessive trailing slash from search dir names in the libtool script.
dnl It occurs in dir names obtained by 'gcc -print-search-dirs' in the created
dnl configure script and causes a test on dir names in libtool to fail,
dnl leading to confusing (but harmless) 'warning: <lib> seems to be moved'.
dnl
dnl This should be fixed in the libtool package itself as all other dir names
dnl there have no trailing slash.
dnl ---------------------------------------------

dnl Note: Brackets [] must be doubled as they are treated as m4 macro quotes.
cat libtool | sed -e '/sys_lib_search_path_spec=/s/\/\([[ "]]\)/\1/g' > libtool.tmp
mv -f libtool.tmp libtool
chmod +x libtool

XINE_LIB_SUMMARY
