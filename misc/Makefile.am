include $(top_srcdir)/misc/Makefile.common

SUBDIRS = fonts

EXTRA_DIST = build_rpms.sh \
	cdda_server.c \
	SlackBuild.in \
	SlackBuild \
	vga.xinefont.gz \
	xine-config \
	xine-fontconv.c \
	xine-lib.spec.in \
	xine-lib.spec \
	libxine.pc.in \
	libdvdcss-1.2.6-network.patch \
	Makefile.common

bin_SCRIPTS = xine-config
bin_PROGRAMS = xine-list-@XINE_SERIES@

pkgconfig_DATA=libxine.pc

xine_list_@XINE_SERIES@_SOURCES = xine-list.c
xine_list_@XINE_SERIES@_LDADD = $(XINE_LIB)

xine-fontconv:	xine-fontconv.c
	$(CC) -W -Wall -g -o xine-fontconv xine-fontconv.c -lz `freetype-config --cflags` `freetype-config --libs`

cdda_server: cdda_server.c
	$(CC) -W -Wall -g -o cdda_server cdda_server.c -ldl
