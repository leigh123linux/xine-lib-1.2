include $(top_srcdir)/misc/Makefile.common

libdir = $(XINE_PLUGINDIR)

lib_LTLIBRARIES = xineplug_decode_lpcm.la

xineplug_decode_lpcm_la_SOURCES = xine_decoder.c
xineplug_decode_lpcm_la_LIBADD = $(XINE_LIB)
xineplug_decode_lpcm_la_CFLAGS = $(VISIBILITY_FLAG)
xineplug_decode_lpcm_la_LDFLAGS = -avoid-version -module
