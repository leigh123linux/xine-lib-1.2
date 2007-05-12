dnl Configure paths for ARTS
dnl Philip Stadermann   2001-06-21
dnl stolen from esd.m4

dnl AM_PATH_ARTS([MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl Test for ARTS, and define ARTS_CFLAGS and ARTS_LIBS
dnl
AC_DEFUN([AM_PATH_ARTS], [
    AC_ARG_WITH([arts-prefix],
                [AS_HELP_STRING([--with-arts-prefix=DIR], [prefix where ARTS is installed (optional)])],
                [arts_prefix="$withval"], [arts_prefix=""])
    AC_ARG_ENABLE([artstest],
                  [AS_HELP_STRING([--disable-artstest], [do not try to compile and run a test ARTS program])],
                  [], [enable_artstest=yes])

    AC_LANG_PUSH([C])

    if test x"$arts_prefix" != x""; then
        arts_args="$arts_args --arts-prefix=$arts_prefix"
        test x"${ARTS_CONFIG+set}" != x"set" && ARTS_CONFIG="$arts_prefix/bin/artsc-config"
    fi

    min_arts_version=ifelse([$1], , [0.9.5], [$1])
    if test x"$enable_artstest" = x"no"; then
        AC_MSG_CHECKING([for ARTS artsc - version >= $min_arts_version])
    else
        AC_PATH_TOOL([ARTS_CONFIG], [artsc-config], [no])
        AC_MSG_CHECKING([for ARTS artsc - version >= $min_arts_version])

        if test x"$ARTS_CONFIG" = x"no"; then
            with_arts=no
        else
            ARTS_CFLAGS=`$ARTS_CONFIG $artsconf_args --cflags`
            ARTS_LIBS=`$ARTS_CONFIG $artsconf_args --libs`
            arts_major_version=`$ARTS_CONFIG $arts_args --version | \
                    sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\1/'`
            arts_minor_version=`$ARTS_CONFIG $arts_args --version | \
                    sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\2/'`
            arts_micro_version=`$ARTS_CONFIG $arts_config_args --version | \
                    sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\3/'`
            ac_save_CFLAGS="$CFLAGS" CFLAGS="$CFLAGS $ARTS_CFLAGS"
            ac_save_LIBS="$LIBS" LIBS="$ARTS_LIBS $LIBS"

            # Now check if the installed ARTS is sufficiently new. (Also sanity
            # checks the results of arts-config to some extent)

            rm -f conf.artstest
            AC_RUN_IFELSE([AC_LANG_SOURCE([[
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <artsc.h>

int main(int argc, char *argv[])
{
    int     major, minor, micro;
    char    *tmp_version;
    FILE    *fp;

    if ((fp = fopen("conf.artstest", "w")) != NULL) {
        fclose(fp);
    }
    else {
        printf("*** could not write to file conf.artstest\n");
        exit(1);
    }

    /* HP/UX 9 (%@#!) writes to sscanf strings */
    tmp_version = strdup("$min_arts_version");
    if (!tmp_version || sscanf(tmp_version, "%d.%d.%d", &major, &minor, &micro) != 3) {
        printf("%s, bad version string\n", "$min_arts_version");
        exit(1);
    }

    if (($arts_major_version > major) ||
        (($arts_major_version == major) && ($arts_minor_version > minor)) ||
        (($arts_major_version == major) && ($arts_minor_version == minor) &&
         ($arts_micro_version >= micro)))
    {
        return 0;
    }
    printf("\n*** 'artsc-config --version' returned %d.%d.%d, but the minimum version\n", $arts_major_version, $arts_minor_version, $arts_micro_version);
    printf("*** of ARTS required is %d.%d.%d. If artsc-config is correct, then it is\n", major, minor, micro);
    printf("*** best to upgrade to the required version.\n");
    printf("*** If artsc-config was wrong, set the environment variable ARTS_CONFIG\n");
    printf("*** to point to the correct copy of artsc-config, and remove the file\n");
    printf("*** config.cache before re-running configure\n");

    return 1;
}
            ]])], [], [with_arts=no], [with_arts=cc])
            if test x"$with_arts" = x"cc"; then
                AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <stdio.h>
                                                  #include <artsc.h>]], [[return 0]])],
                               [with_arts=yes], [with_arts=no])
            fi
            CFLAGS="$ac_save_CFLAGS"
            LIBS="$ac_save_LIBS"
        fi
    fi

    if test x"$with_arts" != x"no"; then
        AC_MSG_RESULT([yes])
        ifelse([$2], , :, [$2])
    else
        AC_MSG_RESULT([no])
        if test x"$enable_artstest" != x"no"; then
            if test x"$ARTS_CONFIG" = x"no"; then
                echo "*** The arts-config script installed by aRts could not be found"
                echo "*** If aRts was installed in PREFIX, make sure PREFIX/bin is in"
                echo "*** your path, or set the ARTS_CONFIG environment variable to the"
                echo "*** full path to arts-config."
            else
                if test ! -f conf.artstest ; then
                    echo "*** Could not run aRts test program, checking why..."
                    CFLAGS="$CFLAGS $ARTS_CFLAGS"
                    LIBS="$ARTS_LIBS $LIBS"
                    AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <stdio.h>
                                                      #include <artsc.h>]], [[return 0]])],
                                   [echo "*** The test program compiled, but did not run. This usually means"
                                    echo "*** that the run-time linker is not finding aRts or finding the wrong"
                                    echo "*** version of aRts. If it is not finding aRts, you'll need to set your"
                                    echo "*** LD_LIBRARY_PATH environment variable, or edit /etc/ld.so.conf to point"
                                    echo "*** to the installed location. Also, make sure you have run ldconfig if that"
                                    echo "*** is required on your system."
                                    echo "***"
                                    echo "*** If you have an old version installed, it is best to remove it, although"
                                    echo "*** you may also be able to get things to work by modifying LD_LIBRARY_PATH."
                                    echo "***"],
                                   [echo "*** The test program failed to compile or link. See the file config.log for the"
                                    echo "*** exact error that occurred. This usually means aRtS was incorrectly installed"
                                    echo "*** or that you have moved aRts since it was installed. In the latter case, you"
                                    echo "*** may want to edit the arts-config script: $ARTS_CONFIG"])
                    CFLAGS="$ac_save_CFLAGS"
                    LIBS="$ac_save_LIBS"
                else
                    rm -f conf.artstest
                fi
            fi
        fi
        ARTS_CFLAGS=""
        ARTS_LIBS=""
        ifelse([$3], , :, [$3])
    fi
    AC_SUBST(ARTS_CFLAGS)
    AC_SUBST(ARTS_LIBS)
    AC_LANG_POP([C])
])
