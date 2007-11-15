include $(top_srcdir)/misc/Makefile.common

EXTRA_DIST = mmx.c xmmx.c ppc_drawings.s ppc_zoom_ultimate.s diff_against_release.patch \
	gfontrle.c mathtools.c

## -fomit-frame-pointer segfaults here
## Use -O2 if -Os is stripped or x86 does not build
#CFLAGS = `echo @CFLAGS@ | sed -e 's/-fomit-frame-pointer//g;s/-Os/-O2/g'`
CFLAGS = `echo @CFLAGS@ | sed -e 's/-Os/-O2/g'`

# Avoid errors with -O0
xineplug_post_goom_la-xmmx.o xineplug_post_goom_la-xmmx.lo: CFLAGS=`echo @CFLAGS@ | sed -e 's/-O0\?\s/-Os /g'`

xinepost_LTLIBRARIES = xineplug_post_goom.la

## doesn't work
#if PPC_ARCH
#extra_files = ppc_drawings.s ppc_zoom_ultimate.s
#AM_CPPFLAGS = -DCPU_POWERPC
#endif

if HAVE_MMX
extra_files = mmx.c xmmx.c
endif

xineplug_post_goom_la_SOURCES = $(extra_files) xine_goom.c \
	config_param.c convolve_fx.c cpu_info.c drawmethods.c filters.c flying_stars_fx.c \
	gfontlib.c goom_core.c goom_tools.c goomsl.c goomsl_hash.c goomsl_heap.c \
	goomsl_lex.c goomsl_yacc.c graphic.c ifs.c lines.c \
	plugin_info.c sound_tester.c surf3d.c tentacle3d.c v3d.c
xineplug_post_goom_la_LIBADD = $(XINE_LIB) $(GOOM_LIBS) $(PTHREAD_LIBS) $(LTLIBINTL) -lm
xineplug_post_goom_la_CFLAGS = $(VISIBILITY_FLAG)
xineplug_post_goom_la_LDFLAGS = -avoid-version -module

noinst_HEADERS = cpu_info.h default_scripts.h drawmethods.h gfontlib.h goom.h \
	goom_config.h goom_config_param.h goom_filters.h goom_fx.h goom_graphic.h \
	goom_plugin_info.h goom_tools.h goom_typedefs.h goom_visual_fx.h goomsl.h goomsl_hash.h \
	goomsl_heap.h goomsl_private.h goomsl_yacc.h ifs.h lines.h mathtools.h mmx.h \
	ppc_drawings.h ppc_zoom_ultimate.h sound_tester.h surf3d.h tentacle3d.h v3d.h \
	motif_goom1.h motif_goom2.h
