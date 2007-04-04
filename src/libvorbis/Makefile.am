include $(top_srcdir)/misc/Makefile.common

libdir = $(XINE_PLUGINDIR)

if HAVE_VORBIS
vorbis_module = xineplug_decode_vorbis.la
endif

lib_LTLIBRARIES = $(vorbis_module)

xineplug_decode_vorbis_la_SOURCES = xine_decoder.c
xineplug_decode_vorbis_la_LIBADD = $(VORBIS_LIBS) $(OGG_LIBS) $(XINE_LIB)
xineplug_decode_vorbis_la_CFLAGS = $(VORBIS_CFLAGS) $(VISIBILITY_FLAG)
xineplug_decode_vorbis_la_LDFLAGS = -avoid-version -module
