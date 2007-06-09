include $(top_srcdir)/misc/Makefile.common

AM_LDFLAGS = $(xineplug_ldflags)

xineplug_LTLIBRARIES = xineplug_decode_sputext.la xineplug_dmx_sputext.la

xineplug_dmx_sputext_la_SOURCES = demux_sputext.c
xineplug_dmx_sputext_la_LIBADD  = $(XINE_LIB) $(LTLIBINTL)
xineplug_dmx_sputext_la_CFLAGS = $(VISIBILITY_FLAG)

xineplug_decode_sputext_la_SOURCES = xine_sputext_decoder.c 
xineplug_decode_sputext_la_LIBADD  = $(XINE_LIB) $(LTLIBINTL)
xineplug_decode_sputext_la_CFLAGS = $(VISIBILITY_FLAG)
