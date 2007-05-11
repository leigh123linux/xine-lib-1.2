include $(top_srcdir)/misc/Makefile.common

SUBDIRS = drivers

AM_CFLAGS   = $(DEFAULT_OCFLAGS)
AM_CPPFLAGS = -I$(top_srcdir)/src/video_out/vidix -I$(top_builddir)/src/video_out/libdha

EXTRA_DIST = README vidix.txt

noinst_HEADERS = fourcc.h vidix.h vidixlib.h

noinst_LTLIBRARIES = libvidix.la

libvidix_la_SOURCES = vidixlib.c
libvidix_la_LIBADD = $(DYNAMIC_LD_LIBS) $(top_builddir)/src/video_out/libdha/libdha.la

