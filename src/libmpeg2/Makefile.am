include $(top_srcdir)/misc/Makefile.common

xineplug_LTLIBRARIES = xineplug_decode_mpeg2.la

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
	xine_mpeg2_decoder.c \
	libmpeg2_accel.c

xineplug_decode_mpeg2_la_LIBADD = $(MLIB_LIBS) $(XINE_LIB) -lm
xineplug_decode_mpeg2_la_CFLAGS = $(VISIBILITY_FLAG) $(LIBMPEG2_CFLAGS)
xineplug_decode_mpeg2_la_LDFLAGS = $(xineplug_ldflags)

noinst_HEADERS = vlc.h mpeg2.h xvmc.h xvmc_vld.h mpeg2_internal.h idct_mlib.h vis.h \
	libmpeg2_accel.h
