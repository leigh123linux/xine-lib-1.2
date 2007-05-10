include $(top_srcdir)/misc/Makefile.common

AM_CFLAGS = $(DEFAULT_OCFLAGS) $(VISIBILITY_FLAG)
AM_CPPFLAGS = -I$(srcdir)/../librtsp

noinst_LTLIBRARIES = libreal.la

libreal_la_SOURCES = \
  real.c \
  asmrp.c \
  rmff.c \
  sdpplin.c

noinst_HEADERS = \
  real.h \
  asmrp.h \
  rmff.h \
  sdpplin.h
