include $(top_srcdir)/misc/Makefile.common

POSTPROC_INT_LIB = $(top_builddir)/src/libffmpeg/libavcodec/libpostproc/libpostprocess.la

if HAVE_FFMPEG
postproc_lib = $(FFMPEG_POSTPROC_LIBS)
ff_cflags = $(FFMPEG_POSTPROC_CFLAGS)
else
ff_cflags = -I$(top_srcdir)/contrib/ffmpeg/libpostproc
postproc_lib = $(top_builddir)/contrib/ffmpeg/libpostproc/libpostproc.a

$(top_builddir)/contrib/ffmpeg/libpostproc/libpostproc.a:
	$(MAKE) -C $(top_builddir)/contrib/ffmpeg/ -f makefile.xine libpostproc/libpostproc.a
endif

# -fomit-frame-pointer is always needed. it might cause debug to not
# work, but at least it compiles.
AM_CFLAGS = $(ff_cflags) -fomit-frame-pointer

xinepost_LTLIBRARIES = xineplug_post_planar.la

xineplug_post_planar_la_SOURCES = planar.c invert.c expand.c fill.c boxblur.c \
                                  denoise3d.c eq.c eq2.c unsharp.c pp.c noise.c
xineplug_post_planar_la_LIBADD = $(XINE_LIB) $(postproc_lib) -lm $(PTHREAD_LIBS)
xineplug_post_planar_la_LDFLAGS = -avoid-version -module \
	@IMPURE_TEXT_LDFLAGS@
xineplug_post_planar_la_CFLAGS = $(VISIBILITY_FLAG) $(AM_CFLAGS)

$(POSTPROC_INT_LIB):
	cd $(top_builddir)/src/libffmpeg/libavcodec/libpostproc && $(MAKE) libpostprocess.la
