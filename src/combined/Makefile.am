include $(top_srcdir)/misc/Makefile.common

if HAVE_WAVPACK
xineplug_wavpack = xineplug_wavpack.la
endif

xineplug_LTLIBRARIES = $(xineplug_wavpack)

xineplug_wavpack_la_SOURCES = demux_wavpack.c decoder_wavpack.c combined_wavpack.c combined_wavpack.h
xineplug_wavpack_la_CFLAGS = $(VISIBILITY_FLAG) $(WAVPACK_CFLAGS) -I$(srcdir)/../demuxers
xineplug_wavpack_la_LIBADD = $(XINE_LIB) $(WAVPACK_LIBS)
xineplug_wavpack_la_LDFLAGS = $(xineplug_ldflags)
