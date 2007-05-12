dnl ---------------------------
dnl Decoder and Demuxer Plugins
dnl ---------------------------
AC_DEFUN([XINE_DECODER_PLUGINS], [

    dnl a52dec (optional; enabled by default; external version allowed)
    AC_ARG_ENABLE([a52dec],
                  [AS_HELP_STRING([--disable-a52dec], [Disable support for a52dec decoding library (default: enabled)])],
                  [], [enable_a52dec=yes])
    AC_ARG_WITH([external-a52dec],
                [AS_HELP_STRING([--with-external-a52dec], [use external a52dec library (not recommended)])],
                [external_a52dec="$withval"], [external_a52dec="no"])
    if test x"$enable_a52dec" != x"no"; then
        dnl REVISIT: --with-external-a52dec=PREFIX
        if test x"$external_a52dec" != x"no"; then
            AC_CHECK_LIB([a52], [a52_init],
                         [AC_CHECK_HEADERS([a52dec/a52.h a52dec/a52_internal.h], [have_a52=yes], [have_a52=no],
                                           [#ifdef HAVE_SYS_TYPES_H
                                            # include <sys/types.h>
                                            #endif
                                            #ifdef HAVE_INTTYPES_H
                                            # include <inttypes.h>
                                            #endif
                                            #ifdef HAVE_STDINT_H
                                            # include <stdint.h>
                                            #endif
                                            #include <a52dec/a52.h>])], [have_a52=no], [-lm])
            if test x"$have_a52" = x"no"; then
                AC_MSG_RESULT([*** no usable version of a52dec found, using internal copy ***])
            fi
        else
            AC_MSG_RESULT([Using included a52dec support])
        fi
    fi
    AM_CONDITIONAL([A52], [test x"$enable_a52dec" != x"no"])
    AM_CONDITIONAL([EXTERNAL_A52DEC], [test x"$have_a52" = x"yes"])


    dnl ASF (optional; enabled by default)
    AC_ARG_ENABLE([asf],
                  [AS_HELP_STRING([--disable-asf], [do not build ASF demuxer])],
                  [], [enable_asf=yes])
    AM_CONDITIONAL([BUILD_ASF], [test x"$enable_asf" != x"no"])


    dnl FAAD (optional; enabled by default)
    AC_ARG_ENABLE([faad],
                  [AS_HELP_STRING([--disable-faad], [do not build FAAD decoder])],
                  [], [enable_faad=yes])
    AM_CONDITIONAL([BUILD_FAAD], [test x"$enable_faad" != x"no"])


    dnl ffmpeg (required; external version allowed)
    AC_ARG_WITH([external-ffmpeg],
                [AS_HELP_STRING([--with-external-ffmpeg], [use external ffmpeg library])],
                [], [with_external_ffmpeg=no])
    case x"$with_external_ffmpeg" in
        x"yes")
            PKG_CHECK_MODULES([FFMPEG], [libavcodec >= 51.20.0])
            ;;
        x"soft")
            PKG_CHECK_MODULES([FFMPEG], [libavcodec >= 51.20.0],
                              [with_external_ffmpeg=yes], [with_external_ffmpeg=no])
            ;;
    esac
    if test "x$with_external_ffmpeg" = "xyes"; then
        PKG_CHECK_MODULES([FFMPEG_POSTPROC], [libpostproc])
        AC_DEFINE([HAVE_FFMPEG], 1, [Define this if you have ffmpeg library])
   
        AC_MSG_NOTICE([
*********************************************************************
xine is configured with external ffmpeg.

This requires the same version of ffmpeg what is included in xine and
you should know what you do. If some problems occur, please try to
use internal ffmpeg.
*********************************************************************])
    else
        AC_MSG_RESULT([using included ffmpeg])
        AC_ARG_ENABLE([ffmpeg_uncommon_codecs],
                      [AS_HELP_STRING([--disable-ffmpeg-uncommon-codecs], [don't build uncommon ffmpeg codecs])],
                      [], [enable_ffmpeg_uncommon_codecs=yes])
        AM_CONDITIONAL([FFMPEG_DISABLE_UNCOMMON_CODECS], [test x"$enable_ffmpeg_uncommon_codecs" = x"no"])
        AC_ARG_ENABLE([ffmpeg_popular_codecs],
                      [AS_HELP_STRING([--disable-ffmpeg-popular-codecs], [don't build popular ffmpeg codecs])],
                      [], [enable_ffmpeg_popular_codecs=yes])
        AM_CONDITIONAL([FFMPEG_DISABLE_POPULAR_CODECS], [test x"$enable_ffmpeg_popular_codecs" = x"no"])
    fi
    AM_CONDITIONAL([WITH_EXTERNAL_FFMPEG], [test x"$with_external_ffmpeg" = x"yes"])


    dnl gdk-pixbuf (optional; enabled by default)
    AC_ARG_ENABLE([gdkpixbuf],
                  [AS_HELP_STRING([--disable-gdkpixbuf], [do not build gdk-pixbuf support])],
                  [], [enable_gdkpixbuf=yes])
    if test x"$enable_gdkpixbuf" != x"no"; then
        PKG_CHECK_MODULES([GDK_PIXBUF], [gdk-pixbuf-2.0], [no_gdkpixbuf=no], [no_gdkpixbuf=yes])
        if test x"$no_gdkpixbuf" != x"yes"; then
            AC_DEFINE([HAVE_GDK_PIXBUF], 1, [Define this if you have gdk-pixbuf installed])
        fi
    else
        no_gdkpixbuf=yes
    fi
    AM_CONDITIONAL([ENABLE_GDK_PIXBUF], [test x"$no_gdkpixbuf" != x"yes"])


    dnl ImageMagick (optional; enabled by default)
    AC_ARG_WITH([imagemagick],
                [AS_HELP_STRING([--without-imagemagick], [Build without ImageMagick image decoder])],
                [], [with_imagemagic=yes])
    if test x"$with_imagemagick" != x"no"; then
        PKG_CHECK_MODULES([WAND], [Wand], [have_imagemagick=yes], [have_imagemagick=no])
        if test x"$with_imagemagick" = x"yes" && test x"$have_imagemagick" = x"no"; then
            AC_MSG_ERROR([ImageMagick support requested, but Wand not found])
        elif test x"$have_imagemagick" = x"yes"; then
            AC_DEFINE([HAVE_WAND], 1, [Define this if you have ImageMagick installed])
        fi
    fi
    AM_CONDITIONAL([ENABLE_IMAGEMAGICK], [test x"$have_imagemagick" != x"no"])


    dnl libdts (optional; enabled by default; external version allowed)
    AC_ARG_ENABLE([dts],
                  [AS_HELP_STRING([--disable-dts], [Disable support for DTS decoding library (default: enabled)])],
                  [], [enable_dts=yes])
    AC_ARG_WITH([external-libdts],
                [AS_HELP_STRING([--with-external-libdts], [use external libdts/libdca library (not recommended)])],
                [], [with_external_libdts=no])
    if test x"$enable_dts" != x"no"; then
        if test x"$with_external_libdts" != x"no"; then
            PKG_CHECK_MODULES([LIBDTS], [libdts], [have_dts=yes], [have_dts=no])
            if test x"$have_dts" = x"no"; then
                AC_MSG_RESULT([*** no usable version of libdts found, using internal copy ***])
            fi
        else
            AC_MSG_RESULT([Using included libdts support])
            LIBDTS_CFLAGS='-I$(top_srcdir)/contrib/libdca/include'
            LIBDTS_DEPS='$(top_builddir)/contrib/libdca/libdca.la'
            LIBDTS_LIBS='$(top_builddir)/contrib/libdca/libdca.la'
            AC_SUBST(LIBDTS_CFLAGS)
            AC_SUBST(LIBDTS_DEPS)
            AC_SUBST(LIBDTS_LIBS)
        fi
    fi
    AM_CONDITIONAL([ENABLE_DTS], [test x"$enable_dts" != x"no"])
    AM_CONDITIONAL([WITH_EXTERNAL_LIBDTS], [test x"$have_dts" = x"yes"])


    dnl libFLAC (optional; disabled by default)
    AC_ARG_WITH([libflac],
                [AS_HELP_STRING([--with-libflac], [build libFLAC-based decoder and demuxer])],
                [], [enable_libflac=no])
    if test x"$with_libflac" != x"no"; then
        AM_PATH_LIBFLAC([have_libflac=yes])
    fi
    AM_CONDITIONAL([ENABLE_LIBFLAC], [test x"$have_libflac" = x"yes"])


    dnl libmad (optional; enabled by default; external version allowed)
    AC_ARG_ENABLE([mad],
                  [AS_HELP_STRING([--disable-mad], [Disable support for MAD decoding library (default: enabled)])],
                  [], [enable_mad=yes])
    AC_ARG_WITH([external-libmad],
                [AS_HELP_STRING([--with-external-libmad], [use external libmad library (not recommended)])],
                [], [with_external_libmad=no])
    if test x"$enable_mad" != x"no"; then
        if test x$"with_external_libmad" != x"no"; then
            PKG_CHECK_MODULES([LIBMAD], [mad],
                              [AC_CHECK_HEADERS([mad.h], [], [with_external_libmad=no])], [with_external_libmad=no])
            if test x$"with_external_libmad" = x"no"; then
                AC_MSG_RESULT([*** no usable version of libmad found, using internal copy ***])
            fi
        fi
        if test x$"with_external_libmad" != x"no"; then
            AC_MSG_RESULT([Using included libmad support])
            case "$host_or_hostalias" in
                i?86-* | k?-* | athlon-* | pentium*-)
                    AC_DEFINE([FPM_INTEL], 1, [Define to select libmad fixed point arithmetic implementation])
                    ;;
                x86_64-*)
                    AC_DEFINE([FPM_64BIT], 1, [Define to select libmad fixed point arithmetic implementation])
                    ;;
                ppc-* | powerpc-*) 
                    AC_DEFINE([FPM_PPC], 1, [Define to select libmad fixed point arithmetic implementation])
                    ;;
                sparc*-*)
                    if test "$GCC" = yes; then
                        AC_DEFINE([FPM_SPARC], 1, [Define to select libmad fixed point arithmetic implementation])
                    else
                        AC_DEFINE([FPM_64BIT], 1, [Define to select libmad fixed point arithmetic implementation])
                    fi
                    ;;
                mips-*)
                    AC_DEFINE([FPM_MIPS], 1, [Define to select libmad fixed point arithmetic implementation])
                    ;;
                alphaev56-* | alpha* | ia64-* | hppa*-linux-*)
                    AC_DEFINE([FPM_64BIT], 1, [Define to select libmad fixed point arithmetic implementation])
                    ;;
                arm*-*)
                    AC_DEFINE([FPM_ARM], 1, [Define to select libmad fixed point arithmetic implementation])
                    ;;
                *)
                    AC_DEFINE([FPM_DEFAULT], 1, [Define to select libmad fixed point arithmetic implementation])
                    ;;
            esac
        fi
    fi
    AM_CONDITIONAL([ENABLE_MAD], [test x"$enable_mad" = x"yes"])
    AM_CONDITIONAL([WITH_EXTERNAL_MAD], [test x"$have_mad" = x"yes"])


    dnl libmodplug (optional; enabled by default)
    AC_ARG_ENABLE([modplug],
                  [AS_HELP_STRING([--enable-modplug], [Enable modplug support])],
                  [], [enable_modplug=yes])
    if test x"$enable_modplug" != x"no"; then
        PKG_CHECK_MODULES([LIBMODPLUG], [libmodplug >= 0.7],
                          [AC_DEFINE([HAVE_MODPLUG], 1, [define this if you have libmodplug installed])],
                          [enable_modplug=no])
    fi
    AM_CONDITIONAL([ENABLE_MODPLUG], [test x"$have_modplug" = x"yes"])


    dnl libmpcdec (optional; enabled by default; external version allowed)
    AC_ARG_ENABLE([musepack],
                  [AS_HELP_STRING([--disable-musepack], [Disable support for MusePack decoding (default: enabled)])],
                  [], [enable_musepack=yes])
    AC_ARG_WITH([external-libmpcdec],
                [AS_HELP_STRING([--with-external-libmpcdec], [Use external libmpc library])],
                [], [with_external_libmpcdec=no])
    if test x"$enable_musepack" != x"no"; then
        if test x"$with_external_libmpcdec" != x"no"; then
            AC_CHECK_LIB([mpcdec], [mpc_decoder_decode],
                         [AC_CHECK_HEADERS([mpcdec/mpcdec.h], [have_mpcdec=yes])])
            if test x"$have_mpcdec" != x"yes"; then
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
        AC_SUBST(MPCDEC_CFLAGS)
        AC_SUBST(MPCDEC_DEPS)
        AC_SUBST(MPCDEC_LIBS)
    fi
    AM_CONDITIONAL([ENABLE_MUSEPACK], [test x"$enable_musepack" != x"no"])
    AM_CONDITIONAL([WITH_EXTERNAL_LIBMPCDEC], [test x"$have_mpcdec" = x"yes"])


    dnl libw32dll (optional; x86 only; enabled if using GNU as; GNU as required)
    AC_ARG_ENABLE([w32dll],
                  [AS_HELP_STRING([--disable-w32dll], [Disable Win32 DLL support])],
                  [], [enable_w32dll="$with_gnu_as"])
    if test x"$enable_w32dll" != x"no"; then
        case "$host_or_hostalias" in
            *-mingw* | *-cygwin)
                enable_w32dll=no
                ;;
            i?86-* | k?-* | athlon-* | pentium*-)
                test x"$with_gnu_as" = x"no" && enable_w32dll=no
                ;;
            *)
                enable_w32dll=no
                ;;
        esac
    fi
    if test x"$enable_w32dll" != x"no"; then
        if test x"with_gnu_as" = x"no"; then
            AC_MSG_ERROR([You need GNU as to enable Win32 codecs support])
        fi
        AC_ARG_WITH([w32-path],
                    [AS_HELP_STRING([--with-w32-path=PATH], [location of Win32 binary codecs])],
                    [], [w32_path="/usr/lib/codecs"])
        AC_SUBST(w32_path)
    fi
    AM_CONDITIONAL([ENABLE_W32DLL], [test x"$enable_w32dll" != x"no"])


    dnl mlib
    AC_ARG_ENABLE([mlib],
	          [AS_HELP_STRING([--disable-mlib], [do not build Sun mediaLib support])],
                  [], [enable_mlib=yes])
    AC_ARG_ENABLE([mlib-lazyload],
                  [AS_HELP_STRING([--enable-mlib-lazyload], [check for Sun mediaLib at runtime])],
                  [], [enable_mlib_lazyload=no])
    if test x$"enable_mlib" != x"no"; then
        mlibhome="$MLIBHOME"
        test x"$mlibhome" = x"" && mlibhome="/opt/SUNWmlib"
        AC_CHECK_LIB([mlib], [mlib_VideoAddBlock_U8_S16],
                     [saved_CPPFLAGS="$CPPFLAGS" CPPFLAGS="$CPPFLAGS -I$mlibhome/include"
                      AC_CHECK_HEADERS([mlib_video.h],
                          [if test x"$enable_mlib_lazyload" != x"no"; then
                               if test "$GCC" = yes; then
                                   MLIB_LIBS="-L$mlibhome/lib -Wl,-z,lazyload,-lmlib,-z,nolazyload"
                               else
                                   MLIB_LIBS="-L$mlibhome/lib -z lazyload -lmlib -z nolazyload"
                               fi
                               AC_DEFINE([MLIB_LAZYLOAD], 1, [Define this if you want to load mlib lazily])
                           else
                               MLIB_LIBS="-L$mlibhome/lib -lmlib"
                           fi
                           MLIB_CFLAGS="-I$mlibhome/include"
                           LIBMPEG2_CFLAGS="$LIBMPEG2_CFLAGS $MLIB_CFLAGS" 
                           AC_SUBST(LIBMPEG2_CFLAGS)
                           AC_SUBST(MLIB_LIBS)
                           AC_SUBST(MLIB_CFLAGS)
                           AC_DEFINE([HAVE_MLIB], 1, [Define this if you have mlib installed])
                           AC_DEFINE([LIBMPEG2_MLIB], 1, [Define this if you have mlib installed])
                           ac_have_mlib=yes])
                      CPPFLAGS="$saved_CPPFLAGS"], [], ["-L$mlibhome/lib"])
    fi
    AM_CONDITIONAL([HAVE_MLIB], [test x"$ac_have_mlib" = x"yes"])


    dnl mng (optional; enabled by default)
    AC_ARG_ENABLE([mng],
                  [AS_HELP_STRING([--disable-mng], [do not build mng support])],
                  [], [enable_mng=yes])
    if test x"$with_mng" != x"no"; then
        AC_CHECK_LIB([mng], [mng_initialize],
                     [AC_CHECK_HEADERS([libmng.h], [MNG_LIBS="-lmng"], [enable_mng=yes])], [enable_mng=no])
        AC_SUBST(MNG_LIBS)
    fi
    AM_CONDITIONAL([ENABLE_MNG], [test x"$enable_mng" != x"no"])


    dnl Ogg/Speex (optional; enabled by default; external)
    AC_ARG_WITH([speex],
                [AS_HELP_STRING([--without-speex], [Build without Speex audio decoder])])
    if test x"$with_speex" != x"no"; then
        PKG_CHECK_MODULES([SPEEX], [ogg speex], [have_speex=yes], [have_speex=no])
        if test x"$with_speex" = x"yes" && test x"$have_speex" = x"no"; then
            AC_MSG_ERROR([Speex support requested, but libspeex not found])
        elif test x"$have_speex" = x"yes"; then
            AC_DEFINE([HAVE_SPEEX], 1, [Define this if you have speex])
        fi
    fi
    AM_CONDITIONAL([ENABLE_SPEEX], [test x"$have_speex" = x"yes"])


    dnl Ogg/Theora (optional; enabled by default; external)
    AC_ARG_WITH([theora],
                [AS_HELP_STRING([--without-theora], [Build without Theora video decoder])])
    if test x"$with_theora" != x"no"; then
        PKG_CHECK_MODULES([THEORA], [ogg theora], [have_theora=yes], [have_theora=no])
        if test x"$with_theora" = x"yes" && test x"$have_theora" = x"no"; then
            AC_MSG_ERROR([Theora support requested, but libtheora not found])
        elif test x"$have_theora" = x"yes"; then
            AC_DEFINE([HAVE_THEORA], 1, [Define this if you have theora])
        fi
    fi
    AM_CONDITIONAL([ENABLE_THEORA], [test x"$have_theora" = x"yes"])


    dnl Ogg/Vorbis (optional; enabled by default; external)
    AC_ARG_WITH([vorbis],
                [AS_HELP_STRING([--without-vorbis], [Build without Vorbis audio decoder])])
    if test x"$with_vorbis" != x"no"; then
        PKG_CHECK_MODULES([VORBIS], [ogg vorbis], [have_vorbis=yes], [have_vorbis=no])
        if test x"$with_vorbis" = x"yes" && test x"$have_vorbis" = "xno"; then
            AC_MSG_ERROR([Vorbis support requested, but libvorbis not found])
        fi
    fi
    AM_CONDITIONAL([ENABLE_VORBIS], [test x"$have_vorbis" = x"yes"])


    dnl real (optional; default depends on platform)
    dnl On some systems, we cannot enable Real codecs support to begin with.
    dnl This includes Darwin, because it uses Mach-O rather than ELF.
    AC_ARG_ENABLE([real-codecs],
                  [AS_HELP_STRING([--disable-real-codecs], [Disable Real binary codecs support])],
                  [], [case $host_os in
                           darwin*) enable_real_codecs=no ;;
                           *) enable_real_codes=yes ;;
                       esac])
    if test x"$enable_real_codecs" != x"no"; then
        AC_ARG_WITH([real-codecs-path],
                    [AS_HELP_STRING([--with-real-codecs-path=PATH], [Specify directory for Real binary codecs])],
                    [AC_DEFINE_UNQUOTED([REAL_CODEC_PATH], ["$withval"], [Specified path for Real binary codecs])])

        dnl For those that have a replacement, break at the first one found
        AC_CHECK_SYMBOLS([__environ _environ environ], [break], [need_weak_aliases=yes])
        AC_CHECK_SYMBOLS([stderr __stderrp], [break], [need_weak_aliases=yes])

        dnl For these there are no replacements
        AC_CHECK_SYMBOLS([___brk_addr __ctype_b])

        if test "x$need_weak_aliases" = "xyes"; then
            CC_ATTRIBUTE_ALIAS([], [AC_MSG_ERROR([You need weak aliases support for Real codecs on your platform])])
        fi
    fi
    AM_CONDITIONAL([ENABLE_REAL], [test "x$enable_real_codecs" != "xno"])


    dnl wavpack (optional; disabled by default)
    AC_ARG_WITH([wavpack],
                [AS_HELP_STRING([--with-wavpack], [Enable Wavpack decoder (requires libwavpack)])],
                [], [with_wavpack=no])
    if test x"$with_wavpack" != x"no"; then
        PKG_CHECK_MODULES([WAVPACK], [wavpack], [have_wavpack=yes])
    fi
    AM_CONDITIONAL([ENABLE_WAVPACK], [test x"$have_wavpack" = x"yes"])


    dnl Only enable building dmx image if either gdk_pixbuf or ImageMagick are enabled
    AM_CONDITIONAL([BUILD_DMX_IMAGE], [test x"$have_imagemagick" = x"yes" -o x"$no_gdkpixbuf" != x"yes"])
])
