include $(top_srcdir)/misc/Makefile.common

SUBDIRS = libvcd

INCLUDES = $(LIBCDIO_CFLAGS) -I$(top_srcdir)/include -I$(top_srcdir)/lib

libvcd_SRCS = \
	vcd.c \
	data_structures.c \
	directory.c \
	files.c \
	image.c \
	image_bincue.c \
	image_cdrdao.c \
	image_nrg.c \
	logging.c \
	mpeg.c \
	mpeg_stream.c \
	pbc.c \
	salloc.c \
	sector.c \
	stream.c \
	stream_stdio.c \
	util.c

libvcdinfo_SRCS = \
	info.c \
	inf.c \
	info_private.c \
	vcd_read.c

EXTRA_DIST = $(libvcd_SRCS) $(libvcdinfo_SRCS)

if ENABLE_VCD
if HAVE_VCDNAV
else
noinst_LTLIBRARIES = libvcd.la libvcdinfo.la
libvcd_la_SOURCES = $(libvcd_SRCS)
libvcd_la_LDFLAGS = -avoid-version -module
libvcd_la_LIBADD = $(LIBCDIO_LIBS) $(LIBISO9660_LIBS)
libvcdinfo_la_SOURCES = $(libvcdinfo_SRCS)
libvcdinfo_la_LDFLAGS = -avoid-version -module
endif
endif

noinst_HEADERS = \
	vcd_assert.h \
	data_structures.h \
	info_private.h \
	pbc.h \
	stream_stdio.h \
	bitvec.h \
	dict.h \
	mpeg.h \
	salloc.h \
	util.h \
	bytesex_asm.h \
	directory.h \
	mpeg_stream.h \
	sector_private.h \
	vcd.h \
	bytesex.h \
	image_sink.h \
	obj.h \
	stream.h \
	vcd_read.h
