include $(top_srcdir)/misc/Makefile.common

AM_CFLAGS   = $(DEFAULT_OCFLAGS) $(VISIBILITY_FLAG)
AM_CPPFLAGS = $(ZLIB_CPPFLAGS)
AM_LDFLAGS  = $(xineplug_ldflags)

# this must always be included, even if the current machine has no DXR3...
EXTRA_DIST = ffmpeg_encoder.c

xineplug_LTLIBRARIES = xineplug_decode_ff.la xineplug_decode_dvaudio.la

if ENABLE_DXR3
AM_CFLAGS   += $(X_CFLAGS)
AM_CPPFLAGS += -I$(top_srcdir)/src/dxr3
xineplug_decode_ff_la_SOURCES = ffmpeg_decoder.c ff_audio_decoder.c ff_video_decoder.c \
                                ffmpeg_encoder.c ff_mpeg_parser.c ffmpeg_decoder.h \
				ff_mpeg_parser.h
else
xineplug_decode_ff_la_SOURCES = ffmpeg_decoder.c ff_audio_decoder.c ff_video_decoder.c \
                                ff_mpeg_parser.c ffmpeg_decoder.h ff_mpeg_parser.h
endif

nodist_xineplug_decode_ff_la_SOURCES = ffmpeg_config.h

xineplug_decode_ff_la_CFLAGS = $(AM_CFLAGS) $(FFMPEG_CFLAGS) $(FFMPEG_POSTPROC_CFLAGS)
xineplug_decode_ff_la_LIBADD = $(MLIB_LIBS) $(XINE_LIB) -lm $(ZLIB_LIBS) \
	$(FFMPEG_LIBS) $(FFMPEG_POSTPROC_LIBS) $(PTHREAD_LIBS) $(LTLIBINTL)
xineplug_decode_ff_la_LDFLAGS = $(AM_LDFLAGS) $(IMPURE_TEXT_LDFLAGS)

xineplug_decode_dvaudio_la_SOURCES  = ff_dvaudio_decoder.c
xineplug_decode_dvaudio_la_LIBADD   = $(XINE_LIB) $(LTLIBINTL)
xineplug_decode_dvaudio_la_CPPFLAGS = $(AM_CPPFLAGS) $(FFMPEG_CFLAGS) -I$(top_srcdir)/contrib/ffmpeg/libavcodec
