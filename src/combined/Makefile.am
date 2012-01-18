include $(top_builddir)/misc/Makefile.plugins
include $(top_srcdir)/misc/Makefile.common

SUBDIRS = ffmpeg

if HAVE_WAVPACK
xineplug_wavpack = xineplug_wavpack.la
endif

if HAVE_LIBFLAC
xineplug_flac = xineplug_flac.la
endif

xineplug_LTLIBRARIES = $(xineplug_wavpack) $(xineplug_flac)

xineplug_wavpack_la_SOURCES = demux_wavpack.c decoder_wavpack.c combined_wavpack.c combined_wavpack.h
xineplug_wavpack_la_CFLAGS = $(VISIBILITY_FLAG) $(WAVPACK_CFLAGS) -I$(srcdir)/../demuxers
xineplug_wavpack_la_LIBADD = $(XINE_LIB) $(WAVPACK_LIBS)
xineplug_wavpack_la_LDFLAGS = $(xineplug_ldflags)

xineplug_flac_la_SOURCES = demux_flac.c decoder_flac.c demux_flac.h
xineplug_flac_la_CFLAGS = $(VISIBILITY_FLAG) $(LIBFLAC_CFLAGS)
xineplug_flac_la_LIBADD = $(XINE_LIB) $(LIBFLAC_LIBS)
xineplug_flac_la_LDFLAGS = $(xineplug_ldflags)
