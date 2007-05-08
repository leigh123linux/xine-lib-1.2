include $(top_srcdir)/misc/Makefile.common

AM_CFLAGS = $(DEFAULT_OCFLAGS) $(VISIBILITY_FLAG)
AM_LDFLAGS = $(xineplug_ldflags)

xineplug_LTLIBRARIES = xineplug_decode_spudvb.la

xineplug_decode_spudvb_la_SOURCES = xine_spudvb_decoder.c
xineplug_decode_spudvb_la_LIBADD  = $(XINE_LIB) $(PTHREAD_LIBS)
