dnl
dnl M4 macro to add support for extra optimizations
dnl
dnl It understand --enable/--disable-optimizations .
dnl when optimizations are disabled, it does not touch cflags
dnl
dnl Note: always disable while crosscompiling
dnl

AC_DEFUN([AC_OPTIMIZATIONS], [
  AC_ARG_ENABLE([optimizations],
    AS_HELP_STRING([--disable-optimizations], [Don't try to guess what optimization to enable]))

  if test "x$enable_optimizations" != "xno"; then
    INLINE_FUNCTIONS=-finline-functions

    if test "$GCC" = yes; then
        dnl
        dnl check cflags not supported by all gcc versions
        CC_CHECK_CFLAGS([-fschedule-insns2], f_si="-fschedule-insns2", f_si="")
        CC_CHECK_CFLAGS([-mwide-multiply], m_wm="-mwide-multiply", m_wm="")
        dnl
        dnl gcc 3.1 uses the -f version
        dnl
        CC_CHECK_CFLAGS([-falign-functions=4], f_af="-falign-functions=4",
            f_af="-malign-functions=4")
        CC_CHECK_CFLAGS([-falign-loops=4], f_al="-falign-loops=4",
            f_al="-malign-loops=4")
        CC_CHECK_CFLAGS([-falign-jumps=4], f_aj="-falign-jumps=4",
            f_aj="-malign-jumps=4")
        dnl
        dnl Check for some optimization disabling
        dnl needed for win32 code
        dnl
        CC_CHECK_CFLAGS([-fno-omit-frame-pointer], W32_NO_OPTIMIZE="$W32_NO_OPTIMIZE -fno-omit-frame-pointer",)
        CC_CHECK_CFLAGS([-fno-inline-functions], W32_NO_OPTIMIZE="$W32_NO_OPTIMIZE -fno-inline-functions",)
        CC_CHECK_CFLAGS([-fno-rename-registers], W32_NO_OPTIMIZE="$W32_NO_OPTIMIZE -fno-rename-registers",)
        AC_SUBST(W32_NO_OPTIMIZE)
        dnl
        dnl Multipass compilation
        dnl
        CC_CHECK_CFLAGS([-fprofile-arcs], PASS1_CFLAGS="-fprofile_arcs $PASS1_CFLAGS",)
        CC_CHECK_CFLAGS([-fbranch-probabilities], PASS2_CFLAGS="-fbranch-probabilities $PASS2_CFLAGS",)
        AC_SUBST(PASS1_CFLAGS)
        AC_SUBST(PASS2_CFLAGS)
        dnl
        dnl Warnings
        dnl
        CFLAGS="-Wchar-subscripts -Wmissing-declarations -Wmissing-prototypes $CFLAGS"
        CFLAGS="-Wnested-externs -Wcast-align $CFLAGS"
        dnl some combinations of gcc+glibc produce useless warnings on memset
        dnl when compiling with -Wpointer-arith, so we check for this first
        AC_MSG_CHECKING(for sane -Wpointer-arith)
        SAVE_CFLAGS="$CFLAGS"
        CFLAGS="-O2 -Wpointer-arith -Werror $CFLAGS"
        AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <string.h>]], [[int a; memset(&a, 0, sizeof(int));]])],
            [AC_MSG_RESULT(yes); CFLAGS="-Wpointer-arith $SAVE_CFLAGS"],
            [AC_MSG_RESULT(no);  CFLAGS="$SAVE_CFLAGS"]);

        dnl gcc 3.3.5 (at least) is known to be buggy wrt optimisation and
        dnl -finline-functions. Use -fno-inline-functions for gcc < 3.4.0.

        AC_MSG_CHECKING(for gcc 3.4.0 or later)
        newGCC="`"$CC" -dumpversion |
                awk -F. '{ if ((@S|@1 * 10000 + @S|@2 * 100 + @S|@3) >= 30400) { print "yes" } }'
                `"
        AC_MSG_RESULT(${newGCC:-no - assuming bugginess in -finline-functions})
        test "$newGCC" = yes || INLINE_FUNCTIONS=-fno-inline-functions
    fi

    dnl Flags not supported by all *cc* variants
    CC_CHECK_CFLAGS([-Wall], wall="-Wall", wall="")

    CFLAGS="$wall ${CFLAGS}"
    DEBUG_CFLAGS="$wall ${DEBUG_CFLAGS}"

    case "$host_or_hostalias" in
      i?86-* | k?-* | athlon-* | pentium*)
        if test "$GCC" = yes -o "${CC##*/}x" = "iccx" ; then

          if test "$GCC" = yes; then
            dnl Check for gcc cpu optimization support
            CC_CHECK_CFLAGS("-mtune=i386",
              sarchopt="-mtune",
              CC_CHECK_CFLAGS("-mcpu=i386",
                sarchopt="-mcpu",
                CC_CHECK_CFLAGS("-march=i386",
                  sarchopt="-march",
                  [ AC_MSG_RESULT(** no cpu optimization supports **)
                    sarchopt=no
                  ]
                )
              )
            )

            dnl special check for k7 cpu CC support
            CC_CHECK_CFLAGS([$sarchopt=athlon], k7cpu="athlon", k7cpu="i686")

            dnl add x86 specific gcc CFLAGS
            CFLAGS="-O3 -pipe -fomit-frame-pointer $f_af $f_al $f_aj $m_wm $m_psb -fexpensive-optimizations $f_si -ffast-math $INLINE_FUNCTIONS $CFLAGS"

            DEBUG_CFLAGS="-O $DEBUG_CFLAGS"

            if test x"$sarchopt" != "xno"; then
              archopt_val=

              case "$host_or_hostalias" in
              i386-*)
                  archopt_val="i386" ;;
              i486-*)
                  archopt_val="i486" ;;
              i586-*)
                  archopt_val="pentium"
                  ;;
              pentium-mmx-*)
                  archopt_val="pentium-mmx"
                  ;;
              pentiumpro-* | pentium2-* | i686-*)
                  archopt_val="pentiumpro"
                  if test x"$check_athlon" = "xyes"; then
                      if test -f /proc/cpuinfo; then
                          modelname=`cat /proc/cpuinfo | grep "model\ name\	:" | sed -e 's/ //g' | cut -d':' -f2`
                          case "$modelname" in
                          *Athlon* | *Duron* | *K7*)
                              archopt_val="$k7cpu"
                              ;;
                          VIAEzra)
                              archopt_val="c3"
                              ;;
                          esac
                      fi
                  fi
                  ;;
              k6-2-* | k6-3-*)
                  archopt_val="k6-2"
                  ;;
              k6-*)
                  archopt_val="k6"
                  ;;
              pentium3-*)
                  archopt_val="pentium3"
                  ;;
              pentium4-*)
                  archopt_val="pentium4"
                  ;;
              athlon-4-* | athlon-xp-* | athlon-mp-*)
                  archopt_val="athlon-4"
                  ;;
              k7-* | athlon-tbird-* | athlon-*)
                  archopt_val="athlon"
                  ;;

              esac
              if test x"$archopt_val" != x; then
                  CFLAGS="$sarchopt=$archopt_val $CFLAGS"
                  DEBUG_CFLAGS="$sarchopt=$archopt_val $DEBUG_CFLAGS"
              fi
            fi
          else
            dnl we have the Intel compiler
            CFLAGS="-unroll -ipo -ipo_obj -O3 $CFLAGS"
            PASS1_CFLAGS="-prof_genx -prof_dir \$(PWD)/\$(top_builddir)/ $PASS1_CFLAGS"
            PASS2_CFLAGS="-prof_use -prof_dir \$(PWD)/\$(top_builddir)/ $PASS2_CFLAGS"
            AC_SUBST(PASS1_CFLAGS)
            AC_SUBST(PASS2_CFLAGS)
          fi

        else
            dnl add x86 specific cc CFLAGS
            CFLAGS="-O $CFLAGS"
            DEBUG_CFLAGS="-O $DEBUG_CFLAGS"
            AC_DEFINE_UNQUOTED(FPM_64BIT,,[Define to select libmad fixed point arithmetic implementation])
        fi
        ;;
      alphaev56-*)
        CFLAGS="-O3 -mcpu=ev56 -mieee $CFLAGS"
        DEBUG_CFLAGS="-O3 -mcpu=ev56 -mieee $DEBUG_CFLAGS"
        ;;
      alpha*)
        CFLAGS="-O3 -mieee $CFLAGS"
        DEBUG_CFLAGS="-O3 -mieee $DEBUG_CFLAGS"
        ;;
      *darwin*)
        CFLAGS="-O3 -pipe -fomit-frame-pointer $m_wm $m_psb -fexpensive-optimizations $f_si -ffast-math $INLINE_FUNCTIONS -no-cpp-precomp -D_INTL_REDIRECT_MACROS $CFLAGS"
        DEBUG_CFLAGS="-O3 $DEBUG_CFLAGS"
        ;;
      ppc-*-linux* | powerpc-*)
        CFLAGS="-O3 -pipe -fomit-frame-pointer $m_wm $m_psb -fexpensive-optimizations $f_si -ffast-math $INLINE_FUNCTIONS $CFLAGS"
        DEBUG_CFLAGS="-O3 $DEBUG_CFLAGS"
        ;;
      sparc*-*-linux*)
        CFLAGS="-O3 $cpu_cflags $INLINE_FUNCTIONS $CFLAGS"
        DEBUG_CFLAGS="-O $cpu_cflags $INLINE_FUNCTIONS $DEBUG_CFLAGS"

        case `uname -m` in
          sparc)
            cpu_cflags="-mcpu=supersparc -mtune=supersparc" ;;
          sparc64)
            cpu_cflags="-mcpu=ultrasparc -mtune=ultrasparc" ;;
        esac
        ;;
      sparc-*-solaris*)
        if test "$GCC" = yes; then
          case `uname -m` in
            sun4c) cpu_cflags="-mcpu=v7 -mtune=supersparc" ;;
            sun4m) cpu_cflags="-mcpu=v8 -mtune=supersparc" ;;
            sun4u)
              case `$CC --version 2>/dev/null` in
                1.*|2.*)
                  # -mcpu=ultrasparc triggers a GCC 2.95.x compiler bug when
                  # compiling video_out.c:
                  #   gcc: Internal compiler error: program cc1 got fatal signal 11
                  # avoid -mcpu=ultrasparc with gcc 2.*
                  cpu_cflags="-mcpu=v8 -mtune=ultrasparc"
                  ;;
                *)
                  # GCC 3 or newer should have no problem with -mcpu=ultrasparc
                  cpu_cflags="-mcpu=ultrasparc -mtune=ultrasparc"
                  ;;
              esac
            ;;
          esac
          cc_optimize_cflags="-O3 $cpu_cflags $INLINE_FUNCTIONS"
          cc_debug_cflags="-O $cpu_cflags $INLINE_FUNCTIONS"
        else
          case `uname -m` in
            sun4c) cpu_cflags="-xarch=v7" ;;
            sun4m) cpu_cflags="-xarch=v8" ;;
            sun4u) cpu_cflags="-xarch=v8plusa" ;;
          esac
          cc_optimize_cflags="-fast $cpu_cflags -xCC"
          cc_debug_cflags="-O"
        fi

        CFLAGS="$cc_optimize_cflags $CFLAGS"
        DEBUG_CFLAGS="$cc_debug_cflags $DEBUG_CFLAGS"
        ;;
      x86_64-*)
        CFLAGS="-O3 -fomit-frame-pointer $m_wm $m_psb -fexpensive-optimizations $f_si -ffast-math $INLINE_FUNCTIONS $CFLAGS"
        DEBUG_CFLAGS="-g $DEBUG_CFLAGS"
        ;;
      armv4l-*-linux*)
        CFLAGS="-O2 -fsigned-char -ffast-math -mcpu=strongarm1100 -fomit-frame-pointer -fthread-jumps -fregmove $CFLAGS"
        dnl    CFLAGS="-O1 -fforce-mem -fforce-addr -fthread-jumps -fcse-follow-jumps -fcse-skip-blocks -fexpensive-optimizations -fregmove -fschedule-insns2 $INLINE_FUNCTIONS -fsigned-char -fomit-frame-pointer -march=armv4 -mtune=strongarm $CFLAGS"
        DEBUG_CFLAGS="-O2 $DEBUG_CFLAGS"
        ;;
    esac
  fi
])

dnl Kate modeline: leave at the end
dnl kate: indent-width 2; replace-trailing-space-save 1; space-indent 1; backspace-indents 1;
