include $(top_srcdir)/misc/Makefile.common

SUBDIRS = fonts

EXTRA_DIST = build_rpms.sh \
	cdda_server.c \
	SlackBuild.in \
	SlackBuild \
	vga.xinefont.gz \
	xine-fontconv.c \
	xine-lib.spec.in \
	xine-lib.spec \
	libxine.pc.in \
	libdvdcss-1.2.6-network.patch \
	Makefile.common

bin_SCRIPTS = xine-config

pkgconfigdir=$(libdir)/pkgconfig
pkgconfig_DATA=libxine.pc

xine-fontconv:	xine-fontconv.c
	$(CC) -W -Wall -g -o xine-fontconv xine-fontconv.c -lz `freetype-config --cflags` `freetype-config --libs`

cdda_server: cdda_server.c
	$(CC) -W -Wall -g -o cdda_server cdda_server.c -ldl
