include $(top_srcdir)/misc/Makefile.common



libdir = $(XINE_PLUGINDIR)

AM_CFLAGS = -D_LARGEFILE64_SOURCE

lib_LTLIBRARIES = \
	xineplug_inp_vdr.la

xineplug_inp_vdr_la_SOURCES = input_vdr.c
xineplug_inp_vdr_la_LIBADD = $(XINE_LIB)
xineplug_inp_vdr_la_LDFLAGS = -avoid-version -module @IMPURE_TEXT_LDFLAGS@

include_HEADERS = input_vdr.h



postlibdir = $(XINE_PLUGINDIR)/post

postlib_LTLIBRARIES = \
	xineplug_post_vdr.la

xineplug_post_vdr_la_SOURCES = post_vdr.c post_vdr_video.c post_vdr_audio.c
xineplug_post_vdr_la_LIBADD = $(XINE_LIB)
xineplug_post_vdr_la_LDFLAGS = -avoid-version -module @IMPURE_TEXT_LDFLAGS@

noinst_HEADERS = post_vdr.h

