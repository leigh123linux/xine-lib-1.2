include $(top_srcdir)/misc/Makefile.common

if HAVE_FFMPEG
AM_CFLAGS = $(FFMPEG_CFLAGS) $(FFMPEG_POSTPROC_CFLAGS)
link_ffmpeg = $(FFMPEG_LIBS) $(FFMPEG_POSTPROC_LIBS)

# When using external FFmpeg, consider everything present, as it is
# outside xine's scope to handle it
ffmpeg_config.h:
	echo "#define EXTERNAL_FFMPEG 1" > ffmpeg_config.h

else
ff_cppflags = -I$(top_srcdir)/contrib/ffmpeg/libavutil \
	-I$(top_srcdir)/contrib/ffmpeg/libavcodec \
	-I$(top_srcdir)/contrib/ffmpeg/libpostproc
link_ffmpeg = \
  $(top_builddir)/contrib/ffmpeg/libavcodec/libavcodec.a \
  $(top_builddir)/contrib/ffmpeg/libavutil/libavutil.a \
  $(top_builddir)/contrib/ffmpeg/libpostproc/libpostproc.a

$(top_builddir)/contrib/ffmpeg/libavcodec/libavcodec.a:
	$(MAKE) -C $(top_builddir)/contrib/ ffmpeg/libavcodec/libavcodec.a

$(top_builddir)/contrib/ffmpeg/libavutil/libavutil.a:
	$(MAKE) -C $(top_builddir)/contrib/ ffmpeg/libavutil/libavutil.a

$(top_builddir)/contrib/ffmpeg/libpostproc/libpostproc.a:
	$(MAKE) -C $(top_builddir)/contrib/ ffmpeg/libpostproc/libpostproc.a

$(top_builddir)/contrib/ffmpeg/config.h:
	$(MAKE) -C $(top_builddir)/contrib/ ffmpeg/config.mak

ffmpeg_config.h: $(top_builddir)/contrib/ffmpeg/config.h
	cp $(top_builddir)/contrib/ffmpeg/config.h ffmpeg_config.h

endif

CLEANFILES = ffmpeg_config.h

# this must always be included, even if the current machine has no DXR3...
EXTRA_DIST = ffmpeg_encoder.c

xineplug_LTLIBRARIES = xineplug_decode_ff.la xineplug_decode_dvaudio.la

if HAVE_DXR3
AM_CPPFLAGS = -I$(top_srcdir)/src/dxr3 $(X_CFLAGS) $(ff_cppflags) \
              $(ZLIB_CPPFLAGS)
xineplug_decode_ff_la_SOURCES = ffmpeg_decoder.c ff_audio_decoder.c ff_video_decoder.c \
                                ffmpeg_encoder.c ff_mpeg_parser.c ffmpeg_decoder.h \
				ff_mpeg_parser.h
else
AM_CPPFLAGS = $(ff_cppflags) $(ZLIB_CPPFLAGS)
xineplug_decode_ff_la_SOURCES = ffmpeg_decoder.c ff_audio_decoder.c ff_video_decoder.c \
                                ff_mpeg_parser.c ffmpeg_decoder.h ff_mpeg_parser.h
endif

xineplug_decode_ff_la_CFLAGS = $(VISIBILITY_FLAG) $(AM_CFLAGS)
xineplug_decode_ff_la_LDFLAGS = $(xineplug_ldflags) $(IMPURE_TEXT_LDFLAGS)
xineplug_decode_ff_la_LIBADD = $(MLIB_LIBS) $(XINE_LIB) -lm $(ZLIB_LIBS) \
	$(link_ffmpeg) $(PTHREAD_LIBS)

xineplug_decode_dvaudio_la_CFLAGS = $(VISIBILITY_FLAG) $(AM_CFLAGS)
xineplug_decode_dvaudio_la_LDFLAGS = $(xineplug_ldflags)
xineplug_decode_dvaudio_la_SOURCES = ff_dvaudio_decoder.c
xineplug_decode_dvaudio_la_LIBADD  = $(XINE_LIB)
