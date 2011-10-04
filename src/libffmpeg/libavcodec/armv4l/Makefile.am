include $(top_srcdir)/misc/Makefile.common

AM_CFLAGS = -O2 -fno-strict-aliasing
AM_CPPFLAGS = $(LIBFFMPEG_CPPFLAGS)
ASFLAGS =

noinst_LTLIBRARIES = libavcodec_armv4l.la

libavcodec_armv4l_src = dsputil_arm.c jrevdct_arm.S mpegvideo_arm.c simple_idct_arm.S \
                       dsputil_arm_s.S dsputil_iwmmxt.c dsputil_iwmmxt_rnd.h  \
                       mpegvideo_armv5te.c mpegvideo_iwmmxt.c simple_idct_armv5te.S

noinst_HEADERS = mathops.h

libavcodec_armv4l_dummy =  libavcodec_armv4l_dummy.c
EXTRA_DIST =  $(libavcodec_armv4l_src) $(libavcodec_armv4l_dummy)

#if HAVE_ARMV4L
#armv4l_modules = $(libavcodec_armv4l_src)
#endif
armv4l_modules =

libavcodec_armv4l_la_SOURCES = $(armv4l_modules) $(libavcodec_armv4l_dummy)
