#!/bin/sh

function usage {
    echo "usage: $0 <action> <source path> <arch list>"
    echo "where: <action> is one of:"
    echo "       -configure    do initial configuration for each architecture"
    echo "       -avcodec      build libavcodec.a for each architecture"
    echo "       -avutil       build libavutil.a for each architecture"
    echo "       -postproc     build libpostproc.a for each architecture"
    echo "       -clean        clean for each architecture"
    echo "       -distclean    distclean for each architecture"
    echo ""
    echo "Configuration must be done before any library builds.  Options to be passed"
    echo "to ffmpeg's configure command-line should be passed in the environment"
    echo "using the FFMPEG_CONFIGURE_OPTIONS environment variable."
    exit 1
}

function run_configure {
    local arch=$1
    local ffmpeg_topbuilddir="$2"

    # start over from scratch
    rm -rf "$ffmpeg_topbuilddir"
    mkdir -p "$ffmpeg_topbuilddir"
    pushd "$ffmpeg_topbuilddir" > /dev/null 2>&1

    # The Makefile should be passing FFMPEG_CONFIGURE_OPTIONS without passing
    # any --extra-cflags or --extra-ldflags options.  Both CFLAGS and LDFLAGS
    # should be in the environment in addition to FFMPEG_CONFIGURE_OPTIONS.

    local CROSS_OPTIONS=
    local EXTRA_CFLAGS=
    local EXTRA_LDFLAGS=

    if test $HOST_ARCH != $arch; then
        EXTRA_CFLAGS="$CFLAGS -isysroot /Developer/SDKs/MacOSX10.4u.sdk -arch $arch"
        EXTRA_LDFLAGS="$LDFLAGS -isysroot /Developer/SDKs/MacOSX10.4u.sdk -arch $arch"
        if test $arch = i386; then
            FFMPEG_ARCH=x86_32
        else
            FFMPEG_ARCH=$arch
        fi
        CROSS_OPTIONS="--cross-compile --arch=$FFMPEG_ARCH"
    fi

    echo "$SOURCE_PATH/configure" $CROSS_OPTIONS $FFMPEG_CONFIGURE_OPTIONS \
                                  --extra-cflags="$EXTRA_CFLAGS" \
                                  --extra-ldflags="$EXTRA_LDFLAGS"
    "$SOURCE_PATH/configure" $CROSS_OPTIONS $FFMPEG_CONFIGURE_OPTIONS \
                             --extra-cflags="$EXTRA_CFLAGS" \
                             --extra-ldflags="$EXTRA_LDFLAGS"
    local retval=$?

    # We're building ffmpeg to put it into a shared library, but ffmpeg thinks
    # it's building to put it into a static library or program image, so it
    # adds -mdynamic-no-pic to its list of options as an optimization.  This is
    # fine in Tiger if later linked with -Wl,-read_only_relocs,warning, but it
    # isn't fine on other versions of Mac OS X.
    if test -f config.mak; then
        cat config.mak | sed -e '/OPTFLAGS=/s/-mdynamic-no-pic//g' > config.tmp
        mv -f config.tmp config.mak
    fi

    popd > /dev/null 2>&1
    `exit $retval` || exit $retval
}

if test x"$*" = x""; then
    usage
fi
case "$1" in
    -configure)
        MODE=configure
        ;;
    -avcodec)
        MODE=avcodec
        ;;
    -avutil)
        MODE=avutil
        ;;
    -postproc)
        MODE=postproc
        ;;
    -clean)
        MODE=clean
        ;;
    -distclean)
        MODE=distclean
        ;;
    *)
        echo "Unrecognized mode: $1"
        usage
        ;;
esac
shift

if test x"$1" = x""; then
    echo "No source path specified!"
    usage
fi
if test ! -d "$1"; then
    echo "Source path $1 does not exist!"
    exit 1
fi
SOURCE_PATH="$1"
shift

TOP_BUILD_PATH="`pwd`/ffmpeg"
if test -f "$TOP_BUILD_PATH/configure"; then
    # Building in place.  Create a temporary build directory and use that
    # instead, but place the final output from lipo into the original
    # build directory.
    BUILD_PATH="$TOP_BUILD_PATH/build"
else
    BUILD_PATH="$TOP_BUILD_PATH"
fi
# This doesn't make sense for -configure, but it does for everything else.
# The -configure mode won't ever use it anyway.
OUTPUT_FILENAME="$BUILD_PATH/lib$MODE/lib$MODE.a"

HOST_ARCH=`arch`
UNIVERSAL_ARCHES=$*
if test x"$UNIVERSAL_ARCHES" = x""; then
    echo "No architecture(s) specified; using $HOST_ARCH only."
    UNIVERSAL_ARCHES=$HOST_ARCH
fi
CONFIG_FILES=
LIPO_CMDLINE="-create -output $OUTPUT_FILENAME"
for arch in $UNIVERSAL_ARCHES; do
    ffmpeg_topbuilddir="$BUILD_PATH/$arch"
    LIPO_CMDLINE="$LIPO_CMDLINE -arch $arch $ffmpeg_topbuilddir/lib$MODE/lib$MODE.a"
    case $MODE in
        configure)
            run_configure $arch "$ffmpeg_topbuilddir"
            CONFIG_FILES="$CONFIG_FILES $ffmpeg_topbuilddir/config.h"
            ;;
        avcodec)
            "$MAKE" -C "$ffmpeg_topbuilddir/libavcodec" libavcodec.a || exit $?
            ;;
        avutil)
            "$MAKE" -C "$ffmpeg_topbuilddir/libavutil" libavutil.a || exit $?
            ;;
        postproc)
            "$MAKE" -C "$ffmpeg_topbuilddir/libpostproc" libpostproc.a || exit $?
            ;;
        clean)
            "$MAKE" -C "$ffmpeg_topbuilddir" clean || exit $?
            ;;
        distclean)
            "$MAKE" -C "$ffmpeg_topbuilddir" distclean || exit $?
            ;;
    esac
done

case $MODE in
    configure)
        # Now that configuration is done, create config.h in the top-level
        # ffmpeg directory.  Pull out only what's needed by xine-lib, removing
        # any possible platform conflicts
        grep -h "define CONFIG_.*_DECODER" $CONFIG_FILES | uniq > "$BUILD_PATH/config.h"
        touch "$BUILD_PATH/config.mak"
        ;;
    clean)
        rm -f "$BUILD_PATH/libavcodec/libavcodec.a" "$BUILD_PATH/libavutil/libavutil.a" \
              "$BUILD_PATH/libpostproc/libpostproc.a"
        ;;
    distclean)
        rm -f "$BUILD_PATH/config.mak" "$BUILD_PATH/config.h"
        rm -f "$BUILD_PATH/libavcodec/libavcodec.a" "$BUILD_PATH/libavutil/libavutil.a" \
              "$BUILD_PATH/libpostproc/libpostproc.a"
        ;;
    *)
        mkdir -p "`dirname "$OUTPUT_FILENAME"`"
        lipo $LIPO_CMDLINE
        ;;
esac
