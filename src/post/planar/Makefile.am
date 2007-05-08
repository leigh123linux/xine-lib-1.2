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
AM_CFLAGS = $(DEFAULT_OCFLAGS) $(VISIBILITY_FLAG) $(ff_cflags) -fomit-frame-pointer
AM_LDFLAGS = $(xineplug_ldflags) $(IMPURE_TEXT_LDFLAGS)

# Avoid errors with -O0
#xineplug_post_planar_la-eq.o xineplug_post_planar_la-eq.lo: CFLAGS=`echo @CFLAGS@ | sed -e 's/-O0\?\s/-Os /g'`
#xineplug_post_planar_la-eq2.o xineplug_post_planar_la-eq2.lo: CFLAGS=`echo @CFLAGS@ | sed -e 's/-O0\?\s/-Os /g'`
#xineplug_post_planar_la-noise.o xineplug_post_planar_la-noise.lo: CFLAGS=`echo @CFLAGS@ | sed -e 's/-O0\?\s/-Os /g'`

xinepost_LTLIBRARIES = xineplug_post_planar.la

xineplug_post_planar_la_SOURCES = planar.c invert.c expand.c fill.c boxblur.c \
                                  denoise3d.c eq.c eq2.c unsharp.c pp.c noise.c
xineplug_post_planar_la_LIBADD = $(XINE_LIB) $(postproc_lib) -lm $(PTHREAD_LIBS)

$(POSTPROC_INT_LIB):
	cd $(top_builddir)/src/libffmpeg/libavcodec/libpostproc && $(MAKE) libpostprocess.la
