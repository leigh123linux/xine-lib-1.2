include $(top_builddir)/misc/Makefile.plugins
include $(top_srcdir)/misc/Makefile.common

AM_CFLAGS  = $(DEFAULT_OCFLAGS) $(VISIBILITY_FLAG)
AM_LDFLAGS = $(xineplug_ldflags) $(IMPURE_TEXT_LDFLAGS)

SUBDIRS = plugins

xinepost_LTLIBRARIES = xineplug_post_tvtime.la

xineplug_post_tvtime_la_SOURCES = \
	deinterlace.c \
	deinterlace.h \
	pulldown.c \
	pulldown.h \
	speedtools.h \
	speedy.c \
	speedy.h \
	tvtime.c \
	tvtime.h \
	xine_plugin.c

xineplug_post_tvtime_la_CFLAGS = $(AM_CFLAGS)
xineplug_post_tvtime_la_LIBADD = $(XINE_LIB) $(LTLIBINTL) $(PTHREAD_LIBS) \
	$(top_builddir)/src/post/deinterlace/plugins/libdeinterlaceplugins.la

