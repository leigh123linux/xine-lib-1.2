AM_CFLAGS = $(DEFAULT_OCFLAGS) $(VISIBILITY_FLAG)

if ENABLE_A52DEC
if !WITH_EXTERNAL_A52DEC
noinst_LTLIBRARIES = liba52.la
endif
endif

liba52_la_SOURCES = \
	a52.h \
	a52_internal.h \
	bitstream.c \
	bitstream.h \
	bit_allocate.c \
	crc.c \
	downmix.c \
	imdct.c \
	parse.c \
	tables.h
liba52_la_LIBADD = -lm
liba52_la_CPPFLAGS = $(AM_CPPFLAGS)
