dnl -----------------
dnl Audio out plugins
dnl -----------------
AC_DEFUN([XINE_AUDIO_OUT_PLUGINS], [

dnl CoreAudio for Mac OS X
MACOSX_AUDIO_SUPPORT

dnl PulseAudio
AC_ARG_WITH([pulseaudio],
            [AS_HELP_STRING([--without-pulseaudio], [Do not build Pulseaudio support])])

if test "x$with_pulseaudio" != "xno"; then
   PKG_CHECK_MODULES([PULSEAUDIO], [libpulse], [have_pulseaudio="yes"], [have_pulseaudio="no"])
fi
AM_CONDITIONAL(HAVE_PULSEAUDIO, [test "x$have_pulseaudio" = x"yes"])

dnl OSS style audio interface
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

   AC_IOCTL_REQUEST
fi

AM_CONDITIONAL(HAVE_OSS, test "x$have_ossaudio" = "xyes")

dnl Alsa support

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
  AS_HELP_STRING([--without-arts], [Build without ARTS audio output]),
  [with_arts=$withval], [with_arts=yes])

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

])dnl XINE_AUDIO_OUT_PLUGINS
