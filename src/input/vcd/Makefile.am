include $(top_srcdir)/misc/Makefile.common

AM_CFLAGS = $(DEFAULT_OCFLAGS) $(LIBCDIO_CFLAGS)
AM_LDFLAGS = $(xineplug_ldflags)

SUBDIRS = libcdio libvcd

vcd_SRCS = xineplug_inp_vcd.c vcdplayer.c vcdio.c xine-extra.c

EXTRA_DIST = $(vcd_SRCS)

if ENABLE_VCD
xineplug_LTLIBRARIES = xineplug_inp_vcd.la

AM_CFLAGS += $(LIBCDIO_CFLAGS) $(LIBVCD_CFLAGS)

xineplug_inp_vcd_la_SOURCES = $(vcd_SRCS)
if HAVE_VCDNAV
xineplug_inp_vcd_la_LIBADD = $(XINE_LIB) $(LIBVCDINFO_LIBS)
else
xineplug_inp_vcd_la_LIBADD = $(XINE_LIB) $(LIBVCD_LIBS) $(LIBVCDINFO_LIBS) -lm
endif

endif

noinst_HEADERS = vcdio.h vcdplayer.h xine-extra.h
