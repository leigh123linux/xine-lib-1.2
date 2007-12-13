include $(top_srcdir)/misc/Makefile.common

AM_CFLAGS  = $(DEFAULT_OCFLAGS) $(VISIBILITY_FLAG)
AM_LDFLAGS = $(xineplug_ldflags)

noinst_HEADERS = cc_decoder.h

xineplug_LTLIBRARIES = xineplug_decode_spucc.la

xineplug_decode_spucc_la_SOURCES =  cc_decoder.c xine_cc_decoder.c
xineplug_decode_spucc_la_LIBADD = $(XINE_LIB)
xineplug_decode_spucc_la_CFLAGS = $(AM_CFLAGS) -fno-strict-aliasing
