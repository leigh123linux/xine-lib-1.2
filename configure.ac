dnl
dnl Configure.ac for xine
dnl

AC_INIT
AC_CONFIG_SRCDIR([src/xine-engine/xine.c])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_LIBOBJ_DIR([lib])

dnl
dnl Require autoconf version 2.60
dnl
AC_PREREQ(2.59)

dnl Making releases:
dnl   XINE_SUB += 1; XINE_PATCH = ''; continue with XINE_LT_* values below
dnl
XINE_MAJOR=1
XINE_MINOR=1
XINE_SUB=16
dnl XINE_PATCH should be left empty or set to ".1" or ".2" or something similar
XINE_PATCH=
dnl Release series number (usually $XINE_MAJOR.$XINE_MINOR)
XINE_SERIES=1.1

dnl if test $XINE_SUB -eq 0 ; then
dnl  XINE_SUBPART="";
dnl else
  XINE_SUBPART=".$XINE_SUB$XINE_PATCH"
dnl fi

dnl The libtool version numbers (XINE_LT_*); Don't even think about faking this!
dnl 
dnl immediately before every release do:
dnl ===================================
dnl if (the interface is totally unchanged from previous release)
dnl    XINE_LT_REVISION ++;
dnl else { /* interfaces have been added, removed or changed */
dnl    XINE_LT_REVISION = 0;
dnl    XINE_LT_CURRENT ++;
dnl    if (any interfaces have been _added_ since last release)
dnl       XINE_LT_AGE ++;
dnl    if (any interfaces have been _removed_ or _incompatibly changed_)
dnl       XINE_LT_AGE = 0;
dnl }
dnl 
dnl If you want to know more about what you are doing, here are some details:
dnl  * XINE_LT_CURRENT is the current API version
dnl  * XINE_LT_REVISION is an internal revision number which is increased when the API
dnl    itself did not change
dnl  * XINE_LT_AGE is the number of previous API versions still supported by this library
dnl  * libtool has its own numbering scheme, because local library numbering schemes
dnl    are platform dependent
dnl  * in Linux, the library will be named
dnl    libname.so.(XINE_LT_CURRENT - XINE_LT_AGE).XINE_LT_AGE.XINE_LT_REVISION

XINE_LT_CURRENT=26
XINE_LT_REVISION=0
XINE_LT_AGE=25

dnl for a release tarball do "rm .cvsversion" before "make dist"
if test -f "${srcdir-.}/.cvsversion"; then
  XINE_PRE="hg"
else
  XINE_PRE=""
fi

AC_SUBST(XINE_MAJOR)
AC_SUBST(XINE_MINOR)
AC_SUBST(XINE_SUB)
AC_SUBST(XINE_PATCH)
AC_SUBST(XINE_SERIES)
AC_SUBST(XINE_IFACE_AGE)
AC_SUBST(XINE_BIN_AGE)

AC_DEFINE_UNQUOTED(XINE_MAJOR, $XINE_MAJOR,[xine major version number])
AC_DEFINE_UNQUOTED(XINE_MINOR, $XINE_MINOR,[xine minor version number])
AC_DEFINE_UNQUOTED(XINE_SUB, $XINE_SUB, [xine sub version number])
AC_DEFINE_UNQUOTED(XINE_PATCH, $XINE_PATCH, [xine patch version number])

AC_SUBST(XINE_LT_CURRENT)
AC_SUBST(XINE_LT_REVISION)
AC_SUBST(XINE_LT_AGE)

AC_DEFINE_UNQUOTED(XINE_LT_CURRENT, $XINE_LT_CURRENT, [xine interface version number])
AC_DEFINE_UNQUOTED(XINE_LT_AGE, $XINE_LT_AGE, [xine interface version age])

SPEC_VERSION=$XINE_MAJOR.$XINE_MINOR${XINE_SUBPART}$XINE_PRE
TAR_NAME="xine-lib-"$SPEC_VERSION
dnl TAR_NAME="xine-lib-"$XINE_MAJOR-$XINE_PRE
dnl SPEC_VERSION=${XINE_MAJOR}_$XINE_PRE

AC_SUBST(TAR_NAME)
AC_SUBST(SPEC_VERSION)

LIBNAME=libxine$XINE_MAJOR
AC_SUBST(LIBNAME)

AC_DEFINE_UNQUOTED(XINE_TEXTDOMAIN, "$LIBNAME", [catalog message text domain])

dnl
AM_INIT_AUTOMAKE("xine-lib", $SPEC_VERSION)
dnl AM_INIT_AUTOMAKE("xine-lib", $XINE_MAJOR-$XINE_PRE)

dnl ---------------------------------------------
dnl Made possible to build for another arch.
dnl ---------------------------------------------

AC_CANONICAL_HOST
AC_CANONICAL_BUILD

test "$host" == "$build" && check_athlon=yes

AC_CONFIG_HEADERS([include/configure.h])


dnl ---------------------------------------------
dnl Check for programs.
dnl ---------------------------------------------

AC_PROG_CC
AM_PROG_AS

case "$host_os" in
  *darwin*)
     m4_ifdef([AC_PROG_OBJC],
       [AC_PROG_OBJC],
       [AC_CHECK_TOOL([OBJC], [gcc])
        AC_SUBST([OBJC])
	AC_SUBST([OBJCFLAGS])
       ])
     CPPFLAGS="$CPPFLAGS -DCONFIG_DARWIN"
     ;;
esac

AC_GNU_SOURCE
dnl Save CFLAGS, AC_ISC_POSIX set some unwanted default CFLAGS
saved_CFLAGS="$CFLAGS"
AC_ISC_POSIX
AC_PATH_MAGIC
CFLAGS="$saved_CFLAGS"
AC_HEADER_STDC

AC_PROG_MAKE_SET
AC_PROG_EGREP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_AWK
AC_C_INLINE

AC_CHECK_TOOL([STRINGS], [strings], [false])

dnl ---------------------------------------------
dnl Libtool
dnl ---------------------------------------------

AC_LIBTOOL_DLOPEN
AC_DISABLE_STATIC
AC_LIBTOOL_WIN32_DLL
AC_PROG_LIBTOOL
AC_PROG_LIBTOOL_SANITYCHECK
AC_SUBST(LIBTOOL_DEPS)
if ${CONFIG_SHELL} ./libtool --features | grep "enable static" >/dev/null; then
  STATIC="-static"
else
  STATIC=
fi
AC_SUBST(STATIC)


dnl ---------------------------------------------
dnl Requirements for macros whose first invocations are conditional
dnl ---------------------------------------------

PKG_PROG_PKG_CONFIG
CC_CHECK_WERROR

dnl ---------------------------------------------
dnl NLS Support
dnl ---------------------------------------------

dnl ALL_LINGUAS="cs da de el en@quot en@boldquot es et fr gl id it ja ko nl nn no pl pt pt_BR ru sl sv tr zh"
AM_ICONV
AC_ARG_ENABLE([iconvtest],
	AS_HELP_STRING([--disable-iconvtest], [don't require iconv library]))
if test "x$enable_iconvtest" != xno; then
  if test "x$am_cv_func_iconv" != xyes; then
    AC_MSG_ERROR([
****************************************************************
* iconv library not found. It's necessary for proper           *
* manipulation with texts so xine requires it as default.      *
*                                                              *
* You need to install iconv library or to specify prefix       *
* by option --with-libiconv-prefix.                            *
*                                                              *
* If you don't want iconv support use the option               *
* --disable-iconvtest.                                          *
****************************************************************
  ])
  fi
fi

AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.16.1])
AC_PROG_GMSGFMT_PLURAL

AC_CHECK_FUNCS([nl_langinfo])

dnl ---------------------------------------------
dnl Checks for typedefs, structures, and compiler characteristics.
dnl ---------------------------------------------

AC_C_BIGENDIAN
AC_C_CONST
dnl AC_C_ALWAYS_INLINE removal allows ffmpeg to be more widely buildable
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_CHECK_SIZEOF(long)
dnl AC_CHECK_TYPES([ptrdiff_t])
dnl For systems without inttypes.h would be needed extend generated replacement.
AC_CHECK_GENERATE_INTTYPES([include])
AM_CONDITIONAL(GENERATED_INTTYPES_H, test "x$ac_cv_header_inttypes_h" != x"yes")
AC_CHECK_TYPE(ssize_t, :, AC_DEFINE(ssize_t, __int64, [define ssize_t to __int64 if it's missing in default includes]))

dnl ---------------------------------------------
dnl threads and OS specific stuff
dnl ---------------------------------------------

CC_PTHREAD_FLAGS(, [AC_MSG_ERROR([Pthread support is needed])])
CC_PTHREAD_RECURSIVE_MUTEX(, [AC_MSG_ERROR([recursive mutex support is needed - please report])])
dnl <pthread.h> is implicitly included by xine-internals.h, so the include dir is needed everywhere
CPPFLAGS="$CPPFLAGS $PTHREAD_CFLAGS"

AC_CHECK_DECL(sysi86,[
	AC_DEFINE(HAVE_SYSI86,1,[Define this if the 'sysi86' function is declared in sys/sysi86.h])
      ],,[
#include <sys/sysi86.h>
])

AC_GETOPT_LONG

dnl ---------------------------------------------
dnl Windows ports checks
dnl ---------------------------------------------
case "$host" in
  *-*-mingw* | *-*-cygwin*)
    dnl check if we are using the cygwin, mingw or cygwin with mno-cygwin mode
    dnl in which case we are actually dealing with a mingw32 compiler
    case "$host" in
      *-*-mingw32*)
        SYS=mingw32
        ;;
      *-*-cygwin*)
        AC_EGREP_CPP(pattern, 
                     [#ifdef WIN32
                      yes
                      #endif],
                     SYS=mingw32, SYS=cygwin)
        ;;
    esac

    if test "$SYS" = "mingw32"; then
      WIN32_CPPFLAGS='-I$(top_srcdir)/win32/include'
      LIBS="-lwinmm -lwsock32 $LIBS"
      GOOM_LIBS="-liberty"
    fi
    LDFLAGS="-no-undefined $LDFLAGS"
    ;;
esac
AC_SUBST(GOOM_LIBS)
AC_SUBST(WIN32_CPPFLAGS)
AM_CONDITIONAL(WIN32, test "x$SYS" = "xmingw32")


dnl ---------------------------------------------
dnl dynamic linker
dnl ---------------------------------------------
AM_DL()


dnl ---------------------------------------------
dnl determine libdir name
dnl ---------------------------------------------

AC_MSG_CHECKING([libdir name])
case $host in
*-*-linux*)
 # Test if the compiler is 64bit
 echo 'int i;' > conftest.$ac_ext
 xine_cv_cc_64bit_output=no
 if AC_TRY_EVAL(ac_compile); then
 case `"$MAGIC_CMD" conftest.$ac_objext` in
 *"ELF 64"*)
   xine_cv_cc_64bit_output=yes
   ;;
 esac
 fi
 rm -rf conftest*
 ;;
esac

case $host_cpu:$xine_cv_cc_64bit_output in
powerpc64:yes | s390x:yes | sparc64:yes | x86_64:yes)
 XINE_LIBNAME="lib64"
 ;;
*:*)
 XINE_LIBNAME="lib"
 ;;
esac
AC_MSG_RESULT([$XINE_LIBNAME])


dnl ---------------------------------------------
dnl mpeg2lib and ffmpeg stuff
dnl ---------------------------------------------

AC_SUBST(LIBMPEG2_CFLAGS)

AC_ARG_WITH([external-ffmpeg], AS_HELP_STRING([--with-external-ffmpeg], [use external ffmpeg library]))

case "x$with_external_ffmpeg" in
   xno)
      ;;
   xyes)
      PKG_CHECK_MODULES([FFMPEG], [libavcodec >= 51.20.0])
      ;;
   *)
      with_external_ffmpeg=yes
      PKG_CHECK_MODULES([FFMPEG], [libavcodec >= 51.20.0], [],
			[AC_MSG_RESULT(no); with_external_ffmpeg=no])
      ;;
esac
if test "x$with_external_ffmpeg" != "xno"; then
   PKG_CHECK_MODULES([FFMPEG_POSTPROC], [libpostproc])
   AC_SUBST([FFMPEG_CFLAGS])
   AC_SUBST([FFMPEG_LIBS])
   AC_SUBST([FFMPEG_POSTPROC_CFLAGS])
   AC_SUBST([FFMPEG_POSTPROC_LIBS])
   AC_DEFINE([HAVE_FFMPEG], [1], [Define this if you have ffmpeg library])

   dnl Check presence of ffmpeg/avutil.h to see if it's old or new
   dnl style for headers. The new style would be preferred actually...
   AC_CHECK_HEADERS([ffmpeg/avutil.h])
   
   AC_MSG_RESULT([using external ffmpeg])
else
  AC_MSG_NOTICE([
*********************************************************************
xine-lib is configured to use internal ffmpeg.

This copy of ffmpeg is old. You are strongly advised to install a
newer version (including development files) and to reconfigure
xine-lib to use it.
*********************************************************************])
  LIBFFMPEG_CPPFLAGS="-DHAVE_AV_CONFIG_H -DRUNTIME_CPUDETECT -DXINE_MPEG_ENCODER -D_ISOC9X_SOURCE -DCONFIG_DECODERS"
  AC_CHECK_TYPES(int_fast8_t, [], [LIBFFMPEG_CPPFLAGS="$LIBFFMPEG_CPPFLAGS -DEMULATE_FAST_INT"])
  AC_SUBST([LIBFFMPEG_CPPFLAGS])
fi
AM_CONDITIONAL(HAVE_FFMPEG, test "x$with_external_ffmpeg" = "xyes")


