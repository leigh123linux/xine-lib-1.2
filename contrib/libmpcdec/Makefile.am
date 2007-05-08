if MUSEPACK
if !EXTERNAL_MPCDEC
noinst_LTLIBRARIES = libmpcdec.la
endif
endif

AM_CFLAGS = $(DEFAULT_OCFLAGS) $(VISIBILITY_FLAG)

libmpcdec_la_SOURCES = huffsv46.c huffsv7.c idtag.c mpc_decoder.c mpc_reader.c \
	requant.c streaminfo.c synth_filter.c \
	mpcdec/internal.h mpcdec/mpcdec.h mpcdec/streaminfo.h mpcdec/math.h \
	mpcdec/decoder.h mpcdec/config_types.h mpcdec/requant.h mpcdec/huffman.h \
	mpcdec/reader.h

EXTRA_DIST = diff_from_libmpcdec_1.2.5.patch COPYING
