AUTOMAKE_OPTIONS = subdir-objects

include $(top_srcdir)/misc/Makefile.common
include $(top_builddir)/misc/Makefile.plugins

noinst_LTLIBRARIES =
EXTRA_DIST =

AM_CFLAGS  = $(DEFAULT_OCFLAGS) $(VISIBILITY_FLAG)
AM_LDFLAGS = $(xineplug_ldflags)

EXTRA_DIST += visualizations/fooviz.c

xinepost_LTLIBRARIES = \
	xineplug_post_audio_filters.la \
	xineplug_post_goom.la \
	xineplug_post_mosaico.la \
	xineplug_post_planar.la \
	xineplug_post_switch.la \
	xineplug_post_tvtime.la \
	xineplug_post_visualizations.la

xineplug_post_visualizations_la_SOURCES = \
	visualizations/fft.h \
	visualizations/fft.c \
	visualizations/fftgraph.c \
	visualizations/fftscope.c \
	visualizations/oscope.c \
	visualizations/tdaudioanalyzer.c \
	visualizations/visualizations.c \
	visualizations/visualizations.h
xineplug_post_visualizations_la_LIBADD = $(XINE_LIB) $(PTHREAD_LIBS) $(MVEC_LIB) -lm

xineplug_post_audio_filters_la_SOURCES = \
	audio/audio_filters.c \
	audio/audio_filters.h \
	audio/dsp.h \
	audio/filter.c \
	audio/filter.h \
	audio/stretch.c \
	audio/upmix.c \
	audio/upmix_mono.c \
	audio/volnorm.c \
	audio/window.c \
	audio/window.h
xineplug_post_audio_filters_la_LIBADD = $(XINE_LIB) $(PTHREAD_LIBS) $(LTLIBINTL) $(MVEC_LIB) -lm

xineplug_post_mosaico_la_SOURCES = mosaico/mosaico.c
xineplug_post_mosaico_la_LIBADD = $(XINE_LIB) $(PTHREAD_LIBS) $(LTLIBINTL)

xineplug_post_switch_la_SOURCES = mosaico/switch.c
xineplug_post_switch_la_LIBADD = $(XINE_LIB) $(PTHREAD_LIBS) $(LTLIBINTL)

#
# deinterlace
#

EXTRA_DIST += \
	deinterlace/plugins/greedy2frame_template.c \
	deinterlace/plugins/greedy2frame_template_sse2.c \
	deinterlace/plugins/greedyh.asm \
	deinterlace/plugins/tomsmocomp/SearchLoop0A.inc \
	deinterlace/plugins/tomsmocomp/SearchLoopBottom.inc \
	deinterlace/plugins/tomsmocomp/SearchLoopEdgeA.inc \
	deinterlace/plugins/tomsmocomp/SearchLoopEdgeA8.inc \
	deinterlace/plugins/tomsmocomp/SearchLoopOddA.inc \
	deinterlace/plugins/tomsmocomp/SearchLoopOddA2.inc \
	deinterlace/plugins/tomsmocomp/SearchLoopOddA6.inc \
	deinterlace/plugins/tomsmocomp/SearchLoopOddAH.inc \
	deinterlace/plugins/tomsmocomp/SearchLoopOddAH2.inc \
	deinterlace/plugins/tomsmocomp/SearchLoopTop.inc \
	deinterlace/plugins/tomsmocomp/SearchLoopVA.inc \
	deinterlace/plugins/tomsmocomp/SearchLoopVAH.inc \
	deinterlace/plugins/tomsmocomp/StrangeBob.inc \
	deinterlace/plugins/tomsmocomp/TomsMoCompAll.inc \
	deinterlace/plugins/tomsmocomp/TomsMoCompAll2.inc \
	deinterlace/plugins/tomsmocomp/WierdBob.inc \
	deinterlace/plugins/tomsmocomp/tomsmocompmacros.h \
	deinterlace/plugins/x86-64_macros.inc

if DEBUG_BUILD
debug_sources   = deinterlace/plugins/greedy2frame.c
nodebug_sources =
else
debug_sources   =
nodebug_sources = deinterlace/plugins/greedy2frame.c
endif

# per-object CFLAGS -- drop optimization on kdetv_greedyh.c so that gcc
# doesn't run out of general registers trying to compile it.

noinst_LTLIBRARIES += \
	libdeinterlaceplugins_O1.la \
	libdeinterlaceplugins.la

libdeinterlaceplugins_O1_la_SOURCES = \
	deinterlace/plugins/kdetv_greedyh.c \
	$(debug_sources)
libdeinterlaceplugins_O1_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(top_srcdir)/src/post/deinterlace
libdeinterlaceplugins_O1_la_CFLAGS = $(O1_CFLAGS) $(VISIBILITY_FLAG)
libdeinterlaceplugins_O1_la_LDFLAGS =

libdeinterlaceplugins_la_SOURCES = \
	deinterlace/plugins/double.c \
	deinterlace/plugins/greedy.c \
	deinterlace/plugins/greedyhmacros.h \
	deinterlace/plugins/kdetv_tomsmocomp.c \
	deinterlace/plugins/linear.c \
	deinterlace/plugins/linearblend.c \
	deinterlace/plugins/plugins.h \
	deinterlace/plugins/vfir.c \
	deinterlace/plugins/weave.c \
	deinterlace/plugins/scalerbob.c \
	$(nodebug_sources)
