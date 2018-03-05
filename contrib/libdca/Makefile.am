AM_CFLAGS = $(DEFAULT_OCFLAGS) $(VISIBILITY_FLAG)

EXTRA_DIST = COPYING

if ENABLE_DTS
if !WITH_EXTERNAL_LIBDTS
noinst_LTLIBRARIES = libdca.la
endif
endif

libdca_la_SOURCES = \
	bitstream.c \
	bitstream.h \
	dca_internal.h \
	downmix.c \
	parse.c \
	tables.h \
	tables_adpcm.h \
	tables_fir.h \
	tables_huffman.h \
	tables_quantization.h \
	tables_vq.h \
	include/dca.h \
	include/dts.h
libdca_la_LIBADD = -lm
libdca_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(srcdir)/include
