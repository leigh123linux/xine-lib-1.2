include $(top_srcdir)/misc/Makefile.common

AM_CFLAGS  = $(DEFAULT_OCFLAGS) $(VISIBILITY_FLAG)
AM_LDFLAGS = $(xineplug_ldflags)

noinst_HEADERS = a52.h a52_internal.h bitstream.h tables.h crc.c

if ENABLE_A52DEC
xineplug_LTLIBRARIES = xineplug_decode_a52.la
endif

if WITH_EXTERNAL_A52DEC
external_libs = -la52
internal_sources =
else
external_libs =
internal_sources = bitstream.c bit_allocate.c downmix.c imdct.c parse.c
endif

xineplug_decode_a52_la_SOURCES = xine_a52_decoder.c $(internal_sources)
xineplug_decode_a52_la_LIBADD = $(XINE_LIB) $(external_libs) -lm
