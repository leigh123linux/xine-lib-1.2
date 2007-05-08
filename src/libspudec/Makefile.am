include $(top_srcdir)/misc/Makefile.common

AM_CFLAGS = $(DEFAULT_OCFLAGS) $(VISIBILITY_FLAG)
AM_CPPFLAGS = -I$(top_srcdir)/src/input/libdvdnav
AM_LDFLAGS = $(xineplug_ldflags)

xineplug_LTLIBRARIES = xineplug_decode_spu.la

if HAVE_DVDNAV

xineplug_decode_spu_la_SOURCES = \
	spu.c \
	xine_spu_decoder.c
xineplug_decode_spu_la_LIBADD = $(XINE_LIB) $(DVDNAV_LIBS) $(PTHREAD_LIBS)

else

xineplug_decode_spu_la_SOURCES = \
	nav_read.c \
	spu.c \
	xine_spu_decoder.c
xineplug_decode_spu_la_LIBADD = $(XINE_LIB) $(PTHREAD_LIBS)

endif

xineplug_decode_spu_la_CFLAGS = $(AM_CFLAGS) $(DVDNAV_CFLAGS)

noinst_HEADERS = spu.h
