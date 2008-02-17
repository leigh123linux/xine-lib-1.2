dnl AM_CHECK_IRIXAL ([ACTION-IF-YES], [ACTION-IF-NO])
dnl Configure paths/version for IRIX AL
AC_DEFUN([AM_CHECK_IRIXAL],
	 [AC_CACHE_CHECK([for IRIX libaudio support],
			 [am_cv_have_irixal],
			 [AC_CHECK_HEADER([dmedia/audio.h],
			  am_cv_have_irixal=yes, am_cv_have_irixal=no)])
	  if test "x$am_cv_have_irixal" = xyes ; then
	    IRIXAL_LIBS="-laudio"
	    IRIXAL_STATIC_LIB="/usr/lib/libaudio.a"
	    ifelse([$1], , :, [$1])
	  else
	    ifelse([$2], , :, [$2])
	  fi
	  AC_SUBST(IRIXAL_CFLAGS)
	  AC_SUBST(IRIXAL_STATIC_LIB)
	  AC_SUBST(IRIXAL_LIBS)
])

