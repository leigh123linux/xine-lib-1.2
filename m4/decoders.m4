dnl ---------------------------
dnl Decoder and Demuxer Plugins
dnl ---------------------------
AC_DEFUN([XINE_DECODER_PLUGINS], [

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
AM_CONDITIONAL([BUILD_DMX_IMAGE], [test "x$have_imagemagick" = "xyes" -o "x$no_gdkpixbuf" != "xyes"])

])
