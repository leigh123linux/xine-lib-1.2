include $(top_srcdir)/misc/Makefile.common

AM_CPPFLAGS = $(LIBFFMPEG_CPPFLAGS) -I$(top_srcdir)/src/libffmpeg/libavutil -I$(top_srcdir)/src/libffmpeg
AM_CFLAGS = -fno-strict-aliasing
ASFLAGS =

noinst_LTLIBRARIES = libavcodec_sparc.la

libavcodec_sparc_src = dsputil_vis.c
libavcodec_sparc_dummy = libavcodec_sparc_dummy.c

EXTRA_DIST = $(libavcodec_sparc_src) $(libavcodec_sparc_dummy) vis.h

if ENABLE_VIS
sparc_modules = $(libavcodec_sparc_src)
endif

libavcodec_sparc_la_SOURCES = $(sparc_modules) $(libavcodec_sparc_dummy)