AC_ARG_ENABLE([ffmpeg_uncommon_codecs],
	AS_HELP_STRING([--disable-ffmpeg-uncommon-codecs], [don't build uncommon ffmpeg codecs]))

AC_ARG_ENABLE([ffmpeg_popular_codecs],
	AS_HELP_STRING([--disable-ffmpeg-popular-codecs], [don't build popular ffmpeg codecs]))

AS_MKDIR_P(src/libffmpeg)
ffmpeg_config_h=src/libffmpeg/ffmpeg_config.h
echo "/* Automatically generated */" > $ffmpeg_config_h

dnl uncommon ffmpeg codecs
ffmpeg_uncommon_codecs="AASC ASV1 ASV2 AVS CSCD CYUV DVVIDEO EIGHTBPS FLIC FLV FOURXM FRAPS HUFFYUV IDCIN INTERPLAY_VIDEO KMVC LOCO MMVIDEO NUV QDRAW QPEG ROQ RPZA SMACKER SMC SNOW TRUEMOTION1 TRUEMOTION2 TSCC ULTI VCR1 VMDVIDEO WNV1 XAN_WC3 XL ZMBV ALAC AMR_NB AMR_WB LIBGSM MACE3 MACE6 SHORTEN SMACKAUD TRUESPEECH TTA VMDAUDIO PCM_ALAW PCM_MULAW PCM_S8 PCM_S16BE PCM_S16LE PCM_S24BE PCM_S24DAUD PCM_S24LE PCM_S32BE PCM_S32LE PCM_U8 PCM_U16BE PCM_U16LE PCM_U24BE PCM_U24LE PCM_U32BE PCM_U32LE INTERPLAY_DPCM ROQ_DPCM SOL_DPCM VQA XAN_DPCM ADPCM_4XM ADPCM_CT ADPCM_EA ADPCM_IMA_DK3 ADPCM_IMA_DK4 ADPCM_IMA_QT ADPCM_IMA_SMJPEG ADPCM_IMA_WAV ADPCM_IMA_WS ADPCM_MS ADPCM_SBPRO_2 ADPCM_SBPRO_3 ADPCM_SBPRO_4 ADPCM_XA ADPCM_YAMAHA"
for ucname in $ffmpeg_uncommon_codecs; do
  config_name="CONFIG_${ucname}_DECODER"
  enabled_name="ENABLE_${ucname}_DECODER"
  
  if test "x$enable_ffmpeg_uncommon_codecs" != xno; then
    echo "#define $config_name 1" >> $ffmpeg_config_h
    echo "#define $enabled_name 1" >> $ffmpeg_config_h
  else
    echo "#define $enabled_name 0" >> $ffmpeg_config_h
  fi
done

dnl popular ffmpeg codecs
ffmpeg_popular_codecs="CINEPAK FLASHSV H261 H263 H263I H264 INDEO2 INDEO3 MJPEG MJPEGB MPEG1VIDEO MPEG2VIDEO MPEG4 MPEGVIDEO MSMPEG4V1 MSMPEG4V2 MSMPEG4V3 MSRLE MSVIDEO1 QTRLE RV10 RV20 SVQ1 SVQ3 VC1 VP3 VP5 VP6 VP6F WMV1 WMV2 WMV3 COOK DTS FLAC MP2 MP3 QDM2 RA_144 RA_288 WAVPACK WMAV1 WMAV2 ADPCM_SWF"

for ucname in $ffmpeg_popular_codecs; do
  config_name="CONFIG_${ucname}_DECODER"
  enabled_name="ENABLE_${ucname}_DECODER"
  
  if test "x$enable_ffmpeg_popular_codecs" != xno; then
    echo "#define $config_name 1" >> $ffmpeg_config_h
    echo "#define $enabled_name 1" >> $ffmpeg_config_h
  else
    echo "#define $enabled_name 0" >> $ffmpeg_config_h
  fi
done

dnl disabled ffmpeg codecs
ffmpeg_disabled_codecs="BMP CAVS CLJR DSICINVIDEO FFV1 FFVHUFF GIF MDEC MPEG_XVMC MSZH PNG RAWVIDEO SP5X TARGA TIERTEXSEQVIDEO TIFF VMNC ZLIB DSICINAUDIO IMC MP3ADU MP3ON4 MPC7 SONIC WS_SND1 ADPCM_ADX ADPCM_G726 DVBSUB DVDSUB THEORA AAC MPEG4AAC AC3 VORBIS"
for ucname in $ffmpeg_disabled_codecs; do
  config_name="CONFIG_${ucname}_DECODER"
  enabled_name="ENABLE_${ucname}_DECODER"
  
  echo "#define $enabled_name 0" >> $ffmpeg_config_h
done

dnl disabled ffmpeg encoders
ffmpeg_extra_encoders="H263P JPEGLS LJPEG PAM PBM PGM PGMYUV PPM SONIC_LS"
for ucname in $ffmpeg_uncommon_codecs $ffmpeg_popular_codecs $ffmpeg_disabled_codecs $ffmpeg_extra_encoders; do
  config_name="CONFIG_${ucname}_ENCODER"
  enabled_name="ENABLE_${ucname}_ENCODER"
  
  echo "#define $enabled_name 0" >> $ffmpeg_config_h
done

dnl disabled parsers
ffmpeg_parsers="AAC AC3 CAVSVIDEO DVBSUB DVDSUB H261 H263 H264 MJPEG MPEG4VIDEO MPEGAUDIO MPEGVIDEO PNM"
for ucname in $ffmpeg_parsers; do
  config_name="CONFIG_${ucname}_PARSER"
  enabled_name="ENABLE_${ucname}_PARSER"
  
  echo "#define $enabled_name 0" >> $ffmpeg_config_h
done

LIBMPEG2_CFLAGS=""

AC_CHECK_DECL(lrintf,[
	AC_DEFINE(HAVE_LRINTF,1,[Define this if the 'lrintf' function is declared in math.h])
	AC_DEFINE(_ISOC9X_SOURCE,1,[Define this if you are ISO C9X compliant])
      ],,[
#define _ISOC9X_SOURCE
#include <math.h>
])
AC_CHECK_DECL(rintf,[
	AC_DEFINE(HAVE_RINTF,1,[Define this if the 'rintf' function is declared in math.h])
      ],,[
#include <math.h>
])

AC_CHECK_FUNCS(memalign)

AC_ARG_ENABLE([altivec],
	AS_HELP_STRING([--disable-altivec], [do not use assembly codes for Motorola 74xx CPUs]))

AC_ARG_ENABLE([vis],
	AS_HELP_STRING([--disable-vis], [do not use assembly codes for Sun UltraSPARC CPUs]))

AC_ARG_ENABLE([mlib],
	AS_HELP_STRING([--enable-mlib], [build Sun mediaLib support]))

AC_ARG_ENABLE([mlib-lazyload],
	AS_HELP_STRING([--enable-mlib-lazyload], [check for Sun mediaLib at runtime]))

if test "x$enable_mlib" = xyes; then
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
dnl Checks for X11
dnl ---------------------------------------------

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


dnl ---------------------------------------------
dnl socket library
dnl ---------------------------------------------

dnl Test for socket and network support library
NET_LIBS=""
AC_CHECK_LIB(socket, socket, NET_LIBS="-lsocket $NET_LIBS",)
AC_CHECK_LIB(nsl, gethostbyname, NET_LIBS="-lnsl $NET_LIBS",)
AC_SUBST(NET_LIBS)


dnl ---------------------------------------------
dnl socklen_t
dnl ---------------------------------------------
AC_CHECK_SOCKLEN_T


dnl ---------------------------------------------
dnl IPv6
dnl ---------------------------------------------

AC_ARG_ENABLE([ipv6],
	AS_HELP_STRING([--enable-ipv6], [enable use of IPv6]))

if test "x$enable_ipv6" = "xyes"; then
   AC_DEFINE([ENABLE_IPV6], [1], [Enable this when IPv6 is requested])
fi

dnl ---------------------------------------------
dnl zlib
dnl ---------------------------------------------

AC_ARG_WITH(zlib-prefix, 
  AS_HELP_STRING(
    [--with-zlib-prefix=PREFIX],
    [path to zlib compression library]),
  [zlib_prefix="$withval"],
  [zlib_prefix="no"])

if test "x$zlib_prefix" = "xno"; then
  ZLIB_LIBS="-lz"
else
  ZLIB_CPPFLAGS="-I$zlib_prefix/include"
  ZLIB_LIBS="-L$zlib_prefix/lib -lz"
fi

ac_save_CPPFLAGS="$CPPFLAGS"
ac_save_LIBS="$LIBS"
CPPFLAGS="$CPPFLAGS $ZLIB_CPPFLAGS"
LIBS="$LIBS $ZLIB_LIBS"

AC_CHECK_LIB(z, gzsetparams,
  [AC_CHECK_HEADER(zlib.h,
     have_zlib=yes)])

CPPFLAGS="$ac_save_CPPFLAGS"
LIBS="$ac_save_LIBS"

if test "x$have_zlib" != "xyes"; then
  AC_MSG_ERROR(zlib needed)
fi
AC_SUBST(ZLIB_CPPFLAGS)
AC_SUBST(ZLIB_LIBS)


dnl ---------------------------------------------
dnl check posix timers
dnl ---------------------------------------------

AC_CHECK_LIB(rt, clock_getres,
             [RT_LIBS="-lrt"
              AC_DEFINE(HAVE_POSIX_TIMERS,1,
                        [Define this if you have POSIX timers.])], 
             AC_MSG_RESULT([*** no POSIX timers available.]))
AC_SUBST(RT_LIBS)

dnl ---------------------------------------------
dnl Check for OpenGL & [GLut | GLU]
dnl ---------------------------------------------

AM_PATH_OPENGL()


dnl ---------------------------------------------
dnl Check for platform which supports syncfb
dnl ---------------------------------------------

AC_ARG_ENABLE([syncfb],
	AS_HELP_STRING([--disable-syncfb], [do not build syncfb plugin]))

case "$host_os" in
  *linux*) ;;
  *)
    if test "x$enable_syncfb" = "xyes"; then
       AC_MSG_ERROR([You cannot build SyncFB support on non-Linux systems.])
    fi
    enable_syncfb=no ;;
esac

AM_CONDITIONAL(HAVE_SYNCFB, test "x$enable_syncfb" != "xno")


dnl ----------------------------------------------
dnl Check for usable video-for-linux (v4l) support
dnl ----------------------------------------------
AC_ARG_ENABLE([v4l],
	AS_HELP_STRING([--disable-v4l], [do not build Video4Linux input plugin]))

if test "x$enable_v4l" != "xno"; then
  AC_CHECK_HEADERS([linux/videodev.h], [have_v4l=yes], [have_v4l=no])
  AC_CHECK_HEADERS([asm/types.h])
  if test "x$enable_v4l" = "xyes" && test "x$have_v4l" = "xno"; then
     AC_MSG_ERROR([Video4Linux support requested, but prerequisite headers not found.])
  fi
fi

AM_CONDITIONAL(HAVE_V4L, [test "x$have_v4l" = "xyes"])

dnl ----------------------------------------------
dnl Check for Xv and XvMC support
dnl ----------------------------------------------

dnl With recent XFree86 or Xorg, dynamic linking is preferred!
dnl Only dynamic linking is possible when using libtool < 1.4.0

AC_ARG_WITH(xv-path, AS_HELP_STRING([--with-xv-path=path], [where libXv is installed]),
            xv_path="$withval",)

AC_ARG_ENABLE([static-xv],
        AS_HELP_STRING([--enable-static-xv],[Enable this to force linking against libXv.a]))

if test "x$enable_static_xv" = "xyes"; then
  xv_prefer_shared="no"
else
  xv_prefer_shared="yes"
fi

if test "x$no_x" != "xyes"; then
   PKG_CHECK_MODULES([XV], [xv], [
     ac_have_xv="yes"
     AC_DEFINE([HAVE_XV], [1], [Define this if you have libXv installed])
   ], [AC_FIND_LIBXV])
fi
AM_CONDITIONAL(HAVE_XV, test "x$ac_have_xv" = "xyes")


host_or_hostalias="$host"
if test "$host_or_hostalias" = ""; then
    dnl user has called ./configure with a host parameter unknown to
    dnl config.sub; the canonical "$host" is empty
    dnl
    dnl Try the following switch with user's original host_alias 
    dnl input instead.
    dnl
    host_or_hostalias="$host_alias"
fi

case "$host_or_hostalias" in
  hppa*)
	if test "x$ac_have_xv_static" = "xyes"; then
	  echo "warning: hppa linker - disabling static libXv"
	  XV_LIBS="libXv.so"
	fi
	;;

  ppc-*-linux* | powerpc-*)
        ppc_arch="yes"
        ;;

  *)
        ;;
esac
AM_CONDITIONAL(PPC_ARCH, test "x$ppc_arch" = "xyes")

dnl
dnl Check if we can enable the xxmc plugin.
dnl

AC_ARG_ENABLE([xvmc],
  AS_HELP_STRING([--disable-xvmc], [Disable XxMC and XvMC outplut plugins]) )

if test "x$no_x" = "x" && test "x$enable_xvmc" != "xno"; then

AC_ARG_WITH(xxmc-path, AS_HELP_STRING([--with-xxmc-path=path], [where libXvMC libraries for the
	xxmc plugin are  installed. Defalts to the default X library path.]),
	xxmc_path="$withval", xxmc_path="${x_libraries:-/usr/lib}")
AC_ARG_WITH(xxmc-lib, AS_HELP_STRING([--with-xxmc-lib=XXXX], [The name of the XvMC library 
	libXXXX.so for the xxmc plugin.]),xxmc_stub="$withval", 
	xxmc_stub="XvMCW")

saved_libs="$LIBS"
saved_CPPFLAGS="$CPPFLAGS"
if test "x$x_includes" != "x"; then
	CPPFLAGS="$CPPFLAGS -I$x_includes"
fi

XXMC_LIBS="-L$xxmc_path -l$xxmc_stub"
AC_MSG_CHECKING(whether to enable the xxmc plugin with vld extensions)
AC_MSG_RESULT()
dnl Check if vld "extended" XvMC is available
if test "x$xxmc_stub" == "xXvMCW" && test "x$ac_have_xv" == "xyes"; then
	AC_CHECK_LIB($xxmc_stub, XvMCPutSlice,
             	     ac_have_xxmc="yes",
	             [ac_have_xxmc="no"
              	        AC_MSG_RESULT([*** Could not link with -l$xxmc_stub for vld extensions.])],
                     [-L$xxmc_path $X_LIBS $X_PRE_LIBS $XV_LIBS -lXext $X_EXTRA_LIBS])
else
    if test "x$ac_have_xv" = "xyes"; then 
      AC_CHECK_LIB($xxmc_stub, XvMCPutSlice,
	           [ac_have_xxmc="yes"
	             XXMC_LIBS="$XXMC_LIBS -lXvMC"],
                   [ac_have_xxmc="no"
		     AC_MSG_RESULT([*** Could not link with -l$xxmc_stub -lXvMC for vld extensions.])],
                   [-L$xxmc_path -lXvMC $X_LIBS $X_PRE_LIBS $XV_LIBS -lXext $X_EXTRA_LIBS])
    else 
      ac_have_xxmc="no",
    fi  
fi

if test "x$ac_have_xxmc" = "xyes"; then
  AC_CHECK_HEADERS(X11/extensions/vldXvMC.h,
     [ac_have_vldxvmc_h="yes"],
     ac_have_vldxvmc="no",)
  if test "x$ac_have_vldxvmc_h" = "xyes"; then
     AC_DEFINE([HAVE_VLDXVMC], [1], 
               [Define 1 if you have vldXvMC.h])
  fi
fi
dnl Try fallback to standard XvMC if vld failed
if test "x$ac_have_xxmc" = "xno"; then
  if test "x$xxmc_stub" == "xXvMCW"; then
	  AC_CHECK_LIB($xxmc_stub, XvMCCreateContext,
               ac_have_xxmc="yes",
	       [ac_have_xxmc="no"
                AC_MSG_RESULT([*** Could not link with -l$xxmc_stub for standard XvMC.])],
               [-L$xxmc_path $X_LIBS $X_PRE_LIBS $XV_LIBS -lXext $X_EXTRA_LIBS])
  else
      if test "x$ac_have_xv" = "xyes"; then 
        AC_CHECK_LIB($xxmc_stub, XvMCCreateContext,
	             [ac_have_xxmc="yes"
		      XXMC_LIBS="$XXMC_LIBS -lXvMC"],
                     [ac_have_xxmc="no"
		      AC_MSG_RESULT([*** Could not link with -lXvMC for standard XvMC.])],
                     [-L$xxmc_path -lXvMC $X_LIBS $X_PRE_LIBS $XV_LIBS -lXext $X_EXTRA_LIBS])
      else 
        ac_have_xxmc="no",
      fi  
  fi
fi
if test "x$ac_have_xxmc" = "xyes"; then
  AC_CHECK_HEADERS(X11/extensions/XvMC.h,,
     ac_have_xxmc="no",)
fi
if test "x$ac_have_xxmc" = "xyes"; then
  AC_DEFINE(HAVE_XXMC,1,[Define this to compile the xxmc plugin.])
  if test "x$ac_have_vldxvmc_h" = "xyes"; then
    AC_MSG_RESULT([*** Enabling xxmc plugin with vld extensions.])
  else
    AC_MSG_RESULT([*** Enabling xxmc plugin for standard XvMC *only*.])
  fi
else
  AC_MSG_RESULT([*** Disabling xxmc plugin due to above errors.])
fi
LIBS="$saved_libs"
fi
AM_CONDITIONAL(HAVE_VLDXVMC, test "x$ac_have_vldxvmc_h" = "xyes")
AM_CONDITIONAL(HAVE_XXMC, test "x$ac_have_xxmc" = "xyes")
AC_SUBST(XXMC_LIBS)
	   
dnl
dnl Check if we can enable the xvmc plugin.
dnl
if test "x$no_x" = "x" && test "x$enable_xvmc" != "xno"; then

AC_ARG_WITH(xvmc-path, AS_HELP_STRING([--with-xvmc-path=path], [where libXvMC libraries for the
	xvmc plugin are  installed. Defalts to the default X library path.]),
	xvmc_path="$withval", xvmc_path="${x_libraries:-/usr/lib}")
AC_ARG_WITH(xvmc-lib, AS_HELP_STRING([--with-xvmc-lib=XXXX], [The name of the XvMC library 
	libXXXX.so for the xvmc plugin.]),xvmc_stub="$withval", 
	xvmc_stub="XvMCW")
saved_libs="$LIBS"
XVMC_LIBS="-L$xvmc_path -l$xvmc_stub"
AC_MSG_CHECKING(whether to enable the xvmc plugin)
AC_MSG_RESULT()
if test "x$xvmc_stub" == "xXvMCW"; then
	AC_CHECK_LIB($xvmc_stub, XvMCCreateContext,
             ac_have_xvmc="yes",
	     [ac_have_xvmc="no"
              AC_MSG_RESULT([*** Could not link with -l$xvmc_stub.])],
             [-L$xvmc_path $X_LIBS $X_PRE_LIBS $XV_LIBS -lXext $X_EXTRA_LIBS])
else
    if test "x$ac_have_xv" = "xyes"; then 
      AC_CHECK_LIB($xvmc_stub, XvMCCreateContext,
	           [ac_have_xvmc="yes"
		    XVMC_LIBS="$XVMC_LIBS -lXvMC"],
                   [ac_have_xvmc="no"
		    AC_MSG_RESULT([*** Could not link with -lXvMC.])],
                   [-L$xvmc_path -lXvMC $X_LIBS $X_PRE_LIBS $XV_LIBS -lXext $X_EXTRA_LIBS])
    else 
      ac_have_xvmc="no",
    fi  
fi
if test "x$ac_have_xvmc" = "xyes"; then
  AC_CHECK_HEADERS(X11/extensions/XvMC.h,,
     ac_have_xvmc="no",)
fi
if test "x$ac_have_xvmc" = "xyes"; then
  AC_DEFINE(HAVE_XVMC,1,[Define this if you have an XvMC library and XvMC.h installed.])
  AC_MSG_RESULT([*** Enabling old xvmc plugin.])
else
  AC_MSG_RESULT([*** Disabling old xvmc plugin due to above errors.])
fi
CPPFLAGS="$saved_CPPFLAGS"
LIBS="$saved_libs"
fi
AM_CONDITIONAL(HAVE_XVMC, test "x$ac_have_xvmc" = "xyes")
AC_SUBST(XVMC_LIBS)


dnl ---------------------------------------------
dnl Check for xcb
dnl ---------------------------------------------
AC_ARG_WITH([xcb], AS_HELP_STRING([--without-xcb], [Doesn't build XCB video out plugins]))

if test "x$with_xcb" != "xno"; then
  PKG_CHECK_MODULES([XCB], [xcb-shape >= 1.0], [have_xcb="yes"], [have_xcb="no"])
fi

AC_SUBST(XCB_CFLAGS)
AC_SUBST(XCB_LIBS)
AM_CONDITIONAL(HAVE_XCB, test "x$have_xcb" = "xyes" )


dnl ---------------------------------------------
dnl Check for xcb-shm
dnl ---------------------------------------------

if test "x$have_xcb" = "xyes"; then
  PKG_CHECK_MODULES([XCBSHM], [xcb-shm], [have_xcbshm="yes"], [have_xcbshm="no"])
fi

AC_SUBST(XCBSHM_CFLAGS)
AC_SUBST(XCBSHM_LIBS)
AM_CONDITIONAL(HAVE_XCBSHM, test "x$have_xcbshm" = "xyes" )


dnl ---------------------------------------------
dnl Check for xcb-xv
dnl ---------------------------------------------

if test "x$have_xcb" = "xyes"; then
  PKG_CHECK_MODULES([XCBXV], [xcb-xv], [have_xcbxv="yes"], [have_xcbxv="no"])
fi

AC_SUBST(XCBXV_CFLAGS)
AC_SUBST(XCBXV_LIBS)
AM_CONDITIONAL(HAVE_XCBXV, test "x$have_xcbxv" = "xyes" )


dnl ---------------------------------------------
dnl Checks for Xinerama extension
dnl ---------------------------------------------

AC_ARG_ENABLE([xinerama],
  AS_HELP_STRING([--disable-xinerama], [do not build Xinerama support]))

if test "x$enable_xinerama" != "xno"; then
   PKG_CHECK_MODULES([XINERAMA], [xinerama], [ac_have_xinerama=yes], [
      AC_CHECK_LIB(Xinerama, XineramaQueryExtension, 
                  [XINERAMA_LIBS="-lXinerama"
                   ac_have_xinerama="yes"],,
                  [$X_LIBS $X_PRE_LIBS -lXext $X_EXTRA_LIBS])
   ])
   if test "x$ac_have_xinerama" = "xyes"; then
      AC_DEFINE(HAVE_XINERAMA,1,[Define this if you have libXinerama installed])
      X_LIBS="${X_LIBS} ${XINERAMA_LIBS}"
   fi
else
  ac_have_xinerama=no
fi
dnl AM_CONDITIONAL(HAVE_XINERAMA, test "x$ac_have_xinerama" = "xyes")

 
dnl ---------------------------------------------
dnl Checks for Ascii-Art library
dnl ---------------------------------------------

AC_ARG_ENABLE([aalib],
  AS_HELP_STRING([--disable-aalib], [do not build AALIB support]),
  [with_aalib=$enableval], [with_aalib=yes])

if test "x$with_aalib" = "xyes"; then
  AM_PATH_AALIB(1.4,, AC_MSG_RESULT([*** All of AALIB dependent parts will be disabled ***]))
else
  no_aalib=yes
fi

AM_CONDITIONAL(HAVE_AA, test "x$no_aalib" != "xyes")

dnl ---------------------------------------------
dnl Checks for Color AsCii Art library
dnl ---------------------------------------------

AC_ARG_WITH([caca],
  AS_HELP_STRING([--without-caca], [Do not build CACA support]))

if test "x$with_caca" != "xno"; then
   PKG_CHECK_MODULES([CACA], [caca cucul], [have_caca="yes"], [have_caca="no"])
   if test "x$with_caca" = "xyes" && test "x$have_caca" = "xno"; then
      AC_MSG_ERROR([CACA support requested, but libcaca 0.99 not found])
   fi
fi

AM_CONDITIONAL([HAVE_CACA], [test "x$have_caca" = "xyes"])

dnl ---------------------------------------------
dnl Check solaris framebuffer device support
dnl ---------------------------------------------

AC_CHECK_HEADER(sys/fbio.h, ac_have_sunfb=yes,)
AM_CONDITIONAL(HAVE_SUNFB, [test "x$ac_have_sunfb" = "xyes"])


dnl ---------------------------------------------
dnl Check for Sun DGA
dnl ---------------------------------------------

saved_LDFLAGS="$LDFLAGS"
LDFLAGS="$LDFLAGS -L/usr/openwin/lib"
saved_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="$CPPFLAGS -I/usr/openwin/include"
AC_CHECK_LIB(dga, XDgaGrabDrawable, [
	AC_CHECK_HEADER(dga/dga.h, [
	        SUNDGA_CFLAGS="-I/usr/openwin/include"
		SUNDGA_LIBS="-L/usr/openwin/lib -R/usr/openwin/lib -ldga"
		ac_have_sundga=yes
	])
])
LDFLAGS="$saved_LDFLAGS"
CPPFLAGS="$saved_CPPFLAGS"
AM_CONDITIONAL(HAVE_SUNDGA, [test "x$ac_have_sundga" = "xyes"])
AC_SUBST(SUNDGA_CFLAGS)
AC_SUBST(SUNDGA_LIBS)


dnl ---------------------------------------------
dnl Check linux framebuffer device support
dnl ---------------------------------------------

AC_CHECK_HEADER(linux/fb.h,
                [AC_DEFINE(HAVE_FB,1,[Define this if you have linux framebuffer support])
                 have_fb=yes],)
AC_ARG_ENABLE(fb, AS_HELP_STRING([--disable-fb], [do not build linux framebuffer support]),
	      have_fb=$enableval)
AM_CONDITIONAL(HAVE_FB, [test "x$have_fb" = "xyes"])


dnl ---------------------------------------------
dnl Check whether to build Mac OS X video output driver
dnl ---------------------------------------------

AC_ARG_ENABLE(macosx_video, AS_HELP_STRING([--enable-macosx-video], [enable support for Mac OS X OpenGL video output]),
              have_macosx_video=$enableval)
AM_CONDITIONAL(HAVE_MACOSX_VIDEO, [test "x$have_macosx_video" = "xyes"])


dnl ---------------------------------------------
dnl Check whether to build Mac OS X audio output driver
dnl ---------------------------------------------

AC_ARG_ENABLE(coreaudio, AS_HELP_STRING([--enable-coreaudio], [enable support for Mac OS X Coreaudio output]),
              have_coreaudio=$enableval)
AM_CONDITIONAL(HAVE_COREAUDIO, [test "x$have_coreaudio" = "xyes"])


dnl ---------------------------------------------
dnl Check for DirectFB
dnl ---------------------------------------------
AC_ARG_ENABLE(directfb,
	AS_HELP_STRING([--enable-directfb], [enable use of DirectFB]),
		enable_directfb=$enableval,
		enable_directfb=no)

if test "x$enable_directfb" = "xyes"; then
  PKG_CHECK_MODULES([DIRECTFB], [directfb >= 0.9.22], [have_directfb="yes"], [have_directfb="no"])
fi

AC_SUBST(DIRECTFB_CFLAGS)
AC_SUBST(DIRECTFB_LIBS)
AM_CONDITIONAL(HAVE_DIRECTFB, test "x$have_directfb" = "xyes" )


dnl ---------------------------------------------
dnl check for SDL
dnl ---------------------------------------------

AC_ARG_WITH([sdl],
	AS_HELP_STRING([--without-sdl], [Build without SDL video output]))

if test "x$with_sdl" != "xno"; then
   PKG_CHECK_MODULES([SDL], [sdl], [have_sdl=yes], [have_sdl=no])
   if test "x$with_sdl" = "xyes" && test "x$have_sdl" = "xno"; then
      AC_MSG_ERROR([SDL support requested, but SDL not found])
   elif test "x$have_sdl" = "xyes"; then
      AC_DEFINE([HAVE_SDL], [1], [Define this if you have SDL installed])
   fi
fi

AM_CONDITIONAL([HAVE_SDL], [test "x$have_sdl" = "xyes"])

AC_SUBST([SDL_CFLAGS])
AC_SUBST([SDL_LIBS])

dnl ---------------------------------------------
dnl check for Libstk
dnl ---------------------------------------------

AC_ARG_WITH([libstk],
	AS_HELP_STRING([--with-libstk], [Build with STK surface video driver]))

if test "x$with_libstk" = "xyes"; then
   PKG_CHECK_MODULES([LIBSTK], [libstk >= 0.2.0], [have_libstk=yes], [have_libstk=no])
   if test "x$with_libstk" = "xyes" && test "x$have_libstk" = "xno"; then
      AC_MSG_ERROR([libstk support requested, but libstk not found])
   fi
fi

AM_CONDITIONAL([HAVE_STK], [test "x$have_libstk" = "xyes"])

dnl ---------------------------------------------
dnl check for pulseaudio
dnl ---------------------------------------------

AC_ARG_WITH([pulseaudio],
   AS_HELP_STRING([--without-pulseaudio], [Do not build Pulseaudio support]))

if test "x$with_pulseaudio" != "xno"; then
   PKG_CHECK_MODULES([PULSEAUDIO], [libpulse], [have_pulseaudio="yes"], [have_pulseaudio="no"])
fi

AM_CONDITIONAL(HAVE_PULSEAUDIO, [test "x$have_pulseaudio" = x"yes"])
AC_SUBST([PULSEAUDIO_CFLAGS])
AC_SUBST([PULSEAUDIO_LIBS])

dnl ---------------------------------------------
dnl check for DirectX
dnl ---------------------------------------------

AM_PATH_DIRECTX()


dnl ---------------------------------------------
dnl dxr3 / hollywood plus card
dnl ---------------------------------------------

case "$host_or_hostalias" in
  *-linux*)
    AC_CHECK_DXR3()
    if test "x$have_libfame" = "xyes" ; then
      AC_DEFINE_UNQUOTED(HAVE_LIBFAME,1,[Define this if you have libfame mpeg encoder installed (fame.sf.net)])
      AM_PATH_LIBFAME(0.8.10, 
        AC_DEFINE(HAVE_NEW_LIBFAME,1,[Define this if you have libfame 0.8.10 or above]))
    fi
    if test "x$have_librte" = "xyes" ; then
      AC_DEFINE_UNQUOTED(HAVE_LIBRTE,1,[Define this if you have librte mpeg encoder installed (zapping.sf.net)])
    fi
    ;;
  *)
    have_dxr3="no"
    have_libfame="no"
    have_librte="no"
    have_encoder="no"
    ;;
esac
AM_CONDITIONAL(HAVE_DXR3, test "x$have_dxr3" = "xyes")
AM_CONDITIONAL(HAVE_LIBFAME, test "x$have_libfame" = "xyes")
AM_CONDITIONAL(HAVE_LIBRTE, test "x$have_librte" = "xyes")


dnl ---------------------------------------------
dnl Vidix/libdha
dnl ---------------------------------------------

AC_LINUX_PATH(/usr/src/linux)
AC_SUBST([LINUX_INCLUDE])

AC_ARG_ENABLE(vidix, AS_HELP_STRING([--disable-vidix], [do not build vidix support]),
	      check_vidix=$enableval, check_vidix=yes)
AC_ARG_ENABLE(dha-kmod, AS_HELP_STRING([--enable-dha-kmod], [build DHA kernel module]),
	      enable_dha_kmod=$enableval,enable_dha_kmod=no)

enable_vidix="no"

AC_MSG_CHECKING(for vidix support)
if test "x$check_vidix" = "xyes" -a "x$ac_cv_prog_AWK" != "xno"; then
  if test "x$no_x" != "xyes" -o "x$have_fb" = "xyes"; then
    case "$host_or_hostalias" in
      i?86-*-linux* | k?-*-linux* | athlon-*-linux*)
        enable_vidix="yes"
        enable_linux="yes"
        ;;
      i?86-*-freebsd* | k?-*-freebsd* | athlon-*-freebsd* | i?86-*-kfreebsd*)
        enable_vidix="yes"
        enable_dha_kmod="no"
        ;;
      *)
        enable_dha_kmod="no"
        enable_vidix="no"
        ;;
    esac
  fi
