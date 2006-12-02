include $(top_srcdir)/misc/Makefile.common

AM_CPPFLAGS = -D_LARGEFILE64_SOURCE -DDVDNAV_COMPILE -DHAVE_DLFCN_H

noinst_LTLIBRARIES = libdvdnav.la

libdvdnav_la_SOURCES = \
	decoder.c \
	dvdnav.c \
	highlight.c \
	navigation.c \
	read_cache.c \
	remap.c \
	searching.c \
	settings.c \
	vm.c \
	vmcmd.c \
	ifo_read.c \
	md5.c \
	nav_print.c \
	nav_read.c \
	dvd_reader.c \
	dvd_input.c \
        dvd_udf.c
libdvdnav_la_LIBADD = $(THREAD_LIBS)
libdvdnav_la_CFLAGS = $(VISIBILITY_FLAG)
libdvdnav_la_LDFLAGS = -avoid-version -module

noinst_HEADERS = \
	decoder.h \
	dvdnav.h \
	dvdnav_events.h \
	dvdnav_internal.h \
	dvdread_internal.h \
	remap.h \
	vm.h \
	vmcmd.h \
	read_cache.h \
	dvd_types.h \
	ifo_read.h \
	ifo_types.h \
	md5.h \
	nav_print.h \
	nav_read.h \
	nav_types.h \
	dvd_reader.h \
	dvd_input.h \
	dvd_udf.h \
	bswap.h 
