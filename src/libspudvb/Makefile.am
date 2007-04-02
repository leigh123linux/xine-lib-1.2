include $(top_srcdir)/misc/Makefile.common

libdir = $(XINE_PLUGINDIR)

lib_LTLIBRARIES = xineplug_decode_spudvb.la

xineplug_decode_spudvb_la_SOURCES = xine_decoder.c
xineplug_decode_spudvb_la_LIBADD  = $(XINE_LIB) $(PTHREAD_LIBS)
xineplug_decode_spudvb_la_CFLAGS = $(VISIBILITY_FLAG)
xineplug_decode_spudvb_la_LDFLAGS = -avoid-version -module
