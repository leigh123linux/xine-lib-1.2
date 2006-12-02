include $(top_srcdir)/misc/Makefile.common

libdir = $(XINE_PLUGINDIR)

lib_LTLIBRARIES = xineplug_decode_spucmml.la

xineplug_decode_spucmml_la_SOURCES = xine_decoder.c
xineplug_decode_spucmml_la_LIBADD  = $(XINE_LIB)
xineplug_decode_spucmml_la_CFLAGS = $(VISIBILITY_FLAG)
xineplug_decode_spucmml_la_LDFLAGS = -avoid-version -module
