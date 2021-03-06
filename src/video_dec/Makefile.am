AUTOMAKE_OPTIONS = subdir-objects
SUBDIRS = \
	libmpeg2new

include $(top_builddir)/misc/Makefile.plugins
include $(top_srcdir)/misc/Makefile.common

AM_CFLAGS  = $(DEFAULT_OCFLAGS) $(VISIBILITY_FLAG)
AM_LDFLAGS = $(xineplug_ldflags)

EXTRA_DIST = foovideo.c
EXTRA_DIST += libvdpau/nal.c libvdpau/dpb.c libvdpau/h264_parser.c

if ENABLE_IMAGEMAGICK
image_module = xineplug_decode_image.la
endif

if ENABLE_GDK_PIXBUF
gdkpixbuf_module = xineplug_decode_gdk_pixbuf.la
endif

if ENABLE_LIBJPEG
libjpeg_module = xineplug_decode_libjpeg.la
endif

if ENABLE_OPENHEVC
libopenhevc_module = xineplug_decode_libopenhevc.la
endif

if ENABLE_VPX
libvpx_module = xineplug_decode_libvpx.la
endif

if ENABLE_MMAL
libmmal_module = xineplug_decode_libmmal.la
endif

if ENABLE_VDPAU
vdpau_module = xineplug_decode_vdpau.la
VDPAU_CFLAGS += -D_ISOC99_SOURCE
endif

if ENABLE_RAWVIDEO
rawvideo_module = xineplug_decode_rawvideo.la
endif

if ENABLE_LIBMPEG2
libmpeg2_module = xineplug_decode_mpeg2.la
endif

xineplug_LTLIBRARIES = $(image_module) \
	$(gdkpixbuf_module) \
	$(libjpeg_module) \
	$(theora_module) \
	$(libopenhevc_module) \
	$(libvpx_module) \
	$(libmmal_module) \
	$(libmpeg2_module) \
	$(vdpau_module) \
	$(rawvideo_module)

xineplug_decode_rawvideo_la_SOURCES = \
	group_raw.c \
	group_raw.h \
	bitplane.c \
	rgb.c \
	yuv.c
xineplug_decode_rawvideo_la_LIBADD = $(XINE_LIB) $(LTLIBINTL)

xineplug_decode_image_la_SOURCES = image.c
xineplug_decode_image_la_LIBADD = $(XINE_LIB) $(WAND_LIBS)
xineplug_decode_image_la_CFLAGS = $(AM_CFLAGS) $(WAND_CFLAGS)

xineplug_decode_gdk_pixbuf_la_SOURCES = gdkpixbuf.c
xineplug_decode_gdk_pixbuf_la_LIBADD = $(XINE_LIB) $(GDK_PIXBUF_LIBS)
xineplug_decode_gdk_pixbuf_la_CFLAGS = $(AM_CFLAGS) $(GDK_PIXBUF_CFLAGS)

xineplug_decode_libjpeg_la_SOURCES = libjpeg.c
xineplug_decode_libjpeg_la_LIBADD = $(XINE_LIB) $(JPEG_LIBS) $(LTLIBINTL)
xineplug_decode_libjpeg_la_CFLAGS = $(AM_CFLAGS) $(JPEG_CFLAGS)

xineplug_decode_libopenhevc_la_SOURCES = libopenhevc.c
xineplug_decode_libopenhevc_la_LIBADD = $(XINE_LIB) $(OPENHEVC_LIBS) -lm
xineplug_decode_libopenhevc_la_CFLAGS = $(AM_CFLAGS) $(OPENHEVC_CFLAGS)
xineplug_decode_libopenhevc_la_LDFLAGS = $(AM_LDFLAGS) -Wl,-Bsymbolic

xineplug_decode_libvpx_la_SOURCES = libvpx.c
xineplug_decode_libvpx_la_LIBADD = $(XINE_LIB) $(VPX_LIBS)
xineplug_decode_libvpx_la_CFLAGS = $(AM_CFLAGS) $(VPX_CFLAGS)

xineplug_decode_libmmal_la_SOURCES = mmal.c
xineplug_decode_libmmal_la_LIBADD = $(XINE_LIB) $(PTHREAD_LIBS) $(MMAL_LIBS)
xineplug_decode_libmmal_la_CFLAGS = $(AM_CFLAGS) $(MMAL_CFLAGS)
xineplug_decode_libmmal_la_LDFLAGS = $(AM_LDFLAGS) $(MMAL_LDFLAGS)

xineplug_decode_vdpau_la_SOURCES = \
	libvdpau/group_vdpau.c \
	libvdpau/group_vdpau.h \
	libvdpau/alterh264_bits_reader.h \
	libvdpau/alterh264_decode.c \
	libvdpau/alterh264_decode.h \
	libvdpau/bits_reader.h \
	libvdpau/cpb.h \
	libvdpau/dpb.h \
	libvdpau/h264_parser.h \
	libvdpau/nal.h \
	libvdpau/vdpau_h264.c \
	libvdpau/vdpau_mpeg12.c \
	libvdpau/vdpau_vc1.c \
	libvdpau/vdpau_mpeg4.c
xineplug_decode_vdpau_la_CFLAGS = $(AM_CFLAGS) $(VDPAU_CFLAGS)
xineplug_decode_vdpau_la_LIBADD = $(XINE_LIB) -lm

xineplug_decode_mpeg2_la_SOURCES = \
	libmpeg2/cpu_state.c \
	libmpeg2/decode.c \
	libmpeg2/header.c \
	libmpeg2/idct.c \
	libmpeg2/idct_altivec.c \
	libmpeg2/idct_mlib.h \
	libmpeg2/idct_mlib.c \
	libmpeg2/idct_mmx.c \
	libmpeg2/motion_comp.c \
	libmpeg2/motion_comp_altivec.c \
	libmpeg2/motion_comp_mmx.c \
	libmpeg2/motion_comp_mlib.c \
	libmpeg2/motion_comp_vis.c \
	libmpeg2/mpeg2.h \
	libmpeg2/mpeg2_internal.h \
	libmpeg2/slice.c \
	libmpeg2/slice_xvmc.c \
	libmpeg2/slice_xvmc_vld.c \
	libmpeg2/stats.c \
	libmpeg2/vis.h \
	libmpeg2/vlc.h \
	libmpeg2/xine_mpeg2_decoder.c \
	libmpeg2/xvmc.h \
	libmpeg2/xvmc_vld.h \
	libmpeg2/libmpeg2_accel.h \
	libmpeg2/libmpeg2_accel.c

xineplug_decode_mpeg2_la_LIBADD = $(XINE_LIB) $(MLIB_LIBS) $(LTLIBINTL) -lm
xineplug_decode_mpeg2_la_CFLAGS = $(AM_CFLAGS) $(MLIB_CFLAGS)