fi
AC_MSG_RESULT($enable_vidix)

AC_MSG_CHECKING(for DHA linux kernel module build)
if test "x$enable_dha_kmod" = "xyes"; then
  AC_MSG_RESULT(yes)
else
  AC_MSG_RESULT(no)
fi

AM_CONDITIONAL(HAVE_VIDIX, test "x$enable_vidix" = "xyes")
AM_CONDITIONAL(HAVE_LINUX, test "x$enable_linux" = "xyes")
AM_CONDITIONAL(BUILD_DHA_KMOD, test "x$enable_dha_kmod" = "xyes")
AC_CHECK_PROG(MKNOD, mknod, mknod, no)
AC_CHECK_PROG(DEPMOD, depmod, depmod, no, "$PATH:/sbin")


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
   AC_DEFINE([HAVE_LIBMPC], [1], [Define if external libmpc is used])
else
   AC_MSG_RESULT([Use included libmusepack])
fi

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
dnl OSS style audio interface
dnl ---------------------------------------------
AC_ARG_ENABLE([oss],
	AS_HELP_STRING([--disable-oss], [Do not build OSS audio output support]))

if test "x$enable_oss" != "xno"; then
   AC_CHECK_HEADERS([sys/soundcard.h machine/soundcard.h soundcard.h], [break])
   AC_CHECK_DECL([SNDCTL_DSP_SETFRAGMENT], [have_ossaudio=yes], [], [
     #ifdef HAVE_SYS_SOUNDCARD_H
     # include <sys/soundcard.h>
     #endif
     #ifdef HAVE_MACHINE_SOUNDCARD_H
     # include <sys/soundcard.h>
     #endif
     #ifdef HAVE_SOUNDCARD_H
     # include <soundcard.h>
     #endif
   ])

   if test "x$have_ossaudio" = "xyes"; then
      AC_IOCTL_REQUEST
   fi
