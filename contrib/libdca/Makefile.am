if DTS
if !EXTERNAL_LIBDTS
noinst_LTLIBRARIES = libdca.la
endif
endif

libdca_la_SOURCES = dca_internal.h tables.h tables_quantization.h \
		    tables_huffman.h tables_fir.h tables_adpcm.h tables_vq.h \
		    bitstream.h bitstream.c parse.c downmix.c \
		    include/dca.h include/dts.h

libdca_la_CFLAGS = -I$(srcdir)/include $(VISIBILITY_FLAG)
libdca_la_LIBADD = -lm

EXTRA_DIST = COPYING
