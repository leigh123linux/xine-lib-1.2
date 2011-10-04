include $(top_srcdir)/misc/Makefile.common

SUBDIRS = armv4l i386 mlib alpha ppc sparc libpostproc

# some of ffmpeg's decoders are not used by xine yet
EXTRA_DIST = motion_est_template.c \
	adx.c cljr.c fdctref.c ffv1.c g726.c jpeg_ls.c mdec.c raw.c svq3.c wmv2.c

# we need to compile everything in debug mode, including the encoders,
# otherwise we get unresolved symbols, because some unsatisfied function calls
# are not optimized away with debug optimization
#AM_CFLAGS = `test "$(CFLAGS)" = "$(DEBUG_CFLAGS)" && echo -DCONFIG_ENCODERS` -fno-strict-aliasing
AM_CFLAGS = `test "$(CFLAGS)" = "$(DEBUG_CFLAGS)"` -fno-strict-aliasing
AM_CPPFLAGS = $(ZLIB_CPPFLAGS) $(LIBFFMPEG_CPPFLAGS) \
	-I$(top_srcdir)/src/libffmpeg/libavutil -I$(top_srcdir)/src/libffmpeg \
	-I$(top_builddir)/src/libffmpeg
ASFLAGS =

noinst_LTLIBRARIES = libavcodec.la

libavcodec_la_SOURCES = \
	4xm.c \
	8bps.c \
	aasc.c \
	adpcm.c \
	alac.c \
	asv1.c \
	avs.c \
	bitstream.c \
	cabac.c \
	cavs.c \
	cinepak.c \
	cook.c \
	cscd.c \
	cyuv.c \
	dpcm.c \
	dsputil.c \
	dv.c \
	error_resilience.c \
	eval.c \
	faandct.c \
	flac.c \
	flashsv.c \
	flicvideo.c \
	fraps.c \
	fft.c \
	golomb.c \
	h261.c \
	h263.c \
	h263dec.c \
	h264.c \
	h264idct.c \
	huffyuv.c \
	idcinvideo.c \
	imgconvert.c \
	imgresample.c \
	indeo2.c \
	indeo3.c \
	interplayvideo.c \
	jfdctfst.c \
	jfdctint.c \
	jrevdct.c \
	kmvc.c \
	lcl.c \
	loco.c \
	lzo.c \
	mdct.c \
	mace.c \
	mjpeg.c \
	mmvideo.c \
	motion_est.c \
	mpeg12.c \
	mpegaudiodec.c \
	mpegvideo.c \
	msmpeg4.c \
	msrle.c \
	msvideo1.c \
	nuv.c \
	opt.c \
	parser.c \
	pcm.c \
	qdm2.c \
	qdrw.c \
	qpeg.c \
	qtrle.c \
	ra144.c \
	ra288.c \
	rangecoder.c \
	ratecontrol.c \
	resample2.c \
	roqvideo.c \
	rpza.c \
	rtjpeg.c \
	rv10.c \
	shorten.c \
	simple_idct.c \
	smacker.c \
	smc.c \
	snow.c \
	svq1.c \
	tscc.c \
	truemotion1.c \
	truemotion2.c \
	truespeech.c \
	tta.c \
	ulti.c \
	utils.c \
	vc1.c \
	vc1dsp.c \
	vcr1.c \
	vmdav.c \
	vorbis.c \
	vorbis_data.c \
	vp3.c \
	vp3dsp.c \
	vp5.c \
        vp56.c \
        vp56data.c \
        vp6.c \
	vqavideo.c \
	wavpack.c \
	wmadec.c \
	wnv1.c \
	xan.c \
	xl.c \
	zmbv.c

libavcodec_la_LDFLAGS = \
	$(top_builddir)/src/libffmpeg/libavcodec/armv4l/libavcodec_armv4l.la \
	$(top_builddir)/src/libffmpeg/libavcodec/i386/libavcodec_mmx.la \
	$(top_builddir)/src/libffmpeg/libavcodec/mlib/libavcodec_mlib.la \
	$(top_builddir)/src/libffmpeg/libavcodec/ppc/libavcodec_ppc.la \
	$(top_builddir)/src/libffmpeg/libavcodec/sparc/libavcodec_sparc.la \
	-avoid-version -module


noinst_HEADERS = \
	avcodec.h \
	bitstream.h \
	bytestream.h \
	cabac.h \
	cavsdata.h \
	cookdata.h \
	dsputil.h \
	dvdata.h \
	eval.h \
	faandct.h \
	fastmemcpy.h \
	golomb.h \
	imgconvert_template.h \
	indeo2data.h \
	indeo3data.h \
	h261data.h \
	h263data.h \
	h264data.h \
	lzo.h \
	mathops.h \
	mpeg4data.h \
	mpeg12data.h \
	mpegaudio.h \
	mpegaudiodectab.h \
	mpegaudiotab.h \
	mpegvideo.h \
	msmpeg4data.h \
	opt.h \
	parser.h \
	qdm2data.h \
	ra144.h \
	ra288.h \
	rangecoder.h \
	ratecontrol.h \
	rtjpeg.h \
	simple_idct.h \
	snow.h \
	sp5x.h \
	svq1_cb.h \
	svq1_vlc.h \
	swscale.h \
	truemotion1data.h \
	truespeech_data.h \
	ulti_cb.h \
	vorbis.h \
	vc1acdata.h \
	vc1data.h \
	vp3data.h \
	vp56.h \
	vp56data.h \
	vp5data.h \
	vp6data.h \
	wmadata.h
