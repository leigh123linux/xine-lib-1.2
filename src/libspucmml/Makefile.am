include $(top_srcdir)/misc/Makefile.common

AM_CFLAGS  = $(DEFAULT_OCFLAGS) $(VISIBILITY_FLAG)
AM_LDFLAGS = $(xineplug_ldflags)

xineplug_LTLIBRARIES = xineplug_decode_spucmml.la

xineplug_decode_spucmml_la_SOURCES = xine_cmml_decoder.c
xineplug_decode_spucmml_la_LIBADD  = $(XINE_LIB) $(LTLIBINTL)
