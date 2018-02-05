include $(top_builddir)/misc/Makefile.plugins
include $(top_srcdir)/misc/Makefile.common

AM_CFLAGS  = $(DEFAULT_OCFLAGS) $(VISIBILITY_FLAG)
AM_LDFLAGS = $(xineplug_ldflags)

if ENABLE_LIBMPEG2
libmpeg2_module = xineplug_decode_mpeg2.la
endif

xineplug_LTLIBRARIES = $(libmpeg2_module)

xineplug_decode_mpeg2_la_SOURCES = \
	cpu_state.c \
	decode.c \
	header.c \
	idct.c \
	idct_altivec.c \
	idct_mlib.h \
	idct_mlib.c \
	idct_mmx.c \
	motion_comp.c \
	motion_comp_altivec.c \
	motion_comp_mmx.c \
	motion_comp_mlib.c \
	motion_comp_vis.c \
	mpeg2.h \
	mpeg2_internal.h \
	slice.c \
	slice_xvmc.c \
	slice_xvmc_vld.c \
	stats.c \
	vis.h \
	vlc.h \
	xine_mpeg2_decoder.c \
	xvmc.h \
	xvmc_vld.h \
	libmpeg2_accel.h \
	libmpeg2_accel.c

xineplug_decode_mpeg2_la_LIBADD = $(XINE_LIB) $(MLIB_LIBS) $(LTLIBINTL) -lm
xineplug_decode_mpeg2_la_CFLAGS = $(AM_CFLAGS) $(MLIB_CFLAGS)
