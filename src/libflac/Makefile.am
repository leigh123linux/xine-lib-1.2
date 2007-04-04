include $(top_srcdir)/misc/Makefile.common

libdir = $(XINE_PLUGINDIR)

if HAVE_LIBFLAC
lib_LTLIBRARIES = xineplug_flac.la
endif

xineplug_flac_la_SOURCES = demux_flac.c decoder_flac.c
xineplug_flac_la_LIBADD = $(LIBFLAC_LIBS) $(XINE_LIB)
xineplug_flac_la_CFLAGS = $(LIBFLAC_CFLAGS) $(VISIBILITY_FLAG)
xineplug_flac_la_LDFLAGS = -avoid-version -module

noinst_HEADERS = demux_flac.h
