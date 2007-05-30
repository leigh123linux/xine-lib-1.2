include $(top_srcdir)/misc/Makefile.common

AM_CFLAGS = -I$(srcdir)/wine -DWIN32_PATH=\"@w32_path@\" $(VISIBILITY_FLAG)

SUBDIRS = wine DirectShow dmo qtx

if HAVE_W32DLL
w32dll_codec = xineplug_decode_w32dll.la
qt_codec = xineplug_decode_qt.la
endif

xineplug_LTLIBRARIES = $(w32dll_codec) $(qt_codec)

EXTRA_DIST = common.c

xineplug_decode_w32dll_la_SOURCES = w32codec.c
xineplug_decode_w32dll_la_CFLAGS = $(AM_CFLAGS) $(XDG_BASEDIR_CFLAGS)
xineplug_decode_w32dll_la_LDFLAGS = -avoid-version -module
xineplug_decode_w32dll_la_DEPS = $(XDG_BASEDIR_DEPS)
xineplug_decode_w32dll_la_LIBADD = \
	$(top_builddir)/src/libw32dll/wine/libwine.la \
	$(XINE_LIB) \
	$(PTHREAD_LIBS) \
	-lm \
	$(top_builddir)/src/libw32dll/DirectShow/libds_filter.la \
	$(top_builddir)/src/libw32dll/dmo/libdmo_filter.la \
	@KSTAT_LIBS@ \
	$(XDG_BASEDIR_LIBS)

xineplug_decode_qt_la_SOURCES = qt_decoder.c
xineplug_decode_qt_la_LDFLAGS = -avoid-version -module
xineplug_decode_qt_la_LIBADD = \
	$(top_builddir)/src/libw32dll/wine/libwine.la \
	$(XINE_LIB) \
	$(PTHREAD_LIBS) \
	-lm \
	@KSTAT_LIBS@

noinst_HEADERS = libwin32.h w32codec.h
