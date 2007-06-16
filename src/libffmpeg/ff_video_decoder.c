/*
 * Copyright (C) 2001-2007 the xine project
 * 
 * This file is part of xine, a free video player.
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 *
 * $Id: video_decoder.c,v 1.73 2007/03/29 18:41:02 dgp85 Exp $
 *
 * xine video decoder plugin using ffmpeg
 *
 */
 
#ifdef HAVE_CONFIG_H
#include "config.h"
# ifndef HAVE_FFMPEG
#  include "ffmpeg_config.h"
# endif
#endif

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <pthread.h>
#include <math.h>
#include <assert.h>

#define LOG_MODULE "ffmpeg_video_dec"
#define LOG_VERBOSE
/*
#define LOG
*/
#include "xine_internal.h"
#include "bswap.h"
#include "buffer.h"
#include "xineutils.h"
#include "ffmpeg_decoder.h"
#include "ff_mpeg_parser.h"

#include <postprocess.h>

#define VIDEOBUFSIZE        (128*1024)
#define SLICE_BUFFER_SIZE   (1194*1024)

#define SLICE_OFFSET_SIZE   128

#define ENABLE_DIRECT_RENDERING

typedef struct ff_video_decoder_s ff_video_decoder_t;

typedef struct ff_video_class_s {
  video_decoder_class_t   decoder_class;

  int                     pp_quality;
  
  xine_t                 *xine;
} ff_video_class_t;

struct ff_video_decoder_s {
  video_decoder_t   video_decoder;

  ff_video_class_t *class;

  xine_stream_t    *stream;
  int64_t           pts;
  int               video_step;

  uint8_t           decoder_ok:1;
  uint8_t           decoder_init_mode:1;
  uint8_t           is_mpeg12:1;
  uint8_t           pp_available:1;
  uint8_t           yuv_init:1;
  uint8_t           is_direct_rendering_disabled:1;
  uint8_t           cs_convert_init:1;

  xine_bmiheader    bih;
  unsigned char    *buf;
  int               bufsize;
  int               size;
  int               skipframes;
  
  int               slice_offset_size;

  AVFrame          *av_frame;
  AVCodecContext   *context;
  AVCodec          *codec;
  
  int               pp_quality;
  int               pp_flags;
  pp_context_t     *pp_context;
  pp_mode_t        *pp_mode;

  /* mpeg-es parsing */
  mpeg_parser_t    *mpeg_parser;

  double            aspect_ratio;
  int               aspect_ratio_prio;
  int               frame_flags;
  int               crop_right, crop_bottom;
  
  int               output_format;

  xine_list_t       *dr1_frames;

  yuv_planes_t      yuv;

  AVPaletteControl  palette_control;
};


static void set_stream_info(ff_video_decoder_t *this) {
  _x_stream_info_set(this->stream, XINE_STREAM_INFO_VIDEO_WIDTH,  this->bih.biWidth);
  _x_stream_info_set(this->stream, XINE_STREAM_INFO_VIDEO_HEIGHT, this->bih.biHeight);
  _x_stream_info_set(this->stream, XINE_STREAM_INFO_VIDEO_RATIO,  this->aspect_ratio * 10000);
}

#ifdef ENABLE_DIRECT_RENDERING
/* called from ffmpeg to do direct rendering method 1 */
static int get_buffer(AVCodecContext *context, AVFrame *av_frame){
  ff_video_decoder_t *this = (ff_video_decoder_t *)context->opaque;
  vo_frame_t *img;
  int width  = context->width;
  int height = context->height;
        
  if (!this->bih.biWidth || !this->bih.biHeight) {
    this->bih.biWidth = width;
    this->bih.biHeight = height;

    if (this->aspect_ratio_prio == 0) {
      this->aspect_ratio = (double)width / (double)height;
      this->aspect_ratio_prio = 1;
      lprintf("default aspect ratio: %f\n", this->aspect_ratio);
      set_stream_info(this);
    }
  }
  
  avcodec_align_dimensions(context, &width, &height);

  if( this->context->pix_fmt != PIX_FMT_YUV420P ) {
    if (!this->is_direct_rendering_disabled) {
      xprintf(this->stream->xine, XINE_VERBOSITY_LOG, 
              _("ffmpeg_video_dec: unsupported frame format, DR1 disabled.\n"));
      this->is_direct_rendering_disabled = 1;
    }

    /* FIXME: why should i have to do that ? */
    av_frame->data[0]= NULL;
    av_frame->data[1]= NULL;
    av_frame->data[2]= NULL;
    return avcodec_default_get_buffer(context, av_frame);
  }
  
  if((width != this->bih.biWidth) || (height != this->bih.biHeight)) {
    if(this->stream->video_out->get_capabilities(this->stream->video_out) & VO_CAP_CROP) {
      this->crop_right = width - this->bih.biWidth;
      this->crop_bottom = height - this->bih.biHeight;
    } else {
      if (!this->is_direct_rendering_disabled) {
        xprintf(this->stream->xine, XINE_VERBOSITY_LOG, 
                _("ffmpeg_video_dec: unsupported frame dimensions, DR1 disabled.\n"));
        this->is_direct_rendering_disabled = 1;
      }
      /* FIXME: why should i have to do that ? */
      av_frame->data[0]= NULL;
      av_frame->data[1]= NULL;
      av_frame->data[2]= NULL;
      return avcodec_default_get_buffer(context, av_frame);
    }
  }

  img = this->stream->video_out->get_frame (this->stream->video_out,
                                            width,
                                            height,
                                            this->aspect_ratio, 
                                            this->output_format,
                                            VO_BOTH_FIELDS|this->frame_flags);

  av_frame->opaque = img;

  av_frame->data[0]= img->base[0];
  av_frame->data[1]= img->base[1];
  av_frame->data[2]= img->base[2];

  av_frame->linesize[0] = img->pitches[0];
  av_frame->linesize[1] = img->pitches[1];
  av_frame->linesize[2] = img->pitches[2];

  /* We should really keep track of the ages of xine frames (see
   * avcodec_default_get_buffer in libavcodec/utils.c)
   * For the moment tell ffmpeg that every frame is new (age = bignumber) */
  av_frame->age = 256*256*256*64;

  av_frame->type= FF_BUFFER_TYPE_USER;

  xine_list_push_back(this->dr1_frames, av_frame);

  return 0;
}

static void release_buffer(struct AVCodecContext *context, AVFrame *av_frame){
  ff_video_decoder_t *this = (ff_video_decoder_t *)context->opaque;

  if (av_frame->type == FF_BUFFER_TYPE_USER) {
    vo_frame_t *img = (vo_frame_t *)av_frame->opaque;
    xine_list_iterator_t it;
    
    assert(av_frame->opaque);  
    img->free(img);
    
    it = xine_list_find(this->dr1_frames, av_frame);
    assert(it);
    if( it != NULL )
      xine_list_remove(this->dr1_frames, it);
  } else {
    avcodec_default_release_buffer(context, av_frame);
  }

  av_frame->opaque = NULL;
  av_frame->data[0]= NULL;
  av_frame->data[1]= NULL;
  av_frame->data[2]= NULL;
}
#endif

