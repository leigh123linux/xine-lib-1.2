include $(top_builddir)/misc/Makefile.plugins
include $(top_srcdir)/misc/Makefile.common

SUBDIRS = libcdio libvcd

vcd_SRCS = xineplug_inp_vcd.c vcdplayer.c vcdio.c xine-extra.c

EXTRA_DIST = $(vcd_SRCS)

if ENABLE_VCD
xineplug_LTLIBRARIES = xineplug_inp_vcd.la

AM_CFLAGS = $(LIBCDIO_CFLAGS) $(LIBVCD_CFLAGS)

xineplug_inp_vcd_la_SOURCES = $(vcd_SRCS)
xineplug_inp_vcd_la_LDFLAGS = $(xineplug_ldflags)
if HAVE_VCDNAV
xineplug_inp_vcd_la_LIBADD = $(XINE_LIB) $(LTLIBINTL) $(LIBVCDINFO_LIBS)
else
xineplug_inp_vcd_la_LIBADD = $(XINE_LIB) $(LTLIBINTL) $(LIBVCD_LIBS) $(LIBVCDINFO_LIBS) -lm
endif

endif

noinst_HEADERS = vcdio.h vcdplayer.h xine-extra.h
