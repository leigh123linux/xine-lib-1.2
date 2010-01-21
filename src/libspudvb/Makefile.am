include $(top_builddir)/misc/Makefile.plugins
include $(top_srcdir)/misc/Makefile.common

xineplug_LTLIBRARIES = xineplug_decode_spudvb.la

xineplug_decode_spudvb_la_SOURCES = xine_spudvb_decoder.c
xineplug_decode_spudvb_la_LIBADD  = $(XINE_LIB) $(PTHREAD_LIBS) $(LTLIBINTL)
xineplug_decode_spudvb_la_CFLAGS = $(VISIBILITY_FLAG)
xineplug_decode_spudvb_la_LDFLAGS = $(xineplug_ldflags)