static const ff_codec_t ff_video_lookup[] = {
  {BUF_VIDEO_MSMPEG4_V1,  CODEC_ID_MSMPEG4V1, "Microsoft MPEG-4 v1 (ffmpeg)"},
  {BUF_VIDEO_MSMPEG4_V2,  CODEC_ID_MSMPEG4V2, "Microsoft MPEG-4 v2 (ffmpeg)"},
  {BUF_VIDEO_MSMPEG4_V3,  CODEC_ID_MSMPEG4V3, "Microsoft MPEG-4 v3 (ffmpeg)"},
  {BUF_VIDEO_WMV7,        CODEC_ID_WMV1,      "MS Windows Media Video 7 (ffmpeg)"},
  {BUF_VIDEO_WMV8,        CODEC_ID_WMV2,      "MS Windows Media Video 8 (ffmpeg)"},
  {BUF_VIDEO_WMV9,        CODEC_ID_WMV3,      "MS Windows Media Video 9 (ffmpeg)"},
  {BUF_VIDEO_MPEG4,       CODEC_ID_MPEG4,     "ISO MPEG-4 (ffmpeg)"},
  {BUF_VIDEO_XVID,        CODEC_ID_MPEG4,     "ISO MPEG-4 (XviD, ffmpeg)"},
  {BUF_VIDEO_DIVX5,       CODEC_ID_MPEG4,     "ISO MPEG-4 (DivX5, ffmpeg)"},
  {BUF_VIDEO_3IVX,        CODEC_ID_MPEG4,     "ISO MPEG-4 (3ivx, ffmpeg)"},
  {BUF_VIDEO_JPEG,        CODEC_ID_MJPEG,     "Motion JPEG (ffmpeg)"},
  {BUF_VIDEO_MJPEG,       CODEC_ID_MJPEG,     "Motion JPEG (ffmpeg)"},
  {BUF_VIDEO_MJPEG_B,     CODEC_ID_MJPEGB,    "Motion JPEG B (ffmpeg)"},
  {BUF_VIDEO_I263,        CODEC_ID_H263I,     "ITU H.263 (ffmpeg)"},
  {BUF_VIDEO_H263,        CODEC_ID_H263,      "H.263 (ffmpeg)"},
  {BUF_VIDEO_RV10,        CODEC_ID_RV10,      "Real Video 1.0 (ffmpeg)"},
  {BUF_VIDEO_RV20,        CODEC_ID_RV20,      "Real Video 2.0 (ffmpeg)"},
  {BUF_VIDEO_IV31,        CODEC_ID_INDEO3,    "Indeo Video 3.1 (ffmpeg)"},
  {BUF_VIDEO_IV32,        CODEC_ID_INDEO3,    "Indeo Video 3.2 (ffmpeg)"},
  {BUF_VIDEO_SORENSON_V1, CODEC_ID_SVQ1,      "Sorenson Video 1 (ffmpeg)"},
  {BUF_VIDEO_SORENSON_V3, CODEC_ID_SVQ3,      "Sorenson Video 3 (ffmpeg)"},
  {BUF_VIDEO_DV,          CODEC_ID_DVVIDEO,   "DV (ffmpeg)"},
  {BUF_VIDEO_HUFFYUV,     CODEC_ID_HUFFYUV,   "HuffYUV (ffmpeg)"},
  {BUF_VIDEO_VP31,        CODEC_ID_VP3,       "On2 VP3.1 (ffmpeg)"},
  {BUF_VIDEO_VP5,         CODEC_ID_VP5,       "On2 VP5 (ffmpeg)"},
  {BUF_VIDEO_VP6,         CODEC_ID_VP6,       "On2 VP6 (ffmpeg)"},
  {BUF_VIDEO_VP6F,        CODEC_ID_VP6F,      "On2 VP6 (ffmpeg)"},
  {BUF_VIDEO_4XM,         CODEC_ID_4XM,       "4X Video (ffmpeg)"},
  {BUF_VIDEO_CINEPAK,     CODEC_ID_CINEPAK,   "Cinepak (ffmpeg)"},
  {BUF_VIDEO_MSVC,        CODEC_ID_MSVIDEO1,  "Microsoft Video 1 (ffmpeg)"},
  {BUF_VIDEO_MSRLE,       CODEC_ID_MSRLE,     "Microsoft RLE (ffmpeg)"},
  {BUF_VIDEO_RPZA,        CODEC_ID_RPZA,      "Apple Quicktime Video/RPZA (ffmpeg)"},
  {BUF_VIDEO_CYUV,        CODEC_ID_CYUV,      "Creative YUV (ffmpeg)"},
  {BUF_VIDEO_ROQ,         CODEC_ID_ROQ,       "Id Software RoQ (ffmpeg)"},
  {BUF_VIDEO_IDCIN,       CODEC_ID_IDCIN,     "Id Software CIN (ffmpeg)"},
  {BUF_VIDEO_WC3,         CODEC_ID_XAN_WC3,   "Xan (ffmpeg)"},
  {BUF_VIDEO_VQA,         CODEC_ID_WS_VQA,    "Westwood Studios VQA (ffmpeg)"},
  {BUF_VIDEO_INTERPLAY,   CODEC_ID_INTERPLAY_VIDEO, "Interplay MVE (ffmpeg)"},
  {BUF_VIDEO_FLI,         CODEC_ID_FLIC,      "FLIC Video (ffmpeg)"},
  {BUF_VIDEO_8BPS,        CODEC_ID_8BPS,      "Planar RGB (ffmpeg)"},
  {BUF_VIDEO_SMC,         CODEC_ID_SMC,       "Apple Quicktime Graphics/SMC (ffmpeg)"},
  {BUF_VIDEO_DUCKTM1,     CODEC_ID_TRUEMOTION1,"Duck TrueMotion v1 (ffmpeg)"},
  {BUF_VIDEO_DUCKTM2,     CODEC_ID_TRUEMOTION2,"Duck TrueMotion v2 (ffmpeg)"},
  {BUF_VIDEO_VMD,         CODEC_ID_VMDVIDEO,   "Sierra VMD Video (ffmpeg)"},
  {BUF_VIDEO_ZLIB,        CODEC_ID_ZLIB,       "ZLIB Video (ffmpeg)"},
  {BUF_VIDEO_MSZH,        CODEC_ID_MSZH,       "MSZH Video (ffmpeg)"},
  {BUF_VIDEO_ASV1,        CODEC_ID_ASV1,       "ASV v1 Video (ffmpeg)"},
  {BUF_VIDEO_ASV2,        CODEC_ID_ASV2,       "ASV v2 Video (ffmpeg)"},
  {BUF_VIDEO_ATIVCR1,     CODEC_ID_VCR1,       "ATI VCR-1 (ffmpeg)"},
  {BUF_VIDEO_FLV1,        CODEC_ID_FLV1,       "Flash Video (ffmpeg)"},
  {BUF_VIDEO_QTRLE,       CODEC_ID_QTRLE,      "Apple Quicktime Animation/RLE (ffmpeg)"},
  {BUF_VIDEO_H264,        CODEC_ID_H264,       "H.264/AVC (ffmpeg)"},
  {BUF_VIDEO_H261,        CODEC_ID_H261,       "H.261 (ffmpeg)"},
  {BUF_VIDEO_AASC,        CODEC_ID_AASC,       "Autodesk Video (ffmpeg)"},
  {BUF_VIDEO_LOCO,        CODEC_ID_LOCO,       "LOCO (ffmpeg)"},
  {BUF_VIDEO_QDRW,        CODEC_ID_QDRAW,      "QuickDraw (ffmpeg)"},
  {BUF_VIDEO_QPEG,        CODEC_ID_QPEG,       "Q-Team QPEG (ffmpeg)"},
  {BUF_VIDEO_TSCC,        CODEC_ID_TSCC,       "TechSmith Video (ffmpeg)"},
  {BUF_VIDEO_ULTI,        CODEC_ID_ULTI,       "IBM UltiMotion (ffmpeg)"},
  {BUF_VIDEO_WNV1,        CODEC_ID_WNV1,       "Winnow Video (ffmpeg)"},
  {BUF_VIDEO_XL,          CODEC_ID_VIXL,       "Miro/Pinnacle VideoXL (ffmpeg)"},
  {BUF_VIDEO_RT21,        CODEC_ID_INDEO2,     "Indeo/RealTime 2 (ffmpeg)"},
  {BUF_VIDEO_FPS1,        CODEC_ID_FRAPS,      "Fraps (ffmpeg)"},
  {BUF_VIDEO_MPEG,        CODEC_ID_MPEG1VIDEO, "MPEG 1/2 (ffmpeg)"},
  {BUF_VIDEO_CSCD,        CODEC_ID_CSCD,       "CamStudio (ffmpeg)"},
  {BUF_VIDEO_AVS,         CODEC_ID_AVS,        "AVS (ffmpeg)"},
  {BUF_VIDEO_ALGMM,       CODEC_ID_MMVIDEO,    "American Laser Games MM (ffmpeg)"},
  {BUF_VIDEO_ZMBV,        CODEC_ID_ZMBV,       "Zip Motion Blocks Video (ffmpeg)"},
  {BUF_VIDEO_SMACKER,     CODEC_ID_SMACKVIDEO, "Smacker (ffmpeg)"},
  {BUF_VIDEO_NUV,         CODEC_ID_NUV,        "NuppelVideo (ffmpeg)"},
  {BUF_VIDEO_KMVC,        CODEC_ID_KMVC,       "Karl Morton's Video Codec (ffmpeg)"},
  {BUF_VIDEO_FLASHSV,     CODEC_ID_FLASHSV,    "Flash Screen Video (ffmpeg)"},
  {BUF_VIDEO_CAVS,        CODEC_ID_CAVS,       "Chinese AVS (ffmpeg)"},
};


