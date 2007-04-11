include $(top_srcdir)/misc/Makefile.common

if DTS
dts_module = xineplug_decode_dts.la
endif

xineplug_LTLIBRARIES = $(dts_module)

if EXTERNAL_LIBDTS
fnsa =
internal_sources =
else
fnsa = -fno-strict-aliasing
internal_sources = bitstream.c downmix.c parse.c 
endif

xineplug_decode_dts_la_SOURCES = xine_dts_decoder.c $(internal_sources)
xineplug_decode_dts_la_CFLAGS = $(LIBDTS_CFLAGS) $(VISIBILITY_FLAG) $(fnsa)
xineplug_decode_dts_la_LDFLAGS = $(xineplug_ldflags)

if EXTERNAL_LIBDTS
xineplug_decode_dts_la_LIBADD = $(XINE_LIB) $(LIBDTS_LIBS) -lm
else
xineplug_decode_dts_la_LIBADD = $(XINE_LIB) -lm
endif

noinst_HEADERS = bitstream.h dts.h dts_internal.h tables.h tables_adpcm.h \
	tables_fir.h tables_huffman.h tables_quantization.h tables_vq.h
