include $(top_srcdir)/misc/Makefile.common

AM_CFLAGS   = $(DEFAULT_OCFLAGS) $(VISIBILITY_FLAG)
AM_CPPFLAGS = $(ZLIB_CPPFLAGS)
AM_LDFLAGS  = $(xineplug_ldflags)

xineplug_LTLIBRARIES = xineplug_decode_ff.la

xineplug_decode_ff_la_SOURCES = ffmpeg_decoder.c ff_audio_decoder.c ff_video_decoder.c \
                                ff_mpeg_parser.c ffmpeg_decoder.h ff_mpeg_parser.h

nodist_xineplug_decode_ff_la_SOURCES = ffmpeg_config.h

xineplug_decode_ff_la_CFLAGS = $(AM_CFLAGS) $(FFMPEG_CFLAGS) $(FFMPEG_POSTPROC_CFLAGS)
xineplug_decode_ff_la_LIBADD = $(XINE_LIB) $(MLIB_LIBS) -lm $(ZLIB_LIBS) \
	$(FFMPEG_LIBS) $(FFMPEG_POSTPROC_LIBS) $(PTHREAD_LIBS) $(LTLIBINTL)
xineplug_decode_ff_la_LDFLAGS = $(AM_LDFLAGS) $(IMPURE_TEXT_LDFLAGS)
