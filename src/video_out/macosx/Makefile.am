include $(top_srcdir)/misc/Makefile.common

AM_OBJCFLAGS = $(DEFAULT_OCFLAGS) $(VISIBILITY_FLAG)

EXTRA_DIST = \
             video_window.h \
             XineOpenGLView.m \
             XineOpenGLView.h \
             XineVideoWindow.m \
             XineVideoWindow.h

if HAVE_MACOSX_VIDEO

lib_LTLIBRARIES = libxineMacOSXVideo.la

libxineMacOSXVideo_la_SOURCES = XineOpenGLView.m XineVideoWindow.m
libxineMacOSXVideo_la_LDFLAGS = -framework Cocoa -framework OpenGL \
				-version-info $(XINE_LT_CURRENT):$(XINE_LT_REVISION):$(XINE_LT_AGE)

xineinclude_HEADERS = video_window.h XineOpenGLView.h XineVideoWindow.h

endif