static void init_video_codec (ff_video_decoder_t *this, unsigned int codec_type) {
  size_t i;

  /* find the decoder */
  this->codec = NULL;

  for(i = 0; i < sizeof(ff_video_lookup)/sizeof(ff_codec_t); i++)
    if(ff_video_lookup[i].type == codec_type) {
      pthread_mutex_lock(&ffmpeg_lock);
      this->codec = avcodec_find_decoder(ff_video_lookup[i].id);
      pthread_mutex_unlock(&ffmpeg_lock);
      _x_meta_info_set_utf8(this->stream, XINE_META_INFO_VIDEOCODEC,
                            ff_video_lookup[i].name);
      break;
    }

  if (!this->codec) {
    xprintf (this->stream->xine, XINE_VERBOSITY_LOG, 
            _("ffmpeg_video_dec: couldn't find ffmpeg decoder for buf type 0x%X\n"),
            codec_type);
    _x_stream_info_set(this->stream, XINE_STREAM_INFO_VIDEO_HANDLED, 0);
    return;
  }

  lprintf("lavc decoder found\n");

  /* force (width % 8 == 0), otherwise there will be 
   * display problems with Xv. 
   */ 
  this->bih.biWidth = (this->bih.biWidth + 1) & (~1);

  this->context->width = this->bih.biWidth;
  this->context->height = this->bih.biHeight;
  this->context->stream_codec_tag = this->context->codec_tag = 
    _x_stream_info_get(this->stream, XINE_STREAM_INFO_VIDEO_FOURCC);


  /* Some codecs (eg rv10) copy flags in init so it's necessary to set
   * this flag here in case we are going to use direct rendering */
  if(this->codec->capabilities & CODEC_CAP_DR1) {
    this->context->flags |= CODEC_FLAG_EMU_EDGE;
  }
 
  pthread_mutex_lock(&ffmpeg_lock);
  if (avcodec_open (this->context, this->codec) < 0) {
    pthread_mutex_unlock(&ffmpeg_lock);
    xprintf (this->stream->xine, XINE_VERBOSITY_LOG, 
             _("ffmpeg_video_dec: couldn't open decoder\n"));
    free(this->context);
    this->context = NULL;
    _x_stream_info_set(this->stream, XINE_STREAM_INFO_VIDEO_HANDLED, 0);
    return;
  }
  pthread_mutex_unlock(&ffmpeg_lock);

  lprintf("lavc decoder opened\n");

  this->decoder_ok = 1;

  if ((codec_type != BUF_VIDEO_MPEG) &&
      (codec_type != BUF_VIDEO_DV)) {

    if (!this->bih.biWidth || !this->bih.biHeight) {
      this->bih.biWidth = this->context->width;
      this->bih.biHeight = this->context->height;
    }


    set_stream_info(this);
  }

  this->stream->video_out->open (this->stream->video_out, this->stream);

  this->skipframes = 0;
  
  /* enable direct rendering by default */
  this->output_format = XINE_IMGFMT_YV12;
#ifdef ENABLE_DIRECT_RENDERING
  if( this->codec->capabilities & CODEC_CAP_DR1 && this->codec->id != CODEC_ID_H264 ) {
    this->context->get_buffer = get_buffer;
    this->context->release_buffer = release_buffer;
    xprintf(this->stream->xine, XINE_VERBOSITY_LOG, 
	    _("ffmpeg_video_dec: direct rendering enabled\n"));
  }
#endif

  /* flag for interlaced streams */
  this->frame_flags = 0;
  /* FIXME: which codecs can be interlaced?
      FIXME: check interlaced DCT and other codec specific info. */
  switch( codec_type ) {
    case BUF_VIDEO_DV:
      this->frame_flags |= VO_INTERLACED_FLAG;
      break;
    case BUF_VIDEO_MPEG:
      this->frame_flags |= VO_INTERLACED_FLAG;
      break;
    case BUF_VIDEO_MJPEG:
      this->frame_flags |= VO_INTERLACED_FLAG;
      break;
    case BUF_VIDEO_HUFFYUV:
      this->frame_flags |= VO_INTERLACED_FLAG;
      break;
  }

}

static void pp_quality_cb(void *user_data, xine_cfg_entry_t *entry) {
  ff_video_class_t   *class = (ff_video_class_t *) user_data;
  
  class->pp_quality = entry->num_value;
}

static void pp_change_quality (ff_video_decoder_t *this) {
  this->pp_quality = this->class->pp_quality;

  if(this->pp_available && this->pp_quality) {
    if(!this->pp_context && this->context)
      this->pp_context = pp_get_context(this->context->width, this->context->height,
                                        this->pp_flags);
    if(this->pp_mode)
      pp_free_mode(this->pp_mode);
      
    this->pp_mode = pp_get_mode_by_name_and_quality("hb:a,vb:a,dr:a", 
                                                    this->pp_quality);
  } else {
    if(this->pp_mode) {
      pp_free_mode(this->pp_mode);
      this->pp_mode = NULL;
    }
    
    if(this->pp_context) {
      pp_free_context(this->pp_context);
      this->pp_context = NULL;
    }
  }
}

static void init_postprocess (ff_video_decoder_t *this) {
  uint32_t cpu_caps;

  /* Allow post processing on mpeg-4 (based) codecs */
  switch(this->codec->id) {
    case CODEC_ID_MPEG4:
    case CODEC_ID_MSMPEG4V1:
    case CODEC_ID_MSMPEG4V2:
    case CODEC_ID_MSMPEG4V3:
    case CODEC_ID_WMV1:
    case CODEC_ID_WMV2:
      this->pp_available = 1;
      break;
    default:
      this->pp_available = 0;
      break;
  }
  
  /* Detect what cpu accel we have */
  cpu_caps = xine_mm_accel();
  this->pp_flags = PP_FORMAT_420;
 
  if(cpu_caps & MM_ACCEL_X86_MMX)
    this->pp_flags |= PP_CPU_CAPS_MMX;
    
  if(cpu_caps & MM_ACCEL_X86_MMXEXT)
    this->pp_flags |= PP_CPU_CAPS_MMX2;
  
  if(cpu_caps & MM_ACCEL_X86_3DNOW)  
    this->pp_flags |= PP_CPU_CAPS_3DNOW;
   
  /* Set level */
  pp_change_quality(this);    
}

static int ff_handle_mpeg_sequence(ff_video_decoder_t *this, mpeg_parser_t *parser) {

  /*
   * init codec
   */
  if (this->decoder_init_mode) {
    _x_meta_info_set_utf8(this->stream, XINE_META_INFO_VIDEOCODEC, 
                          "mpeg-1 (ffmpeg)");

    init_video_codec (this, BUF_VIDEO_MPEG);
    this->decoder_init_mode = 0;
  }
  
  /* frame format change */
  if ((parser->width != this->bih.biWidth) ||
      (parser->height != this->bih.biHeight) ||
      (parser->frame_aspect_ratio != this->aspect_ratio)) {
    xine_event_t event;
    xine_format_change_data_t data;

    this->bih.biWidth  = parser->width;
    this->bih.biHeight = parser->height;
    this->aspect_ratio = parser->frame_aspect_ratio;
    this->aspect_ratio_prio = 2;
    lprintf("mpeg seq aspect ratio: %f\n", this->aspect_ratio);
    set_stream_info(this);

    event.type = XINE_EVENT_FRAME_FORMAT_CHANGE;
    event.stream = this->stream;
    event.data = &data;
    event.data_length = sizeof(data);
    data.width = this->bih.biWidth;
    data.height = this->bih.biHeight;
    data.aspect = this->aspect_ratio;
    data.pan_scan = 0;
    xine_event_send(this->stream, &event);
  }
  this->video_step = this->mpeg_parser->frame_duration;
  
  return 1;
}

