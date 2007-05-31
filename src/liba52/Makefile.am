include $(top_srcdir)/misc/Makefile.common

AM_CFLAGS  = $(DEFAULT_OCFLAGS) $(VISIBILITY_FLAG)
AM_LDFLAGS = $(xineplug_ldflags)

$(top_builddir)/contrib/a52dec/liba52.la:
	$(MAKE) -C $(top_builddir)/contrib/a52dec

if ENABLE_A52DEC
xineplug_LTLIBRARIES = xineplug_decode_a52.la
endif

xineplug_decode_a52_la_SOURCES = xine_a52_decoder.c
xineplug_decode_a52_la_LIBADD = $(XINE_LIB) $(A52DEC_LIBS) -lm
xineplug_decode_a52_la_CFLAGS = $(AM_CFLAGS) $(A52DEC_CFLAGS)
xineplug_decode_a52_la_DEPENDENCIES = $(A52DEC_DEPS)
