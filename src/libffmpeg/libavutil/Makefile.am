include $(top_srcdir)/misc/Makefile.common

AM_CPPFLAGS = $(LIBFFMPEG_CPPFLAGS) -I$(top_srcdir)/src/libffmpeg \
	-I$(top_builddir)/src/libffmpeg
AM_CFLAGS = -fno-strict-aliasing
ASFLAGS =

noinst_LTLIBRARIES = libavutil.la

libavutil_la_SOURCES = \
	adler32.c \
	crc.c \
	integer.c \
	lls.c \
	log.c \
	mathematics.c \
	md5.c \
	mem.c \
	rational.c

libavutil_la_LDFLAGS = -avoid-version -module

noinst_HEADERS = \
	adler32.h \
	avutil.h \
	bswap.h \
	common.h \
	crc.h \
	integer.h \
	internal.h \
	intfloat_readwrite.h \
	intreadwrite.h \
	lls.h \
	log.h \
	mathematics.h \
	md5.h \
	rational.h \
	x86_cpu.h
