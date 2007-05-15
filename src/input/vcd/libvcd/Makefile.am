include $(top_srcdir)/misc/Makefile.common

AM_CFLAGS = $(DEFAULT_OCFLAGS) $(VISIBILITY_FLAG)

INCLUDES = -I$(top_srcdir)/include -I$(top_builddir)/include -I$(top_srcdir)/lib -I$(top_builddir)/lib

SUBDIRS = libvcd

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

noinst_LTLIBRARIES = libvcd.la libvcdinfo.la
libvcd_la_SOURCES = \
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
libvcd_la_CFLAGS = $(AM_CFLAGS) $(LIBCDIO_CFLAGS)

libvcdinfo_la_SOURCES = \
	info.c \
	inf.c \
	info_private.c \
	vcd_read.c
libvcdinfo_la_CFLAGS = $(AM_CFLAGS) $(LIBCDIO_CFLAGS)