static void ff_convert_frame(ff_video_decoder_t *this, vo_frame_t *img) {
  int         y;
  uint8_t    *dy, *du, *dv, *sy, *su, *sv;

  dy = img->base[0];
  du = img->base[1];
  dv = img->base[2];
  sy = this->av_frame->data[0];
  su = this->av_frame->data[1];
  sv = this->av_frame->data[2];

  if (this->context->pix_fmt == PIX_FMT_YUV410P) {

    yuv9_to_yv12(
     /* Y */
      this->av_frame->data[0],
      this->av_frame->linesize[0],
      img->base[0],
      img->pitches[0],
     /* U */
      this->av_frame->data[1],
      this->av_frame->linesize[1],
      img->base[1],
      img->pitches[1],
     /* V */
      this->av_frame->data[2],
      this->av_frame->linesize[2],
      img->base[2],
      img->pitches[2],
     /* width x height */
      img->width,
      img->height);

  } else if (this->context->pix_fmt == PIX_FMT_YUV411P) {

    yuv411_to_yv12(
     /* Y */
      this->av_frame->data[0],
      this->av_frame->linesize[0],
      img->base[0],
      img->pitches[0],
     /* U */
      this->av_frame->data[1],
      this->av_frame->linesize[1],
      img->base[1],
      img->pitches[1],
     /* V */
      this->av_frame->data[2],
      this->av_frame->linesize[2],
      img->base[2],
      img->pitches[2],
     /* width x height */
      img->width,
      img->height);

  } else if (this->context->pix_fmt == PIX_FMT_RGBA32) {
          
    int x, plane_ptr = 0;
    uint32_t *argb_pixels;
    uint32_t argb;

    for(y = 0; y < img->height; y++) {
      argb_pixels = (uint32_t *)sy;
      for(x = 0; x < img->width; x++) {
        uint8_t r, g, b;
              
        /* this is endian-safe as the ARGB pixels are stored in
         * machine order */
        argb = *argb_pixels++;
        r = (argb >> 16) & 0xFF;
        g = (argb >>  8) & 0xFF;
        b = (argb >>  0) & 0xFF;

        this->yuv.y[plane_ptr] = COMPUTE_Y(r, g, b);
        this->yuv.u[plane_ptr] = COMPUTE_U(r, g, b);
        this->yuv.v[plane_ptr] = COMPUTE_V(r, g, b);
        plane_ptr++;
      }
      sy += this->av_frame->linesize[0];
    }
            
    yuv444_to_yuy2(&this->yuv, img->base[0], img->pitches[0]);
          
  } else if (this->context->pix_fmt == PIX_FMT_RGB565) {

    int x, plane_ptr = 0;
    uint8_t *src;
    uint16_t pixel16;

    for(y = 0; y < img->height; y++) {
      src = sy;
      for(x = 0; x < img->width; x++) {
        uint8_t r, g, b;
              
        /* a 16-bit RGB565 pixel is supposed to be stored in native-endian
         * byte order; the following should be endian-safe */
        pixel16 = *((uint16_t *)src);
        src += 2;
        b = (pixel16 << 3) & 0xFF;
        g = (pixel16 >> 3) & 0xFF;
        r = (pixel16 >> 8) & 0xFF;

        this->yuv.y[plane_ptr] = COMPUTE_Y(r, g, b);
        this->yuv.u[plane_ptr] = COMPUTE_U(r, g, b);
        this->yuv.v[plane_ptr] = COMPUTE_V(r, g, b);
        plane_ptr++;
      }
      sy += this->av_frame->linesize[0];
    }
            
    yuv444_to_yuy2(&this->yuv, img->base[0], img->pitches[0]);
          
  } else if (this->context->pix_fmt == PIX_FMT_RGB555) {
          
    int x, plane_ptr = 0;
    uint8_t *src;
    uint16_t pixel16;
            
    for(y = 0; y < img->height; y++) {
      src = sy;
      for(x = 0; x < img->width; x++) {
        uint8_t r, g, b;
              
        /* a 16-bit RGB555 pixel is supposed to be stored in native-endian
         * byte order; the following should be endian-safe */
        pixel16 = *((uint16_t *)src);
        src += 2;
        b = (pixel16 << 3) & 0xFF;
        g = (pixel16 >> 2) & 0xFF;
        r = (pixel16 >> 7) & 0xFF;

        this->yuv.y[plane_ptr] = COMPUTE_Y(r, g, b);
        this->yuv.u[plane_ptr] = COMPUTE_U(r, g, b);
        this->yuv.v[plane_ptr] = COMPUTE_V(r, g, b);
        plane_ptr++;
      }
      sy += this->av_frame->linesize[0];
    }
            
    yuv444_to_yuy2(&this->yuv, img->base[0], img->pitches[0]);
          
  } else if (this->context->pix_fmt == PIX_FMT_BGR24) {

    int x, plane_ptr = 0;
    uint8_t *src;

    for(y = 0; y < img->height; y++) {
      src = sy;
      for(x = 0; x < img->width; x++) {
        uint8_t r, g, b;
              
        b = *src++;
        g = *src++;
        r = *src++;

        this->yuv.y[plane_ptr] = COMPUTE_Y(r, g, b);
        this->yuv.u[plane_ptr] = COMPUTE_U(r, g, b);
        this->yuv.v[plane_ptr] = COMPUTE_V(r, g, b);
        plane_ptr++;
      }
      sy += this->av_frame->linesize[0];
    }
            
    yuv444_to_yuy2(&this->yuv, img->base[0], img->pitches[0]);
          
  } else if (this->context->pix_fmt == PIX_FMT_RGB24) {

    int x, plane_ptr = 0;
    uint8_t *src;

    for(y = 0; y < img->height; y++) {
      src = sy;
      for(x = 0; x < img->width; x++) {
        uint8_t r, g, b;
              
        r = *src++;
        g = *src++;
        b = *src++;

        this->yuv.y[plane_ptr] = COMPUTE_Y(r, g, b);
        this->yuv.u[plane_ptr] = COMPUTE_U(r, g, b);
        this->yuv.v[plane_ptr] = COMPUTE_V(r, g, b);
        plane_ptr++;
      }
      sy += this->av_frame->linesize[0];
    }
            
    yuv444_to_yuy2(&this->yuv, img->base[0], img->pitches[0]);
          
  } else if (this->context->pix_fmt == PIX_FMT_PAL8) {
          
    int x, plane_ptr = 0;
    uint8_t *src;
    uint8_t pixel;
    uint32_t *palette32 = (uint32_t *)su;  /* palette is in data[1] */
    uint32_t rgb_color;
    uint8_t r, g, b;
    uint8_t y_palette[256];
    uint8_t u_palette[256];
    uint8_t v_palette[256];

    for (x = 0; x < 256; x++) {
      rgb_color = palette32[x];
      b = rgb_color & 0xFF;
      rgb_color >>= 8;
      g = rgb_color & 0xFF;
      rgb_color >>= 8;
      r = rgb_color & 0xFF;
      y_palette[x] = COMPUTE_Y(r, g, b);
      u_palette[x] = COMPUTE_U(r, g, b);
      v_palette[x] = COMPUTE_V(r, g, b);
    }

    for(y = 0; y < img->height; y++) {
      src = sy;
      for(x = 0; x < img->width; x++) {
        pixel = *src++;

        this->yuv.y[plane_ptr] = y_palette[pixel];
        this->yuv.u[plane_ptr] = u_palette[pixel];
        this->yuv.v[plane_ptr] = v_palette[pixel];
        plane_ptr++;
      }
      sy += this->av_frame->linesize[0];
    }
            
    yuv444_to_yuy2(&this->yuv, img->base[0], img->pitches[0]);
          
  } else {
          
    for (y=0; y<img->height; y++) {
      xine_fast_memcpy (dy, sy, img->width);
  
      dy += img->pitches[0];
  
      sy += this->av_frame->linesize[0];
    }

    for (y=0; y<(img->height/2); y++) {
      
      if (this->context->pix_fmt != PIX_FMT_YUV444P) {
        
        xine_fast_memcpy (du, su, img->width/2);
        xine_fast_memcpy (dv, sv, img->width/2);
        
      } else {
        
        int x;
        uint8_t *src;
        uint8_t *dst;
      
        /* subsample */
        
        src = su; dst = du;
        for (x=0; x<(img->width/2); x++) {
          *dst = *src;
          dst++;
          src += 2;
        }
        src = sv; dst = dv;
        for (x=0; x<(img->width/2); x++) {
          *dst = *src;
          dst++;
          src += 2;
        }

      }
  
      du += img->pitches[1];
      dv += img->pitches[2];

      if (this->context->pix_fmt != PIX_FMT_YUV420P) {
        su += 2*this->av_frame->linesize[1];
        sv += 2*this->av_frame->linesize[2];
      } else {
        su += this->av_frame->linesize[1];
        sv += this->av_frame->linesize[2];
      }
    }
  }
}

static void ff_check_bufsize (ff_video_decoder_t *this, int size) {
  if (size > this->bufsize) {
    this->bufsize = size + size / 2;
    xprintf(this->stream->xine, XINE_VERBOSITY_LOG, 
	    _("ffmpeg_video_dec: increasing buffer to %d to avoid overflow.\n"), 
	    this->bufsize);
    this->buf = realloc(this->buf, this->bufsize + FF_INPUT_BUFFER_PADDING_SIZE );
  }
}

