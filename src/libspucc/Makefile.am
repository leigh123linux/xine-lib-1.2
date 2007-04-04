include $(top_srcdir)/misc/Makefile.common

libdir = $(XINE_PLUGINDIR)

lib_LTLIBRARIES = xineplug_decode_spucc.la

xineplug_decode_spucc_la_SOURCES =  cc_decoder.c xine_decoder.c
xineplug_decode_spucc_la_CFLAGS = $(VISIBILITY_FLAG) $(AM_CFLAGS) -fno-strict-aliasing
xineplug_decode_spucc_la_LIBADD = $(XINE_LIB)
xineplug_decode_spucc_la_LDFLAGS = -avoid-version -module

noinst_HEADERS = cc_decoder.h
