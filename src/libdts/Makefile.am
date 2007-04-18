include $(top_srcdir)/misc/Makefile.common

if DTS
dts_module = xineplug_decode_dts.la
endif

$(top_builddir)/contrib/libdca/libdca.la:
	$(MAKE) -C $(top_builddir)/contrib/libdca

xineplug_LTLIBRARIES = $(dts_module)

xineplug_decode_dts_la_SOURCES = xine_dts_decoder.c
xineplug_decode_dts_la_CFLAGS = $(LIBDTS_CFLAGS) $(VISIBILITY_FLAG)
xineplug_decode_dts_la_DEPENDENCIES = $(LIBDTS_DEPS)
xineplug_decode_dts_la_LIBADD = $(XINE_LIB) $(LIBDTS_LIBS)
xineplug_decode_dts_la_LDFLAGS = $(xineplug_ldflags)
