include $(top_srcdir)/misc/Makefile.common

# -fomit-frame-pointer is always needed. it might cause debug to not
# work, but at least it compiles.
AM_CFLAGS = -fomit-frame-pointer -fno-strict-aliasing
AM_CPPFLAGS = $(LIBFFMPEG_CPPFLAGS) -I$(top_srcdir)/src/libffmpeg/libavcodec  
ASFLAGS =

noinst_LTLIBRARIES = libpostprocess.la

EXTRA_DIST = postprocess_template.c postprocess_altivec_template.c

libpostprocess_la_SOURCES = postprocess.c

noinst_HEADERS = mangle.h postprocess.h postprocess_internal.h
