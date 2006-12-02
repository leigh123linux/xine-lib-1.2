include $(top_srcdir)/misc/Makefile.common

AM_CFLAGS = -I$(srcdir)/wine -DWIN32_PATH=\"@w32_path@\" $(VISIBILITY_FLAG)

SUBDIRS = wine DirectShow dmo qtx

if HAVE_W32DLL
w32dll_codec = xineplug_decode_w32dll.la
qt_codec = xineplug_decode_qt.la
endif

libdir = $(XINE_PLUGINDIR)

lib_LTLIBRARIES = $(w32dll_codec) $(qt_codec)

EXTRA_DIST = common.c

xineplug_decode_w32dll_la_SOURCES = w32codec.c
xineplug_decode_w32dll_la_LDFLAGS = -avoid-version -module \
	@IMPURE_TEXT_LDFLAGS@
xineplug_decode_w32dll_la_LIBADD = \
	$(top_builddir)/src/libw32dll/wine/libwine.la \
	$(XINE_LIB) \
	$(top_builddir)/src/libw32dll/DirectShow/libds_filter.la \
	$(top_builddir)/src/libw32dll/dmo/libdmo_filter.la \
	@KSTAT_LIBS@

xineplug_decode_qt_la_SOURCES = qt_decoder.c
xineplug_decode_qt_la_LDFLAGS = -avoid-version -module \
	IMPURE_TEXT_LDFLAGS@
xineplug_decode_qt_la_LIBADD = \
	$(top_builddir)/src/libw32dll/wine/libwine.la \
	$(XINE_LIB) \
	@KSTAT_LIBS@

noinst_HEADERS = libwin32.h w32codec.h
