include $(top_srcdir)/misc/Makefile.common

AM_CFLAGS   = $(VISIBILITY_FLAG) $(ff_cflags)
AM_CPPFLAGS = 
AM_LDFLAGS  = 

if WITH_EXTERNAL_FFMPEG
postproc_lib = $(FFMPEG_POSTPROC_LIBS)
ff_cflags    = $(FFMPEG_POSTPROC_CFLAGS)
else
AM_CPPFLAGS += -I$(top_srcdir)/contrib/ffmpeg/libpostproc
postproc_lib =  $(top_builddir)/contrib/ffmpeg/libpostproc/libpostproc.a \
                $(top_builddir)/contrib/ffmpeg/libavutil/libavutil.a

$(top_builddir)/contrib/ffmpeg/libpostproc/libpostproc.a:
	$(MAKE) -C $(top_builddir)/contrib ffmpeg/libpostproc/libpostproc.a
$(top_builddir)/contrib/ffmpeg/libavutil/libavutil.a:
	$(MAKE) -C $(top_builddir)/contrib ffmpeg/libavutil/libavutil.a
endif

noinst_LTLIBRARIES = libpost_planar_asm.la
libpost_planar_asm_la_SOURCES = eq.c eq2.c noise.c
libpost_planar_asm_la_CFLAGS  = $(O1_CFLAGS) -fomit-frame-pointer $(AM_CFLAGS)

xinepost_LTLIBRARIES = xineplug_post_planar.la
xineplug_post_planar_la_SOURCES = planar.c invert.c expand.c fill.c boxblur.c \
                                  denoise3d.c unsharp.c pp.c
xineplug_post_planar_la_LIBADD  = $(XINE_LIB) $(postproc_lib) -lm $(PTHREAD_LIBS) $(LTLIBINTL) $(noinst_LTLIBRARIES)
xineplug_post_planar_la_CFLAGS  = $(DEFAULT_OCFLAGS) $(AM_CFLAGS)
xineplug_post_planar_la_LDFLAGS = $(AM_LDFLAGS) $(xineplug_ldflags) $(IMPURE_TEXT_LDFLAGS)
