include $(top_builddir)/misc/Makefile.plugins
include $(top_srcdir)/misc/Makefile.common

AM_CFLAGS = -DOPT_SPEED

EXTRA_DIST = imdct_l_arm.S

if MAD
mad_module = xineplug_decode_mad.la
endif

xineplug_LTLIBRARIES = $(mad_module)

if EXTERNAL_LIBMAD
internal_sources =
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
endif

xineplug_decode_mad_la_SOURCES = \
	xine_mad_decoder.c \
	$(internal_sources)

if EXTERNAL_LIBMAD
xineplug_decode_mad_la_LIBADD = $(XINE_LIB) $(LIBMAD_LIBS)
else
xineplug_decode_mad_la_LIBADD = $(XINE_LIB)
endif
xineplug_decode_mad_la_CFLAGS = $(LIBMAD_CFLAGS) $(VISIBILITY_FLAG)
xineplug_decode_mad_la_LDFLAGS = $(xineplug_ldflags)

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