static void ff_handle_preview_buffer (ff_video_decoder_t *this, buf_element_t *buf) {
  int codec_type;

  lprintf ("preview buffer\n");

  codec_type = buf->type & 0xFFFF0000;
  if (codec_type == BUF_VIDEO_MPEG) {
    this->is_mpeg12 = 1;
    if ( this->mpeg_parser == NULL ) {
      this->mpeg_parser = xine_xmalloc(sizeof(mpeg_parser_t));
      mpeg_parser_init(this->mpeg_parser);
      this->decoder_init_mode = 0;
    }
  }

  if (this->decoder_init_mode && !this->is_mpeg12) {
    init_video_codec(this, codec_type);
    init_postprocess(this);
    this->decoder_init_mode = 0;
  }
}

static void ff_handle_header_buffer (ff_video_decoder_t *this, buf_element_t *buf) {

  lprintf ("header buffer\n");

  /* accumulate data */
  ff_check_bufsize(this, this->size + buf->size);
  xine_fast_memcpy (&this->buf[this->size], buf->content, buf->size);
  this->size += buf->size;

  if (buf->decoder_flags & BUF_FLAG_FRAME_END) {
    int codec_type;

    lprintf ("header complete\n");
    codec_type = buf->type & 0xFFFF0000;

    if (buf->decoder_flags & BUF_FLAG_STDHEADER) {

      lprintf("standard header\n");
    
      /* init package containing bih */
      memcpy ( &this->bih, this->buf, sizeof(xine_bmiheader) );

      if (this->bih.biSize > sizeof(xine_bmiheader)) {
      this->context->extradata_size = this->bih.biSize - sizeof(xine_bmiheader);
        this->context->extradata = malloc(this->context->extradata_size + 
                                          FF_INPUT_BUFFER_PADDING_SIZE);
        memcpy(this->context->extradata, this->buf + sizeof(xine_bmiheader),
              this->context->extradata_size);
      }
      
      this->context->bits_per_sample = this->bih.biBitCount;
            
    } else {
    
      switch (codec_type) {
      case BUF_VIDEO_RV10:
      case BUF_VIDEO_RV20:
        this->bih.biWidth  = _X_BE_16(&this->buf[12]);
        this->bih.biHeight = _X_BE_16(&this->buf[14]);
        
        this->context->sub_id = _X_BE_32(&this->buf[30]);

        this->context->slice_offset = xine_xmalloc(sizeof(int)*SLICE_OFFSET_SIZE);
        this->slice_offset_size = SLICE_OFFSET_SIZE;

	lprintf("w=%d, h=%d\n", this->bih.biWidth, this->bih.biHeight);

        break;
      default:
        xprintf(this->stream->xine, XINE_VERBOSITY_DEBUG,
                "ffmpeg_video_dec: unknown header for buf type 0x%X\n", codec_type);
        return;
      }
    }

    /* reset accumulator */
    this->size = 0;
  }
}

static void ff_handle_special_buffer (ff_video_decoder_t *this, buf_element_t *buf) {
  /* take care of all the various types of special buffers 
  * note that order is important here */
  lprintf("special buffer\n");

  if (buf->decoder_info[1] == BUF_SPECIAL_STSD_ATOM &&
      !this->context->extradata_size) {

    lprintf("BUF_SPECIAL_STSD_ATOM\n");
    this->context->extradata_size = buf->decoder_info[2];
    this->context->extradata = xine_xmalloc(buf->decoder_info[2] + 
                                            FF_INPUT_BUFFER_PADDING_SIZE);
    memcpy(this->context->extradata, buf->decoder_info_ptr[2],
      buf->decoder_info[2]);

  } else if (buf->decoder_info[1] == BUF_SPECIAL_DECODER_CONFIG &&
            !this->context->extradata_size) {
    
    lprintf("BUF_SPECIAL_DECODER_CONFIG\n");
    this->context->extradata_size = buf->decoder_info[2];
    this->context->extradata = xine_xmalloc(buf->decoder_info[2] +
                                            FF_INPUT_BUFFER_PADDING_SIZE);
    memcpy(this->context->extradata, buf->decoder_info_ptr[2],
      buf->decoder_info[2]);
      
  } else if (buf->decoder_info[1] == BUF_SPECIAL_PALETTE) {
    unsigned int i;

    palette_entry_t *demuxer_palette;
    AVPaletteControl *decoder_palette;
    
    lprintf("BUF_SPECIAL_PALETTE\n");
    this->context->palctrl = &this->palette_control;
    decoder_palette = (AVPaletteControl *)this->context->palctrl;
    demuxer_palette = (palette_entry_t *)buf->decoder_info_ptr[2];

    for (i = 0; i < buf->decoder_info[2]; i++) {
      decoder_palette->palette[i] = 
        (demuxer_palette[i].r << 16) |
        (demuxer_palette[i].g <<  8) |
        (demuxer_palette[i].b <<  0);
    }
    decoder_palette->palette_changed = 1;

  } else if (buf->decoder_info[1] == BUF_SPECIAL_RV_CHUNK_TABLE) {
    int i;
  
    lprintf("BUF_SPECIAL_RV_CHUNK_TABLE\n");
    this->context->slice_count = buf->decoder_info[2]+1;

    lprintf("slice_count=%d\n", this->context->slice_count);
    
    if(this->context->slice_count > this->slice_offset_size) {
      this->context->slice_offset = realloc(this->context->slice_offset,
                                            sizeof(int)*this->context->slice_count);
      this->slice_offset_size = this->context->slice_count;
    }
    
    for(i = 0; i < this->context->slice_count; i++) {
      this->context->slice_offset[i] = 
        ((uint32_t *) buf->decoder_info_ptr[2])[(2*i)+1];
      lprintf("slice_offset[%d]=%d\n", i, this->context->slice_offset[i]);
    }
  }
}

static void ff_handle_mpeg12_buffer (ff_video_decoder_t *this, buf_element_t *buf) {

  vo_frame_t *img;
  int         free_img;
  int         got_picture, len;
  int         offset = 0;
  int         flush = 0;
  int         size = buf->size;

  lprintf("handle_mpeg12_buffer\n");

  while ((size > 0) || (flush == 1)) {

    uint8_t *current;
    int next_flush;

    got_picture = 0;
    if (!flush) {
      current = mpeg_parser_decode_data(this->mpeg_parser,
                                        buf->content + offset, buf->content + offset + size,
                                        &next_flush);
    } else {
      current = buf->content + offset + size; /* end of the buffer */
      next_flush = 0;
    }
    if (current == NULL) {
      lprintf("current == NULL\n");
      return;
    }

    if (this->mpeg_parser->has_sequence) {
      ff_handle_mpeg_sequence(this, this->mpeg_parser);
    }

    if (!this->decoder_ok)
      return;
    
    if (flush) {
      lprintf("flush lavc buffers\n");
      /* hack: ffmpeg outputs the last frame if size=0 */
      this->mpeg_parser->buffer_size = 0;
    }

    /* skip decoding b frames if too late */
    this->context->hurry_up = (this->skipframes > 0);

    lprintf("avcodec_decode_video: size=%d\n", this->mpeg_parser->buffer_size);
    len = avcodec_decode_video (this->context, this->av_frame,
                                &got_picture, this->mpeg_parser->chunk_buffer,
                                this->mpeg_parser->buffer_size);
    lprintf("avcodec_decode_video: decoded_size=%d, got_picture=%d\n",
            len, got_picture);
    len = current - buf->content - offset;
    lprintf("avcodec_decode_video: consumed_size=%d\n", len);
    
    flush = next_flush;

    if ((len < 0) || (len > buf->size)) {
      xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG, 
                "ffmpeg_video_dec: error decompressing frame\n");
      size = 0; /* draw a bad frame and exit */
    } else {
      size -= len;
      offset += len;
    }

    if (got_picture && this->av_frame->data[0]) {
      /* got a picture, draw it */
      if(!this->av_frame->opaque) {
        /* indirect rendering */
        img = this->stream->video_out->get_frame (this->stream->video_out,
                                                  this->bih.biWidth,
                                                  this->bih.biHeight,
                                                  this->aspect_ratio, 
                                                  this->output_format,
                                                  VO_BOTH_FIELDS|this->frame_flags);
        free_img = 1;
      } else {
        /* DR1 */
        img = (vo_frame_t*) this->av_frame->opaque;
        free_img = 0;
      }

      img->pts  = this->pts;
      this->pts = 0;

      if (this->av_frame->repeat_pict)
        img->duration = this->video_step * 3 / 2;
      else
        img->duration = this->video_step;

      img->crop_right  = this->crop_right;
      img->crop_bottom = this->crop_bottom;
      
      this->skipframes = img->draw(img, this->stream);

      if(free_img)
        img->free(img);

    } else {

      if (this->context->hurry_up) {
        /* skipped frame, output a bad frame */
        img = this->stream->video_out->get_frame (this->stream->video_out,
                                                  this->bih.biWidth,
                                                  this->bih.biHeight,
                                                  this->aspect_ratio, 
                                                  this->output_format,
                                                  VO_BOTH_FIELDS|this->frame_flags);
        img->pts       = 0;
        img->duration  = this->video_step;
        img->bad_frame = 1;
        this->skipframes = img->draw(img, this->stream);
        img->free(img);
      }
    }
  }
}

