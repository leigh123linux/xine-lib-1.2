include $(top_srcdir)/misc/Makefile.common

AM_CFLAGS = $(VISIBILITY_FLAG)
AM_LDFLAGS = $(xineplug_ldflags)

if HAVE_VDPAU
vdpau_h264_module = xineplug_decode_vdpau_h264.la
VDPAU_CFLAGS = -D_ISOC99_SOURCE

vdpau_mpeg12_module = xineplug_decode_vdpau_mpeg12.la

vdpau_vc1_module = xineplug_decode_vdpau_vc1.la
endif

xineplug_LTLIBRARIES = $(vdpau_h264_module) $(vdpau_mpeg12_module) $(vdpau_vc1_module)

xineplug_decode_vdpau_h264_la_SOURCES = nal.c dpb.c h264_parser.c vdpau_h264.c
xineplug_decode_vdpau_h264_la_CFLAGS = $(AM_CFLAGS) $(VDPAU_CFLAGS)
xineplug_decode_vdpau_h264_la_LIBADD = $(XINE_LIB) $(DYNAMIC_LD_LIBS) -lm

xineplug_decode_vdpau_mpeg12_la_SOURCES = vdpau_mpeg12.c
xineplug_decode_vdpau_mpeg12_la_CFLAGS = $(AM_CFLAGS)
xineplug_decode_vdpau_mpeg12_la_LIBADD = $(XINE_LIB) $(DYNAMIC_LD_LIBS)

xineplug_decode_vdpau_vc1_la_SOURCES = vdpau_vc1.c
xineplug_decode_vdpau_vc1_la_CFLAGS = $(AM_CFLAGS)
xineplug_decode_vdpau_vc1_la_LIBADD = $(XINE_LIB) $(DYNAMIC_LD_LIBS)
