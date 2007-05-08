include $(top_srcdir)/misc/Makefile.common

AM_CFLAGS = $(DEFAULT_OCFLAGS) $(VISIBILITY_FLAG)
AM_LDFLAGS = $(xineplug_ldflags)

if HAVE_WAVPACK
xineplug_wavpack = xineplug_wavpack.la
endif

if HAVE_LIBFLAC
xineplug_flac = xineplug_flac.la
endif

xineplug_LTLIBRARIES = $(xineplug_wavpack) $(xineplug_flac)

xineplug_wavpack_la_SOURCES = demux_wavpack.c decoder_wavpack.c combined_wavpack.c combined_wavpack.h
xineplug_wavpack_la_CFLAGS = $(AM_CFLAGS) $(WAVPACK_CFLAGS)
xineplug_wavpack_la_CPPFLAGS = -I$(srcdir)/../demuxers
xineplug_wavpack_la_LIBADD = $(XINE_LIB) $(WAVPACK_LIBS)

xineplug_flac_la_SOURCES = demux_flac.c decoder_flac.c demux_flac.h
xineplug_flac_la_CFLAGS = $(AM_CFLAGS) $(LIBFLAC_CFLAGS)
xineplug_flac_la_LIBADD = $(XINE_LIB) $(LIBFLAC_LIBS)