fi

AM_CONDITIONAL(HAVE_OSS, test "x$have_ossaudio" = "xyes")


dnl ---------------------------------------------
dnl Alsa support
dnl ---------------------------------------------

AC_ARG_WITH([alsa],
   AS_HELP_STRING([--without-alsa], [Build without ALSA audio output]))

if test "x$with_alsa" != "xno"; then
   PKG_CHECK_MODULES([ALSA], [alsa >= 0.9.0], [have_alsa=yes], [have_alsa=no])
   AC_SUBST([ALSA_LIBS])
   AC_SUBST([ALSA_CFLAGS])
   if test "x$have_alsa" = "xyes"; then
      AC_DEFINE([HAVE_ALSA], [1], [Define this if you have ALSA installed])
   elif test "x$with_alsa" = "xyes"; then
      AC_MSG_ERROR([ALSA support requested but not found.])
   fi
fi

AM_CONDITIONAL([HAVE_ALSA], [test "x$have_alsa" = "xyes"])

dnl ---------------------------------------------
dnl ESD support
dnl ---------------------------------------------

AC_ARG_WITH([esound],
	AS_HELP_STRING([--without-esound], [Build without ESounD audio output]))

if test "x$with_esound" != "xno"; then
   PKG_CHECK_MODULES([ESD], [esound], [have_esound=yes], [have_esound=no])
   if test "x$with_esound" = "xyes" && test "x$have_esound" = "xno"; then
      AC_MSG_ERROR([ESounD support requested, but libesd not found])
   elif test "x$have_esound" = "xyes"; then
      AC_DEFINE([HAVE_ESD], [1], [Define this if you have ESounD installed])
   fi
fi

AM_CONDITIONAL([HAVE_ESD], [test "x$have_esound" = "xyes"])

AC_SUBST([ESD_CFLAGS])
AC_SUBST([ESD_LIBS])


dnl ---------------------------------------------
dnl ARTS support
dnl ---------------------------------------------

AC_ARG_WITH([arts],
  AS_HELP_STRING([--with-arts], [Build with ARTS audio output]),
  [with_arts=$withval], [with_arts=no])

if test "x$with_arts" = "xyes"; then
  AM_PATH_ARTS(0.9.5,
        AC_DEFINE(HAVE_ARTS,1,[Define this if you have ARTS (libartsc) installed]),
        AC_MSG_RESULT(*** All of ARTS dependent parts will be disabled ***))
else
  no_arts=yes
fi
AM_CONDITIONAL(HAVE_ARTS, test "x$no_arts" != "xyes")


dnl ---------------------------------------------
dnl FusionSound support
dnl ---------------------------------------------

AC_ARG_WITH([fusionsound],
  AS_HELP_STRING([--with-fusionsound], [Build with FunsionSound audio output]),
  [with_fusionsound=$withval], [with_fusionsound=no])

if test "x$with_fusionsound" = "xyes"; then
  PKG_CHECK_MODULES(FUSIONSOUND, fusionsound >= 0.9.23,
      AC_DEFINE(HAVE_FUSIONSOUND,1,[Define to 1 if you have FusionSound.]),
      AC_MSG_RESULT(*** All of FusionSound dependent parts will be disabled ***))
  AC_SUBST(FUSIONSOUND_CFLAGS)
  AC_SUBST(FUSIONSOUND_LIBS)
else
  no_fusionsound=yes
fi
AM_CONDITIONAL(HAVE_FUSIONSOUND, test "x$no_fusionsound" != "xyes")


dnl ---------------------------------------------
dnl JACK support
dnl ---------------------------------------------

AC_ARG_WITH([jack],
	AS_HELP_STRING([--without-jack], [Build without Jack support]))

if test "x$with_jack" != "xno"; then
   PKG_CHECK_MODULES([JACK], [jack >= 0.100], [have_jack=yes], [have_jack=no])
   
   if test "x$with_jack" = "xyes" && test "x$have_jack" = "xno"; then
      AC_MSG_ERROR([Jack support requested, but Jack not found])
   fi
fi

AM_CONDITIONAL([HAVE_JACK], [test "x$have_jack" = "xyes"])


dnl ---------------------------------------------
dnl gnome-vfs support
dnl ---------------------------------------------

AC_ARG_ENABLE([gnomevfs],
  AS_HELP_STRING([--disable-gnomevfs], [do not build gnome-vfs support]),
  [with_gnome_vfs=$enableval], [with_gnome_vfs=yes])

if test "x$with_gnome_vfs" = "xyes"; then
  PKG_CHECK_MODULES(GNOME_VFS, gnome-vfs-2.0,
                no_gnome_vfs=no,
		no_gnome_vfs=yes)
  AC_SUBST(GNOME_VFS_CFLAGS)
  AC_SUBST(GNOME_VFS_LIBS)
  if test "x$no_gnome_vfs" != "xyes"; then
    AC_DEFINE(HAVE_GNOME_VFS,1,[Define this if you have gnome-vfs installed])
  else
    AC_MSG_RESULT(*** All of the gnome-vfs dependent parts will be disabled ***)
  fi
else
  no_gnome_vfs=yes
fi
AM_CONDITIONAL(HAVE_GNOME_VFS, test "x$no_gnome_vfs" != "xyes")

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
dnl libsmbclient support
dnl ---------------------------------------------

AC_ARG_ENABLE([samba],
  AS_HELP_STRING([--disable-samba], [do not build Samba support]),
  [with_samba=$enableval], [with_samba=yes])

if test "x$with_samba" = "xyes"; then
  AC_CHECK_LIB(smbclient, smbc_init,
	[ AC_CHECK_HEADER(libsmbclient.h,
		[ have_libsmbclient=yes
		  LIBSMBCLIENT_LIBS="-lsmbclient" ], 
		AC_MSG_RESULT([*** All libsmbclient dependent parts will be disabled ***]))],
	AC_MSG_RESULT([*** All libsmbclient dependent parts will be disabled ***]))
  AC_SUBST(LIBSMBCLIENT_LIBS)
fi
AM_CONDITIONAL(HAVE_LIBSMBCLIENT, test "x$have_libsmbclient" = "xyes")


dnl ---------------------------------------------
dnl SUN style audio interface
dnl ---------------------------------------------

AC_MSG_CHECKING(for Sun audio support)
have_sunaudio=no
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
	    #include <sys/types.h>
	    #include <sys/audioio.h>
	]], [[
	    audio_info_t audio_info;
	    AUDIO_INITINFO(&audio_info);
	]])],[
	    have_sunaudio=yes
	],[])
AC_MSG_RESULT($have_sunaudio)
AM_CONDITIONAL(HAVE_SUNAUDIO, test "x$have_sunaudio" = "xyes")

if test "x$have_sunaudio" = "xyes"; then
   dnl NetBSD and OpenBSD don't have this, but check for it
   dnl rather than assuming that it doesn't happen elsewhere.
   AC_CHECK_MEMBERS([audio_info_t.output_muted])
fi


dnl ---------------------------------------------
dnl IRIX style audio interface
dnl ---------------------------------------------

AM_CHECK_IRIXAL([AC_DEFINE(HAVE_IRIXAL,1,
			[Define this if you have a usable IRIX al interface available])],
	[AC_MSG_RESULT([*** All of IRIX AL dependent parts will be disabled ***])])
AM_CONDITIONAL(HAVE_IRIXAL, [test "x$am_cv_have_irixal" = xyes])


dnl ---------------------------------------------
dnl Solaris kstat kernel statistics
dnl ---------------------------------------------

AC_CHECK_LIB(kstat, kstat_open,
	[KSTAT_LIBS=-lkstat
	 AC_DEFINE(HAVE_KSTAT,1,[Define this if you have kernel statistics available via kstat interface])])
AC_SUBST(KSTAT_LIBS)


dnl ---------------------------------------------
dnl cdrom ioctls
dnl ---------------------------------------------

AC_CHECK_HEADERS([linux/cdrom.h sys/dvdio.h], [break])
AC_CHECK_HEADERS([sys/cdio.h sys/scsiio.h])
AM_CHECK_CDROM_IOCTLS(
	[AC_DEFINE(HAVE_CDROM_IOCTLS,1,[Define this if you have CDROM ioctls])],
	[AC_MSG_RESULT([*** (S)VCD support will be disabled ***])])
AM_CONDITIONAL(HAVE_CDROM_IOCTLS, [test "x$have_cdrom_ioctls" = "xyes"])


dnl ---------------------------------------------
dnl check for a usable version of libdvdnav
dnl ---------------------------------------------

AC_ARG_WITH(external-dvdnav, AS_HELP_STRING([--with-external-dvdnav], [use external dvdnav library (not recommended)]),
            [external_dvdnav="$withval"], [no_dvdnav="yes"; external_dvdnav="no"])

