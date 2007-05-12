AC_DEFUN([XINE_LIB_SUMMARY], [
    dnl ---------------------------------------------
    dnl Some infos:
    dnl ---------------------------------------------

    echo "xine-lib summary:"
    echo "----------------"

    dnl Input
    echo " * input plugins:"
    echo "   - file          - net"
    echo "   - stdin_fifo    - rtp"
    echo "   - http          - mms"
    echo "   - pnm           - rtsp"
    echo "   - dvb"
    if test "x$with_external_dvdnav" = "xyes"; then
        echo "   - dvd (external libs)"
    else
        echo "   - dvd (internal libs)"
    fi
    if test "x$enable_vcd" = "xyes"; then
        if test "x$internal_vcdnav" = "xno"; then
            echo "   - vcd (external libs)"
        else
            echo "   - vcd (internal libs)"
        fi
    fi
    echo "   - vcdo"
    echo "   - cdda"
    if test "x$no_gnome_vfs" = "xno"; then
        echo "   - gnome-vfs"
    fi
    if test "x$have_v4l" = "xyes"; then
        echo "   - v4l"
    fi
    if test "x$have_libsmbclient" = "xyes"; then
        echo "   - smbclient"
    fi
    echo ""

    dnl Demuxers
    echo " * demultiplexer plugins:"
    echo "   - avi           - mpeg"
    echo "   - mpeg_block    - mpeg_audio"
    echo "   - mpeg_elem     - mpeg_pes"
    echo "   - mpeg_ts       - qt/mpeg-4"
    echo "   - film          - roq"
    echo "   - fli           - smjpeg"
    echo "   - idcin         - wav"
    echo "   - wc3 mve       - voc"
    echo "   - vqa           - aiff"
    echo "   - cdda          - snd/au"
    echo "   - yuv4mpeg2     - real/realaudio"
    echo "   - ea wve        - raw dv"
    echo "   - interplay mve - psx str"
    echo "   - ws aud        - pva"
    echo "   - vox           - nsf"
    echo "   - nsv           - 4xm"
    echo "   - FLAC          - aac"
    echo "   - iff           - matroska"
    echo "   - vmd           - flv"
    if test "x$enable_asf" = "xyes"; then
        echo "   - asf"
    fi
    if test "x$have_vorbis" = "xyes"; then
        echo "   - ogg"
    fi
    if test "x$enable_mng" != x"no"; then
        echo "   - mng"
    fi
    if test "x$enable_modplug" != x"no"; then
        echo "   - mod"
    fi
    if test "x$have_libflac" = "xyes"; then
        echo "   - FLAC (with libFLAC)"
    fi
    if test "x$have_wavpack" = "xyes"; then
        echo "   - WavPack"
    fi
    if test "x$enable_a52dec" = "xyes"; then
        if test "x$have_a52" = "xyes"; then
            echo "   - ac3 (external library)"
        else
            echo "   - ac3 (internal library)"
        fi
    fi
    echo ""

    dnl video decoders
    echo " * video decoder plugins:"
    echo "   - MPEG 1,2         - Amiga Bitplane"
    echo "   - Raw RGB          - Raw YUV"
    if test "x$with_external_ffmpeg" = "xyes"; then
        echo "   - ffmpeg (external library):"
    else
        echo "   - ffmpeg (internal library):"
    fi
    echo "     - MPEG-4 (ISO, Microsoft, DivX*, XviD)"
    echo "     - Creative YUV    - Motion JPEG"
    echo "     - Cinepak         - MS Video-1"
    echo "     - FLI/FLC         - MS RLE"
    echo "     - Id RoQ          - Id Cin"
    echo "     - Apple Graphics  - Apple Video"
    echo "     - Apple Animation - Interplay Video"
    echo "     - Westwood VQA    - Origin Xan"
    echo "     - H.263           - Intel Indeo 3"
    echo "     - SVQ1            - SVQ3"
    echo "     - Real Video 1.0  - Real Video 2.0"
    echo "     - 4X Video        - Sierra Video"
    echo "     - Asus v1/v2      - HuffYUV"
    echo "     - On2 VP3         - DV"
    echo "     - 8BPS            - Duck TrueMotion v1"
    echo "     - ATI VCR1        - Flash Video"
    echo "     - ZLIB            - MSZH"
    if test "x$have_dxr3" = "xyes"; then
        echo "   - dxr3_video"
    fi
    if test "x$enable_w32dll" = "xyes"; then
        echo "   - w32dll"
    fi
    if test "x$have_imagemagick" = "xyes"; then
        echo "   - image"
    fi
    if test x"no_gdkpixbuf" != "xyes"; then
        echo "   - gdk-pixbuf"
    fi
    if test "x$have_theora" = "xyes"; then
        echo "   - theora"
    fi
    echo ""

    dnl audio decoders
    echo " * audio decoder plugins:"
    echo "   - GSM 06.10"
    echo "   - linear PCM      - Nosefart (NSF)"
    if test "x$with_external_ffmpeg" = "xyes"; then
        echo "   - ffmpeg (external library):"
    else
        echo "   - ffmpeg (internal library):"
    fi
    echo "     - Windows Media Audio v1/v2"
    echo "     - DV            - logarithmic PCM"
    echo "     - 14k4          - 28k8"
    echo "     - MS ADPCM      - IMA ADPCM"
    echo "     - XA ADPCM      - Game DPCM/ADPCM"
    echo "     - Mace 3:13     - Mace 6:1"
    echo "     - FLAC"
    if test "x$have_libflac" = "xyes"; then
        echo "   - FLAC (with libFLAC)"
    fi
    if test "x$have_vorbis" = "xyes"; then
        echo "   - vorbis"
    fi
    if test "x$have_speex" = "xyes"; then
        echo "   - speex"
    fi
    if test "x$enable_w32dll" = "xyes"; then
        echo "   - w32dll"
    fi
    if test "x$enable_faad" = "xyes"; then
        echo "   - faad"
    fi
    if test "x$enable_mad" = "xyes"; then
        if test "x$with_external_mad" != "xno"; then
            echo "   - MAD (MPG 1/2/3) (external library)"
        else
            echo "   - MAD (MPG 1/2/3) (internal library)"
        fi
    fi
    if test "x$enable_libdts" = "xyes"; then
        if test "x$have_dts" = "xyes"; then
            echo "   - DTS (external library)"
        else
            echo "   - DTS (internal library)"
        fi
    fi
    if test "x$enable_a52dec" = "xyes"; then
        if test "x$have_a52" = "xyes"; then
            echo "   - A52/ra-dnet (external library)"
        else
            echo "   - A52/ra-dnet (internal library)"
        fi
    fi
    if test "x$enable_musepack" != "xno"; then
        if test "x$have_mpcdec" = "xyes"; then
            echo "   - MusePack (external library)"
        else
            echo "   - MusePack (internal library)"
        fi
    fi
    if test "x$have_wavpack" = "xyes"; then
        echo "   - WavPack"
    fi
    echo ""

    dnl spu decoders
    echo " * subtitle decoder plugins:"
    echo "   - spu             - spucc"
    echo "   - spucmml         - sputext"
    echo "   - spudvb"
    if test "x$have_dxr3" = "xyes"; then
        echo "   - dxr3_spu"
    fi
    echo ""

    dnl post plugins
    echo " * post effect plugins:"
    echo "  * planar video effects:"
    echo "   - invert          - expand"
    echo "   - eq              - eq2"
    echo "   - boxblur         - denoise3d"
    echo "   - unsharp         - tvtime"
    echo "  * SFX:"
    echo "   - goom            - oscope"
    echo "   - fftscope        - mosaico"
    echo ""

    dnl Video plugins
    echo " * video driver plugins:"
    if test "x$no_x" != "xyes"; then
        echo "   - XShm (X11 shared memory)"
        dnl synfb
        if test "x$enable_syncfb" != "xno"; then
            echo "   - SyncFB (for Matrox G200/G400 cards)"
        fi
        dnl Xv
        if test "x$ac_have_xv" = "xyes"; then
            if test "x$ac_have_xv_static" = "xyes"; then
                echo "   - Xv (XVideo *static*)"
            else
                echo "   - Xv (XVideo *shared*)"
            fi
        fi
        dnl XxMC
        if test "x$ac_have_xxmc" = "xyes"; then
            if test "x$ac_have_vldxvmc_h" = "xyes"; then
                echo "   - XxMC (XVideo extended motion compensation)"
            else
                echo "   - XxMC (XVideo motion compensation - vld extensions DISABLED)"
            fi
        fi
        dnl XvMC
        if test "x$ac_have_xvmc" = "xyes"; then
            echo "   - XvMC (XVideo motion compensation)"
        fi
        if test "x$ac_have_opengl" = "xyes" -a "x$ac_have_glut" = "xyes" -o \
                x$"ac_have_opengl" = "xyes" -a "x$ac_have_glu" = "xyes"; then	
            echo "   - OpenGL"
        fi
        if test "x$ac_have_sunfb" = "xyes"; then
            if test "x$ac_have_sundga" = "xyes"; then
                echo "   - PGX64 (for Sun XVR100/PGX64/PGX24 cards)"
                echo "   - PGX32 (for Sun PGX32 cards)"
            fi
        fi
    fi
    if test "x$have_xcb" = "xyes"; then
        dnl xcb-shm
        if test "x$have_xcbshm" = "xyes"; then
            echo "   - xcb-shm (X shared memory using XCB)"
        fi
        dnl xcb-xv
        if test "x$have_xcbxv" = "xyes"; then
            echo "   - xcb-xv (XVideo using XCB)"
        fi
    fi
    if test "x$no_aalib" != "xyes"; then
        echo "   - aa (Ascii ART)"
    fi
    if test "x$have_caca" = "xyes"; then
        echo "   - caca (Color AsCii Art)"
    fi
    if test "x$have_fb" = "xyes"; then
        echo "   - fb (Linux framebuffer device)"
    fi
    if test "x$have_sdl" = "xyes"; then
        echo "   - sdl (Simple DirectMedia Layer)"
    fi
    if test "x$have_libstk" = "xyes"; then
        echo "   - stk (Libstk Set-top Toolkit)"
    fi
    if test "x$have_directfb" = "xyes"; then
        echo "   - directfb (DirectFB driver)"
    fi
    if test "x$have_dxr3" = "xyes"; then
        if test "x$have_encoder" = "xyes"; then
            echo "   - dxr3 (Hollywood+ and Creative dxr3, both mpeg and non-mpeg video)"
        else
            echo "   - dxr3 (Hollywood+ and Creative dxr3, mpeg video only)"
        fi
    fi
    if test "x$enable_vidix" = "xyes"; then
        echo $ECHO_N "   - vidix ("
      
        if test "x$no_x" != "xyes"; then
            echo $ECHO_N "X11"
            if test "x$have_fb" = "xyes"; then
                echo $ECHO_N " and "
            fi
        fi
        
        if test "x$have_fb" = "xyes"; then
            echo $ECHO_N "framebuffer"
        fi
      
        echo $ECHO_N " support"

        if test "x$enable_dha_kmod" = "xyes"; then
            echo " with dhahelper)"
        else
            echo ")"
        fi
    fi
    if test "x$have_directx" = "xyes"; then
        echo "   - directx (DirectX video driver)"
    fi
    if test "x$have_macosx_video" = "xyes"; then
        echo "   - Mac OS X OpenGL"
    fi

    echo ""

    dnl Audio plugins
    echo " * audio driver plugins:"
    if test "x$have_ossaudio" = "xyes"; then
        echo "   - oss (Open Sound System)"
    fi
    if test "x$have_alsa" = "xyes"; then
        echo "   - alsa"
    fi
    if test "x$have_esound" = "xyes"; then
        echo "   - esd (Enlightened Sound Daemon)"
    fi
    if test "x$no_arts" != "xyes"; then
        echo "   - arts (aRts - KDE soundserver)"
    fi
    if test "x$no_fusionsound" != "xyes"; then
        echo "   - fusionsound (FusionSound driver)"
    fi
    if test "x$have_sunaudio" = "xyes"; then
        echo "   - sun ()"
    fi
    if test "x$am_cv_have_irixal" = xyes; then
        echo "   - irixal (Irix audio library)"
    fi
    if test "x$have_directx" = "xyes"; then
        echo "   - directx (DirectX audio driver)"
    fi
    if test "x$have_coreaudio" = "xyes"; then
        echo "   - CoreAudio (Mac OS X audio driver)"
    fi  
    if test "x$have_pulseaudio" = "xyes"; then
        echo "   - pulseaudio sound server"
    fi
    if test "x$have_jack" = "xyes"; then
        echo "   - Jack"
    fi
    echo "---"


    dnl ---------------------------------------------
    dnl some user warnings
    dnl ---------------------------------------------

    dnl some levels of variable expansion to get final install paths
    final_libdir="`eval eval eval eval echo $libdir`"
    final_bindir="`eval eval eval eval echo $bindir`"

    if test -r /etc/ld.so.conf && ! grep -x "$final_libdir" /etc/ld.so.conf >/dev/null ; then
        if test "$final_libdir" != "/lib" -a "$final_libdir" != "/usr/lib" ; then
            if ! echo "$LD_LIBRARY_PATH" | egrep "(:|^)$final_libdir(/?:|/?$)" >/dev/null ; then
                echo
                echo "****************************************************************"
                echo "xine-lib will be installed to $final_libdir"
                echo
                echo "This path is not mentioned among the linker search paths in your"
                echo "/etc/ld.so.conf. This means it is possible that xine-lib will"
                echo "not be found when you try to compile or run a program using it."
                echo "If this happens, you should add $final_libdir to"
                echo "the environment variable LD_LIBRARY_PATH like that:"
                echo
                echo "export LD_LIBRARY_PATH=$final_libdir:\$LD_LIBRARY_PATH"
                echo
                echo "Alternatively you can add a line \"$final_libdir\""
                echo "to your /etc/ld.so.conf."
                echo "****************************************************************"
                echo
            fi
        fi
    fi

    if ! echo "$PATH" | egrep "(:|^)$final_bindir(/?:|/?$)" >/dev/null ; then
        echo
        echo "****************************************************************"
        echo "xine-config will be installed to $final_bindir"
        echo
        echo "This path is not in your search path. This means it is possible"
        echo "that xine-config will not be found when you try to compile a"
        echo "program using xine-lib. This will result in build failures."
        echo "If this happens, you should add $final_bindir to"
        echo "the environment variable PATH like that:"
        echo
        echo "export PATH=$final_bindir:\$PATH"
        echo
        echo "Note that this is only needed for compilation. It is not needed"
        echo "to have xine-config in your search path at runtime. (Although"
        echo "it will not cause any harm either.)"
        echo "****************************************************************"
        echo
    fi

    dnl warn if no X11 plugins will be built
    if test "x$no_x" = "xyes"; then
        case $host in
            *mingw*|*-cygwin) ;;
            *-darwin*) ;;
            *)
                echo
                echo "****************************************************************"
                echo "WARNING! No X11 output plugins will be built."
                echo
                echo "For some reason, the requirements for building the X11 video"
                echo "output plugins are not met. That means, that you will NOT be"
                echo "able to use the resulting xine-lib to watch videos in a window"
                echo "on any X11-based display (e.g. your desktop)."
                echo
                echo "If this is not what you want, provide the necessary X11 build"
                echo "dependencies (usually done by installing a package called"
                echo "XFree86-devel or similar) and run configure again."
                echo "****************************************************************"
                echo
                ;;
        esac
    fi
])
