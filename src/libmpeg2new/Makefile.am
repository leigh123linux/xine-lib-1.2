include $(top_srcdir)/misc/Makefile.common

SUBDIRS = libmpeg2

xineplug_LTLIBRARIES = xineplug_decode_mpeg2new.la

xineplug_decode_mpeg2new_la_SOURCES = \
	xine_mpeg2new_decoder.c

xineplug_decode_mpeg2new_la_CFLAGS = $(VISIBILITY_FLAG) $(LIBMPEG2_CFLAGS)
xineplug_decode_mpeg2new_la_LIBADD = $(XINE_LIB) libmpeg2/libmpeg2.la 
xineplug_decode_mpeg2new_la_LDFLAGS = $(xineplug_ldflags)

