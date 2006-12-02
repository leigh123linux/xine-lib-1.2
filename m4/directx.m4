dnl
dnl autoconf script for DirectX
dnl
dnl written by Frantisek Dvorak <valtri@users.sourceforge.net>
dnl
dnl
dnl AM_PATH_DIRECTX([ACTION IF FOUND [, ACTION IF NOT FOUND]]))
dnl
dnl It looks for DirectX, defines DIRECTX_CPPFLAGS, DIRECTX_AUDIO_LIBS and 
dnl DIRECTX_VIDEO_LIBS.
dnl
AC_DEFUN([AM_PATH_DIRECTX], [

AC_ARG_WITH(dxheaders, AC_HELP_STRING([--with-dxheaders], [specify location of DirectX headers]),
  [dxheaders_prefix="$withval"],
  [dxheaders_prefix="no"]
)

if test x"$dxheaders_prefix" != "xno"; then
  DIRECTX_CPPFLAGS="-I${dxheaders_prefix} ${DIRECTX_CPPFLAGS}"
fi

AC_MSG_CHECKING(for DirectX)
DIRECTX_VIDEO_LIBS="$DIRECTX_LIBS -lgdi32 -lddraw"
DIRECTX_AUDIO_LIBS="$DIRECTX_LIBS -ldsound"
AC_LANG_SAVE()
AC_LANG_C()
ac_save_CPPFLAGS="$CPPFLAGS"
ac_save_LIBS="$LIBS"
CPPFLAGS="$CPPFLAGS $DIRECTX_CPPFLAGS"
LIBS="$LIBS  $DIRECTX_VIDEO_LIBS $DIRECTX_AUDIO_LIBS"
AC_COMPILE_IFELSE(
  [
#include <stddef.h>

#include <windows.h>
#include <ddraw.h>
#include <dsound.h>

int main() {
  DirectDrawCreate(0, NULL, 0);
  DirectsoundCreate(0, NULL, 0);
  
  return 0;
}
  ],
  [have_directx=yes
   AC_DEFINE(HAVE_DIRECTX,1,[Define this if you have DirectX])],,)
CPPFLAGS=$ac_save_CPPFLAGS
LIBS=$ac_save_LIBS
AC_LANG_RESTORE()

if test x$have_directx = xyes ; then
  AC_MSG_RESULT(yes)
else
  AC_MSG_RESULT(no)
  AC_MSG_RESULT(*** All DirectX dependent parts will be disabled ***)
fi

AC_SUBST(DIRECTX_CPPFLAGS)
AC_SUBST(DIRECTX_AUDIO_LIBS)
AC_SUBST(DIRECTX_VIDEO_LIBS)
AM_CONDITIONAL(HAVE_DIRECTX, test x$have_directx = "xyes")

dnl result
if test x"$have_directx" = "xyes"; then
  ifelse([$1], , :, [$1])
else
  ifelse([$2], , :, [$2])
fi

])
