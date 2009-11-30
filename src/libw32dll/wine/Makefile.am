include $(top_srcdir)/misc/Makefile.common

LIBTOOL = $(SHELL) $(top_builddir)/libtool

EXTRA_DIST = stubs.s wrapper.S

noinst_LTLIBRARIES = $(wine_lib)

AM_CFLAGS = $(X_CFLAGS) @W32_NO_OPTIMIZE@ \
	-Wmissing-prototypes -Wimplicit-function-declaration \
	-DWIN32_PATH=\"@w32_path@\" -I$(srcdir)/.. -D__WINE__ \
	-Ddbg_printf=__vprintf -DTRACE=__vprintf \
	-I$(top_srcdir)/src/xine-utils

# CFLAGS is here to filter out -fomit-frame-pointer,
# -finline-functions and -frename-registers because they cause bad
# behavior of wine
CFLAGS = `echo @CFLAGS@ | sed -e 's/-fomit-frame-pointer//g;s/-finline-functions//g;s/-frename-registers//g'`

if HAVE_W32DLL
wine_lib = libwine.la
endif

libwine_la_SOURCES = \
	afl.c \
	driver.c \
	elfdll.c \
	ext.c \
	ldt_keeper.c \
	module.c \
	pe_image.c  \
	pe_resource.c \
	resource.c \
	registry.c \
	vfl.c \
	win32.c \
	stubs.s \
	wrapper.S

noinst_HEADERS = \
	avifmt.h \
	basetsd.h \
	com.h \
	debugtools.h \
	driver.h \
	elfdll.h \
	ext.h \
	heap.h \
	ldt.h \
	ldt_keeper.h \
	loader.h \
	mmreg.h \
	module.h \
	msacm.h \
	msacmdrv.h \
	ntdef.h \
	pe_image.h \
	poppack.h \
	pshpack1.h \
	pshpack2.h \
	pshpack4.h \
	pshpack8.h \
	registry.h \
	resource.h \
	vfw.h \
	win32.h\
	wineacm.h \
	winbase.h \
	windef.h \
	windows.h \
	winerror.h \
	winestring.h \
	winnt.h \
	winreg.h \
	winuser.h \
	wrapper.h

