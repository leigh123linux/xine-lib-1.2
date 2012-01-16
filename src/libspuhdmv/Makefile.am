include $(top_builddir)/misc/Makefile.plugins
include $(top_srcdir)/misc/Makefile.common

xineplug_LTLIBRARIES = xineplug_decode_spuhdmv.la

xineplug_decode_spuhdmv_la_SOURCES = xine_hdmv_decoder.c
xineplug_decode_spuhdmv_la_LIBADD  = $(XINE_LIB) $(PTHREAD_LIBS) $(LTLIBINTL)
xineplug_decode_spuhdmv_la_CFLAGS = $(VISIBILITY_FLAG)
xineplug_decode_spuhdmv_la_LDFLAGS = $(xineplug_ldflags)
