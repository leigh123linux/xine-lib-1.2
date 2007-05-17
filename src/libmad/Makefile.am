include $(top_srcdir)/misc/Makefile.common

AM_CFLAGS   = $(DEFAULT_OCFLAGS) $(VISIBILITY_FLAG)
AM_CPPFLAGS = -DOPT_SPEED
AM_LDFLAGS  = $(xineplug_ldflags)

EXTRA_DIST = imdct_l_arm.S

noinst_HEADERS = \
	D.dat \
	imdct_s.dat \
	qc_table.dat \
	rq_table.dat \
	sf_table.dat \
	bit.h \
	fixed.h \
	frame.h \
	global.h \
	huffman.h \
	layer12.h \
	layer3.h \
	stream.h \
	synth.h \
	timer.h \
	version.h

if ENABLE_MAD
xineplug_LTLIBRARIES = xineplug_decode_mad.la
endif

if WITH_EXTERNAL_MAD
internal_sources =
external_libs = $(LIBMAD_LIBS)
else
internal_sources = \
	bit.c \
	fixed.c \
	frame.c \
	huffman.c \
	layer12.c \
	layer3.c \
	stream.c \
	synth.c \
	timer.c \
	version.c
external_libs =
endif

xineplug_decode_mad_la_SOURCES = xine_mad_decoder.c $(internal_sources)
xineplug_decode_mad_la_LIBADD = $(XINE_LIB) $(external_libs)
xineplug_decode_mad_la_CFLAGS = $(AM_CFLAGS) $(LIBMAD_CFLAGS)
