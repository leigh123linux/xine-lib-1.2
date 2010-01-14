include $(top_srcdir)/misc/Makefile.common

# -fomit-frame-pointer is always needed. it might cause debug to not
# work, but at least it compiles.
AM_CFLAGS = -fomit-frame-pointer -fno-strict-aliasing
# CFLAGS is here to filter out -funroll-loops because it causes bad
# behavior of libavcodec
CFLAGS := `echo @CFLAGS@ | sed -e 's/-funroll-loops//g'`
AM_CPPFLAGS = $(LIBFFMPEG_CPPFLAGS) -I$(top_srcdir)/src/libffmpeg/libavutil -I$(top_srcdir)/src/libffmpeg -I$(top_builddir)/src/libffmpeg

# Avoid "can't find register" failures with -O1 and higher
dsputil_mmx.o dsputil_mmx.lo: CFLAGS=$(shell echo @CFLAGS@ | sed -e 's/-funroll-loops//g; s/$$/ -Os/')

# Avoid errors on (at least) amd64 with -O0
fdct_mmx.o fdct_mmx.lo: CFLAGS=`echo @CFLAGS@ | sed -e 's/^/-Os /; s/-O0\?\s/-Os /g'`

# Avoid errors with -O0
mpegvideo_mmx.o mpegvideo_mmx.lo: CFLAGS=`echo @CFLAGS@ | sed -e 's/-O0\?\s/-Os /g'`

ASFLAGS =

noinst_LTLIBRARIES = libavcodec_mmx.la

libavcodec_mmx_src = \
	cputest.c \
	dsputil_mmx.c \
	fdct_mmx.c \
	fft_3dn.c \
	fft_3dn2.c \
	fft_sse.c \
	idct_mmx.c \
	idct_mmx_xvid.c \
	motion_est_mmx.c \
	mpegvideo_mmx.c \
	simple_idct_mmx.c \
	vp3dsp_mmx.c \
	vp3dsp_sse2.c

libavcodec_mmx_dummy = libavcodec_mmx_dummy.c

EXTRA_DIST = \
	$(libavcodec_mmx_dummy) \
	$(libavcodec_mmx_src) \
	dsputil_h264_template_mmx.c \
	h264dsp_mmx.c \
	mpegvideo_mmx_template.c

if HAVE_MMX
mmx_modules = $(libavcodec_mmx_src)
endif

libavcodec_mmx_la_SOURCES = $(mmx_modules) $(libavcodec_mmx_dummy)

noinst_HEADERS = dsputil_mmx_avg.h dsputil_mmx_rnd.h mathops.h mmx.h
