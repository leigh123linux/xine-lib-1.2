include $(top_srcdir)/misc/Makefile.common

AM_CFLAGS = $(VISIBILITY_FLAG)
AM_LDFLAGS = $(xineplug_ldflags)

EXTRA_DIST = vdpau_h264.c

if HAVE_VDPAU
vdpau_h264_module = xineplug_decode_vdpau_h264.la
endif

xineplug_LTLIBRARIES = $(vdpau_h264_module) \
	xineplug_decode_vdpau_h264.la \

xineplug_decode_image_la_SOURCES = nal_parser.c vdpau_h264.c
xineplug_decode_image_la_CFLAGS = $(AM_CFLAGS) $(WAND_CFLAGS)
xineplug_decode_image_la_LIBADD = $(XINE_LIB) $(DYNAMIC_LD_LIBS) $(WAND_LIBS)
