include $(top_srcdir)/misc/Makefile.common

$(top_builddir)/contrib/libcdio/libcdio.la:
	$(MAKE) -C $(top_builddir)/contrib/libcdio

$(top_builddir)/contrib/libcdio/libiso9660.la:
	$(MAKE) -C $(top_builddir)/contrib/libcdio

$(top_builddir)/contrib/libvcd/libvcd.la:
	$(MAKE) -C $(top_builddir)/contrib/libvcd

$(top_builddir)/contrib/libvcd/libvcdinfo.la:
	$(MAKE) -C $(top_builddir)/contrib/libvcd

AM_CFLAGS  = $(DEFAULT_OCFLAGS) $(VISIBILITY_FLAG)
AM_LDFLAGS = $(xineplug_ldflags)

noinst_HEADERS = vcdio.h vcdplayer.h xine-extra.h

xineplug_LTLIBRARIES = xineplug_inp_vcd.la

xineplug_inp_vcd_la_SOURCES = xineplug_inp_vcd.c vcdplayer.c vcdio.c xine-extra.c
xineplug_inp_vcd_la_LIBADD = $(XINE_LIB) $(LTLIBINTL) $(LIBCDIO_LIBS) $(LIBVCD_LIBS) $(LIBVCDINFO_LIBS) $(LIBISO9660_LIBS) -lm
xineplug_inp_vcd_la_DEPENDENCIES = $(LIBCDIO_DEPS) $(LIBVCD_DEPS) $(LIBVCDINFO_DEPS) $(LIBISO9660_DEPS)
xineplug_inp_vcd_la_CFLAGS  = $(AM_CFLAGS) $(LIBCDIO_CFLAGS) $(LIBVCD_CFLAGS)
