include $(top_srcdir)/misc/Makefile.common

libdir = $(XINE_PLUGINDIR)

sputext_decoder = xineplug_decode_sputext.la xineplug_dmx_sputext.la

lib_LTLIBRARIES = $(sputext_decoder)

xineplug_dmx_sputext_la_SOURCES = demux_sputext.c
xineplug_dmx_sputext_la_LIBADD  = $(XINE_LIB)
xineplug_dmx_sputext_la_CFLAGS = $(VISIBILITY_FLAG)
xineplug_dmx_sputext_la_LDFLAGS = -avoid-version -module

xineplug_decode_sputext_la_SOURCES = xine_decoder.c 
xineplug_decode_sputext_la_LIBADD  = $(XINE_LIB)
xineplug_decode_sputext_la_CFLAGS = $(VISIBILITY_FLAG)
xineplug_decode_sputext_la_LDFLAGS = -avoid-version -module
