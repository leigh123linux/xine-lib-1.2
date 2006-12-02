include $(top_srcdir)/misc/Makefile.common

AM_CFLAGS = $(LIBMPEG2_CFLAGS)

libdir = $(XINE_PLUGINDIR)

lib_LTLIBRARIES = xineplug_decode_mpeg2.la

xineplug_decode_mpeg2_la_SOURCES = \
	cpu_state.c \
	decode.c \
	header.c \
	idct.c \
	idct_altivec.c \
	idct_mlib.c \
	idct_mmx.c \
	motion_comp.c \
	motion_comp_altivec.c \
	motion_comp_mmx.c \
	motion_comp_mlib.c \
	motion_comp_vis.c \
	slice.c \
	slice_xvmc.c \
	slice_xvmc_vld.c \
	stats.c \
	xine_decoder.c \
	libmpeg2_accel.c

xineplug_decode_mpeg2_la_LIBADD = $(MLIB_LIBS) $(XINE_LIB)
xineplug_decode_mpeg2_la_CFLAGS = $(VISIBILITY_FLAG)
xineplug_decode_mpeg2_la_LDFLAGS = -avoid-version -module

noinst_HEADERS = vlc.h mpeg2.h xvmc.h xvmc_vld.h mpeg2_internal.h idct_mlib.h vis.h \
	libmpeg2_accel.h