static void ff_handle_buffer (ff_video_decoder_t *this, buf_element_t *buf) {
  uint8_t *chunk_buf = this->buf;
  AVRational avr00 = {0, 1};

  lprintf("handle_buffer\n");

  if (!this->decoder_ok) {
    if (this->decoder_init_mode) {
      int codec_type = buf->type & 0xFFFF0000;

      /* init ffmpeg decoder */
      init_video_codec(this, codec_type);
      init_postprocess(this);
      this->decoder_init_mode = 0;
    } else {
      return;
    }
  }

  if (buf->decoder_flags & BUF_FLAG_FRAME_START) {
    lprintf("BUF_FLAG_FRAME_START\n");
    this->size = 0;
  }

  /* data accumulation */
  if (buf->size > 0) {
    if ((this->size == 0) &&
	((buf->size + FF_INPUT_BUFFER_PADDING_SIZE) < buf->max_size) && 
	(buf->decoder_flags & BUF_FLAG_FRAME_END)) {
      /* buf contains a complete frame */
      /* no memcpy needed */
      chunk_buf = buf->content;
      this->size = buf->size;
      lprintf("no memcpy needed to accumulate data\n");
    } else {
      /* copy data into our internal buffer */
      ff_check_bufsize(this, this->size + buf->size);
      chunk_buf = this->buf; /* ff_check_bufsize might realloc this->buf */

      xine_fast_memcpy (&this->buf[this->size], buf->content, buf->size);
      
      this->size += buf->size;
      lprintf("accumulate data into this->buf\n");
    }
  }

  if (buf->decoder_flags & BUF_FLAG_FRAME_END) {

    vo_frame_t *img;
    int         free_img;
    int         got_picture, len;
    int         got_one_picture = 0;
    int         offset = 0;
    int         codec_type = buf->type & 0xFFFF0000;

    /* pad input data */
    /* note: bitstream, alt bitstream reader or something will cause
     * severe mpeg4 artifacts if padding is less than 32 bits.
     */
    memset(&chunk_buf[this->size], 0, FF_INPUT_BUFFER_PADDING_SIZE);

    while (this->size > 0) {
      
      /* DV frames can be completely skipped */
      if( codec_type == BUF_VIDEO_DV && this->skipframes ) {
        this->size = 0;
        got_picture = 0;
      } else {
        /* skip decoding b frames if too late */
        this->context->hurry_up = (this->skipframes > 0);

        lprintf("buffer size: %d\n", this->size);
        len = avcodec_decode_video (this->context, this->av_frame,
                                    &got_picture, &chunk_buf[offset],
                                    this->size);
        lprintf("consumed size: %d, got_picture: %d\n", len, got_picture);
        if ((len <= 0) || (len > this->size)) {
          xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG, 
                    "ffmpeg_video_dec: error decompressing frame\n");
          this->size = 0;

        } else {

          offset += len;
          this->size -= len;

          if (this->size > 0) {
            ff_check_bufsize(this, this->size);
            memmove (this->buf, &chunk_buf[offset], this->size);
            chunk_buf = this->buf;
          }
        }
      }

      /* aspect ratio provided by ffmpeg, override previous setting */
      if ((this->aspect_ratio_prio < 2) &&
	  av_cmp_q(this->context->sample_aspect_ratio, avr00)) {

        if (!this->bih.biWidth || !this->bih.biHeight) {
          this->bih.biWidth  = this->context->width;
          this->bih.biHeight = this->context->height;
        }

	this->aspect_ratio = av_q2d(this->context->sample_aspect_ratio) * 
	  (double)this->bih.biWidth / (double)this->bih.biHeight;
	this->aspect_ratio_prio = 2;
	lprintf("ffmpeg aspect ratio: %f\n", this->aspect_ratio);
	set_stream_info(this);
      }

      if (got_picture && this->av_frame->data[0]) {
        /* got a picture, draw it */
        got_one_picture = 1;
        if(!this->av_frame->opaque) {
	  /* indirect rendering */

	  /* initialize the colorspace converter */
	  if (!this->cs_convert_init) {
	    if ((this->context->pix_fmt == PIX_FMT_RGBA32) ||
	        (this->context->pix_fmt == PIX_FMT_RGB565) ||
	        (this->context->pix_fmt == PIX_FMT_RGB555) ||
	        (this->context->pix_fmt == PIX_FMT_BGR24) ||
	        (this->context->pix_fmt == PIX_FMT_RGB24) ||
	        (this->context->pix_fmt == PIX_FMT_PAL8)) {
	      this->output_format = XINE_IMGFMT_YUY2;
	      init_yuv_planes(&this->yuv, this->bih.biWidth, this->bih.biHeight);
	      this->yuv_init = 1;
	    }
	    this->cs_convert_init = 1;
	  }

	  if (this->aspect_ratio_prio == 0) {
	    this->aspect_ratio = (double)this->bih.biWidth / (double)this->bih.biHeight;
	    this->aspect_ratio_prio = 1;
	    lprintf("default aspect ratio: %f\n", this->aspect_ratio);
	    set_stream_info(this);
	  }

          img = this->stream->video_out->get_frame (this->stream->video_out,
                                                    this->bih.biWidth,
                                                    this->bih.biHeight,
                                                    this->aspect_ratio, 
                                                    this->output_format,
                                                    VO_BOTH_FIELDS|this->frame_flags);
          free_img = 1;
        } else {
          /* DR1 */
          img = (vo_frame_t*) this->av_frame->opaque;
          free_img = 0;
        }

        /* post processing */
        if(this->pp_quality != this->class->pp_quality)
          pp_change_quality(this);

        if(this->pp_available && this->pp_quality) {

          if(this->av_frame->opaque) {
            /* DR1 */
            img = this->stream->video_out->get_frame (this->stream->video_out,
                                                      img->width,
                                                      img->height,
                                                      this->aspect_ratio, 
                                                      this->output_format,
                                                      VO_BOTH_FIELDS|this->frame_flags);
            free_img = 1;
          }

          pp_postprocess(this->av_frame->data, this->av_frame->linesize, 
                        img->base, img->pitches, 
                        img->width, img->height,
                        this->av_frame->qscale_table, this->av_frame->qstride,
                        this->pp_mode, this->pp_context, 
                        this->av_frame->pict_type);

        } else if (!this->av_frame->opaque) {
	  /* colorspace conversion or copy */
          ff_convert_frame(this, img);
        }

        img->pts  = this->pts;
        this->pts = 0;

        /* workaround for weird 120fps streams */
        if( this->video_step == 750 ) {
          /* fallback to the VIDEO_PTS_MODE */
          this->video_step = 0;
        }
        
        if (this->av_frame->repeat_pict)
          img->duration = this->video_step * 3 / 2;
        else
          img->duration = this->video_step;

        img->crop_right  = this->crop_right;
        img->crop_bottom = this->crop_bottom;
        
        this->skipframes = img->draw(img, this->stream);
        
        if(free_img)
          img->free(img);
      }
    }

    if (!got_one_picture) {
      /* skipped frame, output a bad frame */
      img = this->stream->video_out->get_frame (this->stream->video_out,
                                                this->bih.biWidth,
                                                this->bih.biHeight,
                                                this->aspect_ratio, 
                                                this->output_format,
                                                VO_BOTH_FIELDS|this->frame_flags);
      img->pts       = 0;
      img->duration  = this->video_step;
      img->bad_frame = 1;
      this->skipframes = img->draw(img, this->stream);
      img->free(img);
    }
  }
}

