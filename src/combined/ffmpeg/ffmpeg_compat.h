/*
 * Copyright (C) 2000-2015 the xine project
 *
 * This file is part of xine, a unix video player.
 *
 * xine is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * xine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 * Compability macros for various ffmpeg versions
 */

#ifndef XINE_AVCODEC_COMPAT_H
#define XINE_AVCODEC_COMPAT_H

#define AV_INT_VERSION(major,minor,micro) ((major<<16)|(minor<<8)|micro)

#ifndef LIBAVCODEC_VERSION_INT
#  if defined(LIBAVCODEC_VERSION_MAJOR) && defined(LIBAVCODEC_VERSION_MINOR)
#    define LIBAVCODEC_VERSION_INT AV_INT_VERSION(LIBAVCODEC_VERSION_MAJOR,LIBAVCODEC_VERSION_MINOR,0)
#  else
#    error ffmpeg headers must be included first !
#  endif
#endif

#ifndef LIBAVUTIL_VERSION_INT
#  if defined(LIBAVUTIL_VERSION_MAJOR) && defined(LIBAVUTIL_VERSION_MINOR)
#    define LIBAVUTIL_VERSION_INT AV_INT_VERSION(LIBAVUTIL_VERSION_MAJOR,LIBAVUTIL_VERSION_MINOR,0)
#  else
#    error ffmpeg headers must be included first !
#  endif
#endif

#if LIBAVCODEC_VERSION_INT >= AV_INT_VERSION(52,0,0)
#  define bits_per_sample bits_per_coded_sample
#endif

#if LIBAVCODEC_VERSION_INT >= AV_INT_VERSION(52,32,0)
#else
#  define pp_context	pp_context_t
#  define pp_mode	pp_mode_t
#endif

/* reordered_opaque appeared in libavcodec 51.68.0 */
#define AVCODEC_HAS_REORDERED_OPAQUE
#if LIBAVCODEC_VERSION_INT < AV_INT_VERSION(51,68,0)
# undef AVCODEC_HAS_REORDERED_OPAQUE
#endif

/* colorspace and color_range were added before 52.29.0 */
#if LIBAVCODEC_VERSION_INT >= AV_INT_VERSION(52,29,0)
# define AVCODEC_HAS_COLORSPACE
#endif

/* "unused" as of v54 */
#if LIBAVCODEC_VERSION_INT < AV_INT_VERSION(54,0,0)
# define AVCODEC_HAS_SUB_ID
#endif

/* not 100% sure about this (between 55.19 and 56.56) */
#if LIBAVCODEC_VERSION_INT < AV_INT_VERSION(56,0,0)
#  define AVCODEC_HAS_STREAM_CODEC_TAG
#endif

/**/
#if LIBAVCODEC_VERSION_INT >= AV_INT_VERSION(53,8,0)
#  define avcodec_init() do {} while(0)
#endif

/* avcodec_alloc_context() */
#if LIBAVCODEC_VERSION_INT >= AV_INT_VERSION(53,6,0)
#  define AVCONTEXT 3
#  define avcodec_alloc_context() avcodec_alloc_context3(NULL)
#else
#  define AVCONTEXT 1
#endif

/* avcodec_open() */
#if LIBAVCODEC_VERSION_INT >= AV_INT_VERSION(53,6,0)
#  define AVOPEN 2
#  define avcodec_open(ctx,codec) avcodec_open2(ctx, codec, NULL)
#else
#  define AVOPEN 1
#endif

/* avcodec_thread_init() */
#if LIBAVCODEC_VERSION_INT >= AV_INT_VERSION(52,112,0)
#  define DEPRECATED_AVCODEC_THREAD_INIT 1
#endif

/* av_parser_parse() */
#if LIBAVCODEC_VERSION_INT >= AV_INT_VERSION(52,94,0)
#  define AVPARSE 2
#else
#  define AVPARSE 1
#endif

/* avcodec_decode_video() */
#if LIBAVCODEC_VERSION_INT >= AV_INT_VERSION(52,32,0)
#  define AVVIDEO 2
#else
#  define AVVIDEO 1
#endif

/* avcodec_decode_audio() */
#if LIBAVCODEC_VERSION_INT >= AV_INT_VERSION(54,0,0)
#  define AVAUDIO 4
#elif LIBAVCODEC_VERSION_INT >= AV_INT_VERSION(52,32,0)
#  define AVAUDIO 3
#else
#  define AVAUDIO 2
#endif

/* avcodec_encode_video(), av_packet_unref */
#if LIBAVCODEC_VERSION_INT >= AV_INT_VERSION(55,25,100)
#  define AVENCVIDEO 2
#else
#  define AVENCVIDEO 1
#endif

/* AVFrame.age */
#if LIBAVCODEC_VERSION_INT >= AV_INT_VERSION(53,28,1) && LIBAVCODEC_VERSION_INT < AV_INT_VERSION(54,0,0) // not sure about this - original condition was broken
#  define AVFRAMEAGE 1
#endif

#if LIBAVCODEC_VERSION_INT < AV_INT_VERSION(53,0,0)
/* release 0.7.x (libavcodec 52) has deprecated AVCodecContext.palctrl but for backwards compatibility no
   working alternative. */
