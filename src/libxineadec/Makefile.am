include $(top_srcdir)/misc/Makefile.common

EXTRA_DIST = fooaudio.c

SUBDIRS = gsm610 nosefart

AM_LDFLAGS = $(xineplug_ldflags)

if HAVE_VORBIS
vorbis_module = xineplug_decode_vorbis.la
endif

if HAVE_SPEEX
speex_module = xineplug_decode_speex.la
endif

if MUSEPACK
musepack_module = xineplug_decode_mpc.la
endif

$(top_builddir)/contrib/libmpcdec/libmpcdec.la:
	$(MAKE) -C $(top_builddir)/contrib/libmpcdec

xineplug_LTLIBRARIES = \
	xineplug_decode_gsm610.la \
	xineplug_decode_nsf.la \
	xineplug_decode_lpcm.la \
	$(vorbis_module) \
	$(speex_module) \
	$(musepack_module)

xineplug_decode_gsm610_la_SOURCES = gsm610.c
xineplug_decode_gsm610_la_CFLAGS = $(VISIBILITY_FLAG)
xineplug_decode_gsm610_la_LIBADD = $(XINE_LIB) gsm610/libgsm610.la

xineplug_decode_nsf_la_SOURCES = nsf.c
xineplug_decode_nsf_la_CFLAGS = $(VISIBILITY_FLAG) -DNSF_PLAYER -fno-strict-aliasing
xineplug_decode_nsf_la_LIBADD = $(XINE_LIB) -lm nosefart/libnosefart.la

xineplug_decode_lpcm_la_SOURCES = xine_lpcm_decoder.c
xineplug_decode_lpcm_la_CFLAGS = $(VISIBILITY_FLAG)
xineplug_decode_lpcm_la_LIBADD = $(XINE_LIB)

xineplug_decode_vorbis_la_SOURCES = xine_vorbis_decoder.c
xineplug_decode_vorbis_la_LIBADD = $(XINE_LIB) $(VORBIS_LIBS) $(OGG_LIBS)
xineplug_decode_vorbis_la_CFLAGS = $(VISIBILITY_FLAG) $(VORBIS_CFLAGS)

xineplug_decode_speex_la_SOURCES = xine_speex_decoder.c
xineplug_decode_speex_la_LIBADD = $(XINE_LIB) $(SPEEX_LIBS)
xineplug_decode_speex_la_CFLAGS = $(VISIBILITY_FLAGS) $(SPEEX_CFLAGS)

xineplug_decode_mpc_la_SOURCES = xine_musepack_decoder.c
xineplug_decode_mpc_la_CFLAGS = $(VISIBILITY_FLAG) $(MPCDEC_CFLAGS)
xineplug_decode_mpc_la_DEPENDENCIES = $(MPCDEC_DEPS)
xineplug_decode_mpc_la_LIBADD = $(XINE_LIB) $(MPCDEC_LIBS)
xineplug_decode_mpc_la_LDFLAGS = $(xineplug_ldflags)
