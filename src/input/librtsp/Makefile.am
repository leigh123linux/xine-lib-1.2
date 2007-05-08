include $(top_srcdir)/misc/Makefile.common

AM_CFLAGS = $(DEFAULT_OCFLAGS) $(VISIBILITY_FLAG)
AM_CPPFLAGS = -D_LARGEFILE64_SOURCE -I$(srcdir)/../libreal
AM_LDFLAGS = $(xineplug_ldflags)

noinst_LTLIBRARIES = librtsp.la

librtsp_la_SOURCES = \
  rtsp.c \
  rtsp_session.c

noinst_HEADERS = \
  rtsp.h \
  rtsp_session.h
