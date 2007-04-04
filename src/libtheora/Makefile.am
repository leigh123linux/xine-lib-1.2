include $(top_srcdir)/misc/Makefile.common

AM_CFLAGS = $(THEORA_CFLAGS)

libdir = $(XINE_PLUGINDIR)

if HAVE_THEORA
theora_module = xineplug_decode_theora.la
endif

lib_LTLIBRARIES = $(theora_module)

xineplug_decode_theora_la_SOURCES = xine_decoder.c
xineplug_decode_theora_la_LIBADD = $(OGG_LIBS) $(THEORA_LIBS) $(XINE_LIB)
xineplug_decode_theora_la_LDFLAGS = -avoid-version -module