static void ff_decode_data (video_decoder_t *this_gen, buf_element_t *buf) {
  ff_video_decoder_t *this = (ff_video_decoder_t *) this_gen;

  lprintf ("processing packet type = %08x, len = %d, decoder_flags=%08x\n", 
           buf->type, buf->size, buf->decoder_flags);

  if (buf->decoder_flags & BUF_FLAG_FRAMERATE) {
    this->video_step = buf->decoder_info[0];
    _x_stream_info_set(this->stream, XINE_STREAM_INFO_FRAME_DURATION, this->video_step);
  }

  if (buf->decoder_flags & BUF_FLAG_PREVIEW) {
  
    ff_handle_preview_buffer(this, buf);

  } else {

    if (buf->decoder_flags & BUF_FLAG_SPECIAL) {

      ff_handle_special_buffer(this, buf);
		     
    }

    if (buf->decoder_flags & BUF_FLAG_HEADER) {

      ff_handle_header_buffer(this, buf);

      if (buf->decoder_flags & BUF_FLAG_ASPECT) {
	if (this->aspect_ratio_prio < 3) {
	  this->aspect_ratio = (double)buf->decoder_info[1] / (double)buf->decoder_info[2];
	  this->aspect_ratio_prio = 3;
	  lprintf("aspect ratio: %f\n", this->aspect_ratio);
	  set_stream_info(this);
	}
      }  

    } else {

      /* decode */
      if (buf->pts)
	this->pts = buf->pts;

      if (this->is_mpeg12) {
	ff_handle_mpeg12_buffer(this, buf);
      } else {
	ff_handle_buffer(this, buf);
      }

    }
  }
}

static void ff_flush (video_decoder_t *this_gen) {
  lprintf ("ff_flush\n");
}

static void ff_reset (video_decoder_t *this_gen) {
  ff_video_decoder_t *this = (ff_video_decoder_t *) this_gen;

  lprintf ("ff_reset\n");

  this->size = 0;

  if(this->context && this->decoder_ok)
    avcodec_flush_buffers(this->context);
  
  if (this->is_mpeg12)
    mpeg_parser_reset(this->mpeg_parser);
}

static void ff_discontinuity (video_decoder_t *this_gen) {
  ff_video_decoder_t *this = (ff_video_decoder_t *) this_gen;
  
  lprintf ("ff_discontinuity\n");
  this->pts = 0;
}

static void ff_dispose (video_decoder_t *this_gen) {
  ff_video_decoder_t *this = (ff_video_decoder_t *) this_gen;

  lprintf ("ff_dispose\n");
  
  if (this->decoder_ok) {
    xine_list_iterator_t it;
    AVFrame *av_frame;
        
    pthread_mutex_lock(&ffmpeg_lock);
    avcodec_close (this->context);
    pthread_mutex_unlock(&ffmpeg_lock);
    
    /* frame garbage collector here - workaround for buggy ffmpeg codecs that
     * don't release their DR1 frames */
    while( (it = xine_list_front(this->dr1_frames)) != NULL )
    {
      av_frame = (AVFrame *)xine_list_get_value(this->dr1_frames, it);
      release_buffer(this->context, av_frame);
    }
    
    this->stream->video_out->close(this->stream->video_out, this->stream);
    this->decoder_ok = 0;
  }

  if(this->context && this->context->slice_offset)
    free(this->context->slice_offset);

  if(this->context && this->context->extradata)
    free(this->context->extradata);

  if(this->yuv_init)
    free_yuv_planes(&this->yuv);
  
  if( this->context )
    free( this->context );

  if( this->av_frame )
    free( this->av_frame );
  
  if (this->buf)
    free(this->buf);
  this->buf = NULL;
  
  if(this->pp_context)
    pp_free_context(this->pp_context);
    
  if(this->pp_mode)
    pp_free_mode(this->pp_mode);

  mpeg_parser_dispose(this->mpeg_parser);
    
  xine_list_delete(this->dr1_frames);
  
  free (this_gen);
}

static video_decoder_t *ff_video_open_plugin (video_decoder_class_t *class_gen, xine_stream_t *stream) {

  ff_video_decoder_t  *this ;

  lprintf ("open_plugin\n");

  this = (ff_video_decoder_t *) xine_xmalloc (sizeof (ff_video_decoder_t));

  this->video_decoder.decode_data         = ff_decode_data;
  this->video_decoder.flush               = ff_flush;
  this->video_decoder.reset               = ff_reset;
  this->video_decoder.discontinuity       = ff_discontinuity;
  this->video_decoder.dispose             = ff_dispose;
  this->size                              = 0;

  this->stream                            = stream;
  this->class                             = (ff_video_class_t *) class_gen;

  this->av_frame          = avcodec_alloc_frame();
  this->context           = avcodec_alloc_context();
  this->context->opaque   = this;
  this->context->palctrl  = NULL;
  
  this->decoder_ok        = 0;
  this->decoder_init_mode = 1;
  this->buf               = xine_xmalloc(VIDEOBUFSIZE + FF_INPUT_BUFFER_PADDING_SIZE);
  this->bufsize           = VIDEOBUFSIZE;

  this->is_mpeg12         = 0;
  this->aspect_ratio      = 0;

  this->pp_quality        = 0;
  this->pp_context        = NULL;
  this->pp_mode           = NULL;
  
  this->mpeg_parser       = NULL;
  
  this->dr1_frames        = xine_list_new();

  return &this->video_decoder;
}

static char *ff_video_get_identifier (video_decoder_class_t *this) {
  return "ffmpeg video";
}

static char *ff_video_get_description (video_decoder_class_t *this) {
  return "ffmpeg based video decoder plugin";
}

static void ff_video_dispose_class (video_decoder_class_t *this) {
  free (this);
}

void *init_video_plugin (xine_t *xine, void *data) {

  ff_video_class_t *this;
  config_values_t  *config;
  
  this = (ff_video_class_t *) xine_xmalloc (sizeof (ff_video_class_t));

  this->decoder_class.open_plugin     = ff_video_open_plugin;
  this->decoder_class.get_identifier  = ff_video_get_identifier;
  this->decoder_class.get_description = ff_video_get_description;
  this->decoder_class.dispose         = ff_video_dispose_class;
  this->xine                          = xine;

  pthread_once( &once_control, init_once_routine );
  
  /* Configuration for post processing quality - default to mid (3) for the
   * moment */
  config = xine->config;
  
  this->pp_quality = xine->config->register_range(config, "video.processing.ffmpeg_pp_quality", 3, 
    0, PP_QUALITY_MAX,
    _("MPEG-4 postprocessing quality"),
    _("You can adjust the amount of post processing applied to MPEG-4 video.\n"
      "Higher values result in better quality, but need more CPU. Lower values may "
      "result in image defects like block artifacts. For high quality content, "
      "too heavy post processing can actually make the image worse by blurring it "
      "too much."),
    10, pp_quality_cb, this);
  
  return this;
}

