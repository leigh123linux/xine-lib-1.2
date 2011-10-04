include $(top_builddir)/misc/Makefile.plugins
include $(top_srcdir)/misc/Makefile.common

xineplug_LTLIBRARIES = xineplug_decode_spucmml.la

xineplug_decode_spucmml_la_SOURCES = xine_cmml_decoder.c
xineplug_decode_spucmml_la_LIBADD  = $(XINE_LIB) $(LTLIBINTL)
xineplug_decode_spucmml_la_CFLAGS = $(VISIBILITY_FLAG)
xineplug_decode_spucmml_la_LDFLAGS = $(xineplug_ldflags)
