include $(top_srcdir)/misc/Makefile.common

EXTRA_DIST = README vidix.txt

SUBDIRS = drivers

AM_CFLAGS = $(STATIC)
AM_CPPFLAGS = -I$(top_srcdir)/src/video_out/vidix \
	      -I$(top_builddir)/src/video_out/libdha

if HAVE_VIDIX
vidix_lib = libvidix.la
endif

noinst_LTLIBRARIES = $(vidix_lib)

libvidix_la_SOURCES = vidixlib.c
libvidix_la_LIBADD = $(DYNAMIC_LD_LIBS) $(top_builddir)/src/video_out/libdha/libdha.la

noinst_HEADERS = fourcc.h vidix.h vidixlib.h

