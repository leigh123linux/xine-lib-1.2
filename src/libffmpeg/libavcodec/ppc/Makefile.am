include $(top_srcdir)/misc/Makefile.common

AM_CPPFLAGS = $(LIBFFMPEG_CPPFLAGS)
AM_CFLAGS = -fno-strict-aliasing
# CFLAGS is here to filter out -funroll-loops because it causes bad
# behavior of libavcodec
CFLAGS = `echo @CFLAGS@ | sed -e 's/-funroll-loops//g'`

ASFLAGS =

noinst_LTLIBRARIES = libavcodec_ppc.la

libavcodec_ppc_src =  dsputil_altivec.c \
		      dsputil_ppc.c \
		      h264_altivec.c \
		      h264_template_altivec.c \
		      fdct_altivec.c \
		      fft_altivec.c \
		      float_altivec.c \
		      idct_altivec.c \
		      gmc_altivec.c \
		      mpegvideo_altivec.c \
		      mpegvideo_ppc.c \
		      snow_altivec.c \
		      vc1dsp_altivec.c
libavcodec_ppc_dummy = libavcodec_ppc_dummy.c

EXTRA_DIST =  $(libavcodec_ppc_src) $(libavcodec_ppc_dummy)

#if PPC_ARCH
#ppc_modules = $(libavcodec_ppc_src)
#endif

libavcodec_ppc_la_SOURCES = $(ppc_modules) $(libavcodec_ppc_dummy)

noinst_HEADERS = dsputil_altivec.h dsputil_ppc.h gcc_fixes.h mathops.h types_altivec.h
