include $(top_builddir)/misc/Makefile.plugins
include $(top_srcdir)/misc/Makefile.common

AM_CFLAGS  = $(DEFAULT_OCFLAGS) $(VISIBILITY_FLAG)
AM_LDFLAGS = $(xineplug_ldflags)

xinepost_LTLIBRARIES = xineplug_post_audio_filters.la

xineplug_post_audio_filters_la_SOURCES = \
	audio_filters.c \
	audio_filters.h \
	dsp.h \
	filter.c \
	filter.h \
	stretch.c \
	upmix.c \
	upmix_mono.c \
	volnorm.c \
	window.c \
	window.h

xineplug_post_audio_filters_la_CFLAGS = $(AM_CFLAGS)
xineplug_post_audio_filters_la_LIBADD = $(XINE_LIB) $(PTHREAD_LIBS) $(LTLIBINTL) -lm