if test "x$external_dvdnav" = "xyes"; then
  AM_PATH_DVDNAV(0.1.9,
	  AC_DEFINE(HAVE_DVDNAV,1,[Define this if you have a suitable version of libdvdnav]),
	  [AC_MSG_RESULT([*** no usable version of libdvdnav found, using internal copy ***])])
else
  AC_MSG_RESULT([Use included DVDNAV support])
fi

AM_CONDITIONAL(HAVE_DVDNAV, [test "x$no_dvdnav" != "xyes"])


dnl ---------------------------------------------
dnl Video CD
dnl ---------------------------------------------

AC_ARG_ENABLE(vcd, AS_HELP_STRING([--disable-vcd], [do not compile VCD plugin]),
              enable_vcd=$enableval, enable_vcd=yes)

dnl Force build of both vcd plugins, for now.
dnl AC_ARG_ENABLE(vcdo, AS_HELP_STRING([--disable-vcdo], [do not compile old VCD plugin]),
dnl               enable_vcdo=$enableval, enable_vcdo=yes)
dnl
enable_vcdo="yes"

AC_ARG_WITH(internal-vcdlibs, AS_HELP_STRING([--with-internal-vcdlibs], [force using internal libcdio/libvcd/libvcdinfo]),
            [internal_vcdnav="$withval"], [internal_vcdnav="no"])

