include $(top_srcdir)/misc/Makefile.common

SUBDIRS = fonts

EXTRA_DIST = build_rpms.sh \
	SlackBuild.in \
	SlackBuild \
	vga.xinefont.gz \
	xine-lib.spec.in \
	xine-lib.spec \
	libxine.pc.in \
	libdvdcss-1.2.6-network.patch \
	Makefile.common

dist_bin_SCRIPTS = xine-config

pkgconfigdir=$(libdir)/pkgconfig
pkgconfig_DATA=libxine.pc

EXTRA_PROGRAMS = xine-fontconv cdda_server

xine_fontconv_SOURCES = xine-fontconv.c
xine_fontconv_CFLAGS = $(FT2_CFLAGS)
xine_fontconv_LDFLAGS = $(GCSECTIONS)
xine_fontconv_LDADD = -lz $(FT2_LIBS)

cdda_server_SOURCES = cdda_server.c
cdda_server_LDFLAGS = $(GCSECTIONS)
cdda_server_LDADD = $(DYNAMIC_LD_LIBS)
