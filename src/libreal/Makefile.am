include $(top_srcdir)/misc/Makefile.common

if ENABLE_REAL
xineplug_LTLIBRARIES = xineplug_decode_real.la
endif

xineplug_decode_real_la_SOURCES = xine_real_video_decoder.c real_common.c xine_real_audio_decoder.c
xineplug_decode_real_la_LIBADD = $(XINE_LIB) $(DYNAMIC_LD_LIBS) $(LTLIBINTL)
xineplug_decode_real_la_CFLAGS = $(VISIBILITY_FLAG)
xineplug_decode_real_la_LDFLAGS = $(xineplug_ldflags)

noinst_HEADERS = real_common.h
