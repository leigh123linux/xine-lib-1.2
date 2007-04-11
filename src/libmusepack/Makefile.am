include $(top_srcdir)/misc/Makefile.common

if MUSEPACK
xineplug_LTLIBRARIES = xineplug_decode_mpc.la
endif

if !EXTERNAL_MPCDEC
link_libmpcdec = $(top_builddir)/contrib/libmpcdec/libmpcdec.la
cflags_libmpcdec = -I$(top_srcdir)/contrib/libmpcdec
else
link_libmpcdec = $(MPCDEC_LIBS)
cflags_libmpcdec = $(MPCDEC_CFLAGS)
endif

$(top_builddir)/contrib/libmpcdec/libmpcdec.la:
	$(MAKE) -C $(top_builddir)/contrib/libmpcdec

xineplug_decode_mpc_la_SOURCES = xine_musepack_decoder.c
xineplug_decode_mpc_la_CFLAGS = $(VISIBILITY_FLAG) $(cflags_libmpcdec)
xineplug_decode_mpc_la_LIBADD = $(XINE_LIB) $(link_libmpcdec)
xineplug_decode_mpc_la_LDFLAGS = $(xineplug_ldflags)
