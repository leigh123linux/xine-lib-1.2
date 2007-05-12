SUBDIRS = libmpcdec libxdg-basedir libdca

srcdir = $(shell cd @srcdir@; pwd)

CC = @CC@
CFLAGS = @CFLAGS@
LDFLAGS = @LDFLAGS@
VISIBILITY_FLAG = @VISIBILITY_FLAG@

if ENABLE_DXR3
dxr3encoder = --enable-encoder=mpeg1video
endif

if FFMPEG_DISABLE_UNCOMMON_CODECS
disable_uncommon_codecs=--disable-decoder=aasc --disable-decoder=asv1 --disable-decoder=asv2 --disable-decoder=avs \
	--disable-decoder=cscd --disable-decoder=cyuv --disable-decoder=dvvideo --disable-decoder=eightbps \
	--disable-decoder=flic --disable-decoder=flv --disable-decoder=fourxm --disable-decoder=fraps \
	--disable-decoder=huffyuv --disable-decoder=idcin --disable-decoder=interplay_video --disable-decoder=kmvc \
	--disable-decoder=loco --disable-decoder=mmvideo --disable-decoder=nuv --disable-decoder=qdraw \
	--disable-decoder=qpeg --disable-decoder=roq --disable-decoder=rpza --disable-decoder=smacker \
	--disable-decoder=smc --disable-decoder=snow --disable-decoder=truemotion1 --disable-decoder=truemotion2 \
	--disable-decoder=tscc --disable-decoder=ulti --disable-decoder=vcr1 --disable-decoder=vmdvideo \
	--disable-decoder=wnv1 --disable-decoder=xan_wc3 --disable-decoder=xl --disable-decoder=zmbv \
	--disable-decoder=alac --disable-decoder=amr_nb --disable-decoder=amr_wb --disable-decoder=libgsm \
	--disable-decoder=mace3 --disable-decoder=mace6 --disable-decoder=shorten --disable-decoder=smackaud \
	--disable-decoder=truespeech --disable-decoder=tta --disable-decoder=vmdaudio --disable-decoder=pcm_alaw \
	--disable-decoder=pcm_mulaw --disable-decoder=pcm_s8 --disable-decoder=pcm_s16be --disable-decoder=pcm_s16le \
	--disable-decoder=pcm_s24be --disable-decoder=pcm_s24daud --disable-decoder=pcm_s24le --disable-decoder=pcm_s32be \
	--disable-decoder=pcm_s32le --disable-decoder=pcm_u8 --disable-decoder=pcm_u16be --disable-decoder=pcm_u16le \
	--disable-decoder=pcm_u24be --disable-decoder=pcm_u24le --disable-decoder=pcm_u32be --disable-decoder=pcm_u32le \
	--disable-decoder=interplay_dpcm --disable-decoder=roq_dpcm --disable-decoder=sol_dpcm --disable-decoder=vqa \
	--disable-decoder=xan_dpcm --disable-decoder=adpcm_4xm --disable-decoder=adpcm_ct --disable-decoder=adpcm_ea \
	--disable-decoder=adpcm_ima_dk3 --disable-decoder=adpcm_ima_dk4 --disable-decoder=adpcm_ima_qt \
	--disable-decoder=adpcm_ima_smjpeg --disable-decoder=adpcm_ima_wav --disable-decoder=adpcm_ima_ws \
	--disable-decoder=adpcm_ms --disable-decoder=adpcm_sbpro_2 --disable-decoder=adpcm_sbpro_3 \
	--disable-decoder=adpcm_sbpro_4 --disable-decoder=adpcm_xa --disable-decoder=adpcm_yamaha
endif

if FFMPEG_DISABLE_POPULAR_CODECS
disable_popular_codecs=--disable-decoder=cinepak --disable-decoder=flashsv --disable-decoder=h261 --disable-decoder=h263 \
	--disable-decoder=h263i --disable-decoder=h264 --disable-decoder=indeo2 --disable-decoder=indeo3 \
	--disable-decoder=mjpeg --disable-decoder=mjpegb --disable-decoder=mpeg1video --disable-decoder=mpeg2video \
	--disable-decoder=mpeg4 --disable-decoder=mpegvideo --disable-decoder=msmpeg4v1 --disable-decoder=msmpeg4v2 \
	--disable-decoder=msmpeg4v3 --disable-decoder=msrle --disable-decoder=msvideo1 --disable-decoder=qtrle \
	--disable-decoder=rv10 --disable-decoder=rv20 --disable-decoder=svq1 --disable-decoder=svq3 --disable-decoder=vc1 \
	--disable-decoder=vp3 --disable-decoder=vp5 --disable-decoder=vp6 --disable-decoder=vp6f --disable-decoder=wmv1 \
	--disable-decoder=wmv2 --disable-decoder=wmv3 --disable-decoder=cook --disable-decoder=dts --disable-decoder=flac \
	--disable-decoder=mp2 --disable-decoder=mp3 --disable-decoder=qdm2 --disable-decoder=ra_144 --disable-decoder=ra_288 \
	--disable-decoder=wavpack --disable-decoder=wmav1 --disable-decoder=wmav2 --disable-decoder=adpcm_swf
