include $(top_srcdir)/misc/Makefile.common

AM_CFLAGS  = $(DEFAULT_OCFLAGS) $(VISIBILITY_FLAG)
AM_LDFLAGS = $(xineplug_ldflags)

xineinclude_HEADERS = input_vdr.h

xineplug_LTLIBRARIES = xineplug_inp_vdr.la
xinepost_LTLIBRARIES = xineplug_post_vdr.la

xineplug_inp_vdr_la_SOURCES = input_vdr.c
xineplug_inp_vdr_la_LIBADD = $(XINE_LIB) $(PTHREAD_LIBS)

xineplug_post_vdr_la_SOURCES = post_vdr.c post_vdr_video.c post_vdr_audio.c post_vdr.h
xineplug_post_vdr_la_LIBADD = $(XINE_LIB)
