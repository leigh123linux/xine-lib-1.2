include $(top_srcdir)/misc/Makefile.common

AM_CFLAGS  = $(DEFAULT_OCFLAGS) $(LIBCDIO_CFLAGS) $(LIBVCD_CFLAGS)
AM_LDFLAGS = $(xineplug_ldflags)

SUBDIRS =
if !WITH_EXTERNAL_VCDLIBS
SUBDIRS += libcdio libvcd
endif

noinst_HEADERS = vcdio.h vcdplayer.h xine-extra.h

xineplug_LTLIBRARIES = xineplug_inp_vcd.la

xineplug_inp_vcd_la_SOURCES = xineplug_inp_vcd.c vcdplayer.c vcdio.c xine-extra.c

if WITH_EXTERNAL_VCDLIBS
xineplug_inp_vcd_la_LIBADD = $(XINE_LIB) $(LIBVCDINFO_LIBS)
else
xineplug_inp_vcd_la_LIBADD = $(XINE_LIB) $(LIBVCD_LIBS) $(LIBVCDINFO_LIBS) -lm
endif
