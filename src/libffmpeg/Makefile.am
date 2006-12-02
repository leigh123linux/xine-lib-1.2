include $(top_srcdir)/misc/Makefile.common

if HAVE_FFMPEG
AM_CFLAGS = $(FFMPEG_CFLAGS) $(FFMPEG_POSTPROC_CFLAGS)
link_ffmpeg = $(FFMPEG_LIBS) $(FFMPEG_POSTPROC_LIBS)
else
ff_cppflags = -I$(top_srcdir)/contrib/ffmpeg/libavutil \
	-I$(top_srcdir)/contrib/ffmpeg/libavcodec \
	-I$(top_srcdir)/contrib/ffmpeg/libpostproc
link_ffmpeg = \
  $(top_builddir)/contrib/ffmpeg/libavcodec/libavcodec.a \
  $(top_builddir)/contrib/ffmpeg/libavutil/libavutil.a \
  $(top_builddir)/contrib/ffmpeg/libpostproc/libpostproc.a

$(top_builddir)/contrib/ffmpeg/libavcodec/libavcodec.a:
	$(MAKE) -C $(top_builddir)/contrib/ffmpeg/ -f makefile.xine libavcodec/libavcodec.a

$(top_builddir)/contrib/ffmpeg/libavutil/libavutil.a:
	$(MAKE) -C $(top_builddir)/contrib/ffmpeg/ -f makefile.xine libavutil/libavutil.a

$(top_builddir)/contrib/ffmpeg/libpostproc/libpostproc.a:
	$(MAKE) -C $(top_builddir)/contrib/ffmpeg/ -f makefile.xine libpostproc/libpostproc.a
endif

# this must always be included, even if the current machine has no DXR3...
EXTRA_DIST = xine_encoder.c diff_to_ffmpeg_cvs.txt

INTERNAL_DOCS = diff_to_ffmpeg_cvs.txt

libdir = $(XINE_PLUGINDIR)

lib_LTLIBRARIES = xineplug_decode_ff.la xineplug_decode_dvaudio.la

if HAVE_DXR3
AM_CPPFLAGS = -I$(top_srcdir)/src/dxr3 $(X_CFLAGS) $(ff_cppflags) \
              $(ZLIB_CPPFLAGS)
xineplug_decode_ff_la_SOURCES = xine_decoder.c audio_decoder.c video_decoder.c \
                                xine_encoder.c mpeg_parser.c
else
AM_CPPFLAGS = $(ff_cppflags) $(ZLIB_CPPFLAGS)
xineplug_decode_ff_la_SOURCES = xine_decoder.c audio_decoder.c video_decoder.c \
                                mpeg_parser.c
endif

xineplug_decode_ff_la_CFLAGS = $(VISIBILITY_FLAG) $(AM_CFLAGS)
xineplug_decode_ff_la_LDFLAGS = -avoid-version -module @IMPURE_TEXT_LDFLAGS@
xineplug_decode_ff_la_LIBADD = $(MLIB_LIBS) $(XINE_LIB) -lm $(ZLIB_LIBS) \
	$(link_ffmpeg) $(THREAD_LIBS)

xineplug_decode_dvaudio_la_CFLAGS = $(VISIBILITY_FLAG) $(AM_CFLAGS)
xineplug_decode_dvaudio_la_LDFLAGS = -avoid-version -module
xineplug_decode_dvaudio_la_SOURCES = dvaudio_decoder.c
xineplug_decode_dvaudio_la_LIBADD  = $(XINE_LIB)

noinst_HEADERS = xine_decoder.h mpeg_parser.h
