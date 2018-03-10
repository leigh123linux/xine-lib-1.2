AUTOMAKE_OPTIONS = subdir-objects

include $(top_builddir)/misc/Makefile.plugins
include $(top_srcdir)/misc/Makefile.common

AM_CFLAGS   = $(DEFAULT_OCFLAGS) $(VISIBILITY_FLAG)
AM_CPPFLAGS +=
AM_LDFLAGS  = $(xineplug_ldflags)

if ENABLE_POSTPROC
pp_module_sources = pp.c
pp_module_libs = $(POSTPROC_LIBS)
pp_module_deps = $(POSTPROC_DEPS)
pp_module_cflags = $(FFMPEG_CFLAGS) $(POSTPROC_CFLAGS)
endif

if ARCH_X86
noinst_LTLIBRARIES = libpost_planar_asm.la
libpost_planar_asm_la_SOURCES = x86/noise.c x86/noise.h
libpost_planar_asm_la_CFLAGS  = $(O1_CFLAGS) -fomit-frame-pointer $(VISIBILITY_FLAG)
libpost_planar_asm_la_LDFLAGS =
endif

xinepost_LTLIBRARIES = xineplug_post_planar.la
xineplug_post_planar_la_SOURCES = \
	boxblur.c \
	denoise3d.c \
	eq.c \
	eq2.c \
	expand.c \
	fill.c \
	invert.c \
	noise.c \
	planar.c \
	planar.h \
	unsharp.c \
	$(pp_module_sources)
xineplug_post_planar_la_LIBADD  = $(XINE_LIB) $(pp_module_libs) -lm $(PTHREAD_LIBS) $(LTLIBINTL) $(noinst_LTLIBRARIES)
xineplug_post_planar_la_DEPS = $(pp_module_deps)
xineplug_post_planar_la_CFLAGS  = $(AM_CFLAGS) $(pp_module_cflags)
xineplug_post_planar_la_LDFLAGS = $(AM_LDFLAGS) $(IMPURE_TEXT_LDFLAGS)
