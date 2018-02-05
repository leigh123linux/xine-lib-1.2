AUTOMAKE_OPTIONS = subdir-objects
include $(top_builddir)/misc/Makefile.plugins
include $(top_srcdir)/misc/Makefile.common

AM_CFLAGS  = $(DEFAULT_OCFLAGS) $(VISIBILITY_FLAG)

EXTRA_DIST = \
	include/alpha_asm.h \
	include/attributes.h \
	include/mmx.h \
	include/mpeg2.h \
	include/mpeg2convert.h \
	include/tendra.h \
	include/video_out.h \
	include/vis.h

if ENABLE_MPEG2NEW
mpeg2new_module = xineplug_decode_mpeg2new.la
noinst_LTLIBRARIES = libmpeg2.la libmpeg2arch.la
endif

xineplug_LTLIBRARIES = $(mpeg2new_module)

libmpeg2_la_SOURCES = \
	libmpeg2/alloc.c \
	libmpeg2/decode.c \
	libmpeg2/header.c \
	libmpeg2/idct.c \
	libmpeg2/motion_comp.c \
	libmpeg2/mpeg2_internal.h \
	libmpeg2/slice.c \
	libmpeg2/vlc.h
libmpeg2_la_LIBADD = libmpeg2arch.la

libmpeg2arch_la_SOURCES = \
	libmpeg2/motion_comp_mmx.c \
	libmpeg2/idct_mmx.c \
	libmpeg2/motion_comp_altivec.c \
	libmpeg2/idct_altivec.c \
	libmpeg2/motion_comp_alpha.c \
	libmpeg2/idct_alpha.c \
	libmpeg2/motion_comp_vis.c \
	libmpeg2/cpu_accel.c \
	libmpeg2/cpu_state.c

xineplug_decode_mpeg2new_la_SOURCES = \
	xine_mpeg2new_decoder.c
xineplug_decode_mpeg2new_la_LIBADD = $(XINE_LIB) libmpeg2.la
xineplug_decode_mpeg2new_la_CFLAGS = $(AM_CFLAGS) $(MLIB_CFLAGS)
xineplug_decode_mpeg2new_la_LDFLAGS = $(xineplug_ldflags)
