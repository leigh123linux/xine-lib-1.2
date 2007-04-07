srcdir = $(shell cd @srcdir@; pwd)

CC = @CC@
CFLAGS = @CFLAGS@
LDFLAGS = @LDFLAGS@
VISIBILITY_FLAG = @VISIBILITY_FLAG@

ffmpeg/config.mak: $(srcdir)/ffmpeg/configure Makefile
	mkdir -p ffmpeg
	cd ffmpeg; \
	$(srcdir)/ffmpeg/configure \
		--disable-shared --enable-static \
		--disable-encoders \
		--disable-demuxers \
		--disable-muxers \
		--disable-strip \
		--make="$(MAKE)" \
		--cc="$(CC)" \
		--extra-cflags="$(CFLAGS) $(VISIBILITY_FLAG) -fPIC -DPIC" \
		--extra-ldflags="$(LDFLAGS)" \
		--enable-gpl \
		--enable-pthreads \
		--disable-ffmpeg \
		--disable-ffserver \
		--disable-ffplay; \
	cd ..

ffmpeg/libavutil/libavutil.a: ffmpeg/config.mak
	$(MAKE) -C ffmpeg/libavutil libavutil.a

ffmpeg/libavcodec/libavcodec.a: ffmpeg/config.mak
	$(MAKE) -C ffmpeg/libavcodec libavcodec.a

ffmpeg/libpostproc/libpostproc.a: ffmpeg/config.mak
	$(MAKE) -C ffmpeg/libpostproc libpostproc.a

clean-local:
	-$(MAKE) -C ffmpeg clean

distclean-local:
	-$(MAKE) -C ffmpeg distclean

EXTRA_DIST = README.contrib
