include $(top_srcdir)/misc/Makefile.common

AM_CPPFLAGS = $(LIBFFMPEG_CPPFLAGS) -I$(top_srcdir)/src/libffmpeg/libavutil
AM_CFLAGS = -fno-strict-aliasing
ASFLAGS =

noinst_LTLIBRARIES = libavcodec_mlib.la

libavcodec_mlib_src = dsputil_mlib.c
libavcodec_mlib_dummy = libavcodec_mlib_dummy.c

EXTRA_DIST = $(libavcodec_mlib_src) $(libavcodec_mlib_dummy)

if HAVE_MLIB
mlib_modules = $(libavcodec_mlib_src)
endif

libavcodec_mlib_la_SOURCES = $(mlib_modules) $(libavcodec_mlib_dummy)