#  define AVPALETTE 1
#elif LIBAVCODEC_VERSION_INT < AV_INT_VERSION(54,92,100)
/* pass palette as AVPacket side data, free with av_destruct_packet () after NULLing main data pointer */
#  define AVPALETTE 2
#else
/* dito, free with av_free_packet () after NULLing main data pointer */
#  define AVPALETTE 3
#endif

#if LIBAVUTIL_VERSION_INT >= AV_INT_VERSION(52,0,0)
#  define PIX_FMT_NONE      AV_PIX_FMT_NONE
#  define PIX_FMT_YUV420P   AV_PIX_FMT_YUV420P
#  define PIX_FMT_YUVJ420P  AV_PIX_FMT_YUVJ420P
#  define PIX_FMT_YUV444P   AV_PIX_FMT_YUV444P
#  define PIX_FMT_YUVJ444P  AV_PIX_FMT_YUVJ444P
#  define PIX_FMT_YUV410P   AV_PIX_FMT_YUV410P
#  define PIX_FMT_YUV411P   AV_PIX_FMT_YUV411P
#  define PIX_FMT_VAAPI_VLD AV_PIX_FMT_VAAPI_VLD
#  define PIX_FMT_ARGB      AV_PIX_FMT_ARGB
#  define PIX_FMT_BGRA      AV_PIX_FMT_BGRA
#  define PIX_FMT_RGB24     AV_PIX_FMT_RGB24
#  define PIX_FMT_BGR24     AV_PIX_FMT_BGR24
#  define PIX_FMT_RGB555BE  AV_PIX_FMT_RGB555BE
#  define PIX_FMT_RGB555LE  AV_PIX_FMT_RGB555LE
#  define PIX_FMT_RGB565BE  AV_PIX_FMT_RGB565BE
#  define PIX_FMT_RGB565LE  AV_PIX_FMT_RGB565LE
#  define PIX_FMT_PAL8      AV_PIX_FMT_PAL8
#  define PixelFormat       AVPixelFormat
/* video_out/video_out_vaapi */
#  define PIX_FMT_VAAPI_IDCT AV_PIX_FMT_VAAPI_IDCT
#  define PIX_FMT_VAAPI_MOCO AV_PIX_FMT_VAAPI_MOCO
#endif

#if LIBAVCODEC_VERSION_INT >= AV_INT_VERSION(54,25,0)
/* dxr3/ffmpeg_encoder */
#  define CODEC_ID_MPEG1VIDEO AV_CODEC_ID_MPEG1VIDEO
/* video_out/video_out_vaapi */
#  define CODEC_ID_MPEG2VIDEO AV_CODEC_ID_MPEG2VIDEO
#  define CODEC_ID_MPEG4      AV_CODEC_ID_MPEG4
#  define CODEC_ID_H263       AV_CODEC_ID_H263
#  define CODEC_ID_H264       AV_CODEC_ID_H264
#  define CODEC_ID_WMV3       AV_CODEC_ID_WMV3
/* video_out/video_out_vaapi, ff_video_decoder */
#  define CODEC_ID_VC1        AV_CODEC_ID_VC1
/* ff_video_decoder */
#  define CODEC_ID_SVQ3       AV_CODEC_ID_SVQ3
#  define CODEC_ID_MSMPEG4V1  AV_CODEC_ID_MSMPEG4V1
#  define CODEC_ID_MSMPEG4V2  AV_CODEC_ID_MSMPEG4V2
#  define CODEC_ID_MSMPEG4V3  AV_CODEC_ID_MSMPEG4V3
#  define CODEC_ID_WMV1       AV_CODEC_ID_WMV1
#  define CODEC_ID_WMV2       AV_CODEC_ID_WMV2
/* demux_avformat */
#  define CODEC_ID_PCM_S16LE  AV_CODEC_ID_PCM_S16LE
#  define CODEC_ID_PCM_S16BE  AV_CODEC_ID_PCM_S16BE
#  define CODEC_ID_MP2        AV_CODEC_ID_MP2
#  define CODEC_ID_AC3        AV_CODEC_ID_AC3
/* ff_*_decoder mapping is already handled by mkcodeclists.pl */
#endif

#ifndef AVCODEC_MAX_AUDIO_FRAME_SIZE
#  define AVCODEC_MAX_AUDIO_FRAME_SIZE 192000
#endif

#if LIBAVCODEC_VERSION_INT < AV_INT_VERSION(52,66,0)
#  define avcodec_get_edge_width() (16)
#elif LIBAVCODEC_VERSION_INT >= AV_INT_VERSION(55,48,102)
/* edges not needed anymore supposedly */
#  define avcodec_get_edge_width() (0)
#endif

#if LIBAVCODEC_VERSION_INT >= AV_INT_VERSION(55,0,100)
#  define AV_BUFFER 1
#endif

#if LIBAVCODEC_VERSION_INT < AV_INT_VERSION(55,63,100)
#  define avcodec_free_context(pp) av_free(*(pp))
#endif

#if LIBAVCODEC_VERSION_INT < AV_INT_VERSION(54,59,100)
#  define avcodec_free_frame(pp) av_free(*(pp))
#elif LIBAVCODEC_VERSION_INT >= AV_INT_VERSION(55,45,101)
#  define avcodec_alloc_frame() av_frame_alloc()
#  define avcodec_free_frame(pp) av_frame_free(pp)
#endif

#endif /* XINE_AVCODEC_COMPAT_H */
