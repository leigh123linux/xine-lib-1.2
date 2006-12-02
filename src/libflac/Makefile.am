include $(top_srcdir)/misc/Makefile.common

if HAVE_FLAC
flac_module = xineplug_flac.la
endif

libdir = $(XINE_PLUGINDIR)

lib_LTLIBRARIES = $(flac_module)

xineplug_flac_la_SOURCES = demux_flac.c decoder_flac.c
xineplug_flac_la_LIBADD = $(LIBFLAC_LIBS) $(XINE_LIB)
xineplug_flac_la_CFLAGS = $(LIBFLAC_CFLAGS) $(VISIBILITY_FLAG)
xineplug_flac_la_LDFLAGS = -avoid-version -module

noinst_HEADERS = demux_flac.h
