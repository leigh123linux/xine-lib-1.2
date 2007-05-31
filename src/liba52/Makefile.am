include $(top_srcdir)/misc/Makefile.common

if A52
a52_module = xineplug_decode_a52.la
endif

xineplug_LTLIBRARIES = $(a52_module)

if EXTERNAL_A52DEC
internal_sources =
else
internal_sources = \
	bitstream.c \
	bit_allocate.c \
	downmix.c \
	imdct.c \
	parse.c
endif

xineplug_decode_a52_la_SOURCES = \
	xine_a52_decoder.c \
	$(internal_sources)

if EXTERNAL_A52DEC
xineplug_decode_a52_la_LIBADD = $(XINE_LIB) -la52 -lm
else
xineplug_decode_a52_la_LIBADD = $(XINE_LIB) -lm
endif

xineplug_decode_a52_la_CFLAGS = $(VISIBILITY_FLAG)
xineplug_decode_a52_la_LDFLAGS = $(xineplug_ldflags)

noinst_HEADERS = \
	a52.h \
	a52_internal.h \
	bitstream.h \
	tables.h \
	crc.c
