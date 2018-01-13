include $(top_builddir)/misc/Makefile.plugins
include $(top_srcdir)/misc/Makefile.common

AM_CFLAGS  = $(DEFAULT_OCFLAGS) $(VISIBILITY_FLAG)
AM_LDFLAGS = $(xineplug_ldflags)

EXTRA_DIST = fooviz.c

xinepost_LTLIBRARIES = xineplug_post_visualizations.la

xineplug_post_visualizations_la_SOURCES = \
	fft.h \
	fft.c \
	fftgraph.c \
	fftscope.c \
	oscope.c \
	tdaudioanalyzer.c \
	visualizations.c \
	visualizations.h

xineplug_post_visualizations_la_LIBADD = $(XINE_LIB) $(PTHREAD_LIBS) -lm
