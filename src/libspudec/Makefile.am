include $(top_builddir)/misc/Makefile.plugins
include $(top_srcdir)/misc/Makefile.common

xineplug_LTLIBRARIES = xineplug_decode_spu.la

if HAVE_DVDNAV

xineplug_decode_spu_la_SOURCES = \
	spu.c \
	xine_spu_decoder.c
xineplug_decode_spu_la_LIBADD = $(XINE_LIB) $(DVDNAV_LIBS) $(PTHREAD_LIBS) $(LTLIBINTL)

else

xineplug_decode_spu_la_SOURCES = \
	nav_read.c \
	spu.c \
	xine_spu_decoder.c
AM_CPPFLAGS = -I$(top_srcdir)/src/input/libdvdnav
xineplug_decode_spu_la_LIBADD = $(XINE_LIB) $(PTHREAD_LIBS) $(LTLIBINTL)

endif

xineplug_decode_spu_la_CFLAGS = $(DVDNAV_CFLAGS) $(VISIBILITY_FLAG)
xineplug_decode_spu_la_LDFLAGS = $(xineplug_ldflags)

noinst_HEADERS = spu.h
