include $(top_srcdir)/misc/Makefile.common

AM_CFLAGS  = $(DEFAULT_OCFLAGS) $(VISIBILITY_FLAG)
AM_LDFLAGS = $(xineplug_ldflags)

SUBDIRS = gsm610

EXTRA_DIST = fooaudio.c

if ENABLE_VORBIS
vorbis_module = xineplug_decode_vorbis.la
endif

if ENABLE_SPEEX
speex_module = xineplug_decode_speex.la
endif

if ENABLE_MUSEPACK
musepack_module = xineplug_decode_mpc.la
endif

if ENABLE_DTS
dts_module = xineplug_decode_dts.la
endif

$(top_builddir)/contrib/libmpcdec/libmpcdec.la:
	$(MAKE) -C $(top_builddir)/contrib/libmpcdec

$(top_builddir)/contrib/libdca/libdca.la:
	$(MAKE) -C $(top_builddir)/contrib/libdca

$(top_builddir)/contrib/nosefart/libnosefart.la:
	$(MAKE) -C $(top_builddir)/contrib/nosefart

xineplug_LTLIBRARIES = \
	xineplug_decode_gsm610.la \
	xineplug_decode_nsf.la \
	xineplug_decode_lpcm.la \
	$(vorbis_module) \
	$(speex_module) \
	$(musepack_module) \
	$(dts_module)

xineplug_decode_gsm610_la_SOURCES = gsm610.c
xineplug_decode_gsm610_la_LIBADD = $(XINE_LIB) gsm610/libgsm610.la

xineplug_decode_nsf_la_SOURCES = nsf.c
xineplug_decode_nsf_la_LIBADD = $(XINE_LIB) $(top_builddir)/contrib/nosefart/libnosefart.la -lm
xineplug_decode_nsf_la_CFLAGS = $(AM_CFLAGS) -fno-strict-aliasing
xineplug_decode_nsf_la_CPPFLAGS = $(AM_CPPFLAGS) -DNSF_PLAYER -I$(top_srcdir)/contrib/nosefart

xineplug_decode_lpcm_la_SOURCES = xine_lpcm_decoder.c
xineplug_decode_lpcm_la_LIBADD = $(XINE_LIB)

xineplug_decode_vorbis_la_SOURCES = xine_vorbis_decoder.c
xineplug_decode_vorbis_la_LIBADD = $(XINE_LIB) $(VORBIS_LIBS) $(OGG_LIBS)
xineplug_decode_vorbis_la_CFLAGS = $(AM_CFLAGS) $(VORBIS_CFLAGS)

xineplug_decode_speex_la_SOURCES = xine_speex_decoder.c
xineplug_decode_speex_la_LIBADD = $(XINE_LIB) $(SPEEX_LIBS)
xineplug_decode_speex_la_CFLAGS = $(AM_CFLAGS) $(SPEEX_CFLAGS)

xineplug_decode_mpc_la_SOURCES = xine_musepack_decoder.c
xineplug_decode_mpc_la_DEPENDENCIES = $(MPCDEC_DEPS)
xineplug_decode_mpc_la_LIBADD = $(XINE_LIB) $(MPCDEC_LIBS)
xineplug_decode_mpc_la_CFLAGS = $(AM_CFLAGS) $(MPCDEC_CFLAGS)

xineplug_decode_dts_la_SOURCES = xine_dts_decoder.c
xineplug_decode_dts_la_DEPENDENCIES = $(LIBDTS_DEPS)
xineplug_decode_dts_la_LIBADD = $(XINE_LIB) $(LIBDTS_LIBS)
xineplug_decode_dts_la_CFLAGS = $(AM_CFLAGS) $(LIBDTS_CFLAGS)
