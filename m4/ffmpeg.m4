dnl
dnl autoconf script for searching and checking ffmpeg
dnl
dnl written by Frantisek Dvorak <valtri@users.sourceforge.net>
dnl
dnl
dnl AM_PATH_FFMPEG([ACTION IF FOUND [, ACTION IF NOT FOUND]]))
dnl
dnl It looks for ffmpeg, defines FFMPEG_CPPFLAGS and FFMPEG_LIBS.
dnl
AC_DEFUN([AM_PATH_FFMPEG], [

AC_ARG_WITH(external-ffmpeg, AC_HELP_STRING([--with-external-ffmpeg], [use external ffmpeg library]),
  [external_ffmpeg="$withval"],
  [external_ffmpeg="no"]
)

if test x"$external_ffmpeg" != "xno"; then
  
  dnl get the prefix, if specified
  if test x"$external_ffmpeg" != "xyes"; then
    ffmpeg_prefix="$withval"
  fi
  
  dnl disable test if requested
  AC_ARG_ENABLE(ffmpegtest,
    AC_HELP_STRING([--disable-ffmpegtest],
      [Do not try compile and run a test ffmpeg program. It will need specify custom FFMPEG_CPPFLAGS and FFMPEG_LIBS environment variables.]
    ),
    enable_ffmpegtest="$enableval",
    enable_ffmpegtest=yes
  )
  
  if test x"$enable_ffmpegtest" = "xyes"; then
    ac_save_LDFLAGS="${LDFLAGS}"
    ac_save_CPPFLAGS="${CPPFLAGS}"
    external_ffmpeg_found=no
  
    dnl look for the ffmpeg or just check specified flags
    if test x"$FFMPEG_CPPFLAGS" = x -a x"$FFMPEG_LIBS" = x; then
      dnl look for ffmpeg
      if test x"$ffmpeg_prefix" = x; then
        prefixes="$ffmpeg_prefix /usr /usr/local /opt"
      else
        prefixes="$ffmpeg_prefix"
      fi
      for dir in $prefixes; do
        FFMPEG_CPPFLAGS="-I${dir}/include/ffmpeg -I${dir}/include/postproc"
        FFMPEG_LIBS="-L${dir}/lib"
        CPPFLAGS="${FFMPEG_CPPFLAGS} ${ac_save_CPPFLAGS}"
        LDFLAGS="${FFMPEG_LIBS} ${ac_save_LDFLAGS}"
  
        dnl drop the cache
        for i in "ac_cv_header_avcodec_h" "ac_cv_header_postprocess_h" \
                 "ac_cv_lib_avcodec_pp_get_context" \
                 "ac_cv_lib_postproc_pp_get_context" \
                 "ac_cv_lib_avcodec_register_avcodec"; do
          $as_unset $i || test "${$i+set}" != set || { $i=; export $i; }
        done
  
        dnl check the headers
        AC_CHECK_HEADERS(avcodec.h postprocess.h,
          [dnl look for libpostproc inside libavcodec
          AC_CHECK_LIB(avcodec, pp_get_context,
            [external_ffmpeg_found=yes
            FFMPEG_LIBS="${FFMPEG_LIBS} -lavcodec"
            break],
            [dnl look for shared libpostproc and avcodec
              AC_CHECK_LIB(postproc, pp_get_context,
                [AC_CHECK_LIB(avcodec, register_avcodec,
                  [external_ffmpeg_found=yes
                  FFMPEG_LIBS="${FFMPEG_LIBS} -lavcodec -lpostproc"
                  break],
                  [continue],
                  [-lavcodec]
                )],
                [continue]
              )],
            []
          )],
          [continue]
        )
      done
  
      dnl result of autodetection
      if test x"$external_ffmpeg_found" = "xyes"; then
        AC_MSG_RESULT([External ffmpeg library was found in ${dir}])
      else
        AC_MSG_ERROR([External ffmpeg library not found.
  *********************************************************************
  You can try to specify prefix of ffmpeg library by the option
  --with-external-ffmpeg=prefix, or to specify custom FFMPEG_CPPFLAGS and 
  FFMPEG_LIBS.
  
  If you would like to use the internal ffmpeg, please remove the configure
  option --with-external-ffmpeg.
  *********************************************************************])
      fi
    else
      dnl check specified flags
      CPPFLAGS="${FFMPEG_CPPFLAGS} ${ac_save_CPPFLAGS}"
      LDFLAGS="${FFMPEG_LIBS} ${ac_save_LDFLAGS}"
      AC_LINK_IFELSE([#include <avcodec.h>
  #include <postprocess.h>
  
  int main() {
    register_avcodec((void *)0);
    pp_get_context(0, 0, 0);
  }
  ],
        [external_ffmpeg_found=yes],
        [external_ffmpeg_found=no],
      )
  
      dnl result
      if test x"$external_ffmpeg_found" = "xyes"; then
        AC_MSG_RESULT([Using custom FFMPEG_CPPFLAGS and FFMPEG_LIBS for external ffmpeg])
      else
        AC_MSG_ERROR([External ffmpeg library not found with specified options.
  *********************************************************************
  You can try to specify prefix of ffmpeg library by the option
  --with-external-ffmpeg=prefix, or to specify different FFMPEG_CPPFLAGS and 
  FFMPEG_LIBS.
  
  If you would like to use the internal ffmpeg, please remove the configure
  option --with-external-ffmpeg.
  *********************************************************************])
      fi
    fi
    CPPFLAGS="${ac_save_CPPFLAGS}"
    LDFLAGS="${ac_save_LDFLAGS}"
  else
    if test x"${FFMPEG_CPPFLAGS}" = "x" -a x"${FFMPEG_LIBS}" = "x"; then
      external_ffmpeg_found=no
      AC_MSG_ERROR([You should specify FFMPEG_CPPFLAGS and FFMPEG_LIBS])
    else
      external_ffmpeg_found=yes
      AC_MSG_RESULT([Forced using custom FFMPEG_CPPFLAGS and FFMPEG_LIBS.])
    fi
  fi
  
  dnl result
  if test x"$external_ffmpeg_found" = "xyes"; then
    ifelse([$1], , :, [$1])
  else
    ifelse([$2], , :, [$2])
  fi
  
  AC_SUBST(FFMPEG_CPPFLAGS)
  AC_SUBST(FFMPEG_LIBS)
  
else
  AC_MSG_RESULT([using included ffmpeg])
fi

])
