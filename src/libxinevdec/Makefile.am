include $(top_srcdir)/misc/Makefile.common

AM_CFLAGS = $(VISIBILITY_FLAG)
AM_LDFLAGS = $(xineplug_ldflags)

EXTRA_DIST = foovideo.c

if HAVE_WAND
image_module = xineplug_decode_image.la
endif

if HAVE_GDK_PIXBUF
gdkpixbuf_module = xineplug_decode_gdk_pixbuf.la
endif

if HAVE_THEORA
theora_module = xineplug_decode_theora.la
endif

xineplug_LTLIBRARIES = $(image_module) \
	$(gdkpixbuf_module) \
	$(theora_module) \
	xineplug_decode_bitplane.la \
	xineplug_decode_rgb.la \
	xineplug_decode_yuv.la

xineplug_decode_bitplane_la_SOURCES = bitplane.c
xineplug_decode_bitplane_la_LIBADD = $(XINE_LIB)

xineplug_decode_rgb_la_SOURCES = rgb.c
xineplug_decode_rgb_la_LIBADD = $(XINE_LIB)

xineplug_decode_yuv_la_SOURCES = yuv.c
xineplug_decode_yuv_la_LIBADD = $(XINE_LIB)

xineplug_decode_image_la_SOURCES = image.c
xineplug_decode_image_la_CFLAGS = $(AM_CFLAGS) $(WAND_CFLAGS)
xineplug_decode_image_la_LIBADD = $(XINE_LIB) $(DYNAMIC_LD_LIBS) $(WAND_LIBS)

xineplug_decode_gdk_pixbuf_la_SOURCES = gdkpixbuf.c
xineplug_decode_gdk_pixbuf_la_CFLAGS = $(AM_CFLAGS) $(GDK_PIXBUF_CFLAGS)
xineplug_decode_gdk_pixbuf_la_LIBADD = $(XINE_LIB) $(DYNAMIC_LD_LIBS) $(GDK_PIXBUF_LIBS)

xineplug_decode_theora_la_SOURCES = xine_theora_decoder.c
xineplug_decode_theora_la_CFLAGS = $(AM_CFLAGS) $(OGG_CFLAGS) $(THEORA_CFLAGS)
xineplug_decode_theora_la_LIBADD = $(XINE_LIB) $(OGG_LIBS) $(THEORA_LIBS)