if test "x$enable_vcd" = "xyes"; then
  dnl empty_array_size
  AC_MSG_CHECKING([how to create empty arrays])

  empty_array_size="xxx"
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[struct { int foo; int bar[]; } doo;]])],[empty_array_size=""],[])

  if test "x$empty_array_size" = "xxxx";then
   AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[struct { int foo; int bar[0]; } doo;]])],[empty_array_size="0"],[])
  fi

  if test "x$empty_array_size" = "xxxx"
  then
    AC_MSG_ERROR([compiler is unable to creaty empty arrays])
  else
    AC_DEFINE_UNQUOTED(EMPTY_ARRAY_SIZE, $empty_array_size, 
	   [what to put between the brackets for empty arrays])
    changequote(`,')
    msg="[${empty_array_size}]"
    changequote([,])
  AC_MSG_RESULT($msg)
  fi
  dnl empty_array_size

  if test "x$internal_vcdnav" = "xno" ; then
    PKG_CHECK_MODULES([LIBCDIO], [libcdio >= 0.71], [], [internal_vcdnav=yes])
    PKG_CHECK_MODULES([LIBVCDINFO], [libvcdinfo >= 0.7.23], [], [internal_vcdnav=yes])
    if test "x$internal_vcdnav" = "xyes"; then
       AC_MSG_RESULT([Use included libcdio/libvcdinfo support])
    fi
  fi

  dnl check twice, fallback is internal copy
  if test "x$internal_vcdnav" = "xno"; then
     AC_DEFINE([HAVE_VCDNAV], [1], [Define this if you use external libcdio/libvcdinfo])
  else
    AC_DEFINE_UNQUOTED(HOST_ARCH, "$host_os/$host_cpu", [host os/cpu identifier])

    AC_DEFINE(_DEVELOPMENT_, [], enable warnings about being development release)
    AC_HEADER_STDC
    AC_CHECK_HEADERS(sys/stat.h stdint.h glob.h inttypes.h stdbool.h)

    if test "x$ac_cv_header_stdint_h" != "xyes" 
     then
       AC_CHECK_SIZEOF(int, 4)
       AC_CHECK_SIZEOF(long, 4)
       AC_CHECK_SIZEOF(long long, 8)
    fi

    dnl ISOC99_PRAGMA
    AC_MSG_CHECKING([whether $CC supports ISOC99 _Pragma()])
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[_Pragma("pack(1)")]])],[
      ISOC99_PRAGMA=yes 
      AC_DEFINE(HAVE_ISOC99_PRAGMA, [], [Supports ISO _Pragma() macro])
    ],[ISOC99_PRAGMA=no])
    AC_MSG_RESULT($ISOC99_PRAGMA)

    dnl
    dnl bitfield order
    dnl
    AC_MSG_CHECKING([bitfield ordering in structs])

    dnl basic compile test for all platforms
    AC_COMPILE_IFELSE([
int main() {
  struct { char bit_0:1, bit_12:2, bit_345:3, bit_67:2; }
#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ > 4)
  __attribute__((packed))
#endif
  bf = { 1,1,1,1 };
  switch (0) case 0: case sizeof(bf) == 1:;
  return 0;
}
], [], AC_MSG_ERROR([compiler doesn't support bitfield structs]))


    dnl run test
    AC_RUN_IFELSE([
int main() {
  struct { char bit_0:1, bit_12:2, bit_345:3, bit_67:2; }
#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ > 4)
  __attribute__((packed))
#endif
  bf = { 1,1,1,1 };
  if (sizeof (bf) != 1) return 1;
  return *((unsigned char*) &bf) != 0x4b; }
], bf_lsbf=1, [
  AC_RUN_IFELSE([
int main() {
  struct { char bit_0:1, bit_12:2, bit_345:3, bit_67:2; }
#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ > 4)
  __attribute__((packed))
#endif
 bf = { 1,1,1,1 };
 if (sizeof (bf) != 1) return 1;
 return *((unsigned char*) &bf) != 0xa5; }
], bf_lsbf=0, AC_MSG_ERROR([unsupported bitfield ordering]))
  ],
  [case "$host" in
     *-*-mingw32* | *-*-cygwin*)
       bf_lsbf=1
     ;;
     *)
       AC_MSG_RESULT([unknown])
       AC_MSG_ERROR([value of bitfield test isn't known for $host
*********************************************************************
Value of bitfield test can't be found out for cross-compiling and we
don't know its value for host "$host".

Because it's needed for VCD plugin, disable VCD by configure option
--disable-vcd or use external VCD library.
*********************************************************************])
  esac]
)

    if test "x$cross_compiling" = "xyes"; then
      TEXT=" (guessed)"
    else
      TEXT=""
    fi
    if test "x$bf_lsbf" = "x1"; then
     AC_MSG_RESULT(LSBF${TEXT})
     AC_DEFINE(BITFIELD_LSBF, [], [compiler does lsbf in struct bitfields])
    else
     AC_MSG_RESULT(MSBF${TEXT})
    fi

    AC_CHECK_HEADERS([errno.h fcntl.h \
		     stdbool.h  stdlib.h stdint.h stdio.h string.h \
		     strings.h linux/version.h sys/cdio.h sys/stat.h \
		     sys/types.h ])

    LIBCDIO_CFLAGS='-I$(top_srcdir)/src/input/vcd/libcdio'
    LIBCDIO_LIBS='$(top_builddir)/src/input/vcd/libcdio/libcdio.la'
    LIBISO9660_LIBS='$(top_builddir)/src/input/vcd/libcdio/libiso9660.la'
    LIBVCD_CFLAGS='-I$(top_srcdir)/src/input/vcd/libvcd'
    LIBVCD_LIBS='$(top_builddir)/src/input/vcd/libvcd/libvcd.la'
    LIBVCDINFO_LIBS='$(top_builddir)/src/input/vcd/libvcd/libvcdinfo.la'

    case $host_os in
         darwin*)
           AC_CHECK_HEADERS(IOKit/IOKitLib.h CoreFoundation/CFBase.h, 
                            [have_iokit_h="yes"])
           if test "x$have_iokit_h" = "xyes" ; then 
             AC_DEFINE([HAVE_DARWIN_CDROM], [1],
	          [Define 1 if you have Darwin OS X-type CD-ROM support])
           fi
         ;;
         linux*)
            AC_CHECK_HEADERS(linux/version.h)
            AC_CHECK_HEADERS(linux/cdrom.h, [have_linux_cdrom_h="yes"])
	    if test "x$have_linux_cdrom_h" = "xyes" ; then
	       AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[
#include <linux/cdrom.h>
struct cdrom_generic_command test;
int has_timeout=sizeof(test.timeout);]])],
                [AC_DEFINE([HAVE_LINUX_CDROM_TIMEOUT], [1], 
                       [Define 1 if timeout is in cdrom_generic_command struct])],[])
	       AC_DEFINE([HAVE_LINUX_CDROM], [1], 
                       [Define 1 if you have Linux-type CD-ROM support])
	    fi
	 ;;
         bsdi*)
            AC_CHECK_HEADERS(dvd.h, [have_bsdi_dvd_h="yes"])
	    if test "x$have_bsdi_dvd_h" = "xyes" ; then
     	      AC_DEFINE([HAVE_BSDI_CDROM], [1], 
                        [Define 1 if you have BSDI-type CD-ROM support])
	    fi
	 ;;
         sunos*|sun*|solaris*)
            AC_CHECK_HEADERS(sys/cdio.h)
     	    AC_DEFINE([HAVE_SOLARIS_CDROM], [1], 
                      [Define 1 if you have Solaris CD-ROM support])
	 ;;
         cygwin*)
	    AC_DEFINE([CYGWIN], [1], 
                      [Define 1 if you are compiling using cygwin])
     	    AC_DEFINE([HAVE_WIN32_CDROM], [1], 
                      [Define 1 if you have MinGW CD-ROM support])
	    LIBCDIO_LIBS="$LIBCDIO_LIBS -lwinmm"
	    LIBVCD_LIBS="$LIBVCD_LIBS -lwinmm"
	 ;;
         mingw*)
	    AC_DEFINE([MINGW32], [1], 
                      [Define 1 if you are compiling using MinGW])
     	    AC_DEFINE([HAVE_WIN32_CDROM], [1], 
                      [Define 1 if you have MinGW CD-ROM support])
	 ;;
         freebsd4.*|kfreebsd*-gnu)
     	    AC_DEFINE([HAVE_FREEBSD_CDROM], [1], 
                      [Define 1 if you have FreeBSD CD-ROM support])
	 ;;
         *)
	    AC_MSG_WARN(Don't have OS CD-reading support for ${host_os}...)
	    AC_MSG_WARN(Will use generic support.)
	 ;;
    esac
    AC_SUBST(LINUX_CDROM_TIMEOUT)
    AC_SUBST(HAVE_BSDI_CDROM)
    AC_SUBST(HAVE_DARWIN_CDROM)
    AC_SUBST(HAVE_FREEBSD_CDROM)
    AC_SUBST(HAVE_LINUX_CDROM)
    AC_SUBST(HAVE_SOLARIS_CDROM)
    AC_SUBST(HAVE_WIN32_CDROM)
    AC_SUBST(LINUX_CDROM_TIMEOUT)
    AC_SUBST(LIBVCD_SYSDEP)

    AC_CHECK_FUNCS( bzero memcpy )

    AC_CHECK_MEMBER([struct tm.tm_gmtoff],
                    [AC_DEFINE(HAVE_TM_GMTOFF, 1,
                           [Define if struct tm has the tm_gmtoff member.])],
                     ,
                     [#include <time.h>])
  fi
fi

AC_DEFINE([LIBCDIO_CONFIG_H], 1, [Get of rid system libcdio build configuration])
AC_DEFINE([EXTERNAL_LIBCDIO_CONFIG_H], 1, [Get of rid system libcdio build configuration])
AC_SUBST(LIBCDIO_CFLAGS)
AC_SUBST(LIBCDIO_LIBS)
AC_SUBST(LIBISO9660_LIBS)
AC_SUBST(LIBVCD_CFLAGS)
AC_SUBST(LIBVCD_LIBS)
AC_SUBST(LIBVCDINFO_LIBS)
AM_CONDITIONAL(HAVE_VCDNAV, [test "x$internal_vcdnav" = "xno"])
AM_CONDITIONAL(ENABLE_VCD, [test "x$enable_vcd" = "xyes"])


dnl ---------------------------------------------
dnl ASF build can be optional
dnl ---------------------------------------------

AC_ARG_ENABLE([asf], AS_HELP_STRING([--disable-asf], [do not build ASF demuxer]))
AM_CONDITIONAL(BUILD_ASF, test "x$enable_asf" != "xno")


dnl ---------------------------------------------
dnl Nosefart build can be optional
dnl ---------------------------------------------

AC_ARG_ENABLE([nosefart], AS_HELP_STRING([--disable-nosefart], [do not build nosefart player]))
if test "x$enable_nosefart" != "xno"; then
   AC_DEFINE([HAVE_NOSEFART], 1, [Define this if Nosefart is being built.])
fi
AM_CONDITIONAL(BUILD_NOSEFART, test "x$enable_nosefart" != "xno")

dnl ---------------------------------------------
dnl FAAD build can be optional
dnl ---------------------------------------------

AC_ARG_ENABLE([faad], AS_HELP_STRING([--disable-faad], [do not build FAAD decoder]))
AC_ARG_WITH(external-libfaad, AS_HELP_STRING([--with-external-libfaad], [use external libfaad (recommended)]),
            [external_libfaad="$withval"], [external_libfaad="yes"])

have_faad=no
if test "x$enable_faad" = "xno"; then
  AC_MSG_RESULT([faad support disabled])
elif test "x$with_external_libfaad" != "xno"; then
  AC_CHECK_LIB([faad], [NeAACDecInit], [have_faad=yes])
  if test "x$have_faad" = xyes; then 
    AC_CHECK_HEADERS([faad.h], , [have_faad=no])
    if test "x$have_faad" != "xyes"; then
      AC_MSG_ERROR([Unable to find libfaad])
    fi
    AC_DEFINE([EXTERNAL_LIBFAAD], [1], [Define if external libfaad is used])
  fi
else
  AC_MSG_RESULT([Use included libfaad])
fi

AM_CONDITIONAL(BUILD_FAAD, test "x$enable_faad" != "xno")
AM_CONDITIONAL(EXTERNAL_LIBFAAD, test "x$have_faad" = "xyes")

dnl ---------------------------------------------
dnl Optional and external libdts
dnl ---------------------------------------------

AC_ARG_ENABLE(dts, AS_HELP_STRING([--disable-dts], [Disable support for DTS decoding library (default: enabled)]),
              [enable_libdts="$enableval"], [enable_libdts="yes"])
AC_ARG_WITH(external-libdts, AS_HELP_STRING([--with-external-libdts], [use external libdts library (not recommended)]),
            [external_libdts="$withval"], [external_libdts="no"])

have_dts="no"

if test "x$enable_libdts" = "xno"; then
  AC_MSG_RESULT([libdts support disabled])
elif test "x$external_libdts" = "xyes"; then
  PKG_CHECK_MODULES(LIBDTS, [libdts], have_dts=yes, have_dts=no)
  AC_CHECK_HEADERS([dts.h])
  AC_SUBST(LIBDTS_LIBS)
  AC_SUBST(LIBDTS_CFLAGS)
  if test "x$have_dts" = "xno"; then
    AC_MSG_RESULT([*** no usable version of libdts found, using internal copy ***])
  fi
else
  AC_MSG_RESULT([Use included libdts support])
fi

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
case $host in
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

dnl --------------------------------------------
dnl mmap() support
dnl --------------------------------------------

AC_ARG_ENABLE([mmap],
	AS_HELP_STRING([--enable-mmap], [Enable mmap() file loading (default: no)]))

if test "x$enable_mmap" = "xyes"; then
   AC_CHECK_FUNCS([mmap])
fi

dnl ---------------------------------------------
dnl antialising support
dnl ---------------------------------------------

AC_ARG_ENABLE([antialiasing],
     AS_HELP_STRING([--enable-antialiasing], [enable font antialiasing]))

if test "x$enable_antialiasing" = "xyes"; then
   AC_DEFINE(ENABLE_ANTIALIASING,1,[Define this to 1 to enable font antialising.])    
fi

dnl ---------------------------------------------
dnl ip_mreqn
dnl ---------------------------------------------

AC_CHECK_IP_MREQN
if test "x$have_ip_mreqn" = "xyes"; then
  AC_DEFINE(HAVE_IP_MREQN,1,[Define this if you have ip_mreqn in netinet/in.h])
fi

dnl ---------------------------------------------
dnl restrict keyword finding (from gstreamer)
dnl ---------------------------------------------
restrict=""
for restrict_keyword in restrict __restrict__ __restrict; do
  AC_MSG_CHECKING(for restrict keyword $restrict_keyword)
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[ void foo(char * $restrict_keyword p); ]])],[
                 KEYWORD_FOUND=yes && AC_MSG_RESULT(yes) ],[
                 KEYWORD_FOUND=no && AC_MSG_RESULT(no) ])
  if test "x$KEYWORD_FOUND" = xyes; then
    restrict="$restrict_keyword"
    break
  fi
done
if test "x$restrict" = x; then
  AC_MSG_ERROR(No restrict keyword found)
fi
AC_DEFINE_UNQUOTED(restrict, $restrict, [restrict keyword])

dnl ---------------------------------------------
dnl ASM ALIGN is power of two ?
dnl Used by internal FFmpeg and Planar postprocess
dnl ---------------------------------------------
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[ asm (".align 3"); ]])],
  AC_DEFINE([ASMALIGN_1SLN], [1], [define if '.align n' means alignment to (1<<n)-byte boundaries]),
  :)
AH_BOTTOM([#ifdef ASMALIGN_1SLN
# define ASMALIGN(ZEROBITS) ".align " #ZEROBITS "\n\t"
#else
# define ASMALIGN(ZEROBITS) ".align 1<<" #ZEROBITS "\n\t"
#endif
])

dnl ---------------------------------------------
dnl Some extra checks.
dnl ---------------------------------------------

AC_SYS_LARGEFILE
AC_CHECK_LIB(posix4, sched_get_priority_min)
AC_CHECK_FUNCS([vsscanf sigaction sigset getpwuid_r nanosleep lstat memset readlink strchr tzset va_copy])
AC_CHECK_FUNCS([snprintf _snprintf], [some_snprintf="yes"; break])
AC_CHECK_FUNCS([vsnprintf _vsnprintf], [some_vsnprintf="yes"; break])
AC_CHECK_FUNCS([strcasecmp _stricmp], [some_strcasecmp="yes"; break])
AC_CHECK_FUNCS([strncasecmp _strnicmp], [some_strncasecmp="yes"; break])
if test "x$some_snprintf" != "xyes" -o \
        "x$some_vsnprintf" != "xyes" -o \
        "x$some_strcasecmp" != "xyes" -o \
        "x$some_strncasecmp" != "xyes"; then
  AC_MSG_ERROR([required function not found])
fi
AC_FUNC_FSEEKO
AC_CHECK_HEADERS(assert.h byteswap.h malloc.h execinfo.h ucontext.h sys/mman.h sys/mixer.h libgen.h netdb.h dirent.h sys/times.h sys/ioctl.h sys/param.h alloca.h)
AC_REPLACE_FUNCS(asprintf basename gettimeofday setenv strcasestr strndup strpbrk strsep strtok_r timegm unsetenv memmem)

AC_LIBSOURCE(hstrerror.c)
AC_LINK_IFELSE([#include <netdb.h>
int main(void) {
  hstrerror(0);
}], ac_cv_function_system_hstrerror="yes")
AC_CHECK_LIB([resolv], [hstrerror], [
  ac_cv_function_system_hstrerror="yes"
  NET_LIBS="-lresolv $NET_LIBS"
])
AC_SUBST(NET_LIBS)
if test "x$ac_cv_function_system_hstrerror" = "xyes"; then
  AC_DEFINE(HAVE_HSTRERROR, 1, [Define to 1 if you have 'hstrerror' in <netdb.h>])

else
  AC_LIBOBJ(hstrerror)
fi

AC_LIBSOURCE(dirent_msvc.c)
AC_CHECK_FUNC(opendir,
  [AC_DEFINE(HAVE_OPENDIR, 1, [Define to 1 if you have 'opendir' function])],
  [if test "x$SYS" = "xmingw32"; then
    AC_LIBOBJ(dirent_msvc)
  else
    AC_MSG_ERROR([dirent is needed (opendir, readdir, ...)])
  fi])

XINE_CHECK_MINMAX([
    AC_DEFINE(HAVE_MAX_MACRO, 1, [Define to 1 if you have 'MAX' macro in sys/param.h])
    AC_DEFINE(HAVE_MIN_MACRO, 1, [Define to 1 if you have 'MIN' macro in sys/param.h])
  ],[])

dnl ---------------------------------------------
dnl cflags and debug cflags
dnl ---------------------------------------------
 
AC_SUBST(DEBUG_CFLAGS)
DEBUG_CFLAGS="-g -DDEBUG $CFLAGS"

dnl dummy
ASFLAGS="$ASFLAGS"
AC_SUBST(ASFLAGS)

dnl ---------------------------------------------
dnl Check for some __attribute__ support needed
dnl ---------------------------------------------

CC_ATTRIBUTE_ALIGNED

CC_ATTRIBUTE_VISIBILITY([protected], [visibility_export="protected"],
  [CC_ATTRIBUTE_VISIBILITY([default], [visibility_export="default"])]
)

if test "x$visibility_export" != "x"; then
  CC_FLAG_VISIBILITY([
    AC_DEFINE([EXPORTED], [__attribute__((visibility("default")))],
      [Mark a symbol as being exported if visibility is changed])
    VISIBILITY_FLAG="-fvisibility=hidden"
    ], [AC_DEFINE([EXPORTED], [], [Dummy mark as being exported])
  ])
else
  AC_DEFINE([EXPORTED], [], [Dummy mark as being exported])
fi

AC_SUBST([VISIBILITY_FLAG])

CC_ATTRIBUTE_SENTINEL
CC_ATTRIBUTE_FORMAT
CC_ATTRIBUTE_FORMAT_ARG
CC_ATTRIBUTE_DEPRECATED
CC_ATTRIBUTE_UNUSED
CC_ATTRIBUTE_MALLOC
CC_ATTRIBUTE_PACKED
CC_ATTRIBUTE_CONST

AC_OPTIMIZATIONS

arch_x86="no"
enable_armv4l="no"

case "$host_or_hostalias" in
  i?86-* | k?-* | athlon-* | pentium*-)
    dnl enable x86 specific parts of the code
    dnl
    dnl all of this stuff needs gcc/gas; it uses gnu c compiler extensions
    dnl like the extended asm() or __attribute(__cdecl__), or other direct
    dnl mmx/sse/3dnow assembler instructions. 
    dnl
    AC_DEFINE_UNQUOTED(ARCH_X86_32,,[Define this if you're running x86 architecture 32 bits])
    AC_DEFINE(FPM_INTEL,1,[Define to select libmad fixed point arithmetic implementation])
    arch_x86="32"
    enable_impure_text="yes"

    case "$host_or_hostalias" in
      *-*-cygwin) 
        LIBS="$LIBS @INTLLIBS@ -lkernel32"
        ;;
    esac
    ;;
  x86_64-*)
    AC_DEFINE_UNQUOTED(ARCH_X86_64,,[Define this if you're running x86 architecture 64 bits])
    AC_DEFINE(FPM_64BIT,1,[Define to select libmad fixed point arithmetic implementation])
    arch_x86="64"
    ;;
  powerpc-*-darwin*)
    dnl avoid ppc compilation crash
    AS="$CC"
    AC_DEFINE_UNQUOTED(FPM_PPC,,[Define to select libmad fixed point arithmetic implementation])
    AC_DEFINE_UNQUOTED(ARCH_PPC,,[Define this if you're running PowerPC architecture])

    AC_CHECK_HEADER([altivec.h], , enable_altivec=no)
    
    if test "x$enable_altivec" != xno; then
        AC_DEFINE_UNQUOTED(ENABLE_ALTIVEC,,[Define this if you want to use altivec on PowerPC CPUs])
        CFLAGS="$CFLAGS -faltivec -maltivec"
        LIBMPEG2_CFLAGS="$LIBMPEG2_CFLAGS -force_cpusubtype_ALL -faltivec -maltivec"
    fi
    ;;
  ppc-*-linux* | powerpc-*) 
    dnl avoid ppc compilation crash
    AS="$CC"
    AC_DEFINE_UNQUOTED(FPM_PPC,,[Define to select libmad fixed point arithmetic implementation])
    AC_DEFINE_UNQUOTED(ARCH_PPC,,[Define this if you're running PowerPC architecture])

    AC_CHECK_HEADER([altivec.h], , enable_altivec=no)
    
    if test "x$enable_altivec" != xno; then
        AC_DEFINE_UNQUOTED(ENABLE_ALTIVEC,,[Define this if you have a Motorola 74xx CPU])
        CFLAGS="$CFLAGS -maltivec"
        LIBMPEG2_CFLAGS="$LIBMPEG2_CFLAGS -maltivec"
    fi
    ;;
  sparc*-*-linux*)
    if test "x$enable_vis" != xno; then
      has_vis=yes
    fi

    AC_DEFINE_UNQUOTED(FPM_SPARC,,[Define to select libmad fixed point arithmetic implementation])
    AC_DEFINE_UNQUOTED(ARCH_SPARC,,[Define this if you're running SPARC architecture])
    ;;
  sparc-*-solaris*)
    if test "$GCC" = yes; then
      case `$CC --version 2>/dev/null` in
	1.*|2.*) ;;
        *)
          if test "x$enable_vis" != xno; then
	     has_vis=yes
          fi
	  ;;
      esac
  
      AC_DEFINE_UNQUOTED(FPM_SPARC,,[Define to select libmad fixed point arithmetic implementation])
    else
      AC_DEFINE_UNQUOTED(FPM_64BIT,,[Define to select libmad fixed point arithmetic implementation])
    fi
    AC_DEFINE_UNQUOTED(ARCH_SPARC,,[Define this if you're running SPARC architecture])
    ;;
  mips-*)
    AC_DEFINE_UNQUOTED(FPM_MIPS,,[Define to select libmad fixed point arithmetic implementation])
    ;;
  alphaev56-* | alpha* | ia64-* | hppa*-linux-*)
    AC_DEFINE_UNQUOTED(FPM_64BIT,,[Define to select libmad fixed point arithmetic implementation])
    AC_DEFINE_UNQUOTED(ARCH_ALPHA,,[Define this if you're running Alpha architecture])
    ;;
  armv4l-*-linux*)
    AC_DEFINE_UNQUOTED(FPM_ARM,,[Define to select libmad fixed point arithmetic implementation])
    AC_DEFINE_UNQUOTED(ARCH_ARM,,[Define this if you're running ARM architecture])
    enable_armv4l="yes"
    ;;
  *)
    AC_DEFINE_UNQUOTED(FPM_DEFAULT,,[Define to select libmad fixed point arithmetic implementation])
    ;;
esac

if test "x$has_vis" = "xyes"; then
   AC_DEFINE_UNQUOTED(ENABLE_VIS,,[Define this if you have Sun UltraSPARC CPU])
   case "$CFLAGS" in
     *-mcpu=*) ;;
     *) CFLAGS="$CFLAGS -mcpu=v9" ;;
   esac
fi
AM_CONDITIONAL(ENABLE_VIS, test "x$has_vis" = "xyes")

if test "x$arch_x86" != "xno"; then
   AC_DEFINE_UNQUOTED(ARCH_X86,,[Define this if you're running x86 architecture])
   AC_DEFINE_UNQUOTED(HAVE_MMX,,[Define this if you can compile MMX asm instructions])
fi
AM_CONDITIONAL(ARCH_X86, test "x$arch_x86" != "xno")
AM_CONDITIONAL(ARCH_X86_32, test "x$arch_x86" = "x32")
AM_CONDITIONAL(ARCH_X86_64, test "x$arch_x86" = "x64")
AM_CONDITIONAL(HAVE_MMX, test "x$arch_x86" != "xno")

case $host_os in
  darwin*)
      HOST_OS_DARWIN=1
      AC_DEFINE_UNQUOTED(HOST_OS_DARWIN, 1, [Define this if built on Mac OS X/Darwin])
      OBJCFLAGS="-D_INTL_REDIRECT_MACROS $OBJCFLAGS"
        ;;
esac
AM_CONDITIONAL(HOST_OS_DARWIN, test "x$HOST_OS_DARWIN" = "x1")

dnl ---------------------------------------------
dnl Set IMPURE_TEXT_LDFLAGS
dnl ---------------------------------------------

IMPURE_TEXT_LDFLAGS=""
if test "x$enable_impure_text" = xyes; then
  case "$host_or_hostalias" in
    *solaris*)
      if test "$GCC" = yes; then
        IMPURE_TEXT_LDFLAGS="-mimpure-text"
      else
        IMPURE_TEXT_LDFLAGS="-z textoff"
      fi
    ;;
    *-darwin*)
      IMPURE_TEXT_LDFLAGS="-Wl,-read_only_relocs,warning"
      ;;
  esac
fi
AC_SUBST(IMPURE_TEXT_LDFLAGS)

dnl ---------------------------------------------
dnl HAVE_ARMV4L is currently used in libavcodec makefile.am
dnl ---------------------------------------------

AM_CONDITIONAL(HAVE_ARMV4L, test "x$enable_armv4l" = "xyes")


dnl ---------------------------------------------
dnl Use features of autoconf 2.61, but stay compatible
dnl with older versions.
dnl ---------------------------------------------

if test "x$datarootdir" = "x"; then
   datarootdir='${datadir}'
   AC_SUBST(datarootdir)
fi

if test "x$docdir" = "x"; then
   docdir='${datarootdir}/doc/${PACKAGE}'
   AC_SUBST(docdir)
fi

if test "x$htmldir" = "x"; then
   htmldir='${docdir}'
   AC_SUBST(htmldir)
fi

dnl ---------------------------------------------
dnl XINE_ROOTDIR does not work if architecture independent files are 
dnl installed to another place than architecture dependent files !!!
dnl ---------------------------------------------

if test "x$prefix" = xNONE; then
   prefix="${ac_default_prefix}"
fi
if test "x$exec_prefix" = xNONE; then
   exec_prefix='${prefix}'
fi

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

xinelibdir='${libdir}/xine'
xinedatadir='${datadir}/xine'
pkgconfigdir='${libdir}/pkgconfig'
AC_SUBST(xinelibdir)
AC_SUBST(xinedatadir)
AC_SUBST(pkgconfigdir)

XINE_PLUGINROOT="\${xinelibdir}/plugins/$(($XINE_LT_CURRENT-$XINE_LT_AGE))"
XINE_PLUGINDIR="$XINE_PLUGINROOT.$XINE_LT_AGE"
XINE_FONTDIR="\${xinedatadir}/libxine$XINE_MAJOR/fonts"
XINE_LOCALEDIR='${datadir}/locale'
XINE_REL_PLUGINROOT="`makeexpand "$XINE_PLUGINROOT"`"
XINE_REL_PLUGINROOT="`makeexpand "$XINE_REL_PLUGINROOT" | sed -e "s,^${prefix}/,,"`"
XINE_REL_PLUGINDIR="$XINE_REL_PLUGINROOT.$XINE_LT_AGE"
XINE_REL_FONTDIR="`makeexpand "$XINE_FONTDIR" | sed -e "s,^${prefix}/,,"`"
XINE_REL_LOCALEDIR="`makeexpand "$XINE_LOCALEDIR" | sed -e "s,^${prefix}/,,"`"
XINE_PKGCONFIG_DIR="`makeexpand "$pkgconfigdir"`"
if test "x$SYS" = "xmingw32" -o "x$SYS" = "xcygwin"; then
  dnl prefix in xine-config
  XINE_CONFIG_PREFIX="\$(cd \$(dirname \$0)/..; pwd)"
  dnl installation directories (in xine-config)
  XINE_PLUGINROOTPATH="$XINE_CONFIG_PREFIX/$XINE_REL_PLUGINROOT"
  XINE_PLUGINPATH="$XINE_PLUGINROOTPATH.$XINE_LT_AGE"
  XINE_FONTPATH="$XINE_CONFIG_PREFIX/$XINE_REL_FONTDIR"
  XINE_LOCALEPATH="$XINE_CONFIG_PREFIX/$XINE_REL_LOCALEDIR"
  dnl runtime directories
  AC_DEFINE(XINE_PLUGINROOT,[xine_get_pluginroot()],[Define this to general plugins directory location])
  AC_DEFINE(XINE_PLUGINDIR,[xine_get_plugindir()],[Define this to specific plugins directory location])
  AC_DEFINE(XINE_FONTDIR,[xine_get_fontdir()],[Define this to osd fonts dir location])
  AC_DEFINE(XINE_LOCALEDIR,[xine_get_localedir()],[Path where catalog files will be.])
else
  dnl prefix in xine-config
  XINE_CONFIG_PREFIX="`makeexpand "${prefix}"`"
  dnl directories from xine-config and runtime directories
  XINE_PLUGINROOTPATH="`makeexpand "$XINE_PLUGINROOT"`"
  XINE_PLUGINPATH="$XINE_PLUGINROOTPATH.$XINE_LT_AGE"
  XINE_FONTPATH="`makeexpand "$XINE_FONTDIR"`"
  XINE_LOCALEPATH="`makeexpand "$XINE_LOCALEDIR"`"
  dnl defining runtime directories
  AC_DEFINE_UNQUOTED(XINE_PLUGINROOT,"$XINE_PLUGINROOTPATH",[Define this to general plugins directory location])
  AC_DEFINE_UNQUOTED(XINE_PLUGINDIR,"$XINE_PLUGINPATH",[Define this to specific plugins directory location])
  AC_DEFINE_UNQUOTED(XINE_FONTDIR,"$XINE_FONTPATH",[Define this to osd fonts dir location])
  AC_DEFINE_UNQUOTED(XINE_LOCALEDIR, "$XINE_LOCALEPATH",[Path where catalog files will be.])
fi
AC_DEFINE_UNQUOTED(XINE_REL_PLUGINDIR,"$XINE_REL_PLUGINDIR",[Define this to specific plugin directory relative to execution prefix])
AC_DEFINE_UNQUOTED(XINE_REL_PLUGINROOT,"$XINE_REL_PLUGINROOT",[Define this to general plugin directory relative to execution prefix])
AC_DEFINE_UNQUOTED(XINE_REL_FONTDIR,"$XINE_REL_FONTDIR",[Define this to font directory relative to prefix])
AC_DEFINE_UNQUOTED(XINE_REL_LOCALEDIR,"$XINE_REL_LOCALEDIR",[Define this to font directory relative to prefix])
AC_SUBST(XINE_CONFIG_PREFIX)
AC_SUBST(XINE_PLUGINPATH)
AC_SUBST(XINE_FONTPATH)
AC_SUBST(XINE_LOCALEPATH)
AC_SUBST(XINE_PLUGINDIR)
AC_SUBST(XINE_FONTDIR)
AC_SUBST(XINE_LOCALEDIR)
AC_SUBST(XINE_PKGCONFIG_DIR)

dnl Where aclocal m4 files should be installed
XINE_ACFLAGS="-I ${datarootdir}/aclocal"
AC_DEFINE_UNQUOTED(XINE_ACFLAGS, "$XINE_ACFLAGS", [Path where aclocal m4 files will be.])
AC_SUBST(XINE_ACFLAGS)

dnl Where architecture independent data (e.g. logo) will/should be installed
XINE_DATADIR="\${xinedatadir}"
AC_SUBST(XINE_DATADIR)

dnl Where scripts will/should be installed.
XINE_SCRIPTPATH="\${xinedatadir}/scripts"
AC_SUBST(XINE_SCRIPTPATH)


dnl ---------------------------------------------
dnl Some informations about xine-lib compilation
dnl ---------------------------------------------

XINE_BUILD_CC="`$CC -v 2>&1 | tail -1 2>/dev/null`"
XINE_BUILD_OS="`uname -s -r -m`"
XINE_BUILD_DATE="`date \"+%a %d %b %Y %T\"`"
AC_SUBST(XINE_BUILD_CC)
AC_SUBST(XINE_BUILD_OS)
AC_SUBST(XINE_BUILD_DATE)


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

case $host in
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
dnl some include paths ( !!! DO NOT REMOVE !!! )
dnl ---------------------------------------------

INCLUDES='-I$(top_srcdir) -I$(top_builddir)/include -I$(top_srcdir)/include -I$(top_srcdir)/src -I$(top_builddir)/src/xine-engine -I$(top_srcdir)/src/xine-engine -I$(top_srcdir)/src/xine-utils $(INTLDIR) -I$(top_builddir)/src/input -I$(top_srcdir)/src/input $(WIN32_CPPFLAGS) -I$(top_builddir)/lib -I$(top_srcdir)/lib'
AC_SUBST(INCLUDES)


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


dnl ---------------------------------------------
dnl It seems automake 1.5 don't take care about this script
dnl ---------------------------------------------

if test ! -z "$am_depcomp"; then
	DEPCOMP="depcomp"
fi
AC_SUBST(DEPCOMP)


dnl ---------------------------------------------
dnl Check for documentation formatting tool
dnl ---------------------------------------------

AC_CHECK_PROG(SGMLTOOLS, sgmltools, sgmltools, no)
AM_CONDITIONAL([HAVE_SGMLTOOLS], [test "$SGMLTOOLS" != "no"])

AC_CHECK_PROG(FIG2DEV, fig2dev, fig2dev, no)
AM_CONDITIONAL([HAVE_FIG2DEV], [test "$FIG2DEV" != "no"])

dnl ---------------------------------------------
dnl Newest automake workaround
dnl ---------------------------------------------

AC_SUBST(mkdir_p)
if test -n "$ac_aux_dir"; then
  case "$ac_aux_dir" in
    /*) MKINSTALLDIRS="$ac_aux_dir/install-sh -d" ;;
    *) MKINSTALLDIRS="\$(top_builddir)/$ac_aux_dir/install-sh -d" ;;
  esac
else
  MKINSTALLDIRS="\$(top_srcdir)/install-sh -d"
fi
AC_SUBST(MKINSTALLDIRS)

dnl this is an internal function we should not use, but
dnl as long as neither autoconf nor automake offer an A[CM]_PROG_OBJC
dnl check we will have to call it
_AM_DEPENDENCIES([OBJC])

AM_CONDITIONAL([BUILD_DMX_IMAGE], [test "x$have_imagemagick" = "xyes" -o "x$no_gdkpixbuf" != "xyes"])

dnl Important warnings we _don't_ want to skip
dnl Don't put these under conditional for optimisations, because these
dnl need always to be enabled.
CC_CHECK_CFLAGS_APPEND([-Wformat=2 -Wformat],
  [CC_CHECK_CFLAGS_APPEND([-Wno-format-zero-length])
   break;
  ])

CC_CHECK_CFLAGS_APPEND([-Wmissing-format-attribute])

dnl check for strict aliasing problem, get the highest between =2 and
dnl normal.
CC_CHECK_CFLAGS_APPEND([-Wstrict-aliasing=2 -Wstrict-aliasing], [break;])

dnl This *has* to stay at the end as it can break some autoconf tests.
CC_CHECK_CFLAGS_APPEND([-Werror=implicit-function-declaration -Werror-implicit-function-declaration], [break;])

case $host in
     dnl FreeBSD (et al.) does not complete linking for shared objects when pthreads
     dnl are requested, as different implementations are present; to avoid problems
     dnl use -Wl,-z,defs only for those platform not behaving this way.
     *-freebsd*) ;;
     *)
	AC_TRY_LDFLAGS([-Wl,-z,defs], [NOUNDEF="-Wl,-z,defs"])
	;;
esac
AC_SUBST([NOUNDEF])


dnl signal FreeBSD have also FreeBSD based kernel.
dnl 

AH_BOTTOM([
#if defined (__FreeBSD__) && !defined (__FreeBSD_kernel__)
#define __FreeBSD_kernel__ __FreeBSD__
#endif

#if defined (__FreeBSD_kernel__)
#include <sys/param.h>
# if defined (__FreeBSD_version) && !defined (__FreeBSD_kernel_version)
# define __FreeBSD_kernel_version __FreeBSD_version
# endif
#endif
])


dnl Common cflags for all platforms
CFLAGS="-D_REENTRANT -D_FILE_OFFSET_BITS=64 -DXINE_COMPILE \$(MULTIPASS_CFLAGS) $CFLAGS"
DEBUG_CFLAGS="-D_REENTRANT -D_FILE_OFFSET_BITS=64 -DXINE_COMPILE $DEBUG_CFLAGS"

if test "x$enable_debug" = "xyes"; then
   CFLAGS="$CFLAGS -DDEBUG"
else
   CFLAGS="$CFLAGS -DNDEBUG"
fi

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
include/Makefile
include/xine.h
lib/Makefile
m4/Makefile
misc/Makefile
misc/SlackBuild
misc/build_rpms.sh
misc/fonts/Makefile
misc/libxine.pc
misc/relchk.sh
misc/xine-config
misc/xine-lib.spec
po/Makevars.extra
po/Makefile.in
src/Makefile
src/audio_out/Makefile
src/combined/Makefile
src/combined/ffmpeg/Makefile
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
src/libdts/Makefile
src/libfaad/Makefile
src/libfaad/codebook/Makefile
src/libffmpeg/Makefile
src/libffmpeg/libavcodec/Makefile
src/libffmpeg/libavcodec/armv4l/Makefile
src/libffmpeg/libavcodec/i386/Makefile
src/libffmpeg/libavcodec/mlib/Makefile
src/libffmpeg/libavcodec/alpha/Makefile
src/libffmpeg/libavcodec/ppc/Makefile
src/libffmpeg/libavcodec/sparc/Makefile
src/libffmpeg/libavcodec/libpostproc/Makefile
src/libffmpeg/libavutil/Makefile
src/libmad/Makefile
src/libmpeg2/Makefile
src/libmusepack/Makefile
src/libmusepack/musepack/Makefile
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

dnl Guard against multiple inclusion
AH_TOP([#ifndef _XINE_CONFIGURE_H_
#define _XINE_CONFIGURE_H_
])
AH_BOTTOM([#endif
])

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

dnl ---------------------------------------------
dnl Some infos:
dnl ---------------------------------------------

echo "xine-lib summary:"
echo "----------------"

dnl Input
echo " * input plugins:"
echo "   - file          - net"
echo "   - stdin_fifo    - rtp"
echo "   - http          - mms"
echo "   - pnm           - rtsp"
echo "   - dvb"
if test "x$external_dvdnav" = "xyes"; then
  echo "   - dvd (external libs)"
else
  echo "   - dvd (internal libs)"
fi
if test "x$have_cdrom_ioctls" = "xyes"; then
  if test "x$enable_vcd" = "xyes"; then
    if test "x$internal_vcdnav" = "xno"; then
      echo "   - vcd (external libs)"
    else
      echo "   - vcd (internal libs)"
    fi
  fi
  echo "   - vcdo"
  echo "   - cdda"
fi
if test "x$no_gnome_vfs" = "xno"; then
  echo "   - gnome-vfs"
fi
if test "x$have_v4l" = "xyes"; then
  echo "   - v4l"
fi
if test "x$have_libsmbclient" = "xyes"; then
  echo "   - smbclient"
fi
echo ""

dnl Demuxers
echo " * demultiplexer plugins:"
echo "   - avi           - mpeg"
echo "   - mpeg_block    - mpeg_audio"
echo "   - mpeg_elem     - mpeg_pes"
echo "   - mpeg_ts       - qt/mpeg-4"
echo "   - film          - roq"
echo "   - fli           - smjpeg"
echo "   - idcin         - wav"
echo "   - wc3 mve       - voc"
echo "   - vqa           - aiff"
echo "   - cdda          - snd/au"
echo "   - yuv4mpeg2     - real/realaudio"
echo "   - ea wve        - raw dv"
echo "   - interplay mve - psx str"
echo "   - ws aud        - pva"
echo "   - vox"
echo "   - nsv           - 4xm"
echo "   - FLAC          - aac"
echo "   - iff           - matroska"
echo "   - vmd           - flv"
if test "x$enable_asf" = "xyes"; then
  echo "   - asf"
fi
if test "x$enable_nosefart" != "xno"; then
  echo "   - nsf"
fi
if test "x$have_vorbis" = "xyes"; then
  echo "   - ogg"
fi
if test "x$have_libmng" = "xyes"; then
  echo "   - mng"
fi
if test "x$enable_modplug" != x"no"; then
  echo "   - mod"
fi
if test "x$have_libflac" = "xyes"; then
  echo "   - FLAC (with libFLAC)"
fi
if test "x$have_wavpack" = "xyes"; then
  echo "   - WavPack"
fi
if test "x$enable_a52dec" = "xyes"; then
  if test "x$have_a52" = "xyes"; then
    echo "   - ac3 (external library)"
  else
    echo "   - ac3 (internal library)"
  fi
fi
echo ""

dnl video decoders
echo " * video decoder plugins:"
echo "   - MPEG 1,2         - Amiga Bitplane"
echo "   - Raw RGB          - Raw YUV"
if test "x$with_external_ffmpeg" = "xyes"; then
  echo "   - ffmpeg (external library):"
else
  echo "   - ffmpeg (internal library):"
fi
echo "     - MPEG-4 (ISO, Microsoft, DivX*, XviD)"
echo "     - Creative YUV    - Motion JPEG"
echo "     - Cinepak         - MS Video-1"
echo "     - FLI/FLC         - MS RLE"
echo "     - Id RoQ          - Id Cin"
echo "     - Apple Graphics  - Apple Video"
echo "     - Apple Animation - Interplay Video"
echo "     - Westwood VQA    - Origin Xan"
echo "     - H.263           - Intel Indeo 3"
echo "     - SVQ1            - SVQ3"
echo "     - Real Video 1.0  - Real Video 2.0"
echo "     - 4X Video        - Sierra Video"
echo "     - Asus v1/v2      - HuffYUV"
echo "     - On2 VP3         - DV"
echo "     - 8BPS            - Duck TrueMotion v1"
echo "     - ATI VCR1        - Flash Video"
echo "     - ZLIB            - MSZH"
if test "x$have_dxr3" = "xyes"; then
  echo "   - dxr3_video"
fi
if test "x$enable_w32dll" = "xyes"; then
  echo "   - w32dll"
fi
if test "x$have_imagemagick" = "xyes"; then
  echo "   - image"
fi
if test x"no_gdkpixbuf" != "xyes"; then
  echo "   - gdk-pixbuf"
fi
if test "x$have_theora" = "xyes"; then
  echo "   - theora"
fi
echo ""

dnl audio decoders
echo " * audio decoder plugins:"
echo "   - GSM 06.10       - linear PCM"
if test "x$with_external_ffmpeg" = "xyes"; then
  echo "   - ffmpeg (external library):"
else
  echo "   - ffmpeg (internal library):"
fi
echo "     - Windows Media Audio v1/v2"
echo "     - DV            - logarithmic PCM"
echo "     - 14k4          - 28k8"
echo "     - MS ADPCM      - IMA ADPCM"
echo "     - XA ADPCM      - Game DPCM/ADPCM"
echo "     - Mace 3:13     - Mace 6:1"
echo "     - FLAC"
if test "x$enable_nosefart" != "xno"; then
  echo "   - Nosefart (NSF)"
fi
if test "x$have_libflac" = "xyes"; then
  echo "   - FLAC (with libFLAC)"
fi
if test "x$have_vorbis" = "xyes"; then
  echo "   - vorbis"
fi
if test "x$have_speex" = "xyes"; then
  echo "   - speex"
fi
if test "x$enable_w32dll" = "xyes"; then
  echo "   - w32dll"
fi
if test "x$enable_faad" = "xyes"; then
  echo "   - faad"
fi
if test "x$enable_libmad" = "xyes"; then
  if test "x$have_mad" = "xyes"; then
    echo "   - MAD (MPG 1/2/3) (external library)"
  else
    echo "   - MAD (MPG 1/2/3) (internal library)"
  fi
fi
if test "x$enable_libdts" = "xyes"; then
  if test "x$have_dts" = "xyes"; then
    echo "   - DTS (external library)"
  else
    echo "   - DTS (internal library)"
  fi
fi
if test "x$enable_a52dec" = "xyes"; then
  if test "x$have_a52" = "xyes"; then
    echo "   - A52/ra-dnet (external library)"
  else
    echo "   - A52/ra-dnet (internal library)"
  fi
fi
if test "x$enable_musepack" != "xno"; then
  if test "x$have_mpcdec" = "xyes"; then
    echo "   - MusePack (external library)"
  else
    echo "   - MusePack (internal library)"
  fi
fi
if test "x$have_wavpack" = "xyes"; then
  echo "   - WavPack"
fi
echo ""

dnl spu decoders
echo " * subtitle decoder plugins:"
echo "   - spu             - spucc"
echo "   - spucmml         - sputext"
echo "   - spudvb"
if test "x$have_dxr3" = "xyes"; then
  echo "   - dxr3_spu"
fi
echo ""

dnl post plugins
echo " * post effect plugins:"
echo "  * planar video effects:"
echo "   - invert          - expand"
echo "   - eq              - eq2"
echo "   - boxblur         - denoise3d"
echo "   - unsharp         - tvtime"
echo "  * SFX:"
echo "   - goom            - oscope"
echo "   - fftscope        - mosaico"
echo ""

dnl Video plugins
echo " * video driver plugins:"
if test "x$no_x" != "xyes"; then
  echo "   - XShm (X11 shared memory)"
  dnl synfb
  if test "x$enable_syncfb" != "xno"; then
    echo "   - SyncFB (for Matrox G200/G400 cards)"
  fi
  dnl Xv
  if test "x$ac_have_xv" = "xyes"; then
    if test "x$ac_have_xv_static" = "xyes"; then
      echo "   - Xv (XVideo *static*)"
    else
      echo "   - Xv (XVideo *shared*)"
    fi
  fi
  dnl XxMC
  if test "x$ac_have_xxmc" = "xyes"; then
    if test "x$ac_have_vldxvmc_h" = "xyes"; then
      echo "   - XxMC (XVideo extended motion compensation)"
    else
      echo "   - XxMC (XVideo motion compensation - vld extensions DISABLED)"
    fi
  fi
  dnl XvMC
  if test "x$ac_have_xvmc" = "xyes"; then
    echo "   - XvMC (XVideo motion compensation)"
  fi
  if test "x$ac_have_opengl" = "xyes" -a "x$ac_have_glut" = "xyes" -o \
	  "x$ac_have_opengl" = "xyes" -a "x$ac_have_glu" = "xyes"; then	
    echo "   - OpenGL"
  fi
  if test "x$ac_have_sunfb" = "xyes"; then
    if test "x$ac_have_sundga" = "xyes"; then
      echo "   - PGX64 (for Sun XVR100/PGX64/PGX24 cards)"
      echo "   - PGX32 (for Sun PGX32 cards)"
    fi
  fi
fi
if test "x$have_xcb" = "xyes"; then
  dnl xcb-shm
  if test "x$have_xcbshm" = "xyes"; then
    echo "   - xcb-shm (X shared memory using XCB)"
  fi
  dnl xcb-xv
  if test "x$have_xcbxv" = "xyes"; then
    echo "   - xcb-xv (XVideo using XCB)"
  fi
fi
if test "x$no_aalib" != "xyes"; then
  echo "   - aa (Ascii ART)"
fi
if test "x$have_caca" = "xyes"; then
  echo "   - caca (Color AsCii Art)"
fi
if test "x$have_fb" = "xyes"; then
  echo "   - fb (Linux framebuffer device)"
fi
if test "x$have_sdl" = "xyes"; then
  echo "   - sdl (Simple DirectMedia Layer)"
fi
if test "x$have_libstk" = "xyes"; then
  echo "   - stk (Libstk Set-top Toolkit)"
fi
if test "x$have_directfb" = "xyes"; then
  echo "   - directfb (DirectFB driver)"
fi
if test "x$have_dxr3" = "xyes"; then
  if test "x$have_encoder" = "xyes"; then
    echo "   - dxr3 (Hollywood+ and Creative dxr3, both mpeg and non-mpeg video)"
  else
    echo "   - dxr3 (Hollywood+ and Creative dxr3, mpeg video only)"
  fi
fi
if test "x$enable_vidix" = "xyes"; then
  echo $ECHO_N "   - vidix ("
  
  if test "x$no_x" != "xyes"; then
    echo $ECHO_N "X11"
    if test "x$have_fb" = "xyes"; then
      echo $ECHO_N " and "
    fi
  fi
    
  if test "x$have_fb" = "xyes"; then
      echo $ECHO_N "framebuffer"
  fi
  
  echo $ECHO_N " support"

  if test "x$enable_dha_kmod" = "xyes"; then
    echo " with dhahelper)"
  else
    echo ")"
  fi
fi
if test "x$have_directx" = "xyes"; then
  echo "   - directx (DirectX video driver)"
fi
if test "x$have_macosx_video" = "xyes"; then
  echo "   - Mac OS X OpenGL"
fi

echo ""

dnl Audio plugins
echo " * audio driver plugins:"
if test "x$have_ossaudio" = "xyes"; then
  echo "   - oss (Open Sound System)"
fi
if test "x$have_alsa" = "xyes"; then
  echo "   - alsa"
fi
if test "x$have_esound" = "xyes"; then
  echo "   - esd (Enlightened Sound Daemon)"
fi
if test "x$no_arts" != "xyes"; then
  echo "   - arts (aRts - KDE soundserver)"
fi
if test "x$no_fusionsound" != "xyes"; then
  echo "   - fusionsound (FusionSound driver)"
fi
if test "x$have_sunaudio" = "xyes"; then
  echo "   - sun ()"
fi
if test "x$am_cv_have_irixal" = xyes; then
  echo "   - irixal (Irix audio library)"
fi
if test "x$have_directx" = "xyes"; then
  echo "   - directx (DirectX audio driver)"
fi
if test "x$have_coreaudio" = "xyes"; then
  echo "   - CoreAudio (Mac OS X audio driver)"
fi  
if test "x$have_pulseaudio" = "xyes"; then
  echo "   - pulseaudio sound server"
fi
if test "x$have_jack" = "xyes"; then
  echo "   - Jack"
fi
echo "---"


dnl ---------------------------------------------
dnl some user warnings
dnl ---------------------------------------------

dnl some levels of variable expansion to get final install paths
final_libdir="`eval eval eval eval echo $libdir`"
final_bindir="`eval eval eval eval echo $bindir`"

if test -r /etc/ld.so.conf && ! grep -x "$final_libdir" /etc/ld.so.conf >/dev/null ; then
  if test "$final_libdir" != "/lib" -a "$final_libdir" != "/usr/lib" ; then
    if ! echo "$LD_LIBRARY_PATH" | egrep "(:|^)$final_libdir(/?:|/?$)" >/dev/null ; then
      echo
      echo "****************************************************************"
      echo "xine-lib will be installed to $final_libdir"
      echo
      echo "This path is not mentioned among the linker search paths in your"
      echo "/etc/ld.so.conf. This means it is possible that xine-lib will"
      echo "not be found when you try to compile or run a program using it."
      echo "If this happens, you should add $final_libdir to"
      echo "the environment variable LD_LIBRARY_PATH like that:"
      echo
      echo "export LD_LIBRARY_PATH=$final_libdir:\$LD_LIBRARY_PATH"
      echo
      echo "Alternatively you can add a line \"$final_libdir\""
      echo "to your /etc/ld.so.conf."
      echo "****************************************************************"
      echo
    fi
  fi
fi

if ! echo "$PATH" | egrep "(:|^)$final_bindir(/?:|/?$)" >/dev/null ; then
  echo
  echo "****************************************************************"
  echo "xine-config will be installed to $final_bindir"
  echo
  echo "This path is not in your search path. This means it is possible"
  echo "that xine-config will not be found when you try to compile a"
  echo "program using xine-lib. This will result in build failures."
  echo "If this happens, you should add $final_bindir to"
  echo "the environment variable PATH like that:"
  echo
  echo "export PATH=$final_bindir:\$PATH"
  echo
  echo "Note that this is only needed for compilation. It is not needed"
  echo "to have xine-config in your search path at runtime. (Although"
  echo "it will not cause any harm either.)"
  echo "****************************************************************"
  echo
fi

dnl warn if no X11 plugins will be built
if test "x$no_x" = "xyes"; then
  case $host in
    *mingw*|*-cygwin) ;;
    *-darwin*) ;;
    *)
      echo
      echo "****************************************************************"
      echo "WARNING! No X11 output plugins will be built."
      echo
      echo "For some reason, the requirements for building the X11 video"
      echo "output plugins are not met. That means, that you will NOT be"
      echo "able to use the resulting xine-lib to watch videos in a window"
      echo "on any X11-based display (e.g. your desktop)."
      echo
      echo "If this is not what you want, provide the necessary X11 build"
      echo "dependencies (usually done by installing a package called"
      echo "XFree86-devel or similar) and run configure again."
      echo "****************************************************************"
      echo
      ;;
  esac
fi
