include $(top_srcdir)/misc/Makefile.common

AM_CFLAGS  = $(DEFAULT_OCFLAGS) $(VISIBILITY_FLAG)
AM_LDFLAGS = $(xineplug_ldflags)

EXTRA_DIST = foovideo.c

if ENABLE_IMAGEMAGICK
image_module = xineplug_decode_image.la
endif

if ENABLE_GDK_PIXBUF
gdkpixbuf_module = xineplug_decode_gdk_pixbuf.la
endif

xineplug_LTLIBRARIES = $(image_module) \
	$(gdkpixbuf_module) \
	$(theora_module) \
	xineplug_decode_bitplane.la \
	xineplug_decode_rgb.la \
	xineplug_decode_yuv.la \
	xineplug_decode_mpeg2.la

xineplug_decode_bitplane_la_SOURCES = bitplane.c
xineplug_decode_bitplane_la_LIBADD = $(XINE_LIB) $(LTLIBINTL)

xineplug_decode_rgb_la_SOURCES = rgb.c
xineplug_decode_rgb_la_LIBADD = $(XINE_LIB)

xineplug_decode_yuv_la_SOURCES = yuv.c
xineplug_decode_yuv_la_LIBADD = $(XINE_LIB)

xineplug_decode_image_la_SOURCES = image.c
xineplug_decode_image_la_LIBADD = $(XINE_LIB) $(DYNAMIC_LD_LIBS) $(WAND_LIBS)
xineplug_decode_image_la_CFLAGS = $(AM_CFLAGS) $(WAND_CFLAGS)

xineplug_decode_gdk_pixbuf_la_SOURCES = gdkpixbuf.c
xineplug_decode_gdk_pixbuf_la_LIBADD = $(XINE_LIB) $(DYNAMIC_LD_LIBS) $(GDK_PIXBUF_LIBS)
xineplug_decode_gdk_pixbuf_la_CFLAGS = $(AM_CFLAGS) $(GDK_PIXBUF_CFLAGS)

xineplug_decode_mpeg2_la_SOURCES = libmpeg2/cpu_state.c			\
	libmpeg2/decode.c libmpeg2/header.c libmpeg2/idct.c		\
	libmpeg2/idct_altivec.c libmpeg2/idct_mlib.c			\
	libmpeg2/idct_mmx.c libmpeg2/motion_comp.c			\
	libmpeg2/motion_comp_altivec.c libmpeg2/motion_comp_mmx.c	\
	libmpeg2/motion_comp_mlib.c libmpeg2/motion_comp_vis.c		\
	libmpeg2/slice.c libmpeg2/slice_xvmc.c				\
	libmpeg2/slice_xvmc_vld.c libmpeg2/stats.c			\
	libmpeg2/xine_mpeg2_decoder.c libmpeg2/libmpeg2_accel.c		\
	libmpeg2/vlc.h libmpeg2/mpeg2.h libmpeg2/xvmc.h			\
	libmpeg2/xvmc_vld.h libmpeg2/mpeg2_internal.h			\
	libmpeg2/idct_mlib.h libmpeg2/vis.h libmpeg2/libmpeg2_accel.h
xineplug_decode_mpeg2_la_LIBADD = $(XINE_LIB) $(MLIB_LIBS) -lm
xineplug_decode_mpeg2_la_CFLAGS = $(AM_CFLAGS) $(MLIB_CFLAGS)
