dnl
dnl Check for OpenGL & GLU
dnl
dnl AM_PATH_OPENGL([ACTION IF FOUND [, ACTION IF NOT FOUND]])
dnl

AC_DEFUN([AM_PATH_OPENGL], [

  AC_ARG_ENABLE(opengl, AC_HELP_STRING([--disable-opengl], [do not build OpenGL plugin]),
    [enableopengl=$enableval],
    [enableopengl="yes"]
  )
  AC_ARG_ENABLE(glu, AC_HELP_STRING([--disable-glu], [build OpenGL plugin without GLU (no verbose errors)]),
    [enableglu=$enableval],
    [enableglu="yes"]
  )

  if test x$enableopengl = "xyes"; then
    AC_CHECK_LIB(GL, glBegin,
      [AC_CHECK_LIB(m, tan,
        [AC_CHECK_HEADER(GL/gl.h,
          [ac_have_opengl="yes"
            OPENGL_LIBS="-lGL -lm"
            AC_DEFINE(HAVE_OPENGL,1,[Define this if you have OpenGL support available])
            dnl
            dnl need to check with some test if this linking with -lGLU actually works...
            dnl check for GLU
            dnl
            if test x$enableglu = "xyes"; then
              AC_CHECK_LIB(GLU, gluPerspective,
                [AC_CHECK_HEADER(GL/glu.h,
                  [AC_MSG_CHECKING([if GLU is sane])
                    ac_save_LIBS="$LIBS"
                    LIBS="$X_LIBS $XPRE_LIBS $OPENGL_LIBS -lGLU $X_EXTRA_LIBS"
                    AC_TRY_LINK([#include <GL/gl.h>
#include <GL/glu.h>],
                      [ gluPerspective(45.0f,1.33f,1.0f,1000.0f); glBegin(GL_POINTS); glEnd(); return 0 ],
                      [ ac_have_glu="yes"
                        GLU_LIBS="-lGLU" 
                        AC_DEFINE(HAVE_GLU,1,[Define this if you have GLU support available])
                        AC_MSG_RESULT(yes)],
                      [ AC_MSG_RESULT(no)
                        echo "*** GLU doesn't link with GL; GLU is disabled ***"])
                    LIBS="$ac_save_LIBS"]
                )],
                [], 
                [$X_LIBS $X_PRE_LIBS $OPENGL_LIBS -lGLU $X_EXTRA_LIBS]
              )
            fi
          ]
        )],
        [],
        [$X_LIBS $X_PRE_LIBS -lGL -lm $X_EXTRA_LIBS]
      )],
      [],
      [$X_LIBS $X_PRE_LIBS $X_EXTRA_LIBS]
    )
    if test x$ac_have_opengl = "xyes"; then
      ac_use_opengl=yes
    fi
  fi

  AC_SUBST(OPENGL_CFLAGS)
  AC_SUBST(OPENGL_LIBS)
  AC_SUBST(GLU_LIBS)
  AM_CONDITIONAL(HAVE_OPENGL, [test x$ac_use_opengl = "xyes"])

  dnl result
  if test x$ac_use_opengl = "xyes"; then
    ifelse([$1], , :, [$1])
  else
    ifelse([$2], , :, [$2])
  fi

])
