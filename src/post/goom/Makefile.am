include $(top_builddir)/misc/Makefile.plugins
include $(top_srcdir)/misc/Makefile.common

AM_CFLAGS = $(DEFAULT_OCFLAGS) $(VISIBILITY_FLAG)
AM_LDFLAGS = $(xineplug_ldflags)

EXTRA_DIST = mmx.c xmmx.c ppc_drawings.s ppc_zoom_ultimate.s diff_against_release.patch \
	gfontrle.c mathtools.c

# following files are currently unused.
EXTRA_DIST += \
	goomsl.c goomsl.h goomsl_hash.c goomsl_hash.h goomsl_heap.c goomsl_heap.h \
	goomsl_private.h goomsl_lex.c goomsl_yacc.c goomsl_yacc.h default_scripts.h

noinst_LTLIBRARIES = libpost_goom_asm.la
libpost_goom_asm_la_SOURCES = xmmx.c
if DEBUG_BUILD
libpost_goom_asm_la_CFLAGS  = $(O1_CFLAGS) $(VISIBILITY_FLAG)
endif
libpost_goom_asm_la_LDFLAGS =

xinepost_LTLIBRARIES = xineplug_post_goom.la
xineplug_post_goom_la_SOURCES = \
	config_param.c \
	convolve_fx.c \
	cpu_info.c \
	cpu_info.h \
	drawmethods.c \
	drawmethods.h \
	filters.c \
	flying_stars_fx.c \
	gfontlib.c \
	gfontlib.h \
	goom.h \
	goom_config.h \
	goom_config_param.h \
	goom_core.c \
	goom_filters.h \
	goom_fx.h \
	goom_graphic.h \
	goom_plugin_info.h \
	goom_tools.c \
	goom_tools.h \
	goom_typedefs.h \
	goom_visual_fx.h \
	graphic.c \
	ifs.c \
	ifs.h \
	lines.c \
	lines.h \
	mathtools.h \
	mmx.c \
	mmx.h \
	motif_goom1.h \
	motif_goom2.h \
	plugin_info.c \
	ppc_drawings.h \
	ppc_zoom_ultimate.h \
	sound_tester.c \
	sound_tester.h \
	surf3d.c \
	surf3d.h \
	tentacle3d.c \
	tentacle3d.h \
	v3d.c \
	v3d.h \
	xine_goom.c
xineplug_post_goom_la_LIBADD = $(XINE_LIB) $(GOOM_LIBS) $(PTHREAD_LIBS) $(LTLIBINTL) -lm libpost_goom_asm.la