endif

configure_options =\
	--disable-shared --enable-static --disable-demuxers --disable-muxers --disable-strip \
	--enable-gpl --enable-pthreads --disable-ffmpeg --disable-ffserver --disable-ffplay

disable_decoders  =\
	--disable-decoder=bmp --disable-decoder=cavs --disable-decoder=cljr --disable-decoder=dsicinvideo \
	--disable-decoder=ffv1 --disable-decoder=ffvhuff --disable-decoder=gif --disable-decoder=mdec \
	--disable-decoder=mpeg_xvmc --disable-decoder=mszh --disable-decoder=png --disable-decoder=rawvideo \
	--disable-decoder=sp5x --disable-decoder=targa --disable-decoder=tiertexseqvideo --disable-decoder=tiff \
	--disable-decoder=vmnc --disable-decoder=zlib --disable-decoder=dsicinaudio --disable-decoder=imc \
	--disable-decoder=mp3adu --disable-decoder=mp3on4 --disable-decoder=mpc7 --disable-decoder=sonic \
	--disable-decoder=ws_snd1 --disable-decoder=adpcm_adx --disable-decoder=adpcm_g726 --disable-decoder=dvbsub \
	--disable-decoder=dvdsub --disable-decoder=theora --disable-decoder=aac --disable-decoder=mpeg4aac \
	--disable-decoder=ac3 --disable-decoder=vorbis

all_configure_options = $(configure_options)							\
			--disable-encoders $(dxr3encoder)					\
			--enable-encoders $(disable_uncommon_codecs) $(disable_popular_codecs)	\
			$(disable_decoders)							\
			--make="$(MAKE)"							\
			--cc="$(CC)"

ffmpeg/config.mak: $(srcdir)/ffmpeg/configure Makefile
if MACOSX_UNIVERSAL_BINARY
	export CFLAGS="$(VISIBILITY_FLAG)";						\
	export FFMPEG_CONFIGURE_OPTIONS="$(all_configure_options)";			\
	$(srcdir)/ffmpeg-universal.sh -configure "$(srcdir)/ffmpeg" $(UNIVERSAL_ARCHES)
else
	mkdir -p ffmpeg
	cd ffmpeg; \
	$(srcdir)/ffmpeg/configure $(all_configure_options) \
		--extra-cflags="$(CFLAGS) $(VISIBILITY_FLAG) -fPIC -DPIC" \
		--extra-ldflags="$(LDFLAGS)"; \
	cd ..
endif

ffmpeg/libavutil/libavutil.a: ffmpeg/config.mak
if MACOSX_UNIVERSAL_BINARY
	export MAKE="$(MAKE)"; \
	$(srcdir)/ffmpeg-universal.sh -avutil "$(srcdir)/ffmpeg" $(UNIVERSAL_ARCHES)
else
	$(MAKE) -C ffmpeg/libavutil libavutil.a
endif

ffmpeg/libavcodec/libavcodec.a: ffmpeg/config.mak
if MACOSX_UNIVERSAL_BINARY
	export MAKE="$(MAKE)"; \
	$(srcdir)/ffmpeg-universal.sh -avcodec "$(srcdir)/ffmpeg" $(UNIVERSAL_ARCHES)
else
	$(MAKE) -C ffmpeg/libavcodec libavcodec.a
endif

ffmpeg/libpostproc/libpostproc.a: ffmpeg/config.mak
if MACOSX_UNIVERSAL_BINARY
	export MAKE="$(MAKE)"; \
	$(srcdir)/ffmpeg-universal.sh -postproc "$(srcdir)/ffmpeg" $(UNIVERSAL_ARCHES)
else
	$(MAKE) -C ffmpeg/libpostproc libpostproc.a
endif

clean-local:
if MACOSX_UNIVERSAL_BINARY
	for arch in $(UNIVERSAL_ARCHES); do	\
		$(MAKE) -C ffmpeg/$$arch clean;	\
	done
	rm -f ffmpeg/libavcodec/libavcodec.a ffmpeg/libavutil/libavutil.a ffmpeg/libpostproc/libpostproc.a
else
	-$(MAKE) -C ffmpeg clean
endif

distclean-local:
if MACOSX_UNIVERSAL_BINARY
	for arch in $(UNIVERSAL_ARCHES); do		\
		$(MAKE) -C ffmpeg/$$arch distclean;	\
	done
	rm -f ffmpeg/config.mak ffmpeg/config.h
	rm -f ffmpeg/libavcodec/libavcodec.a ffmpeg/libavutil/libavutil.a ffmpeg/libpostproc/libpostproc.a
else
	-$(MAKE) -C ffmpeg distclean
endif

EXTRA_DIST = README.contrib ffmpeg-distfiles

dist-hook:
	while read entry; do		 \
	  test -d $(srcdir)/$$entry &&	 \
	    mkdir -p $(distdir)/$$entry; \
	  test -f $(srcdir)/$$entry &&	 \
	    cp -p $(srcdir)/$$entry $(distdir)/$$entry; \
	done < $(srcdir)/ffmpeg-distfiles

