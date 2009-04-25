include $(top_builddir)/misc/Makefile.plugins
include $(top_srcdir)/misc/Makefile.common

noinst_HEADERS = dsp.h filter.h window.h audio_filters.h

xinepost_LTLIBRARIES = xineplug_post_audio_filters.la

xineplug_post_audio_filters_la_SOURCES = \
        upmix.c upmix_mono.c filter.c window.c stretch.c volnorm.c audio_filters.c
xineplug_post_audio_filters_la_LIBADD = $(XINE_LIB) $(PTHREAD_LIBS) $(LTLIBINTL) -lm
xineplug_post_audio_filters_la_CFLAGS = $(VISIBILITY_FLAG)
xineplug_post_audio_filters_la_LDFLAGS = -avoid-version -module

