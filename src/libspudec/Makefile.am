include $(top_srcdir)/misc/Makefile.common

AM_CFLAGS   = $(DEFAULT_OCFLAGS) $(VISIBILITY_FLAG)
AM_CPPFLAGS = -I$(top_srcdir)/src/input/libdvdnav
AM_LDFLAGS  = $(xineplug_ldflags)

noinst_HEADERS = spu.h

xineplug_LTLIBRARIES = xineplug_decode_spu.la

if HAVE_DVDNAV
external_dvdnav_libs = $(DVDNAV_LIBS)
internal_dvdnav_sources =
else
external_dvdnav_libs =
internal_dvdnav_sources = nav_read.c
endif

xineplug_decode_spu_la_SOURCES = $(internal_dvdnav_sources) spu.c xine_spu_decoder.c
xineplug_decode_spu_la_LIBADD  = $(XINE_LIB) $(external_dvdnav_libs) $(PTHREAD_LIBS)
xineplug_decode_spu_la_CFLAGS  = $(AM_CFLAGS) $(DVDNAV_CFLAGS)