static uint32_t supported_video_types[] = { 
#if defined(HAVE_FFMPEG) || CONFIG_MSMPEG4V1_DECODER
  BUF_VIDEO_MSMPEG4_V1,
#endif
#if defined(HAVE_FFMPEG) || CONFIG_MSMPEG4V2_DECODER
  BUF_VIDEO_MSMPEG4_V2,
#endif
#if defined(HAVE_FFMPEG) || CONFIG_MSMPEG4V3_DECODER
  BUF_VIDEO_MSMPEG4_V3,
#endif
#if defined(HAVE_FFMPEG) || CONFIG_WMV1_DECODER
  BUF_VIDEO_WMV7,
#endif
#if defined(HAVE_FFMPEG) || CONFIG_WMV2_DECODER
  BUF_VIDEO_WMV8,
#endif
#if defined(HAVE_FFMPEG) || CONFIG_WMV3_DECODER
  BUF_VIDEO_WMV9,
#endif
#if defined(HAVE_FFMPEG) || CONFIG_MPEG4_DECODER
  BUF_VIDEO_MPEG4,
#endif
#if defined(HAVE_FFMPEG) || CONFIG_MPEG4_DECODER
  BUF_VIDEO_XVID,
#endif
#if defined(HAVE_FFMPEG) || CONFIG_MPEG4_DECODER
  BUF_VIDEO_DIVX5,
#endif
#if defined(HAVE_FFMPEG) || CONFIG_MPEG4_DECODER
  BUF_VIDEO_3IVX,
#endif
#if defined(HAVE_FFMPEG) || CONFIG_MJPEG_DECODER
  BUF_VIDEO_JPEG,
#endif
#if defined(HAVE_FFMPEG) || CONFIG_MJPEG_DECODER
  BUF_VIDEO_MJPEG,
#endif
#if defined(HAVE_FFMPEG) || CONFIG_MJPEGB_DECODER
  BUF_VIDEO_MJPEG_B,
#endif
#if defined(HAVE_FFMPEG) || CONFIG_H263I_DECODER
  BUF_VIDEO_I263,
#endif
#if defined(HAVE_FFMPEG) || CONFIG_H263_DECODER
  BUF_VIDEO_H263,
#endif
#if defined(HAVE_FFMPEG) || CONFIG_RV10_DECODER
  BUF_VIDEO_RV10,
#endif
#if defined(HAVE_FFMPEG) || CONFIG_RV20_DECODER
  BUF_VIDEO_RV20,
#endif
#if defined(HAVE_FFMPEG) || CONFIG_INDEO3_DECODER
  BUF_VIDEO_IV31,
#endif
#if defined(HAVE_FFMPEG) || CONFIG_INDEO3_DECODER
  BUF_VIDEO_IV32,
#endif
#if defined(HAVE_FFMPEG) || CONFIG_SVQ1_DECODER
  BUF_VIDEO_SORENSON_V1,
#endif
#if defined(HAVE_FFMPEG) || CONFIG_SVQ3_DECODER
  BUF_VIDEO_SORENSON_V3,
#endif
#if defined(HAVE_FFMPEG) || CONFIG_DVVIDEO_DECODER
  BUF_VIDEO_DV,
#endif
#if defined(HAVE_FFMPEG) || CONFIG_HUFFYUV_DECODER
  BUF_VIDEO_HUFFYUV,
#endif
#if defined(HAVE_FFMPEG) || CONFIG_VP3_DECODER
  BUF_VIDEO_VP31,
#endif
#if defined(HAVE_FFMPEG) || CONFIG_VP5_DECODER
  BUF_VIDEO_VP5,
#endif
#if defined(HAVE_FFMPEG) || CONFIG_VP6_DECODER
  BUF_VIDEO_VP6,
  BUF_VIDEO_VP6F,
#endif
#if defined(HAVE_FFMPEG) || CONFIG_4XM_DECODER
  BUF_VIDEO_4XM,
#endif
#if defined(HAVE_FFMPEG) || CONFIG_CINEPAK_DECODER
  BUF_VIDEO_CINEPAK,
#endif
#if defined(HAVE_FFMPEG) || CONFIG_MSVIDEO1_DECODER
  BUF_VIDEO_MSVC,
#endif
#if defined(HAVE_FFMPEG) || CONFIG_MSRLE_DECODER
  BUF_VIDEO_MSRLE,
#endif
#if defined(HAVE_FFMPEG) || CONFIG_RPZA_DECODER
  BUF_VIDEO_RPZA,
#endif
#if defined(HAVE_FFMPEG) || CONFIG_CYUV_DECODER
  BUF_VIDEO_CYUV,
#endif
#if defined(HAVE_FFMPEG) || CONFIG_ROQ_DECODER
  BUF_VIDEO_ROQ,
#endif
#if defined(HAVE_FFMPEG) || CONFIG_IDCIN_DECODER
  BUF_VIDEO_IDCIN,
#endif
#if defined(HAVE_FFMPEG) || CONFIG_XAN_WC3_DECODER
  BUF_VIDEO_WC3,
#endif
#if defined(HAVE_FFMPEG) || CONFIG_WS_VQA_DECODER
  BUF_VIDEO_VQA,
#endif
#if defined(HAVE_FFMPEG) || CONFIG_INTERPLAY_VIDEO_DECODER
  BUF_VIDEO_INTERPLAY,
#endif
#if defined(HAVE_FFMPEG) || CONFIG_FLIC_DECODER
  BUF_VIDEO_FLI,
#endif
#if defined(HAVE_FFMPEG) || CONFIG_8BPS_DECODER
  BUF_VIDEO_8BPS,
#endif
#if defined(HAVE_FFMPEG) || CONFIG_SMC_DECODER
  BUF_VIDEO_SMC,
#endif
#if defined(HAVE_FFMPEG) || CONFIG_TRUEMOTION1_DECODER
  BUF_VIDEO_DUCKTM1,
#endif
#if defined(HAVE_FFMPEG) || CONFIG_TRUEMOTION2_DECODER
  BUF_VIDEO_DUCKTM2,
#endif
#if defined(HAVE_FFMPEG) || CONFIG_VMDVIDEO_DECODER
  BUF_VIDEO_VMD,
#endif
#if defined(HAVE_FFMPEG) || CONFIG_ZLIB_DECODER
  BUF_VIDEO_ZLIB,
#endif
#if defined(HAVE_FFMPEG) || CONFIG_MSZH_DECODER
  BUF_VIDEO_MSZH,
#endif
#if defined(HAVE_FFMPEG) || CONFIG_ASV1_DECODER
  BUF_VIDEO_ASV1,
#endif
#if defined(HAVE_FFMPEG) || CONFIG_ASV2_DECODER
  BUF_VIDEO_ASV2,
#endif
#if defined(HAVE_FFMPEG) || CONFIG_VCR1_DECODER
  BUF_VIDEO_ATIVCR1,
#endif
#if defined(HAVE_FFMPEG) || CONFIG_FLV_DECODER
  BUF_VIDEO_FLV1,
#endif
#if defined(HAVE_FFMPEG) || CONFIG_QTRLE_DECODER
  BUF_VIDEO_QTRLE,
#endif
#if defined(HAVE_FFMPEG) || CONFIG_H264_DECODER
  BUF_VIDEO_H264,
#endif
#if defined(HAVE_FFMPEG) || CONFIG_H261_DECODER
  BUF_VIDEO_H261,
#endif
#if defined(HAVE_FFMPEG) || CONFIG_AASC_DECODER
  BUF_VIDEO_AASC,
#endif
#if defined(HAVE_FFMPEG) || CONFIG_LOCO_DECODER
  BUF_VIDEO_LOCO,
#endif
#if defined(HAVE_FFMPEG) || CONFIG_QDRAW_DECODER
  BUF_VIDEO_QDRW,
#endif
#if defined(HAVE_FFMPEG) || CONFIG_QPEG_DECODER
  BUF_VIDEO_QPEG,
#endif
#if defined(HAVE_FFMPEG) || CONFIG_TSCC_DECODER
  BUF_VIDEO_TSCC,
#endif
#if defined(HAVE_FFMPEG) || CONFIG_ULTI_DECODER
  BUF_VIDEO_ULTI,
#endif
#if defined(HAVE_FFMPEG) || CONFIG_WNV1_DECODER
  BUF_VIDEO_WNV1,
#endif
#if defined(HAVE_FFMPEG) || CONFIG_VIXL_DECODER
  BUF_VIDEO_XL,
#endif
#if defined(HAVE_FFMPEG) || CONFIG_INDEO2_DECODER
  BUF_VIDEO_RT21,
#endif
#if defined(HAVE_FFMPEG) || CONFIG_FRAPS_DECODER
  BUF_VIDEO_FPS1,
#endif
#if defined(HAVE_FFMPEG) || CONFIG_MPEG1VIDEO_DECODER
  BUF_VIDEO_MPEG,
#endif
#if defined(HAVE_FFMPEG) || CONFIG_CSCD_DECODER
  BUF_VIDEO_CSCD,
#endif
#if defined(HAVE_FFMPEG) || CONFIG_AVS_DECODER
  BUF_VIDEO_AVS,
#endif
#if defined(HAVE_FFMPEG) || CONFIG_MMVIDEO_DECODER
  BUF_VIDEO_ALGMM,
#endif
#if defined(HAVE_FFMPEG) || CONFIG_ZMBV_DECODER
  BUF_VIDEO_ZMBV,
#endif
#if defined(HAVE_FFMPEG) || CONFIG_SMACKVIDEO_DECODER
  BUF_VIDEO_SMACKER,
#endif
#if defined(HAVE_FFMPEG) || CONFIG_NUV_DECODER
  BUF_VIDEO_NUV,
#endif
#if defined(HAVE_FFMPEG) || CONFIG_KMVC_DECODER
  BUF_VIDEO_KMVC,
#endif
#if defined(HAVE_FFMPEG) || CONFIG_FLASHSV_DECODER
  BUF_VIDEO_FLASHSV,
#endif
#if defined(HAVE_FFMPEG) || CONFIG_CAVS_DECODER
  BUF_VIDEO_CAVS,
#endif

  0 
};

static uint32_t wmv8_video_types[] = { 
  BUF_VIDEO_WMV8,
  0 
};

static uint32_t wmv9_video_types[] = { 
  BUF_VIDEO_WMV9,
  0 
};

decoder_info_t dec_info_ffmpeg_video = {
  supported_video_types,   /* supported types */
  6                        /* priority        */
};

decoder_info_t dec_info_ffmpeg_wmv8 = {
  wmv8_video_types,        /* supported types */
  0                        /* priority        */
};

decoder_info_t dec_info_ffmpeg_wmv9 = {
  wmv9_video_types,        /* supported types */
  0                        /* priority        */
};
