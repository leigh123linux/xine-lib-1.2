include $(top_srcdir)/misc/Makefile.common

AM_CFLAGS   = $(DEFAULT_OCFLAGS) $(VISIBILITY_FLAG)
AM_LDFLAGS  = $(xineplug_ldflags)

$(top_builddir)/contrib/libmad/libmad.la:
	$(MAKE) -C $(top_builddir)/contrib/libmad

if ENABLE_MAD
xineplug_LTLIBRARIES = xineplug_decode_mad.la
endif

xineplug_decode_mad_la_SOURCES = xine_mad_decoder.c
xineplug_decode_mad_la_DEPENDENCIES = $(LIBMAD_DEPS)
xineplug_decode_mad_la_LIBADD = $(XINE_LIB) $(LIBMAD_LIBS)
xineplug_decode_mad_la_CFLAGS = $(AM_CFLAGS) $(LIBMAD_CFLAGS)