libdeinterlaceplugins_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(top_srcdir)/src/post/deinterlace
libdeinterlaceplugins_la_LIBADD = $(XINE_LIB) libdeinterlaceplugins_O1.la
libdeinterlaceplugins_la_LDFLAGS =

#

xineplug_post_tvtime_la_SOURCES = \
	deinterlace/deinterlace.c \
	deinterlace/deinterlace.h \
	deinterlace/pulldown.c \
	deinterlace/pulldown.h \
	deinterlace/speedtools.h \
	deinterlace/speedy.c \
	deinterlace/speedy.h \
	deinterlace/tvtime.c \
	deinterlace/tvtime.h \
	deinterlace/xine_plugin.c
xineplug_post_tvtime_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(top_srcdir)/src/post/deinterlace
xineplug_post_tvtime_la_LIBADD = $(XINE_LIB) $(LTLIBINTL) $(PTHREAD_LIBS) libdeinterlaceplugins.la
xineplug_post_tvtime_la_LDFLAGS = $(AM_LDFLAGS) $(IMPURE_TEXT_LDFLAGS)

#
# planar
#

if ENABLE_POSTPROC
pp_module_sources = planar/pp.c
pp_module_libs = $(POSTPROC_LIBS)
pp_module_deps = $(POSTPROC_DEPS)
pp_module_cflags = $(FFMPEG_CFLAGS) $(POSTPROC_CFLAGS)
endif

if ARCH_X86
PLANAR_X86_LIB = libpost_planar_x86.la
libpost_planar_x86_la_SOURCES = planar/x86/noise.c planar/x86/noise.h
libpost_planar_x86_la_CFLAGS  = $(O1_CFLAGS) -fomit-frame-pointer $(VISIBILITY_FLAG)
libpost_planar_x86_la_LDFLAGS =
noinst_LTLIBRARIES += $(PLANAR_X86_LIB)
endif

#

xineplug_post_planar_la_SOURCES = \
	planar/boxblur.c \
	planar/denoise3d.c \
	planar/eq.c \
	planar/eq2.c \
	planar/expand.c \
	planar/fill.c \
	planar/invert.c \
	planar/noise.c \
	planar/planar.c \
	planar/planar.h \
	planar/unsharp.c \
	$(pp_module_sources)
xineplug_post_planar_la_LIBADD  = $(XINE_LIB) $(pp_module_libs) $(MVEC_LIB) -lm $(PTHREAD_LIBS) $(LTLIBINTL) $(PLANAR_X86_LIB)
xineplug_post_planar_la_DEPS = $(pp_module_deps)
xineplug_post_planar_la_CFLAGS  = $(AM_CFLAGS) $(pp_module_cflags)
xineplug_post_planar_la_LDFLAGS = $(AM_LDFLAGS) $(IMPURE_TEXT_LDFLAGS)

#
# goom
#

EXTRA_DIST += goom/diff_against_release.patch

# following files are included to other files.
# TODO: rename to .h ?
EXTRA_DIST += goom/gfontrle.c goom/mathtools.c

# following files are currently unused.
EXTRA_DIST += \
	goom/goomsl.c goom/goomsl.h goom/goomsl_hash.c goom/goomsl_hash.h \
	goom/goomsl_heap.c ggoom/oomsl_heap.h goom/goomsl_private.h \
	goom/goomsl_lex.c goom/goomsl_yacc.c goom/goomsl_yacc.h goom/default_scripts.h

noinst_LTLIBRARIES += libpost_goom_asm.la
libpost_goom_asm_la_SOURCES = goom/xmmx.c
if DEBUG_BUILD
libpost_goom_asm_la_CFLAGS  = $(O1_CFLAGS) $(VISIBILITY_FLAG)
endif
libpost_goom_asm_la_LDFLAGS =

xineplug_post_goom_la_SOURCES = \
	goom/config_param.c \
	goom/convolve_fx.c \
	goom/cpu_info.c \
	goom/cpu_info.h \
	goom/drawmethods.c \
	goom/drawmethods.h \
	goom/filters.c \
	goom/flying_stars_fx.c \
	goom/gfontlib.c \
	goom/gfontlib.h \
	goom/goom.h \
	goom/goom_config.h \
	goom/goom_config_param.h \
	goom/goom_core.c \
	goom/goom_filters.h \
	goom/goom_fx.h \
	goom/goom_graphic.h \
	goom/goom_plugin_info.h \
	goom/goom_tools.c \
	goom/goom_tools.h \
	goom/goom_typedefs.h \
	goom/goom_visual_fx.h \
	goom/graphic.c \
	goom/ifs.c \
	goom/ifs.h \
	goom/lines.c \
	goom/lines.h \
	goom/mathtools.h \
	goom/mmx.c \
	goom/mmx.h \
	goom/motif_goom1.h \
	goom/motif_goom2.h \
	goom/plugin_info.c \
	goom/ppc_drawings.h \
	goom/ppc_zoom_ultimate.h \
	goom/sound_tester.c \
	goom/sound_tester.h \
	goom/surf3d.c \
	goom/surf3d.h \
	goom/tentacle3d.c \
	goom/tentacle3d.h \
	goom/v3d.c \
	goom/v3d.h \
	goom/xine_goom.c
xineplug_post_goom_la_LIBADD = $(XINE_LIB) $(GOOM_LIBS) $(PTHREAD_LIBS) $(LTLIBINTL) -lm libpost_goom_asm.la
